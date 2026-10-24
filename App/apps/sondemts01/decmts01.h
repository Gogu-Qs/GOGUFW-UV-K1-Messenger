#ifndef DECMTS01_H
#define DECMTS01_H
#include <stdint.h>
#include <stdbool.h>

#define MTS01_FRAME_LEN 131u

typedef struct {
    char id[9];
    int32_t lat_e5, lon_e5, alt_m;
    uint16_t frame, batt_mv, speed_10, heading_10;
    int16_t temp_10;
    uint8_t flags;
} mts01_info_t;

enum {
    MTS01_HAS_POS  = 1u,
    MTS01_HAS_TEMP = 2u,
    MTS01_HAS_ID   = 4u
};

typedef struct {
    int32_t mean_q8, phase_q8, sum, prev_err;
    uint32_t sync;
    uint16_t bit_count;
    uint8_t state, sign, inv;
    uint8_t frame[MTS01_FRAME_LEN];
} decmts01_t;

void decmts01_init(decmts01_t *d);
void decmts01_rearm(decmts01_t *d);
bool decmts01_busy(const decmts01_t *d);
bool decmts01_push(decmts01_t *d, uint16_t sample);
bool decmts01_parse(const decmts01_t *d, mts01_info_t *out);

#endif
