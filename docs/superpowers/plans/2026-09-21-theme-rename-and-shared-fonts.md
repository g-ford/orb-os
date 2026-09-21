# Theme rename and shared font faces: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename the current `default` theme to `elegant` (spec step 0), then let a theme define named font faces once and share them, migrating `elegant`, `fallout` and `portal` (Barlow) (spec step 1).

**Architecture:** `tools/build_theme.py` gains a `fonts:` block: named faces (a `.ttf` baked once by a shared `tools/font_bake.py`, or a pre-baked `.bin` copied verbatim) plus a slot-to-face map written into `theme.json`. On the device, a pure header (`theme_font_resolve.h`) resolves each slot to a file from that map (from `theme.json` when the card is present, from a `fonts.map` blob the bake stores in flash when it is not), and `theme_font` loads each distinct file once and shares it. The simulator learns to load theme fonts from the folder so a typeface can be judged in a `--themeshot`.

**Tech Stack:** Python 3 with PyYAML (builder and tests), `lv_font_conv@1.5.3` (via `npx`), C++17 firmware (LVGL 8.4, ArduinoJson 7), PlatformIO (`~/.platformio/penv/bin/pio`, not on PATH).

**Spec:** `docs/superpowers/specs/2026-09-20-theme-palette-fonts-builtin-default-design.md` (sections "Naming", "Design 1. Fonts", "Design 5. Simulator", "Migrating the shipped themes", rollout steps 0 and 1). Read it first. This plan does **not** cover the palette roles, the procedural clock, retiring Default/Office, or deleting the compiled art (spec steps 2 to 6); each gets its own plan when reached, because they depend on measurements and tuning done here.

## Global Constraints

