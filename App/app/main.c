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

#include <string.h>

#include "app/action.h"
#include "app/app.h"
#include "app/chFrScanner.h"
#include "app/common.h"
#ifdef ENABLE_FMRADIO
    #include "app/fm.h"
#endif
#include "app/generic.h"
#include "app/main.h"
#ifdef ENABLE_MESSENGER
    #include "app/messenger.h"
    #include "app/messenger_rf.h"
    #include "app/messenger_store.h"
#endif
#include "app/scanner.h"

#ifdef ENABLE_SPECTRUM
#include "app/spectrum.h"
#endif

#ifdef ENABLE_FEAT_F4HWN_GAME
#include "app/breakout.h"
#endif

#include "audio.h"
#include "board.h"
#include "driver/bk4819.h"
#include "driver/system.h"
#include "dtmf.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/ui.h"
#include <stdlib.h>

bool gCallToneTxActive = false;

// Full VFO backup for restore on EXIT
static VFO_Info_t gVfoBackup;
static uint16_t   gScreenChannelBackup = 0;
static uint16_t   gFreqChannelBackup = 0;

static void VFO_RestoreBackup(void) {
    if (gHasVfoBackup) {
        const uint8_t Vfo = gEeprom.TX_VFO;

        // Restore indices
        gEeprom.ScreenChannel[Vfo] = gScreenChannelBackup;
        gEeprom.FreqChannel[Vfo] = gFreqChannelBackup;

        // Restore full VFO
        memcpy(gTxVfo, &gVfoBackup, sizeof(VFO_Info_t));

        // Save and apply
        SETTINGS_SaveVfoIndices();
        RADIO_ConfigureSquelchAndOutputPower(gTxVfo);
        RADIO_SetupRegisters(true);

        gHasVfoBackup = false;
    }
}

static void toggle_chan_scanlist(void)
{   // toggle the selected channels scanlist setting

    if (SCANNER_IsScanning())
        return;

    if(!IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
#ifdef ENABLE_SCAN_RANGES
        CHFRSCANNER_ScanRange();
#endif
        return;
    }
    
    ChannelAttributes_t *att = MR_GetChannelAttributes(gTxVfo->CHANNEL_SAVE);

    // Remove exclude
    if(att->exclude == true)
    {
        att->exclude = false;
        MR_SaveChannelAttributesToFlash(gTxVfo->CHANNEL_SAVE, att);
    } 
    else 
    {
        uint8_t scanlist = gTxVfo->SCANLIST_PARTICIPATION;

        scanlist++;

        if (scanlist > MR_CHANNELS_LIST + 1)
            scanlist = 0;

        gTxVfo->SCANLIST_PARTICIPATION = scanlist;

        SETTINGS_UpdateChannel(gTxVfo->CHANNEL_SAVE, gTxVfo, true, true, true);
    }

    gVfoConfigureMode = VFO_CONFIGURE;
    gFlagResetVfos    = true;
}



typedef struct {
    uint16_t hz;
    uint8_t  on_10ms;
    uint8_t  off_10ms;
} CallToneNote_t;

// Five clearly different PMR-style melodies.  Total length is ~3.3-3.8 seconds.
static const CallToneNote_t gCallToneMelodies[5][16] = {
    // TONE1: classic dili-dili repeated
    { {1180,7,2},{1580,7,3},{1180,7,2},{1580,7,5}, {1180,7,2},{1580,7,3},{1180,7,2},{1580,7,5}, {1180,7,2},{1580,7,3},{1180,7,2},{1580,7,5}, {0,0,0},{0,0,0},{0,0,0},{0,0,0} },
    // TONE2: fast double-beep chirp pattern
    { {1800,4,1},{1800,4,4},{1350,4,1},{1350,4,7}, {1800,4,1},{1800,4,4},{1350,4,1},{1350,4,7}, {1800,4,1},{1800,4,4},{1350,4,1},{1350,4,7}, {1800,4,1},{1800,4,4},{1350,4,1},{1350,4,7} },
    // TONE3: rising three/four-note melody
    { {820,8,2},{1080,8,2},{1380,8,2},{1760,12,6}, {820,8,2},{1080,8,2},{1380,8,2},{1760,12,6}, {820,8,2},{1080,8,2},{1380,8,2},{1760,12,6}, {0,0,0},{0,0,0},{0,0,0},{0,0,0} },
    // TONE4: falling-answer melody
    { {1850,9,2},{1450,7,2},{1050,9,5},{1450,7,2}, {1850,9,2},{1450,7,2},{1050,9,5},{1450,7,2}, {1850,9,2},{1450,7,2},{1050,9,5},{1450,7,2}, {0,0,0},{0,0,0},{0,0,0},{0,0,0} },
    // TONE5: urgent long-short-long pattern
    { {980,16,3},{1650,5,2},{1650,5,8}, {980,16,3},{1650,5,2},{1650,5,8}, {980,16,3},{1650,5,2},{1650,5,8}, {980,16,3},{1650,5,2},{1650,5,8}, {0,0,0},{0,0,0},{0,0,0},{0,0,0} },
};

static uint16_t MAIN_ScaleToneFreq(uint16_t freq)
{
    return (uint16_t)((((uint32_t)freq * 1353245u) + (1u << 16)) >> 17);
}

