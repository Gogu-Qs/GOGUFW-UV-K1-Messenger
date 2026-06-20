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

#ifdef ENABLE_FMRADIO

#include <string.h>

#include "app/action.h"
#include "app/fm.h"
#include "app/generic.h"
#include "app/messenger_t9.h"
#include "audio.h"
#include "external/printf/printf.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
#include "driver/py25q16.h"
#include "driver/gpio.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

uint16_t          gFM_Channels[FM_CHANNELS_MAX];
bool              gFmRadioMode;
uint8_t           gFmRadioCountdown_500ms;
volatile uint16_t gFmPlayCountdown_10ms;
volatile int8_t   gFM_ScanState;
bool              gFM_AutoScan;
uint8_t           gFM_ChannelPosition;
bool              gFM_FoundFrequency;
uint16_t          gFM_RestoreCountdown_10ms;

#define FM_NAMES_FLASH_ADDR 0x013000u
#define FM_NAMES_MAGIC      0x4747464Du  /* "GGFM" */
#define FM_NAME_LEN         16u
#define FM_NAMES_VERSION    1u

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    uint8_t reserved[10];
} __attribute__((packed)) FM_NamesHeader_t;

#define FM_NAMES_HEADER_SIZE ((uint32_t)sizeof(FM_NamesHeader_t))
#define FM_NAMES_SLOT_ADDR(ch) (FM_NAMES_FLASH_ADDR + FM_NAMES_HEADER_SIZE + ((uint32_t)(ch) * FM_NAME_LEN))
#define FM_NAMES_USED_SIZE  (FM_NAMES_HEADER_SIZE + ((uint32_t)FM_CHANNELS_MAX * FM_NAME_LEN))

static char s_fmNameReadBuf[FM_NAME_LEN];

typedef enum {
    FM_MENU_NONE = 0,
    FM_MENU_DELETE,
    FM_MENU_NAME,
} FM_MenuMode_t;

static FM_MenuMode_t s_fmMenuMode;
static bool s_fmNameEdit;
static bool s_fmAutoScanConfirm;
static char s_fmNameEditBuf[FM_NAME_LEN];
static MSG_T9Editor_t s_fmNameEditor;

const uint8_t BUTTON_STATE_PRESSED = 1 << 0;
const uint8_t BUTTON_STATE_HELD = 1 << 1;

const uint8_t BUTTON_EVENT_PRESSED = BUTTON_STATE_PRESSED;
const uint8_t BUTTON_EVENT_HELD = BUTTON_STATE_PRESSED | BUTTON_STATE_HELD;
const uint8_t BUTTON_EVENT_SHORT =  0;
const uint8_t BUTTON_EVENT_LONG =  BUTTON_STATE_HELD;


static void Key_FUNC(KEY_Code_t Key, uint8_t state);

static void FM_MakeDefaultName(uint8_t Channel, char *out)
{
    snprintf(out, FM_NAME_LEN, "CH-%02u", (uint8_t)(Channel + 1U));
}

static bool FM_NamesHeaderValid(void)
{
    FM_NamesHeader_t hdr;
    PY25Q16_ReadBuffer(FM_NAMES_FLASH_ADDR, &hdr, sizeof(hdr));
    return hdr.magic == FM_NAMES_MAGIC && hdr.version == FM_NAMES_VERSION && hdr.count == FM_CHANNELS_MAX;
}

static void FM_NamesWriteHeader(void)
{
    FM_NamesHeader_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = FM_NAMES_MAGIC;
    hdr.version = FM_NAMES_VERSION;
    hdr.count = FM_CHANNELS_MAX;
    PY25Q16_WriteBuffer(FM_NAMES_FLASH_ADDR, &hdr, sizeof(hdr), false);
}

static void FM_NamesEnsureStore(void)
{
    if (!FM_NamesHeaderValid()) {
        PY25Q16_SectorErase(FM_NAMES_FLASH_ADDR);
        FM_NamesWriteHeader();
    }
}

