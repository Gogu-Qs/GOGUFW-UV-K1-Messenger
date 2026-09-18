#!/usr/bin/env python3
"""Host-only sanitized tests; firmware is always built with ARM GCC/CMake."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
os.chdir(ROOT)
subprocess.run(['python3', 'tools/pack_big_font.py'], check=True)
cc = os.environ.get('HOST_CC', 'clang')
flags = ['-std=c11', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
includes = ['App', 'App/usb', 'Middlewares/CherryUSB/core', 'Middlewares/CherryUSB/port',
            'Middlewares/CherryUSB/common', 'Middlewares/CherryUSB/class/cdc',
            'Drivers/CMSIS/Include', 'Drivers/CMSIS/Device/PY32F071/Include']
with tempfile.TemporaryDirectory(prefix='gogufw-memory-tests-') as tmp:
    tmp = Path(tmp)
    def run(name, sources, extra=()):
        output = tmp / name
        subprocess.run([cc, *flags, *['-I' + p for p in includes], '-I' + str(ROOT),
                        *extra, *map(str, sources), '-o', str(output)], check=True)
        subprocess.run([str(output)], check=True)
    run('usb', ['tools/tests/usb_memory_test.c'],
        ['-DPY32F071x8', '-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast'])
    run('messenger', ['tools/tests/memory_smoke.c', 'App/app/messenger_packet.c'],
        ['-Wno-unknown-attributes'])  # Clang does not implement GCC's size-tuning optimize attribute.
    # Compile the production renderer verbatim; only replace its framebuffer backing.
    source = (ROOT / 'App/ui/helper.c').read_text()
    start = source.index('void UI_PrintString(')
    end = source.index('\n}\n', start) + 2
    render = source[start:end]
    harness = '''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "App/font.c"
#include "tools/tests/fixtures/font_big_legacy.h"
static uint8_t gFrameBuffer[7][128], referenceFrame[7][128];
#include "tools/tests/fixtures/font_render_legacy.h"
''' + render + '''
static void compare(const char *s, uint8_t start, uint8_t end, uint8_t line, uint8_t width) {
    memset(gFrameBuffer, 0xa5, sizeof(gFrameBuffer));
    memset(referenceFrame, 0xa5, sizeof(referenceFrame));
    UI_PrintString(s, start, end, line, width);
    Reference_PrintString(s, start, end, line, width);
    assert(!memcmp(gFrameBuffer, referenceFrame, sizeof(gFrameBuffer)));
}
int main(void) {
    for (unsigned g = 0; g < 94; g++)
        for (unsigned x = 0; x < 7; x++) {
            uint8_t code = gFontBigColumnIndex[g][x];
            assert(gFontBigColumns[code][0] == legacyFontBig[g][x]);
            assert(gFontBigColumns[code][1] == legacyFontBig[g][x+7]);
        }
    for (unsigned c = 1; c <= 127; c++) {
        char s[] = {(char)c, 0};
        for (uint8_t line = 0; line < 6; line++) {
            compare(s, 0, 0, line, 8);
            compare(s, 0, 127, line, 8);
            compare(s, 121, 0, line, 8);
        }
    }
    const char *samples[] = {"", " ", "GOGUFW v2.0.3", "145.50000", "g j ~ ! 09", "CH-1024", "MESSENGER"};
    for (unsigned i = 0; i < sizeof(samples)/sizeof(samples[0]); i++)
        for (uint8_t line = 0; line < 6; line++) {
            compare(samples[i], 0, 127, line, 8);
            compare(samples[i], 1, 0, line, 8);
        }
    puts("Font: all 1316 bytes and 2370 complete framebuffer comparisons PASS");
}
'''
    file = tmp / 'font.c'
    file.write_text(harness)
    run('font', [file])
print('All host memory regression tests passed (ASan + UBSan).')