static uint8_t MAIN_GetCallToneTxGain(void)
{
    /* Keep tone-generator gain fixed. CllVol must not affect local sidetone;
     * RF volume is tested only through a small temporary REG_40 deviation trim. */
    return 96;
}

static uint16_t MAIN_CallToneApplyDeviationForVolume(void)
{
    uint16_t saved = BK4819_ReadRegister(0x40);
    uint8_t vol = 1;
#ifdef ENABLE_MESSENGER
    vol = gMessengerConfig.call_vol;
#endif
    if (vol > 1u) vol = 1u;

    /* BK4829 REG_40 deviation trim only. Keep PA bias/RF power untouched.
     * LOW  = slightly below stock tone deviation for nearby/quiet use
     * HIGH = previous successful MID test level
     * Always restore immediately after CALLTX. */
    uint16_t tuning = saved & 0x0FFFu;
    if (vol == 0u) {
        tuning = (tuning > 0x0080u) ? (uint16_t)(tuning - 0x0080u) : 0u;
    } else {
        tuning = (uint16_t)((tuning + 0x0080u > 0x0FFFu) ? 0x0FFFu : tuning + 0x0080u);
    }

    const uint16_t patched = (uint16_t)((saved & 0xE000u) | 0x1000u | tuning);
    BK4819_WriteRegister(0x40, patched);
    return saved;
}

static void MAIN_CallToneRestoreDeviation(uint16_t saved)
{
    BK4819_WriteRegister(0x40, saved);
}

static void MAIN_SetQuietLocalMonitor(void)
{
    /* Lower only the local AF/speaker path.  REG_70 tone gain still controls
     * the generated tone level used by the transmitter. */
    BK4819_WriteRegister(BK4819_REG_48,
        (11u << 12) |      /* existing high field used by F4HWN */
        ( 0u << 10) |      /* AF Rx Gain-1 */
        ( 8u << 4)  |      /* AF Rx Gain-2: clearer local preview */
        ( 2u << 0));       /* AF DAC gain */
}

void MAIN_CancelCallTonePreview(void)
{
    /* GGFW 0.6.5: CllTon preview was removed. Keep a safe no-op so
     * stale menu transitions and tick hooks cannot affect audio/RF paths. */
}

void MAIN_CallToneTick10ms(void)
{
    /* No-op: CllTon preview removed. */
}

