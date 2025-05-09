import os
import json
import time
import pathlib
import requests

REPO = os.getenv("GITHUB_REPOSITORY", "NabuCasa/silabs-firmware-builder")
API_URL = f"https://api.github.com/repos/{REPO}/releases/latest"
ASSETS_ROOT = pathlib.Path(__file__).parent.parent / "static/assets"


def download_release():
    rsp = requests.get(API_URL)
    rsp.raise_for_status()

    for asset in rsp.json()["assets"]:
        url = asset["browser_download_url"]
        name = asset["name"]

        asset_rsp = requests.get(url)
        asset_rsp.raise_for_status()

        (ASSETS_ROOT / name).write_bytes(asset_rsp.content)


def main():
    ASSETS_ROOT.mkdir(parents=True, exist_ok=True)

    for attempt in range(5):
        download_release()

        if (ASSETS_ROOT / "manifest.json").exists():
            break

        time.sleep(30)
    else:
        raise RuntimeError("Manifest was never uploaded, cannot release")

    manifest = json.loads((ASSETS_ROOT / "manifest.json").read_text())["firmwares"]

    zbt1_firmwares = {
        fw["metadata"]["fw_type"]: fw
        for fw in manifest
        if fw["filename"].startswith(("skyconnect_", "zbt1_"))
    }

    (ASSETS_ROOT / "zbt1.json").write_text(
        json.dumps(
            {
                "product_name": "Home Assistant ZBT-1",
                "baudrates": {
                    "bootloader": [115200],
                    "cpc": [460800, 115200, 230400],
                    "ezsp": [115200],
                    "spinel": [460800],
                    "router": [115200],
                },
                "usb_filters": [{"pid": 60000, "vid": 4292}],
                "firmwares": [
                    {
                        "name": "Zigbee (EZSP)",
                        "url": "assets/" + zbt1_firmwares["zigbee_ncp"]["filename"],
                        "type": "ncp-uart-hw",
                        "version": zbt1_firmwares["zigbee_ncp"]["metadata"][
                            "ezsp_version"
                        ],
                    },
                    {
                        "name": "OpenThread (RCP)",
                        "url": "assets/" + zbt1_firmwares["openthread_rcp"]["filename"],
                        "type": "ot-rcp",
                        "version": zbt1_firmwares["openthread_rcp"]["metadata"][
                            "ot_rcp_version"
                        ]
                        .split("/")[-1]
                        .split("_")[0],
                    },
                    {
                        "name": "Multiprotocol (deprecated)",
                        "url": "https://raw.githubusercontent.com/NabuCasa/silabs-firmware/main/RCPMultiPAN/beta/NabuCasa_SkyConnect_RCP_v4.3.2_rcp-uart-hw-802154_460800.gbl",
                        "type": "rcp-uart-802154",
                        "version": "4.3.2",
                    },
                ],
                "allow_custom_firmware_upload": True,
            }
        )
    )


main()
