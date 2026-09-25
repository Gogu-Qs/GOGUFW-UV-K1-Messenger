/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0
 */

#ifdef ENABLE_FMRADIO

#include <string.h>

#include "app/fm.h"
#include "driver/bk1080.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/fmradio.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/ui.h"

static uint8_t text_width_3x5(const char *s) { return (uint8_t)(strlen(s) * 4U); }
static uint8_t center_x_3x5(const char *s) { uint8_t w = text_width_3x5(s); return (w >= 128U) ? 0U : (uint8_t)((128U - w) / 2U); }
#ifndef ENABLE_FEAT_F4HWN
static uint8_t text_width_small(const char *s) { return (uint8_t)(strlen(s) * 7U); }
#endif

static uint8_t FM_UI_BigFreqWidth(const char *s)
{
    uint8_t w = 0U;
    bool started = false;
    if (s == NULL) return 0U;
    while (*s) {
        char c = *s++;
        if (!started && c == ' ') continue;
        started = true;
        if (c == '.') {
            w = (uint8_t)(w + 3U);
        } else {
            w = (uint8_t)(w + 13U);
        }
    }
    return w;
}

static uint8_t FM_UI_BigFreqCenterX(const char *s)
{
    uint8_t w = FM_UI_BigFreqWidth(s);
    return (w >= 128U) ? 0U : (uint8_t)((128U - w) / 2U);
}

static uint8_t FM_UI_MapFreqToScaleX(uint16_t freq10)
{
    const uint16_t lo = BK1080_GetFreqLoLimit(gEeprom.FM_Band);
    const uint16_t hi = BK1080_GetFreqHiLimit(gEeprom.FM_Band);
    const uint8_t x1 = 12U;
    const uint8_t x2 = 116U;
    uint32_t span;

    if (hi <= lo) return x1;
    if (freq10 < lo) freq10 = lo;
    if (freq10 > hi) freq10 = hi;

    span = (uint32_t)(hi - lo);
    return (uint8_t)(x1 + (((uint32_t)(freq10 - lo) * (uint32_t)(x2 - x1)) / span));
}

static void FM_UI_DrawVfoScale(uint16_t freq10)
{
    const uint8_t x1 = 12U;
    const uint8_t x2 = 116U;
    const uint8_t y = 35U;
    uint8_t x;
    uint8_t i;
    char loText[8];
    char hiText[8];

    UI_DrawLineBuffer(gFrameBuffer, x1, y, x2, y, 1);
    for (i = 0; i <= 6U; i++) {
        x = (uint8_t)(x1 + (((uint16_t)(x2 - x1) * i) / 6U));
        UI_DrawLineBuffer(gFrameBuffer, x, (i == 0U || i == 6U) ? (y - 4U) : (y - 2U), x, y + 2U, 1);
    }

    x = FM_UI_MapFreqToScaleX(freq10);
    UI_DrawLineBuffer(gFrameBuffer, x, y - 6U, x, y + 5U, 1);
    if (x > 0U) UI_DrawLineBuffer(gFrameBuffer, x - 1U, y - 4U, x - 1U, y - 1U, 1);
    if (x < 127U) UI_DrawLineBuffer(gFrameBuffer, x + 1U, y - 4U, x + 1U, y - 1U, 1);

    snprintf(loText, sizeof(loText), "%u.%u", BK1080_GetFreqLoLimit(gEeprom.FM_Band) / 10U, BK1080_GetFreqLoLimit(gEeprom.FM_Band) % 10U);
    snprintf(hiText, sizeof(hiText), "%u", BK1080_GetFreqHiLimit(gEeprom.FM_Band) / 10U);
#ifdef ENABLE_FEAT_F4HWN
    GUI_DisplaySmallest(loText, x1, 39, false, true);
    GUI_DisplaySmallest(hiText, (uint8_t)(x2 - text_width_3x5(hiText) + 1U), 39, false, true);
#else
    UI_PrintStringSmallNormal(loText, x1, 0, 5);
    UI_PrintStringSmallNormal(hiText, (uint8_t)(x2 - text_width_small(hiText) + 1U), 0, 5);
#endif
}

