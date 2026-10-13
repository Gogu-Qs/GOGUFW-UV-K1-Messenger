#!/usr/bin/env bash
set -euo pipefail
APP=aprsedit
APP_NAME="APRS Edit"
APP_VER=2.1-gogu
APP_API_MIN=5
APP_VMA=${APP_VMA:-0x20000280}
CC=/opt/toolchain/bin/arm-none-eabi-gcc
OBJCOPY=/opt/toolchain/bin/arm-none-eabi-objcopy
command -v arm-none-eabi-gcc >/dev/null 2>&1 && { CC=arm-none-eabi-gcc; OBJCOPY=arm-none-eabi-objcopy; }
CFLAGS="-mcpu=cortex-m0plus -mthumb -Os -std=gnu11 -ffreestanding -fno-builtin -fno-common -fomit-frame-pointer -ffunction-sections -fdata-sections -Wall -Wextra"
LDFLAGS="-nostdlib -nostartfiles -T app.ld -Wl,--defsym,APP_VMA=${APP_VMA} -Wl,--gc-sections -Wl,-Map=${APP}.map -Wl,--build-id=none -Wl,--no-warn-rwx-segments"
rm -f ./*.app ./*.elf ./*.bin
APP_VER="$APP_VER" python3 ./gen_assets.py aprsedit_assets.bin aprsedit_assets.h
"$CC" $CFLAGS $LDFLAGS "${APP}_app.c" -lgcc -o "${APP}.elf"
"$OBJCOPY" -O binary "${APP}.elf" "${APP}.bin"
python3 ../pack_app.py "${APP}.bin" APRSEdit.app --name "$APP_NAME" --ver "$APP_VER" \
  --api-min "$APP_API_MIN" --vma "$APP_VMA" --shortcut none --assets aprsedit_assets.bin
BYTES=$(wc -c < "${APP}.bin")
printf 'APRSEdit.app: %d B (%d%% of 4 KiB)\n' "$BYTES" "$((BYTES * 100 / 4096))"
