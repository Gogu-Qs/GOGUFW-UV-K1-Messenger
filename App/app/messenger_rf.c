#include <string.h>

#include "app/messenger_rf.h"
#include "app/messenger_packet.h"
#include "app/messenger_store.h"
#include "app/messenger.h"
#include "app/aircopy.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/bk4819-regs.h"
#include "driver/system.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"

#ifdef ENABLE_AIRCOPY
extern uint8_t gFSKWriteIndex;
#endif

/*
 * GOGUFW 0.3.3 next-fix build based on 0.3.2.
 *
 * This build is intentionally conservative:
 * - Messenger UI enter/exit does not start/stop RF.
 * - REG_47 is never changed for FSK RX; normal AF output is preserved.
 * - No RX-start PrepareFSKReceive() call. That caused voice ticks and cut FSK bursts.
 * - Sidecar FSK RX is armed only while the radio is idle, with exact Aircopy RX values.
 * - A valid voice snapshot is captured before any sidecar/TX register write.
 * - PTT and message-TX both run the 8G-style hard restore before normal voice TX.
 */

#define MSG_RF_WORDS                 50u
#define MSG_RF_BYTES_CARRIED         MSG_PKT_WIRE_LEN
#define MSG_RF_TEXT_LIMIT            MSG_TEXT_LEN
#define MSG_RF_POST_TX_RESTORE_TICKS 10u
#define MSG_RF_REARM_DELAY_TICKS     80u
#define MSG_RF_RX_STALE_TICKS        120u
#define MSG_RF_ACK_TIMEOUT_TICKS      400u  /* 4.0s: wait past delayed ACK before retry */
#define MSG_RF_ACK_COLLECT_TICKS      250u  /* 2.5s: after first ACK, keep RX locked to collect more ACK sources */
#define MSG_RF_ACK_RETRY_GAP_TICKS    50u
#define MSG_RF_ACK_SEND_DELAY_MIN_TICKS  80u   /* 800ms compatibility-safe ACK delay min */
#define MSG_RF_ACK_SEND_DELAY_JIT_TICKS 220u   /* +0..2200ms random ACK jitter; ACK window = 0.8..3.0s */
#define MSG_RF_REPRIME_DELAY_TICKS     20u   /* 200ms after TX/RX end */
#define MSG_RF_IDLE_REPRIME_TICKS      0u    /* RF28: periodic idle re-prime disabled; event-based only */
#define MSG_RF_UNREAD_LED_PERIOD_TICKS 300u   /* 3 seconds */
#define MSG_RF_UNREAD_LED_FLASH_TICKS  8u     /* 80 ms */
#define MSG_RF_UNREAD_LED_GAP_TICKS    14u    /* 140 ms between double flashes */
#define MSG_RF_RANGE_PONG_MIN_TICKS     300u   /* 0.5.18: 3000 ms; wait until 3x PING repeats are safely over */
#define MSG_RF_RANGE_PONG_JIT_TICKS     500u   /* +0..5000 ms; PONG window = 3..8s inside 12s listen */
#define MSG_RF_RANGE_BACKOFF_TICKS      100u   /* 1000 ms if FSK busy */
#define MSG_RF_RANGE_PING_REPEATS       2u     /* 1.0.1d: send two Range Check PING frames; first may wake UV-K1 FSK RX */
#define MSG_RF_RANGE_PONG_REPEATS       1u     /* 1.0.1: single Range Check PONG; reduce collisions */
#define MSG_RF_RANGE_TX_WARMUP_MS        180u   /* 1.0.1b: refresh BK4829/FSK TX path before Range PING/PONG */
#define MSG_RF_RANGE_PING_DUP_WINDOW_TICKS 200u   /* 1.0.1c: suppress same PING only for 2 seconds */
#define MSG_RF_ACK_REPEATS              1u     /* GGFW 1.0.1: one ACK frame; message retry still handles missed ACKs */
#define MSG_RF_ACK_QUEUE_LEN             4u     /* 1.0.1: queue ACKs so delayed ACKs do not overwrite each other */
#define MSG_RF_REPEAT_GAP_MS            100u   /* short gap between repeated FSK frames */
#define MSG_RF_RANGE_WAIT_PONG_TICKS    1200u  /* 12 s same-channel PONG listen lock */
#define MSG_RF_SAFE_KEEPALIVE_TICKS      500u   /* 5 s safe Messenger/Range RX keepalive */
#define MSG_RF_SAFE_KEEPALIVE_RETRY_TICKS 50u   /* retry in 0.5 s if channel busy */

#define MSG_RF_REG59_RX_CLEAR        0x4068u
#define MSG_RF_REG59_RX_ENABLE       0x3068u
#define MSG_RF_FSK_IRQ_MASK           (BK4819_REG_3F_FSK_RX_SYNC | BK4819_REG_3F_FSK_RX_FINISHED | BK4819_REG_3F_FSK_FIFO_ALMOST_FULL)

/* RF13: TX lead-in without duplicate packets.
 * REG_59 bits <7:4> select preamble length: 0=1 byte ... 15=16 bytes.
 * Existing Aircopy uses 0x0068 / 0x2868 (7-byte preamble, 4-byte sync).
 * For Messenger TX only, use the maximum hardware preamble length so the
 * receiver modem can wake/lock before the real native packet starts.
 */
#define MSG_RF_REG59_TX_CLEAR_LONG_PRE 0x80F8u
#define MSG_RF_REG59_TX_READY_LONG_PRE 0x00F8u
#define MSG_RF_REG59_TX_START_LONG_PRE 0x28F8u
#define MSG_RF_REG5D_LEN_100_BYTES    0x6300u

static uint8_t s_tx_count;
static uint8_t s_restore_count;
static uint8_t s_sync_count;
static uint8_t s_fifo_count;
static uint8_t s_decode_count;
static uint8_t s_sidecar_count;
static uint8_t s_post_tx_restore_ticks;
static uint8_t s_rearm_delay_ticks;
static uint8_t s_rx_stale_ticks;
static uint8_t s_deferred_beep_ticks;
static uint8_t s_deferred_beep_max_ticks;
static bool s_deferred_beep_pending;
static uint8_t s_ack_success_beep_ticks;
static bool s_ack_success_beep_pending;
static bool s_store_initialized;
static uint8_t s_reprime_delay_ticks;
static uint16_t s_idle_reprime_ticks;
static uint16_t s_safe_keepalive_ticks;
static bool s_boot_prime_done;
static bool s_sidecar_armed;
static bool s_rx_capture_active;
static bool s_ignore_next_self_rx;
static bool s_fsk_audio_muted;
static bool s_fsk_audio_prev_speaker;
static uint16_t s_fsk_audio_prev_reg48;
static uint16_t s_unread_led_ticks;
static bool s_unread_led_owner;
static bool s_bw_lock_active;
static uint8_t s_bw_lock_tx_old;
static uint8_t s_bw_lock_rx_old;

static bool s_rx_channel_lock_active;
static uint16_t s_rx_channel_lock_ticks;
static uint8_t s_rx_channel_lock_vfo;
static uint8_t s_rx_channel_lock_old_rx_vfo;
static bool s_rx_channel_lock_old_dw_active;
static bool s_range_beep_pending;
static uint8_t s_range_beep_ticks;

typedef struct {
    bool active;
    uint8_t delay_ticks;
    uint16_t id;
    char to[MSG_CALLSIGN_LEN + 1];
    uint8_t vfo;
} MSG_RF_PendingAck_t;

static MSG_RF_PendingAck_t s_pending_ack_queue[MSG_RF_ACK_QUEUE_LEN];

static bool s_pending_range_pong_active;
static uint16_t s_pending_range_pong_delay_ticks;
static uint16_t s_pending_range_pong_id;
static uint16_t s_last_range_ping_id;
static uint16_t s_last_range_ping_age_ticks;
static char s_last_range_ping_from[MSG_CALLSIGN_LEN + 1];
static char s_pending_range_pong_to[MSG_CALLSIGN_LEN + 1];
static uint8_t s_pending_range_pong_vfo;

static bool MSG_RF_ParseAckText(const char *text, uint16_t *id);

static bool s_wait_ack_active;
static uint16_t s_wait_ack_id;
static uint16_t s_wait_ack_ticks;
static uint8_t s_wait_ack_retries;
static uint8_t s_wait_ack_ttl;
static char s_wait_ack_text[MSG_TEXT_LEN + 1];

static bool s_ack_collect_active;
static uint16_t s_ack_collect_id;
static uint16_t s_ack_collect_ticks;

/* RF22 ACK debug: replace old RF register debug on-screen with the
 * actual ACK state machine values.  These fields are intentionally tiny
 * so they fit on one Messenger debug line. */
static uint16_t s_ack_dbg_pending_id;
static uint16_t s_ack_dbg_sent_id;
static uint16_t s_ack_dbg_rx_id;
static uint8_t  s_ack_dbg_sent_count;
static uint8_t  s_ack_dbg_rx_count;
static uint8_t  s_ack_dbg_match_count;
static uint8_t  s_ack_dbg_miss_count;
static uint8_t  s_ack_dbg_wait_active;
static uint8_t  s_ack_dbg_retry_count;
static uint16_t s_ack_jitter_seed;
static uint32_t s_range_jitter_seed;

/* RF7 diagnostic snapshot: lets us see exactly which BK4829 state
 * corresponds to the "open RX / messages decode perfectly" condition.
 * These are read-only probes; they do not change radio state. */
static uint16_t s_dbg_02, s_dbg_0b, s_dbg_0c, s_dbg_30, s_dbg_3f, s_dbg_47, s_dbg_58, s_dbg_59, s_dbg_67;
static uint8_t  s_dbg_open_ticks;
static uint8_t  s_dbg_last_decode_open;

#ifdef ENABLE_AIRCOPY
static uint8_t s_rx_words;
#endif

typedef struct {
    uint16_t r19, r2b, r30, r3f, r47, r58, r59, r5c, r5d, r5e, r70, r72;
    bool valid;
} MSG_RF_RegSnapshot_t;

static MSG_RF_RegSnapshot_t s_voice_snapshot;