static void FM_UI_ChannelLabel(char *buf, size_t len)
{
    if (buf == NULL || len == 0U) {
        return;
    }
    snprintf(buf, len, "CH%02u", (unsigned)(gEeprom.FM_SelectedChannel + 1U));
}

static void FM_UI_DrawMenuArrows(void)
{
    const uint8_t center_y = 36u;
    for (uint8_t depth = 0u; depth < 5u; depth++) {
        UI_DrawLineBuffer(gFrameBuffer, (uint8_t)(10u + depth),
                          (uint8_t)(center_y - depth), (uint8_t)(10u + depth),
                          (uint8_t)(center_y + depth), 1);
        UI_DrawLineBuffer(gFrameBuffer, (uint8_t)(117u - depth),
                          (uint8_t)(center_y - depth), (uint8_t)(117u - depth),
                          (uint8_t)(center_y + depth), 1);
    }
}

static uint8_t FM_UI_ReadRssiLevel(void)
{
    return FM_GetRssiLevel();
}

static void FM_UI_DrawRssiBars(void)
{
    const uint8_t level = FM_UI_ReadRssiLevel();
    /* Header meter: same five-step geometry as HEARD, above the y=9 line. */
    const uint8_t x0 = 0U;
    const uint8_t yBase = 6U;

    for (uint8_t i = 0U; i < 5U; i++) {
        const uint8_t h = (uint8_t)(2U + i);
        const uint8_t x = (uint8_t)(x0 + i * 4U);
        const uint8_t yTop = (uint8_t)(yBase - h);

        if (level > i) {
            for (uint8_t xx = x; xx <= (uint8_t)(x + 2U); xx++)
                UI_DrawLineBuffer(gFrameBuffer, xx, yTop, xx, yBase, 1);
        } else {
            UI_DrawLineBuffer(gFrameBuffer, x, yBase, (uint8_t)(x + 2U), yBase, 1);
        }
    }
}

void UI_UpdateFMRssiBar(void)
{
    if (FM_IsNameEditActive() || FM_IsAutoScanConfirmActive() || FM_IsLiveRssiEditActive())
        return;

    /* Update only the left header strip to avoid audible full-page FM writes. */
    memset(&gFrameBuffer[0][0], 0, 36);
    FM_UI_DrawRssiBars();
    ST7565_DrawLine(0, 1, &gFrameBuffer[0][0], 36);
}

static void FM_UI_DrawEditName(void)
{
    const char *edit = FM_GetNameEditBuffer();
    UI_GOGU_DrawTextEditor("CH-NAME", edit, 15u, "SAVE",
                           (FM_GetNameEditorMode() == 2U) ? "2" : (FM_GetNameEditorUpper() ? "B" : "b"),
                           false);
}

static void FM_UI_DrawAutoScanConfirm(void)
{
    UI_DisplayClear();
#ifdef ENABLE_FEAT_F4HWN
    UI_DisplayUnlockKeyboard(5);
#endif
    UI_GOGU_DrawHeader("AUTO SCAN", NULL);
    UI_GOGU_DrawDottedSeparator(9u);
    GUI_DisplaySmallest("SAVED CHANNELS", center_x_3x5("SAVED CHANNELS"), 17u, false, true);
    GUI_DisplaySmallest("WILL BE ERASED", center_x_3x5("WILL BE ERASED"), 27u, false, true);
    GUI_DisplaySmallest("SURE?", center_x_3x5("SURE?"), 37u, false, true);
    UI_GOGU_DrawFooter("SCAN", NULL, "EXIT");
}

static void FM_UI_DrawLiveRssiEdit(void)
{
    UI_DisplayClear();
#ifdef ENABLE_FEAT_F4HWN
    UI_DisplayUnlockKeyboard(5);
#endif
    UI_GOGU_DrawHeader("LIVE RSSI", NULL);
    UI_GOGU_DrawDottedSeparator(9u);
    UI_PrintString(FM_GetLiveRssiSelection() ? "ON" : "OFF", 0, 127, 3, 10);
    UI_GOGU_DrawFooter("SAVE", "UP/DN", "EXIT");
}

