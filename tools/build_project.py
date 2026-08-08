"""Tool to retarget and build a SLCP project based on a manifest."""

from __future__ import annotations

import argparse
import ast
import contextlib
import dataclasses
import enum
import hashlib
import json
import logging
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time
import typing
from datetime import UTC, datetime

from elftools.elf.elffile import ELFFile
from ruamel.yaml import YAML

from .create_gbl import create_gbl

LOGGER = logging.getLogger(__name__)


yaml = YAML(typ="safe")


class Toolchain(enum.StrEnum):
    LLVM = "llvm"
    GCC = "gcc"


def evaluate_f_string(f_string: str, variables: dict[str, typing.Any]) -> str:
    """Evaluates an `f`-string with the given locals."""

    return eval("f" + repr(f_string), variables)


def ensure_folder(path: str | pathlib.Path) -> pathlib.Path:
    """Ensure that the path exists and is a folder."""
    path = pathlib.Path(path)

    if not path.is_dir():
        raise argparse.ArgumentTypeError(f"Folder {path} does not exist")

    return path


def get_toolchain_default_paths() -> list[pathlib.Path]:
    """Return the path to the toolchain."""
    return list(pathlib.Path("/opt/toolchains").glob("*"))


def get_apack_default_paths() -> list[pathlib.Path]:
    """Return the folders containing slc adapter packs."""

    # slc wants each pack's own folder. Without them it falls back to the SDK's own
    # apacks, which lack Python, and every template generator silently fails.
    return [p.parent for p in pathlib.Path("/opt/silabs").glob("*/apack.json")]


def get_sdk_default_paths() -> list[pathlib.Path]:
    """Return the path to the SDK."""
    return list(pathlib.Path("/opt/silabs/sdks").glob("*sdk*"))


def parse_override(override: str) -> tuple[str, dict | list]:
    """Parse a config override."""
    if "=" not in override:
        raise argparse.ArgumentTypeError("Override must be of the form `key=json`")

    key, value = override.split("=", 1)

    try:
        return key, json.loads(value)
    except json.JSONDecodeError as exc:
        raise argparse.ArgumentTypeError(f"Invalid JSON: {exc}")


def parse_prefixed_output(output: str) -> tuple[str, pathlib.Path | None]:
    """Parse a prefixed output parameter."""
    if ":" in output:
        prefix, _, path = output.partition(":")
        path = pathlib.Path(path)
    else:
        prefix = output
        path = None

    if prefix not in ("gbl", "hex", "out"):
        raise argparse.ArgumentTypeError(
            "Output format is of the form `gbl:overridden_filename.gbl` or just `gbl`"
        )

    return prefix, path


def get_git_commit_id(repo: pathlib.Path) -> str:
    """Get a commit hash for the current git repository."""

    def git(*args: str) -> str:
        result = subprocess.run(
            ["git", "-C", str(repo)] + list(args),
            text=True,
            capture_output=True,
            check=True,
        )
        return result.stdout.strip()

    # Get the current commit ID
    commit_id = git("rev-parse", "HEAD")[:8]

    # Check if the repository is dirty
    is_dirty = git("status", "--porcelain")

    # If dirty, append the SHA256 hash of the git diff to the commit ID
    if is_dirty:
        dirty_diff = git("diff")
        sha256_hash = hashlib.sha256(dirty_diff.encode()).hexdigest()[:8]
        commit_id += f"-dirty-{sha256_hash}"

    return commit_id


def load_sdks(paths: list[pathlib.Path]) -> dict[pathlib.Path, str]:
    """Load the SDK metadata from the SDKs."""
    sdks = {}

    for sdk in paths:
        sdk_file = next(sdk.glob("*_sdk.slcs"))
        sdk_meta = yaml.load(sdk_file.read_text())

        sdk_id = sdk_meta["id"]
        sdk_version = sdk_meta["sdk_version"]
        sdks[sdk] = f"{sdk_id}:{sdk_version}"

    return sdks


def load_toolchains(paths: list[pathlib.Path]) -> dict[pathlib.Path, str]:
    """Load the toolchain metadata from the toolchains."""
    toolchains = {}

    for toolchain in paths:
        # libc++ encodes its version in `__config` as `_LIBCPP_VERSION` (MMmmpp, e.g. 210101)
        libcpp_config = next(
            toolchain.glob("lib/clang-runtimes/**/include/c++/v1/__config"), None
        )
        if libcpp_config is not None:
            for line in libcpp_config.read_text().split("\n"):
                if "define _LIBCPP_VERSION " in line:
                    version = int(line.split()[-1])
                    toolchains[toolchain] = (
                        f"{Toolchain.LLVM}:"
                        f"{version // 10000}.{version // 100 % 100}.{version % 100}"
                    )
                    break
            continue

        gcc_plugin_version_h = next(
            toolchain.glob("lib/gcc/arm-none-eabi/*/plugin/include/plugin-version.h")
        )
        version_info = {}

        for line in gcc_plugin_version_h.read_text().split("\n"):
            # static char basever[] = "10.3.1";
            if line.startswith("static char") and line.endswith(";"):
                name = line.split("[]", 1)[0].split()[-1]
                value = ast.literal_eval(line.split(" = ", 1)[1][:-1])
                version_info[name] = value

        toolchains[toolchain] = (
            f"{Toolchain.GCC}:{version_info['basever']}.{version_info['datestamp']}"
        )

    return toolchains


