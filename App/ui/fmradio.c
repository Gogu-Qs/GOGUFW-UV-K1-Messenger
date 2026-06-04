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
#include "ui/ui.h"

static uint8_t text_width_3x5(const char *s) { return (uint8_t)(strlen(s) * 4U); }
static uint8_t center_x_3x5(const char *s) { uint8_t w = text_width_3x5(s); return (w >= 128U) ? 0U : (uint8_t)((128U - w) / 2U); }
static uint8_t text_width_small(const char *s) { return (uint8_t)(strlen(s) * 7U); }
static uint8_t center_x_small(const char *s) { uint8_t w = text_width_small(s); return (w >= 128U) ? 0U : (uint8_t)((128U - w) / 2U); }
static uint8_t right_x_small(const char *s) { uint8_t w = text_width_small(s); return (w >= 128U) ? 0U : (uint8_t)(128U - w); }

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
    const uint8_t y = 36U;
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
    GUI_DisplaySmallest(loText, x1, 42, false, true);
    GUI_DisplaySmallest(hiText, (uint8_t)(x2 - text_width_3x5(hiText) + 1U), 42, false, true);
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

static void FM_UI_PrintBandLimitTiny(const char *text)
{
#ifdef ENABLE_FEAT_F4HWN
    GUI_DisplaySmallest(text, 1, 50, false, true);
#else
    UI_PrintStringSmallNormal(text, 1, 0, 6);
#endif
}

static void FM_UI_DrawEditName(void)
{
    char buf[20];
    const char *edit = FM_GetNameEditBuffer();
    UI_DisplayClear();
#ifdef ENABLE_FEAT_F4HWN
    UI_DisplayUnlockKeyboard(5);
#endif
    UI_PrintString("CH-NAME", 0, 127, 0, 8);
    UI_DrawLineBuffer(gFrameBuffer, 8, 17, 119, 17, 1);
    UI_PrintStringSmallBold(edit && edit[0] ? edit : " ", 4, 123, 3);
    UI_DrawLineBuffer(gFrameBuffer, 8, 46, 119, 46, 1);
    GUI_DisplaySmallest("SAVE", 0, 49, false, true);
    snprintf(buf, sizeof(buf), "%u/15", (uint8_t)strlen(edit ? edit : ""));
    GUI_DisplaySmallest(buf, 56, 49, false, true);
    GUI_DisplaySmallest((FM_GetNameEditorMode() == 2U) ? "2" : (FM_GetNameEditorUpper() ? "B" : "b"), 120, 49, false, true);
}

static void FM_UI_DrawAutoScanConfirm(void)
{
    UI_DisplayClear();
#ifdef ENABLE_FEAT_F4HWN
    UI_DisplayUnlockKeyboard(5);
#endif
    UI_PrintString("AUTO SCAN", 0, 127, 0, 8);
    GUI_DisplaySmallest("SAVED CHANNELS", center_x_3x5("SAVED CHANNELS"), 17, false, true);
    GUI_DisplaySmallest("WILL BE ERASED", center_x_3x5("WILL BE ERASED"), 25, false, true);
    GUI_DisplaySmallest("SURE?", center_x_3x5("SURE?"), 35, false, true);
    GUI_DisplaySmallest("YES/SCAN", 0, 49, false, true);
    GUI_DisplaySmallest("NO/EXIT", 100, 49, false, true);
}

void UI_DisplayFM(void)
{
    char String[20] = {0};
    char centerText[20] = {0};
    char modeLabel[8] = {0};
    char bandText[16] = {0};
    const char *centerLabel = NULL;

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

    UI_DisplayClear();

#ifdef ENABLE_FEAT_F4HWN
    UI_DisplayUnlockKeyboard(5);
#endif

    UI_PrintStringSmallNormal("FM", 0, 0, 0);

    if (gAskToSave) {
        centerLabel = "SAVE?";
        strcpy(modeLabel, "SAVE");
    } else if (gEeprom.FM_IsMrMode && FM_GetMenuMode() != 0U) {
        FM_UI_ChannelLabel(modeLabel, sizeof(modeLabel));
        centerLabel = (FM_GetMenuMode() == 2U) ? "CH-NAME" : "CH-DEL?";
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

    UI_PrintStringSmallNormal(modeLabel, right_x_small(modeLabel), 0, 0);

    if (gAskToSave || (gEeprom.FM_IsMrMode && gInputBoxIndex > 0)) {
        UI_GenerateChannelString(String, gFM_ChannelPosition);
        UI_PrintString(String, 0, 127, 2, 10);
    } else if (gAskToDelete && !gEeprom.FM_IsMrMode) {
        snprintf(String, sizeof(String), "CH-%02u", gEeprom.FM_SelectedChannel + 1U);
        UI_PrintString(String, 0, 127, 2, 10);
    } else {
        if (gInputBoxIndex == 0) {
            snprintf(String, sizeof(String), "%u.%u", gEeprom.FM_FrequencyPlaying / 10U, gEeprom.FM_FrequencyPlaying % 10U);
        } else {
            const char *ascii = INPUTBOX_GetAscii();
            snprintf(String, sizeof(String), "%.3s.%.1s", ascii, ascii + 3);
        }
        /* Keep the main FM frequency in the original large F4HWN style and
           original upper position.  Do not move the lower MR box / VFO ruler. */
        UI_DisplayFrequency(String, FM_UI_BigFreqCenterX(String), 1, false);
    }

    if (gEeprom.FM_IsMrMode || gAskToSave || gAskToDelete || (gFM_ScanState != FM_SCAN_OFF && gFM_AutoScan)) {
        /* Memory/menu/save/autoscan panel: full rectangle with centered label. */
        UI_DrawLineBuffer(gFrameBuffer, 8, 29, 119, 29, 1);
        UI_DrawLineBuffer(gFrameBuffer, 8, 43, 119, 43, 1);
        UI_DrawLineBuffer(gFrameBuffer, 8, 29, 8, 43, 1);
        UI_DrawLineBuffer(gFrameBuffer, 119, 29, 119, 43, 1);
        if (centerLabel != NULL) {
            UI_PrintStringSmallBold(centerLabel, 10, 117, 4);
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
    FM_UI_PrintBandLimitTiny(bandText);

    ST7565_BlitFullScreen();
}

#endif
