/* Resident hardware/storage bridge for the Search overlay app. */
#include "app/scanner.h"
#include "apps/app_overlay.h"
#include "audio.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"

DCS_CodeType_t gScanCssResultType;
uint8_t gScanCssResultCode;
bool gScanSingleFrequency;
SCAN_SaveState_t gScannerSaveState;
uint16_t gScanChannel;
uint32_t gScanFrequency;
SCAN_CssState_t gScanCssState;
uint8_t gScanProgressIndicator;
bool gScanUseCssResult;

uint16_t SCANNER_OverlayDefaultChannel(void)
{
    const uint16_t ch = IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)
        ? gTxVfo->CHANNEL_SAVE : gEeprom.MrChannel[gEeprom.TX_VFO];
    return IS_MR_CHANNEL(ch) ? ch : 0u;
}

bool SCANNER_OverlayChannelInfo(uint16_t ch, char *name, uint8_t size)
{
    if (!IS_MR_CHANNEL(ch)) return false;
    const bool used = RADIO_CheckValidChannel(ch, false, gEeprom.TX_VFO);
    if (name && size) {
        name[0] = 0;
        if (used) SETTINGS_FetchChannelName(name, ch);
        name[size - 1u] = 0;
    }
    return used;
}

bool SCANNER_OverlaySave(uint32_t frequency, uint16_t tone, uint8_t toneType,
                         bool single, uint16_t ch)
{
    uint8_t code = 0xFFu;
    DCS_CodeType_t type;
    if (!frequency || !IS_MR_CHANNEL(ch)) return false;
    if (toneType == 1u) {
        code = DCS_GetCtcssCode(tone);
        type = CODE_TYPE_CONTINUOUS_TONE;
    } else if (toneType == 2u) {
        type = CODE_TYPE_DIGITAL;
        for (uint8_t i = 0; i < ARRAY_SIZE(DCS_Options); i++)
            if (DCS_Options[i] == tone) { code = i; break; }
    } else return false;
    if (code == 0xFFu) return false;

    const uint8_t power = gTxVfo->OUTPUT_POWER;
    STEP_Setting_t step = gTxVfo->STEP_SETTING;
    if (!single) {
        const uint32_t a = FREQUENCY_RoundToStep(frequency, 250u);
        const uint32_t b = FREQUENCY_RoundToStep(frequency, 625u);
        if ((frequency > a ? frequency - a : a - frequency) >
            (frequency > b ? frequency - b : b - frequency)) {
            frequency = b; step = STEP_6_25kHz;
        } else { frequency = a; step = STEP_2_5kHz; }
        RADIO_InitInfo(gTxVfo, ch, frequency);
        gTxVfo->Modulation = FREQUENCY_GetBand(frequency) == BAND2_108MHz
            ? MODULATION_AM : MODULATION_FM;
        gTxVfo->STEP_SETTING = step;
    } else {
        RADIO_ConfigureChannel(0, VFO_CONFIGURE_RELOAD);
        RADIO_ConfigureChannel(1, VFO_CONFIGURE_RELOAD);
    }
    gTxVfo->freq_config_RX.CodeType = type;
    gTxVfo->freq_config_RX.Code = code;
    gTxVfo->freq_config_TX = gTxVfo->freq_config_RX;
    gTxVfo->OUTPUT_POWER = power;
    gTxVfo->CHANNEL_BANDWIDTH = BANDWIDTH_NARROW;
    RADIO_ConfigureSquelchAndOutputPower(gTxVfo);
    gTxVfo->CHANNEL_SAVE = ch;
    gEeprom.MrChannel[gEeprom.TX_VFO] = ch;
    gEeprom.ScreenChannel[gEeprom.TX_VFO] = ch;
    gRequestSaveChannel = 2;
    gRequestSaveVFO = true;
    gVfoConfigureMode = VFO_CONFIGURE_RELOAD;
    return true;
}

void SCANNER_Start(bool single)
{
    const uint8_t shortcut = single ? APP_SHORTCUT_SEARCH_TONE : APP_SHORTCUT_SEARCH_FREQ;
    if (APP_LaunchOverlayShortcut(shortcut) != APP_OK)
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}
void SCANNER_Stop(void) {}
void SCANNER_TimeSlice10ms(void) {}
void SCANNER_TimeSlice500ms(void) {}
bool SCANNER_IsScanning(void) { return false; }
void SCANNER_ProcessKeys(KEY_Code_t key, bool pressed, bool held)
{ (void)key; (void)pressed; (void)held; }
void UI_DisplayScanner(void) { UI_DisplayClear(); }