- `THEME_CAPS` goes 51 -> 52 with a ledger entry in `theme_style.h`; "never renumber, never reuse".
- A face's file is `font_<face>.bin`; that name must be **at most 23 characters** (`theme_art`'s index stores 23 and silently never finds a longer name), so a face name is **at most 14 characters**, and it may **not equal a slot name**.
- Faces are baked with the pinned `lv_font_conv@1.5.3 --bpp 4 --no-compress --format bin`, default ranges `0x20-0x7E,0xB0,0xB1,0xB7,0x2013,0x2014,0x2018,0x2019,0x201C,0x201D,0x2022,0x2026`.
- Sizes are a budget: the builder prints the distinct-font count and total bytes and **warns above 10**.
- A theme with no `fonts:` block builds exactly as it does today; themes already on a card keep working.
- Slot names are `theme_font.cpp`'s `SLOT_FILE` entries without `font_` and `.bin` (27 slots today); the builder reads them from that file, never from a copy.
- No `git add -A`; stage explicit paths. Put a test before the fix in commit order so no commit fails its own test. One branch per step, merged `--no-ff`: **Task 1 on `chore/rename-default-to-elegant`; Tasks 2 to 11 on `feat/theme-font-faces`**, each in its own git worktree (the main checkout carries the owner's uncommitted work). Base both branches on `worktree-docs-theme-palette-fonts-flash-default` so the spec and this plan travel with them.
- Firmware is **not done until it has booted on a real Orb** (CLAUDE.md rule 1). Task 11 ends with a hardware gate that an agent cannot pass alone; hand it to the owner and say so.
- Every commit message ends with the trailer `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` (pass it as a second `-m`).
- `pio` is `~/.platformio/penv/bin/pio`. A fresh worktree has no `.pio/`: copy it from the main checkout (Task 1 step 1).

## Review Focus

Failure modes the spec implies that no single task's headline tests would catch. Each has a test in the task named.

1. **An Orb with no card** cannot read `theme.json`, so a face-only theme would lose its typeface. The map must round-trip through the `fonts.map` flash blob and resolve identically (Task 4 host test; Task 11 hardware gate).
2. **A face `src` that escapes the theme folder** (`../../something.ttf`) must be refused, not read (Task 3).
3. **`lv_font_conv` missing or failing** must give a one-line `error:` and exit 1, never a Python traceback (Task 2, Task 3).
4. **A face or slot name that silently misbehaves**: a face named like a slot (an unmapped slot would pick it up by accident), a face name whose file exceeds 23 characters (stored truncated, never found), a misspelt slot (Task 3).
5. **A stale per-slot `font_<slot>.bin` left beside a `fonts:` block** must not ship or shadow the mapped face (Task 3).

## File Structure

| File | Responsibility |
|---|---|
| `tools/font_bake.py` (new) | Bake one face at one size; find `lv_font_conv`. Used by the builder. |
| `tools/build_theme.py` | Parse and validate `fonts:`, bake or copy faces, write the map into `theme.json`, count faces. |
| `tools/font_golden.py` (new) | For a built theme: `slot sha256` per slot, resolved as the firmware does. |
| `tools/gen_elegant_theme.py` (renamed) | Regenerate Elegant's `theme.yaml`; now preserves its hand-written `fonts:` block. |
| `src/theme/core/theme_font_resolve.h` (new) | Pure logic: slot name, font map, slot-to-file resolution with de-duplication. |
| `src/theme/core/theme_style.{h,cpp}` | Parse `fonts` from `theme.json` into a `FontMap`; `THEME_CAPS` 52. |
| `src/theme/core/theme_font.{h,cpp}` | Resolve, load each distinct face once, share it; `distinct_files()`, `map_text()`. |
| `src/theme/core/theme_art_bake.cpp` | Bake the distinct faces and store `fonts.map`. |
| `src/theme/core/theme_art.cpp` | Native `find_blob` reads theme files from `sim/sdcard`. |
| `src/platform/sim/sim_main.cpp` | Call `theme_font::begin()` after `lv_init()`. |
| `tests/stub_lv_font_conv.py`, `tests/test_font_bake.py`, `tests/font_facts.py`, `tests/test_theme_fonts_golden.py`, `tests/test_theme_font_coverage.py`, `tests/theme_font_resolve_test.cpp`, `tests/run_theme_font_resolve_test.sh`, `tests/golden/fonts_*.txt` (new) | Tests and goldens. |

---

### Task 1: Rename the `default` theme to `elegant` (spec step 0)

**Files:**
- Rename: `src/theme_assets/default/` -> `src/theme_assets/elegant/`; `tools/gen_default_theme.py` -> `tools/gen_elegant_theme.py`; `tests/test_default_theme.py` -> `tests/test_elegant_theme.py`
- Modify: `tools/gen_elegant_theme.py`, `tests/test_elegant_theme.py`, `tests/test_portal_theme.py:10`, `tests/test_fallout_theme.py:12`, `tools/build_theme.py:4`, `tools/dump_theme_defaults.cpp`, `docs/theme-yaml.md`, `docs/adding-a-screen.md`, `src/theme_assets/portal/theme.yaml:3`, `src/theme_assets/fallout/theme.yaml:4`, regenerated `src/theme_assets/elegant/theme.yaml`
- Leave alone: `tools/render_theme_bitmaps.py` (its `"default"`/`"office"` are the compiled Default/Office skins, retired in a later plan) and `docs/superpowers/*` (history).

**Interfaces:**
- Produces: theme slug `elegant`; module `gen_elegant_theme` (same functions as `gen_default_theme`: `render_yaml`, `generate`, `run_dumper`, `build_dumper`, `build_folder`, `unpack_orb`, `GenError`, `SLUG`, `THEME_DIR`).

- [ ] **Step 1: Worktree, libdeps, baseline**

```bash
cd /Users/geoffford/code/orb-os
git worktree add .claude/worktrees/chore-rename-elegant -b chore/rename-default-to-elegant worktree-docs-theme-palette-fonts-flash-default
cd .claude/worktrees/chore-rename-elegant
mkdir -p .pio && cp -R /Users/geoffford/code/orb-os/.pio/libdeps .pio/libdeps
python3 -m unittest tests/test_default_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py tests/test_build_theme.py tests/test_build_all_themes.py 2>&1 | tail -4
```
Expected: `OK` (some skips are fine, e.g. a font-loader test needing `liblvgl.a`; note the skip count for step 6).

- [ ] **Step 2: Rename with git**

```bash
git mv src/theme_assets/default src/theme_assets/elegant
git mv tools/gen_default_theme.py tools/gen_elegant_theme.py
git mv tests/test_default_theme.py tests/test_elegant_theme.py
```

- [ ] **Step 3: Rewrite every reference (each replacement asserts it matched)**

```bash
python3 - <<'EOF'
import pathlib

def edit(path, pairs):
    # A pair with a third element is optional (a file may hold only one of the two old module names);
    # every other pair must match, so a phrase that has moved fails loudly instead of being skipped.
    p = pathlib.Path(path)
    s = p.read_text(encoding='utf-8')
    for pair in pairs:
        old, new = pair[0], pair[1]
        if old not in s:
            assert len(pair) == 3, f'{path}: {old!r} not found'
            continue
        s = s.replace(old, new)
    p.write_text(s, encoding='utf-8')

TOKENS = [('gen_default_theme', 'gen_elegant_theme', 'optional'), ('test_default_theme', 'test_elegant_theme', 'optional')]

edit('tools/gen_elegant_theme.py', TOKENS + [
    ('"""Write src/theme_assets/default: the theme every Orb starts from, and the template for new ones.',
     '"""Write src/theme_assets/elegant: the Elegant theme, and the reference listing every option a theme can set.'),
    ('# make the default this theme', '# make Elegant this theme'),
    ('The default theme is a design like any other, and the folder is where it',
     'Elegant is a design like any other, and the folder is where it'),
    ('So theme.yaml is both the default look and a template', 'So theme.yaml is both the Elegant look and a reference'),
    ('or an option the firmware added that the default theme does not mention.',
     'or an option the firmware added that Elegant does not mention.'),
    ("THEME_DIR = REPO / 'src' / 'theme_assets' / 'default'", "THEME_DIR = REPO / 'src' / 'theme_assets' / 'elegant'"),
    ("SLUG, NAME, AUTHOR = 'default', 'Default', 'Orb OS'", "SLUG, NAME, AUTHOR = 'elegant', 'Elegant', 'Orb OS'"),
    ("'# The default theme, and a template listing every option a theme can set.',",
     "'# Elegant, and the reference listing every option a theme can set.',"),
    ("'default: true',", "'default: false',"),
    ('"""Replace the default theme\'s folder with the theme in a .orb."""',
     '"""Replace Elegant\'s folder with the theme in a .orb."""'),
    ("help='make the default theme the theme in this .orb'", "help='make Elegant the theme in this .orb'"),
    ("print('default theme is up to date')", "print('elegant theme is up to date')"),
])

edit('tests/test_elegant_theme.py', TOKENS + [
    ('"""The default theme is an imported design;', '"""Elegant is an imported design;'),
    ("THEME = ROOT / 'src' / 'theme_assets' / 'default'", "THEME = ROOT / 'src' / 'theme_assets' / 'elegant'"),
    ("'the default theme has no fonts'", "'elegant has no fonts'"),
    ('def test_it_is_the_default_theme(self):', 'def test_it_is_the_elegant_theme(self):'),
    ("['slug'], 'default')", "['slug'], 'elegant')"),
    ("r'(?m)^default: true$'", "r'(?m)^default: false$'"),
    ("built = Path(tmp) / 'default'", "built = Path(tmp) / 'elegant'"),
    ("options theme_style.cpp reads that the default theme does not mention",
     "options theme_style.cpp reads that Elegant does not mention"),
])

edit('tests/test_portal_theme.py', TOKENS)
edit('tests/test_fallout_theme.py', TOKENS)
edit('tools/build_theme.py', [('src/theme_assets/default [--out build/themes]', 'src/theme_assets/elegant [--out build/themes]')])
edit('tools/dump_theme_defaults.cpp', TOKENS + [
    ("default theme's theme.yaml, and tests/test_elegant_theme.py runs it on the built default to prove",
     "Elegant's theme.yaml, and tests/test_elegant_theme.py runs it on the built Elegant to prove"),
])

edit('docs/theme-yaml.md', TOKENS + [
    ('src/theme_assets/default # -> build/themes/default/', 'src/theme_assets/elegant # -> build/themes/elegant/'),
    ('python3 tools/build_theme.py src/theme_assets/default --out', 'python3 tools/build_theme.py src/theme_assets/elegant --out'),
    ('Copy `src/theme_assets/default/` and edit it.', 'Copy `src/theme_assets/elegant/` and edit it.'),
    ('with the value the default theme gives it', 'with the value Elegant gives it'),
    ("The default theme's folder is where it lives.", "Elegant's folder is where it lives."),
    ('an option the default theme does not yet list', 'an option Elegant does not yet list'),
    ("The default theme is not the firmware's compiled fallback.", "Elegant is not the firmware's compiled fallback."),
    ('not the one the default theme states.', 'not the one Elegant states.'),
])

edit('docs/adding-a-screen.md', TOKENS + [
    ('- [ ] The default theme is regenerated', '- [ ] Elegant is regenerated'),
    ('| Default theme | `src/theme_assets/default/theme.yaml`, generated by',
     '| Elegant theme (the reference) | `src/theme_assets/elegant/theme.yaml`, generated by'),
])

for slug in ('portal', 'fallout'):
    edit(f'src/theme_assets/{slug}/theme.yaml', [
        ("(That is not the default theme's folder: see docs/theme-yaml.md.)", "(That is not Elegant's folder: see docs/theme-yaml.md.)")])
print('ok')
EOF
```
Expected: `ok`. An `AssertionError` names the file and the string that moved; fix that line, do not skip it.

- [ ] **Step 4: Regenerate Elegant's `theme.yaml` and check the diff is only the identity lines**

```bash
python3 tools/gen_elegant_theme.py
git diff -U0 src/theme_assets/elegant/theme.yaml
```
Expected: only these changes (the header comment is 5 lines; the slug, name and default lines):
```
-# The default theme, and a template listing every option a theme can set.
-# tools/gen_default_theme.py writes this file: from a .orb, or, after
+# Elegant, and the reference listing every option a theme can set.
+# tools/gen_elegant_theme.py writes this file: from a .orb, or, after
-slug: default
-name: Default
+slug: elegant
+name: Elegant
-default: true
+default: false
```
If anything else changed, stop: the dumper is reading a different theme than before.

- [ ] **Step 5: Run the tests**

```bash
python3 -m unittest tests/test_elegant_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py tests/test_build_theme.py tests/test_build_all_themes.py 2>&1 | tail -4
python3 tools/gen_elegant_theme.py --check
```
Expected: `OK` with the same skip count as step 1, and `elegant theme is up to date`.

- [ ] **Step 6: Confirm nothing still names the old theme**

```bash
grep -rnE "gen_default_theme|test_default_theme|theme_assets/default|themes/default" . --include='*.py' --include='*.sh' --include='*.md' --include='*.cpp' --include='*.yaml' | grep -v "^./docs/superpowers/" | grep -v "^./.pio"
```
Expected: no output.

- [ ] **Step 7: Commit**

```bash
git add src/theme_assets/elegant tools/gen_elegant_theme.py tests/test_elegant_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py tools/build_theme.py tools/dump_theme_defaults.cpp docs/theme-yaml.md docs/adding-a-screen.md src/theme_assets/portal/theme.yaml src/theme_assets/fallout/theme.yaml
git add -u src/theme_assets/default tools/gen_default_theme.py tests/test_default_theme.py
git commit -m "Rename the default theme to elegant" -m "The slug 'default' is reserved for the built-in look coming in a later step. No firmware change. An Orb whose saved slug is 'default' now finds no folder: copy elegant to /themes/elegant on the card and select it once." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 2: A shared font baker, `tools/font_bake.py`

**Files:**
- Create: `tools/font_bake.py`, `tests/stub_lv_font_conv.py`, `tests/test_font_bake.py`
- Modify: `tools/fallout_fonts.py` (use the shared baker; the file is deleted in Task 9)

**Interfaces:**
- Produces:
  - `font_bake.DEFAULT_RANGES: str`
  - `font_bake.FontBakeError(Exception)`
  - `font_bake.converter() -> list[str]` (env `LV_FONT_CONV`, then `lv_font_conv` on PATH, then `npx --yes lv_font_conv@1.5.3`; raises `FontBakeError` if none)
  - `font_bake.bake_face(src: Path, size: int, out: Path, ranges: str = DEFAULT_RANGES) -> None` (raises `FontBakeError` on a missing binary, a non-zero exit, or an empty output)
- Test helper: `tests/stub_lv_font_conv.py`, an executable stand-in for `lv_font_conv` that writes `STUB <font file name> <size>\n` to the `-o` path, appends its arguments to `$STUB_LOG`, and exits 3 with a message when `$STUB_FAIL` is set.

- [ ] **Step 1: Write the stub converter and the failing tests**

`tests/stub_lv_font_conv.py`:
```python
#!/usr/bin/env python3
"""A stand-in for lv_font_conv, for tests. It writes a tiny deterministic file instead of a font, appends
its arguments to $STUB_LOG (one line per call) and fails on request ($STUB_FAIL)."""
import os
import sys

a = sys.argv[1:]


def arg(flag):
    return a[a.index(flag) + 1]


if os.environ.get('STUB_FAIL'):
    sys.stderr.write('stub: asked to fail\n')
    sys.exit(3)
font, size, out = arg('--font'), arg('--size'), arg('-o')
with open(out, 'wb') as f:
    f.write(('STUB %s %s\n' % (os.path.basename(font), size)).encode())
log = os.environ.get('STUB_LOG')
if log:
    with open(log, 'a') as f:
        f.write(' '.join(a) + '\n')
```

`tests/test_font_bake.py`:
```python
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import font_bake  # noqa: E402

STUB = ROOT / 'tests' / 'stub_lv_font_conv.py'


class BakeFaceTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.log = self.tmp / 'calls.log'
        env = mock.patch.dict(os.environ, {'LV_FONT_CONV': str(STUB), 'STUB_LOG': str(self.log)})
        env.start()
        self.addCleanup(env.stop)
        self.face = self.tmp / 'Face.ttf'
        self.face.write_bytes(b'TTF')

    def test_the_environment_override_wins(self):
        self.assertEqual(font_bake.converter(), [str(STUB)])

    def test_a_face_is_baked_with_the_arguments_the_firmware_needs(self):
        out = self.tmp / 'out.bin'
        font_bake.bake_face(self.face, 22, out)
        self.assertEqual(out.read_bytes(), b'STUB Face.ttf 22\n')
        args = self.log.read_text().split()
        for want in ('--bpp', '4', '--no-compress', '--format', 'bin', '--size', '22'):
            self.assertIn(want, args)
        self.assertEqual(args[args.index('--range') + 1], font_bake.DEFAULT_RANGES)

    def test_ranges_can_be_replaced(self):
        font_bake.bake_face(self.face, 16, self.tmp / 'o.bin', ranges='0x20-0x7E')
        args = self.log.read_text().split()
        self.assertEqual(args[args.index('--range') + 1], '0x20-0x7E')

    def test_a_failing_converter_raises_with_its_message(self):
        with mock.patch.dict(os.environ, {'STUB_FAIL': '1'}):
            with self.assertRaises(font_bake.FontBakeError) as cm:
                font_bake.bake_face(self.face, 16, self.tmp / 'o.bin')
        self.assertIn('asked to fail', str(cm.exception))

    def test_a_missing_converter_raises_instead_of_a_traceback(self):
        with mock.patch.dict(os.environ, {'LV_FONT_CONV': '/nonexistent/lv_font_conv'}):
            with self.assertRaises(font_bake.FontBakeError) as cm:
                font_bake.bake_face(self.face, 16, self.tmp / 'o.bin')
        self.assertIn('lv_font_conv', str(cm.exception))


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

```bash
chmod +x tests/stub_lv_font_conv.py
python3 -m unittest tests/test_font_bake.py 2>&1 | tail -4
```
Expected: ERROR with `ModuleNotFoundError: No module named 'font_bake'`.

- [ ] **Step 3: Implement `tools/font_bake.py`**

```python
#!/usr/bin/env python3
"""Bake one typeface at one size into the lv_font_conv binary the firmware loads.

The format is fixed by the firmware, not chosen: `--bpp 4 --no-compress`, because the curved-text and
menu renderers read the 4-bit glyph bitmaps straight out of the file (see curved_text.cpp).

Needs lv_font_conv (npm). It is found, in order, from $LV_FONT_CONV, from PATH, or fetched by
`npx --yes lv_font_conv@1.5.3`. Pinned: a different version could lay glyphs out differently.
"""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

CONVERTER_VERSION = '1.5.3'

# ASCII, then what real text on these screens carries beyond it: degrees and plus-minus for weather,
# a middle dot the theme's own weather line uses, and the curly quotes, dashes, ellipsis and bullet
# a headline is full of (they are not sanitised on the way in, and a glyph the face lacks draws blank).
DEFAULT_RANGES = '0x20-0x7E,0xB0,0xB1,0xB7,0x2013,0x2014,0x2018,0x2019,0x201C,0x201D,0x2022,0x2026'


class FontBakeError(Exception):
    pass


def converter() -> list[str]:
    env = os.environ.get('LV_FONT_CONV')
    if env:
        return [env]
    if shutil.which('lv_font_conv'):
        return ['lv_font_conv']
    if shutil.which('npx'):
        return ['npx', '--yes', f'lv_font_conv@{CONVERTER_VERSION}']
    raise FontBakeError('lv_font_conv is needed to bake a .ttf face: npm install -g lv_font_conv, '
                        'or set $LV_FONT_CONV to its path')


def bake_face(src: Path, size: int, out: Path, ranges: str = DEFAULT_RANGES) -> None:
    cmd = converter() + ['--font', str(src), '--size', str(size), '--bpp', '4', '--no-compress',
                         '--format', 'bin', '--range', ranges, '-o', str(out)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
    except OSError as e:
        raise FontBakeError(f'cannot run lv_font_conv ({cmd[0]}): {e}')
    if result.returncode != 0 or not out.exists() or out.stat().st_size == 0:
        detail = (result.stderr or result.stdout).strip() or 'no output'
        raise FontBakeError(f'lv_font_conv failed for {src.name} at {size}px: {detail}')
```

- [ ] **Step 4: Point `tools/fallout_fonts.py` at the shared baker**

```bash
python3 - <<'EOF'
import pathlib
p = pathlib.Path('tools/fallout_fonts.py')
s = p.read_text(encoding='utf-8')
old_const = "CONVERTER_VERSION = '1.5.3'\n"
assert old_const in s
s = s.replace(old_const, "sys.path.insert(0, str(Path(__file__).resolve().parent))\nimport font_bake  # noqa: E402\n")
start = s.index("RANGES = '0x20-0x7E")
end = s.index('\n', start)
s = s[:start] + "RANGES = font_bake.DEFAULT_RANGES" + s[end:]
old_fn = s[s.index('def converter() -> list[str]:'):s.index('def bake(out_dir: Path) -> None:')]
s = s.replace(old_fn, '''def converter() -> list[str]:
    try:
        return font_bake.converter()
    except font_bake.FontBakeError as e:
        sys.exit(f'fallout_fonts.py: {e}')


''')
p.write_text(s, encoding='utf-8')
EOF
python3 -c "import sys; sys.path.insert(0,'tools'); import fallout_fonts, font_bake; assert fallout_fonts.RANGES == font_bake.DEFAULT_RANGES; print('same ranges')"
```
Expected: `same ranges`.

- [ ] **Step 5: Run the tests**

```bash
python3 -m unittest tests/test_font_bake.py tests/test_fallout_theme.py 2>&1 | tail -4
```
Expected: `OK`.

- [ ] **Step 6: Commit**

```bash
git add tools/font_bake.py tests/stub_lv_font_conv.py tests/test_font_bake.py tools/fallout_fonts.py
git commit -m "Add tools/font_bake.py, the shared face baker" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 3: The builder understands `fonts:`

**Files:**
- Modify: `tools/build_theme.py`
- Test: `tests/test_build_theme.py` (append a new class)

**Interfaces:**
- Consumes: `font_bake.bake_face`, `font_bake.DEFAULT_RANGES`, `font_bake.FontBakeError` (Task 2).
- Produces: `theme.json["fonts"]`: `{slot: "font_<face>.bin"}`; face files `font_<face>.bin` in the built folder and in `theme.json["assets"]`; `firmware_facts()["slots"]` (a set of slot names). A face `spec` has `src` (required), and `size` / `ranges` (`.ttf`/`.otf` only).

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_build_theme.py`, above `if __name__ == '__main__':`. Add `import os` and `from unittest import mock` to its imports if they are absent.

```python
STUB_CONVERTER = ROOT / 'tests' / 'stub_lv_font_conv.py'

FONTS_YAML = """slug: sample
fonts:
  faces:
    label: {src: t.ttf, size: 16}
  slots:
    radar2: label
    radar3: label
"""


class FontFacesTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.src = self.tmp / 'sample'
        self.src.mkdir()
        self.out = self.tmp / 'out'
        self.log = self.tmp / 'calls.log'
        env = mock.patch.dict(os.environ, {'LV_FONT_CONV': str(STUB_CONVERTER), 'STUB_LOG': str(self.log)})
        env.start()
        self.addCleanup(env.stop)

    def write(self, yaml_text, **files):
        (self.src / 'theme.yaml').write_text(yaml_text, encoding='utf-8')
        for name, data in files.items():
            (self.src / name.replace('__', '.')).write_bytes(data)

    def built(self, name):
        return json.loads((self.out / 'sample' / name).read_text(encoding='utf-8'))

    def fonts_built(self):
        return sorted(p.name for p in (self.out / 'sample').glob('font_*.bin'))

    def calls(self):
        return len(self.log.read_text().splitlines()) if self.log.exists() else 0

    def test_a_face_is_baked_once_however_many_slots_use_it(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.built('theme.json')['fonts'],
                         {'radar2': 'font_label.bin', 'radar3': 'font_label.bin'})
        self.assertEqual(self.fonts_built(), ['font_label.bin'])
        self.assertEqual((self.out / 'sample' / 'font_label.bin').read_bytes(), b'STUB t.ttf 16\n')
        self.assertEqual(self.calls(), 1)
        self.assertIn('font_label.bin', self.built('theme.json')['assets'])

    def test_changing_a_face_changes_the_assets_hash(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        run(self.src, self.out)
        first = self.built('theme.json')['assetsHash']
        self.write(FONTS_YAML.replace('size: 16', 'size: 18'), t__ttf=b'TTF')
        run(self.src, self.out)
        self.assertNotEqual(self.built('theme.json')['assetsHash'], first)

    def test_a_bin_face_is_copied_verbatim_and_needs_no_converter(self):
        (self.src / 'fonts').mkdir()
        (self.src / 'fonts' / 'raw.bin').write_bytes(b'\x01\x02RAW')
        self.write('slug: sample\nfonts:\n  faces:\n    raw: {src: fonts/raw.bin}\n  slots:\n    radar1: raw\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual((self.out / 'sample' / 'font_raw.bin').read_bytes(), b'\x01\x02RAW')
        self.assertEqual(self.calls(), 0)

    def test_no_fonts_block_builds_as_before(self):
        self.write('slug: sample\nradar:\n  rangeKm: 30\n')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertNotIn('fonts', self.built('theme.json'))

    def assert_refused(self, yaml_text, needle, **files):
        self.write(yaml_text, **files)
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1, msg=result.stdout)
        self.assertIn(needle, result.stderr)
        self.assertNotIn('Traceback', result.stderr)
        return result

    def test_a_misspelt_slot_is_an_error_that_lists_the_real_ones(self):
        result = self.assert_refused(FONTS_YAML.replace('radar2:', 'radr2:'), 'fonts.slots.radr2', t__ttf=b'TTF')
        self.assertIn('known:', result.stderr)
        self.assertIn('radar2', result.stderr)

    def test_a_slot_naming_an_undefined_face_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace('radar2: label', 'radar2: nope'), "'nope'", t__ttf=b'TTF')

    def test_a_face_named_like_a_slot_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace('label', 'radar2'), 'also a slot name', t__ttf=b'TTF')

    def test_a_face_name_too_long_for_the_flash_index_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace('label', 'a' * 15), '14 characters', t__ttf=b'TTF')

    def test_a_src_outside_the_theme_folder_is_an_error(self):
        (self.tmp / 'evil.ttf').write_bytes(b'TTF')
        self.assert_refused(FONTS_YAML.replace('src: t.ttf', 'src: ../evil.ttf'), 'outside the theme folder')
        self.assertEqual(self.calls(), 0)

    def test_a_missing_src_is_an_error(self):
        self.assert_refused(FONTS_YAML, 'does not exist')

    def test_a_bin_face_with_a_size_is_an_error(self):
        (self.src / 'raw.bin').write_bytes(b'x')
        self.assert_refused('slug: sample\nfonts:\n  faces:\n    raw: {src: raw.bin, size: 16}\n  slots:\n    radar1: raw\n',
                            'already baked')

    def test_a_ttf_face_without_a_size_is_an_error(self):
        self.assert_refused(FONTS_YAML.replace(', size: 16', ''), 'pixel size', t__ttf=b'TTF')

    def test_a_missing_converter_is_one_clean_error(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        with mock.patch.dict(os.environ, {'LV_FONT_CONV': '/nonexistent/lv_font_conv'}):
            result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn('error:', result.stderr)
        self.assertIn('lv_font_conv', result.stderr)
        self.assertNotIn('Traceback', result.stderr)

    def test_a_converter_failure_is_reported_with_its_message(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        with mock.patch.dict(os.environ, {'STUB_FAIL': '1'}):
            result = run(self.src, self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn('asked to fail', result.stderr)

    def test_a_stale_slot_file_beside_a_mapping_is_not_shipped(self):
        self.write(FONTS_YAML, t__ttf=b'TTF', font_radar2__bin=b'STALE')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.fonts_built(), ['font_label.bin'])
        self.assertIn('shadowed', result.stderr)
        self.assertNotIn('font_radar2.bin', self.built('theme.json')['assets'])

    def test_an_unmapped_slot_keeps_its_own_file(self):
        self.write(FONTS_YAML, t__ttf=b'TTF', font_radar4__bin=b'OWN')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertEqual(self.fonts_built(), ['font_label.bin', 'font_radar4.bin'])
        self.assertIn('font_radar4.bin', self.built('theme.json')['assets'])

    def test_an_unused_face_warns_and_is_not_baked(self):
        self.write(FONTS_YAML.replace('  slots:', '    spare: {src: t.ttf, size: 20}\n  slots:'), t__ttf=b'TTF')
        result = run(self.src, self.out)
        self.assertEqual(result.returncode, 0, msg=result.stderr)
        self.assertIn('fonts.faces.spare', result.stderr)
        self.assertEqual(self.calls(), 1)

    def faces_and_slots(self, count):
        slots = ['radar1', 'radar2', 'radar3', 'radar4', 'weather1', 'weather2', 'weather3', 'weather4',
                 'intel_title', 'intel_text', 'intel_source', 'intel_age']
        faces = ''.join(f'    f{i}: {{src: t.ttf, size: {10 + i}}}\n' for i in range(count))
        maps = ''.join(f'    {slots[i]}: f{i}\n' for i in range(count))
        return f'slug: sample\nfonts:\n  faces:\n{faces}  slots:\n{maps}'

    def test_more_than_ten_distinct_fonts_warn_and_ten_do_not(self):
        self.write(self.faces_and_slots(11), t__ttf=b'TTF')
        eleven = run(self.src, self.out)
        self.assertEqual(eleven.returncode, 0, msg=eleven.stderr)
        self.assertIn('distinct fonts', eleven.stderr)
        self.write(self.faces_and_slots(10), t__ttf=b'TTF')
        ten = run(self.src, self.out)
        self.assertEqual(ten.returncode, 0, msg=ten.stderr)
        self.assertNotIn('distinct fonts', ten.stderr)

    def test_the_font_count_and_size_are_printed(self):
        self.write(FONTS_YAML, t__ttf=b'TTF')
        result = run(self.src, self.out)
        self.assertRegex(result.stdout, r'fonts: 1 file\(s\), \d+ KB')
        self.assertRegex(result.stdout.strip().splitlines()[-1], r'image\(s\), 1 font\(s\)')
```

- [ ] **Step 2: Run to verify they fail**

```bash
python3 -m unittest tests/test_build_theme.py 2>&1 | tail -6
```
Expected: the new `FontFacesTest` tests FAIL (the top-level key `fonts` is refused today); the 19 existing tests still pass.

- [ ] **Step 3: Implement in `tools/build_theme.py`**

3a. Imports and constants. Add `import tempfile` with the other imports. After the `REPO = ...` and `CORE = ...` lines add:
```python
sys.path.insert(0, str(Path(__file__).resolve().parent))
import font_bake  # noqa: E402  (tools/font_bake.py)
```
After the `MAX_SLUG_LEN = 31 ...` line add:
```python
FONTS_KEY = 'fonts'
FACE_NAME = re.compile(r'[a-z][a-z0-9_]*')
MAX_FACE_NAME = 14                # 'font_' + name + '.bin' must fit theme_art's 23-character asset name
MAX_FONTS_BEFORE_WARNING = 10     # each distinct font is a face in flash and a copy in PSRAM
MIN_FONT_PX, MAX_FONT_PX = 6, 128
FACE_KEYS = {'src', 'size', 'ranges'}
```

3b. In `firmware_facts()` replace the `return {...}` with:
```python
    slots = {n[len('font_'):-len('.bin')] for n in fonts}
    return {'images': images, 'fonts': fonts, 'slots': slots, 'aliases': aliases,
            'max_json': int(limit.group(1)), 'keys': keys}
```

3c. Add `build_fonts` above the `# ---- build ----` banner:
```python
def build_fonts(theme_dir: Path, block, facts: dict, bake_dir: Path, warnings: list):
    """Turn the `fonts:` block into face files in bake_dir and a {slot: file} map.

    Returns ({slot: 'font_<face>.bin'}, {'font_<face>.bin': Path}). A face is baked once however
    many slots use it, and a face no slot uses is not baked at all."""
    if not isinstance(block, dict) or set(block) - {'faces', 'slots'}:
        raise BuildError('fonts: holds `faces` and `slots` and nothing else')
    faces, slots = block.get('faces') or {}, block.get('slots') or {}
    if not isinstance(faces, dict) or not isinstance(slots, dict):
        raise BuildError('fonts.faces and fonts.slots must each be a mapping')

    root = theme_dir.resolve()
    for name, spec in faces.items():
        where = f'fonts.faces.{name}'
        if not FACE_NAME.fullmatch(name) or len(name) > MAX_FACE_NAME:
            raise BuildError(f'{where}: a face name is lowercase letters, digits and _, starting with a letter, at most '
                             f'{MAX_FACE_NAME} characters (its file, font_{name}.bin, must fit the flash '
                             f"index's 23-character asset names)")
        if name in facts['slots']:
            raise BuildError(f'{where}: {name!r} is also a slot name, so an unmapped {name} slot would load this '
                             f'face by accident. Pick another name')
        if not isinstance(spec, dict) or 'src' not in spec or set(spec) - FACE_KEYS:
            raise BuildError(f'{where}: needs `src`, and may also have `size` and `ranges` (got {spec!r})')
        src = (root / str(spec['src'])).resolve()
        if root not in src.parents:
            raise BuildError(f'{where}.src: {spec["src"]!r} is outside the theme folder')
        if not src.is_file():
            raise BuildError(f'{where}.src: {spec["src"]!r} does not exist')
        suffix = src.suffix.lower()
        if suffix == '.bin':
            if 'size' in spec or 'ranges' in spec:
                raise BuildError(f'{where}: a .bin is already baked at one size, so it takes no size or ranges')
        elif suffix in ('.ttf', '.otf'):
            size = spec.get('size')
            if not isinstance(size, int) or isinstance(size, bool) or not MIN_FONT_PX <= size <= MAX_FONT_PX:
                raise BuildError(f'{where}.size: a pixel size from {MIN_FONT_PX} to {MAX_FONT_PX} is needed')
            if 'ranges' in spec and not isinstance(spec['ranges'], str):
                raise BuildError(f'{where}.ranges: must be text, in lv_font_conv --range form')
        else:
            raise BuildError(f'{where}.src: must be a .ttf, .otf or an already-baked .bin, '
                             f'not {suffix or "a file with no extension"}')

    used = []
    for slot, face in slots.items():
        if slot not in facts['slots']:
            raise BuildError(f'fonts.slots.{slot}: not a slot theme_font.cpp loads '
                             f'(known: {", ".join(sorted(facts["slots"]))})')
        if not isinstance(face, str) or face not in faces:
            raise BuildError(f'fonts.slots.{slot}: {face!r} is not a face defined in fonts.faces')
        if face not in used:
            used.append(face)
    for name in faces:
        if name not in used:
            warnings.append(f'fonts.faces.{name}: no slot uses it, so it is not baked')

    files = {}
    for name in sorted(used):
        spec = faces[name]
        src = (root / str(spec['src'])).resolve()
        out = bake_dir / f'font_{name}.bin'
        if src.suffix.lower() == '.bin':
            shutil.copyfile(src, out)
        else:
            try:
                font_bake.bake_face(src, spec['size'], out, spec.get('ranges', font_bake.DEFAULT_RANGES))
            except font_bake.FontBakeError as e:
                raise BuildError(f'fonts.faces.{name}: {e}')
        files[out.name] = out
    return {slot: f'font_{face}.bin' for slot, face in sorted(slots.items())}, files
```

3d. Split `build` so faces are baked into a temporary folder that lives for the whole build. Rename the existing `def build(theme_dir: Path, out_root: Path, warnings: list) -> Path:` to `def _build(theme_dir: Path, out_root: Path, warnings: list, bake_dir: Path) -> Path:` and add above it:
```python
def build(theme_dir: Path, out_root: Path, warnings: list) -> Path:
    with tempfile.TemporaryDirectory(prefix='orb-theme-faces-') as bake_dir:
        return _build(theme_dir, out_root, warnings, Path(bake_dir))


```

3e. Inside `_build`:
- Directly after `data = clean(load_yaml(yaml_path), 'theme.yaml', warnings)` add `font_block = data.pop(FONTS_KEY, None)`.
- After the loop `for asset_name, source in assets.items(): if asset_name.endswith('.png'): check_png(source, asset_name)` add:
```python
    font_map = {}
    if font_block is not None:
        font_map, face_files = build_fonts(theme_dir, font_block, facts, bake_dir, warnings)
        for slot in font_map:                     # a mapped slot no longer loads its own file
            legacy = f'font_{slot}.bin'
            if legacy in assets:
                warnings.append(f'{legacy}: shadowed by fonts.slots.{slot}, so it is not copied')
                del assets[legacy]
        assets.update(face_files)
    fonts_shipped = sorted(a for a in assets if a.endswith('.bin'))
    if len(fonts_shipped) > MAX_FONTS_BEFORE_WARNING:
        warnings.append(f'{len(fonts_shipped)} distinct fonts; each is a separate face in flash and in PSRAM. '
                        f'Reuse a size where the layout allows (the budget is {MAX_FONTS_BEFORE_WARNING}).')
```
- After `theme['assetsHash'] = assets_hash(assets)` add:
```python
    if font_map:
        theme['fonts'] = font_map
```
- Replace the two closing `print` calls, keeping the LAST line's shape (`tools/build_all_themes.py` parses it):
```python
    print(f'built {target}')
    if fonts_shipped:
        total_kb = sum(assets[a].stat().st_size for a in fonts_shipped) / 1024
        print(f'  fonts: {len(fonts_shipped)} file(s), {total_kb:.0f} KB')
    print(f'  {len([a for a in assets if a.endswith(".png")])} image(s), '
          f'{len(fonts_shipped)} font(s), '
          f'{len(payloads)} JSON file(s), assetsHash 0x{theme["assetsHash"]:08x}')
```

- [ ] **Step 4: Run the tests**

```bash
python3 -m unittest tests/test_build_theme.py tests/test_build_all_themes.py tests/test_font_bake.py 2>&1 | tail -4
```
Expected: `OK`.

- [ ] **Step 5: Commit**

```bash
git add tools/build_theme.py tests/test_build_theme.py
git commit -m "Builder: a fonts block of named faces and a slot-to-face map" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 4: The pure resolver, `theme_font_resolve.h`, with a host test

**Files:**
- Create: `src/theme/core/theme_font_resolve.h`, `tests/theme_font_resolve_test.cpp`, `tests/run_theme_font_resolve_test.sh`
- Modify: `tests/run_host_tests.sh`

**Interfaces:**
- Produces (namespace `theme_font`):
  - `constexpr size_t MAX_SLOTS = 32, MAP_MAX = 32, MAP_NAME_BYTES = 24;`
  - `struct FontMap { char slot[MAP_MAX][MAP_NAME_BYTES]; char file[MAP_MAX][MAP_NAME_BYTES]; size_t n = 0; };`
  - `bool map_add(FontMap &m, const char *slot, const char *file)` (false when a name is null, empty, 24 characters or more, or the map is full)
  - `const char *map_lookup(const char *slotName, void *ctx)` (`ctx` is a `FontMap *`; nullptr when unmapped)
  - `size_t map_serialize(const FontMap &m, char *buf, size_t cap)` (`slot file\n` lines; returns 0 rather than write half a map)
  - `void map_parse(FontMap &m, const char *text, size_t len)` (ignores malformed lines)
  - `bool slot_name(const char *legacyFile, char *out, size_t cap)` (`font_radar2.bin` -> `radar2`)
  - `struct Resolved { const char *file[MAX_SLOTS]; int group[MAX_SLOTS]; const char *distinct[MAX_SLOTS]; size_t slots, distinctCount; };`
  - `void resolve(const char *const *legacy, size_t n, MapLookup mapped, void *ctx, Resolved &out)` where `using MapLookup = const char *(*)(const char *slotName, void *ctx);`

- [ ] **Step 1: Write the failing test**

`tests/theme_font_resolve_test.cpp`:
```cpp
// Host test for src/theme/core/theme_font_resolve.h.   tests/run_theme_font_resolve_test.sh
#include "theme_font_resolve.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

using namespace theme_font;

static void must_add(FontMap &m, const char *slot, const char *file) {
    const bool ok = map_add(m, slot, file);
    assert(ok);
    (void)ok;
}

static const char *SLOTS[] = { "font_menu_current.bin", "font_radar1.bin", "font_radar2.bin",
                               "font_radar3.bin", "font_settings.bin" };
constexpr size_t N = sizeof(SLOTS) / sizeof(SLOTS[0]);

static void with_no_map_every_slot_loads_its_own_file() {
    Resolved r;
    resolve(SLOTS, N, nullptr, nullptr, r);
    assert(r.slots == N && r.distinctCount == N);
    for (size_t i = 0; i < N; ++i) {
        assert(strcmp(r.file[i], SLOTS[i]) == 0);
        assert(r.group[i] == (int)i);
    }
}

static void slots_mapped_to_one_face_share_one_group() {
    FontMap m;
    must_add(m, "radar1", "font_body.bin");
    must_add(m, "radar2", "font_body.bin");
    must_add(m, "radar3", "font_body.bin");
    Resolved r;
    resolve(SLOTS, N, map_lookup, &m, r);
    assert(r.distinctCount == 3);                                 // menu_current, body, settings
    assert(r.group[1] == r.group[2] && r.group[2] == r.group[3]);
    assert(strcmp(r.file[1], "font_body.bin") == 0);
    assert(strcmp(r.file[0], "font_menu_current.bin") == 0);      // an unmapped slot keeps its own file
    assert(strcmp(r.file[4], "font_settings.bin") == 0);
}

static void fallout_shaped_22_slots_become_10_faces() {
    static const struct { const char *slot; int px; } FALLOUT[] = {
        {"menu_current", 46}, {"settings", 27}, {"radar1", 22}, {"radar2", 16}, {"radar3", 16}, {"radar4", 16},
        {"weather1", 22}, {"weather2", 22}, {"weather3", 16}, {"weather4", 16}, {"intel_title", 32},
        {"intel_text", 22}, {"intel_source", 12}, {"intel_age", 12}, {"intel_brief", 18}, {"ticker_name", 16},
        {"ticker_price", 40}, {"ticker_change", 22}, {"ticker_strip", 16}, {"wind_title", 28},
        {"wind_ask", 20}, {"wind_turns", 16} };
    constexpr size_t COUNT = sizeof(FALLOUT) / sizeof(FALLOUT[0]);
    static char legacy[COUNT][40];
    const char *legacyPtr[COUNT];
    FontMap m;
    for (size_t i = 0; i < COUNT; ++i) {
        snprintf(legacy[i], sizeof(legacy[i]), "font_%s.bin", FALLOUT[i].slot);
        legacyPtr[i] = legacy[i];
        char face[MAP_NAME_BYTES];
        snprintf(face, sizeof(face), "font_st%d.bin", FALLOUT[i].px);
        must_add(m, FALLOUT[i].slot, face);
    }
    Resolved r;
    resolve(legacyPtr, COUNT, map_lookup, &m, r);
    assert(r.slots == COUNT);
    assert(r.distinctCount == 10);
}

static void an_empty_mapping_value_is_ignored() {
    FontMap m;
    assert(!map_add(m, "radar1", ""));
    assert(!map_add(m, "", "font_a.bin"));
    assert(!map_add(m, nullptr, "font_a.bin"));
    assert(!map_add(m, "radar1", nullptr));
    assert(m.n == 0);
    Resolved r;
    resolve(SLOTS, N, map_lookup, &m, r);
    assert(strcmp(r.file[1], "font_radar1.bin") == 0);
}

static void a_name_too_long_for_the_flash_index_is_rejected() {
    FontMap m;
    char longName[MAP_NAME_BYTES + 1];
    memset(longName, 'a', sizeof(longName));
    longName[MAP_NAME_BYTES] = 0;                                  // 24 characters: one too many
    assert(!map_add(m, "radar1", longName));
    assert(!map_add(m, longName, "font_a.bin"));
    longName[MAP_NAME_BYTES - 1] = 0;                              // 23 characters: the most that fits
    assert(map_add(m, "radar1", longName));
}

static void a_full_map_refuses_more() {
    FontMap m;
    for (size_t i = 0; i < MAP_MAX; ++i) {
        char slot[16];
        snprintf(slot, sizeof(slot), "s%zu", i);
        must_add(m, slot, "font_a.bin");
    }
    assert(!map_add(m, "extra", "font_a.bin"));
    assert(m.n == MAP_MAX);
}

static void slot_names_come_from_legacy_file_names() {
    char out[40];
    assert(slot_name("font_radar2.bin", out, sizeof(out)) && strcmp(out, "radar2") == 0);
    assert(slot_name("font_intel_title.bin", out, sizeof(out)) && strcmp(out, "intel_title") == 0);
    assert(!slot_name("radar2.bin", out, sizeof(out)));
    assert(!slot_name("font_.bin", out, sizeof(out)));
    assert(!slot_name("font_radar2.png", out, sizeof(out)));
    char tiny[4];
    assert(!slot_name("font_radar2.bin", tiny, sizeof(tiny)));
}

// Review focus 1: an Orb with no card resolves from the flash blob, and must get the same answer.
static void the_blob_round_trips_and_resolves_identically() {
    FontMap fromJson;
    must_add(fromJson, "radar1", "font_body.bin");
    must_add(fromJson, "radar3", "font_body.bin");
    must_add(fromJson, "menu_current", "font_big.bin");
    char blob[512];
    const size_t len = map_serialize(fromJson, blob, sizeof(blob));
    assert(len > 0);
    FontMap fromBlob;
    map_parse(fromBlob, blob, len);
    assert(fromBlob.n == fromJson.n);
    Resolved a, b;
    resolve(SLOTS, N, map_lookup, &fromJson, a);
    resolve(SLOTS, N, map_lookup, &fromBlob, b);
    assert(a.distinctCount == b.distinctCount);
    for (size_t i = 0; i < N; ++i) assert(strcmp(a.file[i], b.file[i]) == 0 && a.group[i] == b.group[i]);
}

static void a_map_that_does_not_fit_the_buffer_writes_nothing() {
    FontMap m;
    must_add(m, "radar1", "font_body.bin");
    char tiny[8];
    assert(map_serialize(m, tiny, sizeof(tiny)) == 0);
}

static void malformed_lines_in_the_blob_are_ignored() {
    const char text[] = "radar1 font_a.bin\nbadline\n  \nradar2\nradar3 font_b.bin";   // no trailing newline
    FontMap m;
    map_parse(m, text, sizeof(text) - 1);
    assert(m.n == 2);
    assert(strcmp(map_lookup("radar1", &m), "font_a.bin") == 0);
    assert(strcmp(map_lookup("radar3", &m), "font_b.bin") == 0);
    assert(map_lookup("radar2", &m) == nullptr);
}

int main() {
    with_no_map_every_slot_loads_its_own_file();
    slots_mapped_to_one_face_share_one_group();
    fallout_shaped_22_slots_become_10_faces();
    an_empty_mapping_value_is_ignored();
    a_name_too_long_for_the_flash_index_is_rejected();
    a_full_map_refuses_more();
    slot_names_come_from_legacy_file_names();
    the_blob_round_trips_and_resolves_identically();
    a_map_that_does_not_fit_the_buffer_writes_nothing();
    malformed_lines_in_the_blob_are_ignored();
    printf("theme_font_resolve: all tests passed\n");
    return 0;
}
```

`tests/run_theme_font_resolve_test.sh`:
```bash
#!/bin/bash
# Builds and runs tests/theme_font_resolve_test.cpp on the host. Needs no libraries.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
c++ -std=c++17 -O1 -g -Wall -Wextra -Isrc/theme/core tests/theme_font_resolve_test.cpp -o "$OUT/theme_font_resolve_test"
"$OUT/theme_font_resolve_test"
```

Add to `tests/run_host_tests.sh`, after the `weather` line:
```bash
run "theme font resolution"              bash tests/run_theme_font_resolve_test.sh
```

- [ ] **Step 2: Run to verify it fails**

```bash
chmod +x tests/run_theme_font_resolve_test.sh
bash tests/run_theme_font_resolve_test.sh 2>&1 | tail -3
```
Expected: FAIL to compile, `theme_font_resolve.h: No such file or directory`.

- [ ] **Step 3: Implement `src/theme/core/theme_font_resolve.h`**

```cpp
#pragma once
// Which font file each theme text slot loads, and which of those files are distinct.
//
// Pure logic on purpose: no LVGL and no theme_style. theme_font.cpp hands it the slot list and a
// slot-to-file map, and tests/theme_font_resolve_test.cpp runs it on the desktop. A theme maps its
// slots to named faces (theme.json "fonts": {"radar2": "font_body16.bin"}); slots that name the same
// file are one face, loaded once. A slot the map does not mention keeps its own font_<slot>.bin, which
// is how themes built before the map existed keep working.
//
// The same map is stored in flash as a `fonts.map` blob ("slot file" lines) by the bake, because
// theme.json is read from the SD card only and an Orb with no card must still know which face each
// slot loads.
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace theme_font {

constexpr size_t MAX_SLOTS = 32;         // theme_font.cpp static_asserts that its slot count fits
constexpr size_t MAP_MAX = 32;
constexpr size_t MAP_NAME_BYTES = 24;    // theme_art stores an asset name in 24 bytes, NUL included

struct FontMap {
    char   slot[MAP_MAX][MAP_NAME_BYTES];
    char   file[MAP_MAX][MAP_NAME_BYTES];
    size_t n = 0;
};

// A name that does not fit is refused, never truncated: a truncated asset name is one that is
// stored fine and then never found.
inline bool map_add(FontMap &m, const char *slot, const char *file) {
    if (!slot || !file || !*slot || !*file) return false;
    if (m.n >= MAP_MAX) return false;
    if (strlen(slot) >= MAP_NAME_BYTES || strlen(file) >= MAP_NAME_BYTES) return false;
    strcpy(m.slot[m.n], slot);
    strcpy(m.file[m.n], file);
    ++m.n;
    return true;
}

inline const char *map_lookup(const char *slotName, void *ctx) {
    const FontMap *m = static_cast<const FontMap *>(ctx);
    for (size_t i = 0; i < m->n; ++i)
        if (strcmp(m->slot[i], slotName) == 0) return m->file[i];
    return nullptr;
}

// "slot file\n" per line: the form of the flash blob. Returns the length, or 0 when the map does not
// fit `cap` (it never writes half a map).
inline size_t map_serialize(const FontMap &m, char *buf, size_t cap) {
    size_t at = 0;
    for (size_t i = 0; i < m.n; ++i) {
        const size_t need = strlen(m.slot[i]) + 1 + strlen(m.file[i]) + 1;
        if (at + need + 1 > cap) return 0;
        at += (size_t)snprintf(buf + at, cap - at, "%s %s\n", m.slot[i], m.file[i]);
    }
    return at;
}

inline void map_parse(FontMap &m, const char *text, size_t len) {
    size_t i = 0;
    while (i < len) {
        size_t e = i;
        while (e < len && text[e] != '\n') ++e;
        char line[2 * MAP_NAME_BYTES + 2];
        const size_t n = e - i;
        if (n > 0 && n < sizeof(line)) {
            memcpy(line, text + i, n);
            line[n] = 0;
            char *sp = strchr(line, ' ');
            if (sp) {
                *sp = 0;
                map_add(m, line, sp + 1);
            }
        }
        i = e + 1;
    }
}

// "font_radar2.bin" -> "radar2". False when the name is not shaped like a slot file.
inline bool slot_name(const char *legacyFile, char *out, size_t cap) {
    const char *prefix = "font_";
    const char *suffix = ".bin";
    const size_t n = strlen(legacyFile), p = strlen(prefix), s = strlen(suffix);
    if (n <= p + s || strncmp(legacyFile, prefix, p) != 0 || strcmp(legacyFile + n - s, suffix) != 0) return false;
    const size_t len = n - p - s;
    if (len + 1 > cap) return false;
    memcpy(out, legacyFile + p, len);
    out[len] = 0;
    return true;
}

using MapLookup = const char *(*)(const char *slotName, void *ctx);

struct Resolved {
    const char *file[MAX_SLOTS];       // per slot: the file it loads
    int         group[MAX_SLOTS];      // per slot: index of that file in distinct[]
    const char *distinct[MAX_SLOTS];   // each distinct file, in first-seen order
    size_t      slots = 0;
    size_t      distinctCount = 0;
};

// legacy[i] is slot i's own file name ("font_radar2.bin"); `mapped` answers "which file does the
// theme's map give this slot name", or nullptr. The pointers in `out` point into `legacy` and into
// whatever `mapped` returned, so both must outlive it.
inline void resolve(const char *const *legacy, size_t n, MapLookup mapped, void *ctx, Resolved &out) {
    out.slots = n < MAX_SLOTS ? n : MAX_SLOTS;
    out.distinctCount = 0;
    for (size_t i = 0; i < out.slots; ++i) {
        const char *file = legacy[i];
        char name[MAP_NAME_BYTES + 8];
        if (mapped && slot_name(legacy[i], name, sizeof(name))) {
            const char *m = mapped(name, ctx);
            if (m && *m) file = m;
        }
        out.file[i] = file;
        int g = -1;
        for (size_t d = 0; d < out.distinctCount; ++d)
            if (strcmp(out.distinct[d], file) == 0) { g = (int)d; break; }
        if (g < 0) {
            g = (int)out.distinctCount;
            out.distinct[out.distinctCount++] = file;
        }
        out.group[i] = g;
    }
}

} // namespace theme_font
```

- [ ] **Step 4: Run to verify it passes**

```bash
bash tests/run_theme_font_resolve_test.sh 2>&1 | tail -3
```
Expected: `theme_font_resolve: all tests passed`.

- [ ] **Step 5: Commit**

```bash
git add src/theme/core/theme_font_resolve.h tests/theme_font_resolve_test.cpp tests/run_theme_font_resolve_test.sh tests/run_host_tests.sh
git commit -m "Add theme_font_resolve.h: slot-to-face resolution with a host test" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 5: Firmware wiring: read the map, share the faces, store the blob, `THEME_CAPS` 52

**Files:**
- Modify: `src/theme/core/theme_style.h`, `src/theme/core/theme_style.cpp`, `src/theme/core/theme_font.h`, `src/theme/core/theme_font.cpp`, `src/theme/core/theme_art_bake.cpp`

**Interfaces:**
- Consumes: everything from Task 4.
- Produces:
  - `theme_font::FontMap &theme_style::fontMap()` (filled by `load()` from `theme.json`; `theme_font` fills it from the flash blob when it is empty; `load()` empties it)
  - `const char *const *theme_font::distinct_files(size_t &count)`
  - `size_t theme_font::map_text(char *buf, size_t cap)`
  - `theme_font::slot_files()` is removed (its one caller, the bake, moves to `distinct_files`).

There is no new unit test here beyond Task 4's: this task is wiring, verified by both firmware builds compiling and by Task 6 (simulator) and Task 11 (hardware). Do not skip those.

- [ ] **Step 1: `theme_style.h`**

Use these exact edits.

(a) Include the resolver. Replace
```
#include <lvgl.h>

namespace theme_style {
```
with
```
#include <lvgl.h>
#include "theme_font_resolve.h"

namespace theme_style {
```

(b) Add the ledger entry and bump the constant. Replace
```
//      An Orb below this level keeps the title up over a story and still moves the band.
constexpr int THEME_CAPS = 51;
```
with
```
//      An Orb below this level keeps the title up over a story and still moves the band.
//  52  fonts as named faces. theme.json's `fonts` maps a text slot to the file it loads, so
//      slots that share a typeface and size load one face once (one lv_font_load, one copy in
//      PSRAM) instead of one each. The bake also stores the map in flash (`fonts.map`),
//      because theme.json is read from the card only and an Orb with no card must still know
//      which face each slot loads. An Orb below this level ignores the map and loads
//      font_<slot>.bin, so a theme that ships only shared faces draws the compiled face in
//      those slots.
constexpr int THEME_CAPS = 52;
```

(c) Declare the accessor. Replace `bool hasAsset(const char *name);` with
```
bool hasAsset(const char *name);

// The theme's font map: which file each text slot loads (theme.json "fonts":
// {"radar2": "font_body16.bin", ...}), THEME_CAPS 52. load() fills it when the card has the
// theme and empties it first; theme_font fills it from the flash blob when the card does not.
// One table, so nothing holds a second copy.
theme_font::FontMap &fontMap();
```

- [ ] **Step 2: `theme_style.cpp`**

(a) State. After the line `uint32_t s_assetsHash = 0;   // theme.json "assetsHash": covers contents, not just names` add:
```cpp
theme_font::FontMap s_fontMap;   // theme.json "fonts": slot -> file, THEME_CAPS 52
```

(b) Reset. Replace
```
    s_names = Names{};      // stock labels; theme.json may relabel any of them
    s_assetsHash = 0;
```
with
```
    s_names = Names{};      // stock labels; theme.json may relabel any of them
    s_assetsHash = 0;
    s_fontMap.n = 0;
```

(c) Parse. Immediately before the line `            JsonArrayConst list = doc["assets"].as<JsonArrayConst>();` insert:
```cpp
            // "fonts": which file each text slot loads, {"radar2": "font_body16.bin", ...},
            // THEME_CAPS 52. Slots naming one file share it (theme_font loads it once). A name
            // that does not fit the flash index's 23 characters is dropped with a line in the
            // log rather than truncated into a name that can never be found.
            JsonObjectConst fm = doc["fonts"].as<JsonObjectConst>();
            if (!fm.isNull()) {
                for (JsonPairConst kv : fm) {
                    if (!theme_font::map_add(s_fontMap, kv.key().c_str(), kv.value().as<const char *>()))
                        printf("[theme_style] fonts.%s ignored: empty, too long, or the map is full\n",
                               kv.key().c_str());
                }
            }
```

(d) Accessor. Replace `const Settings &settings() { return s_settings; }` with
```cpp
const Settings &settings() { return s_settings; }
theme_font::FontMap &fontMap() { return s_fontMap; }
```

- [ ] **Step 3: `theme_font.h`**

Replace the whole block
```
// Every font file a theme can ship, in slot order, for the bake. theme_art_bake used to
// keep a list of its own with eleven names on it, written when there were eleven slots;
// the fifteen added since (Headlines, Ticker, Weather, wind screen) were shipped by the theme tool,
// declared by the theme, written to the card, and never baked, and this loader reads only
// the bake. Every one of those screens drew the compiled face whatever the design said.
// One list, owned here, is the fix.
const char *const *slot_files(size_t &count);
```
with
```
// The distinct font files this theme loads, for the bake: each slot's file from the theme's
// `fonts` map (theme.json when the card is present, the flash blob when it is not), else the
// slot's own font_<slot>.bin. A face shared by several slots appears once. The slot list is
// owned here, so a screen that gains a slot is baked without anyone touching the bake. The
// pointer is valid until the next call.
const char *const *distinct_files(size_t &count);

// The current slot-to-face map as `slot file` lines: the form theme_art stores as the
// `fonts.map` blob, so an Orb with no card still knows which face each slot loads. Returns the
// length written, or 0 when the theme maps nothing or the buffer is too small.
size_t map_text(char *buf, size_t cap);
```

- [ ] **Step 4: `theme_font.cpp`**

(a) Includes. Replace
```
#include "theme_select.h"
#include "custom_text.h"
```
with
```
#include "theme_select.h"
#include "theme_style.h"
#include "theme_font_resolve.h"
#include "custom_text.h"
```

(b) Resolution. After the line `const lv_font_t *s_font[S_COUNT] = { nullptr };` add:
```cpp
static_assert(S_COUNT <= MAX_SLOTS, "theme_font_resolve.h's MAX_SLOTS must cover every slot");

Resolved s_res;

// Which file each slot loads, for the current theme. The map is theme.json's when the card has
// the theme; with no card it is unreadable, so the bake's flash blob fills the same table.
// Recomputed on every call (27 slots), so nothing goes stale across a theme change.
const Resolved &current() {
    FontMap &m = theme_style::fontMap();
    if (m.n == 0) {
        const uint8_t *blob = nullptr;
        size_t len = 0;
        if (theme_art::find_blob(theme_select::activeSlug(), "fonts.map", blob, len))
            map_parse(m, (const char *)blob, len);
    }
    resolve(SLOT_FILE, S_COUNT, map_lookup, &m, s_res);
    return s_res;
}
```

(c) Loading. Replace the whole loop in `begin()`, from `    for (int i = 0; i < S_COUNT; ++i) {` down to and including the closing `#endif` of the `Serial.printf("[theme_font] %d of %d slots loaded from the theme\n", ...)` line, with:
```cpp
    const Resolved &r = current();
    const lv_font_t *loaded[MAX_SLOTS] = { nullptr };
    for (size_t d = 0; d < r.distinctCount; ++d) {
        const uint8_t *data = nullptr;
        size_t len = 0;
        // Cheap existence check before asking LVGL to parse: a theme that ships no font
        // for a slot is the normal case, not an error worth a log line each boot.
        if (!theme_art::find_blob(theme_select::activeSlug(), r.distinct[d], data, len)) continue;
        // lv_font_load() parses the whole face into LVGL's heap. That heap is PSRAM now
        // (LV_MEM_CUSTOM in lv_conf.h); while it was the 64 KB internal pool, a 44 KB face
        // exhausted it, LVGL did not check the failed allocation, and load_glyph() wrote
        // through the null pointer, a boot loop before any screen drew.
        //
        // Still a copy rather than a read in place, which is not the ideal shape given the
        // bytes are already memory-mapped. It is bounded (tens of KB against megabytes
        // free) and uses LVGL's own tested parser, so the remaining zero-copy version is
        // an optimisation, not a correctness fix. Each DISTINCT file is parsed once and every
        // slot that names it shares the result.
        char path[40];
        snprintf(path, sizeof(path), "%c:%s", DRIVE_LETTER, r.distinct[d]);
        const lv_font_t *f = lv_font_load(path);
        if (!f) {
#ifdef ARDUINO
            Serial.printf("[theme_font] %s failed to parse, using the compiled font\n", r.distinct[d]);
#else
            printf("[theme_font] %s failed to parse, using the compiled font\n", r.distinct[d]);
#endif
            continue;
        }
        loaded[d] = f;
    }
    int distinctLoaded = 0;
    for (size_t d = 0; d < r.distinctCount; ++d) if (loaded[d]) ++distinctLoaded;
    for (int i = 0; i < S_COUNT; ++i) {
        s_font[i] = loaded[r.group[i]];
        if (s_font[i]) ++s_loaded;
    }
#ifdef ARDUINO
    Serial.printf("[theme_font] %d of %d slots loaded from the theme, %d distinct face(s)\n",
                  s_loaded, (int)S_COUNT, distinctLoaded);
#else
    printf("[theme_font] %d of %d slots loaded from the theme, %d distinct face(s)\n",
           s_loaded, (int)S_COUNT, distinctLoaded);
#endif
```

(d) Accessors. Replace `const char *const *slot_files(size_t &count) { count = S_COUNT; return SLOT_FILE; }` with:
```cpp
const char *const *distinct_files(size_t &count) {
    const Resolved &r = current();
    count = r.distinctCount;
    return r.distinct;
}

size_t map_text(char *buf, size_t cap) { return map_serialize(theme_style::fontMap(), buf, cap); }
```

- [ ] **Step 5: `theme_art_bake.cpp`**

(a) Replace `#include "theme_font.h"   // slot_files(): the fonts a theme may ship` with `#include "theme_font.h"   // distinct_files(), map_text(): the fonts this theme loads`.

(b) Replace `const char *const *FONT_ASSETS = theme_font::slot_files(fontN);` with `const char *const *FONT_ASSETS = theme_font::distinct_files(fontN);`.

(c) Store the map. Immediately before the two lines
```
    for (size_t i = 0; i < ASSET_N; ++i) {
        // Never bake something the theme does not declare. Stale files from older pushes
```
insert:
```cpp
    {   // The font map, so an Orb with no card still knows which face each slot loads. It is
        // not a file on the card, so it is written from memory, and only when the theme maps.
        static char mapText[1200];
        const size_t n = theme_font::map_text(mapText, sizeof(mapText));
        if (n && install_asset(slug, "fonts.map", 0, 0, FMT_RAW, (const uint8_t *)mapText, n)) {
            ++baked;
            Serial.printf("[theme_art] stored fonts.map (%u bytes)\n", (unsigned)n);
        }
    }

```

- [ ] **Step 6: Build both environments and run the host tests**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -3
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -3
bash tests/run_host_tests.sh 2>&1 | tail -4
python3 -m unittest tests/test_build_theme.py tests/test_font_bake.py 2>&1 | tail -3
```
Expected: `SUCCESS` for both builds (the device build is cold in a fresh worktree, so allow several minutes), `all host tests passed`, and `OK`. A compile error naming `slot_files` means a caller was missed; `grep -rn slot_files src` must return nothing.

- [ ] **Step 7: Commit**

```bash
git add src/theme/core/theme_style.h src/theme/core/theme_style.cpp src/theme/core/theme_font.h src/theme/core/theme_font.cpp src/theme/core/theme_art_bake.cpp
git commit -m "Firmware: load each font face once, from the theme's fonts map or the flash blob" -m "THEME_CAPS 52. The bake stores the map as fonts.map so an Orb with no card keeps its typeface. Not verified on hardware yet (see the plan's Task 11 gate)." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 6: The simulator loads a theme's fonts from its folder

**Files:**
- Modify: `src/theme/core/theme_art.cpp` (the native `#else` block), `src/platform/sim/sim_main.cpp`

**Interfaces:**
- Consumes: `theme_sd::read_whole(path, len, maxBytes)` (native: reads `sim/sdcard` + `path`), `theme_font::begin()`.
- Produces: native `theme_art::find_blob(slug, assetName, data, len)` returning a pointer to the file `sim/sdcard/themes/<slug>/<assetName>` (read once, cached, never freed, as a memory-mapped blob is not). Misses are cached too.

This is moved forward from spec step 5 so a theme's typeface can be judged in a `--themeshot`. Until now the simulator drew only the compiled fonts.

- [ ] **Step 1: `theme_art.cpp`, native block**

Replace
```
#else   // ---- desktop simulator: no flash partitions, always use the SD/PNG path ----

namespace theme_art {
bool begin() { return false; }
```
with
```
#else   // ---- desktop simulator: no flash partitions, always use the SD/PNG path ----

#include <map>
#include <string>
#include <utility>
#include "theme_sd.h"

namespace theme_art {
bool begin() { return false; }
```
and replace the native stub line
```
bool find_blob(const char *, const char *, const uint8_t *&, size_t &) { return false; }
```
with
```cpp
// The simulator has no flash partition, but a theme's font files are plain files in the folder its
// fake SD card holds (sim/sdcard/themes/<slug>/). Each is read once and kept, never freed, because
// lv_fs hands out pointers into it, exactly as it does into a memory-mapped blob on the device. A
// miss is remembered too, so a slot the theme does not ship costs one failed open, not one per call.
bool find_blob(const char *slug, const char *assetName, const uint8_t *&data, size_t &len) {
    static std::map<std::string, std::pair<uint8_t *, size_t>> cache;
    if (!slug || !slug[0] || !assetName || !assetName[0]) return false;
    const std::string key = std::string(slug) + "/" + assetName;
    auto it = cache.find(key);
    if (it == cache.end()) {
        size_t n = 0;
        uint8_t *buf = theme_sd::read_whole(("/themes/" + key).c_str(), n, 2 * 1024 * 1024);
        it = cache.emplace(key, std::make_pair(buf, n)).first;
    }
    if (!it->second.first) return false;
    data = it->second.first;
    len  = it->second.second;
    return true;
}
```

- [ ] **Step 2: `sim_main.cpp`, load the fonts after LVGL is up**

Find how the theme headers are included and where LVGL starts:
```bash
grep -n '#include "theme_' src/platform/sim/sim_main.cpp
grep -n 'lv_init();' src/platform/sim/sim_main.cpp
```
Add `#include "theme_font.h"` beside the other `theme_*.h` includes. Replace the single `lv_init();` line (it must match exactly once) with:
```cpp
    lv_init();
    theme_font::begin();   // after lv_init() (it registers an lv_fs drive) and before ui_create() reads any face
```

- [ ] **Step 3: Build**

```bash
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -3
```
Expected: `SUCCESS`.

- [ ] **Step 4: Verify in the simulator with a real theme**

Fallout still ships one file per slot (Task 9 migrates it), so its legacy names exercise the same path. Note the current slug first so you can restore it.
```bash
mkdir -p sim/sdcard/themes
cat /tmp/orb_sim_theme_slug
python3 tools/build_theme.py src/theme_assets/fallout --out sim/sdcard/themes
echo fallout > /tmp/orb_sim_theme_slug
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --themeshot /tmp/fontshot > /tmp/fontshot.log 2>&1 &
```
Wait about 40 seconds (the simulator ignores SIGTERM), then:
```bash
kill -9 %1
grep "theme_font" /tmp/fontshot.log
```
Expected: `[theme_font] 22 of 27 slots loaded from the theme, 22 distinct face(s)`. Convert and look at a screenshot:
```bash
python3 -c "from PIL import Image; import glob; [Image.open(f).save(f[:-4] + '.png') for f in glob.glob('/tmp/fontshot*.bmp')]"
ls /tmp/fontshot*.png
```
Open one (for example the flight or headlines screen) and confirm the text is Share Tech Mono (a squared monospace), not Montserrat.

Restore the simulator: `echo <the slug you noted> > /tmp/orb_sim_theme_slug` and `rm -rf sim/sdcard/themes/fallout`.

- [ ] **Step 5: Confirm the other themes still start**

```bash
python3 -m unittest tests/test_elegant_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py 2>&1 | tail -3
```
Expected: `OK`.

- [ ] **Step 6: Commit**

```bash
git add src/theme/core/theme_art.cpp src/platform/sim/sim_main.cpp
git commit -m "Simulator: load a theme's fonts from its folder" -m "find_blob was a native stub and sim_main never called theme_font::begin(), so the simulator only ever drew the compiled fonts. Moved forward from step 5 so a typeface can be judged in a themeshot." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 7: A font golden, captured before any theme is migrated

**Files:**
- Create: `tools/font_golden.py`, `tests/test_theme_fonts_golden.py`, `tests/golden/fonts_elegant.txt`, `tests/golden/fonts_fallout.txt`

**Interfaces:**
- Consumes: `build_theme.firmware_facts()["slots"]` (Task 3).
- Produces:
  - `font_golden.resolve(built: Path) -> dict[str, str]` (slot -> sha256 of the file it resolves to, through `theme.json`'s `fonts` map, else `font_<slot>.bin`, omitting a slot with no file)
  - `font_golden.render(resolved: dict) -> str` (`slot sha256\n` lines, sorted)
  - CLI: `python3 tools/font_golden.py <built theme dir> [--write FILE]`

The golden is captured from the **current, unmigrated** themes and committed first, so Tasks 8 and 9 have a fixed target: after migration each slot must resolve to byte-identical glyphs. Portal is deliberately not covered; its typeface changes on purpose (Task 10).

- [ ] **Step 1: Write the failing test**

`tests/test_theme_fonts_golden.py`:
```python
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import font_golden  # noqa: E402

BUILD = ROOT / 'tools' / 'build_theme.py'
THEMES = ROOT / 'src' / 'theme_assets'
GOLDEN = ROOT / 'tests' / 'golden'


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class ResolveTest(unittest.TestCase):
    def test_a_slot_resolves_through_the_map_else_its_own_file_else_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = Path(tmp)
            (built / 'theme.json').write_text(json.dumps({'fonts': {'radar1': 'font_face.bin'}}))
            (built / 'font_face.bin').write_bytes(b'FACE')
            (built / 'font_radar2.bin').write_bytes(b'OWN')
            got = font_golden.resolve(built)
            self.assertEqual(got['radar1'], sha(b'FACE'))
            self.assertEqual(got['radar2'], sha(b'OWN'))
            self.assertNotIn('radar3', got)

    def test_a_theme_json_with_no_map_uses_the_slot_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = Path(tmp)
            (built / 'theme.json').write_text('{}')
            (built / 'font_settings.bin').write_bytes(b'S')
            self.assertEqual(font_golden.resolve(built), {'settings': sha(b'S')})

    def test_render_is_sorted_slot_and_digest_lines(self):
        self.assertEqual(font_golden.render({'b': 'y', 'a': 'x'}), 'a x\nb y\n')


def converter_available() -> bool:
    return bool(os.environ.get('LV_FONT_CONV') or shutil.which('lv_font_conv') or shutil.which('npx'))


class ShippedThemesKeepTheirTypefaceTest(unittest.TestCase):
    """Every slot of a shipped theme must resolve to byte-identical glyphs before and after it was moved to
    the fonts: block. The golden files were captured from the themes as they were shipped, one font file per slot."""

    def check(self, slug):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run([sys.executable, str(BUILD), str(THEMES / slug), '--out', tmp],
                               capture_output=True, text=True)
            if r.returncode != 0 and 'lv_font_conv' in r.stderr and not converter_available():
                self.skipTest('lv_font_conv (or npx) is needed to bake this theme\'s faces')
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            got = font_golden.render(font_golden.resolve(Path(tmp) / slug))
        want = (GOLDEN / f'fonts_{slug}.txt').read_text(encoding='utf-8')
        self.assertEqual(got, want, f'{slug}: a slot now draws different glyphs than it did when the golden was captured')

    def test_elegant(self):
        self.check('elegant')

    def test_fallout(self):
        self.check('fallout')


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

```bash
python3 -m unittest tests/test_theme_fonts_golden.py 2>&1 | tail -4
```
Expected: ERROR `ModuleNotFoundError: No module named 'font_golden'`.

- [ ] **Step 3: Implement `tools/font_golden.py`**

```python
#!/usr/bin/env python3
"""Which font file each text slot resolves to in a BUILT theme, as `slot sha256` lines.

    python3 tools/font_golden.py build/themes/fallout
    python3 tools/font_golden.py build/themes/fallout --write tests/golden/fonts_fallout.txt

A slot resolves the way the firmware resolves it: through theme.json's `fonts` map, else font_<slot>.bin,
else nothing (the compiled face draws). The hash is of the file's bytes, so two builds match exactly when
the same typeface at the same size draws the same slot. Used to prove a theme moved to a `fonts:` block
still draws every slot with byte-identical glyphs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_theme  # noqa: E402


def resolve(built: Path) -> dict:
    slots = build_theme.firmware_facts()['slots']
    mapping = {}
    theme_json = built / 'theme.json'
    if theme_json.exists():
        mapping = json.loads(theme_json.read_text(encoding='utf-8')).get('fonts', {})
    out = {}
    for slot in sorted(slots):
        path = built / mapping.get(slot, f'font_{slot}.bin')
        if path.is_file():
            out[slot] = hashlib.sha256(path.read_bytes()).hexdigest()
    return out


def render(resolved: dict) -> str:
    return ''.join(f'{slot} {digest}\n' for slot, digest in sorted(resolved.items()))


def main(argv) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('built', type=Path, help='a built theme folder (the output of tools/build_theme.py)')
    ap.add_argument('--write', type=Path, metavar='FILE', help='write the lines here instead of printing them')
    args = ap.parse_args(argv[1:])
    if not args.built.is_dir():
        print(f'error: {args.built} is not a folder', file=sys.stderr)
        return 2
    text = render(resolve(args.built))
    if args.write:
        args.write.parent.mkdir(parents=True, exist_ok=True)
        args.write.write_text(text, encoding='utf-8')
        print(f'wrote {args.write} ({len(text.splitlines())} slot(s))')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
```

- [ ] **Step 4: Capture the goldens from the UNMIGRATED themes**

```bash
python3 tools/build_theme.py src/theme_assets/elegant --out build/golden
python3 tools/build_theme.py src/theme_assets/fallout --out build/golden
python3 tools/font_golden.py build/golden/elegant --write tests/golden/fonts_elegant.txt
python3 tools/font_golden.py build/golden/fallout --write tests/golden/fonts_fallout.txt
wc -l tests/golden/fonts_elegant.txt tests/golden/fonts_fallout.txt
cut -d' ' -f2 tests/golden/fonts_elegant.txt | sort -u | wc -l
cut -d' ' -f2 tests/golden/fonts_fallout.txt | sort -u | wc -l
```
Expected: 11 lines and 22 lines; **8** and **10** distinct digests. Those distinct counts are the design's premise (Elegant 11 files become 8 faces, Fallout 22 become 10); if they differ, stop and re-check the spec before migrating.

- [ ] **Step 5: Run the tests**

```bash
python3 -m unittest tests/test_theme_fonts_golden.py 2>&1 | tail -3
```
Expected: `OK`.

- [ ] **Step 6: Commit**

```bash
git add tools/font_golden.py tests/test_theme_fonts_golden.py tests/golden/fonts_elegant.txt tests/golden/fonts_fallout.txt
git commit -m "Capture a per-slot font golden before migrating any theme" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 8: Migrate Elegant to a `fonts:` block

**Files:**
- Modify: `.gitignore`, `tools/gen_elegant_theme.py`, `tests/test_elegant_theme.py`, `src/theme_assets/elegant/theme.yaml`
- Move: eight of Elegant's eleven `font_*.bin` into `src/theme_assets/elegant/fonts/`, delete the other three (byte-identical duplicates)

**Interfaces:**
- Consumes: the golden `tests/golden/fonts_elegant.txt` (Task 7).
- Produces: `gen_elegant_theme.preserved_blocks(yaml_path: Path) -> str` and `render_yaml(defaults, preserved='')`. Elegant's `theme.yaml` is regenerated from what the firmware reads, which has no place for a hand-written `fonts:` block, so the generator now copies that block across instead of dropping it.

Elegant has no typeface source (its fonts came pre-baked from a `.orb`), so its faces are `.bin` passthrough.

- [ ] **Step 1: Write the failing tests**

In `tests/test_elegant_theme.py`:

(a) Change the gitignore test to look in the new folder. Replace `fonts = sorted(THEME.glob('font_*.bin'))` with `fonts = sorted((THEME / 'fonts').glob('*.bin'))`.

(b) Add to `DefaultThemeFilesTest` (the class that needs nothing but Python; keep its name or rename it, but leave the tests in it):
```python
    def test_eleven_slots_share_eight_faces(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(THEME), '--out', tmp],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, msg=r.stderr)
            built = Path(tmp) / 'elegant'
            fonts = json.loads((built / 'theme.json').read_text())['fonts']
            self.assertEqual(len(fonts), 11)
            self.assertEqual(len(set(fonts.values())), 8)
            self.assertEqual(sorted(p.name for p in built.glob('font_*.bin')), sorted(set(fonts.values())))
