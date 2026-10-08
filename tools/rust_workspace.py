"""Generate the cargo workspace of a build from the SLC components' slcc files.

A Rust component is a slcc with `metadata.nabucasa.crate` pointing at a crate directory.
The crate's manifest declares everything SLC has no notion of: registry dependencies,
build dependencies, features. Its dependencies on other components are the enabled
components providing what the slcc `requires`, so SLC resolves the dependency graph and
this module adds the result to a copy of the manifest in the build's workspace.
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
    metadata: dict[str, typing.Any]


@dataclasses.dataclass
class Crate:
    path: pathlib.Path
    name: str
    manifest: dict[str, typing.Any]
    components: list[Component]
    enabled_features: set[str] = dataclasses.field(default_factory=set)
    deps: set[str] = dataclasses.field(default_factory=set)

    @property
    def proc_macro(self) -> bool:
        return self.manifest.get("lib", {}).get("proc-macro", False)


def toml_value(value: typing.Any) -> str:
    if isinstance(value, str):
        return f'"{value}"'
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
            manifest = tomllib.loads((path / "Cargo.toml").read_text())
            crates[path] = Crate(
                path=path,
                name=manifest["package"]["name"],
                manifest=manifest,
                components=[],
            )
        crates[path].components.append(component)

    for crate in crates.values():
        for component in crate.components:
            crate.enabled_features.update(component.crate_features)
            for name in component.requires:
                found = {
                    crates[p.crate_path].name
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


def link(target: pathlib.Path, link_path: pathlib.Path) -> None:
    link_path.symlink_to(os.path.relpath(target, start=link_path.parent))


def member_manifest(crate: Crate) -> str:
    """The crate's manifest with its `requires` dependencies added."""
    text = (crate.path / "Cargo.toml").read_text()
    if not crate.deps:
        return text

    declared = set(crate.manifest.get("dependencies", {}))
    assert not declared & crate.deps, (
        f"{crate.name} declares dependencies its slcc already requires: "
        f"{sorted(declared & crate.deps)}"
    )

    block = "".join(f'{dep} = {{ path = "../{dep}" }}\n' for dep in sorted(crate.deps))
    header = "[dependencies]\n"
    if header in text:
        return text.replace(header, header + block, 1)
    return text.rstrip("\n") + "\n\n" + header + block


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
        spec: dict[str, typing.Any] = {"path": f"../{name}"}
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


def write_workspace(out: pathlib.Path, crates: dict[str, Crate]) -> None:
    """Write the workspace: manifests, with the sources linked from the tree."""
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    members = []

    # Crates with no component, which the build scripts depend on by relative path
    component_paths = {crate.path for crate in crates.values()}
    for path in sorted(CRATES_DIR.iterdir()):
        if (path / "Cargo.toml").exists() and path not in component_paths:
            link(path, out / path.name)
            members.append(path.name)

    for name, crate in sorted(crates.items()):
        member = out / name
        member.mkdir()
        for entry in sorted(crate.path.iterdir()):
            if entry.name not in {"Cargo.toml", ".DS_Store", "target"}:
                link(entry, member / entry.name)
        manifest = member_manifest(crate)
        tomllib.loads(manifest)
        (member / "Cargo.toml").write_text(manifest)
        members.append(name)

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
    crates = plan(discover(roots), enabled)
    write_workspace(out, crates)
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
    subprocess.run(["cargo", "fetch", *flags], cwd=workspace, check=True)
    subprocess.run(
        ["cargo", "fetch", *flags, "--target", RUST_TARGET, "-Zbuild-std=core"],
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