def subprocess_run_verbose(command: list[str], prefix: str, **kwargs) -> None:
    with subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs
    ) as proc:
        for line in proc.stdout:
            LOGGER.info("[%s] %s", prefix, line.decode("utf-8").strip())

    if proc.returncode != 0:
        LOGGER.error("[%s] Error: %s", prefix, proc.returncode)
        sys.exit(1)


def validate_wrap_declarations(project_path: pathlib.Path) -> dict[str, str | None]:
    """Map each wrapped symbol to where its definition lives, or `None` if unspecified."""
    declarations: dict[str, str | None] = {}

    # Shared extensions are symlinked in from the repo root, and `rglob` does not
    # descend into symlinked directories on its own
    for slcc in sorted(project_path.rglob("*.slcc", recurse_symlinks=True)):
        component = yaml.load(slcc.read_text())
        declared: dict[str, set[str]] = {}

        for setting in component.get("toolchain_settings", []):
            targets = set(re.findall(r"--wrap=(\w+)", str(setting["value"])))

            if targets:
                declared.setdefault(setting["option"], set()).update(targets)

        if not declared:
            continue

        gcc = declared.get("gcc_linker_option", set())
        llvm = declared.get("llvm_linker_option", set())

        # `gcc_linker_option` only reaches the GCC linker, so a wrap declared under it
        # alone vanishes from LLVM builds: no flag, no `__wrap_` symbol, no error
        if gcc != llvm:
            LOGGER.error(
                "%s declares --wrap for one toolchain only: gcc-only=%s llvm-only=%s",
                slcc.relative_to(project_path),
                sorted(gcc - llvm),
                sorted(llvm - gcc),
            )
            sys.exit(1)

        # `metadata` is the SDK's free-form slcc key; a bare top-level key is a
        # "junk key" warning and spec 8 refuses to generate with any warning
        definitions = (
            component.get("metadata", {})
            .get("nabucasa", {})
            .get("wrap_definitions", {})
        )

        if set(definitions) - gcc:
            LOGGER.error(
                "%s names definitions for symbols it does not wrap: %s",
                slcc.relative_to(project_path),
                sorted(set(definitions) - gcc),
            )
            sys.exit(1)

        for target in gcc:
            declarations[target] = definitions.get(target)

    return declarations


def linker_wrap_targets(build: ResolvedBuild) -> set[str]:
    """The symbols the generated project actually asks the linker to wrap."""
    return set(re.findall(r"-Wl,--wrap=(\w+)", build.project_cmake.read_text()))


def validate_linker_wraps(elf_path: pathlib.Path, targets: set[str]) -> None:
    """Check that every reference to a wrapped symbol goes via its wrapper."""

    # `--emit-relocs` retains relocations naming the symbol each call site and function
    # pointer resolved to, so vector table entries are covered as directly as branches
    with elf_path.open("rb") as f:
        elf = ELFFile(f)
        symbols = list(elf.get_section_by_name(".symtab").iter_symbols())
        names = [symbol.name for symbol in symbols]
        functions = {
            symbol.name: (symbol["st_value"] & ~1, symbol["st_size"])
            for symbol in symbols
            if symbol["st_info"]["type"] == "STT_FUNC" and symbol.name
        }

        references: dict[str, list[tuple[str, int]]] = {}

        for section in elf.iter_sections():
            if section.header["sh_type"] not in ("SHT_REL", "SHT_RELA"):
                continue

            # `.ARM.exidx` describes functions for unwinding, it does not reference them
            if ".debug" in section.name or ".ARM.exidx" in section.name:
                continue

            for reloc in section.iter_relocations():
                references.setdefault(names[reloc["r_info_sym"]], []).append(
                    (section.name, reloc["r_offset"])
                )

    # Without them every target below would trivially pass, which is the failure mode
    # this check exists to eliminate
    if targets and not references:
        raise RuntimeError(
            f"{elf_path.name} has no relocations, `-Wl,--emit-relocs` is required to"
            f" validate --wrap"
        )

    problems = []

    for target in sorted(targets):
        refs = references.get(target, [])
        wrapper = functions.get(f"__wrap_{target}")

        if wrapper is None:
            # With no references left the linker drops the wrapper too, which is fine
            if refs:
                section, offset = refs[0]
                problems.append(
                    f"--wrap={target}: __wrap_{target} is not in the image, but"
                    f" {len(refs)} reference(s) remain, e.g. {section}+0x{offset:08x}"
                )

            continue

        start, size = wrapper
        stray = [ref for ref in refs if not (start <= ref[1] < start + size)]

        if stray:
            section, offset = stray[0]
            problems.append(
                f"--wrap={target}: {len(stray)} reference(s) bypass __wrap_{target},"
                f" e.g. {section}+0x{offset:08x}"
            )

        if target in functions:
            address = functions[target][0]
            peers = sorted(n for n, (a, _) in functions.items() if a == address)

            if len(peers) > 1:
                problems.append(
                    f"--wrap={target}: __real_{target} shares address 0x{address:08x}"
                    f" with {peers}, so it is likely an empty stub folded by LTO"
                )

    if problems:
        raise RuntimeError(
            "Linker --wrap validation failed:\n - " + "\n - ".join(problems)
        )


