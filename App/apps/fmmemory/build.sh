#!/usr/bin/env bash
set -euo pipefail

APP="$(basename "$PWD")"
APP_NAME="FM Memory"
APP_VER="3.2.0-gogu"
APP_API_MIN=9
APP_VMA=${APP_VMA:-0x20000280}
OUT="${APP_NAME// /}"

CC=/opt/toolchain/bin/arm-none-eabi-gcc
OBJCOPY=/opt/toolchain/bin/arm-none-eabi-objcopy
command -v arm-none-eabi-gcc >/dev/null 2>&1 && { CC=arm-none-eabi-gcc; OBJCOPY=arm-none-eabi-objcopy; }

CFLAGS="-mcpu=cortex-m0plus -mthumb -Oz -std=gnu11 -ffreestanding -fno-builtin -fno-common \
  -fomit-frame-pointer -ffunction-sections -fdata-sections -Wall -Wextra"
LDFLAGS="-nostdlib -nostartfiles -T ../fm/app.ld -Wl,--defsym,APP_VMA=${APP_VMA} \
  -Wl,--gc-sections -Wl,-Map=${APP}.map -Wl,--build-id=none -Wl,--no-warn-rwx-segments"

rm -f ./*.app ./*.elf ./*.bin
python3 ../fm/gen_assets.py "${APP}_assets.bin" ../fm/fm_assets.h
"$CC" $CFLAGS $LDFLAGS "${APP}_app.c" -lgcc -o "${APP}.elf"
"$OBJCOPY" -O binary "${APP}.elf" "${APP}.bin"
python3 ../pack_app.py "${APP}.bin" "${OUT}.app" \
  --name "$APP_NAME" --ver "$APP_VER" --api-min "$APP_API_MIN" --vma "$APP_VMA" \
  --shortcut fm-memory --require fm --screensaver --assets "${APP}_assets.bin" >/dev/null

BYTES=$(wc -c < "${APP}.bin")
if [ "$BYTES" -gt 4096 ]; then
  printf 'FM Memory overflows 4 KiB (%d B)\n' "$BYTES"; exit 1
fi
printf 'FM Memory: %d B -> %s.app\n' "$BYTES" "$OUT"
