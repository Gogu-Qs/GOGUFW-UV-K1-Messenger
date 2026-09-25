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

#include <string.h>

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "misc.h"
#include "settings.h"


void UI_GenerateChannelString(char *pString, const uint16_t Channel)
{
    unsigned int i;

    if (gInputBoxIndex == 0)
    {
        sprintf(pString, "CH-%02u", Channel + 1);
        return;
    }

    pString[0] = 'C';
    pString[1] = 'H';
    pString[2] = '-';
    for (i = 0; i < 2; i++)
        pString[i + 3] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';

    pString[5] = 0;
}

void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint16_t ChannelNumber)
{
    if (gInputBoxIndex > 0) {
        for (unsigned int i = 0; i < 4; i++) {
            pString[i] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
        }

        pString[4] = 0;
        return;
    }

    if (bShowPrefix) {
        // BUG here? Prefixed NULLs are allowed
        sprintf(pString, "CH-%04u", ChannelNumber + 1);
    } else if (ChannelNumber == MR_CHANNEL_LAST + 1) {
        strcpy(pString, "None");
    } else if (ChannelNumber == 0xFFFF) {
        strcpy(pString, "NULL");
    } else {
        sprintf(pString, "%04u", ChannelNumber + 1);
    }
}

void UI_PrintStringBuffer(const char *pString, uint8_t * buffer, uint32_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;
    for (size_t i = 0; i < Length; i++) {
        const unsigned int index = pString[i] - ' ' - 1;
        if (pString[i] > ' ' && pString[i] < 127) {
            const uint32_t offset = i * char_spacing + 1;
            memcpy(buffer + offset, font + index * char_width, char_width);
        }
    }
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
    size_t i;
    size_t Length = strlen(pString);

    if (End > Start)
        Start += (((End - Start) - (Length * Width)) + 1) / 2;

    for (i = 0; i < Length; i++)
    {
        const unsigned int ofs   = (unsigned int)Start + (i * Width);
        if (pString[i] > ' ' && pString[i] < 127)
        {
            const unsigned int index = pString[i] - ' ' - 1;
            for (uint8_t col = 0; col < 7; col++) {
                uint8_t code = gFontBigColumnIndex[index][col];
                gFrameBuffer[Line][ofs + col] = gFontBigColumns[code][0];
                gFrameBuffer[Line + 1][ofs + col] = gFontBigColumns[code][1];
            }
        }
    }
}

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;

    if (End > Start) {
        Start += (((End - Start) - Length * char_spacing) + 1) / 2;
    }

    UI_PrintStringBuffer(pString, gFrameBuffer[Line] + Start, char_width, font);
}


void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    UI_PrintStringSmall(pString, Start, End, Line, ARRAY_SIZE(gFontSmall[0]), (const uint8_t *)gFontSmall);
}

void UI_PrintStringSmallNormalInverse(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    // First draw the string normally
    UI_PrintStringSmallNormal(pString, Start, End, Line);

    // Now invert the framebuffer bits for the rendered area
    uint8_t len = strlen(pString);
    uint8_t char_width = 7; // small font is typically 6px wide

    uint8_t x_start = Start;
    uint8_t x_end   = Start + (len * char_width) + 1;

    if (End != 0 && x_end > End)
        x_end = End;

    //gFrameBuffer[Line][x_start - 2] ^= 0x3E;
    gFrameBuffer[Line][x_start - 1] ^= 0x7F;
    //gFrameBuffer[Line][x_start - 1] ^= 0xFF;
    for (uint8_t x = x_start; x < x_end; x++)
    {
        gFrameBuffer[Line][x] ^= 0xFF;
        gFrameBuffer[Line - 1][x] ^= 0x80;
    }
    //gFrameBuffer[Line][x_end + 0] ^= 0xFF;
    gFrameBuffer[Line][x_end + 0] ^= 0x7F;
    //gFrameBuffer[Line][x_end + 1] ^= 0x3E;
}


void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif

    UI_PrintStringSmall(pString, Start, End, Line, char_width, font);
}