const char *FM_GetChannelName(uint8_t Channel)
{
    if (Channel >= FM_CHANNELS_MAX) return "";
    if (!FM_NamesHeaderValid()) return "";
    PY25Q16_ReadBuffer(FM_NAMES_SLOT_ADDR(Channel), s_fmNameReadBuf, FM_NAME_LEN);
    if ((uint8_t)s_fmNameReadBuf[0] == 0xFFU || s_fmNameReadBuf[0] == 0) return "";
    s_fmNameReadBuf[FM_NAME_LEN - 1U] = 0;
    return s_fmNameReadBuf;
}

void FM_SetChannelName(uint8_t Channel, const char *Name)
{
    /* Flash cannot reliably change 0 bits back to 1 without erase.
       Keep only a temporary stack copy of the small FM-name store, update one slot,
       erase the private 0x013000 sector, then write it back. This avoids a permanent
       50x16 RAM table and allows renaming the same channel repeatedly. */
    uint8_t store[FM_NAMES_USED_SIZE];
    char slot[FM_NAME_LEN];

    if (Channel >= FM_CHANNELS_MAX) return;
    FM_NamesEnsureStore();

    PY25Q16_ReadBuffer(FM_NAMES_FLASH_ADDR, store, sizeof(store));

    memset(slot, 0, sizeof(slot));
    if (Name != NULL) strncpy(slot, Name, FM_NAME_LEN - 1U);
    slot[FM_NAME_LEN - 1U] = 0;

    memcpy(&store[FM_NAMES_HEADER_SIZE + ((uint32_t)Channel * FM_NAME_LEN)], slot, sizeof(slot));

    PY25Q16_SectorErase(FM_NAMES_FLASH_ADDR);
    PY25Q16_WriteBuffer(FM_NAMES_FLASH_ADDR, store, sizeof(store), false);
}

void FM_SetChannelDefaultName(uint8_t Channel)
{
    char name[FM_NAME_LEN];
    if (Channel >= FM_CHANNELS_MAX) return;
    FM_MakeDefaultName(Channel, name);
    FM_SetChannelName(Channel, name);
}

void FM_NamesLoad(void)
{
    FM_NamesEnsureStore();
}

void FM_NamesSave(void)
{
    FM_NamesEnsureStore();
}

void FM_NamesErase(void)
{
    PY25Q16_SectorErase(FM_NAMES_FLASH_ADDR);
    FM_NamesWriteHeader();
}

void FM_Tick(void)
{
    if (s_fmNameEdit)
        MSG_T9_Tick(&s_fmNameEditor);
}

static void FM_NameEditStart(void)
{
    memset(s_fmNameEditBuf, 0, sizeof(s_fmNameEditBuf));
    const char *cur = FM_GetChannelName(gEeprom.FM_SelectedChannel);
    if (cur[0]) strncpy(s_fmNameEditBuf, cur, FM_NAME_LEN - 1U);
    else FM_MakeDefaultName(gEeprom.FM_SelectedChannel, s_fmNameEditBuf);
    s_fmNameEditBuf[FM_NAME_LEN - 1U] = 0;
    MSG_T9_Start(&s_fmNameEditor, s_fmNameEditBuf, FM_NAME_LEN - 1U);
    s_fmNameEdit = true;
    gRequestDisplayScreen = DISPLAY_FM;
}

static void FM_NameEditSave(void)
{
    MSG_T9_Commit(&s_fmNameEditor);
    FM_SetChannelName(gEeprom.FM_SelectedChannel, s_fmNameEditBuf);
    s_fmNameEdit = false;
    s_fmMenuMode = FM_MENU_NONE;
    gRequestDisplayScreen = DISPLAY_FM;
}