def get_elf_source_paths(elf_path: pathlib.Path) -> set[pathlib.PurePosixPath]:
    """Gets the set of source paths in the given ELF file."""
    paths = set()

    with elf_path.open("rb") as f:
        elf = ELFFile(f)
        # `--emit-relocs` leaves `.rel.debug_*` behind, which pyelftools would otherwise
        # try to apply, and it has no handler for `R_ARM_NONE`
        dwarf = elf.get_dwarf_info(relocate_dwarf_sections=False)

        for cu in dwarf.iter_CUs():
            line_program = dwarf.line_program_for_CU(cu)

            for entry in line_program.get_entries():
                state = entry.state
                if state is None:
                    continue

                file_entry = line_program["file_entry"][state.file - 1]
                directory = line_program["include_directory"][
                    file_entry.dir_index - 1
                ].decode("utf-8")
                filename = file_entry.name.decode("utf-8")

                paths.add(pathlib.PurePosixPath(f"{directory}/{filename}"))

    return paths


def remap_debug_build_paths(elf_path: pathlib.Path, prefix_map: dict[str, str]) -> None:
    """Remap absolute build-path prefixes in an ELF's DWARF string tables, in place."""
    replacements = {}
    for old, new in prefix_map.items():
        old_bytes, new_bytes = old.encode(), new.encode()
        assert len(new_bytes) <= len(old_bytes), f"{new!r} is longer than {old!r}"
        replacements[old_bytes] = new_bytes + b"/" * (len(old_bytes) - len(new_bytes))

    with elf_path.open("rb") as f:
        elf = ELFFile(f)
        sections = [
            (section["sh_offset"], bytearray(section.data()))
            for name in (".debug_str", ".debug_line_str")
            if (section := elf.get_section_by_name(name)) is not None
        ]

    with elf_path.open("r+b") as f:
        for offset, data in sections:
            for old_bytes, padded in replacements.items():
                search = 0

                while (index := data.find(old_bytes, search)) != -1:
                    # Only rewrite at a string start (offset 0 or right after a NUL)
                    if index == 0 or data[index - 1] == 0:
                        data[index : index + len(old_bytes)] = padded

                    search = index + 1

            f.seek(offset)
            f.write(data)


@dataclasses.dataclass(frozen=True)
class ResolvedBuild:
    """Everything about a build that is resolved up front and never changes."""

    manifest: dict[str, typing.Any]
    manifest_path: pathlib.Path
    build_dir: pathlib.Path
    projects_root: pathlib.Path
    base_project_path: pathlib.Path
    base_project_name: str
    sdk: pathlib.Path
    sdk_name: str
    sdk_version: str
    toolchain: Toolchain
    toolchain_path: pathlib.Path
    build_timestamp: datetime

    @property
    def manifest_dir(self) -> pathlib.Path:
        return self.manifest_path.parent

    @property
    def build_template_path(self) -> pathlib.Path:
        return self.build_dir / "template"

    @property
    def base_project_slcp(self) -> pathlib.Path:
        return self.build_template_path / f"{self.base_project_name}.slcp"

    @property
    def cmake_dir(self) -> pathlib.Path:
        return self.build_dir / f"cmake_{self.toolchain}"

    @property
    def project_cmake(self) -> pathlib.Path:
        return self.cmake_dir / f"{self.base_project_name}.cmake"


def build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--manifest",
        type=pathlib.Path,
        required=True,
        help="Firmware build manifest",
    )
    parser.add_argument(
        "--output",
        action="append",
        dest="outputs",
        type=parse_prefixed_output,
        required=True,
        help="Output file prefixed with its file type",
    )
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        type=pathlib.Path,
        default=pathlib.Path("."),
        help="Output directory for artifacts, will be created if it does not exist",
    )
    parser.add_argument(
        "--no-clean-build-dir",
        action="store_false",
        dest="clean_build_dir",
        default=True,
        help="Do not clean the build directory",
    )
    parser.add_argument(
        "--build-dir",
        type=pathlib.Path,
        default=None,
        help="Temporary build directory, generated based on the manifest by default",
    )
    parser.add_argument(
        "--sdk",
        action="append",
        dest="sdks",
        type=ensure_folder,
        default=get_sdk_default_paths(),
        required=len(get_sdk_default_paths()) == 0,
        help="Path to a Gecko SDK",
    )
    parser.add_argument(
        "--toolchain",
        action="append",
        dest="toolchains",
        type=ensure_folder,
        default=get_toolchain_default_paths(),
        required=len(get_toolchain_default_paths()) == 0,
        help="Path to a GCC toolchain",
    )
    parser.add_argument(
        "--tool-path",
        action="append",
        dest="tool_paths",
        type=ensure_folder,
        default=get_apack_default_paths(),
        help="Path to a folder containing an slc adapter pack",
    )
    parser.add_argument(
        "--override",
        action="append",
        dest="overrides",
        required=False,
        type=parse_override,
        default=[],
        help="Override config key with JSON.",
    )
    parser.add_argument(
        "--keep-slc-daemon",
        action="store_true",
        dest="keep_slc_daemon",
        default=False,
        help="Do not shut down the SLC daemon after the build",
    )
    parser.add_argument(
        "--slc-daemon",
        action="store_true",
        dest="slc_daemon",
        default=False,
        help="Whether to use the SLC daemon for the build",
    )
    parser.add_argument(
        "--build-timestamp",
        dest="build_timestamp",
        type=str,
        default=None,
        help="Build timestamp for reproducible builds (YYYYMMDDHHmmss format)",
    )

    return parser


