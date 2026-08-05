#!/usr/bin/env python3
"""Tool to create a JSON manifest file for a collection of firmwares."""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import pathlib
import re
import sys
from datetime import UTC, datetime

from universal_silabs_flasher.firmware import parse_firmware_image

_LOGGER = logging.getLogger(__name__)


def parse_markdown_changelog(text: str) -> list[dict[str, str | None]]:
    """Parse a changelog into an ordered list of entries, newest first."""
    entries = []
    chunks = re.split(r"^# (.*?)\n", text, flags=re.MULTILINE)[1:]

    for version, raw_text in zip(chunks[::2], chunks[1::2]):
        first_line, rest = raw_text.split("\n", 1)

        if len(first_line) > 255:
            raise ValueError(
                "First line of every changelog must be less than 255 characters"
            )

        entries.append(
            {
                "version": version,
                "summary": first_line,
                "notes": rest.strip() or None,
            }
        )

    return entries


def get_firmware_version(metadata: dict) -> str | None:
    """Extract the firmware version from its metadata, if it is versioned at all."""
    version_keys = {k for k in metadata if k.endswith("_version")} - {
        "sdk_version",
        "metadata_version",
    }

    # Some firmwares, such as the Zigbee router, carry no version of their own
    if not version_keys:
        return None

    (version_key,) = version_keys

    return metadata[version_key]


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
            "created_at": datetime.now(UTC).isoformat(),
        },
        "changelogs": {},
        "firmwares": [],
    }

    for firmware_file in sorted(args.firmware_dir.glob("*.gbl")):
        data = firmware_file.read_bytes()

        try:
            firmware = parse_firmware_image(data)
        except ValueError:
            _LOGGER.warning("Ignoring invalid firmware file: %s", firmware_file)
            continue

        try:
            gbl_metadata = firmware.get_nabucasa_metadata()
        except KeyError, ValueError:
            metadata = None
        else:
            metadata = gbl_metadata.original_json

        manifest["firmwares"].append(
            {
                "filename": firmware_file.name,
                "version": get_firmware_version(metadata) if metadata else None,
                "checksum": f"sha3-256:{hashlib.sha3_256(data).hexdigest()}",
                "size": len(data),
                "metadata": metadata,
                "release_notes": None,
                "release_summary": None,
            }
        )

    missing_changelogs = False
    changelogs: dict[str, list[dict[str, str | None]]] = {}

    for fw in manifest["firmwares"]:
        if fw["metadata"] is None:
            continue

        fw_type = fw["metadata"]["fw_type"]
        changelog_md = args.source_dir / fw_type / "CHANGELOG.md"

        if fw_type not in changelogs:
            if changelog_md.exists():
                changelogs[fw_type] = parse_markdown_changelog(changelog_md.read_text())
            else:
                changelogs[fw_type] = []

        if not changelogs[fw_type]:
            continue

        entry = next(
            (e for e in changelogs[fw_type] if e["version"] == fw["version"]), None
        )

        if entry is None:
            _LOGGER.error(
                "Firmware %s version %s has no changelog entry in %s",
                fw["filename"],
                fw["version"],
                changelog_md,
            )
            missing_changelogs = True
            continue

        # These two fields are kept for backwards compatibility with older clients that
        # predate `changelogs`. Their names are inverted: `release_notes` holds the
        # one-line summary and `release_summary` holds the detailed body.
        fw["release_notes"] = entry["summary"]
        fw["release_summary"] = entry["notes"]

    manifest["changelogs"] = {t: e for t, e in changelogs.items() if e}

    if missing_changelogs:
        sys.exit(1)

    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
