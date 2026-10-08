"""Generate the cargo workspace of a build from the SLC components' slcc files.

A Rust component is a slcc with `metadata.nabucasa.crate` and a crate directory holding
`src/`, an optional `build.rs` and `wrapper.h`, but no manifest. Its cargo dependencies
are the enabled components providing what it `requires`, so SLC resolves the dependency
graph and this module only writes it down. A crate under `crates/` keeps its manifest and
is linked into the workspace as-is.
"""

from __future__ import annotations

import argparse
import dataclasses
import logging
import os
import pathlib
import shutil
import subprocess
import sys
import tomllib
import typing

from ruamel.yaml import YAML

LOGGER = logging.getLogger(__name__)

yaml = YAML(typ="safe")

PROJECTS_ROOT = pathlib.Path(__file__).parent.parent
CRATES_DIR = PROJECTS_ROOT / "crates"
LOCKFILE = CRATES_DIR / "Cargo.lock"
UNIVERSE_ROOTS = [PROJECTS_ROOT / "src", PROJECTS_ROOT / "extension"]

# All supported parts are Cortex-M33 with a single-precision FPU, linked hard-float
RUST_TARGET = "thumbv8m.main-none-eabihf"

AGGREGATOR = "ohf-firmware"
COMPONENT_CONTRIBUTION = "ohf_rust_component"

# Crates with no component, used by the build scripts
BUILD_DEPENDENCIES = ["ohf-bindgen", "ohf-config"]

WORKSPACE_MANIFEST = """\
[workspace]
resolver = "2"
members = [{members}]

[workspace.package]
edition = "2021"
version = "0.1.0"

[profile.release]
panic = "abort"
opt-level = "z"
# The firmware link does LTO across C and Rust. rustc's own LTO output carries no
# summary, which LLD treats as an unsplit LTO unit.
lto = false
codegen-units = 1
strip = true
debug = false

[profile.dev]
panic = "abort"
"""


@dataclasses.dataclass
class Component:
    id: str
    slcc: pathlib.Path
    provides: set[str]
    requires: list[str]
    crate_path: pathlib.Path
    crate_features: list[str]
    crate_dependencies: dict[str, typing.Any]
    proc_macro: bool
    metadata: dict[str, typing.Any]


@dataclasses.dataclass
class Crate:
    path: pathlib.Path
    name: str
    components: list[Component]
    manifest: dict[str, typing.Any] | None
    proc_macro: bool = False
    dependencies: dict[str, typing.Any] = dataclasses.field(default_factory=dict)
    features: set[str] = dataclasses.field(default_factory=set)
    enabled_features: set[str] = dataclasses.field(default_factory=set)
    deps: set[str] = dataclasses.field(default_factory=set)

    @property
    def dirname(self) -> str:
        # A committed crate's relative path dependencies name its siblings' directories
        return self.path.name if self.manifest is not None else self.name


def toml_value(value: typing.Any) -> str:
    if isinstance(value, str):
        return f'"{value}"'
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, list):
        return "[" + ", ".join(toml_value(v) for v in value) + "]"
    if isinstance(value, dict):
        return (
            "{ " + ", ".join(f"{k} = {toml_value(v)}" for k, v in value.items()) + " }"
        )
    raise TypeError(value)


def discover(roots: list[pathlib.Path]) -> list[Component]:
    """Every Rust component under the roots, through symlinked extensions."""
    components: dict[pathlib.Path, Component] = {}

    for root in roots:
        for slcc in sorted(root.rglob("*.slcc", recurse_symlinks=True)):
            slcc = slcc.resolve()
            if slcc in components:
                continue

            component = yaml.load(slcc.read_text())
            metadata = component.get("metadata", {}).get("nabucasa", {})
            if "crate" not in metadata:
                continue

            crate = metadata["crate"]
            contributions = [
                c["value"]
                for c in component.get("template_contribution", [])
                if c["name"] == COMPONENT_CONTRIBUTION
            ]
            assert contributions == [component["id"]], (
                f"{slcc} must contribute `{COMPONENT_CONTRIBUTION}: {component['id']}`"
            )

            components[slcc] = Component(
                id=component["id"],
                slcc=slcc,
                provides={p["name"] for p in component.get("provides", [])},
                requires=[r["name"] for r in component.get("requires", [])],
                crate_path=(slcc.parent / crate["path"]).resolve(),
                crate_features=crate.get("features", []),
                crate_dependencies=crate.get("dependencies", {}),
                proc_macro=crate.get("proc_macro", False),
                metadata=metadata,
            )

    by_id: dict[str, Component] = {}
    for component in components.values():
        assert component.id not in by_id, f"{component.id} is defined twice"
        by_id[component.id] = component

    return list(by_id.values())


