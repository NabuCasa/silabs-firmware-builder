import json
import argparse
import pathlib

import enum
import dataclasses

from universal_silabs_flasher.firmware import GBLImage, GBLTagId


@dataclasses.dataclass(frozen=True)
class ManufacturingToken:
    name: str
    address: int
    size: int
    string: bool = False


# Mapping is here: gecko_sdk_4.4.4/platform/service/token_manager/inc/sl_token_manufacturing_series_2.h
# fmt: off
MANUFACTURING_TOKENS = {
    "TOKEN_MFG_EMBER_EUI_64"              : ManufacturingToken("TOKEN_MFG_EMBER_EUI_64",               address=0x1f0, size= 8),
    "TOKEN_MFG_CUSTOM_EUI_64"             : ManufacturingToken("TOKEN_MFG_CUSTOM_EUI_64",              address=0x002, size= 8),
    "TOKEN_MFG_CUSTOM_VERSION"            : ManufacturingToken("TOKEN_MFG_CUSTOM_VERSION",             address=0x00C, size= 2),
    "TOKEN_MFG_STRING"                    : ManufacturingToken("TOKEN_MFG_STRING",                     address=0x010, size=16, string=True),
    "TOKEN_MFG_BOARD_NAME"                : ManufacturingToken("TOKEN_MFG_BOARD_NAME",                 address=0x020, size=16, string=True),
    "TOKEN_MFG_MANUF_ID"                  : ManufacturingToken("TOKEN_MFG_MANUF_ID",                   address=0x030, size= 2),
    "TOKEN_MFG_PHY_CONFIG"                : ManufacturingToken("TOKEN_MFG_PHY_CONFIG",                 address=0x034, size= 2),
    "TOKEN_MFG_ASH_CONFIG"                : ManufacturingToken("TOKEN_MFG_ASH_CONFIG",                 address=0x038, size=40),
    "TOKEN_MFG_SYNTH_FREQ_OFFSET"         : ManufacturingToken("TOKEN_MFG_SYNTH_FREQ_OFFSET",          address=0x060, size= 2),
    "TOKEN_MFG_CCA_THRESHOLD"             : ManufacturingToken("TOKEN_MFG_CCA_THRESHOLD",              address=0x064, size= 2),
    "TOKEN_MFG_EZSP_STORAGE"              : ManufacturingToken("TOKEN_MFG_EZSP_STORAGE",               address=0x068, size= 8),
    "TOKEN_MFG_XO_TUNE"                   : ManufacturingToken("TOKEN_MFG_XO_TUNE",                    address=0x070, size= 2),
    "TOKEN_MFG_ZWAVE_COUNTRY_FREQ"        : ManufacturingToken("TOKEN_MFG_ZWAVE_COUNTRY_FREQ",         address=0x074, size= 1),
    "TOKEN_MFG_ZWAVE_HW_VERSION"          : ManufacturingToken("TOKEN_MFG_ZWAVE_HW_VERSION",           address=0x078, size= 1),
    "TOKEN_MFG_ZWAVE_PSEUDO_RANDOM_NUMBER": ManufacturingToken("TOKEN_MFG_ZWAVE_PSEUDO_RANDOM_NUMBER", address=0x07C, size=16),
    "TOKEN_MFG_SERIAL_NUMBER"             : ManufacturingToken("TOKEN_MFG_SERIAL_NUMBER",              address=0x08C, size=16),
    "TOKEN_MFG_LFXO_TUNE"                 : ManufacturingToken("TOKEN_MFG_LFXO_TUNE",                  address=0x09C, size= 1),
    "TOKEN_MFG_CTUNE"                     : ManufacturingToken("TOKEN_MFG_CTUNE",                      address=0x100, size= 2),
    "TOKEN_MFG_KIT_SIGNATURE"             : ManufacturingToken("TOKEN_MFG_KIT_SIGNATURE",              address=0x104, size= 4),
}
# fmt: on