static void FM_NameEditCancel(void)
{
    MSG_T9_Commit(&s_fmNameEditor);
    s_fmNameEdit = false;
    s_fmMenuMode = FM_MENU_NAME;
    gRequestDisplayScreen = DISPLAY_FM;
}

static void FM_StartAutoScanNow(void)
{
    uint16_t freq;
    s_fmAutoScanConfirm = false;
    gFM_AutoScan = true;
    gFM_ChannelPosition = 0;
    FM_EraseChannels();
    freq = BK1080_GetFreqLoLimit(gEeprom.FM_Band);
    FM_Tune(freq, 1, false);
}

bool FM_IsNameEditActive(void) { return s_fmNameEdit; }
bool FM_IsAutoScanConfirmActive(void) { return s_fmAutoScanConfirm; }
uint8_t FM_GetMenuMode(void) { return (uint8_t)s_fmMenuMode; }
const char *FM_GetNameEditBuffer(void) { return s_fmNameEditBuf; }
uint8_t FM_GetNameEditorMode(void) { return s_fmNameEditor.mode; }
bool FM_GetNameEditorUpper(void) { return s_fmNameEditor.upper; }


bool FM_CheckValidChannel(uint8_t Channel)
{
    return  Channel < ARRAY_SIZE(gFM_Channels) && 
            gFM_Channels[Channel] >= BK1080_GetFreqLoLimit(gEeprom.FM_Band) && 
            gFM_Channels[Channel] < BK1080_GetFreqHiLimit(gEeprom.FM_Band);
}

uint8_t FM_FindNextChannel(uint8_t Channel, uint8_t Direction)
{
    for (unsigned i = 0; i < ARRAY_SIZE(gFM_Channels); i++) {
        if (Channel == 0xFF)
            Channel = ARRAY_SIZE(gFM_Channels) - 1;
        else if (Channel >= ARRAY_SIZE(gFM_Channels))
            Channel = 0;
        if (FM_CheckValidChannel(Channel))
            return Channel;
        Channel += Direction;
    }

    return 0xFF;
}

int FM_ConfigureChannelState(void)
{
    gEeprom.FM_FrequencyPlaying = gEeprom.FM_SelectedFrequency;

    if (gEeprom.FM_IsMrMode) {
        const uint8_t Channel = FM_FindNextChannel(gEeprom.FM_SelectedChannel, FM_CHANNEL_UP);
        if (Channel == 0xFF) {
            gEeprom.FM_IsMrMode = false;
            return -1;
        }
        gEeprom.FM_SelectedChannel  = Channel;
        gEeprom.FM_FrequencyPlaying = gFM_Channels[Channel];
    }

    return 0;
}

void FM_TurnOff(void)
{
    gFmRadioMode              = false;
    gFM_ScanState             = FM_SCAN_OFF;
    gFM_RestoreCountdown_10ms = 0;

    AUDIO_AudioPathOff();
    gEnableSpeaker = false;

    BK1080_Init0();

    // Enable relevant LNA based on VFO frequency
    BK4819_PickRXFilterPathBasedOnFrequency(gRxVfo->freq_config_RX.Frequency);


    gUpdateStatus  = true;

    #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        gEeprom.CURRENT_STATE = 0;
        SETTINGS_WriteCurrentState();
    #endif
}

void FM_EraseChannels(void)
{
    //PY25Q16_SectorErase(0x003000);
    
    uint8_t clearBuf[128];
    memset(clearBuf, 0xFF, sizeof(clearBuf));
    PY25Q16_WriteBuffer(0x00A028, clearBuf, sizeof(clearBuf), false);

    memset(gFM_Channels, 0xFF, sizeof(gFM_Channels));
    FM_NamesErase();
}

uint16_t FM_WrapFrequency(uint16_t Frequency) {
    const uint16_t freqLoLimit = BK1080_GetFreqLoLimit(gEeprom.FM_Band);
    const uint16_t freqHiLimit = BK1080_GetFreqHiLimit(gEeprom.FM_Band);

    if (Frequency < freqLoLimit)
        return freqHiLimit;
    else if (Frequency > freqHiLimit)
        return freqLoLimit;

    return Frequency;
}