def plan(components: list[Component], enabled: set[str] | None) -> dict[str, Crate]:
    """The crates of the enabled components, or of every component when `enabled` is None."""
    if enabled is not None:
        unknown = enabled - {c.id for c in components}
        assert not unknown, f"Enabled components without a crate: {sorted(unknown)}"
        components = [c for c in components if c.id in enabled]

    providers: dict[str, list[Component]] = {}
    for component in components:
        for name in component.provides:
            providers.setdefault(name, []).append(component)

    crates: dict[pathlib.Path, Crate] = {}
    for component in components:
        path = component.crate_path
        if path not in crates:
            manifest_path = path / "Cargo.toml"
            manifest = (
                tomllib.loads(manifest_path.read_text())
                if manifest_path.exists()
                else None
            )
            crates[path] = Crate(path=path, name="", components=[], manifest=manifest)
        crates[path].components.append(component)

    for crate in crates.values():
        if crate.manifest is not None:
            crate.name = crate.manifest["package"]["name"]
        else:
            # The component without crate features owns the crate and names it
            owners = [c for c in crate.components if not c.crate_features]
            assert len(owners) == 1, f"{crate.path} needs exactly one owner: {owners}"
            crate.name = owners[0].id

        for component in crate.components:
            crate.proc_macro |= component.proc_macro
            crate.enabled_features.update(component.crate_features)
            for name, spec in component.crate_dependencies.items():
                assert crate.dependencies.get(name, spec) == spec, (
                    f"{crate.name} declares {name} twice"
                )
                crate.dependencies[name] = spec

    by_path = {path: crate for path, crate in crates.items()}
    for crate in crates.values():
        for component in crate.components:
            for name in component.requires:
                found = {
                    by_path[p.crate_path].name
                    for p in providers.get(name, [])
                    if p.crate_path != crate.path
                }
                if enabled is not None:
                    assert len(found) <= 1, (
                        f"{component.id} requires {name}, provided by {sorted(found)}"
                    )
                crate.deps.update(found)

    by_name: dict[str, Crate] = {}
    for crate in crates.values():
        assert crate.name not in by_name, f"Two crates are named {crate.name}"
        by_name[crate.name] = crate

    return by_name


def declared_features(components: list[Component]) -> dict[pathlib.Path, set[str]]:
    """Every crate feature any component declares, so a generated manifest defines them all."""
    features: dict[pathlib.Path, set[str]] = {}
    for component in components:
        features.setdefault(component.crate_path, set()).update(
            component.crate_features
        )
    return features


def link(target: pathlib.Path, link_path: pathlib.Path) -> None:
    link_path.symlink_to(os.path.relpath(target, start=link_path.parent))


def member_manifest(crate: Crate, crates: dict[str, Crate], features: set[str]) -> str:
    lines = [
        "[package]",
        f'name = "{crate.name}"',
        "edition.workspace = true",
        "version.workspace = true",
        "",
    ]

    if crate.proc_macro:
        lines += ["[lib]", "proc-macro = true", ""]

    if features:
        lines += ["[features]", *(f"{f} = []" for f in sorted(features)), ""]

    lines.append("[dependencies]")
    for dep in sorted(crate.deps):
        lines.append(f'{dep} = {{ path = "../{crates[dep].dirname}" }}')
    for name, spec in sorted(crate.dependencies.items()):
        lines.append(f"{name} = {toml_value(spec)}")
    lines.append("")

    if (crate.path / "build.rs").exists():
        lines.append("[build-dependencies]")
        for dep in BUILD_DEPENDENCIES:
            lines.append(f'{dep} = {{ path = "../{dep}" }}')
        lines.append("")

    return "\n".join(lines)


def aggregator_manifest(crates: dict[str, Crate]) -> str:
    lines = [
        "[package]",
        f'name = "{AGGREGATOR}"',
        "edition.workspace = true",
        "version.workspace = true",
        "",
        "[lib]",
        "# The only staticlib, so there is one copy of `core`",
        'crate-type = ["staticlib"]',
        "",
        "[dependencies]",
    ]

    for name, crate in sorted(crates.items()):
        if crate.proc_macro:
            continue
        spec: dict[str, typing.Any] = {"path": f"../{crate.dirname}"}
        if crate.enabled_features:
            spec["features"] = sorted(crate.enabled_features)
        lines.append(f"{name} = {toml_value(spec)}")

    lines.append("")
    return "\n".join(lines)


