"""Tool to create a JSON manifest file for a collection of firmwares."""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import pathlib
import re
import sys
import typing
from datetime import UTC, datetime

from pygbl import (
    FirmwareImage,
    GBL3Header,
    GBL3Image,
    GBL3Type,
    GBLError,
    parse_firmware_image,
    read_encryption_key,
)

from .build_project import Manifest

_LOGGER = logging.getLogger(__name__)

# Metadata fields copied verbatim from the manifest's `gbl` section by `create_gbl`
STATIC_METADATA_KEYS = ("fw_type", "fw_variant", "baudrate")


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


def load_manifests(directory: pathlib.Path) -> list[Manifest]:
    """Load every build manifest below a directory."""
    paths = sorted(
        path for pattern in ("*.yaml", "*.yml") for path in directory.rglob(pattern)
    )

    return [Manifest.load(path) for path in paths]


def is_encrypted(firmware: FirmwareImage) -> bool:
    """Whether an image's payload is encrypted, and so needs a key to be read."""
    if not isinstance(firmware, GBL3Image):
        return False

    return GBL3Type.ENCRYPTION_AESCCM in firmware.get_first_tag(GBL3Header).type


def unlock(manifest: Manifest, firmware: FirmwareImage) -> FirmwareImage | None:
    """Open an image with a manifest's own key, or `None` if it did not build it."""
    if is_encrypted(firmware) != (manifest.encryption_key_path is not None):
        return None

    if manifest.encryption_key_path is None:
        return firmware

    key = read_encryption_key(manifest.encryption_key_path.read_text())

    try:
        return firmware.decrypt(key)
    except GBLError:
        return None


def metadata_mismatches(
    manifest: Manifest, metadata: dict[str, typing.Any]
) -> list[str]:
    """Describe every way an image's metadata disagrees with its manifest."""
    expected = {
        "sdk_version": manifest.sdk_version,
        **{key: manifest.gbl.get(key) for key in STATIC_METADATA_KEYS},
    }

    mismatches = [
        f"{key}: manifest declares {value!r}, image has {metadata.get(key)!r}"
        for key, value in expected.items()
        if metadata.get(key) != value
    ]

    mismatches.extend(
        f"{key}: manifest declares it dynamic but the image has no such key"
        for key, value in manifest.gbl.items()
        if value == "dynamic" and key not in metadata
    )

    return mismatches


def find_manifest(
    manifests: list[Manifest], firmware: FirmwareImage, stem: str
) -> tuple[Manifest, dict[str, typing.Any]] | None:
    """Find the manifest whose build produced a firmware file, and read its metadata."""
    matches = []

    for manifest in manifests:
        image = unlock(manifest, firmware)

        if image is None:
            continue

        raw_metadata = image.get_metadata()

        if raw_metadata is None:
            continue

        metadata = json.loads(raw_metadata)

        # `fw_type` is checked first: the output filename of an unrelated firmware is
        # templated on metadata this image does not carry
        if (
            manifest.gbl.get("fw_type") == metadata["fw_type"]
            and manifest.output_stem(metadata) == stem
        ):
            matches.append((manifest, metadata))

    if len(matches) != 1:
        return None

    return matches[0]


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--manifests",
        type=pathlib.Path,
        required=True,
        help="Directory containing the build manifests the firmwares were built from",
    )
    parser.add_argument(
        "--artifacts",
        type=pathlib.Path,
        required=True,
        help="Directory containing firmware images",
    )

    args = parser.parse_args()

    manifests = load_manifests(args.manifests)
    _LOGGER.info("Loaded %d manifests from %s", len(manifests), args.manifests)

    output = {
        "metadata": {
            "created_at": datetime.now(UTC).isoformat(),
        },
        "changelogs": {},
        "firmwares": [],
    }

    inconsistent = False
    matched: dict[pathlib.Path, Manifest] = {}
    changelogs: dict[str, list[dict[str, str | None]]] = {}

    for firmware_file in sorted(args.artifacts.glob("*.gbl")):
        data = firmware_file.read_bytes()

        try:
            firmware = parse_firmware_image(data)
        except GBLError:
            _LOGGER.warning("Ignoring invalid firmware file: %s", firmware_file)
            continue

        match = find_manifest(manifests, firmware, firmware_file.stem)

        if match is None:
            _LOGGER.error(
                "Firmware %s matches no manifest in %s", firmware_file, args.manifests
            )
            inconsistent = True
            continue

        source_manifest, metadata = match
        mismatches = metadata_mismatches(source_manifest, metadata)

        if mismatches:
            _LOGGER.error(
                "Firmware %s disagrees with %s:\n - %s",
                firmware_file,
                source_manifest.path,
                "\n - ".join(mismatches),
            )
            inconsistent = True
            continue

        matched[source_manifest.path] = source_manifest
        version_key = source_manifest.version_key
        version = metadata[version_key] if version_key else None

        fw = {
            "filename": firmware_file.name,
            "version": version,
            "checksum": f"sha3-256:{hashlib.sha3_256(data).hexdigest()}",
            "size": len(data),
            "metadata": metadata,
            "release_notes": None,
            "release_summary": None,
        }
        output["firmwares"].append(fw)

        # The manifest names the source directory the changelog lives in
        fw_type = metadata["fw_type"]
        changelog_md = source_manifest.base_project_path / "CHANGELOG.md"

        if fw_type not in changelogs:
            if changelog_md.exists():
                changelogs[fw_type] = parse_markdown_changelog(changelog_md.read_text())
            else:
                changelogs[fw_type] = []

        if not changelogs[fw_type]:
            continue

        entry = next((e for e in changelogs[fw_type] if e["version"] == version), None)

        if entry is None:
            _LOGGER.error(
                "Firmware %s version %s has no changelog entry in %s",
                firmware_file,
                version,
                changelog_md,
            )
            inconsistent = True
            continue

        # These two fields are kept for backwards compatibility with older clients that
        # predate `changelogs`. Their names are inverted: `release_notes` holds the
        # one-line summary and `release_summary` holds the detailed body.
        fw["release_notes"] = entry["summary"]
        fw["release_summary"] = entry["notes"]

    for unbuilt in manifests:
        if unbuilt.path not in matched:
            _LOGGER.warning("Manifest %s produced no firmware", unbuilt.path)

    output["changelogs"] = {t: e for t, e in changelogs.items() if e}

    if inconsistent:
        sys.exit(1)

    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    main()