static void MAIN_SendCallToneNote(uint16_t hz, uint8_t on_10ms, uint8_t off_10ms)
{
    if (hz == 0 || on_10ms == 0) return;

    const uint8_t gain = MAIN_GetCallToneTxGain();
    BK4819_WriteRegister(BK4819_REG_71, MAIN_ScaleToneFreq(hz));
    BK4819_WriteRegister(BK4819_REG_70,
        BK4819_REG_70_ENABLE_TONE1 | ((uint16_t)(gain & 0x7fu) << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
    BK4819_ExitTxMute();
    SYSTEM_DelayMs((uint16_t)on_10ms * 10u);
    BK4819_EnterTxMute();
    if (off_10ms) {
        SYSTEM_DelayMs((uint16_t)off_10ms * 10u);
    }
}

static void MAIN_SendPmrCallTone(void)
{
    // F+9 PMR-style RF call tone. Long-press 9 keeps the original 1-CALL channel shortcut.
    if (gCurrentFunction == FUNCTION_TRANSMIT || FUNCTION_IsRx()) {
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    uint8_t tone = 0;
#ifdef ENABLE_MESSENGER
    MSG_STORE_Init();
    tone = gMessengerConfig.call_tone;
#endif
    if (tone > 4u) tone = 0;

    MAIN_CancelCallTonePreview();

#ifdef ENABLE_MESSENGER
    MSG_RF_HardRestoreVoicePath();
#endif

    gCallToneTxActive = true;
    RADIO_PrepareTX();
    if (gCurrentFunction != FUNCTION_TRANSMIT) {
        gCallToneTxActive = false;
        return;
    }

    gUpdateStatus = true;
    gUpdateDisplay = true;
    GUI_DisplayScreen();

    BK4819_DisableScramble();
    BK4819_EnterTxMute();

    /* Clean single-tone TX path with quiet local monitor. */
    BK4819_SetAF(BK4819_AF_BEEP);
    MAIN_SetQuietLocalMonitor();
    AUDIO_AudioPathOn();
    gEnableSpeaker = true;
    BK4819_WriteRegister(BK4819_REG_70, 0x0000);
    BK4819_EnableTXLink();
    SYSTEM_DelayMs(80); // let TX/tone path settle before first tone

    const uint16_t saved_reg40 = MAIN_CallToneApplyDeviationForVolume();

    /* Send for a full ~3 seconds. A single melody pass can be only around
     * 1.5-2.0s depending on the selected tone, so repeat the melody until the
     * requested call-tone duration is reached. */
    uint16_t elapsed = 0;
    while (elapsed < 3000u) {
        bool played_any = false;
        for (uint8_t i = 0; i < 16u && elapsed < 3000u; ++i) {
            const CallToneNote_t *n = &gCallToneMelodies[tone][i];
            if (n->hz == 0) break;
            MAIN_SendCallToneNote(n->hz, n->on_10ms, n->off_10ms);
            elapsed += (uint16_t)((uint16_t)n->on_10ms + (uint16_t)n->off_10ms) * 10u;
            played_any = true;
        }
        if (!played_any) break;
    }

    MAIN_CallToneRestoreDeviation(saved_reg40);

    AUDIO_AudioPathOff();
    gEnableSpeaker = false;
    BK4819_EnterTxMute();
    BK4819_WriteRegister(BK4819_REG_70, 0x0000);
    BK4819_SetRxAudioGain();
    APP_HandleEndTransmission();
    gCallToneTxActive = false;

    RADIO_SetVfoState(VFO_STATE_NORMAL);
    gRequestDisplayScreen = DISPLAY_MAIN;
    gUpdateStatus = true;
    gUpdateDisplay = true;
}

void MAIN_SendPmrCallToneAction(void)
{
    MAIN_SendPmrCallTone();
}

static void processFKeyFunction(const KEY_Code_t Key, const bool beep)
{
    uint8_t Vfo = gEeprom.TX_VFO;

    if (beep)
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
    if(gEeprom.MENU_LOCK == true && Key != 2) {
        HideFKeyIcon();

        return;
    }
#endif

    if (gScreenToDisplay == DISPLAY_MENU) {
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    switch (Key) {
        case KEY_0:
            #ifdef ENABLE_FMRADIO
                ACTION_FM();
            #endif
            break;

        case KEY_1:
            if (!IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
                HideFKeyIcon();

#ifdef ENABLE_COPY_CHAN_TO_VFO
                if (!gEeprom.VFO_OPEN || gCssBackgroundScan) {
                    gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                    return;
                }

                if (gScanStateDir != SCAN_OFF) {
                    if (gCurrentFunction != FUNCTION_INCOMING ||
                        gRxReceptionMode == RX_MODE_NONE      ||
                        gScanPauseDelayIn_10ms == 0)
                    {   // scan is running (not paused)
                        return;
                    }
                }

                const uint8_t vfo = gEeprom.TX_VFO;

                if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo]))
                {   // copy channel to VFO, then swap to the VFO

                    gEeprom.ScreenChannel[vfo] = FREQ_CHANNEL_FIRST + gEeprom.VfoInfo[vfo].Band;
                    gEeprom.VfoInfo[vfo].CHANNEL_SAVE = gEeprom.ScreenChannel[vfo];

                    RADIO_SelectVfos();
                    RADIO_ApplyOffset(gRxVfo);
                    RADIO_ConfigureSquelchAndOutputPower(gRxVfo);
                    RADIO_SetupRegisters(true);

                    //SETTINGS_SaveChannel(channel, gEeprom.RX_VFO, gRxVfo, 1);

                    gRequestSaveChannel = 1;
                    gRequestSaveVFO = true;
                    gUpdateDisplay = true;
                }
#endif
                return;
            }

#ifdef ENABLE_WIDE_RX
            if(gTxVfo->Band == BAND7_470MHz && gTxVfo->pRX->Frequency < _1GHz_in_KHz) {
                    gTxVfo->pRX->Frequency = _1GHz_in_KHz;
                    return;
            }
#endif
            gTxVfo->Band += 1;

            if (gTxVfo->Band == BAND5_350MHz && !gSetting_350EN) {
                // skip if not enabled
                gTxVfo->Band += 1;
            } else if (gTxVfo->Band >= BAND_N_ELEM){
                // go arround if overflowed
                gTxVfo->Band = BAND1_50MHz;
            }

            gEeprom.ScreenChannel[Vfo] = FREQ_CHANNEL_FIRST + gTxVfo->Band;
            gEeprom.FreqChannel[Vfo]   = FREQ_CHANNEL_FIRST + gTxVfo->Band;

            gRequestSaveVFO            = true;
            gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;

            gRequestDisplayScreen      = DISPLAY_MAIN;

            break;

        case KEY_2:
            #ifdef ENABLE_FEAT_F4HWN
                gVfoConfigureMode     = VFO_CONFIGURE;
            #endif
            COMMON_SwitchVFOs();
            break;

        case KEY_3:
            #ifdef ENABLE_FEAT_F4HWN
                gVfoConfigureMode     = VFO_CONFIGURE;
            #endif
            COMMON_SwitchVFOMode();

            break;

        case KEY_4:
            HideFKeyIcon();

            gBackup_CROSS_BAND_RX_TX  = gEeprom.CROSS_BAND_RX_TX;
            gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;     

            if (!gSurvivalMode) {
                SCANNER_Start(false);
                gRequestDisplayScreen = DISPLAY_SCANNER;
            } else {
                gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                gEeprom.CROSS_BAND_RX_TX = gBackup_CROSS_BAND_RX_TX;
            }
            break;

        case KEY_5:
            if(!beep) {
#ifdef ENABLE_NOAA
                if (!IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
                    gEeprom.ScreenChannel[Vfo] = gEeprom.NoaaChannel[gEeprom.TX_VFO];
                }
                else {
                    gEeprom.ScreenChannel[Vfo] = gEeprom.MrChannel[gEeprom.TX_VFO];
#ifdef ENABLE_VOICE
                        gAnotherVoiceID = VOICE_ID_CHANNEL_MODE;
#endif
                }
                gRequestSaveVFO   = true;
                gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
#elif defined(ENABLE_SPECTRUM)
                if (!gSurvivalMode) APP_RunSpectrum(); else gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                gRequestDisplayScreen = DISPLAY_MAIN;
#endif
            }
            else {
                toggle_chan_scanlist();
            }

            break;

        case KEY_6:
            ACTION_Power();
            break;

        case KEY_7:
#ifdef ENABLE_MESSENGER
            if (!beep) {
                if (!gSurvivalMode) MSG_RangeOpen();
                else gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            } else
#endif
            {
#ifdef ENABLE_VOX
                ACTION_Vox();
#endif
            }

            break;

        case KEY_8:
            if (!beep) {
                ACTION_BackLightOnDemand(); 
            }
            else {
                gTxVfo->FrequencyReverse = gTxVfo->FrequencyReverse == false;
                gRequestSaveChannel = 1;
            }

            break;

        case KEY_9:
            if (!beep) {
                MAIN_SendPmrCallTone();
            }
            else {
                if (RADIO_CheckValidChannel(gEeprom.CHAN_1_CALL, false, 0)) {
                    gEeprom.MrChannel[Vfo]     = gEeprom.CHAN_1_CALL;
                    gEeprom.ScreenChannel[Vfo] = gEeprom.CHAN_1_CALL;
#ifdef ENABLE_VOICE
                    AUDIO_SetVoiceID(0, VOICE_ID_CHANNEL_MODE);
                    AUDIO_SetDigitVoice(1, gEeprom.CHAN_1_CALL + 1);
                    gAnotherVoiceID        = (VOICE_ID_t)0xFE;
#endif
                    gRequestSaveVFO            = true;
                    gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
                }
            }

            break;

#ifdef ENABLE_FEAT_F4HWN // Set Squelch F + UP or Down and Step F + SIDE1 or F + SIDE2
        case KEY_UP:
        case KEY_DOWN:
            {
                bool isKeyUp = (Key == KEY_UP);

                if (gScanStateDir != SCAN_OFF) {
                    RADIO_NextValidList(isKeyUp ? 1 : -1);
                    UI_MAIN_NotifyScanProgressDataChanged();
                } else {
                    // Adjust squelch: UP increments, DOWN decrements
                    if (gSquelchLevelOriginal == 10)
                        gSquelchLevelOriginal =  gEeprom.SQUELCH_LEVEL;

                    if (isKeyUp) {
                        if (gEeprom.SQUELCH_LEVEL < 9) gEeprom.SQUELCH_LEVEL++;
                    } else {
                        if (gEeprom.SQUELCH_LEVEL > 0) gEeprom.SQUELCH_LEVEL--;
                    }
                    gVfoConfigureMode = VFO_CONFIGURE;
                }

                gWasFKeyPressed = false;

                break;
            }
        case KEY_SIDE1:
        case KEY_SIDE2:
            {
                bool isKeySide1 = (Key == KEY_SIDE1);
                uint8_t idx = FREQUENCY_GetSortedIdxFromStepIdx(gTxVfo->STEP_SETTING);

                if ((isKeySide1 && idx < STEP_N_ELEM - 1) || (!isKeySide1 && idx > 0)) 
                {
                    gTxVfo->STEP_SETTING = FREQUENCY_GetStepIdxFromSortedIdx(idx + (isKeySide1 ? 1 : -1));
                    
                    if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
                        gRequestSaveChannel = 1;
                    }
                    gVfoConfigureMode = VFO_CONFIGURE;
                }
                
                gWasFKeyPressed = false;

                break;
            }
#endif

        default:
            HideFKeyIcon();

            if (beep)
                gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
            break;
    }
}

