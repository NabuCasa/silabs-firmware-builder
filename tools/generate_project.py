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
import subprocess

from ruamel.yaml import YAML

SDK = pathlib.Path("~/SimplicityStudio/SDKs/gecko_sdk/").expanduser()
TOOLCHAIN = "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/toolchains/gnu_arm/12.2.rel1_2023.7"
POSTBUILD = pathlib.Path(__file__).parent / "create_gbl.py"


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

manifest_path = pathlib.Path(sys.argv[1])
output_path = pathlib.Path(sys.argv[2])
projects_root = manifest_path.parent.parent

manifest = yaml.load(manifest_path.read_text())

# First, load the base project
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
    (manifest.get("c_defines", {}), output_project.get("define", [])),
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
output_project_root = output_path / manifest["base_project"]
output_path.mkdir(exist_ok=True)
output_project_root.mkdir(exist_ok=True)

shutil.copytree(
    base_project_path,
    output_project_root,
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
    ],
)

# Delete the original project file
(output_project_root / base_project_slcp.name).unlink()

# Write the new project SLCP file
with (output_project_root / f"{manifest['base_project']}.slcp").open("w") as f:
    yaml.dump(output_project, f)

# Create a GBL metadata file
with pathlib.Path(output_project_root / "gbl_metadata.yaml").open("w") as f:
    yaml.dump(manifest["gbl"], f)

# Generate a build directory
build_dir = output_project_root
cmake_build_root = build_dir / f"{manifest['base_project']}_cmake"
shutil.rmtree(cmake_build_root, ignore_errors=True)

subprocess.run(
    [
        "slc-cli",
        "generate",
        "--project-file",
        (output_project_root / f"{manifest['base_project']}.slcp").resolve(),
        "--export-destination",
        build_dir.resolve(),
        "--sdk",
        SDK.resolve(),
        "--toolchain",
        "toolchain_gcc",
        "--output-type",
        "cmake",
    ],
    check=True,
)

# XXX: Copy the source tree into the build dir, slc-cli does not copy the generated files
"""
shutil.copytree(
    base_project_path,
    build_dir,
    dirs_exist_ok=True,
    ignore=lambda dir, contents: [".git"],
)
"""

cmake_build_root = build_dir / f"{manifest['base_project']}_cmake"
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
    env={**os.environ, "ARM_GCC_DIR": TOOLCHAIN, "POST_BUILD_EXE": POSTBUILD},
    check=True,
)

# Build it!
subprocess.run(
    [
        "ninja",
        # "make",
        "-C",
        cmake_build_root,
    ],
    check=True,
)
