#!/usr/bin/env python3
"""Tool to create a GBL image in a Simplicity Studio build directory."""

from __future__ import annotations

import os
import ast
import sys
import json
import struct
import pathlib
import argparse
import subprocess

from typing import BinaryIO
from ruamel.yaml import YAML
from elftools.elf.elffile import ELFFile


def _jump_to_elf_symbol(file: BinaryIO, symbol_name: str) -> tuple[ELFFile, int, int]:
    elf = ELFFile(file)

    symtab = elf.get_section_by_name(".symtab")
    symbols = symtab.get_symbol_by_name(symbol_name)

    if len(symbols) != 1:
        raise ValueError(f"Expected one symbol for {symbol_name!r}, got {symbols}")

    symbol = symbols[0]
    symbol_addr = symbol["st_value"]
    symbol_size = symbol["st_size"]

    for segment in elf.iter_segments():
        if segment["p_type"] != "PT_LOAD":
            continue

        segment_start = segment["p_vaddr"]
        segment_end = segment_start + segment["p_filesz"]

        if segment_start <= symbol_addr < segment_end:
            return (
                elf,
                symbol_addr - segment_start + segment["p_offset"],
                symbol_size,
            )

    raise ValueError("Could not find segment")


def read_elf_symbol(file: BinaryIO, symbol_name: str) -> bytes:
    """
    Read an ELF symbol.
    """
    elf, offset, size = _jump_to_elf_symbol(file, symbol_name)

    file.seek(offset)
    return file.read(size)


def modify_elf_symbol(file: BinaryIO, symbol_name: str, value: bytes) -> None:
    """
    Modify an ELF symbol.
    """
    elf, offset, size = _jump_to_elf_symbol(file, symbol_name)
    assert len(value) == size

    file.seek(offset)
    file.write(value)


def parse_c_header_defines(file_content: str) -> dict[str, str]:
    """
    Parses a C header file's `#define`s.
    """
    config = {}

    for line in file_content.split("\n"):
        if not line.startswith("#define"):
            continue

        _, *key_value = line.split(None, 2)

        if len(key_value) == 2:
            key, value = key_value
        else:
            key, value = key_value + [None]

        try:
            config[key] = ast.literal_eval(value)
        except (ValueError, SyntaxError):
            pass

    return config


def parse_properties_file(file_content: str) -> dict[str, str | list[str]]:
    """
    Parses custom .properties file format into a dictionary.
    Handles double backslashes as escape characters for spaces.
    """
    properties = {}

    for line in file_content.split("\n"):
        line = line.strip()

        if not line or line.startswith("#"):
            continue

        key, value = line.split("=", 1)
        key = key.strip()

        properties[key] = []
        current_value = ""
        i = 0

        while i < len(value):
            if value[i : i + 2] == "\\\\":
                current_value += " "
                i += 2
            elif value[i] == " ":
                properties[key].append(current_value)
                current_value = ""
                i += 1
            else:
                current_value += value[i]
                i += 1

        if current_value:
            properties[key].append(current_value)

    return properties


def find_file_in_parent_dirs(root: pathlib.Path, filename: str) -> pathlib.Path:
    """
    Finds a file in the given directory or any of its parents.
    """
    root = root.resolve()

    while True:
        if (root / filename).exists():
            return root / filename

        if root.parent == root:
            raise FileNotFoundError(
                f"Could not find {filename} in any parent directory"
            )

        root = root.parent


