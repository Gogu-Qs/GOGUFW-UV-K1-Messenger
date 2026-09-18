#!/usr/bin/env python3
"""Report real FLASH use including the RAM overlay load image, and check layout.
Example: python3 tools/check_memory_build.py build/Fusion --baseline BASELINE_DIR
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('build', type=Path)
p.add_argument('--baseline', type=Path)
a = p.parse_args()

def measure(directory):
    elf = directory / 'gogufw.elf'
    symbols = {}
    for line in subprocess.check_output(['arm-none-eabi-nm', '-n', str(elf)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    flash = symbols['_eflash_used'] - symbols['g_pfnVectors']
    static_ram = symbols['_ebss'] - symbols['_sdata']
    reserved = ((static_ram + 7) // 8) * 8 + symbols['_Min_Heap_Size'] + symbols['_Min_Stack_Size']
    assert flash == (directory / 'gogufw.bin').stat().st_size
    assert 0 < flash <= 118 * 1024 and reserved <= 16 * 1024
    assert symbols['__mb_workspace_start'] == symbols['__mb_ramfunc_start'] == 0x20000280
    assert symbols['__mb_workspace_size'] == 4096
    return {'flash_bytes': flash, 'free_flash_bytes': 118*1024-flash,
            'static_ram_span_bytes': static_ram, 'ram_with_stack_reserve_bytes': reserved,
            'free_ram_after_reserve_bytes': 16*1024-reserved,
            'ram_stub_bytes': symbols['__mb_ramfunc_size']}

result = measure(a.build)
headers = subprocess.check_output(['arm-none-eabi-objdump', '-h', str(a.build/'gogufw.elf')], text=True)
section = re.search(r'^\s*\d+ \.noncacheable\s+[^\n]+\n([^\n]+)', headers, re.M)
assert section and 'LOAD' not in section[1], 'USB NOLOAD section unexpectedly has a FLASH image'
result['usb_noload'] = True
if a.baseline:
    baseline = measure(a.baseline)
    result['saved_flash_bytes'] = baseline['flash_bytes'] - result['flash_bytes']
    result['saved_ram_bytes'] = baseline['ram_with_stack_reserve_bytes'] - result['ram_with_stack_reserve_bytes']
    with tempfile.TemporaryDirectory() as td:
        blobs = []
        for i, directory in enumerate([a.baseline, a.build]):
            blob = Path(td)/f'{i}.bin'
            subprocess.run(['arm-none-eabi-objcopy', '--dump-section', f'.mb_ramfunc={blob}',
                            str(directory/'gogufw.elf'), str(Path(td)/f'{i}.elf')], check=True)
            blobs.append(blob.read_bytes())
        assert blobs[0] == blobs[1], 'RAM restore code changed: review before hardware testing'
        result['ram_stub_identical_to_baseline'] = True
print(json.dumps(result, indent=2))
