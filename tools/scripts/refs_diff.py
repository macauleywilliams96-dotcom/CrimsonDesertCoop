#!/usr/bin/env python3
"""Which game objects hold a pointer to actor A but none to actor B? (read-only)

Usage:
    python refs_diff.py <probe_exe> <actor_a> <actor_b> [--max N] [--back N]

For each actor, `cdmp_probe findptr <addr> --heap` lists every heap qword equal
to the actor's address. Each hit is attributed to its owning object: the
nearest qword at or below it (up to --back bytes) that is a vtable with RTTI.
The output groups hits by class and field offset, and lists the classes that
reference A only, B only, or both. Used to find the world registration an
injected copy (B) is missing compared with the local player (A).
"""

from __future__ import annotations

import argparse
import collections
import re
import subprocess


def run(exe, *args):
    return subprocess.run([exe, *args], capture_output=True, text=True, errors="replace").stdout


def hits(exe, addr, limit):
    out = run(exe, "findptr", hex(addr), "--heap", "--max", str(limit))
    return [int(m, 16) for m in re.findall(r"^\s+(0x[0-9A-Fa-f]+)\s*$", out, re.M)]


def owner(exe, at, back):
    """(class, offset of the hit inside it) for the object that holds `at`."""
    base = at - back
    rows = run(exe, "dump", hex(base), "--size", hex(back + 8))
    best = None
    for line in rows.splitlines():
        m = re.match(r"\+([0-9A-F]{4})\s+[0-9A-F]{16}\s+.*exe\+0x[0-9A-F]+", line)
        if not m:
            continue
        off = int(m.group(1), 16)
        obj = base + off
        name = run(exe, "rtti", hex(obj)).strip().splitlines()[-1]
        if name.startswith(".?A"):
            best = (name, at - obj)
    return best or ("(no owner found)", 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("probe")
    ap.add_argument("actor_a", type=lambda s: int(s, 0))
    ap.add_argument("actor_b", type=lambda s: int(s, 0))
    ap.add_argument("--max", type=int, default=200)
    ap.add_argument("--back", type=lambda s: int(s, 0), default=0x200)
    a = ap.parse_args()
    groups = {}
    for label, addr in (("A", a.actor_a), ("B", a.actor_b)):
        found = hits(a.probe, addr, a.max)
        print(f"{label} {addr:#x}: {len(found)} references")
        g = collections.Counter()
        for h in found:
            g[owner(a.probe, h, a.back)] += 1
        groups[label] = g
    classes = {lab: {k[0] for k in g} for lab, g in groups.items()}
    for title, keys in (("A only", classes["A"] - classes["B"]), ("B only", classes["B"] - classes["A"]),
                        ("both", classes["A"] & classes["B"])):
        print(f"\n== {title}")
        for cls in sorted(keys):
            for lab in ("A", "B"):
                for (c, off), n in sorted(groups[lab].items()):
                    if c == cls:
                        print(f"   {lab} {cls} +{off:#x} x{n}")


if __name__ == "__main__":
    main()