void channelMove(uint16_t Channel)
{
    const uint8_t Vfo = gEeprom.TX_VFO;

    if (!RADIO_CheckValidChannel(Channel, false, 0)) {
        if (gKeyInputCountdown <= 1) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        }

        return;
    }

    gBeepToPlay = BEEP_NONE;

    #ifdef ENABLE_VOICE
        gAnotherVoiceID        = (VOICE_ID_t)Key;
    #endif

    gEeprom.MrChannel[Vfo]     = (uint16_t)Channel;
    gEeprom.ScreenChannel[Vfo] = (uint16_t)Channel;
    //gRequestSaveVFO            = true;
    gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;

#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
    gRemoveOffset = false;
    gPowerHigh = false;
#endif

    RADIO_ConfigureChannel(gEeprom.TX_VFO, gVfoConfigureMode);

    SETTINGS_SaveVfoIndices();
    
    return;
}

void channelMoveSwitch(void) {
    if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) { // user is entering channel number
        uint16_t Channel = 0;

        /*
        switch (gInputBoxIndex)
        {
            case 1:
                Channel = gInputBox[0];
                break;
            case 2:
                Channel = (gInputBox[0] * 10) + gInputBox[1];
                break;
            case 3:
                Channel = (gInputBox[0] * 100) + (gInputBox[1] * 10) + gInputBox[2];
                break;
        }
        */

        for (uint8_t i = 0; i < gInputBoxIndex; i++) {
            Channel = (Channel * 10) + gInputBox[i];
        }

        if ((Channel == 0) && (gInputBoxIndex != 4)) {
            return;
        }

        if (gInputBoxIndex == 4) {
            gInputBoxIndex = 0;
            gKeyInputCountdown = 1;
        }

        channelMove(Channel - 1);
        SETTINGS_SaveVfoIndices();
    }
}

