#!/usr/bin/env python3
"""Interop check: parse generate_corpus output with bthome-ble.

Reads JSON Lines from stdin (or a file given as argv[1]). Each case carries a
BTHome service-data payload built by bthome-cpp plus the semantic values that
went into it. The payload is handed to bthome-ble - the parser Home Assistant
uses - and the decoded result must contain those values.

Requires: pip install bthome-ble
"""

import datetime
import json
import sys

from bluetooth_sensor_state_data import BluetoothServiceInfoBleak
from bthome_ble import BTHomeBluetoothDeviceData

FCD2_UUID = "0000fcd2-0000-1000-8000-00805f9b34fb"

failures = 0
# Cases sharing a "session" reuse one parser instance, so the parser's
# replay protection sees the increasing encryption counter across packets.
sessions: dict[str, BTHomeBluetoothDeviceData] = {}


def service_info(mac: str, payload: bytes) -> BluetoothServiceInfoBleak:
    return BluetoothServiceInfoBleak(
        name="bthome-cpp interop",
        address=mac,
        rssi=-60,
        manufacturer_data={},
        service_data={FCD2_UUID: payload},
        service_uuids=[FCD2_UUID],
        source="local",
        device=None,
        advertisement=None,
        connectable=False,
        time=0,
        tx_power=None,
    )


def report(name: str, ok: bool, detail: str = "") -> None:
    global failures
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f"\n  {detail}" if detail and not ok else ""))
    if not ok:
        failures += 1


def as_number(value):
    """Sensor values comparable as numbers; timestamps count as epoch seconds."""
    if isinstance(value, datetime.datetime):
        return value.timestamp()
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return float(value)
    return None


def check_case(case: dict) -> None:
    name = case["name"]
    payload = bytes.fromhex(case["payload_hex"])
    bindkey = case.get("bindkey")

    session = case.get("session")
    device = sessions.get(session)
    if device is None:
        device = BTHomeBluetoothDeviceData(
            bindkey=bytes.fromhex(bindkey) if bindkey else None
        )
        if session:
            sessions[session] = device

    try:
        update = device.update(service_info(case["mac"], payload))
    except Exception as exc:  # noqa: BLE001 - a parser crash is a test failure
        report(name, False, f"parser raised {type(exc).__name__}: {exc}")
        return

    if bindkey and not device.bindkey_verified:
        report(name, False, "bindkey_verified is False (decryption failed)")
        return

    sensor_values = {
        key.key: value.native_value
        for key, value in update.entity_values.items()
        if key.key != "signal_strength"  # injected by us via rssi, not payload data
    }
    binary_values = {key.key: value.native_value for key, value in update.binary_entity_values.items()}
    events = {key.key: event for key, event in update.events.items()}
    sw_versions = [d.sw_version for d in update.devices.values()]

    for expected in case["expected"]:
        kind = expected["kind"]
        if kind == "no_crash":
            continue
        if kind == "sensor":
            want = float(expected["value"])
            tol = float(expected["tolerance"])
            # With a key, only that entity may satisfy the expectation - a wrong
            # object id then fails even if the value happens to match elsewhere.
            if "key" in expected:
                candidates = [sensor_values[expected["key"]]] if expected["key"] in sensor_values else []
            else:
                candidates = list(sensor_values.values())
            numbers = [n for n in map(as_number, candidates) if n is not None]
            if not any(abs(n - want) <= tol for n in numbers):
                report(name, False, f"expected {expected.get('key', 'any')}={want}±{tol}, "
                                    f"parsed sensors: {sensor_values}")
                return
        elif kind == "binary":
            if expected["value"] not in binary_values.values():
                report(name, False, f"expected binary {expected['value']}, parsed: {binary_values}")
                return
        elif kind == "event":
            event = events.get(expected["key"])
            if event is None or event.event_type != expected["value"]:
                report(name, False, f"expected {expected['key']}={expected['value']}, parsed events: "
                                    f"{ {k: e.event_type for k, e in events.items()} }")
                return
            props = event.event_properties or {}
            if "steps" in expected and props.get("steps") != expected["steps"]:
                report(name, False, f"expected steps={expected['steps']}, got {props}")
                return
            if "args" in expected and props.get("args") != expected["args"]:
                report(name, False, f"expected args={expected['args']}, got {props}")
                return
        elif kind in ("text", "raw"):
            if expected["value"] not in sensor_values.values():
                report(name, False, f"expected {expected['value']!r}, parsed: {sensor_values}")
                return
        elif kind == "sw_version":
            if expected["value"] not in sw_versions:
                report(name, False, f"expected sw_version {expected['value']!r}, got {sw_versions}")
                return
        else:
            report(name, False, f"unknown expected kind {kind!r}")
            return

    report(name, True)


def main() -> int:
    stream = open(sys.argv[1], encoding="utf-8") if len(sys.argv) > 1 else sys.stdin
    count = 0
    for line in stream:
        line = line.strip()
        if line:
            check_case(json.loads(line))
            count += 1
    print(f"\n{count} cases, {failures} failure{'s' if failures != 1 else ''} "
          f"({'ALL TESTS PASSED' if failures == 0 else 'TESTS FAILED'})")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