void FM_Tune(uint16_t Frequency, int8_t Step, bool bFlag)
{
    AUDIO_AudioPathOff();

    gEnableSpeaker = false;

    gFmPlayCountdown_10ms = (gFM_ScanState == FM_SCAN_OFF) ? fm_play_countdown_noscan_10ms : fm_play_countdown_scan_10ms;

    gScheduleFM                 = false;
    gFM_FoundFrequency          = false;
    gAskToSave                  = false;
    gAskToDelete                = false;
    s_fmMenuMode                = FM_MENU_NONE;
    gEeprom.FM_FrequencyPlaying = Frequency;

    if (!bFlag) {
        Frequency += Step;
        Frequency = FM_WrapFrequency(Frequency);

        gEeprom.FM_FrequencyPlaying = Frequency;
    }

    gFM_ScanState = Step;

    BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);
}

void FM_AudioPathOn(void) {
    BACKLIGHT_TurnOn();
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
}

void FM_PlayAndUpdate(void)
{
    gFM_ScanState = FM_SCAN_OFF;

    if (gFM_AutoScan) {
        gEeprom.FM_IsMrMode        = true;
        gEeprom.FM_SelectedChannel = 0;
    }

    FM_ConfigureChannelState();
    BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);
    SETTINGS_SaveFM();

    gFmPlayCountdown_10ms = 0;
    gScheduleFM           = false;
    gAskToSave            = false;

    FM_AudioPathOn();
}

int FM_CheckFrequencyLock(uint16_t Frequency, uint16_t LowerLimit)
{
    const uint16_t Test2     = BK1080_ReadRegister(BK1080_REG_07);
    const uint16_t Deviation = BK1080_REG_07_GET_FREQD(Test2);

    // Helper macro to update globals and return
    #define RETURN(val) \
        do { \
            BK1080_FrequencyDeviation = Deviation; \
            BK1080_BaseFrequency      = Frequency; \
            return (val); \
        } while (0)

    if (BK1080_REG_07_GET_SNR(Test2) <= 2)
        RETURN(-1);

    const uint16_t Status = BK1080_ReadRegister(BK1080_REG_10);
    if ((Status & BK1080_REG_10_MASK_AFCRL) != BK1080_REG_10_AFCRL_NOT_RAILED ||
        BK1080_REG_10_GET_RSSI(Status) < 10)
        RETURN(-1);

    if (Deviation >= 280 && Deviation <= 3815)
        RETURN(-1);

    // Scanning upward: previous deviation was negative (bit 11 set) or near zero
    if (Frequency > LowerLimit && (Frequency - BK1080_BaseFrequency) == 1) {
        if (BK1080_FrequencyDeviation & 0x800 || BK1080_FrequencyDeviation < 20)
            RETURN(-1);
    }

    // Scanning downward: previous deviation was positive or saturated high
    if (Frequency >= LowerLimit && (BK1080_BaseFrequency - Frequency) == 1) {
        if ((BK1080_FrequencyDeviation & 0x800) == 0 || BK1080_FrequencyDeviation > 4075)
            RETURN(-1);
    }

    #undef RETURN

    BK1080_FrequencyDeviation = Deviation;
    BK1080_BaseFrequency      = Frequency;
    return 0;
}