static void MAIN_Key_DIGITS(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
    if (bKeyHeld) { // key held down
        if (bKeyPressed) {
            if (gScreenToDisplay == DISPLAY_MAIN) {
                if (gInputBoxIndex > 0) { // delete any inputted chars
                    gInputBoxIndex        = 0;
                    gRequestDisplayScreen = DISPLAY_MAIN;
                }

                HideFKeyIcon();

                processFKeyFunction(Key, true);
            }
        }
        return;
    }

    if (bKeyPressed)
    {   // key is pressed
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;  // beep when key is pressed
        return;                                 // don't use the key till it's released
    }

    if (!gWasFKeyPressed) { // F-key wasn't pressed

        if (gScanStateDir != SCAN_OFF){
            /*
            switch(Key) {
                case KEY_0:
                    gEeprom.SCAN_LIST_DEFAULT = MR_CHANNELS_LIST + 1;
                    #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
                        SETTINGS_WriteCurrentState();
                    #endif
                    break;
                case KEY_1...KEY_9:
                    gEeprom.SCAN_LIST_DEFAULT = Key;
                    #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
                        SETTINGS_WriteCurrentState();
                    #endif
                    break;
                default:
                    break;
            }
            return;
            */

            INPUTBOX_Append(Key);

            /* Wait until exactly two digits are entered */
            if (gInputBoxIndex < 2)
                return;

            /* Two digits entered */
            gInputBoxIndex = 0;

            uint8_t value = (gInputBox[0] * 10) + gInputBox[1];

            /* 00 = ALL scan lists */
            if (value == 0)
            {
                gEeprom.SCAN_LIST_DEFAULT = MR_CHANNELS_LIST + 1;
                UI_MAIN_NotifyScanProgressDataChanged();
            #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
                SETTINGS_WriteCurrentState();
            #endif
                return;
            }

            /* 01 .. MR_CHANNELS_LIST */
            if (value <= MR_CHANNELS_LIST)
            {
                gEeprom.SCAN_LIST_DEFAULT = value;

                if (!RADIO_CheckValidList(value))
                {
                    /* Requested scan list is empty or invalid:
                        jump to the next valid scan list */
                    gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                    RADIO_NextValidList(1);
                }
                UI_MAIN_NotifyScanProgressDataChanged();

            #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
                SETTINGS_WriteCurrentState();
            #endif
            }

            return;
        }

        const uint8_t Vfo = gEeprom.TX_VFO;

        // Save full VFO state BEFORE first digit
        if (gInputBoxIndex == 0 && IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
            memcpy(&gVfoBackup, gTxVfo, sizeof(VFO_Info_t));
            gScreenChannelBackup = gEeprom.ScreenChannel[Vfo];
            gFreqChannelBackup = gEeprom.FreqChannel[Vfo];
            gHasVfoBackup = true;
        }

        INPUTBOX_Append(Key);
        gKeyInputCountdown = key_input_timeout_500ms;

        channelMoveSwitch();

        gRequestDisplayScreen = DISPLAY_MAIN;

        if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) { // user is entering channel number

            gKeyInputCountdown = (key_input_timeout_500ms / 4); // short time...

            #ifdef ENABLE_VOICE
                gAnotherVoiceID   = (VOICE_ID_t)Key;
            #endif
            
            return;
        }

//      #ifdef ENABLE_NOAA
//          if (!IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE))
//      #endif
        if (IS_FREQ_CHANNEL(gTxVfo->CHANNEL_SAVE))
        {   // user is entering a frequency

#ifdef ENABLE_VOICE
            gAnotherVoiceID = (VOICE_ID_t)Key;
#endif
            uint8_t totalDigits = 6; // by default frequency is lower than 1 GHz
            if (gTxVfo->pRX->Frequency >= _1GHz_in_KHz) {
                totalDigits = 7; // if frequency is uppen than GHz
            }

            if (gInputBoxIndex == 0) {
                // do nothing
                return;
            }
            
            gKeyInputCountdown = key_input_timeout_500ms / (gInputBoxIndex >= totalDigits ? 16 : 3);

            if (gInputBoxIndex > totalDigits) {
                gInputBoxIndex = totalDigits;

                return;
            }

            const char *inputStr = INPUTBOX_GetAscii();
            uint8_t inputLength = gInputBoxIndex;

            // convert to int
            uint32_t inputFreq = StrToUL(inputStr);

            // add missing zero
            for (uint8_t i = 0; i < totalDigits - inputLength; i++) {
                inputFreq *= 10;
            }

            uint32_t Frequency = inputFreq * 100;

            // clamp the frequency entered to some valid value
            if (Frequency < frequencyBandTable[0].lower) {
                Frequency = frequencyBandTable[0].lower;
            }
            else if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower) {
                const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
                Frequency = (Frequency < center) ? BX4819_band1.upper : BX4819_band2.lower;
            }
            else if (Frequency > frequencyBandTable[BAND_N_ELEM - 1].upper) {
                Frequency = frequencyBandTable[BAND_N_ELEM - 1].upper;
            }

            const FREQUENCY_Band_t band = FREQUENCY_GetBand(Frequency);

            if (gTxVfo->Band != band) {
                gTxVfo->Band               = band;
                gEeprom.ScreenChannel[Vfo] = band + FREQ_CHANNEL_FIRST;
                gEeprom.FreqChannel[Vfo]   = band + FREQ_CHANNEL_FIRST;

                SETTINGS_SaveVfoIndices();

                RADIO_ConfigureChannel(Vfo, VFO_CONFIGURE_RELOAD);
            }

            Frequency = FREQUENCY_RoundToStep(Frequency, gTxVfo->StepFrequency);

            if (Frequency >= BX4819_band1.upper && Frequency < BX4819_band2.lower)
            {   // clamp the frequency to the limit
                const uint32_t center = (BX4819_band1.upper + BX4819_band2.lower) / 2;
                Frequency = (Frequency < center) ? BX4819_band1.upper - gTxVfo->StepFrequency : BX4819_band2.lower;
            }

            gTxVfo->freq_config_RX.Frequency = Frequency;

            gRequestSaveChannel = 1;
            return;

        }
        #ifdef ENABLE_NOAA
            else
            if (IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE))
            {   // user is entering NOAA channel
                if (gInputBoxIndex != 2) {
                    #ifdef ENABLE_VOICE
                        gAnotherVoiceID   = (VOICE_ID_t)Key;
                    #endif
                    gRequestDisplayScreen = DISPLAY_MAIN;
                    return;
                }

                gInputBoxIndex = 0;
                gHasVfoBackup = false;

                uint8_t Channel = (gInputBox[0] * 10) + gInputBox[1];
                if (Channel >= 1 && Channel <= ARRAY_SIZE(NoaaFrequencyTable)) {
                    Channel                   += NOAA_CHANNEL_FIRST;
                    #ifdef ENABLE_VOICE
                        gAnotherVoiceID        = (VOICE_ID_t)Key;
                    #endif
                    gEeprom.NoaaChannel[Vfo]   = Channel;
                    gEeprom.ScreenChannel[Vfo] = Channel;
                    gRequestSaveVFO            = true;
                    gVfoConfigureMode          = VFO_CONFIGURE_RELOAD;
                    return;
                }
            }
        #endif

        gRequestDisplayScreen = DISPLAY_MAIN;
        gBeepToPlay           = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    HideFKeyIcon();

    processFKeyFunction(Key, false);
}

