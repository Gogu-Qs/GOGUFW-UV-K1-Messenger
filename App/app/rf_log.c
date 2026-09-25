#include <stdbool.h>
#include <string.h>

#include "app/common.h"
#include "app/generic.h"
#include "app/rf_log.h"
#include "audio.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/main.h"
#include "ui/ui.h"

#define RFLOG_CAPACITY       20u
#define RFLOG_VISIBLE_ROWS UI_GOGU_CONTENT_ROWS
#define RFLOG_CHANNEL_NONE 0xFFFFu
#define RFLOG_FLAG_TX       (1u << 0)

typedef struct {
    uint32_t frequency;
    uint16_t duration_seconds;
    uint16_t age_seconds;
    uint16_t channel;
    uint8_t  flags;
    uint8_t  reserved;
} RFLOG_Entry_t;

_Static_assert(sizeof(RFLOG_Entry_t) == 12u, "unexpected RF log entry layout");

static RFLOG_Entry_t gEntries[RFLOG_CAPACITY];
static uint8_t gCount;
static uint8_t gCursor;
static uint8_t gScroll;
static bool gSessionActive;
static uint8_t gSessionFlags;
static uint32_t gSessionFrequency;
static uint16_t gSessionChannel;
static uint16_t gSessionTicks500ms;
static bool gAgeHalfSecond;

static void RFLOG_Begin(uint8_t flags, const VFO_Info_t *vfo, bool tx)
{
    if (vfo == NULL)
        return;

    const uint32_t frequency = tx ? vfo->pTX->Frequency : vfo->pRX->Frequency;
    const uint16_t channel = IS_MR_CHANNEL(vfo->CHANNEL_SAVE)
        ? vfo->CHANNEL_SAVE
        : RFLOG_CHANNEL_NONE;

    if (gSessionActive && gSessionFlags == flags &&
        gSessionFrequency == frequency && gSessionChannel == channel)
        return;

    GOGU_RFLOG_EndActive();
    gSessionActive = true;
    gSessionFlags = flags;
    gSessionFrequency = frequency;
    gSessionChannel = channel;
    gSessionTicks500ms = 0u;
}

void GOGU_RFLOG_BeginRx(const VFO_Info_t *vfo, FUNCTION_Type_t function)
{
    (void)function;
    RFLOG_Begin(0u, vfo, false);
}

void GOGU_RFLOG_BeginTx(const VFO_Info_t *vfo)
{
    RFLOG_Begin(RFLOG_FLAG_TX, vfo, true);
}

void GOGU_RFLOG_EndActive(void)
{
    if (!gSessionActive)
        return;

    const uint8_t moveCount = gCount < RFLOG_CAPACITY
        ? gCount
        : (uint8_t)(RFLOG_CAPACITY - 1u);
    if (moveCount > 0u)
        memmove(&gEntries[1], &gEntries[0], moveCount * sizeof(gEntries[0]));

    gEntries[0].frequency = gSessionFrequency;
    gEntries[0].duration_seconds = (uint16_t)((gSessionTicks500ms + 1u) / 2u);
    if (gEntries[0].duration_seconds == 0u)
        gEntries[0].duration_seconds = 1u;
    gEntries[0].age_seconds = 0u;
    gEntries[0].channel = gSessionChannel;
    gEntries[0].flags = gSessionFlags;
    gEntries[0].reserved = 0u;

    if (gCount < RFLOG_CAPACITY)
        gCount++;
    gCursor = 0u;
    gScroll = 0u;
    gSessionActive = false;

    if (gScreenToDisplay == DISPLAY_RF_LOG)
        gUpdateDisplay = true;
}

void GOGU_RFLOG_Tick500ms(void)
{
    if (gSessionActive && gSessionTicks500ms < 0xFFFEu)
        gSessionTicks500ms++;

    gAgeHalfSecond = !gAgeHalfSecond;
    if (gAgeHalfSecond)
        return;

    for (uint8_t i = 0u; i < gCount; i++)
        if (gEntries[i].age_seconds < 0xFFFFu)
            gEntries[i].age_seconds++;

    if (gScreenToDisplay == DISPLAY_RF_LOG)
        gUpdateDisplay = true;
}

void GOGU_RFLOG_Open(void)
{
    gCursor = 0u;
    gScroll = 0u;
    GUI_SelectNextDisplay(DISPLAY_RF_LOG);
}

static void RFLOG_MoveSelection(int8_t direction)
{
    if (gCount == 0u)
        return;

    int16_t next = (int16_t)gCursor + direction;
    if (next < 0)
        next = (int16_t)gCount - 1;
    if (next >= gCount)
        next = 0;
    gCursor = (uint8_t)next;

    if (gCursor < gScroll)
        gScroll = gCursor;
    if (gCursor >= (uint8_t)(gScroll + RFLOG_VISIBLE_ROWS))
        gScroll = (uint8_t)(gCursor - (RFLOG_VISIBLE_ROWS - 1u));
}

