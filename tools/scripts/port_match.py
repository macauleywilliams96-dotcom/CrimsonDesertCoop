#!/usr/bin/env python3
"""Map the RVAs of one CrimsonDesert.exe build onto another (read-only, on disk).

Usage:
    python port_match.py <old_exe> <new_exe> <src_dir> [--only NAME] [-o OUT.json]

Every ``constexpr uintptr_t kName = 0x...;`` under <src_dir> is an old RVA.

* Code (RVA in an executable section): the function start is disassembled and
  turned into a pattern with every rel32 / rip-relative displacement masked;
  the pattern grows until it matches exactly once in the new exe.
* Data (globals, vtables): code in the old exe that references the RVA through
  a rip-relative operand is found, a masked pattern around each reference is
  searched in the new exe, and the new target is decoded from the matched
  instruction. The answer is the majority vote over the references.

Needs: pefile, capstone, numpy.
"""

from __future__ import annotations

import argparse
import collections
import json
import pathlib
import re
import sys

import capstone
import numpy as np
import pefile

SCN_EXEC = 0x20000000


class Image:
    def __init__(self, path: str):
        pe = pefile.PE(path, fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.mem = bytearray(pe.OPTIONAL_HEADER.SizeOfImage)
        raw = open(path, "rb").read()
        self.exec_ranges = []
        for s in pe.sections:
            data = raw[s.PointerToRawData:s.PointerToRawData + s.SizeOfRawData]
            n = min(len(data), s.Misc_VirtualSize or len(data))
            self.mem[s.VirtualAddress:s.VirtualAddress + n] = data[:n]
            if s.Characteristics & SCN_EXEC:
                self.exec_ranges.append((s.VirtualAddress, s.VirtualAddress + max(n, 0)))
        self.mem = bytes(self.mem)
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        self.md.detail = True

    def is_exec(self, rva: int) -> bool:
        return any(a <= rva < b for a, b in self.exec_ranges)

    def masked(self, rva: int, max_len: int):
        """Yield (insn, mask) with address-dependent bytes masked."""
        out = []
        for ins in self.md.disasm(self.mem[rva:rva + max_len + 16], rva):
            mask = [True] * ins.size
            disp_off = ins.disp_offset if ins.disp_size == 4 else 0
            rip_rel = any(op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP
                          for op in ins.operands)
            if rip_rel and disp_off:
                for i in range(disp_off, disp_off + 4):
                    mask[i] = False
            if ins.imm_size == 4 and ins.group(capstone.x86.X86_GRP_BRANCH_RELATIVE):
                for i in range(ins.imm_offset, ins.imm_offset + 4):
                    mask[i] = False
            out.append((ins, mask))
            if sum(i.size for i, _ in out) >= max_len:
                break
        return out


def to_regex(chunks) -> bytes:
    pat = bytearray()
    for ins, mask in chunks:
        for b, keep in zip(ins.bytes, mask):
            pat += re.escape(bytes([b])) if keep else b"."
    return bytes(pat)


def search(img: Image, rx: bytes, limit: int = 3):
    hits = []
    r = re.compile(rx, re.DOTALL)
    for a, b in img.exec_ranges:
        for m in r.finditer(img.mem, a, b):
            hits.append(m.start())
            if len(hits) >= limit:
                return hits
    return hits


def match_code(old: Image, new: Image, rva: int):
    for length in (16, 24, 32, 48, 64, 96, 128):
        chunks = old.masked(rva, length)
        if not chunks:
            return None, "undecodable"
        hits = search(new, to_regex(chunks))
        if len(hits) == 1:
            return hits[0], f"unique@{length}B"
        if not hits:
            return None, f"not found@{length}B"
    return None, f"ambiguous ({len(hits)} hits@128B)"


def find_refs(img: Image, target: int, cap: int = 12):
    """Code offsets i where int32 at i + (i+4) == target (disp ends the insn)."""
    refs = []
    for a, b in img.exec_ranges:
        step = 1 << 24
        for lo in range(a, b, step):
            hi = min(b, lo + step)
            for k in range(4):
                n = (hi - lo - k) // 4
                if n <= 0:
                    continue
                d = np.frombuffer(img.mem, dtype=np.int32, count=n, offset=lo + k)
                pos = lo + k + 4 * np.arange(len(d), dtype=np.int64)
                # rip-relative displacement followed by 0, 1 or 4 immediate bytes
                for tail in (4, 5, 8):
                    hit = np.nonzero(pos + tail + d.astype(np.int64) == target)[0]
                    refs.extend((int(pos[h]), tail) for h in hit)
                    if len(refs) >= cap * 4:
                        return refs
    return refs


def rel_field(ins):
    """Offset of the rip-relative disp32 or rel32 branch target, else 0."""
    if ins.disp_size == 4 and any(op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP
                                  for op in ins.operands):
        return ins.disp_offset
    if ins.imm_size == 4 and ins.group(capstone.x86.X86_GRP_BRANCH_RELATIVE):
        return ins.imm_offset
    return 0


def insn_at_disp(img: Image, disp_pos: int, tail: int):
    """Instruction whose rip-relative disp32 / rel32 sits at disp_pos."""
    for back in range(1, 12):
        start = disp_pos - back
        for ins in img.md.disasm(img.mem[start:start + 16], start):
            off = rel_field(ins)
            if off and start + off == disp_pos and ins.address + ins.size == disp_pos + tail:
                return ins
            break
    return None


def match_data(old: Image, new: Image, rva: int):
    votes = collections.Counter()
    tried = 0
    for disp_pos, tail in find_refs(old, rva):
        ins = insn_at_disp(old, disp_pos, tail)
        if not ins:
            continue
        tried += 1
        # Pattern: the reference instruction plus what follows it.
        for length in (24, 40, 64):
            chunks = old.masked(ins.address, length)
            hits = search(new, to_regex(chunks))
            if len(hits) == 1:
                n = hits[0]
                nins = next(new.md.disasm(new.mem[n:n + 16], n))
                off = rel_field(nins)
                if off:
                    disp = int.from_bytes(new.mem[n + off:n + off + 4], "little", signed=True)
                    votes[n + nins.size + disp] += 1
                break
            if not hits:
                break
        if tried >= 12 or (votes and votes.most_common(1)[0][1] >= 3):
            break
    if not votes:
        return None, f"no match ({tried} refs tried)"
    best, n = votes.most_common(1)[0]
    total = sum(votes.values())
    return best, f"{n}/{total} refs agree" + ("" if n == total else f" (others: {dict(votes)})")


def collect(src: pathlib.Path):
    rx = re.compile(r"constexpr\s+uintptr_t\s+(k\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;")
    out = {}
    for f in sorted(src.rglob("*")):
        if f.suffix not in (".h", ".hpp", ".cpp"):
            continue
        for name, val in rx.findall(f.read_text(encoding="utf-8", errors="replace")):
            if name != "kImageBase":
                out[name] = (int(val, 16), str(f.relative_to(src)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("src")
    ap.add_argument("--only")
    ap.add_argument("-o", "--out")
    a = ap.parse_args()

    targets = collect(pathlib.Path(a.src))
    if a.only:
        targets = {k: v for k, v in targets.items() if a.only in k}
    print("loading images...", file=sys.stderr)
    old, new = Image(a.old), Image(a.new)
    results = {}
    for name, (rva, file) in targets.items():
        kind = "code" if old.is_exec(rva) else "data"
        got, how = (match_code if kind == "code" else match_data)(old, new, rva)
        if got is None and kind == "code":
            # Body changed or is not unique: match the code that calls it.
            got, via = match_data(old, new, rva)
            how += f"; callers: {via}"
        results[name] = {"file": file, "kind": kind, "old": hex(rva),
                         "new": hex(got) if got is not None else None, "how": how}
        print(f"{name:32} {kind:4} {rva:#010x} -> {'-' if got is None else hex(got):>12}  {how}", flush=True)
    if a.out:
        pathlib.Path(a.out).write_text(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
