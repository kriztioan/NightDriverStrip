#!/usr/bin/env python3

"""
Smoke-test WS281x runtime topology against a live NightDriverStrip device.

This test is intentionally end-to-end:
- read compiled/runtime capability from /api/v1/settings/schema
- apply several valid live topology reshapes
- verify /api/v1/settings and /statistics reflect each change
- verify invalid requests are rejected with the expected error
- restore the original topology before exiting

Requires:
    pip install requests
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import dataclass
from typing import Iterable

try:
    import requests
except ModuleNotFoundError:  # pragma: no cover - dependency handling path
    requests = None


class TestFailure(RuntimeError):
    pass


@dataclass(frozen=True)
class Shape:
    width: int
    height: int

    @property
    def leds(self) -> int:
        return self.width * self.height

    def label(self) -> str:
        return f"{self.width}x{self.height} ({self.leds} leds)"


class Device:
    def __init__(self, host: str, port: int = 80, timeout: float = 5.0):
        if requests is None:
            fail("This script requires the 'requests' package. Install it with: pip install requests")
        self.host = host
        self.port = port
        self.base_url = f"http://{host}:{port}"
        self.timeout = timeout

    def get_json(self, path: str, timeout: float | None = None) -> dict:
        response = requests.get(f"{self.base_url}{path}", timeout=self.timeout if timeout is None else timeout)
        response.raise_for_status()
        return response.json()

    def post_json(self, path: str, payload: dict, timeout: float | None = None) -> requests.Response:
        response = requests.post(
            f"{self.base_url}{path}",
            json=payload,
            timeout=self.timeout if timeout is None else timeout,
        )
        return response

    def post_form(self, path: str, payload: dict, timeout: float | None = None) -> requests.Response:
        response = requests.post(
            f"{self.base_url}{path}",
            data=payload,
            timeout=self.timeout if timeout is None else timeout,
        )
        return response


def fail(message: str) -> None:
    raise TestFailure(message)


def expect(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def unique_shapes(shapes: Iterable[Shape]) -> list[Shape]:
    seen: set[tuple[int, int]] = set()
    result: list[Shape] = []
    for shape in shapes:
        key = (shape.width, shape.height)
        if key in seen:
            continue
        seen.add(key)
        result.append(shape)
    return result


def choose_valid_shapes(schema: dict, current: Shape) -> list[Shape]:
    topology = schema["topology"]
    max_leds = int(topology["compiledMaxLEDs"])
    nominal_width = int(topology["compiledNominalWidth"])
    nominal_height = int(topology["compiledNominalHeight"])
    nominal_area = nominal_width * nominal_height

    candidates: list[Shape] = []

    for width, height in ((32, 16), (32, 8), (16, 8), (24, 8), (16, 16), (8, 8)):
        if width > 0 and height > 0 and width * height <= max_leds:
            candidates.append(Shape(width, height))

    if current.leds > 1:
        reduced_height = max(1, current.height // 2)
        candidates.append(Shape(current.width, reduced_height))
        reduced_width = max(1, current.width // 2)
        candidates.append(Shape(reduced_width, current.height))

    # If compiled LED cap is larger than the nominal matrix area, deliberately prove that runtime
    # reshape is bounded by NUM_LEDS rather than the original compile-time width/height envelope.
    if max_leds > nominal_area:
        wider = nominal_width + 2
        if wider <= max_leds:
            stretched_height = max(1, max_leds // wider)
            if wider * stretched_height <= max_leds:
                candidates.append(Shape(wider, stretched_height))

        taller = nominal_height + 2
        stretched_width = max(1, max_leds // taller)
        if stretched_width * taller <= max_leds:
            candidates.append(Shape(stretched_width, taller))

    filtered = [
        shape
        for shape in unique_shapes(candidates)
        if shape != current and shape.width > 0 and shape.height > 0 and shape.leds <= max_leds
    ]

    if not filtered:
        fail("Could not derive any valid topology test shapes from the device schema")

    return filtered


def wait_for_live_shape(device: Device, shape: Shape, attempts: int = 20, delay_seconds: float = 0.25) -> dict:
    last_stats = {}
    for _ in range(attempts):
        stats = device.get_json("/statistics")
        last_stats = stats
        if (
            int(stats["CONFIGURED_MATRIX_WIDTH"]) == shape.width
            and int(stats["CONFIGURED_MATRIX_HEIGHT"]) == shape.height
            and int(stats["CONFIGURED_NUM_LEDS"]) == shape.leds
            and int(stats["ACTIVE_MATRIX_WIDTH"]) == shape.width
            and int(stats["ACTIVE_MATRIX_HEIGHT"]) == shape.height
            and int(stats["ACTIVE_NUM_LEDS"]) == shape.leds
        ):
            return stats
        time.sleep(delay_seconds)

    fail(
        "Device did not converge to the expected live topology. "
        f"expected={shape.label()} last_stats="
        + json.dumps(
            {
                "CONFIGURED_MATRIX_WIDTH": last_stats.get("CONFIGURED_MATRIX_WIDTH"),
                "CONFIGURED_MATRIX_HEIGHT": last_stats.get("CONFIGURED_MATRIX_HEIGHT"),
                "CONFIGURED_NUM_LEDS": last_stats.get("CONFIGURED_NUM_LEDS"),
                "ACTIVE_MATRIX_WIDTH": last_stats.get("ACTIVE_MATRIX_WIDTH"),
                "ACTIVE_MATRIX_HEIGHT": last_stats.get("ACTIVE_MATRIX_HEIGHT"),
                "ACTIVE_NUM_LEDS": last_stats.get("ACTIVE_NUM_LEDS"),
            }
        )
    )


def restore_shape(device: Device, shape: Shape) -> None:
    payload = {"topology": {"width": shape.width, "height": shape.height}}

    try:
        response = device.post_json("/api/v1/settings", payload)
        if response.status_code == 200:
            wait_for_live_shape(device, shape, attempts=40, delay_seconds=0.5)
            return

        print(
            f"Warning: restore request returned HTTP {response.status_code}: {response.text}",
            file=sys.stderr,
        )
    except requests.exceptions.ReadTimeout:
        print(
            "Restore request timed out; polling to see whether the device applied it anyway...",
            file=sys.stderr,
        )
        try:
            wait_for_live_shape(device, shape, attempts=40, delay_seconds=0.5)
            return
        except TestFailure:
            print("Restore was not visible after timeout; retrying once with a longer HTTP timeout...", file=sys.stderr)
    except requests.exceptions.RequestException as exc:
        print(f"Restore request hit a transport error: {exc}. Retrying once...", file=sys.stderr)

    try:
        response = device.post_json("/api/v1/settings", payload, timeout=15.0)
        if response.status_code == 200:
            wait_for_live_shape(device, shape, attempts=40, delay_seconds=0.5)
            return
        print(
            f"Warning: restore retry returned HTTP {response.status_code}: {response.text}",
            file=sys.stderr,
        )
    except requests.exceptions.RequestException as exc:
        print(f"Warning: restore retry failed: {exc}", file=sys.stderr)


def verify_settings(device: Device, shape: Shape) -> None:
    settings = device.get_json("/api/v1/settings")
    topology = settings["topology"]
    expect(int(topology["width"]) == shape.width, f"/api/v1/settings width mismatch for {shape.label()}")
    expect(int(topology["height"]) == shape.height, f"/api/v1/settings height mismatch for {shape.label()}")
    expect(int(topology["ledCount"]) == shape.leds, f"/api/v1/settings ledCount mismatch for {shape.label()}")


def apply_shape(device: Device, shape: Shape) -> None:
    response = device.post_json(
        "/api/v1/settings",
        {"topology": {"width": shape.width, "height": shape.height}},
    )
    if response.status_code != 200:
        fail(f"Expected topology {shape.label()} to succeed, got {response.status_code}: {response.text}")

    verify_settings(device, shape)
    wait_for_live_shape(device, shape)


def verify_invalid_area(device: Device, max_leds: int) -> None:
    invalid = Shape(max_leds + 1, 1)
    response = device.post_json(
        "/api/v1/settings",
        {"topology": {"width": invalid.width, "height": invalid.height}},
    )
    expect(response.status_code == 400, f"Expected invalid topology {invalid.label()} to be rejected with HTTP 400")
    expect("recompile needed" in response.text.lower(), "Expected invalid-area rejection to mention 'recompile needed'")


def verify_driver_mismatch(device: Device, compiled_driver: str) -> None:
    invalid_driver = "hub75" if compiled_driver == "ws281x" else "ws281x"
    response = device.post_json("/api/v1/settings", {"outputs": {"driver": invalid_driver}})
    expect(response.status_code == 400, "Expected output-driver mismatch to be rejected with HTTP 400")
    expect("recompile needed" in response.text.lower(), "Expected driver-mismatch rejection to mention 'recompile needed'")


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke-test NightDriverStrip runtime topology on a live WS281x device.")
    parser.add_argument("host", help="Device hostname or IP address")
    parser.add_argument("--port", type=int, default=80, help="HTTP port (default: 80)")
    parser.add_argument(
        "--keep",
        action="store_true",
        help="Leave the last successfully applied topology in place instead of restoring the original topology",
    )
    args = parser.parse_args()

    device = Device(args.host, args.port)

    schema = device.get_json("/api/v1/settings/schema")
    settings = device.get_json("/api/v1/settings")
    statistics = device.get_json("/statistics")

    compiled_driver = str(schema["outputs"]["compiledDriver"])
    expect(compiled_driver == "ws281x", f"This smoke test currently targets WS281x devices, got compiled driver '{compiled_driver}'")
    expect(bool(schema["topology"]["liveApply"]) is True, "Expected live topology apply to be enabled for this device")

    original = Shape(
        width=int(settings["topology"]["width"]),
        height=int(settings["topology"]["height"]),
    )

    compiled_max_leds = int(schema["topology"]["compiledMaxLEDs"])
    compiled_nominal_width = int(schema["topology"]["compiledNominalWidth"])
    compiled_nominal_height = int(schema["topology"]["compiledNominalHeight"])
    compiled_topology = Shape(compiled_nominal_width, compiled_nominal_height)

    print(
        "Compiled capacity:",
        f"nominal={compiled_nominal_width}x{compiled_nominal_height}",
        f"max_leds={compiled_max_leds}",
        f"current={original.label()}",
    )

    shapes = choose_valid_shapes(schema, original)

    # Prefer a few compact smoke-test cases rather than exhaustively walking every possible reshape.
    shapes = shapes[:4]
    print("Testing valid shapes:", ", ".join(shape.label() for shape in shapes))

    try:
        for shape in shapes:
            print(f"Applying valid topology: {shape.label()}")
            apply_shape(device, shape)

        print("Verifying invalid area rejection")
        verify_invalid_area(device, compiled_max_leds)

        print("Verifying driver mismatch rejection")
        verify_driver_mismatch(device, compiled_driver)

        # Final consistency check after invalid requests.
        last_applied = shapes[-1] if shapes else original
        verify_settings(device, last_applied)
        wait_for_live_shape(device, last_applied)

        final_stats = device.get_json("/statistics")
        print("Smoke test passed")
        print(
            "Final live stats:",
            json.dumps(
                {
                    "CONFIGURED_MATRIX_WIDTH": final_stats.get("CONFIGURED_MATRIX_WIDTH"),
                    "CONFIGURED_MATRIX_HEIGHT": final_stats.get("CONFIGURED_MATRIX_HEIGHT"),
                    "ACTIVE_MATRIX_WIDTH": final_stats.get("ACTIVE_MATRIX_WIDTH"),
                    "ACTIVE_MATRIX_HEIGHT": final_stats.get("ACTIVE_MATRIX_HEIGHT"),
                    "COMPILED_NUM_LEDS": final_stats.get("COMPILED_NUM_LEDS"),
                }
            ),
        )
        return 0
    finally:
        if not args.keep:
            print(f"Restoring compiled topology: {compiled_topology.label()}")
            restore_shape(device, compiled_topology)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except TestFailure as exc:
        print(f"Topology smoke test failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
