#!/usr/bin/env python3
"""Tool to create a SLCP project in a Simplicity Studio build directory."""

from __future__ import annotations

import re
import sys
import copy
import shutil
import logging
import pathlib

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
output_project["project_name"] = manifest["output_project"]

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
output_project_root = output_path / manifest["output_project"]
output_path.mkdir(exist_ok=True)
output_project_root.mkdir(exist_ok=True)

shutil.copytree(
    base_project_path,
    output_project_root,
    dirs_exist_ok=True,
    ignore=lambda dir, contents: [".git"],
)

# Delete the original project file
(output_project_root / base_project_slcp.name).unlink()

# Write the new project SLCP file
with (output_project_root / "project.slcp").open("w") as f:
    yaml.dump(output_project, f)

# Generate a build directory

"""
# Remove the old Simplicity Studio project file
(output_project_root / base_project_slcp.name).with_suffix(".slps").unlink()
studio_project_file = (output_project_root / manifest["output_project"]).with_suffix(".slps")

# Extra pintool data to map chip labels to their part IDs
pintool_paths = {}

for device_f in pathlib.Path("/Applications/Simplicity Studio.app/Contents/Eclipse/offline").glob("com.silabs.sdk.stack.super_*/platform/hwconf_data/pin_tool/*/*/*.device"):
    root = ElementTree.parse(device_f)
    device = root.find('.//device[@partId]')
    pintool_data[device.attribute['label']] = device.attribute['partId']


# Rewrite it
root = ElementTree.parse(studio_project_file)

for xpath, value in {
    ".//descriptors/@name": manifest["output_project"],
    ".//properties[@key='universalConfig.relativeWorkspacePath']/@value": f"../{manifest['output_project']}.slcw",
    ".//properties[@key='projectCommon.boardIds']/@value": "com.silabs.board.none:0.0.0",
    ".//properties[@key='projectCommon.partId']/@value": "mcu.arm.efr32.mg21.efr32mg21a020f512im32",
}.items():
    xpath, _, attribute = xpath.rpartition('/@')
    element = root.find(xpath, namespaces={"model": "http://www.silabs.com/ss/Studio.ecore"})
    element.set(attribute, value)

# Serialize the result back to a string
updated_xml = ElementTree.tostringZ(root, encoding='unicode')


studio_project_f.write_text(
    studio_project_file.read_text().replace()
)
"""
