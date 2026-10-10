/* Experimental FM-broadcast RDS decoder for an externally wired EARO input.
 *
 * ADC1 channel 9 (PB1) is sampled at 64 ksample/s.  The 57 kHz carrier aliases
 * cleanly to 7 kHz; a complex mixer then moves it to baseband.
 * low-pass filter, eight biphase timing hypotheses and differential detector
 * feed an RDS CRC/offset-word synchronizer.  This deliberately uses integer
 * arithmetic so the 48 MHz Cortex-M0+ can run it while the FM UI is active.
 */

#ifdef ENABLE_RDS_PROBE

#include <string.h>

#include "board.h"
#include "driver/rds_decoder.h"
#include "py32f071_ll_adc.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_dma.h"
#include "py32f071_ll_gpio.h"
#include "py32f071_ll_system.h"
#include "py32f071_ll_tim.h"

#define RDS_DMA_CHANNEL       LL_DMA_CHANNEL_1
#define RDS_SAMPLE_TIMER      TIM3
#define RDS_DMA_SAMPLES       1024u
#define RDS_HALF_SAMPLES      (RDS_DMA_SAMPLES / 2u)
#define RDS_LANES             8u
#define RDS_DECIMATION        4u
#define RDS_BIT_PHASE_STEP    0x13000000u /* 1187.5 / 16000 in Q0.32 */
#define RDS_DMA_HALF_FIRST    0x01u
#define RDS_DMA_HALF_SECOND   0x02u
#define RDS_BLOCK_MASK        0x03FFFFFFu
#define RDS_POLY              0x05B9u
#define RDS_OFFSET_A          0x00FCu
#define RDS_OFFSET_B          0x0198u
#define RDS_OFFSET_C          0x0168u
#define RDS_OFFSET_CP         0x0350u
#define RDS_OFFSET_D          0x01B4u

typedef struct {
    uint32_t phase;
    int32_t accum_i;
    int32_t accum_q;
    int16_t prev_i;
    int16_t prev_q;
    bool have_previous;

    uint32_t search;
    uint32_t search_inv;
    uint32_t block;
    uint16_t words[4];
    uint8_t block_pos;
    uint8_t expected;
    bool inverted;
    bool locked;
} RDS_Lane_t;

static uint16_t s_dma[RDS_DMA_SAMPLES];
static RDS_Lane_t s_lanes[RDS_LANES];
static volatile bool s_active;
static volatile RDS_Snapshot_t s_snapshot;
static int8_t s_selected_lane;

static int32_t s_dc_q8;
static int32_t s_lp_i;
static int32_t s_lp_q;
static uint8_t s_carrier_phase;
static uint8_t s_decimation_phase;
static uint32_t s_raw_sum;
static uint32_t s_baseband_sum;
static uint32_t s_adc_sum;
static uint16_t s_metric_count;
static uint16_t s_window_min;
static uint16_t s_window_max;

static const int8_t s_cos64[64] = {
    127,126,125,122,117,112,106,98,90,81,71,60,49,37,25,12,
    0,-12,-25,-37,-49,-60,-71,-81,-90,-98,-106,-112,-117,-122,-125,-126,
    -127,-126,-125,-122,-117,-112,-106,-98,-90,-81,-71,-60,-49,-37,-25,-12,
    0,12,25,37,49,60,71,81,90,98,106,112,117,122,125,126
};

static const int8_t s_sin64[64] = {
    0,-12,-25,-37,-49,-60,-71,-81,-90,-98,-106,-112,-117,-122,-125,-126,
    -127,-126,-125,-122,-117,-112,-106,-98,-90,-81,-71,-60,-49,-37,-25,-12,
    0,12,25,37,49,60,71,81,90,98,106,112,117,122,125,126,
    127,126,125,122,117,112,106,98,90,81,71,60,49,37,25,12
};

static uint32_t abs32(const int32_t value)
{
    return (uint32_t)(value < 0 ? -value : value);
}

static uint16_t RDS_Syndrome(uint32_t block)
{
    for (int8_t bit = 25; bit >= 10; --bit) {
        if ((block & (1u << bit)) != 0u)
            block ^= (uint32_t)RDS_POLY << (bit - 10);
    }
    return (uint16_t)(block & 0x03FFu);
}

static bool RDS_ExpectedSyndrome(const uint8_t expected, const uint16_t syndrome)
{
    if (expected == 0u) return syndrome == RDS_OFFSET_A;
    if (expected == 1u) return syndrome == RDS_OFFSET_B;
    if (expected == 2u) return syndrome == RDS_OFFSET_C || syndrome == RDS_OFFSET_CP;
    return syndrome == RDS_OFFSET_D;
}

