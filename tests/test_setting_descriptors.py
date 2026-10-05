# tests/test_setting_descriptors.py
"""Guards for *_settings.cpp SettingDescriptor arrays: the properties a compiler cannot check.

A SettingDescriptor's StorageRef is always a pointer into settings_store.h's catalogue -- that
much the type system already enforces. What it can't check:
  * an Enum descriptor's optionLabels array is the right length for its Int's lo..hi span
    (settings_descriptor.h reads optionLabels[value - lo] with no bounds check of its own --
    the host test in radar_settings_test.cpp catches this for radar specifically; this test
    catches it for any future *_settings.cpp the same way test_settings_store.py catches a
    raw NVS key);
  * no key is declared twice within one group's own array (two rows silently fighting over
    the same setting);
  * a Slider or Enum descriptor's StorageRef actually points at an Int catalogue constant --
    nothing stops one being declared over a Bool/Float/Str, which both renderers would then
    read through the wrong union member (settings_pages.cpp's refresh_group() and main.cpp's
    handleRoot() both treat "not Toggle, not Enum" as Slider and read storage.asInt);
  * no descriptor uses Control::Text at all -- neither renderer has a branch for it, so one
    would silently fall into the Slider path and read a Str through asInt.
"""
import glob
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORE_HEADER = os.path.join("src", "platform", "storage", "settings_store.h")
STORE_DECL = re.compile(r"\b(?:Int|Bool|Float|UInt|Double|Str)\s+[A-Za-z_0-9]+\s*\{\s*\"([^\"]+)\"")

# One descriptor row's reference to a catalogue constant, e.g. `&settings::MAX_AC`.
STORAGE_REF = re.compile(r"&settings::([A-Za-z_][A-Za-z_0-9]*)")
# A descriptor row's optionLabels array, by name, e.g. `kUnitsLabels` in a row that uses it.
ENUM_ROW = re.compile(
    r"settings::Control::Enum\s*,\s*&settings::([A-Za-z_][A-Za-z_0-9]*)\s*,\s*([A-Za-z_][A-Za-z_0-9]*)"
)
# Any descriptor row's {control, catalogue constant} pair, regardless of which control it uses
# -- e.g. `settings::Control::Slider, &settings::MAX_AC` or `settings::Control::Text, &settings::TZ`.
CONTROL_ROW = re.compile(
    r"settings::Control::([A-Za-z_][A-Za-z_0-9]*)\s*,\s*&settings::([A-Za-z_][A-Za-z_0-9]*)"
)


def catalogue_kinds():
    """catalogue constant name -> its declared struct type (Int/Bool/Float/UInt/Double/Str),
    for every settings_store.h catalogue constant. Lets a Python test tell a Slider/Enum
    descriptor declared over a Bool/Float/Str apart from one correctly declared over an Int --
    something the C++ type system does NOT catch here, since StorageRef's constructor accepts
    any of the four and the descriptor's `control` field is set independently of it."""
    path = os.path.join(ROOT, STORE_HEADER)
    with open(path, encoding="utf-8") as f:
        text = f.read()
    kinds = {}
    for m in re.finditer(r"\b(Int|Bool|Float|UInt|Double|Str)\s+([A-Za-z_][A-Za-z_0-9]*)\s*\{", text):
        kinds[m.group(2)] = m.group(1)
    return kinds


def catalogue_ranges():
    """key constant name -> (lo, hi), for every Int in settings_store.h (Enum only wraps Int)."""
    path = os.path.join(ROOT, STORE_HEADER)
    with open(path, encoding="utf-8") as f:
        text = f.read()
    ranges = {}
    for m in re.finditer(r"\bInt\s+([A-Za-z_0-9]+)\s*\{\s*\"[^\"]+\"\s*,\s*[^,]+,\s*([^,]+),\s*([^}]+)\}", text):
        name, lo, hi = m.group(1), m.group(2).strip(), m.group(3).strip()
        ranges[name] = (lo, hi)
    return ranges


def settings_files():
    return sorted(glob.glob(os.path.join(ROOT, "src", "app", "*", "*_settings.cpp")))


def array_literal(text, array_name):
    """The const char* const array_name[] = { ... }; literal's element count (incl. any nullptr sentinel)."""
    m = re.search(array_name + r"\s*\[\s*\]\s*=\s*\{([^}]*)\}", text)
    if not m:
        return None
    body = m.group(1)
    return len([p for p in body.split(",") if p.strip()])


