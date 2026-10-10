#ifndef DRIVER_RDS_DECODER_H
#define DRIVER_RDS_DECODER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RDS_STAGE_OFF = 0,
    RDS_STAGE_NO_INPUT,
    RDS_STAGE_NO_57K,
    RDS_STAGE_CARRIER,
    RDS_STAGE_SYNC,
    RDS_STAGE_PS,
} RDS_Stage_t;

typedef struct {
    RDS_Stage_t stage;
    uint8_t profile;
    uint8_t carrier_quality;
    uint8_t input_span;
    uint16_t input_level;
    uint8_t ps_mask;
    uint16_t pi;
    uint16_t valid_blocks;
    uint16_t block_errors;
    char ps[9];
} RDS_Snapshot_t;

void RDS_Start(uint8_t profile);
void RDS_Reset(uint8_t profile);
void RDS_Service(void);
void RDS_Stop(void);
bool RDS_IsActive(void);
void RDS_GetSnapshot(RDS_Snapshot_t *snapshot);

#endif
