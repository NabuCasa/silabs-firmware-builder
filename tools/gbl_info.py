"""Print what a GBL contains: its bootloader and application parts, and their sizes.

python -m tools.gbl_info --key manifests/nabucasa/zwa2/config/keys/vendor_encrypt.key \
    zwa2_controller_1.4.1.gbl
"""

from __future__ import annotations

import argparse
import json
import pathlib

from pygbl import (
    GBL3ApplicationInfo,
    GBL3ApplicationType,
    GBL3Bootloader,
    GBL3EraseProg,
    GBL3Header,
    GBL3Image,
    GBL3Prog,
    GBL3ProgLZ4,
    GBL3ProgLZMA,
    GBL3SeUpgrade,
    GBL3Signature,
    GBL3Type,
    parse_firmware_image,
    read_encryption_key,
)

PROG_TAGS = (GBL3Prog, GBL3EraseProg, GBL3ProgLZ4, GBL3ProgLZMA)


def describe(path: pathlib.Path, key: bytes | None) -> list[str]:
    """Describe a GBL file, decrypting it with `key` if it is encrypted."""
    data = path.read_bytes()
    image = parse_firmware_image(data)

    if not isinstance(image, GBL3Image):
        raise TypeError(f"{path} is not a GBL3 image")

    header = image.get_first_tag(GBL3Header)
    flags = [flag.name for flag in GBL3Type if flag and flag in header.type] or ["NONE"]
    signed = bool(image.get_tags(GBL3Signature, allow_missing=True))
    lines = [
        f"{path.name}: {len(data)} bytes, flags {'|'.join(flags)}, signed {signed}"
    ]

    if GBL3Type.ENCRYPTION_AESCCM in header.type:
        if key is None:
            lines.append("  encrypted, pass --key to show its contents")
            return lines

        image = image.decrypt(key)

    lines.append(f"  tags: {', '.join(type(tag).__name__ for tag in image.tags)}")

    for app_info in image.get_tags(GBL3ApplicationInfo, allow_missing=True):
        app_types = [t.name for t in GBL3ApplicationType if t and t in app_info.type]
        lines.append(
            f"  application info: type {'|'.join(app_types)}, version"
            f" {app_info.version:#010x}, product id {app_info.product_id.hex()}"
        )

    for se_upgrade in image.get_tags(GBL3SeUpgrade, allow_missing=True):
        lines.append(f"  SE upgrade: {len(se_upgrade.data)} bytes")

    for bootloader in image.get_tags(GBL3Bootloader, allow_missing=True):
        lines.append(
            f"  bootloader part: version {bootloader.version}, writes"
            f" {bootloader.address:#010x}..{bootloader.address + len(bootloader.data):#010x}"
            f" ({len(bootloader.data)} bytes)"
        )

    stored = sum(len(tag.data) for tag in image.tags if isinstance(tag, PROG_TAGS))
    ranges = [
        (tag.address, tag.address + len(tag.data))
        for tag in image.decompress().tags
        if isinstance(tag, PROG_TAGS)
    ]

    if ranges:
        start = min(start for start, _ in ranges)
        end = max(end for _, end in ranges)
        lines.append(
            f"  application part: writes {start:#010x}..{end:#010x}"
            f" ({end - start} bytes, {(end - start) / 1024:.1f} KiB),"
            f" {stored} bytes stored in {len(ranges)} tags"
        )

    metadata = image.get_metadata()

    if metadata is not None:
        try:
            lines.append(f"  metadata: {json.dumps(json.loads(metadata))}")
        except ValueError:
            lines.append(f"  metadata: {metadata!r}")

    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument(
        "--key",
        type=pathlib.Path,
        default=None,
        help="Encryption key file, as used by a manifest's `gbl.encrypt_key`",
    )
    parser.add_argument("gbl", type=pathlib.Path, nargs="+")
    args = parser.parse_args()

    key = None if args.key is None else read_encryption_key(args.key.read_text())

    for path in args.gbl:
        print("\n".join(describe(path, key)))


if __name__ == "__main__":
    main()