```

(c) Make the committed-form test pass the preserved block. Replace
```python
        self.assertEqual((THEME / 'theme.yaml').read_text(encoding='utf-8'), gen.render_yaml(self.state))
```
with
```python
        self.assertEqual((THEME / 'theme.yaml').read_text(encoding='utf-8'),
                         gen.render_yaml(self.state, gen.preserved_blocks(THEME / 'theme.yaml')))
```

(d) Add a class that needs nothing but Python:
```python
class PreservedBlocksTest(unittest.TestCase):
    """The reference is regenerated from what the firmware reads, which has no place for a fonts: block,
    so the generator copies it across instead of dropping it."""

    def test_a_fonts_block_is_copied_verbatim_and_nothing_else(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'theme.yaml'
            p.write_text('slug: x\nnames:\n  clock: A\nfonts:\n  faces:\n    a: {src: a.bin}\n  slots:\n    radar1: a\n'
                         'radar:\n  rangeKm: 1\n', encoding='utf-8')
            self.assertEqual(gen.preserved_blocks(p),
                             'fonts:\n  faces:\n    a: {src: a.bin}\n  slots:\n    radar1: a\n')

    def test_no_file_or_no_block_preserves_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(gen.preserved_blocks(Path(tmp) / 'missing.yaml'), '')
            p = Path(tmp) / 'theme.yaml'
            p.write_text('slug: x\nradar:\n  rangeKm: 1\n', encoding='utf-8')
            self.assertEqual(gen.preserved_blocks(p), '')
```

- [ ] **Step 2: Run to verify they fail**

```bash
python3 -m unittest tests/test_elegant_theme.py 2>&1 | tail -6
```
Expected: FAIL/ERROR (`preserved_blocks` does not exist; there is no `fonts/` folder or `fonts` map yet).

- [ ] **Step 3: The generator preserves hand-written blocks**

In `tools/gen_elegant_theme.py`:

(a) After `_COLOR = re.compile(r'0x[0-9A-F]{6}')` add:
```python
PRESERVED_KEYS = ('fonts',)     # blocks written by hand; the firmware's view of a theme has no place for them


def preserved_blocks(yaml_path: Path) -> str:
    """The top-level blocks named in PRESERVED_KEYS, copied verbatim from an existing theme.yaml, so that
    regenerating the file re-lists every option without discarding what a person wrote by hand."""
    if not yaml_path.exists():
        return ''
    out, keep = [], False
    for line in yaml_path.read_text(encoding='utf-8').splitlines():
        top = re.match(r'^([A-Za-z_][A-Za-z0-9_]*):', line)
        if top:
            keep = top.group(1) in PRESERVED_KEYS
        if keep:
            out.append(line)
    return ('\n'.join(out).rstrip() + '\n') if out else ''
```

(b) Change `def render_yaml(defaults: dict) -> str:` to `def render_yaml(defaults: dict, preserved: str = '') -> str:` and, immediately before that function's final `return '\n'.join(out) + '\n'`, add:
```python
    if preserved:
        out.append('')
        out.append(preserved.rstrip('\n'))
```

(c) In `generate`, replace
```python
        return render_yaml(run_dumper(binary, build_folder(theme, Path(tmp) / 'built')))
```
with
```python
        return render_yaml(run_dumper(binary, build_folder(theme, Path(tmp) / 'built')),
                           preserved_blocks(theme / 'theme.yaml'))
```

- [ ] **Step 4: Let git track the new font folder**

`.gitignore` has `*.bin` with a single exception for `font_*.bin`. Add a second exception directly under the line `!src/theme_assets/**/font_*.bin`:
```
!src/theme_assets/**/fonts/*.bin
```
Check it works:
```bash
git check-ignore -v src/theme_assets/elegant/fonts/x.bin; echo "exit $?"
```
Expected: no output and `exit 1` (not ignored).

- [ ] **Step 5: Move the fonts, after proving the duplicates are duplicates**

```bash
cmp src/theme_assets/elegant/font_intel_age.bin src/theme_assets/elegant/font_intel_source.bin
cmp src/theme_assets/elegant/font_menu_next.bin src/theme_assets/elegant/font_menu_prev.bin
cmp src/theme_assets/elegant/font_menu_next.bin src/theme_assets/elegant/font_radar2.bin
```
`cmp` prints nothing when the files are identical. If it prints anything, stop.
```bash
mkdir src/theme_assets/elegant/fonts
git mv src/theme_assets/elegant/font_intel_age.bin src/theme_assets/elegant/fonts/age12.bin
git mv src/theme_assets/elegant/font_intel_brief.bin src/theme_assets/elegant/fonts/brief18.bin
git mv src/theme_assets/elegant/font_intel_text.bin src/theme_assets/elegant/fonts/text22.bin
git mv src/theme_assets/elegant/font_intel_title.bin src/theme_assets/elegant/fonts/title32.bin
git mv src/theme_assets/elegant/font_menu_current.bin src/theme_assets/elegant/fonts/menu46.bin
git mv src/theme_assets/elegant/font_menu_next.bin src/theme_assets/elegant/fonts/body16.bin
git mv src/theme_assets/elegant/font_radar1.bin src/theme_assets/elegant/fonts/radar22.bin
git mv src/theme_assets/elegant/font_settings.bin src/theme_assets/elegant/fonts/settings27.bin
git rm -q src/theme_assets/elegant/font_intel_source.bin src/theme_assets/elegant/font_menu_prev.bin src/theme_assets/elegant/font_radar2.bin
```
The face names say what each is: `age12` (12 px, used by the age and source credits), `body16` (16 px: the menu's prev/next hints and radar text 2), `brief18`, `text22`, `radar22`, `title32`, `settings27`, `menu46`.

- [ ] **Step 6: Add the `fonts:` block and regenerate**

Add this block at the end of `src/theme_assets/elegant/theme.yaml` (after a blank line):
```yaml
fonts:
  faces:                        # each typeface and size, defined once (docs/theme-yaml.md)
    age12:      {src: fonts/age12.bin}
    body16:     {src: fonts/body16.bin}
    brief18:    {src: fonts/brief18.bin}
    text22:     {src: fonts/text22.bin}
    radar22:    {src: fonts/radar22.bin}
    title32:    {src: fonts/title32.bin}
    settings27: {src: fonts/settings27.bin}
    menu46:     {src: fonts/menu46.bin}
  slots:
    intel_age: age12
    intel_source: age12
    intel_brief: brief18
    intel_text: text22
    intel_title: title32
    menu_current: menu46
    menu_next: body16
    menu_prev: body16
    radar1: radar22
    radar2: body16
    settings: settings27
```
Regenerate, then confirm the file did not change (the block was preserved) and the form check passes:
```bash
python3 tools/gen_elegant_theme.py
git diff --stat src/theme_assets/elegant/theme.yaml
python3 tools/gen_elegant_theme.py --check
```
Expected: the diff shows only the appended block (about 23 added lines, none removed), and `elegant theme is up to date`.

- [ ] **Step 7: Run the tests, including the golden**

```bash
python3 -m unittest tests/test_elegant_theme.py tests/test_theme_fonts_golden.py tests/test_build_all_themes.py 2>&1 | tail -4
```
Expected: `OK`. `test_elegant` in the golden test is the proof that all 11 slots draw byte-identical glyphs.

- [ ] **Step 8: Commit**

```bash
git add .gitignore tools/gen_elegant_theme.py tests/test_elegant_theme.py src/theme_assets/elegant
git commit -m "Elegant: define its eight font faces once" -m "Eleven per-slot font files become eight faces in fonts/, mapped by a fonts: block. The golden proves every slot still resolves to byte-identical glyphs. gen_elegant_theme now preserves a hand-written fonts block when it regenerates the reference." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 9: Migrate Fallout to a `fonts:` block, and retire `fallout_fonts.py`

**Files:**
- Create: `tests/font_facts.py`
- Modify: `tests/test_fallout_theme.py`, `src/theme_assets/fallout/theme.yaml`
- Delete: the 22 `src/theme_assets/fallout/font_*.bin`, `tools/fallout_fonts.py`

**Interfaces:**
- Consumes: golden `tests/golden/fonts_fallout.txt`; `font_bake` (through the builder); `tests/stub`-free real `lv_font_conv` (needs `npx`).
- Produces: `tests/font_facts.py` exporting `font_facts(path) -> dict` (`tag`, `size`, `bpp`, `compression`, `codepoints`), `font_table_bytes(path) -> int`, `converter_available() -> bool`.

Fallout's typeface is now baked by the builder from `source/ShareTechMono-Regular.ttf` on every build, so building Fallout needs `lv_font_conv` (found from `$LV_FONT_CONV`, `PATH`, or fetched with `npx`). The tests skip with a reason when none is available.

- [ ] **Step 1: Create `tests/font_facts.py` by moving the helpers**

Cut the two functions `font_facts` and `font_table_bytes` (with their docstrings, unchanged) out of `tests/test_fallout_theme.py` and paste them into a new `tests/font_facts.py`, under this header:
```python
"""Helpers for tests that read lv_font_conv binaries the way the firmware's loader does."""
import os
import shutil
import struct
from pathlib import Path


def converter_available() -> bool:
    return bool(os.environ.get('LV_FONT_CONV') or shutil.which('lv_font_conv') or shutil.which('npx'))
```

- [ ] **Step 2: Rewrite Fallout's font tests (they will fail until the theme is migrated)**

In `tests/test_fallout_theme.py`:

(a) Replace the line `import fallout_fonts  # noqa: E402` with:
```python
sys.path.insert(0, str(ROOT / 'tests'))
from font_facts import converter_available, font_facts, font_table_bytes  # noqa: E402
```
and add `import json` to the imports at the top.

(b) After the `SPRITES = ...` constant add:
```python
FACE = THEME / 'source' / 'ShareTechMono-Regular.ttf'
FONT_PX = {     # the size each slot's layout was tuned for: the contract this theme keeps
    'menu_current': 46, 'settings': 27, 'radar1': 22, 'radar2': 16, 'radar3': 16, 'radar4': 16,
    'weather1': 22, 'weather2': 22, 'weather3': 16, 'weather4': 16, 'intel_title': 32, 'intel_text': 22,
    'intel_source': 12, 'intel_age': 12, 'intel_brief': 18, 'ticker_name': 16, 'ticker_price': 40,
    'ticker_change': 22, 'ticker_strip': 16, 'wind_title': 28, 'wind_ask': 20, 'wind_turns': 16}
GLYPHS = list(range(0x20, 0x7F)) + [0xB0, 0xB1, 0xB7, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026]


def build_or_skip(out: Path):
    """Build Fallout into `out`; skip the caller's class when its faces cannot be baked here."""
    r = build(out)
    if r.returncode != 0:
        if 'lv_font_conv' in r.stderr and not converter_available():
            raise unittest.SkipTest("lv_font_conv (or npx) is needed to bake Fallout's faces")
        raise AssertionError(r.stderr)
    return out / 'fallout'
```

(c) Delete the local definitions of `font_facts` and `font_table_bytes` (they moved in step 1).

(d) Replace the class `FalloutFirmwareFontTest` with:
```python
class FalloutFirmwareFontTest(unittest.TestCase):
    """The device loads each font with LVGL's lv_font_load. If that fails it says nothing and draws the compiled
    face, so a bad file would just look like the font never shipped. Needs the native env's liblvgl.a
    (`pio run -e native` once) and a C++ compiler."""

    @classmethod
    def setUpClass(cls):
        libs = sorted((ROOT / '.pio' / 'build' / 'native').glob('lib*/liblvgl.a'))
        if not libs:
            raise unittest.SkipTest('liblvgl.a is not built: run `pio run -e native` once')
        cls._tmp = tempfile.TemporaryDirectory()
        cls.check = Path(cls._tmp.name) / 'font_load_check'
        lvgl = ROOT / '.pio' / 'libdeps' / 'native' / 'lvgl'
        try:
            subprocess.run(['c++', '-std=c++17', '-O1', '-DLV_CONF_INCLUDE_SIMPLE', f'-I{ROOT / "include"}', f'-I{lvgl}',
                            f'-I{lvgl / "src"}', str(ROOT / 'tests/font_load_check.cpp'), str(libs[0]), '-lm',
                            '-o', str(cls.check)], check=True, capture_output=True)
            cls.built = build_or_skip(Path(cls._tmp.name) / 'themes')
        except (OSError, subprocess.CalledProcessError) as e:
            cls._tmp.cleanup()
            raise unittest.SkipTest(f'cannot build the font loader harness: {e}')
        except unittest.SkipTest:
            cls._tmp.cleanup()
            raise
        cls.faces = sorted(set(json.loads((cls.built / 'theme.json').read_text())['fonts'].values()))

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_lvgl_loads_every_face_with_the_glyphs_the_screens_draw(self):
        want = [f'{c:X}' for c in GLYPHS]
        for name in self.faces:
            r = subprocess.run([str(self.check), str(self.built), name, *want], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, f'{name}: {r.stdout.strip() or r.stderr.strip()}')
```

(e) Replace the class `FalloutFontTest` (all of it, including `test_the_font_script_reproduces_the_committed_fonts`, which the golden test replaces) with:
```python
class FalloutFontTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        try:
            cls.built = build_or_skip(Path(cls._tmp.name))
        except unittest.SkipTest:
            cls._tmp.cleanup()
            raise
        cls.map = json.loads((cls.built / 'theme.json').read_text())['fonts']

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_it_maps_exactly_the_slots_it_always_did(self):
        self.assertEqual(set(self.map), set(FONT_PX))

    def test_it_ships_ten_faces_not_twenty_two(self):
        self.assertEqual(len(set(self.map.values())), 10)
        self.assertEqual(sorted(p.name for p in self.built.glob('font_*.bin')), sorted(set(self.map.values())))

    def test_each_slot_is_baked_at_the_size_its_layout_was_tuned_for(self):
        for slot, px in FONT_PX.items():
            f = font_facts(self.built / self.map[slot])
            self.assertEqual(f['tag'], b'head', slot)
            # 4 bits per pixel, uncompressed: the curved-text and menu renderers read the bitmaps directly
            self.assertEqual((f['bpp'], f['compression']), (4, 0), slot)
            self.assertEqual(f['size'], px, f'{slot} is baked at the wrong size')

    def test_no_font_is_truncated(self):
        for name in set(self.map.values()):
            size = (self.built / name).stat().st_size
            self.assertEqual(font_table_bytes(self.built / name), size,
                             f'{name}: its tables do not add up to its {size} bytes')

    def test_each_font_has_the_glyphs_the_screens_draw(self):
        want = set(range(0x20, 0x7F)) | {0xB0, 0xB7, 0x2019, 0x201C, 0x2014}     # ASCII, degrees, dot, quotes, dash
        for name in set(self.map.values()):
            missing = want - font_facts(self.built / name)['codepoints']
            self.assertFalse(missing, f'{name} lacks {sorted(hex(c) for c in missing)}: it would draw blank')

    def test_the_licence_ships_with_the_face(self):
        # the OFL requires the copyright notice and licence to travel with the font
        self.assertTrue(FACE.exists())
        licence = (THEME / 'source' / 'OFL.txt').read_text(encoding='utf-8')
        self.assertIn('SIL Open Font License', licence)
        self.assertIn('Carrois', licence)
```

- [ ] **Step 3: Run to verify they fail**

```bash
python3 -m unittest tests/test_fallout_theme.py 2>&1 | tail -6
```
Expected: `FalloutFontTest` errors (`KeyError: 'fonts'`, since `theme.json` has no map yet).

- [ ] **Step 4: Migrate the theme**

Add this block at the end of `src/theme_assets/fallout/theme.yaml`:
```yaml

# ---- TYPEFACE -------------------------------------------------------------------------------
# Share Tech Mono, one face per size. Each is baked once from source/ by tools/build_theme.py and
# shared by every slot that names it: ten faces for twenty-two slots.
fonts:
  faces:
    st12: {src: source/ShareTechMono-Regular.ttf, size: 12}
    st16: {src: source/ShareTechMono-Regular.ttf, size: 16}
    st18: {src: source/ShareTechMono-Regular.ttf, size: 18}
    st20: {src: source/ShareTechMono-Regular.ttf, size: 20}
    st22: {src: source/ShareTechMono-Regular.ttf, size: 22}
    st27: {src: source/ShareTechMono-Regular.ttf, size: 27}
    st28: {src: source/ShareTechMono-Regular.ttf, size: 28}
    st32: {src: source/ShareTechMono-Regular.ttf, size: 32}
    st40: {src: source/ShareTechMono-Regular.ttf, size: 40}
    st46: {src: source/ShareTechMono-Regular.ttf, size: 46}
  slots:
    menu_current: st46
    settings: st27
    radar1: st22
    radar2: st16
    radar3: st16
    radar4: st16
    weather1: st22
    weather2: st22
    weather3: st16
    weather4: st16
    intel_title: st32
    intel_text: st22
    intel_source: st12
    intel_age: st12
    intel_brief: st18
    ticker_name: st16
    ticker_price: st40
    ticker_change: st22
    ticker_strip: st16
    wind_title: st28
    wind_ask: st20
    wind_turns: st16
```
Update the header comment: replace the line
```
# baked into font_*.bin by tools/fallout_fonts.py. Each face is baked at one size, so the *Size options do not
```
with
```
# baked from source/ by tools/build_theme.py (the fonts: block at the end). Each face is baked at one size, so the *Size options do not
```
Delete the 22 committed per-slot fonts and the script that made them:
```bash
git rm -q src/theme_assets/fallout/font_*.bin tools/fallout_fonts.py
```

- [ ] **Step 5: Run the tests, including the golden**

```bash
python3 -m unittest tests/test_fallout_theme.py tests/test_theme_fonts_golden.py tests/test_font_bake.py tests/test_build_all_themes.py 2>&1 | tail -6
```
Expected: `OK`. The `test_fallout` golden test proves each of the 22 slots resolves to the **byte-identical** face that the deleted per-slot file was. If it fails, its message names a slot whose glyphs changed: the pinned `lv_font_conv@1.5.3` is not the one that made the old files. Stop and find out why; do not regenerate the golden.

- [ ] **Step 6: Confirm nothing still calls the deleted script**

```bash
grep -rn "fallout_fonts" . --include='*.py' --include='*.sh' --include='*.yaml' --include='*.cpp' | grep -v "^./docs/superpowers/"
```
Expected: no output. (`docs/theme-yaml.md` still names it in prose; Task 11 rewrites that paragraph.)

- [ ] **Step 7: Commit**

```bash
git add tests/font_facts.py tests/test_fallout_theme.py src/theme_assets/fallout/theme.yaml
git commit -m "Fallout: bake its ten faces from the TTF and share them across 22 slots" -m "The 22 committed per-slot .bin files and tools/fallout_fonts.py go; build_theme.py bakes each face once from source/ShareTechMono-Regular.ttf. The golden proves every slot resolves to byte-identical glyphs. Building Fallout now needs lv_font_conv (npx fetches it)." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 10: Portal gets Barlow, in five shared sizes

**Files:**
- Create: `src/theme_assets/portal/source/Barlow-Regular.ttf`, `src/theme_assets/portal/source/OFL.txt`
- Modify: `src/theme_assets/portal/theme.yaml`, `src/theme_assets/portal/README.md`, `tests/test_portal_theme.py`

**Interfaces:**
- Consumes: `tests/font_facts.py` (Task 9), the builder's `fonts:` block (Task 3).
- Produces: a `fonts:` block giving 24 slots five faces (`barlow14`, `barlow20`, `barlow28`, `barlow40`, `barlow46`).

Portal shipped no fonts at all, so its lettering was the compiled faces. This is a **deliberate change of Portal's typeface** (owner decision D1): Barlow, an open-licence DIN-style sans, with sizes kept few because each distinct size is a separate face in flash and PSRAM. All four radar text slots use the 20 px face, so the callsign line is no longer larger than the others. No golden covers Portal's fonts.

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_portal_theme.py` (add `import hashlib`, `import json`, `import os` where absent, and, near its other `sys.path` line, `sys.path.insert(0, str(ROOT / 'tests'))` then `from font_facts import converter_available, font_facts, font_table_bytes  # noqa: E402`). Reuse the file's existing `ROOT`, `THEME` and `build` helpers; if it has no `THEME`/`build`, define them as `tests/test_fallout_theme.py` does, for `portal`.

```python
BARLOW_SHA256 = '95aa02c7c43096e0dd44d787ba6216864a67157e402adab59b35572e0c1577ea'   # Barlow-Regular.ttf, google/fonts ofl/barlow
# Every slot Fallout themes, plus the menu's prev/next hints: one typeface for all the text Portal draws
PORTAL_SLOTS = {'menu_current', 'menu_prev', 'menu_next', 'settings', 'radar1', 'radar2', 'radar3', 'radar4',
                'weather1', 'weather2', 'weather3', 'weather4', 'intel_title', 'intel_text', 'intel_source',
                'intel_age', 'intel_brief', 'ticker_name', 'ticker_price', 'ticker_change', 'ticker_strip',
                'wind_title', 'wind_ask', 'wind_turns'}
PORTAL_PX = {14, 20, 28, 40, 46}
GLYPHS = set(range(0x20, 0x7F)) | {0xB0, 0xB1, 0xB7, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026}


class PortalFontsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(ROOT / 'src' / 'theme_assets' / 'portal'),
                            '--out', cls._tmp.name], capture_output=True, text=True)
        if r.returncode != 0:
            cls._tmp.cleanup()
            if 'lv_font_conv' in r.stderr and not converter_available():
                raise unittest.SkipTest("lv_font_conv (or npx) is needed to bake Portal's faces")
            raise AssertionError(r.stderr)
        cls.stderr = r.stderr
        cls.built = Path(cls._tmp.name) / 'portal'
        cls.map = json.loads((cls.built / 'theme.json').read_text())['fonts']

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_barlow_ships_with_its_licence(self):
        ttf = ROOT / 'src' / 'theme_assets' / 'portal' / 'source' / 'Barlow-Regular.ttf'
        self.assertEqual(hashlib.sha256(ttf.read_bytes()).hexdigest(), BARLOW_SHA256)
        licence = (ttf.parent / 'OFL.txt').read_text(encoding='utf-8')
        self.assertIn('SIL OPEN FONT LICENSE', licence.upper())
        self.assertIn('Barlow Project Authors', licence)

    def test_every_slot_portal_draws_has_a_face(self):
        self.assertEqual(set(self.map), PORTAL_SLOTS)

    def test_sizes_are_kept_to_five(self):
        files = set(self.map.values())
        self.assertLessEqual(len(files), 5)
        self.assertEqual({font_facts(self.built / f)['size'] for f in files}, PORTAL_PX)
        self.assertEqual(sorted(p.name for p in self.built.glob('font_*.bin')), sorted(files))

    def test_all_four_radar_text_slots_share_one_face(self):
        self.assertEqual(len({self.map[f'radar{i}'] for i in range(1, 5)}), 1)

    def test_every_face_is_the_format_the_firmware_reads_and_has_the_glyphs_the_screens_draw(self):
        for name in set(self.map.values()):
            f = font_facts(self.built / name)
            self.assertEqual((f['tag'], f['bpp'], f['compression']), (b'head', 4, 0), name)
            self.assertEqual(font_table_bytes(self.built / name), (self.built / name).stat().st_size, name)
            missing = GLYPHS - f['codepoints']
            self.assertFalse(missing, f'{name} lacks {sorted(hex(c) for c in missing)}: it would draw blank')

    def test_it_builds_without_a_warning(self):
        self.assertEqual(self.stderr, '')
```

- [ ] **Step 2: Run to verify they fail**

```bash
python3 -m unittest tests/test_portal_theme.py 2>&1 | tail -6
```
Expected: `PortalFontsTest` errors (`KeyError: 'fonts'`).

- [ ] **Step 3: Add the face and its licence**

```bash
curl -fsSL -o src/theme_assets/portal/source/Barlow-Regular.ttf https://github.com/google/fonts/raw/main/ofl/barlow/Barlow-Regular.ttf
curl -fsSL -o src/theme_assets/portal/source/OFL.txt https://github.com/google/fonts/raw/main/ofl/barlow/OFL.txt
shasum -a 256 src/theme_assets/portal/source/Barlow-Regular.ttf
head -3 src/theme_assets/portal/source/OFL.txt
```
Expected: the digest `95aa02c7c43096e0dd44d787ba6216864a67157e402adab59b35572e0c1577ea` (as fetched on 2026-09-21) and a first line `Copyright 2017 The Barlow Project Authors (https://github.com/jpt/barlow)` followed by the SIL OFL 1.1 text. If upstream has since changed the file, read the new `OFL.txt`, confirm it is still the OFL, and update the pin in the test and say so in the commit; do not weaken the pin silently.

- [ ] **Step 4: Add the `fonts:` block to `src/theme_assets/portal/theme.yaml`**

At the end of the file (after a blank line):
```yaml
# ---- TYPEFACE -------------------------------------------------------------------------------
# Barlow (SIL OFL, licence in source/): a DIN-style sans, clinical and industrial, legible at 14 to 20 px on
# the AMOLED. Five sizes, not one per slot: each is a separate face in flash and in PSRAM. Every radar text
# line uses the 20.
fonts:
  faces:
    barlow14: {src: source/Barlow-Regular.ttf, size: 14}
    barlow20: {src: source/Barlow-Regular.ttf, size: 20}
    barlow28: {src: source/Barlow-Regular.ttf, size: 28}
    barlow40: {src: source/Barlow-Regular.ttf, size: 40}
    barlow46: {src: source/Barlow-Regular.ttf, size: 46}
  slots:
    menu_current: barlow46
    menu_prev: barlow14
    menu_next: barlow14
    settings: barlow28
    radar1: barlow20
    radar2: barlow20
    radar3: barlow20
    radar4: barlow20
    weather1: barlow20
    weather2: barlow20
    weather3: barlow14
    weather4: barlow14
    intel_title: barlow28
    intel_text: barlow20
    intel_source: barlow14
    intel_age: barlow14
    intel_brief: barlow20
    ticker_name: barlow14
    ticker_price: barlow40
    ticker_change: barlow20
    ticker_strip: barlow14
    wind_title: barlow28
    wind_ask: barlow20
    wind_turns: barlow14
```
Append to `src/theme_assets/portal/README.md`:
```markdown

## Typeface

Barlow Regular (Copyright 2017 The Barlow Project Authors, SIL Open Font License 1.1; the licence text is
`source/OFL.txt`), baked by `tools/build_theme.py` from the `fonts:` block in `theme.yaml`. Five sizes are
shared by 24 slots. Add a size only where a `--themeshot` shows a layout needs it: every extra size is another
face in flash and in PSRAM.
```

- [ ] **Step 5: Run the tests**

```bash
python3 -m unittest tests/test_portal_theme.py 2>&1 | tail -4
```
Expected: `OK`. A missing-glyph failure means Barlow lacks a codepoint the screens draw; add the codepoint to that face's `ranges` only if the font has it, otherwise stop and report it.

- [ ] **Step 6: Judge it in the simulator (owner checkpoint)**

Task 6 made the simulator load theme fonts, so this is the first time Portal's lettering can be seen.
```bash
mkdir -p sim/sdcard/themes
cat /tmp/orb_sim_theme_slug
python3 tools/build_theme.py src/theme_assets/portal --out sim/sdcard/themes
echo portal > /tmp/orb_sim_theme_slug
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SIM_SETTLE_MS=1500 .pio/build/native/program --themeshot /tmp/portalshot > /tmp/portalshot.log 2>&1 &
```
After about 40 seconds: `kill -9 %1`, then
```bash
grep "theme_font" /tmp/portalshot.log
python3 -c "from PIL import Image; import glob; [Image.open(f).save(f[:-4] + '.png') for f in glob.glob('/tmp/portalshot*.bmp')]"
```
Expected log line: `[theme_font] 24 of 27 slots loaded from the theme, 5 distinct face(s)`. Look at the flight (radar), menu, Settings, headlines and ticker screens. Check in particular that the radar's three text lines still fit their dark pills at 20 px and nothing clips. If a pill needs adjusting, edit the `radar.rtext` entries in Portal's `theme.yaml` (the option names are in `RadarText` in `theme_style.h`) and re-shoot. **The owner decides whether the lettering suits Portal;** changing a size is one line in the `fonts:` block. Restore the simulator: `echo <noted slug> > /tmp/orb_sim_theme_slug` and `rm -rf sim/sdcard/themes/portal`.

- [ ] **Step 7: Commit**

```bash
git add src/theme_assets/portal/source/Barlow-Regular.ttf src/theme_assets/portal/source/OFL.txt src/theme_assets/portal/theme.yaml src/theme_assets/portal/README.md tests/test_portal_theme.py
git commit -m "Portal: a chosen typeface, Barlow, in five shared sizes" -m "Portal shipped no fonts and inherited compiled faces from a temporary TTF that is not in the repo. It now bakes Barlow (SIL OFL) for 24 slots from five faces, with all four radar text lines on the 20 px face. A deliberate change of Portal's lettering; no golden covers it." -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

---

### Task 11: Coverage guard, docs, version bump, and the hardware gate

**Files:**
- Create: `tests/test_theme_font_coverage.py`
- Modify: `docs/theme-yaml.md`, `docs/adding-a-screen.md`, `src/config.h`

**Interfaces:**
- Consumes: `font_golden.resolve` (Task 7), every shipped theme's `fonts:` block (Tasks 8 to 10).
- Produces: a test that fails when a shipped theme has no face for `menu_current`, `radar1` or `radar2`, the three slots that draw with a compiled bitmap face today. Deleting the compiled fonts (a later plan) is safe only while this passes.

- [ ] **Step 1: Write the coverage test**

`tests/test_theme_font_coverage.py`:
```python
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tests'))
import font_golden  # noqa: E402
from font_facts import converter_available  # noqa: E402

THEMES = ROOT / 'src' / 'theme_assets'
# The only three text slots whose compiled fallback is a bitmap face baked into the firmware
# (custom_menu_font1, custom_radar_font1, custom_radar_font2). Radar text 3, the menu's prev/next
# hints and Settings fall back to LVGL's own Montserrat, which stays. A theme with no face for one of
# these three would change lettering the day the compiled fonts are deleted.
COMPILED_BITMAP_SLOTS = ('menu_current', 'radar1', 'radar2')


def missing_compiled_slots(resolved: dict) -> list:
    return [s for s in COMPILED_BITMAP_SLOTS if s not in resolved]


class FontCoverageTest(unittest.TestCase):
    def test_the_check_can_fail(self):
        self.assertEqual(missing_compiled_slots({}), list(COMPILED_BITMAP_SLOTS))
        self.assertEqual(missing_compiled_slots({'menu_current': 'x', 'radar1': 'x'}), ['radar2'])
        self.assertEqual(missing_compiled_slots({s: 'x' for s in COMPILED_BITMAP_SLOTS}), [])

    def test_every_shipped_theme_supplies_a_face_for_the_three_compiled_bitmap_slots(self):
        themes = sorted(p for p in THEMES.iterdir() if (p / 'theme.yaml').exists())
        self.assertGreaterEqual(len(themes), 3, 'expected at least elegant, fallout and portal')
        for theme in themes:
            with self.subTest(theme=theme.name), tempfile.TemporaryDirectory() as tmp:
                r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'build_theme.py'), str(theme), '--out', tmp],
                                   capture_output=True, text=True)
                if r.returncode != 0 and 'lv_font_conv' in r.stderr and not converter_available():
                    self.skipTest("lv_font_conv (or npx) is needed to bake this theme's faces")
                self.assertEqual(r.returncode, 0, msg=r.stderr)
                built = next(p for p in Path(tmp).iterdir() if not p.name.startswith('.'))
                missing = missing_compiled_slots(font_golden.resolve(built))
                self.assertFalse(missing, f'{theme.name} draws {missing} with a compiled face; give it a face in fonts:')


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it**

