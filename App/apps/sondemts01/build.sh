#!/usr/bin/env bash
set -euo pipefail

APP="$(basename "$PWD")"
APP_NAME="Sonde MTS01"
APP_VER="1.2-gogu"
APP_API_MIN=3
APP_VMA=${APP_VMA:-0x20000280}
OUT="${APP_NAME// /}"

CC=/opt/toolchain/bin/arm-none-eabi-gcc
OBJCOPY=/opt/toolchain/bin/arm-none-eabi-objcopy
command -v arm-none-eabi-gcc >/dev/null 2>&1 && { CC=arm-none-eabi-gcc; OBJCOPY=arm-none-eabi-objcopy; }

CFLAGS="-mcpu=cortex-m0plus -mthumb -Os -fno-jump-tables -std=gnu11 -ffreestanding -fno-builtin -fno-common -fomit-frame-pointer -ffunction-sections -fdata-sections -Wall -Wextra"
LDFLAGS="-nostdlib -nostartfiles -T app.ld -Wl,--defsym,APP_VMA=${APP_VMA} -Wl,--gc-sections -Wl,-Map=${APP}.map -Wl,--build-id=none -Wl,--no-warn-rwx-segments"

rm -f ./*.app ./*.elf ./*.bin
"$CC" $CFLAGS $LDFLAGS "${APP}_app.c" -lgcc -o "${APP}.elf"
"$OBJCOPY" -O binary "${APP}.elf" "${APP}.bin"
python3 ../pack_app.py "${APP}.bin" "${OUT}.app" \
  --name "$APP_NAME" --ver "$APP_VER" --api-min "$APP_API_MIN" \
  --vma "$APP_VMA" --shortcut none --require gogu >/dev/null

BYTES=$(wc -c < "${APP}.bin")
if [ "$BYTES" -gt 4096 ]; then
  echo "Sonde MTS01 overflows 4 KiB ($BYTES B)"; exit 1
fi
printf 'Sonde MTS01: %d B -> %s.app\n' "$BYTES" "$OUT"