static char RDS_Printable(const uint8_t value)
{
    return (value >= 32u && value <= 126u) ? (char)value : ' ';
}

static void RDS_ParseGroup(RDS_Lane_t *lane)
{
    const uint16_t block_b = lane->words[1];
    const uint8_t group_type = (uint8_t)(block_b >> 12);
    const bool version_b = (block_b & 0x0800u) != 0u;

    /* Version B repeats PI in block C; reject an internally inconsistent
       group before it can update the displayed station name. */
    if (version_b && lane->words[2] != lane->words[0])
        return;

    s_snapshot.pi = lane->words[0];
    if (group_type == 0u) {
        const uint8_t segment = (uint8_t)(block_b & 3u);
        const uint16_t block_d = lane->words[3];
        s_snapshot.ps[segment * 2u] = RDS_Printable((uint8_t)(block_d >> 8));
        s_snapshot.ps[segment * 2u + 1u] = RDS_Printable((uint8_t)block_d);
        s_snapshot.ps[8] = '\0';
        s_snapshot.ps_mask |= (uint8_t)(1u << segment);
    }

    s_snapshot.stage = (s_snapshot.ps_mask == 0x0Fu) ? RDS_STAGE_PS : RDS_STAGE_SYNC;
}

static void RDS_LoseLane(const uint8_t lane_index, RDS_Lane_t *lane)
{
    lane->locked = false;
    lane->expected = 0u;
    lane->block_pos = 0u;
    if (s_selected_lane == (int8_t)lane_index) {
        s_snapshot.block_errors++;
        s_selected_lane = -1;
        if (s_snapshot.stage >= RDS_STAGE_SYNC)
            s_snapshot.stage = RDS_STAGE_CARRIER;
    }
}

static void RDS_DecodeBit(const uint8_t lane_index, RDS_Lane_t *lane, const uint8_t bit)
{
    lane->search = ((lane->search << 1) | bit) & RDS_BLOCK_MASK;
    lane->search_inv = ((lane->search_inv << 1) | (bit ^ 1u)) & RDS_BLOCK_MASK;

    if (!lane->locked) {
        uint32_t candidate = lane->search;
        bool inverted = false;
        if (RDS_Syndrome(candidate) != RDS_OFFSET_A) {
            candidate = lane->search_inv;
            inverted = true;
            if (RDS_Syndrome(candidate) != RDS_OFFSET_A)
                return;
        }

        lane->locked = true;
        lane->inverted = inverted;
        lane->expected = 1u;
        lane->block_pos = 0u;
        lane->block = 0u;
        lane->words[0] = (uint16_t)(candidate >> 10);
        return;
    }

    lane->block = ((lane->block << 1) | (bit ^ (lane->inverted ? 1u : 0u))) & RDS_BLOCK_MASK;
    if (++lane->block_pos < 26u)
        return;

    lane->block_pos = 0u;
    const uint16_t syndrome = RDS_Syndrome(lane->block);
    if (!RDS_ExpectedSyndrome(lane->expected, syndrome)) {
        RDS_LoseLane(lane_index, lane);
        return;
    }

    lane->words[lane->expected] = (uint16_t)(lane->block >> 10);
    if (s_selected_lane < 0)
        s_selected_lane = (int8_t)lane_index;
    if (s_selected_lane == (int8_t)lane_index)
        s_snapshot.valid_blocks++;

    if (lane->expected == 3u) {
        if (s_selected_lane == (int8_t)lane_index)
            RDS_ParseGroup(lane);
        lane->expected = 0u;
    } else {
        ++lane->expected;
    }
    lane->block = 0u;
}

static void RDS_FinishSymbol(const uint8_t index, RDS_Lane_t *lane)
{
    int32_t current_i = lane->accum_i >> 10;
    int32_t current_q = lane->accum_q >> 10;
    if (current_i > 32767) current_i = 32767;
    if (current_i < -32768) current_i = -32768;
    if (current_q > 32767) current_q = 32767;
    if (current_q < -32768) current_q = -32768;

    if (lane->have_previous) {
        const int32_t dot = current_i * lane->prev_i + current_q * lane->prev_q;
        RDS_DecodeBit(index, lane, dot < 0 ? 1u : 0u);
    }
    lane->prev_i = (int16_t)current_i;
    lane->prev_q = (int16_t)current_q;
    lane->have_previous = true;
    lane->accum_i = 0;
    lane->accum_q = 0;
}

