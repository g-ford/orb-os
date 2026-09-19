"""Guards for src/platform/storage/settings_store.h: the properties a compiler cannot check.

The header declares every saved setting once. These tests keep it that way:
  * every key is unique and fits NVS's 15-character limit (a longer one fails silently);
  * the NVS namespace literal exists in that header and nowhere else;
  * a catalogued key is never read or written through a raw string literal elsewhere, because
    that is how a key gets spelled two ways and saved but never loaded.
"""
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join("src", "platform", "storage", "settings_store.h")
DECL = re.compile(r"\b(?:Int|Bool|Float|UInt|Double|Str)\s+[A-Za-z_0-9]+\s*\{\s*\"([^\"]+)\"")
NAMESPACE_LITERAL = '"capsuleradar"'
RAW_ACCESS = r'\.\s*(?:get|put)(?:Int|Bool|Float|UInt|Double|String|Long|ULong)\s*\(\s*"{key}"|\.\s*isKey\s*\(\s*"{key}"'


def strip_comments(src):
    """Remove // and /* */ comments, keeping string literals (and line numbers) intact."""
    out, i, n = [], 0, len(src)
    while i < n:
        two = src[i:i + 2]
        if two == "//":
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif two == "/*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j])); i = j
        elif src[i] == '"':
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1]); i = j + 1
        else:
            out.append(src[i]); i += 1
    return "".join(out)


def sources():
    for base, _, files in os.walk(os.path.join(ROOT, "src")):
        for name in sorted(files):
            if name.endswith((".cpp", ".h", ".c", ".ino")):
                path = os.path.join(base, name)
                rel = os.path.relpath(path, ROOT)
                with open(path, encoding="utf-8", errors="ignore") as f:
                    yield rel, strip_comments(f.read())


def catalogue():
    with open(os.path.join(ROOT, HEADER), encoding="utf-8") as f:
        return DECL.findall(strip_comments(f.read()))


class SettingsStoreTest(unittest.TestCase):
    def test_the_catalogue_is_not_empty(self):
        self.assertGreater(len(catalogue()), 20)

    def test_every_key_is_unique(self):
        keys = catalogue()
        dupes = sorted({k for k in keys if keys.count(k) > 1})
        self.assertEqual(dupes, [], f"declared twice: {dupes}")

    def test_every_key_fits_nvs(self):
        too_long = [k for k in catalogue() if len(k) > 15]
        self.assertEqual(too_long, [], f"NVS keys are limited to 15 characters: {too_long}")

    def test_the_namespace_literal_lives_only_in_the_header(self):
        found = []
        for rel, text in sources():
            if rel == HEADER:
                continue
            for n, line in enumerate(text.split("\n"), 1):
                if NAMESPACE_LITERAL in line:
                    found.append(f"{rel}:{n}: {line.strip()}")
        self.assertEqual(found, [], "use settings::NAMESPACE:\n" + "\n".join(found))

    def test_catalogued_keys_are_not_accessed_by_raw_literal(self):
        keys = "|".join(re.escape(k) for k in catalogue())
        pat = re.compile(RAW_ACCESS.format(key="(?:" + keys + ")"))
        found = []
        for rel, text in sources():
            if rel == HEADER:
                continue
            for n, line in enumerate(text.split("\n"), 1):
                if pat.search(line):
                    found.append(f"{rel}:{n}: {line.strip()}")
        self.assertEqual(found, [], "go through settings::Store and the catalogue:\n" + "\n".join(found))


if __name__ == "__main__":
    unittest.main()
