/* Copyright 2025 muzkr
 * https://github.com/muzkr
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

#ifndef DRIVER_PY25Q16_H
#define DRIVER_PY25Q16_H

#include <stdint.h>
#include <stdbool.h>

void PY25Q16_Init();
void PY25Q16_ReadBuffer(uint32_t Address, void *pBuffer, uint32_t Size);
void PY25Q16_WriteBuffer(uint32_t Address, const void *pBuffer, uint32_t Size, bool Append);
void PY25Q16_SectorErase(uint32_t Address);

void PY25Q16_InvalidateCache(void);

#ifdef ENABLE_FEAT_F4HWN_MULTIBOOT
/* Standard F4HWN profile bank plus two GOGUFW-private sectors. Profile 0 keeps
 * the legacy physical addresses; profiles 1..4 redirect them into unused
 * offsets 0xB000 and 0xC000 of their private 64 KiB banks. */
#define PY25Q16_PROFILE_SHARED_FROM       0x00010000u
#define PY25Q16_GOGU_PRIVATE_FROM         0x00012000u
#define PY25Q16_GOGU_PRIVATE_TO           0x00014000u
#define PY25Q16_GOGU_PROFILE_OFFSET       0x0000B000u
void PY25Q16_SetProfileBase(uint32_t Base);
#endif

#endif