static void Key_DIGITS(KEY_Code_t Key, uint8_t state)
{
    enum { STATE_FREQ_MODE, STATE_MR_MODE, STATE_SAVE };

    if (s_fmMenuMode != FM_MENU_NONE || s_fmAutoScanConfirm) {
        if (state == BUTTON_EVENT_SHORT || state == BUTTON_EVENT_PRESSED)
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    if (state == BUTTON_EVENT_SHORT && !gWasFKeyPressed) {
        uint8_t State;

        if (gAskToDelete) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }

        if (gAskToSave) {
            State = STATE_SAVE;
        }
        else {
            if (gFM_ScanState != FM_SCAN_OFF) {
                gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                return;
            }

            State = gEeprom.FM_IsMrMode ? STATE_MR_MODE : STATE_FREQ_MODE;
        }

        INPUTBOX_Append(Key);
        gKeyInputCountdown = key_input_timeout_500ms;

        gRequestDisplayScreen = DISPLAY_FM;

        if (State == STATE_FREQ_MODE) {
            if (gInputBoxIndex == 1) {
                if (gInputBox[0] > 1) {
                    gInputBox[1] = gInputBox[0];
                    gInputBox[0] = 0;
                    gInputBoxIndex = 2;
                }
            }
            else if (gInputBoxIndex > 3) {
                uint32_t Frequency;

                gInputBoxIndex = 0;
                gKeyInputCountdown = 1;

                Frequency = StrToUL(INPUTBOX_GetAscii());

                if (Frequency < BK1080_GetFreqLoLimit(gEeprom.FM_Band) || BK1080_GetFreqHiLimit(gEeprom.FM_Band) < Frequency) {
                    gBeepToPlay           = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                    gRequestDisplayScreen = DISPLAY_FM;
                    return;
                }

                gEeprom.FM_SelectedFrequency = (uint16_t)Frequency;
#ifdef ENABLE_VOICE
                gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
                gEeprom.FM_FrequencyPlaying = gEeprom.FM_SelectedFrequency;
                BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);
                gRequestSaveFM = true;
                return;
            }
        }
        else if (gInputBoxIndex == 2) {
            uint8_t Channel;

            gInputBoxIndex = 0;
            gKeyInputCountdown = 1;
            
            Channel = ((gInputBox[0] * 10) + gInputBox[1]) - 1;

            if (State == STATE_MR_MODE) {
                if (FM_CheckValidChannel(Channel)) {
#ifdef ENABLE_VOICE
                    gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
                    gEeprom.FM_SelectedChannel = Channel;
                    gEeprom.FM_FrequencyPlaying = gFM_Channels[Channel];
                    BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);
                    gRequestSaveFM = true;
                    return;
                }
            }
            else if (Channel < FM_CHANNELS_MAX) {
#ifdef ENABLE_VOICE
                gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
                gRequestDisplayScreen = DISPLAY_FM;
                gInputBoxIndex = 0;
                gFM_ChannelPosition = Channel;
                return;
            }

            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }

#ifdef ENABLE_VOICE
        gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
    }
    else
        Key_FUNC(Key, state);
}

static void Key_FUNC(KEY_Code_t Key, uint8_t state)
{
    if (state == BUTTON_EVENT_SHORT || state == BUTTON_EVENT_HELD) {
        bool autoScan = gWasFKeyPressed || (state == BUTTON_EVENT_HELD);

        gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
        HideFKeyIcon();
        gRequestDisplayScreen = DISPLAY_FM;

        switch (Key) {
            case KEY_0:
                ACTION_FM();
                break;

            case KEY_1:
                gEeprom.FM_Band++;
                gRequestSaveFM = true;
                break;

            // case KEY_2:
            //  gEeprom.FM_Space = (gEeprom.FM_Space + 1) % 3;
            //  gRequestSaveFM = true;
            //  break;

            case KEY_3:
                /* GOGUFW 1.0.2: VFO/MR mode change must not inherit the
                 * previous mode's FM scan state.  A VFO scan could otherwise
                 * enter MR mode with the UI still showing M-SCAN. */
                if (gFM_ScanState != FM_SCAN_OFF) {
                    gFM_ScanState = FM_SCAN_OFF;
                    gFM_AutoScan = false;
                    gScheduleFM = false;
                    gFmPlayCountdown_10ms = 0;
                    gFM_FoundFrequency = false;
                }

                gEeprom.FM_IsMrMode = !gEeprom.FM_IsMrMode;

                if (!FM_ConfigureChannelState()) {
                    BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);
                    gRequestSaveFM = true;
                }
                else
                    gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                break;

            case KEY_8:
                ACTION_BackLightOnDemand();
                break;

            case KEY_9:
                ACTION_BackLight();
                break;

            case KEY_STAR:
                if (autoScan && gEeprom.FM_IsMrMode && gFM_ScanState == FM_SCAN_OFF) {
                    s_fmAutoScanConfirm = true;
                    gRequestDisplayScreen = DISPLAY_FM;
                } else {
                    /* Keep original VFO scan behavior: auto-scan/save flow is only for MR mode. */
                    ACTION_Scan(gEeprom.FM_IsMrMode ? autoScan : false);
                }
                break;

            default:
                gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                break;
        }
    }
}

