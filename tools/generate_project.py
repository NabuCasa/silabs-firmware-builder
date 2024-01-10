#!/usr/bin/env python3
"""Tool to create a SLCP project in a Simplicity Studio build directory."""

from __future__ import annotations

import os
import re
import sys
import copy
import shutil
import logging
import pathlib
import argparse
import subprocess

from ruamel.yaml import YAML

# Matches all known chip and board names. Some varied examples:
#   MGM210PB32JIA EFM32GG890F512 MGM12P22F1024GA EZR32WG330F128R69 EFR32FG14P231F256GM32
CHIP_SPECIFIC_REGEX = re.compile(
    r"""
    ^
    (?:
        # Chips
        (?:[a-z]+\d+){2,5}[a-z]*
        |
        # Boards and board-specific config
        brd\d+[a-z](?:_.+)?
    )
    $
""",
    flags=re.IGNORECASE | re.VERBOSE,
)
LOGGER = logging.getLogger(__name__)

yaml = YAML(typ="safe")


def ensure_folder(path: str | pathlib.Path) -> pathlib.Path:
    """Ensure that the path exists and is a folder."""
    path = pathlib.Path(path)

    if not path.is_dir():
        raise argparse.ArgumentTypeError(f"Folder {path} does not exist")

    return path


def get_toolchain_default_path() -> pathlib.Path | None:
    """Return the path to the toolchain."""
    if sys.platform == "darwin":
        return pathlib.Path(
            "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/toolchains/gnu_arm/12.2.rel1_2023.7"
        )

    return None


def get_sdk_default_path() -> pathlib.Path | None:
    """Return the path to the SDK."""
    if sys.platform == "darwin":
        return pathlib.Path("~/SimplicityStudio/SDKs/gecko_sdk").expanduser()

    return None


