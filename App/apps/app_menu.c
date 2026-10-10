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

#include "apps/app_overlay.h"

#ifdef ENABLE_FEAT_F4HWN_OVERLAY_APPS

#include <string.h>
#include "apps/app_menu.h"
#include "app/app.h"
#include "app/uart.h"
#include "driver/backlight.h"
#include "driver/st7565.h"
#include "driver/keyboard.h"
#include "driver/system.h"
#include "driver/gpio.h"
#include "ui/helper.h"
#include "ui/status.h"
#ifdef ENABLE_FEAT_F4HWN_K5VIEWER
#include "k5viewer.h"
#endif

/* Standard GOGUFW chrome: title + position counter, dotted separators and
 * four content rows between them. */
static void app_status_bar(uint8_t selected, uint8_t total)
{
    char counter[6];
    const uint8_t n = total ? (uint8_t)(selected + 1u) : 0u;
    counter[0] = (char)('0' + n / 10u);
    counter[1] = (char)('0' + n % 10u);
    counter[2] = '/';
    counter[3] = (char)('0' + total / 10u);
    counter[4] = (char)('0' + total % 10u);
    counter[5] = '\0';
    UI_DisplayStatus();
    UI_GOGU_DrawHeader("GOGUFW APPS", counter);
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);
}

/* Fixed selection capsule around the primary information (the app name).
 * Slot number stays in the normal font; size is plain 3x5 metadata. */
#define APP_NAME_BOX_START 19u
#define APP_NAME_BOX_END   102u
#define APP_NAME_TEXT_X    21u

static void app_wait_release(void);

static void app_invert_name(uint8_t y)
{
    UI_GOGU_InvertArea(APP_NAME_BOX_START, APP_NAME_BOX_END,
                       y > 0u ? (uint8_t)(y - 1u) : y, 9u);
}

/* Debounced blocking key read, then wait for release (mirrors mb_get_key). */
static KEY_Code_t app_get_key(void)
{
#if defined(ENABLE_UART) || defined(ENABLE_USB)
    const uint8_t slot_revision = APP_SlotRevision();
#endif

    for (;;)
    {
#ifdef ENABLE_FEAT_F4HWN_K5VIEWER
        /* APP_MenuOpen() is modal and does not return to APP_Update(). Keep
         * serial key injection and the viewer connection alive while waiting. */
        K5VIEWER_ParseInput();
#endif
#if defined(ENABLE_UART) || defined(ENABLE_USB)
        UART_ServiceCommands();
        if (APP_SlotRevision() != slot_revision)
            return KEY_INVALID;
#endif
        APP_ModalBacklightTick(true);
        APP_ModalRadioService();

        if (APP_IsScreenSaverDisplayed())
        {
            if (KEYBOARD_GetKey() != KEY_INVALID)
            {
                APP_ModalScreenSaverExit();
                BACKLIGHT_TurnOn();
                app_wait_release();
                return KEY_INVALID;
            }

            SYSTEM_DelayMs(10);
            continue;
        }

#ifdef ENABLE_FEAT_F4HWN_K5VIEWER
        K5VIEWER_Update(false);
#endif
        KEY_Code_t key = KEYBOARD_Poll();
        if (key != KEY_INVALID)
        {
            SYSTEM_DelayMs(30);
            if (KEYBOARD_Poll() == key)
            {
                BACKLIGHT_TurnOn();
                while (KEYBOARD_Poll() != KEY_INVALID)
                {
                    SYSTEM_DelayMs(10);
                    APP_ModalBacklightTick(false);
                    APP_ModalRadioService();
                }
                return key;
            }
        }
        SYSTEM_DelayMs(10);
    }
}

static void app_wait_release(void)
{
    uint8_t stable = 0;
    while (stable < 10u)
    {
        if (!GPIO_IsPttPressed() && KEYBOARD_Poll() == KEY_INVALID)
            stable++;
        else
            stable = 0;
        SYSTEM_DelayMs(10);
        APP_ModalBacklightTick(true);
        APP_ModalRadioService();
    }
}