static void Key_EXIT(uint8_t state)
{
    if (gInputBoxIndex) {
        if (state != BUTTON_EVENT_SHORT)
            return;
    } 
    else {
        if (state != BUTTON_EVENT_PRESSED)
            return;
    }

    gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

    if (s_fmNameEdit) { FM_NameEditCancel(); return; }
    if (s_fmAutoScanConfirm) { s_fmAutoScanConfirm = false; gRequestDisplayScreen = DISPLAY_FM; return; }

    if (gFM_ScanState == FM_SCAN_OFF) {
        if (gInputBoxIndex == 0) {
            if (!gAskToSave && !gAskToDelete && s_fmMenuMode == FM_MENU_NONE) {
                ACTION_FM();
                return;
            }

            gAskToSave   = false;
            gAskToDelete = false;
            s_fmMenuMode = FM_MENU_NONE;
        }
        else {
            gInputBox[--gInputBoxIndex] = 10;
            gKeyInputCountdown = key_input_timeout_500ms;

            if (gInputBoxIndex) {
                if (gInputBoxIndex != 1) {
                    gRequestDisplayScreen = DISPLAY_FM;
                    return;
                }

                if (gInputBox[0] != 0) {
                    gRequestDisplayScreen = DISPLAY_FM;
                    return;
                }
            }
            gInputBoxIndex = 0;
        }

#ifdef ENABLE_VOICE
        gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
    }
    else {
        FM_PlayAndUpdate();
#ifdef ENABLE_VOICE
        gAnotherVoiceID = VOICE_ID_SCANNING_STOP;
#endif
    }

    gRequestDisplayScreen = DISPLAY_FM;
}

static void Key_MENU(uint8_t state)
{
    if (state == BUTTON_EVENT_HELD) {
        ACTION_Handle(KEY_MENU, true, true);
        return;
    }
    else if (state != BUTTON_EVENT_SHORT) {
        return;
    }

    gRequestDisplayScreen = DISPLAY_FM;
    gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;

    HideFKeyIcon();

    if (s_fmNameEdit) { FM_NameEditSave(); return; }
    if (s_fmAutoScanConfirm) { FM_StartAutoScanNow(); return; }

    if (gFM_ScanState == FM_SCAN_OFF) {
        if (gInputBoxIndex) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }

        if (!gEeprom.FM_IsMrMode) {
            if (gAskToSave) {
                gFM_Channels[gFM_ChannelPosition] = gEeprom.FM_FrequencyPlaying;
                gRequestSaveFM = true;
            }
            gAskToSave = !gAskToSave;
        }
        else {
            if (s_fmMenuMode == FM_MENU_NONE) {
                s_fmMenuMode = FM_MENU_NAME;
                gAskToDelete = false;
            } else if (s_fmMenuMode == FM_MENU_DELETE) {
                gFM_Channels[gEeprom.FM_SelectedChannel] = 0xFFFF;
                FM_SetChannelName(gEeprom.FM_SelectedChannel, "");

                FM_ConfigureChannelState();
                BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);

                s_fmMenuMode = FM_MENU_NONE;
                gAskToDelete = false;
                gRequestSaveFM = true;
            } else if (s_fmMenuMode == FM_MENU_NAME) {
                FM_NameEditStart();
            }
        }
    }
    else {
        if (gFM_AutoScan || !gFM_FoundFrequency) {
            gBeepToPlay    = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            gInputBoxIndex = 0;
            return;
        }

        if (gAskToSave) {
            gFM_Channels[gFM_ChannelPosition] = gEeprom.FM_FrequencyPlaying;
            gRequestSaveFM = true;
        }
        gAskToSave = !gAskToSave;
    }
}

