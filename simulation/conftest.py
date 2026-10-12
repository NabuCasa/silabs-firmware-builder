from pathlib import Path

import pytest


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption("--firmware", type=Path, required=True, help="ZBT-2 Zigbee NCP .out to simulate")


@pytest.fixture
def firmware(pytestconfig: pytest.Config) -> Path:
    return pytestconfig.getoption("--firmware")