def main():
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
        "--output-gbl",
        type=pathlib.Path,
        required=True,
        help="Output GBL file",
    )
    parser.add_argument(
        "--build-dir",
        type=pathlib.Path,
        required=True,
        help="Temporary build directory",
    )
    parser.add_argument(
        "--sdk",
        type=ensure_folder,
        default=get_sdk_default_path(),
        required=get_sdk_default_path() is None,
        help="Path to Gecko SDK",
    )
    parser.add_argument(
        "--toolchain",
        type=ensure_folder,
        default=get_toolchain_default_path(),
        required=get_toolchain_default_path() is None,
        help="Path to GCC toolchain",
    )
    parser.add_argument(
        "--postbuild",
        default=pathlib.Path(__file__).parent / "create_gbl.py",
        required=False,
        help="Postbuild executable",
    )

    args = parser.parse_args()

    manifest = yaml.load(args.manifest.read_text())

    # First, load the base project
    projects_root = args.manifest.parent.parent
    base_project_path = projects_root / manifest["base_project"]
    assert base_project_path.is_relative_to(projects_root)
    (base_project_slcp,) = base_project_path.glob("*.slcp")
    base_project = yaml.load(base_project_slcp.read_text())

    output_project = copy.deepcopy(base_project)

    # Strip chip- and board-specific components to modify the base device type
    output_project["component"] = [
        c for c in output_project["component"] if not CHIP_SPECIFIC_REGEX.match(c["id"])
    ]
    output_project["component"].append({"id": manifest["device"]})

    # Add new components
    output_project["component"].extend(manifest.get("add_components", []))

    # Remove components
    for component in manifest.get("remove_components", []):
        try:
            output_project["component"].remove(component)
        except ValueError:
            LOGGER.warning(
                "Component %s is not present in manifest, cannot remove", component
            )

    # Extend configuration and C defines
    for input_config, output_config in [
        (manifest.get("configuration", {}), output_project["configuration"]),
        (manifest.get("slcp_defines", {}), output_project.get("define", [])),
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

    # Copy the base project into the output directory
    args.build_dir.mkdir(exist_ok=True)

    shutil.copytree(
        base_project_path,
        args.build_dir,
        dirs_exist_ok=True,
        ignore=lambda dir, contents: [
            # XXX: pruning `autogen` is extremely important!
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

    # Delete the original project file
    (args.build_dir / base_project_slcp.name).unlink()

    # Delete RAIL config: it will be regenerated
    for filename in (args.build_dir / "config").glob("sl_rail_*.h"):
        filename.unlink()

    # Write the new project SLCP file
    with (args.build_dir / f"{manifest['base_project']}.slcp").open("w") as f:
        yaml.dump(output_project, f)

    # Create a GBL metadata file
    with pathlib.Path(args.build_dir / "gbl_metadata.yaml").open("w") as f:
        yaml.dump(manifest["gbl"], f)

    # Generate a build directory
    args.build_dir = args.build_dir
    cmake_build_root = args.build_dir / f"{manifest['base_project']}_cmake"
    shutil.rmtree(cmake_build_root, ignore_errors=True)

    # On macOS `slc` doesn't execute properly
    slc = shutil.which("slc-cli") or shutil.which("slc")

    if not slc:
        print("`slc` and/or `slc-cli` not found in PATH")
        sys.exit(1)

    subprocess.run(
        [
            slc,
            "generate",
            "--project-file",
            (args.build_dir / f"{manifest['base_project']}.slcp").resolve(),
            "--export-destination",
            args.build_dir.resolve(),
            "--sdk",
            args.sdk.resolve(),
            "--toolchain",
            "toolchain_gcc",
            "--output-type",
            "cmake",
        ],
        check=True,
    )

    # Actually search for C defines within config
    unused_defines = set(manifest.get("c_defines", {}).keys())

    for config_root in [args.build_dir / "autogen", args.build_dir / "config"]:
        for config_f in config_root.glob("*.h"):
            config_h_lines = config_f.read_text().split("\n")
            written_config = {}
            new_config_h_lines = []

            for index, line in enumerate(config_h_lines):
                for define, value in manifest.get("c_defines", {}).items():
                    if f"#define {define} " not in line:
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
                            c["name"] == define for c in output_project["define"]
                        )
                        new_config_h_lines[index - 1] = "#if 1"
                    elif "#warning" in prev_line:
                        assert re.match(r'#warning ".*? not configured"', prev_line)
                        new_config_h_lines.pop(index - 1)

                    new_config_h_lines.append(f"#define {define}{alignment}{value}")
                    written_config[define] = value

                    if define not in unused_defines:
                        print(f"Define {define!r} used twice!")
                        sys.exit(1)

                    unused_defines.remove(define)
                    break
                else:
                    new_config_h_lines.append(line)

            if written_config:
                print(f"Patching {config_f} with {written_config}")
                config_f.write_text("\n".join(new_config_h_lines))

    if unused_defines:
        print(f"Defines were unused, aborting: {unused_defines}")
        sys.exit(1)

    cmake_build_root = args.build_dir / f"{manifest['base_project']}_cmake"
    cmake_config = cmake_build_root / f"{manifest['base_project']}.cmake"
    fixed_cmake = []

    # Fix CMake config quoting bug
    for line in cmake_config.read_text().split("\n"):
        if ">:SHELL:" not in line and (":-imacros " in line or ":-x " in line):
            line = '    "' + line.replace(">:", ">:SHELL:").strip() + '"'

        fixed_cmake.append(line)

    cmake_config.write_text("\n".join(fixed_cmake))

    # Generate the build system
    subprocess.run(
        [
            "cmake",
            "-G",
            "Ninja",
            "-DCMAKE_TOOLCHAIN_FILE=toolchain.cmake",
            ".",
        ],
        cwd=cmake_build_root,
        env={
            **os.environ,
            "ARM_GCC_DIR": args.toolchain,
            "POST_BUILD_EXE": args.postbuild,
        },
        check=True,
    )

    # Build it!
    subprocess.run(
        [
            "ninja",
            "-C",
            cmake_build_root,
        ],
        check=True,
    )

    # Copy the final GBL
    shutil.copy(
        src=(cmake_build_root / manifest["base_project"]).with_suffix(".gbl"),
        dst=args.output_gbl,
    )


if __name__ == "__main__":
    main()