static void RDS_UpdateMetrics(void)
{
    const uint32_t raw = s_raw_sum / 2048u;
    const uint32_t baseband = s_baseband_sum / 2048u;
    const uint16_t span = s_window_max - s_window_min;
    uint32_t quality = (baseband * 100u) / (raw + 1u);
    if (quality > 99u) quality = 99u;

    s_snapshot.carrier_quality = (uint8_t)quality;
    s_snapshot.input_span = (uint8_t)(span > 255u ? 255u : span);
    s_snapshot.input_level = (uint16_t)(s_adc_sum / 2048u);
    if (s_snapshot.stage < RDS_STAGE_SYNC) {
        if (span < 12u)
            s_snapshot.stage = RDS_STAGE_NO_INPUT;
        else if (quality < 2u)
            s_snapshot.stage = RDS_STAGE_NO_57K;
        else
            s_snapshot.stage = RDS_STAGE_CARRIER;
    }

    s_raw_sum = 0u;
    s_baseband_sum = 0u;
    s_adc_sum = 0u;
    s_metric_count = 0u;
    s_window_min = 0xFFFFu;
    s_window_max = 0u;
}

static void RDS_ProcessSamples(const uint16_t *samples, const uint16_t count)
{
    for (uint16_t n = 0u; n < count; ++n) {
        const uint16_t raw = samples[n] & 0x0FFFu;
        s_dc_q8 += ((((int32_t)raw << 8) - s_dc_q8) >> 10);
        const int32_t input = (int32_t)raw - (s_dc_q8 >> 8);
        const int32_t mix_i = input * s_cos64[s_carrier_phase];
        const int32_t mix_q = input * s_sin64[s_carrier_phase];
        /* At 64 ksample/s the real 57 kHz carrier aliases to 7 kHz. */
        s_carrier_phase = (uint8_t)((s_carrier_phase + 7u) & 63u);

        s_lp_i += (mix_i - s_lp_i) >> 2;
        s_lp_q += (mix_q - s_lp_q) >> 2;

        /* The RDS baseband is only a few kHz wide, so timing recovery can run
           at 16 ksample/s after the low-pass. */
        if (++s_decimation_phase >= RDS_DECIMATION) {
            s_decimation_phase = 0u;
            for (uint8_t lane_index = 0u; lane_index < RDS_LANES; ++lane_index) {
                RDS_Lane_t *lane = &s_lanes[lane_index];
                const uint32_t old_phase = lane->phase;
                const int32_t sign = (old_phase & 0x80000000u) == 0u ? 1 : -1;
                lane->accum_i += sign * s_lp_i;
                lane->accum_q += sign * s_lp_q;
                lane->phase += RDS_BIT_PHASE_STEP;
                if (lane->phase < old_phase)
                    RDS_FinishSymbol(lane_index, lane);
            }
        }

        const uint32_t abs_input = abs32(input);
        s_raw_sum += abs_input;
        s_baseband_sum += (abs32(s_lp_i) + abs32(s_lp_q)) >> 7;
        s_adc_sum += raw;
        if (raw < s_window_min) s_window_min = raw;
        if (raw > s_window_max) s_window_max = raw;
        if (++s_metric_count >= 2048u)
            RDS_UpdateMetrics();
    }
}

