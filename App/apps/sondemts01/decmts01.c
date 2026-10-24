/* Meteosis MTS01 streaming decoder, derived from the framing in
 * projecthorus/radiosonde_auto_rx demod/mod/mts01mod.c (GPL-3.0-or-later).
 * 1200 baud NRZ, 9.6 kHz ADC, 0xAAAAB42B sync, 0x80 + 128 ASCII bytes + CRC.
 */
#include "decmts01.h"

#define SPB_Q8 2048
#define SYNC_WORD 0xAAAAB42Bu
enum { ST_SEARCH, ST_DATA, ST_DONE };

static uint8_t pop32(uint32_t x)
{
    uint8_t n = 0;
    while (x) { x &= x - 1u; n++; }
    return n;
}

static void emit_bit(decmts01_t *d, uint8_t bit)
{
    if (d->state == ST_SEARCH) {
        uint8_t e, ei;
        d->sync = (d->sync << 1) | bit;
        if (d->bit_count < 32u) { d->bit_count++; return; }
        e = pop32(d->sync ^ SYNC_WORD);
        ei = pop32((~d->sync) ^ SYNC_WORD);
        if (e > 2u && ei > 2u) return;
        d->inv = ei < e;
        d->bit_count = 0;
        d->state = ST_DATA;
        return;
    }
    if (d->state != ST_DATA) return;
    if (d->inv) bit ^= 1u;
    if (!(d->bit_count & 7u)) d->frame[d->bit_count >> 3] = 0;
    if (bit) d->frame[d->bit_count >> 3] |= (uint8_t)(0x80u >> (d->bit_count & 7u));
    if (++d->bit_count >= MTS01_FRAME_LEN * 8u) d->state = ST_DONE;
}

void decmts01_init(decmts01_t *d)
{
    uint8_t *p = (uint8_t *)d;
    for (uint16_t i = 0; i < sizeof *d; i++) p[i] = 0;
    /* PA4 is held at DAC mid-scale by the app. Starting there preserves the
     * short 32-bit preamble; an arbitrary first sample may be a data peak. */
    d->mean_q8 = 2048 << 8;
    d->phase_q8 = SPB_Q8 / 2;
}

void decmts01_rearm(decmts01_t *d)
{
    int32_t mean = d->mean_q8;
    decmts01_init(d);
    d->mean_q8 = mean;
}

bool decmts01_busy(const decmts01_t *d) { return d->state == ST_DATA; }

bool decmts01_push(decmts01_t *d, uint16_t sample)
{
    int32_t x = (int32_t)sample << 8;
    if (d->state == ST_DONE) return true;
    if (d->mean_q8 < 0) d->mean_q8 = x;
    int32_t e = x - d->mean_q8;
    if (d->state != ST_DATA) d->mean_q8 += e >> 7;

    uint8_t sign = e >= 0;
    if (sign != d->sign) {
        uint32_t a = (uint32_t)(d->prev_err < 0 ? -d->prev_err : d->prev_err);
        uint32_t b = (uint32_t)(e < 0 ? -e : e);
        int32_t edge = (int32_t)((a << 8) / (a + b + 1u));
        int32_t err = edge - d->phase_q8;
        if (err < -(SPB_Q8 / 2)) err += SPB_Q8;
        if (err >  (SPB_Q8 / 2)) err -= SPB_Q8;
        d->phase_q8 += err >> 3;
        d->sign = sign;
    }
    d->prev_err = e;

    if (d->phase_q8 < 256) {
        int32_t part = (e * (d->phase_q8 < 0 ? 0 : d->phase_q8)) >> 8;
        emit_bit(d, (d->sum + part) >= 0);
        d->sum = e - part;
        d->phase_q8 += SPB_Q8 - 256;
    } else {
        d->sum += e;
        d->phase_q8 -= 256;
    }
    return d->state == ST_DONE;
}

static uint16_t crc16_re(const uint8_t *p, uint8_t len)
{
    uint16_t rem = 0xFFFFu, rev = 0;
    for (uint8_t i = 0; i < len; i++) {
        rem ^= (uint16_t)p[i] << 8;
        for (uint8_t j = 0; j < 8u; j++)
            rem = (uint16_t)((rem << 1) ^ ((rem & 0x8000u) ? 0x8005u : 0u));
    }
    for (uint8_t j = 0; j < 16u; j++) if (rem & (1u << (15u - j))) rev |= 1u << j;
    return rev;
}

static const uint8_t *field(const uint8_t *p, uint8_t n)
{
    while (n--) { while (*p && *p != ',') p++; if (*p == ',') p++; }
    return p;
}

static int32_t number(const uint8_t *p, uint32_t scale)
{
    bool neg = *p == '-';
    if (neg || *p == '+') p++;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10u + (uint8_t)(*p++ - '0'); }
    v *= scale;
    if (*p == '.') {
        p++;
        for (uint32_t place = scale / 10u; place && *p >= '0' && *p <= '9'; place /= 10u)
            v += (uint8_t)(*p++ - '0') * place;
    }
    return neg ? -(int32_t)v : (int32_t)v;
}

static int16_t temperature(uint16_t r10)
{
    static const uint16_t rt[] = {14181,9583,6602,4630,3302,2391,1757,1309,987,754,582,454,357,284,228,184,150,123,102,85,71,60,51,43,37,32,27};
    if (r10 > rt[0] || r10 < rt[26]) return -32768;
    for (uint8_t i = 0; i < 26u; i++) if (r10 <= rt[i] && r10 >= rt[i + 1u])
        return (int16_t)(-800 + i * 50 + (int32_t)(rt[i] - r10) * 50 / (rt[i] - rt[i + 1u]));
    return -32768;
}

bool decmts01_parse(const decmts01_t *d, mts01_info_t *out)
{
    const uint8_t *f = d->frame, *p;
    uint8_t text[129];
    if (f[0] != 0x80u) return false;
    uint16_t got = (uint16_t)f[129] | (uint16_t)f[130] << 8;
    if (got != crc16_re(f + 1, 128u)) return false;
    for (uint8_t i = 0; i < 128u; i++) text[i] = f[i + 1u];
    text[128] = 0;
    uint8_t *z = (uint8_t *)out;
    for (uint16_t i = 0; i < sizeof *out; i++) z[i] = 0;
    p = field(text, 0); for (uint8_t i = 0; i < 8u && *p && *p != ','; i++) out->id[i] = (char)*p++;
    if (out->id[0]) out->flags |= MTS01_HAS_ID;
    out->frame = (uint16_t)number(field(text, 2), 1u);
    out->batt_mv = (uint16_t)number(field(text, 4), 1u);
    out->lat_e5 = number(field(text, 5), 100000u);
    out->lon_e5 = number(field(text, 6), 100000u);
    out->alt_m = number(field(text, 7), 1u);
    out->heading_10 = (uint16_t)number(field(text, 8), 10u);
    out->speed_10 = (uint16_t)number(field(text, 9), 10u);
    out->temp_10 = temperature((uint16_t)number(field(text, 11), 10u));
    if (out->lat_e5 || out->lon_e5) out->flags |= MTS01_HAS_POS;
    if (out->temp_10 != -32768) out->flags |= MTS01_HAS_TEMP;
    return true;
}
