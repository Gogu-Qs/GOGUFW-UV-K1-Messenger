#include "app/calltx_store.h"
#include "driver/py25q16.h"

/* Preserve the established v7 Messenger layout without pulling its complete
 * inbox/outbox/draft persistence code into a CALLTX-only build. */
#define CALLTX_TONE_FLASH_ADDR (0x012000u + 317u)

uint8_t gCallTxTone;
uint8_t gCallTxVol;

void CALLTX_STORE_Init(void)
{
    uint8_t stored[2];
    PY25Q16_ReadBuffer(CALLTX_TONE_FLASH_ADDR, stored, sizeof(stored));
    gCallTxTone = stored[0] <= 4u ? stored[0] : 0u;
    gCallTxVol = stored[1] <= 1u ? stored[1] : 1u;
}

void CALLTX_STORE_Save(void)
{
    const uint8_t stored[2] = {gCallTxTone, gCallTxVol};
    PY25Q16_WriteBuffer(CALLTX_TONE_FLASH_ADDR, stored, sizeof(stored), false);
}
