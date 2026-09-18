/* Copyright 2026 Armel F4HWN
 * https://github.com/armel
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include <stddef.h>
#include <string.h>

#include "driver/mb_flash.h"
#include "driver/mb_ram.h"
#include "driver/py25q16.h"

#include "py32f0xx.h"

/* Compile-time proof of the external-flash layout used by GOGUFW. The final
 * marker sector ends well before the stock voice tables at 0x14C000. */
_Static_assert(MB_SLOT0_EXT_BASE + MB_SLOT_COUNT * MB_SLOT_STRIDE ==
               MB_BANK1_EXT_BASE,
               "multiboot slots overlap config banks");
_Static_assert(MB_BANK1_EXT_BASE + (MB_BANK_COUNT - 1u) * MB_BANK_SIZE ==
               MB_STATE_A_BASE,
               "multiboot config banks overlap state marker");
_Static_assert(MB_STATE_B_BASE + 0x1000u <= 0x0014C000u,
               "multiboot state marker overlaps stock voice data");

/* Internal-flash program/erase keys (FLASH_KEY1 / FLASH_KEY2). */
#define MB_FLASH_KEY1   0x45670123u
#define MB_FLASH_KEY2   0xCDEF89ABu

/* Internal flash granularity (PY32F071xB): program & page-erase = 256 bytes. */
#define MB_FLASH_PAGE   256u

/* External SPI flash chip-select is on PA3 (see driver/py25q16.c). */
#define MB_CS_PIN       (1u << 3)

/* LCD control pins used only for RAM-resident progress updates. */
#define MB_LCD_CS_PIN   (1u << 2)  /* PB2 */
#define MB_LCD_A0_PIN   (1u << 6)  /* PA6 */

/* Rounded progress gauge geometry, matching ScanProgress_DrawGaugeLine(). */
#define MB_PROGRESS_COLS        118u
#define MB_PROGRESS_FIRST_COL     5u
#define MB_PROGRESS_FILLED      0x2Du

/* Number of erase/program retries per page before giving up (and resetting
 * anyway - the region is already erased, so USB recovery is the only option). */
#define MB_PAGE_RETRIES 3u

/* Bounded waits used by the RAM-only copier. A timeout forces an immediate
 * reset instead of hanging forever with IRQs disabled. */
#define MB_RAM_SPI_TIMEOUT    100000u
#define MB_RAM_FLASH_TIMEOUT 10000000u

/* -------------------------------------------------------------------------- */
/* RAM-resident copier.                                                       */
/*                                                                            */
/* This runs while the internal application flash is being erased/programmed, */
/* during which the flash bus is unavailable. It must therefore NOT fetch any */
/* code from flash nor read any flash data: it uses raw register access only  */
/* (no external calls), reads the source from the external SPI flash in       */
/* polled mode, and resets the MCU when done. Normally it is placed in         */
/* .RamFunc, which startup copies to RAM alongside .data. With the overlay     */
/* enabled it is linked in .MBRamFunc and copied over the PY25Q16 sector cache */
/* only immediately before use.                                               */
/* -------------------------------------------------------------------------- */


/* Only the restore copier and its helpers live here. Keep this translation
 * unit non-LTO: no instruction/data fetch may leave RAM during FLASH erase.
 * Normal preparation and slot logic live in mb_flash_logic.c with LTO. */
#ifdef ENABLE_FEAT_F4HWN_MULTIBOOT_OVERLAY
    #define MB_RAM_SECTION ".MBRamFunc"
#else
    #define MB_RAM_SECTION ".RamFunc"
#endif
#define MB_RAM_HELPER __attribute__((section(MB_RAM_SECTION), noinline, noclone, used))

/* Polled single-byte SPI2 transfer. The result is returned through out so
 * timeout and received 0xFF remain distinguishable. */
MB_RAM_HELPER static bool mb_ram_spi(uint8_t v, uint8_t *out)
{
    uint32_t timeout = MB_RAM_SPI_TIMEOUT;
    while (!(SPI2->SR & SPI_SR_TXE))
        if (!--timeout)
            return false;

    *(volatile uint8_t *)&SPI2->DR = v;

    timeout = MB_RAM_SPI_TIMEOUT;
    while (!(SPI2->SR & SPI_SR_RXNE))
        if (!--timeout)
            return false;

    *out = *(volatile uint8_t *)&SPI2->DR;
    return true;
}

MB_RAM_HELPER static bool mb_ram_flash_idle(void)
{
    uint32_t timeout = MB_RAM_FLASH_TIMEOUT;
    while (FLASH->SR & FLASH_SR_BSY)
        if (!--timeout)
            return false;
    return true;
}

