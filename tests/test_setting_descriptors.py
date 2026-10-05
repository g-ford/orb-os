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
    the same setting).
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


if __name__ == "__main__":
    unittest.main()