static void RFLOG_SelectEntry(void)
{
    if (gCount == 0u || gCursor >= gCount)
        return;

    const RFLOG_Entry_t *entry = &gEntries[gCursor];
    const uint8_t targetVfo = gEeprom.TX_VFO;

    if (entry->channel != RFLOG_CHANNEL_NONE) {
        if (!RADIO_CheckValidChannel(entry->channel, false, targetVfo)) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }
        gEeprom.MrChannel[targetVfo] = entry->channel;
        gEeprom.ScreenChannel[targetVfo] = entry->channel;
        gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
    } else {
        const uint16_t channel = (uint16_t)(FREQ_CHANNEL_FIRST + FREQUENCY_GetBand(entry->frequency));
        gEeprom.FreqChannel[targetVfo] = channel;
        gEeprom.ScreenChannel[targetVfo] = channel;
        RADIO_ConfigureChannel(targetVfo, VFO_CONFIGURE_RELOAD);

        VFO_Info_t *target = &gEeprom.VfoInfo[targetVfo];
        target->FrequencyReverse = false;
        target->pRX = &target->freq_config_RX;
        target->pTX = &target->freq_config_TX;
        target->freq_config_RX.Frequency = entry->frequency;
        RADIO_ApplyOffset(target);
        gRequestSaveChannel = 1;
        gVfoConfigureMode = VFO_CONFIGURE;
    }

    gRequestSaveVFO = true;
    gRequestDisplayScreen = DISPLAY_MAIN;
    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
}

void GOGU_RFLOG_ProcessKeys(KEY_Code_t key, bool isPressed, bool isHeld)
{
    if (key == KEY_PTT) {
        GENERIC_Key_PTT(isPressed);
        return;
    }

    if (key == KEY_F && isPressed && isHeld) {
        COMMON_KeypadLockToggle();
        gUpdateStatus = true;
        return;
    }

    /* Select on key release so the same MENU event cannot reach the main
     * screen after RFLOG_SelectEntry() requests the display transition. */
    if (key == KEY_MENU && !isPressed && !isHeld) {
        RFLOG_SelectEntry();
        return;
    }

    if (!isPressed || isHeld)
        return;

    switch (key) {
        case KEY_UP:
            RFLOG_MoveSelection(-1);
            gUpdateDisplay = true;
            break;

        case KEY_DOWN:
            RFLOG_MoveSelection(1);
            gUpdateDisplay = true;
            break;

        case KEY_EXIT:
            gRequestDisplayScreen = DISPLAY_MAIN;
            break;

        default:
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            break;
    }
}

static void RFLOG_FormatFrequency(uint32_t frequency, char *buffer)
{
    sprintf(buffer, "%u.%05u", frequency / 100000u, frequency % 100000u);
}

static void RFLOG_FormatTitle(const RFLOG_Entry_t *entry, char *buffer)
{
    buffer[0] = 0;
    if (entry->channel != RFLOG_CHANNEL_NONE) {
        SETTINGS_FetchChannelName(buffer, entry->channel);
        if (buffer[0] == 0)
            sprintf(buffer, "CH-%u", entry->channel + 1u);
    } else {
        RFLOG_FormatFrequency(entry->frequency, buffer);
    }
    buffer[10] = 0;
}

static void RFLOG_FormatDuration(uint16_t seconds, char *buffer)
{
    if (seconds > 5999u)
        seconds = 5999u;
    sprintf(buffer, "%02u:%02u", seconds / 60u, seconds % 60u);
}

static void RFLOG_FormatAge(uint16_t seconds, char *buffer)
{
    if (seconds < 60u)
        strcpy(buffer, "NOW");
    else if (seconds < 3600u)
        sprintf(buffer, "%um", seconds / 60u);
    else
        sprintf(buffer, "%uh", seconds / 3600u);
}

void UI_DisplayGoguRfLog(void)
{
    UI_DisplayClear();

    char counter[6];
    sprintf(counter, "%u/%u", gCount ? (uint8_t)(gCursor + 1u) : 0u, gCount);
    UI_GOGU_DrawHeader("RF LOG", counter);
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);

    if (gCount == 0u) {
        UI_GOGU_PrintSmallAtY("NO RF LOG", 36u, 25u, false);
        UI_GOGU_DrawFooter("SELECT", NULL, "EXIT");
        ST7565_BlitFullScreen();
        return;
    }

    for (uint8_t row = 0u; row < RFLOG_VISIBLE_ROWS; row++) {
        const uint8_t index = (uint8_t)(gScroll + row);
        if (index >= gCount)
            break;

        const RFLOG_Entry_t *entry = &gEntries[index];
        char title[11];
        char duration[6];
        char age[5];

        RFLOG_FormatTitle(entry, title);
        RFLOG_FormatDuration(entry->duration_seconds, duration);
        RFLOG_FormatAge(entry->age_seconds, age);

        const uint8_t y = UI_GOGU_CONTENT_ROW_Y(row);
        GUI_DisplaySmallest((entry->flags & RFLOG_FLAG_TX) ? "TX" : "RX",
                            1u, (uint8_t)(y + 1u), false, true);
        UI_GOGU_PrintSmallAtY(title, 11u, y, false);
        GUI_DisplaySmallest(duration, 91u, (uint8_t)(y + 1u), false, true);
        {
            const uint8_t age_width = (uint8_t)(strlen(age) * 4u);
            GUI_DisplaySmallest(age, age_width >= 128u ? 0u : (uint8_t)(128u - age_width),
                                (uint8_t)(y + 1u), false, true);
        }

        if (index == gCursor)
            UI_GOGU_InvertBand((uint8_t)(y - 1u), 9u);
    }

    UI_GOGU_DrawFooter("SELECT", NULL, "EXIT");

    ST7565_BlitFullScreen();
}