void UI_DisplayFM(void)
{
    char String[20] = {0};
    char centerText[20] = {0};
    char modeLabel[8] = {0};
    char bandText[16] = {0};
    char saveName[19] = {0};
    const char *centerLabel = NULL;
    bool showSaveName = false;

    if (FM_IsNameEditActive()) {
        FM_UI_DrawEditName();
        ST7565_BlitFullScreen();
        return;
    }
    if (FM_IsAutoScanConfirmActive()) {
        FM_UI_DrawAutoScanConfirm();
        ST7565_BlitFullScreen();
        return;
    }
    if (FM_IsLiveRssiEditActive()) {
        FM_UI_DrawLiveRssiEdit();
        ST7565_BlitFullScreen();
        return;
    }

#ifdef ENABLE_FEAT_F4HWN_ACTION_PICKER
    if (UI_DisplayActionPicker())
        return;
#endif

    UI_DisplayClear();

#ifdef ENABLE_FEAT_F4HWN
    UI_DisplayUnlockKeyboard(5);
#endif

    if (gAskToSave) {
        const bool occupied = FM_CheckValidChannel(gFM_ChannelPosition);
        const char *name = occupied ? FM_GetChannelName(gFM_ChannelPosition) : NULL;
        if (name != NULL && name[0] != '\0') {
            const uint8_t name_len = (uint8_t)strlen(name);
            snprintf(saveName, sizeof(saveName), "%02u - ",
                     (unsigned)(gFM_ChannelPosition + 1u));
            if (name_len <= 13u) {
                memcpy(&saveName[5], name, name_len + 1u);
            } else {
                memcpy(&saveName[5], name, 11u);
                saveName[16] = '.';
                saveName[17] = '.';
                saveName[18] = '\0';
            }
            showSaveName = true;
        }
        centerLabel = "SAVE?";
        strcpy(modeLabel, "SAVE");
    } else if (FM_GetMenuMode() != 0U) {
        if (gEeprom.FM_IsMrMode)
            FM_UI_ChannelLabel(modeLabel, sizeof(modeLabel));
        else
            strcpy(modeLabel, "VFO");
        if (FM_GetMenuMode() == 3U)
            centerLabel = "LIVE RSSI";
        else if (FM_GetMenuMode() == 2U)
            centerLabel = "CH-NAME";
        else if (FM_GetMenuMode() == 1U)
            centerLabel = "CH-DEL?";
        else
            centerLabel = "SAVE?";
    } else if (gAskToDelete) {
        centerLabel = "DEL?";
        strcpy(modeLabel, "DEL");
    } else if (gFM_ScanState == FM_SCAN_OFF) {
        if (gEeprom.FM_IsMrMode) {
            const char *name;
            FM_UI_ChannelLabel(modeLabel, sizeof(modeLabel));
            name = FM_GetChannelName(gEeprom.FM_SelectedChannel);
            if (name[0]) {
                strncpy(centerText, name, sizeof(centerText) - 1U);
                centerText[sizeof(centerText) - 1U] = 0;
            } else {
                snprintf(centerText, sizeof(centerText), "MR(CH%02u)", gEeprom.FM_SelectedChannel + 1U);
            }
            centerLabel = centerText;
        } else {
            strcpy(modeLabel, "VFO");
            centerLabel = "VFO";
        }
    } else if (gFM_AutoScan) {
        strcpy(modeLabel, "SCAN");
        snprintf(centerText, sizeof(centerText), "A-SCAN %u", gFM_ChannelPosition);
        centerLabel = centerText;
    } else {
        strcpy(modeLabel, "SCAN");
        centerLabel = "M-SCAN";
    }

    UI_GOGU_DrawHeader("FM RADIO", modeLabel);

    if (gAskToSave || (gEeprom.FM_IsMrMode && gInputBoxIndex > 0)) {
        if (!showSaveName) {
            UI_GenerateChannelString(String, gFM_ChannelPosition);
            UI_PrintString(String, 0, 127, 1, 10);
        }
    } else if (gAskToDelete && !gEeprom.FM_IsMrMode) {
        snprintf(String, sizeof(String), "CH-%02u", gEeprom.FM_SelectedChannel + 1U);
        UI_PrintString(String, 0, 127, 1, 10);
    } else {
        if (gInputBoxIndex == 0) {
            snprintf(String, sizeof(String), "%u.%u", gEeprom.FM_FrequencyPlaying / 10U, gEeprom.FM_FrequencyPlaying % 10U);
        } else {
            const char *ascii = INPUTBOX_GetAscii();
            snprintf(String, sizeof(String), "%.3s.%.1s", ascii, ascii + 3);
        }
        UI_DisplayFrequency(String, FM_UI_BigFreqCenterX(String), 1, false);
    }

    /* The stock big renderer starts at y=8. Move both frequency and CH entry
       down four pixels so their shared baseline is centred in the content. */
    for (uint8_t x = 0u; x < 128u; x++) {
        const uint16_t bits = (uint16_t)gFrameBuffer[1][x] |
                              ((uint16_t)gFrameBuffer[2][x] << 8u);
        gFrameBuffer[1][x] = (uint8_t)(bits << 4u);
        gFrameBuffer[2][x] = (uint8_t)(bits >> 4u);
        gFrameBuffer[3][x] = (uint8_t)(bits >> 12u);
    }
    if (showSaveName) {
        const uint8_t width = (uint8_t)(strlen(saveName) * 7u);
        UI_GOGU_PrintSmallAtY(saveName, width >= 128u ? 0u : (uint8_t)((128u - width) / 2u),
                              17u, false);
    }

    if (gEeprom.FM_IsMrMode || FM_GetMenuMode() != 0U || gAskToSave || gAskToDelete ||
        (gFM_ScanState != FM_SCAN_OFF && gFM_AutoScan)) {
        /* Memory/menu/save/autoscan panel: full rectangle with centered label. */
        UI_DrawLineBuffer(gFrameBuffer, 8, 29, 119, 29, 1);
        UI_DrawLineBuffer(gFrameBuffer, 8, 43, 119, 43, 1);
        UI_DrawLineBuffer(gFrameBuffer, 8, 29, 8, 43, 1);
        UI_DrawLineBuffer(gFrameBuffer, 119, 29, 119, 43, 1);
        if (FM_GetMenuMode() != 0u)
            FM_UI_DrawMenuArrows();
        if (centerLabel != NULL) {
            UI_PrintStringSmallBold(centerLabel, 10, 117, 4);
            /* Small-font rendering is page based. Shift only the label's
             * columns down one pixel inside the panel; leave its border and
             * the rest of the FM screen untouched. */
            for (uint8_t x = 10U; x <= 117U; ++x)
                gFrameBuffer[4][x] <<= 1;
        }
    } else {
        /* VFO mode: no rectangle; show a real frequency ruler. The marker is
           calculated from the current playing frequency, so it follows manual
           tuning and normal VFO scan movement. */
        FM_UI_DrawVfoScale(gEeprom.FM_FrequencyPlaying);
    }

    snprintf(bandText, sizeof(bandText), "%d%s-%dM",
            BK1080_GetFreqLoLimit(gEeprom.FM_Band) / 10,
            gEeprom.FM_Band == 0 ? ".5" : "",
            BK1080_GetFreqHiLimit(gEeprom.FM_Band) / 10);
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);
    UI_GOGU_DrawFooter("MENU", bandText, "EXIT");
    FM_UI_DrawRssiBars();
    if (FM_IsLiveRssiEnabled())
        GUI_DisplaySmallest("LIVE", 0u, 11u, false, true);

    ST7565_BlitFullScreen();
}

#endif