def resolve_build(args: argparse.Namespace) -> ResolvedBuild:
    """Load the manifest and pin down the SDK, toolchain, and build paths."""
    manifest = yaml.load(args.manifest.read_text())

    for key, override in args.overrides:
        manifest[key] = override

    sdks = load_sdks(args.sdks)
    sdk, sdk_and_version = next(
        (path, version) for path, version in sdks.items() if version == manifest["sdk"]
    )
    sdk_name, sdk_version = sdk_and_version.split(":", 1)

    toolchains = load_toolchains(args.toolchains)
    toolchain_path = next(
        path
        for path, name in toolchains.items()
        if manifest["toolchain"] in (name, name.split(":", 1)[1])
    )
    toolchain = Toolchain(toolchains[toolchain_path].split(":", 1)[0])

    projects_root = pathlib.Path(__file__).parent.parent
    base_project_path = projects_root / manifest["base_project"]
    assert base_project_path.is_relative_to(projects_root)

    # The template copy preserves `.slcp` files, so the source project names the build
    (base_project_slcp,) = base_project_path.glob("*.slcp")

    return ResolvedBuild(
        manifest=manifest,
        manifest_path=args.manifest,
        build_dir=args.build_dir,
        projects_root=projects_root,
        base_project_path=base_project_path,
        base_project_name=base_project_slcp.stem,
        sdk=sdk,
        sdk_name=sdk_name,
        sdk_version=sdk_version,
        toolchain=toolchain,
        toolchain_path=toolchain_path,
        build_timestamp=args.build_timestamp,
    )


def copy_base_project(build: ResolvedBuild) -> None:
    """Copy the base project into the build dir, then overlay SDK sources onto it."""
    shutil.copytree(
        build.base_project_path,
        build.build_template_path,
        dirs_exist_ok=True,
        ignore=lambda dir, contents: [
            "autogen",
            ".git",
            ".settings",
            ".projectlinkstore",
            ".project",
            ".pdm",
            ".cproject",
            ".uceditor",
        ],
    )

    # Copy SDK files into the build template (e.g. unmodified sample app sources).
    # Files already present in the project (customized) are not overwritten.
    for sdk_file in build.manifest.get("copy_sdk_files", []):
        src = build.sdk / sdk_file["source"]
        dst = build.build_template_path / sdk_file["path"]

        if dst.exists():
            LOGGER.info("Skipping SDK file (already in project): %s", sdk_file["path"])
            continue

        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(src, dst)
        LOGGER.info("Copied SDK file: %s -> %s", sdk_file["source"], sdk_file["path"])


def merge_manifest_into_slcp(
    base_project: dict[str, typing.Any], manifest: dict[str, typing.Any]
) -> dict[str, typing.Any]:
    """Extend the base project with the manifest's additions and removals."""
    base_project["component"].extend(manifest.get("add_components", []))
    base_project.setdefault("toolchain_settings", []).extend(
        manifest.get("toolchain_settings", [])
    )
    base_project.setdefault("sdk_extension", []).extend(
        manifest.get("sdk_extension", [])
    )
    base_project.setdefault("template_contribution", []).extend(
        manifest.get("template_contribution", [])
    )

    for component in manifest.get("remove_components", []):
        try:
            base_project["component"].remove(component)
        except ValueError:
            LOGGER.warning(
                "Component %s is not present in manifest, cannot remove", component
            )
            sys.exit(1)

    base_project.setdefault("source", []).extend(manifest.get("add_sources", []))
    base_project.setdefault("include", []).extend(manifest.get("add_includes", []))
    base_project.setdefault("config_file", []).extend(manifest.get("config_file", []))

    # Extend configuration and C defines
    for input_config, output_config in [
        (
            manifest.get("configuration", {}),
            base_project.setdefault("configuration", []),
        ),
        (
            manifest.get("slcp_defines", {}),
            base_project.setdefault("define", []),
        ),
    ]:
        for name, value in input_config.items():
            # Values are always strings
            value = str(value)

            # First try to replace any existing config entries
            for config in output_config:
                if config["name"] == name:
                    config["value"] = value
                    break
            else:
                # Otherwise, append it
                output_config.append({"name": name, "value": value})

    return base_project


def prepare_project_slcp(build: ResolvedBuild) -> dict[str, typing.Any]:
    """Merge the manifest into the copied project and write it back out."""
    # Config files (e.g. ZAP files) live next to the manifest, not in the base project
    for config_file in build.manifest.get("config_file", []):
        src_path = build.manifest_dir / config_file["path"]
        dst_path = build.build_template_path / config_file["path"]
        dst_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(src_path, dst_path)
        LOGGER.info("Copied config file: %s", config_file["path"])

    base_project = merge_manifest_into_slcp(
        yaml.load(build.base_project_slcp.read_text()), build.manifest
    )

    with build.base_project_slcp.open("w") as f:
        yaml.dump(base_project, f)

    return base_project