```bash
python3 -m unittest tests/test_theme_font_coverage.py 2>&1 | tail -4
```
Expected: `OK` (all three themes now supply the slots). To see the guard bite, temporarily remove `menu_current: st46` from Fallout's `fonts.slots`, rerun (expect FAIL naming `menu_current`), then put it back.

- [ ] **Step 3: Docs**

(a) In `docs/theme-yaml.md`, insert this section immediately before the heading `## Omitting a section`:
```markdown
## Fonts: define each face once

A face is one typeface at one size, baked into one `.bin`. Every text slot that wants it points at it by name,
and the Orb loads each face once however many slots share it:

```yaml
fonts:
  faces:
    label:    {src: source/ShareTechMono-Regular.ttf, size: 16}
    title:    {src: source/ShareTechMono-Regular.ttf, size: 32}
    prebaked: {src: fonts/old.bin}          # a .bin is copied as it is
  slots:
    radar2: label
    radar3: label
    intel_title: title
```

- `src` is a `.ttf` or `.otf` inside the theme folder (baked with `lv_font_conv`, which the build fetches with
  `npx` if it is not installed) or a `.bin` that is already baked. `size` is in pixels and belongs to a
  `.ttf`/`.otf` only; `ranges` optionally replaces the default characters (`lv_font_conv --range` form). Keep a
  `.bin` face's file outside the top of the folder (for example in `fonts/`): the top level is scanned for
  legacy per-slot files.
- Slot names are the firmware's slot files without `font_` and `.bin`: `radar2`, `intel_title`,
  `menu_current`, and so on. The build lists them when you get one wrong.
- A face name is at most 14 characters and cannot be a slot name.
- **Sizes cost memory.** Every distinct typeface-and-size is a separate face in flash and in PSRAM, so reuse a
  size unless a layout genuinely needs another. The build prints the count and warns above 10.
- A slot you leave out keeps loading `font_<slot>.bin` from the folder if there is one (how themes were made
  before this), otherwise the compiled face.
- The Orb stores the slot-to-face map in flash when it bakes the theme, so it keeps its typeface with no card.
```
(b) In the same file, replace the Fallout paragraph's font sentence. The text
```
(which borrows Portal's drawing helpers), and a typeface baked by `tools/fallout_fonts.py`. It is also the
one to copy for fonts: one `font_*.bin` per text slot at the size that slot is laid out for, made with
`lv_font_conv --bpp 4 --no-compress`.
```
becomes
```
(which borrows Portal's drawing helpers), and a typeface baked from `source/` by its `fonts:` block. It is
also the one to copy for fonts: ten faces, one per size, shared by twenty-two slots.
```
(c) In `docs/adding-a-screen.md`, replace ``shipped as a `font_<screen>_<slot>.bin` theme font, with a slot`` with ``declared as a face in the theme's `fonts:` block and mapped to a slot``.

