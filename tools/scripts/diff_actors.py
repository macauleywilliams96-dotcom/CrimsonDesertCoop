#!/usr/bin/env python3
"""Compare two live actors component by component (read-only, via cdmp_probe).

Usage:
    python diff_actors.py <probe_exe> <key_a> <key_b> [--size N]

Finds both actors through the client's actor manager (same recipe as
src/mod/game/actor_lookup.cpp), then dumps the actor and each component in
its table and prints the qwords where one side is zero and the other is not,
or where the pointed-to RTTI classes differ. Values that are pointers on both
sides are ignored (heap addresses always differ).
"""

from __future__ import annotations

import argparse
import re
import subprocess

WORLD_SLOT = 0x1471B6C50  # exe + kWorldSystemSlot (build 3479)
COMPONENTS = 0x68
TABLE_SLOTS = 0x248 // 8


def probe(exe, *args):
    out = subprocess.run([exe, *args], capture_output=True, text=True, errors="replace").stdout
    return out


def read_u64(exe, addr):
    m = re.search(r"\((0x[0-9A-Fa-f]+)\)", probe(exe, "read", hex(addr), "--type", "u64").splitlines()[-1])
    return int(m.group(1), 16) if m else 0


def read_u32(exe, addr):
    m = re.search(r"\((0x[0-9A-Fa-f]+)\)", probe(exe, "read", hex(addr), "--type", "u32").splitlines()[-1])
    return int(m.group(1), 16) if m else 0


def find_actor(exe, key):
    """Linear walk of the manager's node array (slow but simple)."""
    world = read_u64(exe, WORLD_SLOT)
    mgr = read_u64(exe, world + 0x30)
    table = read_u64(exe, mgr + 0x08)
    nodes = read_u64(exe, table + 0xA0)
    for i in range(20000):
        node = nodes + i * 0x10
        k = read_u32(exe, node + 0x04)
        if k == key:
            return read_u64(exe, node + 0x08)
        if k == 0 and i > 4096:
            break
    return 0


def dump(exe, addr, size):
    rows = {}
    for line in probe(exe, "dump", hex(addr), "--size", hex(size)).splitlines():
        m = re.match(r"\+([0-9A-F]{4})\s+([0-9A-F]{16})(.*)", line)
        if m:
            cls = re.search(r"(\.\?A[VU]\S+)", m.group(3))
            rows[int(m.group(1), 16)] = (int(m.group(2), 16), cls.group(1) if cls else "")
    return rows


def name(exe, addr):
    return probe(exe, "rtti", hex(addr)).strip().splitlines()[-1] if addr else "(null)"


def diff(label, a, b):
    shown = 0
    for off in sorted(set(a) & set(b)):
        (va, ca), (vb, cb) = a[off], b[off]
        if (va == 0) != (vb == 0) or (ca and cb and ca != cb) or (bool(ca) != bool(cb)):
            if shown == 0:
                print(f"== {label}")
            print(f"   +{off:04X}  A {va:016X} {ca:45}  B {vb:016X} {cb}")
            shown += 1
    return shown


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("probe")
    ap.add_argument("key_a", type=lambda s: int(s, 0))
    ap.add_argument("key_b", type=lambda s: int(s, 0))
    ap.add_argument("--size", type=lambda s: int(s, 0), default=0x300)
    ap.add_argument("--actor-a", type=lambda s: int(s, 0))
    ap.add_argument("--actor-b", type=lambda s: int(s, 0))
    a = ap.parse_args()
    actor_a = a.actor_a or find_actor(a.probe, a.key_a)
    actor_b = a.actor_b or find_actor(a.probe, a.key_b)
    print(f"A 0x{a.key_a:08X} at {actor_a:#x}   B 0x{a.key_b:08X} at {actor_b:#x}")
    if not actor_a or not actor_b:
        return
    diff("actor", dump(a.probe, actor_a, a.size), dump(a.probe, actor_b, a.size))
    ta, tb = read_u64(a.probe, actor_a + COMPONENTS), read_u64(a.probe, actor_b + COMPONENTS)
    for slot in range(TABLE_SLOTS):
        ca, cb = read_u64(a.probe, ta + slot * 8), read_u64(a.probe, tb + slot * 8)
        if not ca and not cb:
            continue
        label = f"component +{slot * 8:03X} {name(a.probe, ca or cb)}"
        if not ca or not cb:
            print(f"== {label}: A {ca:#x} B {cb:#x}")
            continue
        diff(label, dump(a.probe, ca, a.size), dump(a.probe, cb, a.size))


if __name__ == "__main__":
    main()