static bool MSG_RF_SendPacketFrame(const uint8_t *packet, bool count_tx, bool ignore_self_rx);
static bool MSG_RF_SendPacketFrameRepeated(const uint8_t *packet, bool count_tx, bool ignore_self_rx, uint8_t repeats);
static bool MSG_RF_SendPacketFrameRepeatedOnVfo(const uint8_t *packet, bool count_tx, bool ignore_self_rx, uint8_t tx_vfo, uint8_t repeats);
static bool MSG_RF_SendRangePacketFrameRepeatedOnVfo(const uint8_t *packet, bool count_tx, bool ignore_self_rx, uint8_t tx_vfo, uint8_t repeats);
static void MSG_RF_RequestControlledReprime(uint8_t delay_ticks);
static void MSG_RF_DoControlledReprime(void);
static void MSG_RF_EnsureFskIrqMask(void);
static bool MSG_RF_AutoTxBusy(void);

static void MSG_RF_QueueAck(uint16_t id, const char *to, uint8_t rx_vfo);

static bool MSG_RF_ChannelBusy(void)
{
    return (BK4819_ReadRegister(BK4819_REG_0C) & (1u << 1)) != 0;
}

static bool MSG_RF_FskBusy(void)
{
    /* Manual user TX keeps the RF23 rule: do not block only because voice
     * squelch/carrier is open. */
    return s_rx_capture_active || (s_rx_stale_ticks != 0u);
}

static bool MSG_RF_AutoTxBusy(void)
{
    /* UV-K5 parity: automatic ACK/PONG/retry must not collide with ordinary
     * channel activity.  Manual SEND still uses MSG_RF_FskBusy(), but all
     * automatic replies defer while squelch/carrier is open. */
    return MSG_RF_FskBusy() || MSG_RF_ChannelBusy();
}

static void MSG_RF_EnsureStoreInitialized(void)
{
    if (!s_store_initialized) {
        MSG_STORE_Init();
        s_store_initialized = true;
    }
}

static void MSG_RF_NarrowLockBegin(void)
{
    /* Temporary Messenger RF narrow-lock: do not persist to EEPROM.
     * This only changes runtime VFO bandwidth while a Messenger FSK TX/RX
     * operation is active, then restores the user setting. */
    if (s_bw_lock_active) return;
    if (!gTxVfo) return;

    s_bw_lock_tx_old = gTxVfo->CHANNEL_BANDWIDTH;
    s_bw_lock_rx_old = gRxVfo ? gRxVfo->CHANNEL_BANDWIDTH : s_bw_lock_tx_old;
    gTxVfo->CHANNEL_BANDWIDTH = BANDWIDTH_NARROW;
    if (gRxVfo) gRxVfo->CHANNEL_BANDWIDTH = BANDWIDTH_NARROW;
    s_bw_lock_active = true;
}

static void MSG_RF_NarrowLockEnd(void)
{
    if (!s_bw_lock_active) return;
    if (gTxVfo) gTxVfo->CHANNEL_BANDWIDTH = s_bw_lock_tx_old;
    if (gRxVfo) gRxVfo->CHANNEL_BANDWIDTH = s_bw_lock_rx_old;
    s_bw_lock_active = false;
}

static bool MSG_RF_AckEnabledNow(void)
{
    /* Safety-critical public setting: re-read the persisted config at the
     * point where an automatic ACK/retry TX decision is made.  This prevents
     * stale in-RAM state from causing an unintended automatic transmission
     * after the user has switched MSG ACK to OFF in the main menu. */
    MSG_STORE_Init();
    s_store_initialized = true;
    return gMessengerConfig.msg_ack != 0u;
}

static uint8_t MSG_RF_RandomAckDelayTicks(uint16_t msg_id)
{
    /* 0.2.4: multi-radio ACK collision reduction with legacy compatibility.
     * Minimum delay stays near the old 800ms ACK window so older builds have
     * time to re-prime RX after TX; jitter then reduces ACK collisions.
     * Keep this tiny and
     * deterministic enough for MCU use, but seeded by time/RSSI/msgid so
     * multiple receivers do not answer on the exact same 10ms tick. */
    if (s_ack_jitter_seed == 0u) {
        s_ack_jitter_seed = (uint16_t)(gFlashLightBlinkCounter ^ BK4819_ReadRegister(BK4819_REG_67) ^ msg_id);
        if (s_ack_jitter_seed == 0u) s_ack_jitter_seed = 0x5A3Cu;
    }
    s_ack_jitter_seed = (uint16_t)(s_ack_jitter_seed * 109u + 89u + msg_id);
    return (uint8_t)(MSG_RF_ACK_SEND_DELAY_MIN_TICKS + (s_ack_jitter_seed % (MSG_RF_ACK_SEND_DELAY_JIT_TICKS + 1u)));
}

static uint16_t MSG_RF_RandomRangeDelayTicks(uint16_t msg_id)
{
    /* 0.5.14: Range PONG jitter must not share the ACK seed.
     * Mix fresh per-ping entropy so replies from one or more radios spread
     * across the first 6s of the 10s listen window instead of clustering near the end.
     * Use multiply-high reduction rather than a raw modulo on a weak 16-bit
     * sequence to avoid visible bias in short field tests. */
    uint32_t entropy = ((uint32_t)gFlashLightBlinkCounter << 16) ^
                       ((uint32_t)BK4819_ReadRegister(BK4819_REG_67) << 1) ^
                       ((uint32_t)BK4819_ReadRegister(BK4819_REG_0C) << 5) ^
                       ((uint32_t)(gEeprom.RX_VFO & 1u) << 12) ^
                       (uint32_t)msg_id ^ 0x3C5A9E37u;

    if (s_range_jitter_seed == 0u) {
        s_range_jitter_seed = entropy ^ 0x6B21A5A5u;
        if (s_range_jitter_seed == 0u) s_range_jitter_seed = 0x13579BDFu;
    } else {
        s_range_jitter_seed ^= entropy + 0x9E3779B9u +
                               (s_range_jitter_seed << 6) +
                               (s_range_jitter_seed >> 2);
    }

    s_range_jitter_seed ^= (s_range_jitter_seed << 13);
    s_range_jitter_seed ^= (s_range_jitter_seed >> 17);
    s_range_jitter_seed ^= (s_range_jitter_seed << 5);

    const uint32_t span = (uint32_t)MSG_RF_RANGE_PONG_JIT_TICKS + 1u;
    const uint16_t rnd16 = (uint16_t)(s_range_jitter_seed ^ (s_range_jitter_seed >> 16));
    const uint16_t jitter = (uint16_t)(((uint32_t)rnd16 * span) >> 16);
    return (uint16_t)(MSG_RF_RANGE_PONG_MIN_TICKS + jitter);
}


static void MSG_RF_RxChannelLockStart(uint8_t vfo, uint16_t ticks)
{
    vfo &= 1u;
    if (!s_rx_channel_lock_active) {
        s_rx_channel_lock_old_rx_vfo = gEeprom.RX_VFO & 1u;
        s_rx_channel_lock_old_dw_active = gDualWatchActive;
    }
    s_rx_channel_lock_active = true;
    s_rx_channel_lock_ticks = ticks ? ticks : 1u;
    s_rx_channel_lock_vfo = vfo;
    if ((gEeprom.RX_VFO & 1u) != vfo) {
        gEeprom.RX_VFO = vfo;
        gRxVfo = &gEeprom.VfoInfo[gEeprom.RX_VFO];
        RADIO_SetupRegisters(false);
        MSG_RF_EnsureFskIrqMask();
    }
    gScheduleDualWatch = false;
    gDualWatchCountdown_10ms = ticks ? ticks : 1u;
    gDualWatchActive = false;
    gUpdateStatus = true;
}

static void MSG_RF_RxChannelLockStop(void)
{
    if (!s_rx_channel_lock_active) return;
    s_rx_channel_lock_active = false;
    s_rx_channel_lock_ticks = 0;
    if ((gEeprom.RX_VFO & 1u) != (s_rx_channel_lock_old_rx_vfo & 1u)) {
        gEeprom.RX_VFO = s_rx_channel_lock_old_rx_vfo & 1u;
        gRxVfo = &gEeprom.VfoInfo[gEeprom.RX_VFO];
        RADIO_SetupRegisters(false);
        MSG_RF_EnsureFskIrqMask();
    }
    gDualWatchActive = s_rx_channel_lock_old_dw_active;
    gScheduleDualWatch = false;
    gUpdateStatus = true;
}

static void MSG_RF_RxChannelLockTick(void)
{
    if (!s_rx_channel_lock_active) return;
    if ((gEeprom.RX_VFO & 1u) != (s_rx_channel_lock_vfo & 1u)) {
        gEeprom.RX_VFO = s_rx_channel_lock_vfo & 1u;
        gRxVfo = &gEeprom.VfoInfo[gEeprom.RX_VFO];
        RADIO_SetupRegisters(false);
        MSG_RF_EnsureFskIrqMask();
    }
    gScheduleDualWatch = false;
    gDualWatchCountdown_10ms = s_rx_channel_lock_ticks ? s_rx_channel_lock_ticks : 1u;
    gDualWatchActive = false;
    if (s_rx_channel_lock_ticks > 0u) --s_rx_channel_lock_ticks;
    if (s_rx_channel_lock_ticks == 0u) MSG_RF_RxChannelLockStop();
}

bool MSG_RF_RxChannelLockActive(void)
{
    return s_rx_channel_lock_active;
}

static void MSG_RF_RequestRangeBeep(void)
{
    if (!gMessengerConfig.msg_beep) return;
    s_range_beep_pending = true;
    s_range_beep_ticks = 25u;
}

static void MSG_RF_RangeBeepTick(void)
{
    if (!s_range_beep_pending) return;
    if (s_range_beep_ticks > 0u) --s_range_beep_ticks;
    if (s_range_beep_ticks == 0u &&
        gCurrentFunction != FUNCTION_TRANSMIT &&
        !MSG_RF_ChannelBusy() &&
        !s_rx_capture_active && !s_rx_stale_ticks) {
#ifdef ENABLE_FEAT_F4HWN
        AUDIO_PlayBeep(BEEP_500HZ_30MS);
#else
        AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
#endif
        gBeepToPlay = BEEP_NONE;
        s_range_beep_pending = false;
    }
}

static void MSG_RF_MuteFskAudio(void)
{
    if (s_fsk_audio_muted) return;

    /* 0.2.3b: use a soft AF-gain mute instead of toggling the speaker GPIO
     * path.  GPIO audio-path on/off produced audible "kist" clicks on the
     * UV-K1.  Saving/restoring REG_48 keeps the normal voice path structure
     * intact and avoids fighting the existing RX/TX speaker logic. */
    s_fsk_audio_prev_speaker = gEnableSpeaker;
    s_fsk_audio_prev_reg48 = BK4819_ReadRegister(BK4819_REG_48);
    BK4819_WriteRegister(BK4819_REG_48, (uint16_t)(s_fsk_audio_prev_reg48 & ~(0x3Fu << 4)));
    s_fsk_audio_muted = true;
}

