#!/usr/bin/env python3
"""List the frontier of a wayfinder map: open, unblocked, unclaimed children.

Usage: python3 issues/frontier.py <map-id> [--all]
  --all  also list blocked and claimed open children, with the reason.
"""
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).parent


def parse(path):
    text = path.read_text()
    m = re.match(r"---\n(.*?)\n---\n", text, re.S)
    meta = {}
    for line in (m.group(1) if m else "").splitlines():
        if ":" in line:
            key, _, val = line.partition(":")
            meta[key.strip()] = val.split("#")[0].strip()
    meta["blocked_by"] = [int(x) for x in re.findall(r"\d+", meta.get("blocked_by", ""))]
    meta["file"] = path.name
    return meta


def main():
    map_id = sys.argv[1]
    show_all = "--all" in sys.argv
    issues = {int(i["id"]): i for i in map(parse, HERE.glob("[0-9]*.md")) if i.get("id")}
    for iid, i in sorted(issues.items()):
        if i.get("parent") != map_id or i.get("status") != "open":
            continue
        blockers = [b for b in i["blocked_by"] if issues.get(b, {}).get("status") != "closed"]
        reason = ""
        if blockers:
            reason = "blocked by " + ", ".join(issues[b]["title"] for b in blockers if b in issues)
        elif i.get("assignee"):
            reason = "claimed by " + i["assignee"]
        if reason and not show_all:
            continue
        print(f"{iid:>4}  {i.get('labels', ''):<24} {i['title']}" + (f"  [{reason}]" if reason else ""))


if __name__ == "__main__":
    main()