def run_slc_generate(
    build: ResolvedBuild, slc: list[str], tool_paths: list[pathlib.Path]
) -> None:
    """Generate a chip-specific project from the modified base project."""
    LOGGER.info(f"Generating project for {build.manifest['device']}")

    # fmt: off
    subprocess_run_verbose(
        slc
        + [
            "generate",
            "--verbose", "DEBUG",
            "--trust-totality",
            "--with", build.manifest["device"],
            "--project-file", build.base_project_slcp.resolve(),
            "--export-destination", build.build_dir.resolve(),
            "--copy-proj-sources",
            "--copy-sdk-sources",
            "--new-project",
            "--toolchain", f"toolchain_{build.toolchain}",
            "--sdk", build.sdk,
            "--output-type", "vscode",
        ]
        + [f"--tool-path={p}" for p in tool_paths]
        + [f"--toolchain-locations={build.toolchain}:{build.toolchain_path}"],
        "slc generate",
        env={
            **os.environ,
        },
    )
    # fmt: on


def exclude_archive_member_from_lto(
    build: ResolvedBuild, archive_name: str, member: str, arch_flags: list[str]
) -> None:
    """Replace one bitcode archive member with a precompiled object, in place."""
    archive = next(build.build_dir.rglob(archive_name)).resolve()
    workdir = build.build_dir / "no_lto"
    workdir.mkdir(parents=True, exist_ok=True)

    gcc = build.toolchain_path / "bin/arm-none-eabi-gcc"
    ar = build.toolchain_path / "bin/arm-none-eabi-ar"
    real_member = f"no_lto_{member}"

    subprocess.run([ar, "x", archive, member], cwd=workdir, check=True)
    # `nolto-rel` runs LTO codegen but emits a plain relocatable object, so the member's
    # own references stay undefined instead of being bound internally
    subprocess.run(
        [
            gcc,
            *arch_flags,
            "-r",
            "-nostdlib",
            "-flinker-output=nolto-rel",
            member,
            "-o",
            real_member,
        ],
        cwd=workdir,
        check=True,
    )
    subprocess.run([ar, "r", archive, real_member], cwd=workdir, check=True)
    subprocess.run([ar, "d", archive, member], check=True)

    LOGGER.info("Excluded %s from LTO (%s)", member, archive.name)


def exclude_definitions_from_lto(build: ResolvedBuild, definitions: list[str]) -> None:
    """Compile the named definitions outside the LTO unit so `--wrap` can see them."""
    # GNU ld only redirects *undefined* references, so a definition LTO can see is called
    # directly instead (binutils PR ld/31956). LLD rewrites symbol tables and is immune.
    if not definitions or build.toolchain is not Toolchain.GCC:
        return

    text = build.project_cmake.read_text()

    # `-fwhole-program` asserts the LTO unit is the entire program, so a definition
    # sitting outside it does not resolve at all
    assert "-fwhole-program" in text
    text = text.replace(" -fwhole-program", "")

    arch_flags = list(
        dict.fromkeys(re.findall(r"-m(?:cpu|fpu|float-abi)=[\w.+-]+|-mthumb", text))
    )

    sources = []

    for definition in definitions:
        archive_name, separator, member = definition.partition(":")

        if separator:
            exclude_archive_member_from_lto(build, archive_name, member, arch_flags)
        else:
            sources.append(definition)

    if sources:
        matched = [
            line.strip().strip('"')
            for line in text.split("\n")
            if line.strip().strip('"').endswith(tuple(sources))
        ]
        assert len(matched) == len(sources), f"{sources} matched {matched}"

        quoted = "\n    ".join(f'"{path}"' for path in matched)
        text += (
            f"\nset_source_files_properties(\n    {quoted}\n"
            f'    PROPERTIES COMPILE_OPTIONS "-fno-lto"\n)\n'
        )
        LOGGER.info("Excluded %s from LTO", sources)

    build.project_cmake.write_text(text)


def apply_sdk_patches(build: ResolvedBuild) -> None:
    """Patch the copied SDK. A last resort, prefer SDK extensions wherever possible!"""
    if not build.manifest.get("sdk_patches"):
        return

    copied_sdk_dir = next(build.build_dir.glob(f"{build.sdk_name}_*"))

    for patch_path in build.manifest["sdk_patches"]:
        patch_file = build.base_project_path / "sdk_patches" / patch_path
        LOGGER.info("Applying SDK patch: %s", patch_file.name)
        subprocess.run(
            [
                "git",
                "apply",
                f"--directory={copied_sdk_dir.resolve().relative_to(build.projects_root.resolve())}",
                str(patch_file.resolve()),
            ],
            check=True,
            cwd=build.projects_root,
        )