void RDS_Reset(const uint8_t profile)
{
    memset(s_lanes, 0, sizeof(s_lanes));
    for (uint8_t i = 0u; i < RDS_LANES; ++i)
        s_lanes[i].phase = (uint32_t)i << 29;
    memset((void *)&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.stage = s_active ? RDS_STAGE_NO_INPUT : RDS_STAGE_OFF;
    s_snapshot.profile = profile;
    memset((void *)s_snapshot.ps, ' ', 8u);
    s_snapshot.ps[8] = '\0';
    s_selected_lane = -1;
    s_dc_q8 = 2048 << 8;
    s_lp_i = 0;
    s_lp_q = 0;
    s_carrier_phase = 0u;
    s_decimation_phase = 0u;
    s_raw_sum = 0u;
    s_baseband_sum = 0u;
    s_adc_sum = 0u;
    s_metric_count = 0u;
    s_window_min = 0xFFFFu;
    s_window_max = 0u;
}

void RDS_Start(const uint8_t profile)
{
    if (s_active) {
        RDS_Reset(profile);
        return;
    }

    LL_TIM_DisableCounter(RDS_SAMPLE_TIMER);
    LL_ADC_Disable(ADC1);

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG | LL_APB1_GRP2_PERIPH_ADC1);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_1, LL_GPIO_MODE_ANALOG);

    LL_DMA_DisableChannel(DMA1, RDS_DMA_CHANNEL);
    LL_DMA_InitTypeDef dma;
    LL_DMA_StructInit(&dma);
    dma.Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    dma.Mode = LL_DMA_MODE_CIRCULAR;
    dma.PeriphOrM2MSrcAddress = LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA);
    dma.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
    dma.MemoryOrM2MDstAddress = (uint32_t)s_dma;
    dma.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_HALFWORD;
    dma.NbData = RDS_DMA_SAMPLES;
    dma.Priority = LL_DMA_PRIORITY_VERYHIGH;
    LL_DMA_Init(DMA1, RDS_DMA_CHANNEL, &dma);
    LL_SYSCFG_SetDMARemap(DMA1, RDS_DMA_CHANNEL, LL_SYSCFG_DMA_MAP_ADC1);
    LL_DMA_ClearFlag_GI1(DMA1);

    LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);
    LL_ADC_SetSequencersScanMode(ADC1, LL_ADC_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_9);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_9, LL_ADC_SAMPLINGTIME_13CYCLES_5);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_EXT_TIM3_TRGO);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);

    LL_TIM_SetPrescaler(RDS_SAMPLE_TIMER, 0u);
    LL_TIM_SetAutoReload(RDS_SAMPLE_TIMER, 749u); /* 48 MHz / 750 = 64 kHz */
    LL_TIM_SetTriggerOutput(RDS_SAMPLE_TIMER, LL_TIM_TRGO_UPDATE);
    LL_TIM_SetCounter(RDS_SAMPLE_TIMER, 0u);

    s_active = true;
    RDS_Reset(profile);
    /* DMA completion is polled by RDS_Service().  Keeping its IRQ disabled is
       intentional: a bad flag/remap combination must never lock the UI. */
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    LL_DMA_EnableChannel(DMA1, RDS_DMA_CHANNEL);
    LL_ADC_Enable(ADC1);
    LL_ADC_REG_StartConversionExtTrig(ADC1, LL_ADC_REG_TRIG_EXT_RISING);
    LL_TIM_GenerateEvent_UPDATE(RDS_SAMPLE_TIMER);
    LL_TIM_EnableCounter(RDS_SAMPLE_TIMER);
}

void RDS_Stop(void)
{
    if (!s_active)
        return;
    s_active = false;
    LL_TIM_DisableCounter(RDS_SAMPLE_TIMER);
    LL_ADC_REG_StartConversionExtTrig(ADC1, 0u);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_NONE);
    LL_DMA_DisableChannel(DMA1, RDS_DMA_CHANNEL);
    LL_DMA_DisableIT_HT(DMA1, RDS_DMA_CHANNEL);
    LL_DMA_DisableIT_TC(DMA1, RDS_DMA_CHANNEL);
    LL_DMA_ClearFlag_GI1(DMA1);
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    LL_ADC_Disable(ADC1);
    BOARD_ADC_Init();
    s_snapshot.stage = RDS_STAGE_OFF;
}

void RDS_Service(void)
{
    if (!s_active)
        return;

    /* Poll latched DMA flags from the normal 10 ms application slice.  No DMA
       interrupt is enabled.  If the application falls behind, a half-buffer
       may be dropped, but input, display and exit keys remain responsive. */
    uint8_t ready = 0u;
    if (LL_DMA_IsActiveFlag_HT1(DMA1)) {
        LL_DMA_ClearFlag_HT1(DMA1);
        ready |= RDS_DMA_HALF_FIRST;
    }
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        ready |= RDS_DMA_HALF_SECOND;
    }

    if ((ready & RDS_DMA_HALF_FIRST) != 0u)
        RDS_ProcessSamples(&s_dma[0], RDS_HALF_SAMPLES);
    if ((ready & RDS_DMA_HALF_SECOND) != 0u)
        RDS_ProcessSamples(&s_dma[RDS_HALF_SAMPLES], RDS_HALF_SAMPLES);
}

bool RDS_IsActive(void)
{
    return s_active;
}

void RDS_GetSnapshot(RDS_Snapshot_t *snapshot)
{
    if (snapshot == NULL)
        return;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    memcpy(snapshot, (const void *)&s_snapshot, sizeof(*snapshot));
    if (primask == 0u)
        __enable_irq();
}

#endif