static void Key_UP_DOWN(uint8_t state, int8_t Step)
{
    HideFKeyIcon();

    if (s_fmNameEdit || s_fmAutoScanConfirm) {
        if (state == BUTTON_EVENT_PRESSED) gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    if (state == BUTTON_EVENT_PRESSED) {
        if (gInputBoxIndex) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }

        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    } else if (gInputBoxIndex || state!=BUTTON_EVENT_HELD) {
        return;
    }

    if (!gEeprom.SET_NAV) {
        Step = -Step;
    }

    if (gAskToSave) {
        gRequestDisplayScreen = DISPLAY_FM;
        gFM_ChannelPosition   = NUMBER_AddWithWraparound(gFM_ChannelPosition, Step, 0, FM_CHANNELS_MAX - 1);
        return;
    }

    if (s_fmMenuMode != FM_MENU_NONE) {
        s_fmMenuMode = (s_fmMenuMode == FM_MENU_NAME) ? FM_MENU_DELETE : FM_MENU_NAME;
        gAskToDelete = (s_fmMenuMode == FM_MENU_DELETE);
        gRequestDisplayScreen = DISPLAY_FM;
        return;
    }

    if (gFM_ScanState != FM_SCAN_OFF) {
        if (gFM_AutoScan) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }

        FM_Tune(gEeprom.FM_FrequencyPlaying, Step, false);
        gRequestDisplayScreen = DISPLAY_FM;
        return;
    }

    if (gEeprom.FM_IsMrMode) {
        const uint8_t Channel = FM_FindNextChannel(gEeprom.FM_SelectedChannel + Step, Step);
        if (Channel == 0xFF || gEeprom.FM_SelectedChannel == Channel)
            goto Bail;

        gEeprom.FM_SelectedChannel  = Channel;
        gEeprom.FM_FrequencyPlaying = gFM_Channels[Channel];
    }
    else {
        uint16_t Frequency = gEeprom.FM_SelectedFrequency + Step;

        Frequency = FM_WrapFrequency(Frequency);

        gEeprom.FM_FrequencyPlaying  = Frequency;
        gEeprom.FM_SelectedFrequency = gEeprom.FM_FrequencyPlaying;
    }

    gRequestSaveFM = true;

Bail:
    BK1080_SetFrequency(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);

    gRequestDisplayScreen = DISPLAY_FM;
}

