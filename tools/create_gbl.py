"""Tool to create a GBL image in a Simplicity Studio build directory."""

from __future__ import annotations

import ast
import json
import pathlib
from typing import Any

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import load_pem_private_key
from pygbl import (
    GBL3Compression,
    build_application_gbl3,
    build_bootloader_gbl3,
    read_encryption_key,
)


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
        except ValueError, SyntaxError:
            pass

    return config


def resolve_key_path(
    key: str, project_root: pathlib.Path, gsdk_path: pathlib.Path
) -> pathlib.Path:
    """Resolve a manifest key path against the generated project, absolutes pass through."""
    return project_root / pathlib.Path(key.format(SDK_DIR=gsdk_path))


def create_gbl(
    build_dir: pathlib.Path,
    project_root: pathlib.Path,
    gsdk_path: pathlib.Path,
    project_name: str,
    sdk_version: str,
    gbl_metadata: dict[str, Any],
) -> dict[str, Any]:
    """Build the GBL image for a linked project, returning its metadata."""
    elf = build_dir / f"{project_name}.out"

    # Prepare the GBL metadata
    metadata = {
        "metadata_version": 2,
        "sdk_version": sdk_version,
        "fw_type": gbl_metadata.get("fw_type"),
        "fw_variant": gbl_metadata.get("fw_variant"),
        "baudrate": gbl_metadata.get("baudrate"),
    }

    # Compute the dynamic metadata
    gbl_dynamic = [k for k, v in gbl_metadata.items() if v == "dynamic"]

    if "ezsp_version" in gbl_dynamic:
        gbl_dynamic.remove("ezsp_version")

        zigbee_config_h = parse_c_header_defines(
            (gsdk_path / "zigbee/stack/config/config.h").read_text()
        )
        special_version = zigbee_config_h["SL_ZIGBEE_SPECIAL_VERSION"]

        # `ezsp_version.c` in the NCP applies the same override at compile time
        xncp_config_path = project_root / "config/xncp_config.h"
        if xncp_config_path.exists():
            xncp_config_h = parse_c_header_defines(xncp_config_path.read_text())
            if xncp_config_h["XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE"] != 0xFF:
                special_version = xncp_config_h["XNCP_EZSP_VERSION_PATCH_NUM_OVERRIDE"]

        metadata["ezsp_version"] = ".".join(
            [
                str(zigbee_config_h["SL_ZIGBEE_MAJOR_VERSION"]),
                str(zigbee_config_h["SL_ZIGBEE_MINOR_VERSION"]),
                str(zigbee_config_h["SL_ZIGBEE_PATCH_VERSION"]),
                str(special_version),
            ]
        )

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
        ot_sdk_path = next(
            (
                path
                for path in (
                    # Gecko SDK and Simplicity SDK up to 2025.6.x
                    (
                        gsdk_path
                        / "protocol/openthread/include/sl_openthread_package_info.h"
                    ),
                    # Simplicity SDK 2025.12.0 and above
                    (gsdk_path / "openthread/include/sl_openthread_package_info.h"),
                )
                if path.exists()
            ),
            None,
        )

        if ot_proj_path.exists():
            openthread_config_h = parse_c_header_defines(ot_proj_path.read_text())
            metadata["ot_rcp_version"] = openthread_config_h["PACKAGE_STRING"]
        elif ot_sdk_path is not None:
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
        btl_config_path = next(
            (
                path
                for path in (
                    # Gecko SDK and Simplicity SDK up to 2025.6.x
                    (gsdk_path / "platform/bootloader/config/btl_config.h"),
                    # Simplicity SDK 2025.12.0 and above
                    (gsdk_path / "bootloader/platform/bootloader/config/btl_config.h"),
                )
                if path.exists()
            ),
            None,
        )
        if btl_config_path is None:
            raise FileNotFoundError("Could not find bootloader btl_config.h")

        btl_config_h = parse_c_header_defines(btl_config_path.read_text())

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

    metadata_json = json.dumps(metadata, sort_keys=True).encode("utf-8")

    with elf.open("rb") as f:
        if gbl_metadata.get("fw_type", None) != "gecko-bootloader":
            image = build_application_gbl3(
                f, metadata=metadata_json if gbl_metadata else None
            )
        else:
            image = build_bootloader_gbl3(
                f, metadata=metadata_json if gbl_metadata else None
            )

    # The order matters: compression only touches program data, encryption then wraps
    # every tag but the header, and the signature covers the resulting ciphertext
    if gbl_metadata.get("compression", None) is not None:
        image = image.compress(GBL3Compression(gbl_metadata["compression"]))

    if gbl_metadata.get("encrypt_key", None) is not None:
        key_path = resolve_key_path(
            gbl_metadata["encrypt_key"], project_root, gsdk_path
        )
        image = image.encrypt(read_encryption_key(key_path.read_text()))

    if gbl_metadata.get("sign_key", None) is not None:
        key_path = resolve_key_path(gbl_metadata["sign_key"], project_root, gsdk_path)
        private_key = load_pem_private_key(key_path.read_bytes(), password=None)
        assert isinstance(private_key, ec.EllipticCurvePrivateKey)
        image = image.sign(private_key)

    # `commander` pads its output to a 4 byte boundary
    elf.with_suffix(".gbl").write_bytes(image.serialize(block_size=4))

    return metadata
