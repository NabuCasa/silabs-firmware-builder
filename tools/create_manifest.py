#!/usr/bin/env python3
"""Tool to create a JSON manifest file for a collection of firmwares."""

from __future__ import annotations

import re
import json
import logging
import hashlib
import pathlib
import argparse

from datetime import datetime, timezone

from universal_silabs_flasher.firmware import parse_firmware_image

_LOGGER = logging.getLogger(__name__)


def parse_markdown_changelog(text: str) -> dict[str, tuple[str, str]]:
    changelogs = {}
    chunks = re.split(r"^# (.*?)\n", text, flags=re.MULTILINE)[1:]

    for version, raw_text in zip(chunks[::2], chunks[1::2]):
        first_line, rest = raw_text.split("\n", 1)

        if len(first_line) > 255:
            raise ValueError(
                "First line of every changelog must be less than 255 characters"
            )

        changelogs[version] = (first_line, rest.strip() or None)

    return changelogs


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "firmware_dir",
        type=pathlib.Path,
        help="Directory containing firmware images",
    )
    parser.add_argument(
        "source_dir",
        type=pathlib.Path,
        help="Directory containing the source tree to identify changelogs",
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
                "release_summary": None,
            }
        )

    for fw in manifest["firmwares"]:
        if fw["metadata"] is None:
            continue

        fw_type = fw["metadata"]["fw_type"]
        changelog_md = args.source_dir / fw_type / "CHANGELOG.md"

        if not changelog_md.exists():
            continue

        changelogs = parse_markdown_changelog(changelog_md.read_text())

        version_keys = {k for k in fw["metadata"] if k.endswith("_version")} - {
            "sdk_version",
            "metadata_version",
        }
        assert len(version_keys) == 1
        version_key = list(version_keys)[0]

        version = fw["metadata"][version_key]
        release_notes, release_summary = changelogs[version]
        fw["release_notes"] = release_notes
        fw["release_summary"] = release_summary

    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