void FM_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    uint8_t state = bKeyPressed + 2 * bKeyHeld;

    if (s_fmNameEdit) {
        if (Key == KEY_MENU) { Key_MENU(state); return; }
        if (Key == KEY_EXIT) { Key_EXIT(state); return; }
        if (state == BUTTON_EVENT_SHORT && Key >= KEY_0 && Key <= KEY_9) { MSG_T9_HandleKey(&s_fmNameEditor, Key); gRequestDisplayScreen = DISPLAY_FM; return; }
        if (state == BUTTON_EVENT_LONG && Key >= KEY_0 && Key <= KEY_9) { MSG_T9_HandleLongKey(&s_fmNameEditor, Key); gRequestDisplayScreen = DISPLAY_FM; return; }
        if (state == BUTTON_EVENT_SHORT && (Key == KEY_STAR || Key == KEY_F)) { MSG_T9_HandleKey(&s_fmNameEditor, Key); gRequestDisplayScreen = DISPLAY_FM; return; }
        return;
    }

    if (s_fmAutoScanConfirm) {
        if (Key == KEY_EXIT) { Key_EXIT(state); return; }
        if ((Key == KEY_STAR && (state == BUTTON_EVENT_PRESSED || state == BUTTON_EVENT_SHORT || state == BUTTON_EVENT_HELD)) ||
            (Key == KEY_MENU && state == BUTTON_EVENT_SHORT)) {
            gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
            FM_StartAutoScanNow();
            gRequestDisplayScreen = DISPLAY_FM;
            return;
        }
        return;
    }

    switch (Key) {
        case KEY_0...KEY_9:
            Key_DIGITS(Key, state);
            break;
        case KEY_STAR:
            Key_FUNC(Key, state);
            break;
        case KEY_MENU:
            Key_MENU(state);
            break;
        case KEY_UP:
        case KEY_DOWN:
            Key_UP_DOWN(state, Key == KEY_UP ? 1 : -1);
            break;
        case KEY_EXIT:
            Key_EXIT(state);
            break;
        case KEY_F:
            GENERIC_Key_F(bKeyPressed, bKeyHeld);
            break;
        case KEY_PTT:
            GENERIC_Key_PTT(bKeyPressed);
            break;
        case KEY_SIDE1:
        case KEY_SIDE2:
            if (state != BUTTON_EVENT_PRESSED) {
                gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                HideFKeyIcon();
            }
            break;
        default:
            break;
    }
}

void FM_Play(void)
{
    if (!FM_CheckFrequencyLock(gEeprom.FM_FrequencyPlaying, BK1080_GetFreqLoLimit(gEeprom.FM_Band))) {
        if (!gFM_AutoScan) {
            gFmPlayCountdown_10ms = 0;
            gFM_FoundFrequency    = true;

            if (!gEeprom.FM_IsMrMode)
                gEeprom.FM_SelectedFrequency = gEeprom.FM_FrequencyPlaying;

            FM_AudioPathOn();

            GUI_SelectNextDisplay(DISPLAY_FM);
            return;
        }

        if (gFM_ChannelPosition < FM_CHANNELS_MAX) {
            gFM_Channels[gFM_ChannelPosition] = gEeprom.FM_FrequencyPlaying;
            FM_SetChannelDefaultName(gFM_ChannelPosition);
            gFM_ChannelPosition++;
        }

        if (gFM_ChannelPosition >= FM_CHANNELS_MAX) {
            FM_PlayAndUpdate();
            GUI_SelectNextDisplay(DISPLAY_FM);
            return;
        }
    }

    if (gFM_AutoScan && gEeprom.FM_FrequencyPlaying >= BK1080_GetFreqHiLimit(1))
        FM_PlayAndUpdate();
    else
        FM_Tune(gEeprom.FM_FrequencyPlaying, gFM_ScanState, false);

    GUI_SelectNextDisplay(DISPLAY_FM);
}

void FM_Start(void)
{
    gDualWatchActive          = false;
    gFmRadioMode              = true;
    gFM_ScanState             = FM_SCAN_OFF;
    gFM_RestoreCountdown_10ms = 0;

    FM_NamesLoad();

    BK1080_Init(gEeprom.FM_FrequencyPlaying, gEeprom.FM_Band/*, gEeprom.FM_Space*/);
    // Disable UHF LNA, enable VHF LNA
    BK4819_PickRXFilterPathBasedOnFrequency(10320000); // 103.2 MHz < 280 MHz

    FM_AudioPathOn();

    gUpdateStatus        = true;

    #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        gEeprom.CURRENT_STATE = 3;
        SETTINGS_WriteCurrentState();
    #endif
}

#endif
