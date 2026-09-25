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

#ifndef UI_UI_H
#define UI_UI_H

#include <stdbool.h>
#include <stdint.h>

#define UI_GOGU_TOP_SEPARATOR_Y     9u
#define UI_GOGU_BOTTOM_SEPARATOR_Y 46u
#define UI_GOGU_CONTENT_ROW_Y(row) ((uint8_t)(11u + ((row) * 9u)))
#define UI_GOGU_CONTENT_ROWS        4u

void UI_GenerateChannelString(char *pString, const uint16_t Channel);
void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint16_t ChannelNumber);
void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width);
void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallNormalInverse(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t *buffer);
void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer);
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center);

void UI_DisplayPopup(const char *string);

/* Shared GOGUFW screen chrome.  Keep feature screens visually consistent and
 * centralise the small drawing primitives so optional builds do not carry
 * several private copies of the same code. */
void UI_GOGU_DrawHeader(const char *title, const char *badge);
void UI_GOGU_DrawDottedSeparator(uint8_t y);
void UI_GOGU_DrawFooter(const char *left, const char *center, const char *right);
void UI_GOGU_PrintSmallAtY(const char *text, uint8_t x, uint8_t y, bool inverted);
void UI_GOGU_InvertArea(uint8_t x0, uint8_t x1, uint8_t y, uint8_t height);
void UI_GOGU_InvertBand(uint8_t y, uint8_t height);
void UI_GOGU_DrawTextEditor(const char *title, const char *text, uint8_t max_len,
                            const char *primary_action, const char *mode,
                            bool multiline);

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black);
#ifdef ENABLE_FEAT_F4HWN
    //void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
    void PutPixel(uint8_t x, uint8_t y, bool fill);
    void PutPixelStatus(uint8_t x, uint8_t y, bool fill);
    void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y, bool statusbar, bool fill);
    void GUI_DisplaySmallestInverse(const char *pString, uint8_t x, uint8_t Line, bool statusbar, bool fill, uint8_t endX);
    void UI_DisplayUnlockKeyboard(uint8_t shift);
    bool IsEmptyName(const char *name, uint8_t len);
#endif
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);

void UI_DisplayClear();
void UI_StatusClear();

#endif