Then confirm nothing else describes the old per-slot shipping:
```bash
grep -rn "fallout_fonts\|one .font_\*\.bin. per text slot" docs README.md CLAUDE.md | grep -v "^docs/superpowers/"
```
Expected: no output.

- [ ] **Step 4: Bump the firmware version**

In `src/config.h`, change `"2.17.0"` to `"2.18.0"` on the `FW_VERSION` line (it is shown on the web config page and the Stats screen, so it is how a given Orb's firmware is told apart). Then:
```bash
grep -rn '2\.17\.0' src docs README.md CLAUDE.md | grep -v "^docs/superpowers/"
```
Expected: no output (nothing else quotes the old version).

- [ ] **Step 5: The whole suite and both builds**

```bash
bash tests/run_host_tests.sh 2>&1 | tail -5
python3 -m unittest tests/test_build_theme.py tests/test_font_bake.py tests/test_theme_fonts_golden.py tests/test_theme_font_coverage.py tests/test_elegant_theme.py tests/test_portal_theme.py tests/test_fallout_theme.py tests/test_build_all_themes.py 2>&1 | tail -4
~/.platformio/penv/bin/pio run -e native 2>&1 | tail -2
~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 2>&1 | tail -2
```
Expected: `all host tests passed`, `OK`, and `SUCCESS` twice.

- [ ] **Step 6: Commit**

```bash
git add tests/test_theme_font_coverage.py docs/theme-yaml.md docs/adding-a-screen.md src/config.h
git commit -m "Guard font coverage, document the fonts block, bump FW_VERSION to 2.18.0" -m "Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>"
```

- [ ] **Step 7: HARDWARE GATE (rule 1): an agent cannot pass this alone; hand it to the owner**

Nothing in Tasks 5 and 6 has run on a real Orb. **Do not call this work done until every line below is checked, and report which were and were not.**

1. Build every theme onto the card: `python3 tools/build_all_themes.py --out /Volumes/ORB/themes`, and copy `elegant` there too if the card still has the old `default` folder (the saved slug `default` no longer matches anything).
2. Flash: `~/.platformio/penv/bin/pio run -e esp32-s3-amoled-175 -t upload`, then watch `pio device monitor -b 115200`.
3. For each theme (select it in Settings, let it reboot and bake), the serial log shows the bake storing the faces and the map, then `[theme_font] N of 27 slots loaded from the theme, M distinct face(s)`: **Elegant 11 and 8, Fallout 22 and 10, Portal 24 and 5**. Note any `failed to parse` line.
4. **Pull the SD card and reboot.** The same `[theme_font]` line must appear (the map now comes from flash) and the lettering must be unchanged. This is the case Review Focus 1 exists for.
5. `?orb mem` before and after switching to Fallout: PSRAM free should be higher than on the previous firmware, by roughly the 53 KB that twelve fewer decoded faces save. Record both numbers.
6. Eyeball each screen of each theme. Portal's typeface is intentionally different (Barlow); Elegant and Fallout must look the same as before.
7. Report results in the merge commit, including anything not checked.

---

## Follow-on plans (not in this plan)

Spec steps 2 to 6, each its own plan when reached: palette roles and the built-in `default` (with goldens and the drift test), the procedural clock face, retiring Default/Office, deleting the compiled art and fonts, and the final docs pass. Two things this plan hands them:

- `tests/test_theme_font_coverage.py` walks every folder in `src/theme_assets` that has a `theme.yaml`. The palette-only `default` folder added in step 2 has no `fonts:` block (it uses LVGL's Montserrat), so that plan must exempt it there explicitly rather than by weakening the guard.
- The owner must copy `elegant` to `/themes/elegant/` on each card and select it once, because the saved slug `default` will mean the built-in look after step 2.
