from __future__ import annotations

import argparse
import json
from pathlib import Path

from luna_pack import parse_bundle


def parse_audit_lines(path: Path) -> list[dict[str, str]]:
    events: list[dict[str, str]] = []
    if not path.exists():
        return events
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = raw.strip()
        if not line.startswith("audit "):
            continue
        parts = line.split()
        event: dict[str, str] = {"raw": line}
        if len(parts) >= 2:
            head = parts[1]
            if "." in head:
                event["flow"], event["step"] = head.split(".", 1)
            else:
                event["flow"] = head
        for token in parts[2:]:
            if "=" in token:
                key, value = token.split("=", 1)
                event[key] = value
        events.append(event)
    return events


def summarize(events: list[dict[str, str]]) -> dict[str, object]:
    flows: dict[str, dict[str, object]] = {}
    for event in events:
        flow = event.get("flow", "unknown")
        summary = flows.setdefault(
            flow,
            {"count": 0, "steps": [], "denied": [], "approved": [], "persisted": []},
        )
        summary["count"] = int(summary["count"]) + 1
        if "step" in event:
            summary["steps"].append(event["step"])
        if "reason" in event:
            summary["denied"].append(event["reason"])
        if "approved" in event:
            summary["approved"].append(event["approved"])
        if "persisted" in event:
            summary["persisted"].append(event["persisted"])
    return flows


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit LunaOS ecosystem orchestration logs and bundles")
    parser.add_argument("--bundle", type=Path, help="Inspect a .luna bundle")
    parser.add_argument("--log", type=Path, action="append", default=[], help="Parse serial or device log for audit lines")
    args = parser.parse_args()

    report: dict[str, object] = {}
    if args.bundle is not None:
        report["bundle"] = parse_bundle(args.bundle.read_bytes())
    if args.log:
        all_events: list[dict[str, str]] = []
        by_log: dict[str, object] = {}
        for path in args.log:
            events = parse_audit_lines(path)
            by_log[str(path)] = {
                "events": events,
                "summary": summarize(events),
            }
            all_events.extend(events)
        report["logs"] = by_log
        report["summary"] = summarize(all_events)

    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
