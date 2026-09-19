import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools"))
import check_sd_guard as g


class ScannerTest(unittest.TestCase):
    def flagged(self, src):
        return [n for n, _ in g.scan_source(src)]

    def test_a_bare_call_is_flagged(self):
        self.assertEqual(self.flagged("void f() {\n    File x = SD.open(p);\n}\n"), [2])

    def test_a_guarded_call_passes(self):
        self.assertEqual(self.flagged("void f() {\n    sdcard::Guard guard;\n    SD.open(p);\n}\n"), [])

    def test_a_guard_declared_after_the_call_does_not_count(self):
        self.assertEqual(self.flagged("void f() {\n    SD.exists(p);\n    sdcard::Guard guard;\n}\n"), [1 + 1])

    def test_a_guard_in_a_scope_that_has_closed_does_not_cover_a_later_call(self):
        src = "void f() {\n    {\n        sdcard::Guard guard;\n        SD.exists(a);\n    }\n    SD.exists(b);\n}\n"
        self.assertEqual(self.flagged(src), [6])

    def test_a_guard_in_an_enclosing_scope_covers_nested_blocks(self):
        src = "void f() {\n    sdcard::Guard guard;\n    for (;;) {\n        if (x) { SD.remove(p); }\n    }\n}\n"
        self.assertEqual(self.flagged(src), [])

    def test_a_guard_in_an_if_initialiser_covers_the_condition(self):
        self.assertEqual(self.flagged("void f() {\n    if (sdcard::Guard guard; SD.exists(p))\n        log();\n}\n"), [])

    def test_a_guard_in_the_previous_function_does_not_leak_into_the_next(self):
        src = "void a() {\n    sdcard::Guard guard;\n}\nvoid b() {\n    SD.mkdir(p);\n}\n"
        self.assertEqual(self.flagged(src), [5])

    def test_manual_lock_counts(self):
        self.assertEqual(self.flagged("void f() {\n    sdcard::lock();\n    SD.open(p);\n    sdcard::unlock();\n}\n"), [])

    def test_calls_in_comments_and_strings_are_ignored(self):
        src = 'void f() {\n    // SD.open(p)\n    /* SD.remove(p) */\n    log("SD.exists(p)");\n}\n'
        self.assertEqual(self.flagged(src), [])

    def test_a_raw_string_with_braces_does_not_upset_scope_tracking(self):
        src = 'const char *P = R"HTML(<script>function(){ SD.open(x) }</script>)HTML";\nvoid f() {\n    SD.open(p);\n}\n'
        self.assertEqual(self.flagged(src), [3])

    def test_a_waiver_on_the_line_is_honoured(self):
        self.assertEqual(self.flagged("void f() {\n    SD.begin(cs);   // sd-guard-ok: single-threaded, before any task\n}\n"), [])


class RepoTest(unittest.TestCase):
    def test_every_sd_call_in_the_firmware_is_guarded(self):
        found = []
        for base, _, files in os.walk(os.path.join(g.ROOT, "src")):
            for name in files:
                if name.endswith((".cpp", ".h", ".c")):
                    path = os.path.join(base, name)
                    with open(path, encoding="utf-8", errors="ignore") as f:
                        found += [f"{os.path.relpath(path, g.ROOT)}:{n}: {t}" for n, t in g.scan_source(f.read())]
        self.assertEqual(found, [], "\n".join(found))


if __name__ == "__main__":
    unittest.main()
