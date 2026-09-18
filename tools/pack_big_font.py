#!/usr/bin/env python3
"""Regenerate the lossless font column dictionary from the golden glyphs.
Change the glyphs in tools/tests/fixtures/font_big_legacy.h, run --write,
then run tools/tests/run_memory_tests.py. No character or pixel is dropped.
"""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--write', action='store_true')
args = parser.parse_args()
reference = (ROOT / 'tools/tests/fixtures/font_big_legacy.h').read_text().split('= {', 1)[1]
values = [int(n, 16) for n in re.findall(r'0x[0-9a-fA-F]+', reference)]
assert len(values) == 94 * 14
columns = [(values[g * 14 + x], values[g * 14 + x + 7]) for g in range(94) for x in range(7)]
dictionary = list(dict.fromkeys(columns))
assert len(dictionary) <= 256
indices = [dictionary.index(c) for c in columns]
assert [dictionary[i] for i in indices] == columns
encoded = 'const uint8_t gFontBigColumnIndex[94][7] = {\n'
encoded += ''.join('    {' + ','.join(map(str, indices[g*7:g*7+7])) + '},\n' for g in range(94))
encoded += '};\nconst uint8_t gFontBigColumns[' + str(len(dictionary)) + '][2] = {\n'
encoded += ''.join('    {%d,%d},\n' % pair for pair in dictionary) + '};'
path = ROOT / 'App/font.c'
source = path.read_text()
start = source.index('const uint8_t gFontBigColumnIndex[')
end = source.index('\n};', source.index('const uint8_t gFontBigColumns[', start)) + 3
if args.write:
    path.write_text(source[:start] + encoded + source[end:])
    header = ROOT / 'App/font.h'
    header.write_text(re.sub(r'gFontBigColumns\[\d+\]', f'gFontBigColumns[{len(dictionary)}]', header.read_text()))
else:
    assert source[start:end] == encoded, 'Font tables differ: run tools/pack_big_font.py --write'
print(f'Font dictionary: all 1316 golden bytes preserved; {len(dictionary)} columns, {len(indices)+2*len(dictionary)} data bytes PASS')