static void MAIN_Key_EXIT(bool bKeyPressed, bool bKeyHeld)
{
    if (!bKeyHeld && bKeyPressed) { // exit key pressed
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;  // beep when key is pressed
        return;                                 // don't use the key till it's released
    }

    if (bKeyHeld) { // exit key held down
        if (bKeyPressed) {
            // Restore full VFO state on long press EXIT
            VFO_RestoreBackup();

            gRequestDisplayScreen = DISPLAY_MAIN;
        }

        return;
    }

#ifdef ENABLE_DTMF_CALLING
    if (gDTMF_CallState != DTMF_CALL_STATE_NONE && gCurrentFunction != FUNCTION_TRANSMIT)
    {   // clear CALL mode being displayed
        gDTMF_CallState = DTMF_CALL_STATE_NONE;
        gUpdateDisplay  = true;
        return;
    }
#endif

#ifdef ENABLE_FMRADIO
    if (!gFmRadioMode)
#endif
    {
        if (gScanStateDir == SCAN_OFF) {
            if (gInputBoxIndex == 0)
                return;
            gInputBox[--gInputBoxIndex] = 10;

            // Restore full VFO state when back to 0
            if (gInputBoxIndex == 0)
                VFO_RestoreBackup();

            gKeyInputCountdown = key_input_timeout_500ms;
            channelMoveSwitch();

#ifdef ENABLE_VOICE
            if (gInputBoxIndex == 0)
                gAnotherVoiceID = VOICE_ID_CANCEL;
#endif
        }
        else {
            gScanKeepResult = false;
            gInputBoxIndex = 0;
            CHFRSCANNER_Stop();

#ifdef ENABLE_VOICE
            gAnotherVoiceID = VOICE_ID_SCANNING_STOP;
#endif
        }

        gRequestDisplayScreen = DISPLAY_MAIN;
        return;
    }

#ifdef ENABLE_FMRADIO
    ACTION_FM();
#endif
    return;
}