def make_padded_string(text: str) -> bytes:
    assert len(text.encode("utf-8")) <= 16

    result = (text.encode("utf-8") + b"\x00").ljust(16, b"\xff")[:16]
    assert len(result) == 16

    return result


def hexdump(
    data: bytes, start: int = 0, first: int | None = None, last: int | None = None
) -> list[str]:
    lines = []

    def add_lines(sub_data: bytes, sub_start: int) -> None:
        offset = 0
        while offset < len(sub_data):
            chunk = sub_data[offset : offset + 16]
            hex_values = " ".join(f"{byte:02x}" for byte in chunk)
            ascii_values = bytes(
                byte if 32 <= byte <= 126 else b"."[0] for byte in chunk
            ).decode("ascii")
            lines.append(
                f"{sub_start + offset:08X}  {hex_values:<48}  |{ascii_values}|"
            )
            offset += 16

    if first is not None and last is not None and len(data) > first + last:
        add_lines(data[:first], start)
        lines.append("...")
        add_lines(data[len(data) - last :], start + len(data) - last)
    else:
        add_lines(data, start)

    return lines


class RecordType(enum.IntEnum):
    DATA = 0x00
    END_OF_FILE = 0x01
    EXTENDED_SEGMENT_ADDRESS = 0x02
    START_SEGMENT_ADDRESS = 0x03
    EXTENDED_LINEAR_ADDRESS = 0x04
    START_LINEAR_ADDRESS = 0x05


@dataclasses.dataclass
class Record:
    address: int
    type: RecordType
    data: bytes


