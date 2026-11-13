from collections.abc import AsyncIterator
from pathlib import Path
from typing import Any

import pytest
from ruamel.yaml import YAML

from simulation.device import Device, simulate


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--manifest",
        type=Path,
        required=True,
        help="Manifest the firmware was built from",
    )
    parser.addoption(
        "--firmware", type=Path, required=True, help="Firmware .out to simulate"
    )


def load_manifest(config: pytest.Config) -> dict[str, Any]:
    return YAML(typ="safe").load(config.getoption("--manifest").read_text())


def pytest_collection_modifyitems(
    config: pytest.Config, items: list[pytest.Item]
) -> None:
    """Deselect tests marked for another firmware type or board than the manifest's."""
    manifest = load_manifest(config)
    actual = {
        "fw_type": manifest["gbl"]["fw_type"],
        "board": manifest["simulation"]["board"],
    }

    selected = []
    deselected = []

    for item in items:
        if all(
            actual[mark.name] in mark.args
            for mark in item.iter_markers()
            if mark.name in actual
        ):
            selected.append(item)
        else:
            deselected.append(item)

    config.hook.pytest_deselected(items=deselected)
    items[:] = selected


@pytest.fixture
def manifest(pytestconfig: pytest.Config) -> dict[str, Any]:
    return load_manifest(pytestconfig)


@pytest.fixture
async def device(
    pytestconfig: pytest.Config, manifest: dict[str, Any], tmp_path: Path
) -> AsyncIterator[Device]:
    async with simulate(
        pytestconfig.getoption("--firmware"), manifest["simulation"]["board"], tmp_path
    ) as device:
        yield device