def main():
    # Run as a Simplicity Studio post-build step
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("command", type=str, help="Command to execute: postbuild")
    parser.add_argument("slpb_file", type=pathlib.Path, help="Path to the .slpb file")
    parser.add_argument(
        "--parameter",
        action="append",
        type=lambda kv: kv.split(":"),
        dest="parameters",
        help="Parameters in the format key:value",
    )

    args = parser.parse_args()
    args.parameters = dict(args.parameters)

    project_name = args.slpb_file.stem
    build_dir = pathlib.Path(args.parameters["build_dir"])
    out_file = build_dir / f"{project_name}.out"

    artifact_root = out_file.parent
    project_name = out_file.stem
    slcp_path = find_file_in_parent_dirs(
        root=artifact_root,
        filename=project_name + ".slcp",
    )

    project_root = slcp_path.parent

    if "sdk_dir" in args.parameters:
        gsdk_path = pathlib.Path(args.parameters["sdk_dir"])
    elif "cmake" in str(build_dir):
        gsdk_path = pathlib.Path(
            pathlib.Path(build_dir / f"{project_name}.cmake")
            .read_text()
            .split('set(SDK_PATH "', 1)[1]
            .split('"', 1)[0]
        )
    else:
        raise RuntimeError("Cannot determine SDK directory")

    # Parse the main Simplicity Studio project config
    slcp = YAML(typ="safe").load(slcp_path.read_text())

    gbl_metadata = YAML(typ="safe").load(
        (project_root / "gbl_metadata.yaml").read_text()
    )

    # Prepare the GBL metadata
    metadata = {
        "metadata_version": 2,
        "sdk_version": slcp["sdk"]["version"],
        "fw_type": gbl_metadata.get("fw_type"),
        "fw_variant": gbl_metadata.get("fw_variant"),
        "baudrate": gbl_metadata.get("baudrate"),
    }

    # Compute the dynamic metadata
    gbl_dynamic = [k for k, v in gbl_metadata.items() if v == "dynamic"]

    if "ezsp_version" in gbl_dynamic:
        gbl_dynamic.remove("ezsp_version")

        elf = list(build_dir.glob("*.out"))[0]
        with elf.open("rb") as f:
            ember_version = read_elf_symbol(f, "emberVersion")

        (
            build,
            major,
            minor,
            patch,
            special,
            version_type,
            padding,
        ) = struct.unpack(">HBBBBBB", ember_version)

        # Look for overrides
        xncp_config_h = parse_c_header_defines(
            (project_root / "config/xncp_config.h").read_text()
        )
        if xncp_config_h["XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE"] != 0xFF:
            special = xncp_config_h["XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE"]

            # Write the override back to the ELF
            with elf.open("r+b") as f:
                new_ember_version = struct.pack(
                    ">HBBBBBB",
                    build,
                    major,
                    minor,
                    patch,
                    special,
                    version_type,
                    padding,
                )
                ember_version = modify_elf_symbol(f, "emberVersion", new_ember_version)

        metadata["ezsp_version"] = f"{major}.{minor}.{patch}.{special}"

    if "cpc_version" in gbl_dynamic:
        gbl_dynamic.remove("cpc_version")
        sl_gsdk_version_h = parse_c_header_defines(
            (gsdk_path / "platform/common/inc/sl_gsdk_version.h").read_text()
        )
        metadata["cpc_version"] = ".".join(
            [
                str(sl_gsdk_version_h["SL_GSDK_MAJOR_VERSION"]),
                str(sl_gsdk_version_h["SL_GSDK_MINOR_VERSION"]),
                str(sl_gsdk_version_h["SL_GSDK_PATCH_VERSION"]),
            ]
        )

        try:
            internal_app_config_h = parse_c_header_defines(
                (project_root / "config/internal_app_config.h").read_text()
            )
        except FileNotFoundError:
            internal_app_config_h = {}

        if "CPC_SECONDARY_APP_VERSION_SUFFIX" in internal_app_config_h:
            metadata["cpc_version"] += internal_app_config_h[
                "CPC_SECONDARY_APP_VERSION_SUFFIX"
            ]

    if "zwave_version" in gbl_dynamic:
        gbl_dynamic.remove("zwave_version")
        zw_version_config_h = parse_c_header_defines(
            (project_root / "config/zw_version_config.h").read_text()
        )

        metadata["zwave_version"] = ".".join(
            [
                str(zw_version_config_h["USER_APP_VERSION"]),
                str(zw_version_config_h["USER_APP_REVISION"]),
                str(zw_version_config_h["USER_APP_PATCH"]),
            ]
        )

    if "ot_rcp_version" in gbl_dynamic:
        gbl_dynamic.remove("ot_rcp_version")

        ot_proj_path = project_root / "config/sl_openthread_generic_config.h"
        ot_sdk_path = (
            gsdk_path / "protocol/openthread/include/sl_openthread_package_info.h"
        )

        if ot_proj_path.exists():
            openthread_config_h = parse_c_header_defines(ot_proj_path.read_text())
            metadata["ot_rcp_version"] = openthread_config_h["PACKAGE_STRING"]
        elif ot_sdk_path.exists():
            openthread_package_info_h = parse_c_header_defines(ot_sdk_path.read_text())
            metadata["ot_rcp_version"] = (
                openthread_package_info_h["PACKAGE_NAME"]
                + "/"
                + openthread_package_info_h["PACKAGE_VERSION"]
            )
        else:
            raise FileNotFoundError("Could not find OpenThread package info")

    if "gecko_bootloader_version" in gbl_dynamic:
        gbl_dynamic.remove("gecko_bootloader_version")
        btl_config_h = parse_c_header_defines(
            (gsdk_path / "platform/bootloader/config/btl_config.h").read_text()
        )

        # Look for overrides
        btl_core_config_h = parse_c_header_defines(
            (project_root / "config/btl_core_cfg.h").read_text()
        )

        btl_config = dict(btl_config_h)
        btl_config.update(btl_core_config_h)

        metadata["gecko_bootloader_version"] = ".".join(
            [
                str(btl_config["BOOTLOADER_VERSION_MAIN_MAJOR"]),
                str(btl_config["BOOTLOADER_VERSION_MAIN_MINOR"]),
                str(btl_config["BOOTLOADER_VERSION_MAIN_CUSTOMER"]),
            ]
        )

    if gbl_dynamic:
        raise ValueError(f"Unknown dynamic metadata: {gbl_dynamic}")

    print("Generated GBL metadata:", metadata, flush=True)

    # Write it to a file for `commander` to read
    (artifact_root / "gbl_metadata.json").write_text(
        json.dumps(metadata, sort_keys=True)
    )

    # Make sure the Commander binary is included in the PATH on macOS
    if sys.platform == "darwin":
        os.environ["PATH"] += (
            os.pathsep
            + "/Applications/Simplicity Studio.app/Contents/Eclipse/developer/adapter_packs/commander/Commander.app/Contents/MacOS"
        )

    commander_args = [
        "commander",
        "gbl",
        "create",
        out_file.with_suffix(".gbl"),
        (
            "--app"
            if gbl_metadata.get("fw_type", None) != "gecko-bootloader"
            else "--bootloader"
        ),
        out_file,
    ] + (
        [
            "--metadata",
            (artifact_root / "gbl_metadata.json"),
        ]
        if gbl_metadata
        else []
    )

    if gbl_metadata.get("compression", None) is not None:
        commander_args += ["--compress", gbl_metadata["compression"]]

    if gbl_metadata.get("sign_key", None) is not None:
        commander_args += ["--sign", gbl_metadata["sign_key"].format(SDK_DIR=gsdk_path)]

    if gbl_metadata.get("encrypt_key", None) is not None:
        commander_args += [
            "--encrypt",
            gbl_metadata["encrypt_key"].format(SDK_DIR=gsdk_path),
        ]

    # Finally, generate the GBL
    subprocess.run(commander_args, check=True)


if __name__ == "__main__":
    main()