MB_RAM_HELPER __attribute__((noreturn)) static void mb_ram_reset(void)
{
    __DSB();

    SCB->AIRCR = (0x5FAu << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
    __DSB();
    for (;;) { }
}

/* Minimal SPI1 LCD writer. A display timeout merely disables progress updates:
 * it must never abort or delay the safety-critical flash copy. */
MB_RAM_HELPER static bool mb_ram_lcd_spi(uint8_t v)
{
    uint32_t timeout = MB_RAM_SPI_TIMEOUT;
    while (!(SPI1->SR & SPI_SR_TXE))
        if (!--timeout)
            return false;
    *(volatile uint8_t *)&SPI1->DR = v;

    timeout = MB_RAM_SPI_TIMEOUT;
    while (!(SPI1->SR & SPI_SR_RXNE))
        if (!--timeout)
            return false;
    (void)*(volatile uint8_t *)&SPI1->DR;
    return true;
}

__attribute__((always_inline)) static inline bool mb_ram_progress_blit(const uint8_t *line)
{
    uint32_t ok = 1u;
    GPIOB->BRR = MB_LCD_CS_PIN;
    GPIOA->BRR = MB_LCD_A0_PIN;       /* command */

    if (!mb_ram_lcd_spi(0xB7u) ||     /* LCD page 7 */
        !mb_ram_lcd_spi(0x10u) ||     /* column high nibble */
        !mb_ram_lcd_spi(0x04u))       /* visible RAM starts at column 4 */
        ok = 0u;

    GPIOA->BSRR = MB_LCD_A0_PIN;      /* data */
    if (ok)
        for (uint32_t i = 0; i < 128u; i++)
            if (!mb_ram_lcd_spi(line[i]))
            {
                ok = 0u;
                break;
            }
    GPIOB->BSRR = MB_LCD_CS_PIN;
    return ok != 0u;
}

/* Blank the whole LCD RAM (all 8 pages) right before the reset. The MCU reset
 * leaves the display controller powered and still showing the "Restore slot N"
 * screen; it stays visible through the next boot until ST7565_Init re-inits the
 * panel. Wiping it here means the reboot window shows nothing instead of a
 * stale restore screen. Best-effort: a display timeout just leaves it as-is. */
__attribute__((always_inline)) static inline void mb_ram_lcd_clear(void)
{
    for (uint8_t page = 0; page < 8u; page++)
    {
        GPIOB->BRR = MB_LCD_CS_PIN;
        GPIOA->BRR = MB_LCD_A0_PIN;             /* command */
        if (!mb_ram_lcd_spi((uint8_t)(0xB0u | page)) ||  /* set page 0..7 */
            !mb_ram_lcd_spi(0x10u) ||           /* column high nibble */
            !mb_ram_lcd_spi(0x04u))             /* visible RAM starts at column 4 */
        {
            GPIOB->BSRR = MB_LCD_CS_PIN;
            return;
        }
        GPIOA->BSRR = MB_LCD_A0_PIN;            /* data */
        for (uint32_t i = 0; i < 128u; i++)
            if (!mb_ram_lcd_spi(0x00u))
            {
                GPIOB->BSRR = MB_LCD_CS_PIN;
                return;
            }
        GPIOB->BSRR = MB_LCD_CS_PIN;
    }
}
__attribute__((section(MB_RAM_SECTION), noinline, used))
void MB_RamReflash(uint32_t intAddr, uint32_t extAddr, uint32_t imageSize,
                          uint8_t *progressLine)
{
    /* 4-byte aligned so the 64-word page program can read it as uint32_t
     * (Cortex-M0+ cannot do unaligned word accesses). */
    uint8_t buf[MB_FLASH_PAGE] __attribute__((aligned(4)));
    uint32_t remaining = imageSize;
    uint32_t regionRemaining = MB_INT_APP_SIZE;
    uint32_t pagesDone = 0;
    uint32_t progressAccumulator = 0;
    uint32_t progressFilled = 0;
    uint32_t lcdEnabled = progressLine != NULL;


    __disable_irq();

    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = MB_FLASH_KEY1;
        FLASH->KEYR = MB_FLASH_KEY2;
    }

    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_OPTVERR;

    /* Rebuild the complete application region. Bytes past imageSize are never
     * read from the external slot: they are forced to erased 0xFF, preventing
     * unvalidated padding or remnants of an older, longer firmware. */
    while (regionRemaining >= MB_FLASH_PAGE)
    {
        uint32_t readSize = remaining < MB_FLASH_PAGE ? remaining : MB_FLASH_PAGE;
        uint32_t needProgram = 0;
        uint32_t success = 0;
        uint8_t ignored;

        /* Volatile stores prevent GCC from replacing this loop with a call
         * to flash-resident memset while the application flash is unavailable. */
        volatile uint8_t *fill = buf;
        for (uint32_t i = 0; i < MB_FLASH_PAGE; i++)
            fill[i] = 0xFFu;

        if (readSize)
        {
            /* Read only CRC-validated image bytes. The rest of the page stays
             * 0xFF when imageSize is not page-aligned. */
            GPIOA->BRR = MB_CS_PIN;             /* CS low */
            if (!mb_ram_spi(0x03u, &ignored) ||
                !mb_ram_spi((extAddr >> 16) & 0xFFu, &ignored) ||
                !mb_ram_spi((extAddr >> 8) & 0xFFu, &ignored) ||
                !mb_ram_spi(extAddr & 0xFFu, &ignored))
                goto fatal_reset;

            for (uint32_t i = 0; i < readSize; i++)
                if (!mb_ram_spi(0xFFu, &buf[i]))
                    goto fatal_reset;

            GPIOA->BSRR = MB_CS_PIN;            /* CS high */
        }

        for (uint32_t i = 0; i < MB_FLASH_PAGE; i++)
        {
            if (buf[i] != 0xFFu)
            {
                needProgram = 1u;
                break;
            }
        }

        for (uint32_t retry = 0; retry < MB_PAGE_RETRIES; retry++)
        {
            const uint32_t   *src = (const uint32_t *)(const void *)buf;
            volatile uint32_t *dst = (volatile uint32_t *)intAddr;

            uint32_t          i;
            uint32_t          ok = 1u;

            /* --- page erase (256 bytes) --- */
            if (!mb_ram_flash_idle())
                goto fatal_reset;
            FLASH->CR |= FLASH_CR_PER;
            *(volatile uint32_t *)intAddr = 0xFFFFFFFFu;
            if (!mb_ram_flash_idle())
                goto fatal_reset;
            FLASH->CR &= ~FLASH_CR_PER;
            FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_OPTVERR;

            if (needProgram)
            {
                /* Page program: 64 words, PGSTRT before the last word. */
                FLASH->CR |= FLASH_CR_PG;
                for (i = 0; i < 64u; i++)
                {
                    dst[i] = src[i];
                    if (i == 62u)
                        FLASH->CR |= FLASH_CR_PGSTRT;
                }
                if (!mb_ram_flash_idle())
                    goto fatal_reset;
                FLASH->CR &= ~FLASH_CR_PG;
                FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_OPTVERR;
            }

            /* --- verify (read-back compare) --- */
            for (i = 0; i < 64u; i++)
            {
                if (dst[i] != src[i])
                {
                    ok = 0u;
                    break;
                }
            }
            if (ok)
            {
                success = 1u;
                break;
            }
        }

        /* Never silently continue after an unprogrammable page. Returning to
         * flash-resident code is unsafe once the application has been erased. */
        if (!success)
            goto fatal_reset;

        /* Advance the gauge without division (which could call a helper
         * in erased flash). Refresh once per 8 KiB internal sector. */
        if (lcdEnabled)
        {
            pagesDone++;
            progressAccumulator += MB_PROGRESS_COLS;
            while (progressAccumulator >= (MB_INT_APP_SIZE / MB_FLASH_PAGE))
            {
                progressAccumulator -= (MB_INT_APP_SIZE / MB_FLASH_PAGE);
                if (progressFilled < MB_PROGRESS_COLS)
                {
                    progressLine[MB_PROGRESS_FIRST_COL + progressFilled] = MB_PROGRESS_FILLED;
                    progressFilled++;
                }
            }
            if ((pagesDone & 31u) == 0u || regionRemaining == MB_FLASH_PAGE)
                lcdEnabled = mb_ram_progress_blit(progressLine);
        }

        intAddr += MB_FLASH_PAGE;
        extAddr += readSize;
        remaining -= readSize;
        regionRemaining -= MB_FLASH_PAGE;
    }

    FLASH->CR |= FLASH_CR_LOCK;
    if (lcdEnabled)
        mb_ram_lcd_clear();
    mb_ram_reset();

fatal_reset:
    /* Release the external flash and reset immediately. If failure happened
     * after an erase, the factory USB/DFU bootloader remains the recovery path. */
    GPIOA->BSRR = MB_CS_PIN;
    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_PG | FLASH_CR_PGSTRT);
    FLASH->CR |= FLASH_CR_LOCK;
    mb_ram_reset();
}