static void app_copy(char *dst, uint8_t cap, const char *src, uint8_t src_cap)
{
    uint8_t n = 0;
    while (n + 1u < cap && n < src_cap && src[n])
    {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

/* A launch failed: name the app and keep the compact standard chrome. */
static void app_show_error(const app_header_t *header, uint8_t rc)
{
    (void)rc;
    char nm[19];
    app_copy(nm, sizeof(nm), header->name, APP_NAME_LEN);

    UI_DisplayClear();
    UI_StatusClear();
    GUI_DisplaySmallestInverse("APP ERROR", 46, 0, true, true, 82);
    UI_PrintStringSmallNormal(nm, 2, 0, 2);                 /* which app */
    UI_PrintStringSmallNormal("CHECK APP/FIRMWARE", 2, 0, 4);
    UI_PrintStringSmallNormal("Press any key", 2, 0, 6);
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();

    app_get_key();   /* blocking: dismiss on any key */
}

/* Four visible slots between the standard top and bottom separators. */
#define APP_MENU_ROWS 4u
#define APP_MENU_SLOT_COUNT APP_SLOT_COUNT

_Static_assert(APP_MENU_SLOT_COUNT <= APP_SLOT_COUNT,
               "APP_MENU_SLOT_COUNT exceeds the physical app slot count");

static uint8_t app_scan_slots(uint8_t slots[APP_MENU_SLOT_COUNT])
{
    uint8_t count = 0u;
    for (uint8_t slot = 0; slot < APP_MENU_SLOT_COUNT; slot++)
    {
        app_header_t hdr;
        if (APP_SlotInfo(slot, &hdr) == APP_OK &&
            (hdr.flags & APP_FLAG_COMMITTED))
            slots[count++] = slot;
    }
    return count;
}

void APP_MenuOpen(void)
{
    APP_ModalScreenSaverExit();
    BACKLIGHT_TurnOn();

#ifdef ENABLE_FEAT_F4HWN_K5VIEWER
    /* Detach the modal selector from the key state that triggered F+7. The
     * normal K5Viewer updater suppresses frames while a key is held; without
     * clearing this stale state, the selector could never publish its first
     * frame to the viewer. */
    gKeyReading0 = KEY_INVALID;
    gKeyReading1 = KEY_INVALID;
#endif

    /* Apps are installed from UV Studio (0x073x) into physical slots 0..N-1.
     * The compact selector lists committed slots only. */
    uint8_t slot_revision = APP_SlotRevision();
    uint8_t slots[APP_MENU_SLOT_COUNT];
    uint8_t count = app_scan_slots(slots);

    /* Remember the physical slot and scrolling window across menu openings. */
    static uint8_t sel = 0;
    static uint8_t top = 0;             /* first visible row of the scrolling window */
    if (sel >= count || top >= count)
        sel = top = 0u;
    app_wait_release();

    for (;;)
    {
        const uint8_t current_revision = APP_SlotRevision();
        if (current_revision != slot_revision)
        {
            count = app_scan_slots(slots);
            if (sel >= count || top >= count)
                sel = top = 0u;
            slot_revision = current_revision;
        }

        UI_DisplayClear();
        app_status_bar(sel, count);

        if (count == 0u) {
            UI_GOGU_PrintSmallAtY("NO APPS", 39u, 25u, false);
            UI_GOGU_DrawFooter("SELECT", NULL, "EXIT");
            ST7565_BlitStatusLine();
            ST7565_BlitFullScreen();
            if (app_get_key() == KEY_EXIT)
                return;
            continue;
        }

        /* Slide [top, top+APP_MENU_ROWS) so it always contains the selection. */
        if (sel < top)
            top = sel;
        else if (sel >= (uint8_t)(top + APP_MENU_ROWS))
            top = (uint8_t)(sel - APP_MENU_ROWS + 1u);

        for (uint8_t index = top;
             index < count && (uint8_t)(index - top) < APP_MENU_ROWS;
             index++)
        {
            char number[3];
            char name[14];
            const uint8_t slot = slots[index];
            const uint8_t visible_number = (uint8_t)(slot + 1u);
            const uint8_t y = UI_GOGU_CONTENT_ROW_Y((uint8_t)(index - top));

            number[0] = (char)('0' + visible_number / 10u);
            number[1] = (char)('0' + visible_number % 10u);
            number[2] = '\0';

            UI_GOGU_PrintSmallAtY(number, 2u, y, false);
            app_header_t hdr;
            APP_SlotInfo(slot, &hdr);
            app_copy(name, sizeof(name), hdr.name, APP_NAME_LEN);
            UI_GOGU_PrintSmallAtY(name, APP_NAME_TEXT_X, y, false);

            if (index == sel)
                app_invert_name(y);
        }

        UI_GOGU_DrawFooter("SELECT", NULL, "EXIT");

        ST7565_BlitStatusLine();
        ST7565_BlitFullScreen();
#ifdef ENABLE_FEAT_F4HWN_K5VIEWER
        K5VIEWER_Update(false);
#endif

        const KEY_Code_t key = app_get_key();
        if (key == KEY_EXIT)
            return;

        switch (key)
        {
            case KEY_UP:
                sel = (sel == 0u) ? (uint8_t)(count - 1u) : (uint8_t)(sel - 1u);
                break;
            case KEY_DOWN:
                sel = (uint8_t)((sel + 1u) % count);
                break;
            case KEY_MENU:
            {
                const uint8_t slot = slots[sel];
                app_header_t hdr;
                APP_SlotInfo(slot, &hdr);
                const uint8_t rc = APP_LaunchOverlay(slot);  /* runs until the app exits */
                if (rc != APP_OK)
                    app_show_error(&hdr, rc);               /* no longer silent */
                app_wait_release();
                /* Radio apps (BEAM, ...) hand back straight to the radio screen. */
                if (rc == APP_OK && (hdr.flags & APP_FLAG_EXIT_TO_MAIN))
                    return;
                break;
            }
            default:
                break;
        }
    }
}

#endif /* ENABLE_FEAT_F4HWN_OVERLAY_APPS */