void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t * buffer)
{
    UI_PrintStringBuffer(pString, buffer, ARRAY_SIZE(gFontSmall[0]), (uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif
    UI_PrintStringBuffer(pString, buffer, char_width, font);
}

void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    uint8_t len = strlen(string);
    for(int i = 0; i < len; i++) {
        char c = string[i];
        if(c=='-') c = '9' + 1;
        if (bCanDisplay || c != ' ')
        {
            bCanDisplay = true;
            if(c>='0' && c<='9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c-'0'],                  char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c-'0'] + char_width - 3, char_width - 3);
            }
            else if(c=='.') {
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                continue;
            }

        }
        else if (center) {
            pFb0 -= 6;
            pFb1 -= 6;
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}

void UI_GOGU_DrawHeader(const char *title, const char *badge)
{
    if (title != NULL)
        UI_PrintStringSmallBold(title, 0, 127, 0);

    if (badge != NULL && badge[0] != '\0') {
#ifdef ENABLE_FEAT_F4HWN
        const uint8_t width = (uint8_t)(strlen(badge) * 4u);
        const uint8_t x = width >= 128u ? 0u : (uint8_t)(128u - width);
        GUI_DisplaySmallest(badge, x, 1, false, true);
#else
        const uint8_t width = (uint8_t)(strlen(badge) * 7u);
        const uint8_t x = width >= 128u ? 0u : (uint8_t)(128u - width);
        UI_PrintStringSmallNormal(badge, x, 0, 0);
#endif
    }
}

void UI_GOGU_DrawDottedSeparator(uint8_t y)
{
    for (uint8_t x = 0u; x < 128u; x = (uint8_t)(x + 4u))
        UI_DrawLineBuffer(gFrameBuffer, x, y, (uint8_t)(x + 1u), y, true);
}

static uint8_t UI_GOGU_TinyWidth(const char *text)
{
    return text == NULL ? 0u : (uint8_t)(strlen(text) * 4u);
}

void UI_GOGU_DrawFooter(const char *left, const char *center, const char *right)
{
    UI_GOGU_DrawDottedSeparator(UI_GOGU_BOTTOM_SEPARATOR_Y);
#ifdef ENABLE_FEAT_F4HWN
    if (left != NULL)
        GUI_DisplaySmallest(left, 0u, 49u, false, true);
    if (center != NULL) {
        const uint8_t width = UI_GOGU_TinyWidth(center);
        GUI_DisplaySmallest(center, width >= 128u ? 0u : (uint8_t)((128u - width) / 2u), 49u, false, true);
    }
    if (right != NULL) {
        const uint8_t width = UI_GOGU_TinyWidth(right);
        GUI_DisplaySmallest(right, width >= 128u ? 0u : (uint8_t)(128u - width), 49u, false, true);
    }
#else
    if (left != NULL) UI_PrintStringSmallNormal(left, 0u, 0u, 6u);
    if (center != NULL) UI_PrintStringSmallNormal(center, 0u, 127u, 6u);
    if (right != NULL) {
        const uint8_t width = (uint8_t)(strlen(right) * 7u);
        UI_PrintStringSmallNormal(right, width >= 128u ? 0u : (uint8_t)(128u - width), 0u, 6u);
    }
#endif
}

void UI_GOGU_PrintSmallAtY(const char *text, uint8_t x, uint8_t y, bool inverted)
{
    if (text == NULL)
        return;

    const uint8_t pitch = 7u;
    if (inverted)
        UI_GOGU_InvertBand(y > 0u ? (uint8_t)(y - 1u) : y, 9u);
    for (uint8_t i = 0u; text[i] != '\0' && x < 128u; i++, x = (uint8_t)(x + pitch)) {
        const char c = text[i];
        if (c <= ' ' || c >= 127)
            continue;
        const uint8_t *glyph = gFontSmall[(uint8_t)c - ' ' - 1u];
        for (uint8_t col = 0u; col < 6u && (uint8_t)(x + col) < 128u; col++) {
            uint8_t bits = glyph[col];
            for (uint8_t row = 0u; row < 7u && (uint8_t)(y + row) < 56u; row++) {
                if ((bits & (1u << row)) != 0u)
                    UI_DrawPixelBuffer(gFrameBuffer, (uint8_t)(x + col), (uint8_t)(y + row), !inverted);
            }
        }
    }
}

void UI_GOGU_InvertArea(uint8_t x0, uint8_t x1, uint8_t y, uint8_t height)
{
    if (x1 > 127u)
        x1 = 127u;
    uint8_t end = (uint8_t)(y + height);
    if (end > 56u)
        end = 56u;
    for (uint8_t py = y; py < end; py++) {
        const uint8_t mask = (uint8_t)(1u << (py & 7u));
        for (uint8_t x = x0; x <= x1; x++)
            gFrameBuffer[py >> 3][x] ^= mask;
    }
}

void UI_GOGU_InvertBand(uint8_t y, uint8_t height)
{
    UI_GOGU_InvertArea(0u, 127u, y, height);
}

void UI_GOGU_DrawTextEditor(const char *title, const char *text, uint8_t max_len,
                            const char *primary_action, const char *mode,
                            bool multiline)
{
    char counter[10];
    char mode_label[4] = "*:B";
    uint8_t used = text == NULL ? 0u : (uint8_t)strlen(text);
    if (used > max_len)
        used = max_len;
    if (mode != NULL && mode[0] != '\0')
        mode_label[2] = mode[0];

    UI_DisplayClear();
#ifdef ENABLE_FEAT_F4HWN
    UI_DisplayUnlockKeyboard(5u);
#endif
    sprintf(counter, "%u/%u", (unsigned)used, (unsigned)max_len);
    UI_GOGU_DrawHeader(title, counter);
#ifdef ENABLE_FEAT_F4HWN
    GUI_DisplaySmallest(mode_label, 0u, 1u, false, true);
#else
    UI_PrintStringSmallNormal(mode_label, 0u, 0u, 0u);
#endif
    UI_GOGU_DrawDottedSeparator(UI_GOGU_TOP_SEPARATOR_Y);

    if (text != NULL && text[0] != '\0') {
        if (multiline) {
            char line[18];
            const char *p = text;
            for (uint8_t row = 0u; row < 3u && *p != '\0'; row++) {
                uint8_t n = 0u;
                while (p[n] != '\0' && n < 17u) {
                    line[n] = p[n];
                    n++;
                }
                line[n] = '\0';
                UI_GOGU_PrintSmallAtY(line, 1u, (uint8_t)(13u + row * 10u), false);
                p += n;
            }
        } else {
            UI_PrintStringSmallBold(text, 4u, 123u, 3u);
        }
    }

    UI_GOGU_DrawFooter(primary_action, "F:DEL", "EXIT");
}

/*
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    if (center) {
        uint8_t len = 0;
        for (const char *ptr = string; *ptr; ptr++)
            if (*ptr != ' ') len++; // Ignores spaces for centering

        X -= (len * char_width) / 2; // Centering adjustment
        pFb0 = gFrameBuffer[Y] + X;
        pFb1 = pFb0 + 128;
    }

    for (; *string; string++) {
        char c = *string;
        if (c == '-') c = '9' + 1; // Remap of '-' symbol

        if (bCanDisplay || c != ' ') {
            bCanDisplay = true;
            if (c >= '0' && c <= '9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c - '0'], char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c - '0'] + char_width - 3, char_width - 3);
            } else if (c == '.') {
                memset(pFb1, 0x60, 3); // Replaces the three assignments
                pFb0 += 3;
                pFb1 += 3;
                continue;
            }
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}
*/

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black)
{
    const uint8_t pattern = 1 << (y % 8);
    if(black)
        buffer[y/8][x] |= pattern;
    else
        buffer[y/8][x] &= ~pattern;
}

static void sort(int16_t *a, int16_t *b)
{
    if(*a > *b) {
        int16_t t = *a;
        *a = *b;
        *b = t;
    }
}

#ifdef ENABLE_FEAT_F4HWN
    /*
    void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
    {
        if(x2==x1) {
            sort(&y1, &y2);
            for(int16_t i = y1; i <= y2; i+=2) {
                UI_DrawPixelBuffer(buffer, x1, i, black);
            }
        } else {
            const int multipl = 1000;
            int a = (y2-y1)*multipl / (x2-x1);
            int b = y1 - a * x1 / multipl;

            sort(&x1, &x2);
            for(int i = x1; i<= x2; i+=2)
            {
                UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
            }
        }
    }
    */

    void PutPixel(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(gFrameBuffer, x, y, fill);
    }

    void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(&gStatusLine, x, y, fill);
    }

    void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                    bool statusbar, bool fill) {
      uint8_t c;
      uint8_t pixels;
      const uint8_t *p = (const uint8_t *)pString;

      while ((c = *p++) && c != '\0') {
        c -= 0x20;
        for (int i = 0; i < 3; ++i) {
          pixels = gFont3x5[c][i];
          for (int j = 0; j < 6; ++j) {
            if (pixels & 1) {
              if (statusbar)
                PutPixelStatus(x + i, y + j, fill);
              else
                PutPixel(x + i, y + j, fill);
            }
            pixels >>= 1;
          }
        }
        x += 4;
      }
    }

    void GUI_DisplaySmallestInverse(const char *pString, uint8_t x, uint8_t Line,
                                bool statusbar, bool fill, uint8_t end)
    {
        // First draw the string normally
        GUI_DisplaySmallest(pString, x, (Line * 8) + 1, statusbar, fill);

        // Now invert the framebuffer/statusline bits for the rendered area
        uint8_t start = (x - 2);
        uint8_t *buffer = statusbar ? gStatusLine : gFrameBuffer[Line];

        buffer[start] ^= 0x3E;
        for (uint8_t i = start + 1; i < end; i++) {
            buffer[i] ^= 0x7F;
        }
        buffer[end] ^= 0x3E;
    }

    void UI_DisplayUnlockKeyboard(uint8_t shift) {
        if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
        {   // tell user how to unlock the keyboard
            
            //memcpy(gFrameBuffer[shift] + 2, gFontKeyLock, sizeof(gFontKeyLock));
            UI_PrintStringSmallBold("UNLOCK KEYBOARD", 12, 0, shift);
            //memcpy(gFrameBuffer[shift] + 120, gFontKeyLock, sizeof(gFontKeyLock));

            /*
            for (uint8_t i = 12; i < 116; i++)
            {
                gFrameBuffer[shift][i] ^= 0xFF;
            }
            */
        }
    }

    bool IsEmptyName(const char *name, uint8_t len) {
        if (name[0] == '\0' || name[0] == '\xff')
            return true;
        for (uint8_t i = 0; i < len; i++) {
            if (name[i] != ' ' && name[i] != '\xff' && name[i] != '\0')
                return false;
        }
        return true;
    }
