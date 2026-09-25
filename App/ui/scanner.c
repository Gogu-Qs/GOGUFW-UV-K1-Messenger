/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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

#include <stdbool.h>
#include <string.h>
#include "app/scanner.h"
#include "dcs.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/scanner.h"

static void format_save_target(char *out, const char *action, uint16_t channel,
                               bool occupied, const char *name)
{
    uint8_t used = (uint8_t)snprintf(out, 20u, occupied ? "%s%04u" : "%sCH-%04u",
                                     action, (unsigned)(channel + 1u));
    if (used > 18u)
        used = 18u;
    if (occupied && used < 18u) {
        const char *label = (name != NULL && name[0] != '\0') ? name : "USED";
        out[used++] = ' ';
        const uint8_t available = (uint8_t)(18u - used);
        const uint8_t length = (uint8_t)strlen(label);
        if (length <= available) {
            memcpy(&out[used], label, length);
            used = (uint8_t)(used + length);
        } else if (available >= 2u) {
            const uint8_t copy = (uint8_t)(available - 2u);
            memcpy(&out[used], label, copy);
            used = (uint8_t)(used + copy);
            out[used++] = '.';
            out[used++] = '.';
        }
    }
    out[used] = '\0';
}

void UI_DisplayScanner(void)
{
    char status[20];
    char channel[8];
    char channel_name[11] = "";
    char frequency[18];
    char tone[18];
    const bool scanning = gScanCssState < SCAN_CSS_STATE_FOUND;
    const bool complete = gScanCssState == SCAN_CSS_STATE_FOUND;

    UI_DisplayClear();

    UI_GOGU_DrawHeader(gScanSingleFrequency ? "SEARCH TONE" : "SEARCH FREQ", NULL);
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);
    if (gScannerSaveState == SCAN_SAVE_CHAN_SEL ||
        gScannerSaveState == SCAN_SAVE_CHANNEL) {
        if (gScannerSaveState == SCAN_SAVE_CHAN_SEL && gInputBoxIndex > 0u) {
            UI_GenerateChannelStringEx(channel, false, gScanChannel);
            snprintf(status, sizeof(status), "SAVE:%s", channel);
        } else {
            if (gShowChPrefix)
                SETTINGS_FetchChannelName(channel_name, gScanChannel);
            format_save_target(status,
                               gScannerSaveState == SCAN_SAVE_CHAN_SEL ? "SAVE:" : "SAVE?",
                               gScanChannel, gShowChPrefix, channel_name);
        }
    }
    else if (complete)
        strcpy(status, "SCAN COMPLETE");
    else if (gScanCssState == SCAN_CSS_STATE_FAILED)
        strcpy(status, "SCAN FAILED");
    else
        strcpy(status, "SCANNING");
    UI_GOGU_PrintSmallAtY(status, 0u, 12u, false);

    if (gScanSingleFrequency || (gScanCssState != SCAN_CSS_STATE_OFF && gScanCssState != SCAN_CSS_STATE_FAILED)) {
        sprintf(frequency, "FREQ:%u.%05u", gScanFrequency / 100000u, gScanFrequency % 100000u);
    } else {
        strcpy(frequency, "FREQ:---.-----");
    }
    UI_GOGU_PrintSmallAtY(frequency, 0u, 23u, false);

    if (gScanCssState < SCAN_CSS_STATE_FOUND) {
        strcpy(tone, "TONE:---");
    } else if (!gScanUseCssResult) {
        strcpy(tone, "TONE:NONE");
    } else if (gScanCssResultType == CODE_TYPE_CONTINUOUS_TONE) {
        sprintf(tone, "TONE:%u.%uHZ", CTCSS_Options[gScanCssResultCode] / 10u,
                CTCSS_Options[gScanCssResultCode] % 10u);
    } else {
        sprintf(tone, "TONE:D%03oN", DCS_Options[gScanCssResultCode]);
    }
    UI_GOGU_PrintSmallAtY(tone, 0u, 34u, false);

    UI_GOGU_DrawFooter(scanning ? "SCANNING" : (complete ? "SAVE" : "FAILED"),
                       NULL, "EXIT");
    ST7565_BlitFullScreen();
}