class SettingDescriptorsTest(unittest.TestCase):
    def test_every_enum_descriptor_has_a_rangesized_option_label_array(self):
        ranges = catalogue_ranges()
        for path in settings_files():
            with open(path, encoding="utf-8") as f:
                text = f.read()
            for m in ENUM_ROW.finditer(text):
                catalogue_name, labels_name = m.group(1), m.group(2)
                n = array_literal(text, labels_name)
                self.assertIsNotNone(n, f"{path}: can't find {labels_name}[] for Enum over {catalogue_name}")
                # The label arrays in this codebase end with a nullptr sentinel (see
                # radar_settings.cpp) so callers can find their length without a separate count;
                # the real label count is one less than the literal's element count.
                self.assertGreater(n, 1, f"{path}: {labels_name}[] looks empty")
                label_count = n - 1

                self.assertIn(catalogue_name, ranges,
                              f"{path}: Enum over settings::{catalogue_name}, but it is not an Int in "
                              f"{STORE_HEADER}")
                lo_text, hi_text = ranges[catalogue_name]
                # lo/hi are captured as raw text by catalogue_ranges() -- usually a literal (e.g.
                # UNITS' "0, 0, 2"), sometimes a symbolic constant (e.g. MAX_AC's
                # ADSB_MAX_AIRCRAFT). Only the literal case can be checked here without a C++
                # evaluator; a symbolic bound is skipped explicitly, with a reason, rather than
                # either silently passing (which is what this test used to do for every Enum) or
                # trying to resolve a #define from Python.
                try:
                    lo, hi = int(lo_text), int(hi_text)
                except ValueError:
                    continue
                span = hi - lo + 1
                self.assertEqual(label_count, span,
                                  f"{path}: {labels_name}[] has {label_count} label(s) but "
                                  f"settings::{catalogue_name} spans {lo}..{hi} ({span} values)")

    def test_no_key_is_registered_twice_within_one_groups_array(self):
        store_keys = {}  # catalogue constant name -> NVS key string, for messages only
        path = os.path.join(ROOT, STORE_HEADER)
        with open(path, encoding="utf-8") as f:
            store_text = f.read()
        for m in re.finditer(r"\b(?:Int|Bool|Float|UInt|Double|Str)\s+([A-Za-z_0-9]+)\s*\{\s*\"([^\"]+)\"", store_text):
            store_keys[m.group(1)] = m.group(2)

        for path in settings_files():
            with open(path, encoding="utf-8") as f:
                text = f.read()
            refs = STORAGE_REF.findall(text)
            dupes = sorted({r for r in refs if refs.count(r) > 1})
            self.assertEqual(dupes, [], f"{path}: registered twice: {[store_keys.get(d, d) for d in dupes]}")

    def test_slider_and_enum_descriptors_reference_an_int_catalogue_constant(self):
        # settings_descriptor.h's StorageRef union accepts Int/Bool/Float/Str indiscriminately,
        # and a descriptor's `control` field is a separate, unrelated piece of the initializer --
        # nothing in the type system ties them together. Both renderers (refresh_group() in
        # settings_pages.cpp, handleRoot() in main.cpp) read storage.asInt for anything that
        # is not Toggle or Enum... and Enum also reads storage.asInt->lo/hi for its option
        # range. So a Slider or Enum declared over a Bool/Float/Str would type-pun the union:
        # this test is the only thing that would catch that before it misreads memory on-device.
        kinds = catalogue_kinds()
        for path in settings_files():
            with open(path, encoding="utf-8") as f:
                text = f.read()
            for m in CONTROL_ROW.finditer(text):
                control, catalogue_name = m.group(1), m.group(2)
                if control not in ("Slider", "Enum"):
                    continue
                self.assertIn(catalogue_name, kinds,
                              f"{path}: {control} descriptor references settings::{catalogue_name}, "
                              f"which is not declared in {STORE_HEADER}")
                self.assertEqual(kinds[catalogue_name], "Int",
                                  f"{path}: {control} descriptor references settings::{catalogue_name}, "
                                  f"which is a {kinds[catalogue_name]} in {STORE_HEADER}, not an Int -- "
                                  f"both renderers read this through storage.asInt, so a {control} over "
                                  f"a non-Int type-puns the StorageRef union")

    def test_no_descriptor_uses_the_unrendered_text_control(self):
        # Control::Text exists in settings_descriptor.h's enum but neither renderer has a branch
        # for it: settings_pages.cpp's refresh_group() and main.cpp's handleRoot() both treat
        # "not Toggle, not Enum" as Slider. A Text descriptor would silently fall into that
        # Slider path and read its (likely Str-backed) storage through asInt. There is no
        # renderer for Text today, so a descriptor that uses it is a mistake, not a feature --
        # this fails loudly and explains why, rather than letting it quietly misbehave on-device.
        for path in settings_files():
            with open(path, encoding="utf-8") as f:
                text = f.read()
            for m in CONTROL_ROW.finditer(text):
                control, catalogue_name = m.group(1), m.group(2)
                if control == "Text":
                    self.fail(
                        f"{path}: a descriptor uses settings::Control::Text over settings::{catalogue_name}, "
                        f"but neither the on-device renderer (settings_pages.cpp's refresh_group()) nor the "
                        f"web renderer (main.cpp's handleRoot()) has a Text branch -- it would silently fall "
                        f"into the Slider path and read through the wrong union member. Add a Text branch "
                        f"to both renderers before using this control, or use a different control."
                    )


if __name__ == "__main__":
    unittest.main()
