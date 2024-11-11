#!/usr/bin/env python3
"""Tool to create a JSON manifest file for a collection of firmwares."""

from __future__ import annotations

import json
import logging
import hashlib
import pathlib
import argparse

from datetime import datetime, timezone

from universal_silabs_flasher.firmware import parse_firmware_image

_LOGGER = logging.getLogger(__name__)


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "firmware_dir",
        type=pathlib.Path,
        help="Directory containing firmware images",
    )

    args = parser.parse_args()
    manifest = {
        "metadata": {
            "created_at": datetime.now(timezone.utc).isoformat(),
        },
        "firmwares": [],
    }

    for firmware_file in args.firmware_dir.glob("*.gbl"):
        data = firmware_file.read_bytes()

        try:
            firmware = parse_firmware_image(data)
        except ValueError:
            _LOGGER.warning("Ignoring invalid firmware file: %s", firmware_file)
            continue

        try:
            gbl_metadata = firmware.get_nabucasa_metadata()
        except (KeyError, ValueError):
            metadata = None
        else:
            metadata = gbl_metadata.original_json

        manifest["firmwares"].append(
            {
                "filename": firmware_file.name,
                "checksum": f"sha3-256:{hashlib.sha3_256(data).hexdigest()}",
                "size": len(data),
                "metadata": metadata,
                "release_notes": None,
            }
        )

    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