static void MAIN_Key_MENU(bool bKeyPressed, bool bKeyHeld)
{
#ifdef ENABLE_MESSENGER
    if (!bKeyHeld && !bKeyPressed && gWasFKeyPressed) {
        gWasFKeyPressed = false;
        if (!gSurvivalMode) {
            MSG_Open();
            gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        } else {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        }
        return;
    }
#endif

    if (bKeyPressed && !bKeyHeld) // menu key pressed
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;

    if (bKeyHeld) { // menu key held down (long press)
        if (bKeyPressed) { // long press MENU key

            #ifdef ENABLE_FEAT_F4HWN
            // Exclude current scan entry
            if(gScanStateDir != SCAN_OFF)
            {
                if(FUNCTION_IsRx() || gScanPauseDelayIn_10ms > 9)
                {
#ifdef ENABLE_SCAN_RANGES
                    if(gScanRangeStart && !IS_MR_CHANNEL(gNextMrChannel))
                    {
                        if(CHFRSCANNER_ExcludeCurrentScanRange())
                        {
                            UI_MAIN_NotifyScanProgressDataChanged();
                            lastFoundFrqOrChan = lastFoundFrqOrChanOld;
                            CHFRSCANNER_ContinueScanning();
                        }

                        return;
                    }
#endif

                    ChannelAttributes_t *att = MR_GetChannelAttributes(lastFoundFrqOrChan);
                    att->exclude = true;

                    MR_SaveChannelAttributesToFlash(lastFoundFrqOrChan, att);
                    UI_MAIN_NotifyScanProgressDataChanged();

                    gVfoConfigureMode = VFO_CONFIGURE;
                    gFlagResetVfos    = true;

                    lastFoundFrqOrChan = lastFoundFrqOrChanOld;

                    CHFRSCANNER_ContinueScanning();
                }

                return;
            }
            #endif
            
            gWasFKeyPressed = false;

            if (gScreenToDisplay == DISPLAY_MAIN) {
                if (gInputBoxIndex > 0) { // delete any inputted chars
                    gInputBoxIndex        = 0;
                    gRequestDisplayScreen = DISPLAY_MAIN;
                }

                gUpdateStatus   = true;

                ACTION_Handle(KEY_MENU, bKeyPressed, bKeyHeld);
            }
        }

        return;
    }

    if (!bKeyPressed && !gDTMF_InputMode) { // menu key released
        gKeyInputCountdown = 1;
        channelMoveSwitch();

        const bool bFlag = !gInputBoxIndex;
        gInputBoxIndex   = 0;

        if (bFlag) {
            if (gScanStateDir != SCAN_OFF) {
                CHFRSCANNER_Stop();
                return;
            }

            #ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
                if(gEeprom.MENU_LOCK == true) {
                    HideFKeyIcon();

                    return;
                }
            #endif

            gFlagRefreshSetting = true;
            gRequestDisplayScreen = DISPLAY_MENU;
            #ifdef ENABLE_VOICE
                gAnotherVoiceID   = VOICE_ID_MENU;
            #endif
        }
        else {
            gRequestDisplayScreen = DISPLAY_MAIN;
        }
    }
}

static void MAIN_Key_STAR(bool bKeyPressed, bool bKeyHeld)
{
    if (gCurrentFunction == FUNCTION_TRANSMIT)
        return;
    
    if (gInputBoxIndex) {
        if (!bKeyHeld && bKeyPressed)
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    if (!bKeyHeld && bKeyPressed) { // star key pressed
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;  // beep when key is pressed
        return;                                 // don't use the key till it's released
    }

    #ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
        if(gEeprom.MENU_LOCK == true) {
            HideFKeyIcon();

            return; // prevent F function if MENU LOCK is true
        }
    #endif

    if (bKeyHeld && !gWasFKeyPressed){ // long press
        if (!bKeyPressed) // released
            return; 

        /*
        #ifdef ENABLE_FEAT_F4HWN_RESUME_STATE
        if(gScanRangeStart == 0) // No ScanRange
        {
            gEeprom.CURRENT_STATE = 1;
        }
        else // ScanRange
        {
            gEeprom.CURRENT_STATE = 2;
        }
        SETTINGS_WriteCurrentState();
        #endif
        */
        ACTION_Scan(false);// toggle scanning

        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
        return;
    }
    
    if (!gWasFKeyPressed) // pressed without the F-key
    {   
        if (gScanStateDir == SCAN_OFF 
#ifdef ENABLE_NOAA
            && !IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE)
#endif
#ifdef ENABLE_SCAN_RANGES
            && gScanRangeStart == 0
#endif      
        )
        {   // start entering a DTMF string
            memcpy(gDTMF_InputBox, gDTMF_String, MIN(sizeof(gDTMF_InputBox), sizeof(gDTMF_String) - 1));
            gDTMF_InputBox_Index  = 0;
            gDTMF_InputMode       = true;

            gKeyInputCountdown    = key_input_timeout_500ms;

            gRequestDisplayScreen = DISPLAY_MAIN;
        }
        else
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
    }
    else
    {   // with the F-key
        gWasFKeyPressed = false;

#ifdef ENABLE_NOAA
        if (IS_NOAA_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            return;
        }               
#endif
        // scan the CTCSS/DCS code
        gBackup_CROSS_BAND_RX_TX  = gEeprom.CROSS_BAND_RX_TX;
        gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;

        if (!gSurvivalMode) {
            SCANNER_Start(true);
            gRequestDisplayScreen = DISPLAY_SCANNER;
        } else {
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            gEeprom.CROSS_BAND_RX_TX = gBackup_CROSS_BAND_RX_TX;
        }
    }
    
    //gPttWasReleased = true; Fixed issue #138
    gUpdateStatus   = true;
}