def validate_sdk_extensions(
    build: ResolvedBuild, base_project: dict[str, typing.Any]
) -> None:
    """Make sure all referenced extensions exist in the SDK or the project."""
    for sdk_extension in base_project.get("sdk_extension", []):
        sdk_ext_dir = build.sdk / f"extension/{sdk_extension['id']}_extension"
        project_ext_dir = (
            build.build_template_path / f"extension/{sdk_extension['id']}_extension"
        )

        if not sdk_ext_dir.is_dir() and not project_ext_dir.is_dir():
            LOGGER.error(
                "Referenced extension not present in SDK (%s) or project (%s)",
                sdk_ext_dir,
                project_ext_dir,
            )
            sys.exit(1)


def normalize_c_defines(manifest: dict[str, typing.Any]) -> dict[str, dict]:
    """Expand the shorthand form of `c_defines` entries into full dicts."""
    normalized = {}

    for define, config in manifest.get("c_defines", {}).items():
        if not isinstance(config, dict):
            config = {"type": "config", "value": config}

        config = {"error_on_duplicate": True, **config}

        if config["type"] not in ("config", "c_flag"):
            raise ValueError(f"Invalid config type: {config['type']}")

        normalized[define] = config

    return normalized


def patch_config_headers(
    config_roots: list[pathlib.Path],
    c_defines: dict[str, dict],
    base_project: dict[str, typing.Any],
    template_env: dict[str, typing.Any],
) -> set[str]:
    """Rewrite `#define`s in the generated config headers, returning the ones applied."""
    written = set()

    for config_root in config_roots:
        for config_f in config_root.glob("*.h"):
            config_h_lines = config_f.read_text().split("\n")
            written_config = {}
            new_config_h_lines = []

            for index, line in enumerate(config_h_lines):
                # The two lists stay in lockstep, so `index` is valid in both
                assert len(new_config_h_lines) == index

                for define, config in c_defines.items():
                    if config["type"] == "c_flag":
                        continue

                    if f"#define {define} " not in line:
                        continue

                    if define in written:
                        if config["error_on_duplicate"]:
                            LOGGER.error("Define %r used twice!", define)
                            sys.exit(1)

                        LOGGER.warning(
                            "Define %r used twice but this is allowed", define
                        )
                        continue

                    define_with_whitespace = line.split(f"#define {define}", 1)[1]
                    alignment = define_with_whitespace[
                        : define_with_whitespace.index(define_with_whitespace.strip())
                    ]

                    prev_line = config_h_lines[index - 1]
                    if "#ifndef" in prev_line:
                        assert (
                            re.match(r"#ifndef\s+([A-Z0-9_]+)", prev_line).group(1)
                            == define
                        )

                        # Make sure that we do not have conflicting defines provided over the command line
                        assert not any(
                            c["name"] == define for c in base_project.get("define", [])
                        )
                        new_config_h_lines[index - 1] = "#if 1"
                    elif "#warning" in prev_line:
                        assert re.match(r'#warning ".*? not configured"', prev_line)
                        new_config_h_lines[index - 1] = f"//{prev_line}"

                    value_template = str(config["value"])

                    if value_template.startswith("template:"):
                        value = value_template.replace("template:", "", 1).format(
                            **template_env
                        )
                    else:
                        value = value_template

                    new_config_h_lines.append(f"#define {define}{alignment}{value}")
                    written_config[define] = value
                    written.add(define)
                    break
                else:
                    new_config_h_lines.append(line)

            if written_config:
                LOGGER.info("Patching %s with %s", config_f, written_config)
                config_f.write_text("\n".join(new_config_h_lines))

    return written


def fix_pti_config_warning(build: ResolvedBuild) -> None:
    """PTI seemingly cannot be excluded, even if it is disabled, breaking `-Werror`."""
    sl_rail_util_pti_config_h = build.build_dir / "config/sl_rail_util_pti_config.h"

    if sl_rail_util_pti_config_h.exists():
        sl_rail_util_pti_config_h.write_text(
            sl_rail_util_pti_config_h.read_text().replace(
                '#warning "RAIL PTI peripheral not configured"\n',
                '// #warning "RAIL PTI peripheral not configured"\n',
            )
        )


def sdk_path_remaps(build: ResolvedBuild) -> dict[str, str]:
    """Absolute paths baked into the SDK and its prebuilt libraries, and their remaps."""
    sdk_src = f"/src/{build.sdk_name}_{build.sdk_version}"

    return {
        str(build.build_dir.absolute()): "/src",
        f"{build.cmake_dir.absolute()}/..": "/src",
        # The toolchain's own resource/runtime headers (e.g. clang's builtin and newlib
        # include dirs) are recorded in debug info under a machine-specific conan path
        str(build.toolchain_path): "/src/vendor/toolchain",
        "/home/buildengineer/jenkins/workspace/Gecko_Workspace/gsdk": sdk_src,
        # Zigbee sources reference the GitHub Actions workspace they were packaged in
        "/__w/zigbee/zigbee": f"{sdk_src}/zigbee",
        "/home/buildengineer/.silabs/slt/installs/conan/p/cmsisfb920dbb6ad42/p": "/src/vendor/cmsis",
        "/home/buildengineer/.silabs/slt/installs/conan/p/platf85e95225bc406/p": f"{sdk_src}/platform_core",
        "/home/buildengineer/.silabs/slt/installs/conan/p/platfe548addd6aec0/p": f"{sdk_src}/platform_core",
        # The Z-Wave libraries were packaged against their own conan hashes
        "/home/buildengineer/.silabs/slt/installs/conan/p/cmsis4dea3e6cbb6ce/p": "/src/vendor/cmsis",
        "/home/buildengineer/.silabs/slt/installs/conan/p/platf6edd0cd4d4914/p": f"{sdk_src}/platform_core",
        # Z-Wave's platform_core was repackaged under a new conan hash in Simplicity SDK 2026.6.1
        "/home/buildengineer/.silabs/slt/installs/conan/p/platf0f636d352884d/p": f"{sdk_src}/platform_core",
        # The zigbee stack libraries reference the silabs_core package they were built against
        "/github/home/.silabs/slt/installs/conan/p/commo8335073ce327e/p": f"{sdk_src}/platform_core",
        # silabs_core was repackaged under a new conan hash in Simplicity SDK 2026.6.1
        "/github/home/.silabs/slt/installs/conan/p/commo6146391826d06/p": f"{sdk_src}/platform_core",
        # The Z-Wave SDK isn't part of the Simplicity SDK but is still referenced. If we
        # ever decide to compile it as part of CI, we can change this remap.
        "/opt/github/runner/_work/z-wave/z-wave": "/src/vendor/zwave",
    }


