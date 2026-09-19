#!/usr/bin/env python3
"""Fail on any call into the Arduino SD library that is not made holding sdcard::Guard.

The card is one SPI device behind a FAT layer that is not safe to enter from two tasks at
once, and this firmware enters it from both cores. The rule "take the lock" used to live in a
comment beside the two callers that took it; six others did not. See sdcard.h.

    python3 tools/check_sd_guard.py          # exit code = number of findings

A call is accepted when an sdcard::Guard (or a manual sdcard::lock()) has been declared
earlier in a scope that is still open at the call. A deliberate exception is a line ending in
    // sd-guard-ok: <why>

What this cannot see: calls on an already-open File (f.read, f.write, f.close). Those are the
same hazard; the rule for them is in sdcard.h, and every one in the tree today sits under a
Guard, but nothing here proves the next one will.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SD_CALL = re.compile(r"\bSD(?:_MMC)?\s*\.\s*(open|exists|mkdir|rmdir|remove|rename|begin|end|"
                     r"cardSize|cardType|totalBytes|usedBytes)\s*\(")
GUARD = re.compile(r"\bsdcard\s*::\s*(Guard|lock)\b")
WAIVER = re.compile(r"//\s*sd-guard-ok\b")


def blank_out(src):
    """Replace comments and string/char literals with spaces, keeping every newline, so brace
    matching and line numbers survive. Raw strings (the web pages in main.cpp) are handled."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        two = src[i:i + 2]
        if two == "//":
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif two == "/*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j])); i = j
        elif c == "R" and src[i + 1:i + 2] == '"' and (i == 0 or not (src[i - 1].isalnum() or src[i - 1] == "_")):
            m = re.match(r'R"([^(\s]*)\(', src[i:])
            if not m:
                out.append(c); i += 1; continue
            end = ")" + m.group(1) + '"'
            j = src.find(end, i + m.end())
            j = n if j < 0 else j + len(end)
            out.append(re.sub(r"[^\n]", " ", src[i:j])); i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(re.sub(r"[^\n]", " ", src[i:j])); i = j
        else:
            out.append(c); i += 1
    return "".join(out)


def scan_source(src):
    """Return [(line_number, text)] for each unguarded SD call in `src`."""
    waived = {k + 1 for k, line in enumerate(src.split("\n")) if WAIVER.search(line)}
    clean = blank_out(src)
    findings = []
    stack = [False]                     # one flag per open scope: has a Guard been declared in it?
    for lineno, line in enumerate(clean.split("\n"), 1):
        # Walk the line so a Guard and a call on the same line are ordered correctly.
        events = []
        for m in GUARD.finditer(line):
            events.append((m.start(), "guard"))
        for m in SD_CALL.finditer(line):
            events.append((m.start(), "call"))
        for k, ch in enumerate(line):
            if ch == "{":
                events.append((k, "open"))
            elif ch == "}":
                events.append((k, "close"))
        for _, kind in sorted(events):
            if kind == "open":
                stack.append(False)
            elif kind == "close":
                if len(stack) > 1:
                    stack.pop()
            elif kind == "guard":
                stack[-1] = True
            elif kind == "call" and not any(stack) and lineno not in waived:
                findings.append((lineno, src.split("\n")[lineno - 1].strip()))
    return findings


def main():
    total = 0
    for base, _, files in os.walk(os.path.join(ROOT, "src")):
        for name in sorted(files):
            if not name.endswith((".cpp", ".h", ".c")):
                continue
            path = os.path.join(base, name)
            with open(path, encoding="utf-8", errors="ignore") as f:
                found = scan_source(f.read())
            for lineno, text in found:
                print(f"  {os.path.relpath(path, ROOT)}:{lineno}: SD call outside an sdcard::Guard: {text}")
            total += len(found)
    print(f"check_sd_guard: {total} finding(s)")
    return total


if __name__ == "__main__":
    sys.exit(main())