static void MSG_RF_RestoreFskAudioMute(void)
{
    if (!s_fsk_audio_muted) return;
    s_fsk_audio_muted = false;
    BK4819_WriteRegister(BK4819_REG_48, s_fsk_audio_prev_reg48);
    gEnableSpeaker = s_fsk_audio_prev_speaker;
}

static bool MSG_RF_LedBlinkAllowed(void)
{
    if (gCurrentFunction == FUNCTION_TRANSMIT) return false;
    // if (MSG_RF_ChannelBusy()) return false;
    if (s_rx_capture_active || s_rx_stale_ticks) return false;
    return true;
}

static void MSG_RF_SetUnreadLed(bool on)
{
    if (gMessengerConfig.msg_led == 2u) {
        BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, on);
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, on);
    } else {
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, on);
    }
    s_unread_led_owner = on;
}

static void MSG_RF_UpdateUnreadLed(void)
{
    if (!gMessengerConfig.msg_led || !MSG_STORE_HasUnread()) {
        if (s_unread_led_owner && MSG_RF_LedBlinkAllowed()) MSG_RF_SetUnreadLed(false);
        s_unread_led_ticks = 0u;
        return;
    }

    if (!MSG_RF_LedBlinkAllowed()) {
        /* Never fight normal RX green or TX red LEDs.
         * If our blink is active and a radio event starts, release only the
         * LED line that does not belong to the active radio state. */
        if (s_unread_led_owner) {
            if (gCurrentFunction == FUNCTION_TRANSMIT) {
                BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
            } else if (MSG_RF_ChannelBusy() && gMessengerConfig.msg_led == 2u) {
                BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
            }
        }
        s_unread_led_owner = false;
        return;
    }

    if (s_unread_led_ticks >= MSG_RF_UNREAD_LED_PERIOD_TICKS) s_unread_led_ticks = 0u;

    const bool on = (s_unread_led_ticks < MSG_RF_UNREAD_LED_FLASH_TICKS) ||
                    (s_unread_led_ticks >= (MSG_RF_UNREAD_LED_FLASH_TICKS + MSG_RF_UNREAD_LED_GAP_TICKS) &&
                     s_unread_led_ticks < (MSG_RF_UNREAD_LED_FLASH_TICKS + MSG_RF_UNREAD_LED_GAP_TICKS + MSG_RF_UNREAD_LED_FLASH_TICKS));

    if (on != s_unread_led_owner) MSG_RF_SetUnreadLed(on);
    s_unread_led_ticks++;
}

static void MSG_RF_RequestDeferredBeep(void)
{
    if (!gMessengerConfig.msg_beep) return;

    /* RF12: do not rely on gBeepToPlay being consumed by the current UI screen.
     * RF messages can arrive on the main screen, where no Messenger UI handler runs.
     * Keep a global pending flag and play the beep directly from MSG_RF_Tick10ms()
     * after RX/FSK activity has gone idle. */
    s_deferred_beep_pending = true;
    s_deferred_beep_ticks = 35u;       /* about 350 ms after RX goes idle */
    s_deferred_beep_max_ticks = 250u;  /* bounded fallback, still waits for idle */
}

static bool MSG_RF_CanPlayNotifyBeep(void)
{
    if (!s_deferred_beep_pending) return false;
    if (gCurrentFunction == FUNCTION_TRANSMIT) return false;
    if (MSG_RF_ChannelBusy()) return false;
    if (s_rx_capture_active || s_rx_stale_ticks) return false;
    return true;
}

static void MSG_RF_PlayNotifyBeepNow(void)
{
    AUDIO_PlayBeep(BEEP_500HZ_60MS_DOUBLE_BEEP_FORCE);
    gBeepToPlay = BEEP_NONE;
    s_deferred_beep_pending = false;
    s_deferred_beep_ticks = 0;
    s_deferred_beep_max_ticks = 0;
}

static void MSG_RF_RequestAckSuccessBeep(void)
{
    if (!gMessengerConfig.msg_beep) return;
    s_ack_success_beep_pending = true;
    s_ack_success_beep_ticks = 30u; /* about 300 ms after + status update */
}

static bool MSG_RF_CanPlayAckSuccessBeep(void)
{
    if (!s_ack_success_beep_pending) return false;
    if (gCurrentFunction == FUNCTION_TRANSMIT) return false;
    if (MSG_RF_ChannelBusy()) return false;
    if (s_rx_capture_active || s_rx_stale_ticks) return false;
    return true;
}

static void MSG_RF_PlayAckSuccessBeepNow(void)
{
    AUDIO_PlayBeep(BEEP_1KHZ_60MS_OPTIONAL);
    gBeepToPlay = BEEP_NONE;
    s_ack_success_beep_pending = false;
    s_ack_success_beep_ticks = 0;
}

static void MSG_RF_EnsureFskIrqMask(void)
{
#ifdef ENABLE_AIRCOPY
    /* RF10 targeted fix: RF9 showed messages decode when REG_3F has
     * FSK IRQ bits enabled (0x3002) and fail when F4HWN leaves 3F at
     * voice-only values such as 0x0C0C. Preserve all existing F4HWN
     * interrupt enables and only OR in the FSK sync/FIFO/RXFIN bits.
     * Do not touch REG_47, REG_58 or REG_59 here. */
    const uint16_t r3f = BK4819_ReadRegister(BK4819_REG_3F);
    const uint16_t wanted = (uint16_t)(r3f | MSG_RF_FSK_IRQ_MASK);
    if (r3f != wanted) {
        BK4819_WriteRegister(BK4819_REG_3F, wanted);
    }
#endif
}

static void MSG_RF_UpdateDebugSnapshot(void)
{
    s_dbg_0c = BK4819_ReadRegister(BK4819_REG_0C);
    s_dbg_02 = BK4819_ReadRegister(BK4819_REG_02);
    s_dbg_0b = BK4819_ReadRegister(BK4819_REG_0B);
    s_dbg_30 = BK4819_ReadRegister(BK4819_REG_30);
    s_dbg_3f = BK4819_ReadRegister(BK4819_REG_3F);
    s_dbg_47 = BK4819_ReadRegister(BK4819_REG_47);
    s_dbg_58 = BK4819_ReadRegister(BK4819_REG_58);
    s_dbg_59 = BK4819_ReadRegister(BK4819_REG_59);
    s_dbg_67 = BK4819_ReadRegister((BK4819_REGISTER_t)0x67);

    if ((s_dbg_0c & (1u << 1)) != 0u) {
        if (s_dbg_open_ticks < 250u) s_dbg_open_ticks++;
    } else {
        s_dbg_open_ticks = 0;
    }
}

static void MSG_RF_CaptureVoiceSnapshot(void)
{
    s_voice_snapshot.r19 = BK4819_ReadRegister(BK4819_REG_19);
    s_voice_snapshot.r2b = BK4819_ReadRegister(BK4819_REG_2B);
    s_voice_snapshot.r30 = BK4819_ReadRegister(BK4819_REG_30);
    s_voice_snapshot.r3f = BK4819_ReadRegister(BK4819_REG_3F);
    s_voice_snapshot.r47 = BK4819_ReadRegister(BK4819_REG_47);
    s_voice_snapshot.r58 = BK4819_ReadRegister(BK4819_REG_58);
    s_voice_snapshot.r59 = BK4819_ReadRegister(BK4819_REG_59);
    s_voice_snapshot.r5c = BK4819_ReadRegister(BK4819_REG_5C);
    s_voice_snapshot.r5d = BK4819_ReadRegister(BK4819_REG_5D);
    s_voice_snapshot.r5e = BK4819_ReadRegister((BK4819_REGISTER_t)0x5E);
    s_voice_snapshot.r70 = BK4819_ReadRegister(BK4819_REG_70);
    s_voice_snapshot.r72 = BK4819_ReadRegister(BK4819_REG_72);
    s_voice_snapshot.valid = true;
}

static void MSG_RF_RestoreVoiceSnapshot(void)
{
    if (!s_voice_snapshot.valid) return;

    BK4819_WriteRegister(BK4819_REG_3F, 0x0000);
    BK4819_WriteRegister(BK4819_REG_58, s_voice_snapshot.r58);
    BK4819_WriteRegister(BK4819_REG_59, s_voice_snapshot.r59);
    BK4819_WriteRegister(BK4819_REG_5C, s_voice_snapshot.r5c);
    BK4819_WriteRegister(BK4819_REG_5D, s_voice_snapshot.r5d);
    BK4819_WriteRegister((BK4819_REGISTER_t)0x5E, s_voice_snapshot.r5e);
    BK4819_WriteRegister(BK4819_REG_70, s_voice_snapshot.r70);
    BK4819_WriteRegister(BK4819_REG_72, s_voice_snapshot.r72);
    BK4819_WriteRegister(BK4819_REG_2B, s_voice_snapshot.r2b);
    BK4819_WriteRegister(BK4819_REG_19, s_voice_snapshot.r19);
    BK4819_WriteRegister(BK4819_REG_47, s_voice_snapshot.r47); /* must remain normal AF */
    BK4819_WriteRegister(BK4819_REG_30, s_voice_snapshot.r30);
    BK4819_WriteRegister(BK4819_REG_3F, s_voice_snapshot.r3f);
    if (gMessengerConfig.msg_rx && gCurrentFunction != FUNCTION_TRANSMIT && !MSG_RF_ChannelBusy()) {
        MSG_RF_EnsureFskIrqMask();
    }
}

static void MSG_RF_DisarmSidecarNoRadioReset(void)
{
    s_sidecar_armed = false;
    s_rx_capture_active = false;
    s_rx_stale_ticks = 0;
#ifdef ENABLE_AIRCOPY
    s_rx_words = 0;
    gFSKWriteIndex = 0;
#endif
    BK4819_WriteRegister(BK4819_REG_3F, 0x0000);
    BK4819_WriteRegister(BK4819_REG_59, 0x0068);
}
static void MSG_RF_KeepFskRxEnabled(void)
{
#ifdef ENABLE_AIRCOPY
    /* Keep the proven working RX state: REG_59<12> FSK RX enable stays set.
     * Clear RX FIFO with a one-shot pulse, then immediately return to 0x3068.
     * Do not touch REG_47 or normal AF output. */
    MSG_RF_EnsureFskIrqMask();
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_RX_CLEAR);
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_RX_ENABLE);
#endif
}


