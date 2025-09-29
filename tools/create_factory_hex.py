import json
import argparse
import pathlib

import enum
import dataclasses


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


def make_padded_string(text):
    assert len(text.encode("utf-8")) <= 16

    result = (text.encode("utf-8") + b"\x00").ljust(16, b"\xff")[:16]
    assert len(result) == 16

    return result


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
    def __init__(self):
        self.records = []

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


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create an Intel HEX file combining a bootloader, application, and manufacturing tokens."
    )
    parser.add_argument(
        "--bootloader",
        type=pathlib.Path,
        required=True,
        help="Path to the bootloader HEX file.",
    )
    parser.add_argument(
        "--application",
        type=pathlib.Path,
        required=True,
        help="Path to the application HEX file.",
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

    bootloader_hex = IntelHex.from_bytes(args.bootloader.read_bytes())
    application_hex = IntelHex.from_bytes(args.application.read_bytes())
    tokens_json = json.loads(args.tokens.read_text())

    # Create a HEX file for the manufacturing tokens
    tokens_hex = IntelHex()
    tokens_hex.records.append(
        Record(
            address=0,
            type=RecordType.EXTENDED_LINEAR_ADDRESS,
            data=b"\x0f\xe0",  # User Data is mapped to 2kB at USERDATA_BASE (0x0FE00000-0x0FE007FF)
        )
    )

    for token, value in tokens_json["znet"].items():
        token_info = MANUFACTURING_TOKENS[token]

        if token_info.string:
            token_data = make_padded_string(value)
        else:
            token_data = bytes.fromhex(value)

        assert len(token_data) == token_info.size

        tokens_hex.records.append(
            Record(
                address=token_info.address,
                type=RecordType.DATA,
                data=token_data,
            )
        )

    tokens_hex.records.append(
        Record(
            address=0,
            type=RecordType.END_OF_FILE,
            data=b"",
        )
    )

    # Combine all three
    output_hex = IntelHex()
    output_hex.records = (
        bootloader_hex.records + application_hex.records + tokens_hex.records
    )

    # Strip out any existing end-of-file records and add a new one
    output_hex.records = [
        r for r in output_hex.records if r.type != RecordType.END_OF_FILE
    ]
    output_hex.records.append(
        Record(
            address=0,
            type=RecordType.END_OF_FILE,
            data=b"",
        )
    )

    # Write the output
    args.output.write_bytes(output_hex.to_bytes())


if __name__ == "__main__":
    main()