static void MAIN_Key_UP_DOWN(bool bKeyPressed, bool bKeyHeld, int8_t Direction)
{
    if (!gEeprom.SET_NAV) {
        Direction = -Direction;
    }

#ifdef ENABLE_FEAT_F4HWN // Set Squelch F + UP or Down
    if(gWasFKeyPressed) {
        processFKeyFunction(Direction == 1 ? KEY_UP : KEY_DOWN, true);
        return;
    }
#endif

#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
    gRemoveOffset = false;
    gPowerHigh = false;
#endif

    uint16_t Channel = gEeprom.ScreenChannel[gEeprom.TX_VFO];

    if (gInputBoxIndex > 0) {
        gInputBoxIndex = 0;
        gHasVfoBackup = false;
    }

    if (bKeyHeld || !bKeyPressed) { // key held or released
        if (!bKeyPressed) {
            if (!bKeyHeld || IS_FREQ_CHANNEL(Channel))
                return;
            // if released long button press and not in freq mode
#ifdef ENABLE_VOICE
            AUDIO_SetDigitVoice(0, gTxVfo->CHANNEL_SAVE + 1); // say channel number
            gAnotherVoiceID = (VOICE_ID_t)0xFE;
#endif
            return;
        }
    }
    else { // short pressed
        gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
    }

    if (gScanStateDir == SCAN_OFF) {
#ifdef ENABLE_NOAA
        if (!IS_NOAA_CHANNEL(Channel))
#endif
        {
            uint16_t Next;
            if (IS_FREQ_CHANNEL(Channel)) { // step/down in frequency
                const uint32_t frequency = APP_SetFrequencyByStep(gTxVfo, Direction);

                if (RX_freq_check(frequency) < 0) { // frequency not allowed
                    gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
                    return;
                }
                gTxVfo->freq_config_RX.Frequency = frequency;
                BK4819_SetFrequency(frequency);
                BK4819_RX_TurnOn();
                gRequestSaveChannel = 1;
                return;
            }

            Next = RADIO_FindNextChannel(Channel + Direction, Direction, false, 0);
            if (Next == 0xFFFF)
                return;
            if (Channel == Next)
                return;
            gEeprom.MrChannel[gEeprom.TX_VFO] = Next;
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = Next;

            if (!bKeyHeld) {
#ifdef ENABLE_VOICE
                AUDIO_SetDigitVoice(0, Next + 1);
                gAnotherVoiceID = (VOICE_ID_t)0xFE;
#endif
            }
        }
#ifdef ENABLE_NOAA
        else {
            Channel = NOAA_CHANNEL_FIRST + NUMBER_AddWithWraparound(gEeprom.ScreenChannel[gEeprom.TX_VFO] - NOAA_CHANNEL_FIRST, Direction, 0, 9);
            gEeprom.NoaaChannel[gEeprom.TX_VFO] = Channel;
            gEeprom.ScreenChannel[gEeprom.TX_VFO] = Channel;
        }
#endif

        gRequestSaveVFO   = true;
        gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
        return;
    }

    // jump to the next channel
    CHFRSCANNER_ManualResume(Direction);

    gPttWasReleased = true;
}

void MAIN_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
#ifdef ENABLE_FMRADIO
    if (gFmRadioMode && Key != KEY_PTT && Key != KEY_EXIT) {
        if (!bKeyHeld && bKeyPressed)
            gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }
#endif

    if (gDTMF_InputMode && bKeyPressed && !bKeyHeld) {
        const char Character = DTMF_GetCharacter(Key);
        if (Character != 0xFF)
        {   // add key to DTMF string
            DTMF_Append(Character);
            gKeyInputCountdown    = key_input_timeout_500ms;
            gRequestDisplayScreen = DISPLAY_MAIN;
            gPttWasReleased       = true;
            gBeepToPlay           = BEEP_1KHZ_60MS_OPTIONAL;
            return;
        }
    }

    // TODO: ???
//  if (Key > KEY_PTT)
//  {
//      Key = KEY_SIDE2;      // what's this doing ???
//  }

    switch (Key) {
#ifdef ENABLE_FEAT_F4HWN
        case KEY_SIDE1:
        case KEY_SIDE2:
#endif
        case KEY_0...KEY_9:
            MAIN_Key_DIGITS(Key, bKeyPressed, bKeyHeld);
            break;
        case KEY_MENU:
            MAIN_Key_MENU(bKeyPressed, bKeyHeld);
            break;
        case KEY_UP:
        case KEY_DOWN:
            MAIN_Key_UP_DOWN(bKeyPressed, bKeyHeld, Key == KEY_UP ? 1 : -1);
            break;
        case KEY_EXIT:
            MAIN_Key_EXIT(bKeyPressed, bKeyHeld);
            break;
        case KEY_STAR:
            MAIN_Key_STAR(bKeyPressed, bKeyHeld);
            break;
        case KEY_F:
            GENERIC_Key_F(bKeyPressed, bKeyHeld);
            break;
        case KEY_PTT:
            GENERIC_Key_PTT(bKeyPressed);
            break;
        default:
            if (!bKeyHeld && bKeyPressed)
                gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
            break;
    }
}
