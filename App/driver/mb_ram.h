#ifndef DRIVER_MB_RAM_H
#define DRIVER_MB_RAM_H

#include <stdint.h>

/* Private restore entry. Implemented in the isolated non-LTO RAM code unit. */
void MB_RamReflash(uint32_t intAddr, uint32_t extAddr, uint32_t imageSize,
                   uint8_t *progressLine);

#endif