void MSG_RF_OnRadioSetupRegisters(void)
{
#ifdef ENABLE_AIRCOPY
    if (gSurvivalMode) return;
    if (!gMessengerConfig.msg_rx) return;
    if (gCurrentFunction == FUNCTION_TRANSMIT) return;

    /* RADIO_SetupRegisters() rewrites the voice/interrupt/FSK related BK4829
     * registers.  The old UV-K1 sidecar flag could remain true while the chip
     * was no longer in the same FSK RX state.  Mark it stale and let the next
     * idle tick fully re-arm the Aircopy-compatible RX path. */
    s_sidecar_armed = false;
    s_rx_capture_active = false;
    s_rx_stale_ticks = 0u;
    s_reprime_delay_ticks = 1u;
    s_safe_keepalive_ticks = MSG_RF_SAFE_KEEPALIVE_RETRY_TICKS;
    MSG_RF_EnsureFskIrqMask();
#endif
}


void MSG_RF_Open(void) { MSG_RF_EnsureStoreInitialized(); }
void MSG_RF_Close(void) {}

void MSG_RF_HardRestoreVoicePath(void)
{
    MSG_RF_RestoreFskAudioMute();
    MSG_RF_NarrowLockEnd();
    if (!s_voice_snapshot.valid && !s_sidecar_armed) {
        return;
    }
    MSG_RF_DisarmSidecarNoRadioReset();
    BK4819_ResetFSK();
    MSG_RF_RestoreVoiceSnapshot();
    BK4819_SetupPowerAmplifier(0, 0);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
    RADIO_SelectVfos();
    RADIO_SetupRegisters(true);
    MSG_RF_RestoreVoiceSnapshot();
    s_post_tx_restore_ticks = 0;
    s_rearm_delay_ticks = MSG_RF_REARM_DELAY_TICKS;
    s_restore_count++;
}

static void MSG_RF_ArmSidecarIfIdle(void)
{
#ifdef ENABLE_AIRCOPY
    if (!gMessengerConfig.msg_rx) return;
    if (s_sidecar_armed) {
        /* If another path (dual-watch, scan, AM/FM retune, voice restore)
         * touched BK4829 while we still believed the sidecar was armed, do
         * not just set REG_59.  UV-K5 is robust because it re-applies the FSK
         * RX path after radio setup.  Do the same here, but only while idle. */
        if (!MSG_RF_ChannelBusy()) {
            const uint16_t r3f = BK4819_ReadRegister(BK4819_REG_3F);
            const uint16_t r59 = BK4819_ReadRegister(BK4819_REG_59);
            if ((r59 & 0x1000u) == 0u || (r3f & MSG_RF_FSK_IRQ_MASK) != MSG_RF_FSK_IRQ_MASK) {
                s_sidecar_armed = false;
            } else {
                MSG_RF_EnsureFskIrqMask();
            }
        }
        if (s_sidecar_armed) return;
    }
    if (gCurrentFunction == FUNCTION_TRANSMIT) return;
    if (MSG_RF_ChannelBusy()) return;

    if (!s_voice_snapshot.valid) MSG_RF_CaptureVoiceSnapshot();

    memset(g_FSK_Buffer, 0, sizeof(g_FSK_Buffer));
    gFSKWriteIndex = 0;
    s_rx_words = 0;
    s_rx_capture_active = false;
    s_rx_stale_ticks = 0;

    /* Exact Aircopy FSK RX setup, but without BK4819_ResetFSK() or BK4819_RX_TurnOn(). */
    BK4819_SetupAircopy();
    BK4819_WriteRegister(BK4819_REG_5D, MSG_RF_REG5D_LEN_100_BYTES);
    BK4819_WriteRegister(BK4819_REG_02, 0x0000);
    MSG_RF_EnsureFskIrqMask();
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_RX_CLEAR);
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_RX_ENABLE);

    s_sidecar_armed = true;
    s_sidecar_count++;
#endif
}


static void MSG_RF_RequestControlledReprime(uint8_t delay_ticks)
{
    /* RF27: schedule a safe RX/FSK re-prime after TX/RX/scan-like state changes.
     * This is intentionally delayed and only executes while the channel is idle,
     * so it must not create the old voice-RX tick/cut problem. */
    if (delay_ticks == 0u) delay_ticks = 1u;
    if (s_reprime_delay_ticks == 0u || delay_ticks < s_reprime_delay_ticks) {
        s_reprime_delay_ticks = delay_ticks;
    }
}

static bool MSG_RF_CanControlledReprimeNow(void)
{
    if (!gMessengerConfig.msg_rx) return false;
    if (gCurrentFunction == FUNCTION_TRANSMIT) return false;
    if (MSG_RF_ChannelBusy()) return false;
    if (s_rx_capture_active || s_rx_stale_ticks) return false;
    return true;
}

static void MSG_RF_DoControlledReprime(void)
{
#ifdef ENABLE_AIRCOPY
    if (!MSG_RF_CanControlledReprimeNow()) return;

    if (!s_sidecar_armed) {
        MSG_RF_ArmSidecarIfIdle();
    } else {
        /* Light re-prime: preserve REG_47/voice path, keep FSK RX enabled,
         * refresh FSK IRQ mask and FIFO only while idle. */
        s_rx_words = 0;
        gFSKWriteIndex = 0;
        s_rx_capture_active = false;
        s_rx_stale_ticks = 0;
        MSG_RF_EnsureFskIrqMask();
        MSG_RF_KeepFskRxEnabled();
        s_sidecar_count++;
    }
    s_idle_reprime_ticks = MSG_RF_IDLE_REPRIME_TICKS;
#endif
}

static void MSG_RF_FinishRxAttempt(bool parsed)
{
    MSG_RF_RestoreFskAudioMute();
    MSG_RF_NarrowLockEnd();
    /* RF7 showed that dropping REG_59 to 0x0068 kills FIFO/decode even though
     * sync keeps increasing. Keep FSK RX enabled and just reset our local
     * capture/FIFO state. This should preserve the "open RX decodes all"
     * condition without intentionally forcing SQL-open audio. */
    s_sidecar_armed = true;
    s_rx_capture_active = false;
    s_rx_stale_ticks = 0;
#ifdef ENABLE_AIRCOPY
    s_rx_words = 0;
    gFSKWriteIndex = 0;
#endif
    MSG_RF_KeepFskRxEnabled();
    s_rearm_delay_ticks = 0;
    /* RF29: do not schedule a controlled re-prime after RX.  RF28 showed
     * RX-side/event re-prime can reduce normal message reception by colliding
     * with the start of the next burst.  Keep TX-after re-prime only. */
    if (parsed) {
        s_dbg_last_decode_open = (uint8_t)((s_dbg_0c & (1u << 1)) ? 1u : 0u);
        MSG_RF_RequestDeferredBeep();
    }
}