#endif
    
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    if(x2==x1) {
        sort(&y1, &y2);
        for(int16_t i = y1; i <= y2; i++) {
            UI_DrawPixelBuffer(buffer, x1, i, black);
        }
    } else {
        const int multipl = 1000;
        int a = (y2-y1)*multipl / (x2-x1);
        int b = y1 - a * x1 / multipl;

        sort(&x1, &x2);
        for(int i = x1; i<= x2; i++)
        {
            UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
        }
    }
}

void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    UI_DrawLineBuffer(buffer, x1,y1, x1,y2, black);
    UI_DrawLineBuffer(buffer, x1,y1, x2,y1, black);
    UI_DrawLineBuffer(buffer, x2,y1, x2,y2, black);
    UI_DrawLineBuffer(buffer, x1,y2, x2,y2, black);
}


void UI_DisplayPopup(const char *string)
{
    UI_DisplayClear();

    // for(uint8_t i = 1; i < 5; i++) {
    //  memset(gFrameBuffer[i]+8, 0x00, 111);
    // }

    // for(uint8_t x = 10; x < 118; x++) {
    //  UI_DrawPixelBuffer(x, 10, true);
    //  UI_DrawPixelBuffer(x, 46-9, true);
    // }

    // for(uint8_t y = 11; y < 37; y++) {
    //  UI_DrawPixelBuffer(10, y, true);
    //  UI_DrawPixelBuffer(117, y, true);
    // }
    // DrawRectangle(9,9, 118,38, true);
    UI_PrintString(string, 9, 118, 2, 8);
    UI_PrintStringSmallNormal("Press EXIT", 9, 118, 6);
}

void UI_DisplayClear()
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
}

void UI_StatusClear()
{
    memset(gStatusLine, 0, sizeof(gStatusLine));
}