class IntelHex:
    def __init__(self, *, record_size: int = 16):
        self.records = []
        self.record_size = record_size

    @classmethod
    def from_bytes(cls, text):
        instance = cls()

        for line in text.splitlines():
            line = line.decode("ascii")
            assert line.startswith(":") or not line.strip()

            line_data = bytes.fromhex(line[1:])
            assert len(line_data) == line_data[0] + 2 + 1 + 1 + 1
            assert sum(line_data) & 0xFF == 0

            instance.records.append(
                Record(
                    address=int.from_bytes(line_data[1:3], "big"),
                    type=RecordType(line_data[3]),
                    data=line_data[4:-1],
                )
            )

        record_sizes = {len(record.data) for record in instance.records}

        if max(record_sizes) > 16:
            raise ValueError(
                f"Inconsistent record sizes in HEX file: {record_sizes}, expected just 16"
            )

        return instance

    def to_bytes(self, line_ending="\r\n"):
        lines = []

        for record in self.records:
            data = bytes(
                [
                    len(record.data),
                    *record.address.to_bytes(2, "big"),
                    record.type,
                    *record.data,
                ]
            )

            checksum = (-sum(data)) & 0xFF
            lines.append(f":{data.hex().upper()}{checksum:02X}{line_ending}")

        return "".join(lines).encode("ascii")

    def flash_data(self, address: int, data: bytes) -> None:
        offset = 0
        extended_linear_address = float("-inf")

        while True:
            chunk = data[offset : offset + self.record_size]

            if not chunk:
                break

            # We always emit an extended linear address record, even if it is not
            # necessary. Otherwise, we cannot be sure if the DATA records we append
            # will inherit a previously set extended linear address base.
            if (address + offset) - extended_linear_address > 0xFFFF:
                self.records.append(
                    Record(
                        address=0,
                        type=RecordType.EXTENDED_LINEAR_ADDRESS,
                        data=((address + offset) >> 16).to_bytes(2, "big"),
                    )
                )
                extended_linear_address = (address + offset) & 0xFFFF0000

            self.records.append(
                Record(
                    address=(address + offset) - extended_linear_address,
                    type=RecordType.DATA,
                    data=chunk,
                )
            )

            offset += len(chunk)

    def validate(self) -> list[tuple[int, int, bytes]]:
        flashed_extents = []
        base_address = 0x00000000

        for index, record in enumerate(self.records):
            if len(record.data) > self.record_size:
                raise ValueError(f"Unexpected record size: {record}")

            if record.type is RecordType.EXTENDED_LINEAR_ADDRESS:
                base_address = int.from_bytes(record.data, "big") << 16
            elif record.type is RecordType.DATA:
                absolute_address = base_address + record.address
                current_start = absolute_address
                current_end = absolute_address + len(record.data)

                flashed_extents.append((current_start, current_end, record.data))
            elif record.type is RecordType.END_OF_FILE:
                break
            else:
                raise ValueError(f"Unsupported record type: {record.type}")
        else:
            raise ValueError("No end-of-file record found")

        if self.records[index + 1 :]:
            raise ValueError("Records found after end-of-file record")

        # Validate that the flashed regions do not overlap
        contiguous_extents = []

        for start, end, data in sorted(flashed_extents):
            if not contiguous_extents:
                contiguous_extents.append((start, end, data))
                continue

            last_start, last_end, last_data = contiguous_extents[-1]

            if start < last_end:
                raise ValueError(
                    f"Flashed regions overlap: 0x{last_start:08X}-0x{last_end:08X} and 0x{start:08X}-0x{end:08X}"
                )

            if last_end == start:
                # Extend the last region if it is contiguous
                contiguous_extents[-1] = (last_start, end, last_data + data)
            else:
                # Otherwise, start a new one
                contiguous_extents.append((start, end, data))

        return contiguous_extents

    def finalize(self) -> None:
        self.records.append(
            Record(
                address=0,
                type=RecordType.END_OF_FILE,
                data=b"",
            )
        )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create an Intel HEX file combining a bootloader, application, and manufacturing tokens."
    )
    parser.add_argument(
        "--bootloader",
        type=pathlib.Path,
        required=True,
        help="Path to the bootloader GBL file.",
    )
    parser.add_argument(
        "--application",
        type=pathlib.Path,
        required=True,
        help="Path to the application GBL file.",
    )
    parser.add_argument(
        "--tokens",
        type=pathlib.Path,
        required=True,
        help="Path to tokens JSON.",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        required=True,
        help="Output HEX file.",
    )
    args = parser.parse_args()

    output_hex = IntelHex()

    # Flash the bootloader
    bootloader_gbl = GBLImage.from_bytes(args.bootloader.read_bytes())
    bootloader_data = bootloader_gbl.get_first_tag(GBLTagId.BOOTLOADER)
    bootloader_hdr, bootloader = bootloader_data[:8], bootloader_data[8:]
    _bootloader_version = int.from_bytes(bootloader_hdr[0:4], "little")
    bootloader_base_addr = int.from_bytes(bootloader_hdr[4:8], "little")

    output_hex.flash_data(address=bootloader_base_addr, data=bootloader)

    # Flash the application
    application_gbl = GBLImage.from_bytes(args.application.read_bytes())
    application_data = application_gbl.get_first_tag(GBLTagId.PROGRAM_DATA2)
    application_hdr, application = application_data[:4], application_data[4:]
    application_base_addr = int.from_bytes(application_hdr[0:4], "little")

    output_hex.flash_data(address=application_base_addr, data=application)

    # Flash USERDATA tokens
    tokens_json = json.loads(args.tokens.read_text())

    for token, value in tokens_json["znet"].items():
        token_info = MANUFACTURING_TOKENS[token]

        if token_info.string:
            token_data = make_padded_string(value)
        else:
            token_data = bytes.fromhex(value)

        assert len(token_data) == token_info.size

        output_hex.flash_data(
            # User Data is mapped to 2kB at USERDATA_BASE (0x0FE00000-0x0FE007FF)
            address=0x0FE00000 | token_info.address,
            data=token_data,
        )

    # Write the output
    output_hex.finalize()
    flashed_regions = output_hex.validate()

    args.output.write_bytes(output_hex.to_bytes())

    # Print information about the flashed regions
    print("Flashed regions:")
    for start, end, data in flashed_regions:
        print(f"  0x{start:08X}-0x{end:08X}  ({end - start:>7,} bytes)")

        for line in hexdump(data, start=start, first=128, last=128):
            print(f"    {line}")

        print()
        print()


if __name__ == "__main__":
    main()