def warning_flags(toolchain: Toolchain) -> list[str]:
    """Warnings that are errors, minus the ones the SDK cannot currently satisfy."""
    return {
        Toolchain.LLVM: [
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unknown-warning-option",  # ignore GCC-only warning names
            "-Wno-error=format-security",  # SDK `diagnostic.c` uses a non-literal format string
            "-Wno-error=unknown-pragmas",  # our `ws2812.c` uses `#pragma GCC optimize`
            "-Wno-error=unused-function",  # unused statics in SDK/OpenThread sources
            "-Wno-error=c23-extensions",  # our `cmds_proprietary.c` has a label before a declaration
            "-Wno-error=unterminated-string-initialization",  # char arrays in zigbee router sources
        ],
        Toolchain.GCC: [
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-error=maybe-uninitialized",  # Linking fails due to a few SDK bugs
            "-Wno-error=uninitialized",  # False positive in zigbee `core-cli.c` under LTO
            "-Wno-error=unused-function",  # mbedTLS `ssl_tls.c` with X.509 hostname verification disabled
        ],
    }[toolchain]


def assemble_build_flags(
    build: ResolvedBuild, c_flag_defines: list[str]
) -> dict[str, list[str]]:
    c_flags = (
        c_flag_defines
        + [
            f"-ffile-prefix-map={src}={dst}"
            for src, dst in sdk_path_remaps(build).items()
        ]
        + warning_flags(build.toolchain)
    )

    return {
        "C_FLAGS": c_flags,
        "CXX_FLAGS": c_flags,
        "LD_FLAGS": [
            # Ensure deterministic linking order
            "-Wl,--sort-section=name",
            # Kept for `validate_linker_wraps`. Non-alloc, so the GBL and HEX are
            # byte-identical with or without it.
            "-Wl,--emit-relocs",
        ],
    }


def cmake_configure_and_build(
    build: ResolvedBuild, build_flags: dict[str, list[str]]
) -> None:
    # The GBL is built in-process after the link, so neutralize the SDK's post-build
    # hook. CMake expects a semicolon-separated list for the command.
    cmake_post_build_command = ";".join([shutil.which("cmake"), "-E", "true"])

    source_date_epoch = str(int(build.build_timestamp.timestamp()))

    # fmt: off
    subprocess_run_verbose(
        [
            "cmake",
            "-G", "Ninja",
            "-D", "CMAKE_TOOLCHAIN_FILE=toolchain.cmake",
            "-D", f"CMAKE_C_FLAGS={' '.join(build_flags['C_FLAGS'])}",
            "-D", f"CMAKE_CXX_FLAGS={' '.join(build_flags['CXX_FLAGS'])}",
            "-D", f"CMAKE_EXE_LINKER_FLAGS={' '.join(build_flags['LD_FLAGS'])}",
            "-D", f"post_build_command={cmake_post_build_command}",
            ".",
        ],
        "cmake",
        env={
            "HOME": os.environ["HOME"],
            "PATH": f"{pathlib.Path(sys.executable).parent}:{os.environ['PATH']}",
            f"ARM_{build.toolchain.upper()}_DIR": build.toolchain_path,
            "NINJA_EXE_PATH": shutil.which("ninja"),
            # Unused, `post_build_command` replaces it. The SDK's toolchain.cmake still
            # requires it to be non-empty, otherwise it errors out.
            "POST_BUILD_EXE": shutil.which("cmake"),
            "SOURCE_DATE_EPOCH": source_date_epoch,
        },
        cwd=build.cmake_dir,
    )
    # fmt: on

    subprocess_run_verbose(
        ["cmake", "--build", "."],
        "cmake --build",
        cwd=build.cmake_dir,
        env={
            "HOME": os.environ["HOME"],
            "PATH": f"{pathlib.Path(sys.executable).parent}:{os.environ['PATH']}",
            "SOURCE_DATE_EPOCH": source_date_epoch,
        },
    )