void MSG_RF_Tick10ms(void)
{
    if (s_last_range_ping_age_ticks < MSG_RF_RANGE_PING_DUP_WINDOW_TICKS) ++s_last_range_ping_age_ticks;
    if (gSurvivalMode) return;
    MSG_RF_EnsureStoreInitialized();
    MSG_RF_UpdateDebugSnapshot();
    MSG_RF_RxChannelLockTick();
    MSG_RF_RangeBeepTick();

    if (gCurrentFunction == FUNCTION_TRANSMIT) {
        MSG_RF_RestoreFskAudioMute();
    }

#ifdef ENABLE_AIRCOPY
    /* Keep FSK IRQ enables present before the next burst arrives, but avoid
     * writing while a carrier is already active. This targets the observed
     * 3F:0C0C -> no FIFO/decode vs 3F:3002 -> FIFO/decode condition. */
    if (gMessengerConfig.msg_rx && gCurrentFunction != FUNCTION_TRANSMIT && !MSG_RF_ChannelBusy()) {
        MSG_RF_EnsureFskIrqMask();
    }
#endif

    if (s_post_tx_restore_ticks && --s_post_tx_restore_ticks == 0) {
        MSG_RF_HardRestoreVoicePath();
    }

    if (s_deferred_beep_pending) {
        if (s_deferred_beep_ticks > 0u) {
            --s_deferred_beep_ticks;
        }
        if (s_deferred_beep_max_ticks > 0u) {
            --s_deferred_beep_max_ticks;
        }

        if (s_deferred_beep_ticks == 0u && MSG_RF_CanPlayNotifyBeep()) {
            MSG_RF_PlayNotifyBeepNow();
        } else if (s_deferred_beep_max_ticks == 0u &&
                   gCurrentFunction != FUNCTION_TRANSMIT &&
                   !s_rx_capture_active && !s_rx_stale_ticks) {
            /* Last-resort fallback: still do not play during TX or active FSK capture.
             * This prevents the old "beep only when Inbox opens" behavior. */
            MSG_RF_PlayNotifyBeepNow();
        }
    }

    if (s_ack_success_beep_pending) {
        if (s_ack_success_beep_ticks > 0u) {
            --s_ack_success_beep_ticks;
        }
        if (s_ack_success_beep_ticks == 0u && MSG_RF_CanPlayAckSuccessBeep()) {
            MSG_RF_PlayAckSuccessBeepNow();
        }
    }

    if (s_rx_stale_ticks && --s_rx_stale_ticks == 0) {
        MSG_RF_FinishRxAttempt(false);
    }

    /* RF29: TX-side controlled re-prime only.  RF27's aggressive 8-second
     * idle keepalive could collide with incoming FSK.  0.5.16 brings back a
     * much slower and strictly gated safe keepalive so Messenger/Range packet
     * handling does not passivate after sitting idle, without touching voice
     * RX or active FSK captures. */
    if (s_reprime_delay_ticks > 0u) {
        --s_reprime_delay_ticks;
        if (s_reprime_delay_ticks == 0u) {
            MSG_RF_DoControlledReprime();
            s_safe_keepalive_ticks = MSG_RF_SAFE_KEEPALIVE_TICKS;
        }
    }

    if (gMessengerConfig.msg_rx) {
        if (s_safe_keepalive_ticks == 0u) {
            s_safe_keepalive_ticks = MSG_RF_SAFE_KEEPALIVE_TICKS;
        } else {
            --s_safe_keepalive_ticks;
        }

        if (s_safe_keepalive_ticks == 0u && s_reprime_delay_ticks == 0u) {
            if (MSG_RF_CanControlledReprimeNow()) {
                MSG_RF_DoControlledReprime();
                s_safe_keepalive_ticks = MSG_RF_SAFE_KEEPALIVE_TICKS;
            } else {
                s_safe_keepalive_ticks = MSG_RF_SAFE_KEEPALIVE_RETRY_TICKS;
            }
        }
    } else {
        s_safe_keepalive_ticks = 0u;
    }

#ifdef ENABLE_AIRCOPY
    /* Stage 2: ACKs and retries are handled from the global RF tick, never
     * from the RX interrupt/parser path. This keeps voice/RX restore behavior
     * close to the RF17 stable baseline. */
    if (s_pending_range_pong_active) {
        if (s_pending_range_pong_delay_ticks > 0u) --s_pending_range_pong_delay_ticks;
        if (s_pending_range_pong_delay_ticks == 0u) {
            if (gCurrentFunction != FUNCTION_TRANSMIT && !MSG_RF_AutoTxBusy()) {
                uint8_t pong_frame[MSG_PKT_WIRE_LEN];
                if (MSG_PACKET_BuildPong(pong_frame, sizeof(pong_frame), s_pending_range_pong_id,
                                         gMessengerConfig.callsign, s_pending_range_pong_to, gBatteryVoltageAverage) == MSG_PKT_WIRE_LEN) {
                    if (MSG_RF_SendRangePacketFrameRepeatedOnVfo(pong_frame, false, true, s_pending_range_pong_vfo, MSG_RF_RANGE_PONG_REPEATS)) {
                        s_pending_range_pong_active = false;
                    } else {
                        s_pending_range_pong_delay_ticks = MSG_RF_RANGE_BACKOFF_TICKS;
                    }
                } else {
                    s_pending_range_pong_active = false;
                }
            } else {
                s_pending_range_pong_delay_ticks = MSG_RF_RANGE_BACKOFF_TICKS;
            }
        }
    }

    if (s_ack_collect_active && !MSG_RF_AckEnabledNow()) {
        s_ack_collect_active = false;
        MSG_RF_RxChannelLockStop();
    }
    if (s_ack_collect_active) {
        if (s_ack_collect_ticks > 0u) --s_ack_collect_ticks;
        if (s_ack_collect_ticks == 0u) {
            s_ack_collect_active = false;
            MSG_RF_RxChannelLockStop();
        }
    }


    if (!MSG_RF_AckEnabledNow()) {
        for (uint8_t i = 0; i < MSG_RF_ACK_QUEUE_LEN; ++i) {
            s_pending_ack_queue[i].active = false;
        }
    }
    for (uint8_t i = 0; i < MSG_RF_ACK_QUEUE_LEN; ++i) {
        if (s_pending_ack_queue[i].active && s_pending_ack_queue[i].delay_ticks > 0u) {
            --s_pending_ack_queue[i].delay_ticks;
        }
    }
    if (MSG_RF_AckEnabledNow() &&
        gCurrentFunction != FUNCTION_TRANSMIT && !MSG_RF_AutoTxBusy()) {
        for (uint8_t i = 0; i < MSG_RF_ACK_QUEUE_LEN; ++i) {
            if (!s_pending_ack_queue[i].active || s_pending_ack_queue[i].delay_ticks != 0u) continue;
            uint8_t ack_frame[MSG_PKT_WIRE_LEN];
            /* GOGUFW 0.6.0: real ACK packet, not ACK-as-text.
             * ACK carries FROM/TO/ID/TTL fields so HEARD can show who acknowledged,
             * and future hop/reverse-hop handling has a proper addressable frame.
             * 1.0.1 ACK queue: do not let a later delayed ACK overwrite an
             * earlier one while the channel is busy or FSK RX is active. */
            if (MSG_PACKET_BuildAck(ack_frame, sizeof(ack_frame), s_pending_ack_queue[i].id,
                                    gMessengerConfig.callsign, s_pending_ack_queue[i].to) == MSG_PKT_WIRE_LEN) {
                if (MSG_RF_SendPacketFrameRepeatedOnVfo(ack_frame, false, true, s_pending_ack_queue[i].vfo, MSG_RF_ACK_REPEATS)) {
                    s_ack_dbg_sent_id = s_pending_ack_queue[i].id;
                    s_ack_dbg_sent_count++;
                    s_pending_ack_queue[i].active = false;
                }
            }
            break;
        }
    }

    if (s_wait_ack_active && !MSG_RF_AckEnabledNow()) {
        MSG_STORE_SetOutboxStatusById(s_wait_ack_id, MSG_STATUS_NONE);
        s_wait_ack_active = false;
        s_ack_collect_active = false;
        MSG_RF_RxChannelLockStop();
        s_ack_dbg_wait_active = 0u;
    }
    if (s_wait_ack_active) {
        if (s_wait_ack_ticks > 0u) {
            --s_wait_ack_ticks;
        }
        if (s_wait_ack_ticks == 0u) {
            if (s_wait_ack_retries == 0u &&
                gCurrentFunction != FUNCTION_TRANSMIT && !MSG_RF_AutoTxBusy()) {
                uint8_t retry_frame[MSG_PKT_WIRE_LEN];
                if (MSG_PACKET_BuildText(retry_frame, sizeof(retry_frame), s_wait_ack_id,
                                         gMessengerConfig.callsign, s_wait_ack_text,
                                         s_wait_ack_ttl) == MSG_PKT_WIRE_LEN &&
                    MSG_RF_SendPacketFrame(retry_frame, true, true)) {
                    s_wait_ack_retries = 1u;
                    s_ack_dbg_retry_count = 1u;
                    s_wait_ack_ticks = MSG_RF_ACK_TIMEOUT_TICKS;
                    MSG_RF_RxChannelLockStart(gEeprom.TX_VFO, MSG_RF_ACK_TIMEOUT_TICKS);
                }
            } else if (s_wait_ack_retries != 0u) {
                MSG_STORE_SetOutboxStatusById(s_wait_ack_id, MSG_STATUS_FAILED);
                s_wait_ack_active = false;
                MSG_RF_RxChannelLockStop();
                s_ack_dbg_wait_active = 0u;
            } else {
                /* Active FSK capture or carrier busy: do not collide; retry shortly. */
                s_wait_ack_ticks = MSG_RF_ACK_RETRY_GAP_TICKS;
            }
        }
    }
#endif

    MSG_RF_UpdateUnreadLed();

    if (!s_boot_prime_done && gMessengerConfig.msg_rx &&
        gCurrentFunction != FUNCTION_TRANSMIT && !MSG_RF_ChannelBusy()) {
        /* RF11: do the one-time RX/FSK prime at boot/global runtime, not only
         * after Messenger UI is opened or after the first sacrificial FSK burst. */
        MSG_RF_ArmSidecarIfIdle();
        MSG_RF_EnsureFskIrqMask();
        s_idle_reprime_ticks = 0u;
        s_boot_prime_done = true;
        /* 0.5.15: the first Range Check after a cold boot could TX the PING
         * but miss the PONG until Messenger had performed a message/ACK cycle.
         * Do one follow-up safe FSK RX re-prime shortly after boot prime, while
         * idle, to put the passive PING/PONG listener in the same ready state
         * Messenger reaches after its first TX/ACK exchange. */
        MSG_RF_RequestControlledReprime(20u);
    } else if (s_rearm_delay_ticks) {
        --s_rearm_delay_ticks;
    } else {
        MSG_RF_ArmSidecarIfIdle();
    }
}


static void MSG_RF_SendFskDataLongPreamble(uint16_t *pData)
{
#ifdef ENABLE_AIRCOPY
    unsigned int i;
    uint8_t timeout = 200;

    /* Same flow as BK4819_SendFSKData(), but with REG_59 preamble length set
     * to the maximum 16 bytes. This is a true preamble/lead-in, not a duplicate
     * message, so future ACK/MsgID logic remains clean.
     */
    SYSTEM_DelayMs(30);

    BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_FSK_TX_FINISHED);
    BK4819_WriteRegister(BK4819_REG_5D, MSG_RF_REG5D_LEN_100_BYTES);
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_TX_CLEAR_LONG_PRE);
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_TX_READY_LONG_PRE);

    for (i = 0; i < MSG_RF_WORDS; i++) {
        BK4819_WriteRegister(BK4819_REG_5F, pData[i]);
    }

    SYSTEM_DelayMs(20);
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_TX_START_LONG_PRE);

    while (timeout-- && (BK4819_ReadRegister(BK4819_REG_0C) & 1u) == 0u) {
        SYSTEM_DelayMs(5);
    }

    BK4819_WriteRegister(BK4819_REG_02, 0x0000);
    SYSTEM_DelayMs(30);
    BK4819_ResetFSK();
#else
    (void)pData;
#endif
}

static void bytes_to_aircopy_words(const uint8_t *bytes)
{
#ifdef ENABLE_AIRCOPY
    memset(g_FSK_Buffer, 0, sizeof(g_FSK_Buffer));
    g_FSK_Buffer[0] = 0xABCDu;
    for (uint8_t i = 0; i < (MSG_RF_BYTES_CARRIED / 2u); i++) {
        g_FSK_Buffer[1u + i] = (uint16_t)bytes[i * 2u] | ((uint16_t)bytes[i * 2u + 1u] << 8);
    }
    g_FSK_Buffer[1u + (MSG_RF_BYTES_CARRIED / 2u)] = MSG_PACKET_Crc16(bytes, MSG_PKT_WIRE_LEN);
    g_FSK_Buffer[MSG_RF_WORDS - 1u] = 0xDCBAu;
#else
    (void)bytes;
#endif
}

static bool MSG_RF_SendPacketFrame(const uint8_t *packet, bool count_tx, bool ignore_self_rx)
{
#ifdef ENABLE_AIRCOPY
    if (!packet) return false;
    if (MSG_RF_FskBusy()) return false;

    if (!s_voice_snapshot.valid) MSG_RF_CaptureVoiceSnapshot();
    MSG_RF_DisarmSidecarNoRadioReset();
    BK4819_ResetFSK();
    gFSKWriteIndex = 0;
    bytes_to_aircopy_words(packet);

    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true);
    MSG_RF_NarrowLockBegin();
    RADIO_SetTxParameters();
    BK4819_SetupAircopy();
    MSG_RF_SendFskDataLongPreamble(g_FSK_Buffer);
    BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
    MSG_RF_NarrowLockEnd();

    MSG_RF_HardRestoreVoicePath();
    s_post_tx_restore_ticks = MSG_RF_POST_TX_RESTORE_TICKS;
    MSG_RF_RequestControlledReprime(MSG_RF_REPRIME_DELAY_TICKS);
    if (count_tx) s_tx_count++;
    if (ignore_self_rx) s_ignore_next_self_rx = true;
    gUpdateDisplay = true;
    return true;
