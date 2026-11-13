"""Tool to create a GBL image in a Simplicity Studio build directory."""

from __future__ import annotations

import ast
import json
import pathlib
from typing import Any

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import load_pem_private_key
from pygbl import (
    GBL3Bootloader,
    GBL3Compression,
    GBL3EraseProg,
    GBL3Header,
    GBL3Image,
    GBL3Prog,
    GBL3Type,
    build_application_gbl3,
    build_bootloader_gbl3,
    parse_firmware_image,
    read_application_properties,
    read_encryption_key,
)

# `application_properties_t` in the SDK's `application_properties.h`
APP_SIGNATURE_TYPE_OFFSET = 20
APP_SIGNATURE_LOCATION_OFFSET = 24
APP_SIGNATURE_SIZES = {
    0: 0,  # APPLICATION_SIGNATURE_NONE
    1 << 0: 64,  # APPLICATION_SIGNATURE_ECDSA_P256
    1 << 1: 4,  # APPLICATION_SIGNATURE_CRC32
}


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


def application_end(image: GBL3Image, app_properties: bytes) -> int:
    """The address just past the last byte an application image writes to flash."""
    end = max(
        tag.address + len(tag.data)
        for tag in image.tags
        if isinstance(tag, (GBL3Prog, GBL3EraseProg))
    )

    signature_type = int.from_bytes(
        app_properties[APP_SIGNATURE_TYPE_OFFSET : APP_SIGNATURE_TYPE_OFFSET + 4],
        "little",
    )
    signature_location = int.from_bytes(
        app_properties[
            APP_SIGNATURE_LOCATION_OFFSET : APP_SIGNATURE_LOCATION_OFFSET + 4
        ],
        "little",
    )

    if signature_type not in APP_SIGNATURE_SIZES:
        raise ValueError(f"Unknown application signature type {signature_type:#x}")

    if signature_type != 0:
        end = max(end, signature_location + APP_SIGNATURE_SIZES[signature_type])

    return end


def check_application_end(end: int, limit: int) -> None:
    """Fail when an application writes past the `max_app_end` address."""
    if end > limit:
        raise ValueError(
            f"Application ends at {end:#010x}, {end - limit} bytes past the"
            f" `max_app_end` limit of {limit:#010x}"
        )

    print(
        f"Application ends at {end:#010x}, {limit - end} bytes below {limit:#010x}",
        flush=True,
    )


def check_gbl_size(name: str, data: bytes, limit: int | None) -> None:
    """Fail when a serialized GBL is larger than `limit` bytes."""
    if limit is None:
        return

    if len(data) > limit:
        raise ValueError(
            f"{name} is {len(data)} bytes, {len(data) - limit} bytes over the"
            f" `max_size` limit of {limit} bytes"
        )

    print(f"{name} is {len(data)} bytes, {limit - len(data)} below {limit}", flush=True)


def load_bootloader_tag(
    path: pathlib.Path, encryption_key_path: pathlib.Path | None
) -> GBL3Bootloader:
    """Read the bootloader tag from a bootloader ELF or a bootloader-only GBL."""
    if path.suffix == ".out":
        with path.open("rb") as f:
            image = build_bootloader_gbl3(f)
    elif path.suffix == ".gbl":
        image = parse_firmware_image(path.read_bytes())

        if not isinstance(image, GBL3Image):
            raise TypeError(f"{path} is not a GBL3 image")

        if GBL3Type.ENCRYPTION_AESCCM in image.get_first_tag(GBL3Header).type:
            if encryption_key_path is None:
                raise ValueError(f"{path} is encrypted and the manifest has no key")

            image = image.decrypt(read_encryption_key(encryption_key_path.read_text()))
    else:
        raise ValueError(f"Bootloader must be an `.out` or `.gbl` file: {path}")

    if image.get_tags(GBL3Prog, allow_missing=True) or image.get_tags(
        GBL3EraseProg, allow_missing=True
    ):
        raise ValueError(f"{path} contains application data")

    return image.get_first_tag(GBL3Bootloader)


def finalize_gbl(
    image: GBL3Image,
    gbl_metadata: dict[str, Any],
    project_root: pathlib.Path,
    gsdk_path: pathlib.Path,
) -> bytes:
    """Compress, encrypt and sign an image as the manifest says, then serialize it."""
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
    return image.serialize(block_size=4)


def create_gbl(
    build_dir: pathlib.Path,
    project_root: pathlib.Path,
    gsdk_path: pathlib.Path,
    project_name: str,
    sdk_version: str,
    gbl_metadata: dict[str, Any],
    bootloader: GBL3Bootloader | None = None,
) -> dict[str, Any]:
    """Build the GBL image for a linked project, returning its metadata.

    With `bootloader`, also write `<project_name>.combined.gbl`, which holds that
    bootloader and the application.
    """
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

    is_bootloader = gbl_metadata.get("fw_type", None) == "gecko-bootloader"

    with elf.open("rb") as f:
        if not is_bootloader:
            image = build_application_gbl3(
                f, metadata=metadata_json if gbl_metadata else None
            )

            if "max_app_end" in gbl_metadata:
                f.seek(0)
                end = application_end(image, read_application_properties(f))
                check_application_end(end, gbl_metadata["max_app_end"])
        else:
            image = build_bootloader_gbl3(
                f, metadata=metadata_json if gbl_metadata else None
            )

    max_size = gbl_metadata.get("max_size")
    gbl = finalize_gbl(image, gbl_metadata, project_root, gsdk_path)
    check_gbl_size(elf.with_suffix(".gbl").name, gbl, max_size)
    elf.with_suffix(".gbl").write_bytes(gbl)

    if bootloader is not None:
        if is_bootloader:
            raise ValueError("A bootloader build cannot be combined with a bootloader")

        combined = image.combine_bootloader_app(
            GBL3Image(tags=[image.get_first_tag(GBL3Header), bootloader])
        )
        combined_gbl = finalize_gbl(combined, gbl_metadata, project_root, gsdk_path)
        combined_path = elf.with_suffix(".combined.gbl")
        check_gbl_size(combined_path.name, combined_gbl, max_size)
        combined_path.write_bytes(combined_gbl)

    return metadata
