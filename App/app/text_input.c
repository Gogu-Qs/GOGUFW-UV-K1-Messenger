#include <string.h>
#include "app/text_input.h"

#define TEXT_INPUT_COMMIT_TICKS 80U  /* 800 ms at 10 ms tick cadence */

static const char *map_chars(KEY_Code_t key)
{
    /* Letter modes intentionally cycle letters only (ABCABC...), not ABC2.
     * Digits are available by long-press or by switching to numeric mode. */
    switch (key) {
        case KEY_0: return " ";
        case KEY_1: return ".,?!";
        case KEY_2: return "ABC";
        case KEY_3: return "DEF";
        case KEY_4: return "GHI";
        case KEY_5: return "JKL";
        case KEY_6: return "MNO";
        case KEY_7: return "PQRS";
        case KEY_8: return "TUV";
        case KEY_9: return "WXYZ";
        default: return "";
    }
}

static char apply_case(char c, bool upper)
{
    if (!upper && c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
    return c;
}

static bool insert_at_cursor(TEXT_INPUT_Editor_t *ed, char c)
{
    if (ed->len >= ed->max_len) return false;
    for (uint8_t i = ed->len; i > ed->cursor; i--)
        ed->buffer[i] = ed->buffer[i - 1U];
    ed->buffer[ed->cursor++] = c;
    ed->buffer[++ed->len] = 0;
    return true;
}

void TEXT_INPUT_Start(TEXT_INPUT_Editor_t *ed, char *buf, uint8_t max_len)
{
    ed->buffer = buf;
    ed->max_len = max_len;
    ed->len = strlen(buf);
    if (ed->len > max_len) ed->len = max_len;
    ed->buffer[ed->len] = 0;
    ed->cursor = ed->len;
    ed->upper = true;
    ed->mode = 0;
    ed->pending_key = KEY_INVALID;
    ed->cycle_index = 0;
    ed->pending_ticks = 0;
    ed->has_pending = false;
}

void TEXT_INPUT_Commit(TEXT_INPUT_Editor_t *ed)
{
    if (!ed) return;
    ed->pending_key = KEY_INVALID;
    ed->cycle_index = 0;
    ed->pending_ticks = 0;
    ed->has_pending = false;
}

bool TEXT_INPUT_Tick(TEXT_INPUT_Editor_t *ed)
{
    if (!ed || !ed->has_pending) return false;
    if (++ed->pending_ticks < TEXT_INPUT_COMMIT_TICKS) return false;
    TEXT_INPUT_Commit(ed);
    return true;
}

uint8_t TEXT_INPUT_GetDisplayCursor(const TEXT_INPUT_Editor_t *ed)
{
    if (!ed) return 0u;
    return ed->has_pending && ed->cursor > 0u
         ? (uint8_t)(ed->cursor - 1u) : ed->cursor;
}

bool TEXT_INPUT_HandleKey(TEXT_INPUT_Editor_t *ed, KEY_Code_t key)
{
    if (!ed || !ed->buffer) return false;

    if (key == KEY_STAR) {
        TEXT_INPUT_Commit(ed);
        ed->mode = (uint8_t)((ed->mode + 1U) % 3U);
        ed->upper = (ed->mode == 0U);
        return true;
    }

    if (key == KEY_F || key == KEY_EXIT) {
        TEXT_INPUT_Commit(ed);
        if (ed->cursor > 0U) {
            ed->cursor--;
            for (uint8_t i = ed->cursor; i < ed->len; i++)
                ed->buffer[i] = ed->buffer[i + 1U];
            ed->len--;
            ed->buffer[ed->len] = 0;
        }
        return true;
    }

    if (key == KEY_UP || key == KEY_DOWN) {
        TEXT_INPUT_Commit(ed);
        if (key == KEY_UP)
            ed->cursor = ed->cursor ? (uint8_t)(ed->cursor - 1U) : ed->len;
        else
            ed->cursor = ed->cursor < ed->len ? (uint8_t)(ed->cursor + 1U) : 0U;
        return true;
    }

    const char *chars = map_chars(key);
    uint8_t n = (uint8_t)strlen(chars);
    if (n == 0) return false;

    /* Numeric mode: STAR cycles B -> b -> 2.  In mode 2, each keypad
     * press inserts the digit immediately; no multi-tap pending state. */
    if (ed->mode == 2U) {
        TEXT_INPUT_Commit(ed);
        if (key >= KEY_0 && key <= KEY_9)
            insert_at_cursor(ed, (char)('0' + (uint8_t)key));
        return true;
    }

    if (n == 1U) {
        TEXT_INPUT_Commit(ed);
        insert_at_cursor(ed, apply_case(chars[0], ed->upper));
        return true;
    }

    if (ed->has_pending && ed->pending_key == key && ed->len > 0) {
        ed->cycle_index = (uint8_t)((ed->cycle_index + 1U) % n);
        ed->buffer[ed->cursor - 1U] = apply_case(chars[ed->cycle_index], ed->upper);
        ed->pending_ticks = 0;
        return true;
    }

    TEXT_INPUT_Commit(ed);
    if (ed->len >= ed->max_len) return true;
    ed->cycle_index = 0;
    ed->pending_key = key;
    ed->has_pending = true;
    ed->pending_ticks = 0;
    insert_at_cursor(ed, apply_case(chars[0], ed->upper));
    return true;
}

bool TEXT_INPUT_HandleLongKey(TEXT_INPUT_Editor_t *ed, KEY_Code_t key)
{
    if (!ed || !ed->buffer) return false;
    if (key < KEY_0 || key > KEY_9) return false;

    /* 0.2.2: while in B/b modes, long-pressing a numeric key inserts its
     * digit directly.  This preserves the existing B/b/2 modes but removes
     * the need to switch to numeric mode just to type one number. */
    TEXT_INPUT_Commit(ed);
    insert_at_cursor(ed, (char)('0' + (uint8_t)key));
    return true;
}