#else
    (void)packet; (void)count_tx; (void)ignore_self_rx;
    return false;
#endif
}

static bool MSG_RF_SendPacketFrameRepeated(const uint8_t *packet, bool count_tx, bool ignore_self_rx, uint8_t repeats)
{
#ifdef ENABLE_AIRCOPY
    if (!packet) return false;
    if (repeats == 0u) repeats = 1u;

    bool any_sent = false;
    for (uint8_t i = 0u; i < repeats; i++) {
        if (MSG_RF_SendPacketFrame(packet, count_tx, ignore_self_rx)) {
            any_sent = true;
        } else if (!any_sent) {
            return false;
        }

        if ((uint8_t)(i + 1u) < repeats) {
            SYSTEM_DelayMs(MSG_RF_REPEAT_GAP_MS);
            if (MSG_RF_FskBusy()) {
                break;
            }
        }
    }
    return any_sent;
#else
    (void)packet; (void)count_tx; (void)ignore_self_rx; (void)repeats;
    return false;
#endif
}

static bool MSG_RF_SendPacketFrameRepeatedOnVfo(const uint8_t *packet, bool count_tx, bool ignore_self_rx, uint8_t tx_vfo, uint8_t repeats)
{
#ifdef ENABLE_AIRCOPY
    const uint8_t old_tx_vfo = gEeprom.TX_VFO;
    bool ok;

    tx_vfo &= 1u;
    if (old_tx_vfo != tx_vfo) {
        gEeprom.TX_VFO = tx_vfo;
        RADIO_SelectVfos();
        RADIO_SetupRegisters(true);
        MSG_RF_EnsureFskIrqMask();
    }

    ok = MSG_RF_SendPacketFrameRepeated(packet, count_tx, ignore_self_rx, repeats);

    if (old_tx_vfo != tx_vfo) {
        gEeprom.TX_VFO = old_tx_vfo;
        RADIO_SelectVfos();
        RADIO_SetupRegisters(true);
        MSG_RF_EnsureFskIrqMask();
    }

    return ok;
#else
    (void)packet; (void)count_tx; (void)ignore_self_rx; (void)tx_vfo; (void)repeats;
    return false;
#endif
}

static void MSG_RF_RangeTxWarmupCurrentVfo(void)
{
#ifdef ENABLE_AIRCOPY
    /* 1.0.1b Range Check reliable TX path:
     * Single PING/PONG frames exposed a long-idle UV-K1 issue that the old
     * 3x PING burst used to hide: the first Range Check FSK TX could start
     * while the BK4829 sidecar/voice path was stale.  Messenger TX usually
     * refreshed the path first, which is why Range Check improved after using
     * Messenger.  Before every Range PING and automatic PONG, explicitly put
     * the radio back into a clean TX-ready state, clear local FSK RX state and
     * give the BK4829 a short settling time.  This is Range-only; normal
     * Messenger SEND/ACK path remains unchanged. */
    if (!s_voice_snapshot.valid) MSG_RF_CaptureVoiceSnapshot();

    MSG_RF_DisarmSidecarNoRadioReset();
    MSG_RF_NarrowLockEnd();
    BK4819_ResetFSK();

    s_rx_capture_active = false;
    s_rx_stale_ticks = 0u;
    s_rx_words = 0u;
    s_reprime_delay_ticks = 0u;
    s_safe_keepalive_ticks = MSG_RF_SAFE_KEEPALIVE_TICKS;
    gFSKWriteIndex = 0u;
    memset(g_FSK_Buffer, 0, sizeof(g_FSK_Buffer));

    RADIO_SelectVfos();
    RADIO_SetupRegisters(true);
    MSG_RF_EnsureFskIrqMask();
    MSG_RF_NarrowLockBegin();
    RADIO_SetTxParameters();
    BK4819_SetupAircopy();
    MSG_RF_NarrowLockEnd();
    SYSTEM_DelayMs(MSG_RF_RANGE_TX_WARMUP_MS);
#else
#endif
}

static bool MSG_RF_SendRangePacketFrameRepeatedOnVfo(const uint8_t *packet, bool count_tx, bool ignore_self_rx, uint8_t tx_vfo, uint8_t repeats)
{
#ifdef ENABLE_AIRCOPY
    const uint8_t old_tx_vfo = gEeprom.TX_VFO;
    bool ok;

    tx_vfo &= 1u;
    if (old_tx_vfo != tx_vfo) {
        gEeprom.TX_VFO = tx_vfo;
        RADIO_SelectVfos();
        RADIO_SetupRegisters(true);
        MSG_RF_EnsureFskIrqMask();
    }

    MSG_RF_RangeTxWarmupCurrentVfo();
    ok = MSG_RF_SendPacketFrameRepeated(packet, count_tx, ignore_self_rx, repeats);

    if (old_tx_vfo != tx_vfo) {
        gEeprom.TX_VFO = old_tx_vfo;
        RADIO_SelectVfos();
        RADIO_SetupRegisters(true);
        MSG_RF_EnsureFskIrqMask();
    }

    return ok;
#else
    (void)packet; (void)count_tx; (void)ignore_self_rx; (void)tx_vfo; (void)repeats;
    return false;
#endif
}