def verify_build_reproducibility(
    build: ResolvedBuild, output_artifact: pathlib.Path
) -> None:
    """Scrub absolute build paths out of the ELF's DWARF and confirm none are left."""
    out_elf = output_artifact.with_suffix(".out")
    remap_debug_build_paths(
        out_elf,
        {
            str(build.build_dir.resolve()): "/src",
            "/home/buildengineer": "/src/vendor",  # Silicon Labs build machines
            "/github/home": "/src/vendor",  # Silicon Labs Zigbee CI
            "/opt/github": "/src/vendor",  # Silicon Labs Z-Wave CI
            "/__w": "/src",  # GitHub Actions workspace
        },
    )

    unreproducible_paths = [
        path
        for path in get_elf_source_paths(out_elf)
        if not path.is_relative_to("/src")
    ]
    if unreproducible_paths:
        raise RuntimeError(
            "Unreproducible source paths in ELF: "
            + "\n - ".join(map(str, unreproducible_paths))
        )


def main() -> None:
    args = build_argument_parser().parse_args()

    if args.build_timestamp is not None:
        args.build_timestamp = datetime.strptime(
            args.build_timestamp, "%Y%m%d%H%M%S"
        ).replace(tzinfo=UTC)
    else:
        args.build_timestamp = datetime.now(UTC)

    if args.slc_daemon:
        slc = ["slc", "--daemon", "--daemon-timeout", "1"]
    else:
        slc = ["slc"]

    if args.build_dir is None:
        args.build_dir = pathlib.Path(f"build/{time.time():.0f}_{args.manifest.stem}")

    # argparse defaults should be replaced, not extended
    if args.sdks != get_sdk_default_paths():
        args.sdks = args.sdks[len(get_sdk_default_paths()) :]

    if args.toolchains != get_toolchain_default_paths():
        args.toolchains = args.toolchains[len(get_toolchain_default_paths()) :]

    build = resolve_build(args)
    LOGGER.info("Building in %s", build.build_dir.resolve())

    if args.clean_build_dir:
        with contextlib.suppress(OSError):
            shutil.rmtree(build.build_dir)

    copy_base_project(build)
    base_project = prepare_project_slcp(build)

    run_slc_generate(build, slc=slc, tool_paths=args.tool_paths)
    apply_sdk_patches(build)
    validate_sdk_extensions(build, base_project)
    declared_wraps = validate_wrap_declarations(build.base_project_path)

    # Template variables for C defines and the output filename
    template_env = {
        "git_repo_hash": get_git_commit_id(repo=build.projects_root),
        "manifest_name": build.manifest_path.stem,
        "now": build.build_timestamp,
    }

    c_defines = normalize_c_defines(build.manifest)
    c_flag_defines = [
        f"-D{define}={config['value']}"
        for define, config in c_defines.items()
        if config["type"] == "c_flag"
    ]
    written_defines = patch_config_headers(
        config_roots=[build.build_dir / "autogen", build.build_dir / "config"],
        c_defines=c_defines,
        base_project=base_project,
        template_env=template_env,
    )

    unused_defines = (
        set(c_defines)
        - written_defines
        - {define for define, config in c_defines.items() if config["type"] == "c_flag"}
    )
    if unused_defines:
        LOGGER.error("Defines were unused, aborting: %s", unused_defines)
        sys.exit(1)

    fix_pti_config_warning(build)

    # Only our own wraps: SDK wrappers such as `main` hand the `__real_` call off to
    # another function entirely, which the reference check below would flag
    wrapped = set(declared_wraps) & linker_wrap_targets(build)

    definitions = [
        declared_wraps[target]
        for target in sorted(wrapped)
        if declared_wraps[target] is not None
    ]
    exclude_definitions_from_lto(build, definitions)
    cmake_configure_and_build(build, assemble_build_flags(build, c_flag_defines))

    validate_linker_wraps(
        elf_path=(build.cmake_dir / build.base_project_name).with_suffix(".out"),
        targets=wrapped,
    )

    # Extract the metadata from the source and build trees, patch the ELF, and build the
    # GBL. This runs before the debug paths below are rewritten, as the GBL contains only
    # loadable segments and is unaffected by them.
    extracted_gbl_metadata = create_gbl(
        build_dir=build.cmake_dir,
        project_root=build.build_dir,
        gsdk_path=build.sdk,
        project_name=build.base_project_name,
        sdk_version=build.sdk_version,
        gbl_metadata=build.manifest["gbl"],
    )

    output_artifact = (build.cmake_dir / build.base_project_name).with_suffix(".gbl")
    verify_build_reproducibility(build, output_artifact)

    base_filename = evaluate_f_string(
        build.manifest.get("filename", "{manifest_name}"),
        {**template_env, **extracted_gbl_metadata},
    )

    args.output_dir.mkdir(exist_ok=True)

    # Copy the output artifacts
    for extension, output_path in args.outputs:
        if output_path is None:
            output_path = f"{base_filename}.{extension}"

        shutil.copy(
            src=output_artifact.with_suffix(f".{extension}"),
            dst=args.output_dir / output_path,
        )

    if args.clean_build_dir:
        with contextlib.suppress(OSError):
            shutil.rmtree(build.build_dir)

    if args.slc_daemon and not args.keep_slc_daemon:
        subprocess.run(slc + ["daemon-shutdown"], check=True)


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    main()
