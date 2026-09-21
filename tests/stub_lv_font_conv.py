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