def aggregator_source(crates: dict[str, Crate]) -> str:
    lines = ["//! Links the enabled components into one staticlib.", "#![no_std]", ""]
    for name, crate in sorted(crates.items()):
        if not crate.proc_macro:
            lines.append(f"extern crate {name.replace('-', '_')};")
    lines.append("")
    return "\n".join(lines)


def write_workspace(
    out: pathlib.Path, components: list[Component], crates: dict[str, Crate]
) -> None:
    """Write the workspace: manifests, with the sources linked from the tree."""
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    members = []

    # Every committed crate, as the build dependencies and their siblings refer to each
    # other by relative path
    for path in sorted(CRATES_DIR.iterdir()):
        if (path / "Cargo.toml").exists():
            link(path, out / path.name)
            members.append(path.name)

    features = declared_features(components)
    for name, crate in sorted(crates.items()):
        if crate.manifest is not None:
            assert crate.dirname in members, f"{crate.path} is not under {CRATES_DIR}"
            continue

        member = out / crate.dirname
        member.mkdir()
        for entry in sorted(crate.path.iterdir()):
            if entry.name not in {".DS_Store", "target"}:
                link(entry, member / entry.name)
        (member / "Cargo.toml").write_text(
            member_manifest(crate, crates, features.get(crate.path, set()))
        )
        members.append(crate.dirname)

    aggregator = out / AGGREGATOR
    (aggregator / "src").mkdir(parents=True)
    (aggregator / "Cargo.toml").write_text(aggregator_manifest(crates))
    (aggregator / "src" / "lib.rs").write_text(aggregator_source(crates))
    members.append(AGGREGATOR)

    (out / "Cargo.toml").write_text(
        WORKSPACE_MANIFEST.format(members=", ".join(f'"{m}"' for m in members))
    )
    shutil.copy(LOCKFILE, out / "Cargo.lock")
    # rustup picks the toolchain from the working directory, so cargo runs from here
    link(CRATES_DIR / "rust-toolchain.toml", out / "rust-toolchain.toml")


def generate(
    out: pathlib.Path, roots: list[pathlib.Path], enabled: set[str] | None
) -> dict[str, Crate]:
    components = discover(roots)
    crates = plan(components, enabled)
    write_workspace(out, components, crates)
    return crates


def lock_packages(lockfile: pathlib.Path) -> dict[tuple[str, str], str | None]:
    """Registry packages by name and version, with their checksums."""
    return {
        (p["name"], p["version"]): p.get("checksum")
        for p in tomllib.loads(lockfile.read_text())["package"]
        if "source" in p
    }


def validate_lockfile(workspace: pathlib.Path) -> None:
    """A build's lockfile is the committed one pruned to the enabled crates."""
    universe = lock_packages(LOCKFILE)
    for package, checksum in lock_packages(workspace / "Cargo.lock").items():
        assert package in universe, f"{package} is not in {LOCKFILE}"
        assert universe[package] == checksum, f"{package} differs from {LOCKFILE}"


def cargo_fetch(workspace: pathlib.Path, locked: bool, offline: bool = False) -> None:
    """Fetch the workspace's dependencies for every platform, including `core`'s."""
    flags = ["--locked"] * locked + ["--offline"] * offline
    subprocess.run(
        ["cargo", "fetch", *flags],
        cwd=workspace,
        check=True,
    )
    subprocess.run(
        [
            "cargo",
            "fetch",
            *flags,
            "--target",
            RUST_TARGET,
            "-Zbuild-std=core",
        ],
        env={**os.environ, "RUSTC_BOOTSTRAP": "1"},
        cwd=workspace,
        check=True,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command",
        choices=["lock", "fetch"],
        help="lock: update the committed lockfile; fetch: cache its packages",
    )
    parser.add_argument(
        "--workspace",
        type=pathlib.Path,
        default=PROJECTS_ROOT / "build" / "rust-universe",
        help="where the workspace of every component is written",
    )
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO)

    generate(args.workspace, UNIVERSE_ROOTS, enabled=None)

    if args.command == "lock":
        cargo_fetch(args.workspace, locked=False)
        shutil.copy(args.workspace / "Cargo.lock", LOCKFILE)
        LOGGER.info("Updated %s", LOCKFILE)
    else:
        cargo_fetch(args.workspace, locked=True)


if __name__ == "__main__":
    sys.exit(main())