static void clamp_rf_text(char *dst, const char *src)
{
    uint8_t i = 0;
    if (!src || !src[0]) src = "EMPTY";
    for (; i < MSG_RF_TEXT_LIMIT && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static bool parse_aircopy_native_packet(MSG_Packet_t *pkt)
{
#ifdef ENABLE_AIRCOPY
    /*
     * 1.0.1e RX alignment hotfix:
     * Do not require the Aircopy header to be exactly at g_FSK_Buffer[0].
     * If a stale word or an early FIFO timing difference shifts the frame,
     * short packets (PING/PONG/ACK) never reach their handlers.  Scan the
     * captured words for the 0xABCD header and validate the native GGFW packet
     * magic/version/type/CRC before accepting it.
     */
    const uint8_t data_words = (uint8_t)(MSG_RF_BYTES_CARRIED / 2u);
    const uint8_t words_available = (s_rx_words > 0u && s_rx_words <= MSG_RF_WORDS) ? s_rx_words : MSG_RF_WORDS;

    for (uint8_t off = 0u; off + 1u + data_words <= words_available; off++) {
        if (g_FSK_Buffer[off] != 0xABCDu) continue;

        uint8_t bytes[MSG_RF_BYTES_CARRIED];
        for (uint8_t i = 0u; i < data_words; i++) {
            const uint16_t w = g_FSK_Buffer[off + 1u + i];
            bytes[i * 2u] = (uint8_t)(w & 0xFFu);
            bytes[i * 2u + 1u] = (uint8_t)(w >> 8);
        }

        if (bytes[0] != MSG_PKT_MAGIC0 || bytes[1] != MSG_PKT_MAGIC1 ||
            bytes[2] != MSG_PKT_MAGIC2 || bytes[3] != MSG_PKT_MAGIC3) continue;
        if (bytes[4] != MSG_PKT_VERSION) continue;
        if (bytes[5] != MSG_PKT_TYPE_TEXT && bytes[5] != MSG_PKT_TYPE_ACK &&
            bytes[5] != MSG_PKT_TYPE_PING && bytes[5] != MSG_PKT_TYPE_PONG) continue;
        if (bytes[27] > MSG_RF_TEXT_LIMIT) continue;

        const uint16_t got = (uint16_t)bytes[MSG_PKT_WIRE_LEN - 2u] |
                             ((uint16_t)bytes[MSG_PKT_WIRE_LEN - 1u] << 8);
        const uint16_t want = MSG_PACKET_Crc16(bytes, MSG_PKT_WIRE_LEN - 2u);
        if (got != want) continue;

        memset(pkt, 0, sizeof(*pkt));
        pkt->type = bytes[5];
        pkt->flags = bytes[6];
        pkt->id = (uint16_t)bytes[7] | ((uint16_t)bytes[8] << 8);
        pkt->ttl_init = bytes[9];
        pkt->ttl_remain = bytes[10];
        memcpy(pkt->from, &bytes[11], MSG_CALLSIGN_LEN);
        pkt->from[MSG_CALLSIGN_LEN] = 0;
        memcpy(pkt->to, &bytes[19], MSG_CALLSIGN_LEN);
        pkt->to[MSG_CALLSIGN_LEN] = 0;
        pkt->payload_len = bytes[27];
        memcpy(pkt->payload, &bytes[28], pkt->payload_len);
        pkt->payload[pkt->payload_len] = 0;
        if (pkt->type == MSG_PKT_TYPE_ACK || pkt->type == MSG_PKT_TYPE_PING || pkt->type == MSG_PKT_TYPE_PONG) return true;
        return pkt->payload[0] != 0;
    }

    return false;
#else
    (void)pkt;
    return false;
#endif
}


static int8_t MSG_RF_CurrentRSSIdBm(void)
{
    uint16_t raw = BK4819_GetRSSI();
    int16_t dbm = (int16_t)(raw / 2u) - 160;
    if (dbm < -127) dbm = -127;
    if (dbm > 0) dbm = 0;
    return (int8_t)dbm;
}

static void MSG_RF_QueueRangePong(uint16_t id, const char *to, uint8_t rx_vfo)
{
    MSG_STORE_Init();
    s_store_initialized = true;
    if (!gMessengerConfig.rng_rsp) return;

    /* 1.0.1c: With Range PING reduced to x1, a permanent same id/from
     * duplicate filter can suppress later manual Range Check attempts if the
     * sender reuses an id.  Keep the old burst protection only for a short
     * window: suppress repeated same-id PINGs from the same station for 2s,
     * then allow a new PONG even if the id is reused. */
    if (s_last_range_ping_id == id &&
        strncmp(s_last_range_ping_from, to ? to : "", MSG_CALLSIGN_LEN) == 0 &&
        s_last_range_ping_age_ticks < MSG_RF_RANGE_PING_DUP_WINDOW_TICKS) {
        return;
    }
    s_last_range_ping_id = id;
    s_last_range_ping_age_ticks = 0u;
    memset(s_last_range_ping_from, 0, sizeof(s_last_range_ping_from));
    if (to && to[0]) strncpy(s_last_range_ping_from, to, MSG_CALLSIGN_LEN);

    s_pending_range_pong_active = true;
    s_pending_range_pong_delay_ticks = MSG_RF_RandomRangeDelayTicks(id);
    s_pending_range_pong_id = id;
    s_pending_range_pong_vfo = (uint8_t)(rx_vfo & 1u);
    memset(s_pending_range_pong_to, 0, sizeof(s_pending_range_pong_to));
    if (to && to[0]) strncpy(s_pending_range_pong_to, to, MSG_CALLSIGN_LEN);
    else strncpy(s_pending_range_pong_to, MSG_PKT_TO_ALL, MSG_CALLSIGN_LEN);
}

static void MSG_RF_QueueAck(uint16_t id, const char *to, uint8_t rx_vfo)
{
    char ack_to[MSG_CALLSIGN_LEN + 1];
    memset(ack_to, 0, sizeof(ack_to));
    if (to && to[0]) strncpy(ack_to, to, MSG_CALLSIGN_LEN);
    else strncpy(ack_to, MSG_PKT_TO_ALL, MSG_CALLSIGN_LEN);

    if (!MSG_RF_AckEnabledNow()) {
        for (uint8_t i = 0; i < MSG_RF_ACK_QUEUE_LEN; ++i) {
            s_pending_ack_queue[i].active = false;
        }
        return;
    }

    /* Avoid duplicate queued ACKs for the same received packet/source.
     * This still allows a later retry frame to get a fresh ACK after the
     * previous queued ACK has already been sent. */
    for (uint8_t i = 0; i < MSG_RF_ACK_QUEUE_LEN; ++i) {
        if (!s_pending_ack_queue[i].active) continue;
        if (s_pending_ack_queue[i].id == id && strncmp(s_pending_ack_queue[i].to, ack_to, MSG_CALLSIGN_LEN) == 0) {
            return;
        }
    }

    for (uint8_t i = 0; i < MSG_RF_ACK_QUEUE_LEN; ++i) {
        if (s_pending_ack_queue[i].active) continue;
        s_pending_ack_queue[i].active = true;
        s_pending_ack_queue[i].delay_ticks = MSG_RF_RandomAckDelayTicks(id);
        s_pending_ack_queue[i].id = id;
        s_pending_ack_queue[i].vfo = (uint8_t)(rx_vfo & 1u);
        memset(s_pending_ack_queue[i].to, 0, sizeof(s_pending_ack_queue[i].to));
        strncpy(s_pending_ack_queue[i].to, ack_to, MSG_CALLSIGN_LEN);
        return;
    }

    /* Queue full: keep the older ACKs rather than overwriting/dropping a
     * delayed ACK that is already waiting for the channel to clear. */
}

/* RF24 daily-v1 ACK fallback: ACK is carried as an ordinary text message
 * payload ("ACK:hhhh") instead of the special short ACK packet type.
 * Normal text RF parsing is already proven reliable; this avoids the separate
 * ACK frame parser path that stayed R0 even when the ACK FSK burst was audible.
 */
static bool MSG_RF_ParseAckText(const char *text, uint16_t *id)
{
    if (!text || !id) return false;
    if (text[0] != 'A' || text[1] != 'C' || text[2] != 'K' || text[3] != ':') return false;
    uint16_t v = 0;
    for (uint8_t i = 0; i < 4u; i++) {
        char c = text[4u + i];
        uint8_t n;
        if (c >= '0' && c <= '9') n = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F') n = (uint8_t)(10 + c - 'A');
        else if (c >= 'a' && c <= 'f') n = (uint8_t)(10 + c - 'a');
        else return false;
        v = (uint16_t)((v << 4) | n);
    }
    *id = v;
    return true;
}

static void MSG_RF_HandleAckFor(uint16_t ack_id, const char *ack_from)
{
    s_ack_dbg_rx_id = ack_id;
    s_ack_dbg_rx_count++;

    if (s_wait_ack_active) {
        if (ack_id == s_wait_ack_id) {
            s_ack_dbg_match_count++;
            MSG_STORE_AddOutboxAckSourceById(s_wait_ack_id, ack_from);
            MSG_STORE_SetOutboxStatusById(s_wait_ack_id, MSG_STATUS_ACKED);

            /* 1.0.1 follow-up: first ACK means delivered and cancels retry,
             * but keep a short same-channel receive window open so the Sent
             * READ screen can collect up to three ACK source IDs. */
            s_wait_ack_active = false;
            s_ack_collect_active = true;
            s_ack_collect_id = ack_id;
            s_ack_collect_ticks = MSG_RF_ACK_COLLECT_TICKS;
            MSG_RF_RxChannelLockStart(gEeprom.TX_VFO, MSG_RF_ACK_COLLECT_TICKS);
            s_ack_dbg_wait_active = 0u;
            MSG_RF_RequestAckSuccessBeep();
        } else {
            s_ack_dbg_miss_count++;
        }
    } else if (s_ack_collect_active && ack_id == s_ack_collect_id) {
        MSG_STORE_AddOutboxAckSourceById(ack_id, ack_from);
        MSG_STORE_SetOutboxStatusById(ack_id, MSG_STATUS_ACKED);
        /* Do not beep again; this is an extra ACK source for the same sent message. */
    } else {
        MSG_STORE_AddOutboxAckSourceById(ack_id, ack_from);
        MSG_STORE_SetOutboxStatusById(ack_id, MSG_STATUS_ACKED);
        MSG_RF_RequestAckSuccessBeep();
    }
    gUpdateDisplay = true;
}

static void try_store_rx_packet(void)
{
#ifdef ENABLE_AIRCOPY
    MSG_Packet_t pkt;
    if (!parse_aircopy_native_packet(&pkt)) return;

    if (pkt.type == MSG_PKT_TYPE_PING) {
        MSG_HeardUpdate(pkt.from, MSG_RF_CurrentRSSIdBm(), MSG_PKT_TYPE_PING);
        if (strncmp(pkt.from, gMessengerConfig.callsign, MSG_CALLSIGN_LEN) != 0) {
            MSG_RF_QueueRangePong(pkt.id, pkt.from, gEeprom.RX_VFO);
            MSG_RF_RequestRangeBeep();
        }
        MSG_RF_FinishRxAttempt(false);
        gUpdateDisplay = true;
        return;
    }

    if (pkt.type == MSG_PKT_TYPE_PONG) {
        if (strncmp(pkt.from, gMessengerConfig.callsign, MSG_CALLSIGN_LEN) != 0) {
            uint16_t remote_battery = 0u;
            if (pkt.payload_len >= 2u) {
                remote_battery = (uint16_t)(uint8_t)pkt.payload[0] | ((uint16_t)(uint8_t)pkt.payload[1] << 8);
            }
            MSG_RangeOnPong(pkt.from, MSG_RF_CurrentRSSIdBm(), remote_battery);
            MSG_RF_RxChannelLockStop();
            MSG_RF_RequestRangeBeep();
        }
        MSG_RF_FinishRxAttempt(false);
        gUpdateDisplay = true;
        return;
    }

    if (pkt.type == MSG_PKT_TYPE_ACK) {
        MSG_HeardUpdate(pkt.from, MSG_RF_CurrentRSSIdBm(), MSG_PKT_TYPE_ACK);
        uint16_t ack_id = pkt.id;
        if (pkt.payload_len >= 2u) {
            ack_id = (uint16_t)(uint8_t)pkt.payload[0] | ((uint16_t)(uint8_t)pkt.payload[1] << 8);
        }
        MSG_RF_HandleAckFor(ack_id, pkt.from);
        MSG_RF_FinishRxAttempt(true);
        return;
    }

    if (pkt.type != MSG_PKT_TYPE_TEXT) return;

    uint16_t ack_text_id;
    if (MSG_RF_ParseAckText(pkt.payload, &ack_text_id)) {
        MSG_HeardUpdate(pkt.from, MSG_RF_CurrentRSSIdBm(), MSG_PKT_TYPE_ACK);
        /* RF28: text ACK is now proven enough for debugging; keep it out of
         * Inbox for daily use and only update the matching Sent status. */
        MSG_RF_HandleAckFor(ack_text_id, pkt.from);
        MSG_RF_FinishRxAttempt(true);
        gUpdateDisplay = true;
        return;
    }

    if (strncmp(pkt.from, gMessengerConfig.callsign, MSG_CALLSIGN_LEN) == 0 && s_ignore_next_self_rx) {
        s_ignore_next_self_rx = false;
        return;
    }

    MSG_HeardUpdate(pkt.from, MSG_RF_CurrentRSSIdBm(), MSG_PKT_TYPE_TEXT);

    const bool duplicate = MSG_STORE_IsDuplicateInbox(pkt.from, pkt.id);

    /* Stage 2 ACK: queue the ACK after a valid text frame is fully parsed.
     * Duplicate retry frames still get an ACK resend, but must not create a
     * second inbox entry or trigger beep/unread state. */
    MSG_RF_QueueAck(pkt.id, pkt.from, gEeprom.RX_VFO);

    if (duplicate) {
        gBeepToPlay = BEEP_NONE;
        MSG_RF_FinishRxAttempt(false);
        gUpdateDisplay = true;
        return;
    }

    MSG_STORE_AddInboxMessage(pkt.payload, pkt.from, pkt.to, pkt.id, pkt.ttl_init, pkt.ttl_remain, true);

    /* The store layer sets gBeepToPlay, but that is only consumed reliably by
     * key/UI paths. For real RF messages RF12 uses the global deferred beep
     * handler instead, so clear the UI-local request to avoid delayed Inbox beeps. */
    gBeepToPlay = BEEP_NONE;
    s_decode_count++;
    MSG_RF_FinishRxAttempt(true);
    gUpdateDisplay = true;
#endif
}

void MSG_RF_OnRadioInterrupt(uint16_t status)
{
    if (gSurvivalMode) return;
#ifdef ENABLE_AIRCOPY
    const bool fsk_sync    = (status & BK4819_REG_02_FSK_RX_SYNC) != 0;
    const bool fifo_full   = (status & BK4819_REG_02_FSK_FIFO_ALMOST_FULL) != 0;
    const bool rx_finished = (status & BK4819_REG_02_FSK_RX_FINISHED) != 0;

    if (!s_sidecar_armed) return;

    if (fsk_sync || (BK4819_ReadRegister(BK4819_REG_0B) & ((1u << 6) | (1u << 7)))) {
        s_sync_count++;
        /*
         * 1.0.1e RX alignment hotfix:
         * A new FSK sync means a new Aircopy frame is starting.  If the local
         * word buffer still contains a stale/partial previous frame, short
         * packets such as PING/PONG/ACK can be shifted and then rejected before
         * they ever reach the packet-type handler.  Reset the capture buffer at
         * the beginning of a new capture, but do not wipe an already active one.
         */
        if (!s_rx_capture_active) {
            s_rx_words = 0;
            gFSKWriteIndex = 0;
            memset(g_FSK_Buffer, 0, sizeof(g_FSK_Buffer));
        }
        s_rx_capture_active = true;
        s_rx_stale_ticks = MSG_RF_RX_STALE_TICKS;
        MSG_RF_NarrowLockBegin();
        MSG_RF_MuteFskAudio();
    }

    if (fifo_full) {
        for (uint8_t i = 0; i < 4u && s_rx_words < MSG_RF_WORDS; i++) {
            g_FSK_Buffer[s_rx_words++] = BK4819_ReadRegister(BK4819_REG_5F);
        }
        gFSKWriteIndex = s_rx_words;
        s_fifo_count++;
        s_rx_capture_active = true;
        s_rx_stale_ticks = MSG_RF_RX_STALE_TICKS;
        MSG_RF_NarrowLockBegin();
        MSG_RF_MuteFskAudio();
    }

    if (rx_finished) {
        while (s_rx_words < MSG_RF_WORDS) {
            g_FSK_Buffer[s_rx_words++] = BK4819_ReadRegister(BK4819_REG_5F);
        }
        gFSKWriteIndex = s_rx_words;
        s_fifo_count++;
        try_store_rx_packet();
        if (s_rx_capture_active) MSG_RF_FinishRxAttempt(false);
    }
#else
    (void)status;
#endif
}


static void MSG_RF_RangeForceRxReprime(void)
{
#ifdef ENABLE_AIRCOPY
    /* 0.6.1 Range Check hardening:
     * Immediately before the PONG wait window, refresh only the FSK sidecar
     * receive path.  Do not touch REG_47/AF speaker routing or globally change
     * normal voice state.  This targets the long-idle Range Check case where
     * PING TX appears to happen but the following PONG decode window is stale. */
    if (!gMessengerConfig.msg_rx) return;
    if (gCurrentFunction == FUNCTION_TRANSMIT) return;

    if (!s_voice_snapshot.valid) MSG_RF_CaptureVoiceSnapshot();
    s_sidecar_armed = true;
    s_rx_capture_active = false;
    s_rx_stale_ticks = 0u;
    s_rx_words = 0u;
    gFSKWriteIndex = 0u;
    memset(g_FSK_Buffer, 0, sizeof(g_FSK_Buffer));

    BK4819_SetupAircopy();
    BK4819_WriteRegister(BK4819_REG_5D, MSG_RF_REG5D_LEN_100_BYTES);
    BK4819_WriteRegister(BK4819_REG_02, 0x0000);
    MSG_RF_EnsureFskIrqMask();
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_RX_CLEAR);
    BK4819_WriteRegister(BK4819_REG_59, MSG_RF_REG59_RX_ENABLE);

    s_reprime_delay_ticks = 0u;
    s_safe_keepalive_ticks = MSG_RF_SAFE_KEEPALIVE_TICKS;
    s_sidecar_count++;
#endif
}

bool MSG_RF_SendRangePing(void)
{
    if (gSurvivalMode) return false;
    MSG_RF_EnsureStoreInitialized();
#ifdef ENABLE_AIRCOPY
    /* 0.6.1: make Range Check self-healing after long idle.  Restore any
     * stale FSK/voice remnants before building and sending the PING; normal
     * Messenger TX still uses the proven generic path. */
    MSG_RF_HardRestoreVoicePath();

    uint8_t packet[MSG_PKT_WIRE_LEN];
    const uint16_t id = MSG_STORE_NextMsgId();
    if (MSG_PACKET_BuildPing(packet, sizeof(packet), id, gMessengerConfig.callsign) != MSG_PKT_WIRE_LEN) {
        return false;
    }
    bool ping_sent = MSG_RF_SendRangePacketFrameRepeatedOnVfo(packet, true, true, gEeprom.TX_VFO, MSG_RF_RANGE_PING_REPEATS);
    if (!ping_sent) {
        /* 1.0.1 follow-up: Range Check is user-triggered; if the first TX attempt
         * is refused by a stale FSK/RX state, force the same voice-path recovery
         * and try once more instead of silently entering an empty wait screen. */
        MSG_RF_HardRestoreVoicePath();
        SYSTEM_DelayMs(80u);
        ping_sent = MSG_RF_SendRangePacketFrameRepeatedOnVfo(packet, true, true, gEeprom.TX_VFO, MSG_RF_RANGE_PING_REPEATS);
    }
    if (!ping_sent) return false;
    SYSTEM_DelayMs(50u);
    MSG_RF_RangeForceRxReprime();
    MSG_RF_RxChannelLockStart(gEeprom.TX_VFO, MSG_RF_RANGE_WAIT_PONG_TICKS);
    /* 0.5.15: for Range Check, enter the PONG listening window as soon as
     * possible after the PING TX restore.  The generic post-TX re-prime delay
     * is fine for normal Messenger, but the first cold-boot range test needs
     * the RX sidecar refreshed immediately so the early PONG is not missed. */
    MSG_RF_RequestControlledReprime(1u);
    gUpdateDisplay = true;
    return true;
#else
    return false;
#endif
}

bool MSG_RF_SendText(const char *text)
{
    if (gSurvivalMode) return false;
    MSG_RF_EnsureStoreInitialized();
#ifdef ENABLE_AIRCOPY
    uint8_t packet[MSG_PKT_WIRE_LEN];
    char rf_text[MSG_RF_TEXT_LIMIT + 1u];
    clamp_rf_text(rf_text, text);

    const uint16_t id = MSG_STORE_NextMsgId();
    const uint8_t ttl = gMessengerConfig.msg_hop ? gMessengerConfig.msg_hop : 1u;

    if (MSG_PACKET_BuildText(packet, sizeof(packet), id, gMessengerConfig.callsign, rf_text, ttl) != MSG_PKT_WIRE_LEN) {
        return false;
    }

    if (!MSG_RF_SendPacketFrame(packet, true, true)) return false;

    MSG_STORE_AddOutboxMessage(rf_text, gMessengerConfig.callsign, MSG_PKT_TO_ALL, id, ttl, ttl);
    if (MSG_RF_AckEnabledNow()) {
        s_wait_ack_active = true;
        s_wait_ack_id = id;
        s_ack_dbg_pending_id = id;
        s_ack_dbg_wait_active = 1u;
        s_wait_ack_ticks = MSG_RF_ACK_TIMEOUT_TICKS;
        s_wait_ack_retries = 0u;
        s_ack_dbg_retry_count = 0u;
        s_wait_ack_ttl = ttl;
        memset(s_wait_ack_text, 0, sizeof(s_wait_ack_text));
        strncpy(s_wait_ack_text, rf_text, MSG_TEXT_LEN);
        MSG_RF_RxChannelLockStart(gEeprom.TX_VFO, MSG_RF_ACK_TIMEOUT_TICKS);
        MSG_RF_RequestControlledReprime(1u);
    } else {
        MSG_STORE_SetOutboxStatusById(id, MSG_STATUS_NONE);
        s_wait_ack_active = false;
        MSG_RF_RxChannelLockStop();
        s_ack_dbg_pending_id = 0u;
        s_ack_dbg_wait_active = 0u;
    }

#ifdef ENABLE_AIRCOPY
    /* RF27: do not immediately poke FSK registers right after TX.
     * Schedule the same safe controlled re-prime used globally; ACK is delayed
     * long enough to arrive after this window. */
    MSG_RF_RequestControlledReprime(MSG_RF_REPRIME_DELAY_TICKS);
#endif

    gUpdateDisplay = true;
    return true;
#else
    (void)text;
    return false;
#endif
}

uint16_t MSG_RF_GetAckDbgPendingId(void) { return s_ack_dbg_pending_id; }
uint16_t MSG_RF_GetAckDbgSentId(void) { return s_ack_dbg_sent_id; }
uint16_t MSG_RF_GetAckDbgRxId(void) { return s_ack_dbg_rx_id; }
uint8_t MSG_RF_GetAckDbgSentCount(void) { return s_ack_dbg_sent_count; }
uint8_t MSG_RF_GetAckDbgRxCount(void) { return s_ack_dbg_rx_count; }
uint8_t MSG_RF_GetAckDbgMatchCount(void) { return s_ack_dbg_match_count; }
uint8_t MSG_RF_GetAckDbgMissCount(void) { return s_ack_dbg_miss_count; }
uint8_t MSG_RF_GetAckDbgWaitActive(void) { return s_ack_dbg_wait_active; }
uint8_t MSG_RF_GetAckDbgRetryCount(void) { return s_ack_dbg_retry_count; }

uint8_t MSG_RF_GetTxCount(void) { return s_tx_count; }
uint8_t MSG_RF_GetSyncCount(void) { return s_sync_count; }
uint8_t MSG_RF_GetFifoCount(void) { return s_fifo_count; }
uint8_t MSG_RF_GetDecodeCount(void) { return s_decode_count; }
uint8_t MSG_RF_GetRestoreCount(void) { return s_restore_count; }
uint8_t MSG_RF_GetSidecarCount(void) { return s_sidecar_count; }
uint8_t MSG_RF_GetOpenTicks(void) { return s_dbg_open_ticks; }
uint8_t MSG_RF_GetLastDecodeOpen(void) { return s_dbg_last_decode_open; }
uint16_t MSG_RF_GetDbg02(void) { return s_dbg_02; }
uint16_t MSG_RF_GetDbg0B(void) { return s_dbg_0b; }
uint16_t MSG_RF_GetDbg0C(void) { return s_dbg_0c; }
uint16_t MSG_RF_GetDbg30(void) { return s_dbg_30; }
uint16_t MSG_RF_GetDbg3F(void) { return s_dbg_3f; }
uint16_t MSG_RF_GetDbg47(void) { return s_dbg_47; }
uint16_t MSG_RF_GetDbg58(void) { return s_dbg_58; }
uint16_t MSG_RF_GetDbg59(void) { return s_dbg_59; }
uint16_t MSG_RF_GetDbg67(void) { return s_dbg_67; }
