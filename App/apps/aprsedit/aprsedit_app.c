#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../app_api.h"

#define MSG_LEN      23u
#define HOLD_TICKS   40u
#define COMMIT_TICKS 80u

static const app_api_t *A;
static char text[MSG_LEN + 1u];
static uint8_t len, mode, pending, cycle, held;
static uint16_t pending_ticks, held_ticks;
static bool has_pending, fired_long, dirty, running;

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst; const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

static uint8_t slen(const char *s, uint8_t max)
{
    uint8_t n = 0u;
    while (n < max && s[n]) n++;
    return n;
}

static void dots(uint8_t y)
{
    for (uint8_t x = 1u; x < 127u; x += 4u)
        A->draw_line(A->fb, x, y, (int16_t)(x + 1u), y, true);
}

static void counter(char out[6])
{
    char *p = out;
    if (len >= 10u) *p++ = (char)('0' + len / 10u);
    *p++ = (char)('0' + len % 10u); *p++ = '/'; *p++ = '2'; *p++ = '3'; *p = 0;
}

static void draw(void)
{
    char count[6], mode_text[4] = "*:B", line[18];
    counter(count);
    mode_text[2] = mode == 0u ? 'B' : mode == 1u ? 'b' : '2';
    A->display_clear();
    A->print_bold("APRS MESSAGE", 0u, 127u, 0u);
    A->print_tiny(count, (uint8_t)(128u - slen(count, 5u) * 4u), 2u, false, true);
    A->print_tiny(mode_text, 1u, 2u, false, true);
    dots(9u);
    for (uint8_t row = 0u, off = 0u; row < 2u && text[off]; row++, off += 17u) {
        uint8_t n = 0u;
        while (n < 17u && text[off + n]) { line[n] = text[off + n]; n++; }
        line[n] = 0;
        A->print_small_y(line, 1u, (uint8_t)(13u + row * 10u), false);
    }
    dots(46u);
    A->print_tiny("SAVE", 1u, 49u, false, true);
    A->print_tiny("F:DEL", 54u, 49u, false, true);
    A->print_tiny("EXIT", 112u, 49u, false, true);
    A->blit_full();
}

static const char *key_chars(uint8_t key)
{
    static const char *const map[10] = {
        " ", ".,?!", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ"
    };
    return key <= APP_KEY_9 ? map[key] : "";
}

static void commit(void)
{
    has_pending = false; pending = APP_KEY_INVALID; cycle = 0u; pending_ticks = 0u;
}

static char letter_case(char c)
{
    return mode == 1u && c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}

static void insert_digit(uint8_t key)
{
    commit();
    if (len < MSG_LEN) { text[len++] = (char)('0' + key); text[len] = 0; }
}

static void edit_key(uint8_t key, bool long_press)
{
    if (long_press && key <= APP_KEY_9) { insert_digit(key); return; }
    if (key == APP_KEY_STAR) { commit(); mode = (uint8_t)((mode + 1u) % 3u); return; }
    if (key == APP_KEY_F) {
        if (len) text[--len] = 0;
        commit(); return;
    }
    if (key > APP_KEY_9) return;
    if (mode == 2u) { insert_digit(key); return; }
    const char *chars = key_chars(key);
    uint8_t count = slen(chars, 4u);
    if (has_pending && pending == key && len) {
        cycle = (uint8_t)((cycle + 1u) % count);
        text[len - 1u] = letter_case(chars[cycle]); pending_ticks = 0u; return;
    }
    commit();
    if (len < MSG_LEN) {
        pending = key; has_pending = true;
        text[len++] = letter_case(chars[0]); text[len] = 0;
    }
}

static void save_and_exit(void)
{
    uint8_t saved[MSG_LEN];
    for (uint8_t i = 0u; i < MSG_LEN; i++) saved[i] = i < len ? (uint8_t)text[i] : (uint8_t)' ';
    A->cfg_save(saved, MSG_LEN);
    running = false;
}

static void short_key(uint8_t key)
{
    if (key == APP_KEY_MENU) save_and_exit();
    else if (key == APP_KEY_EXIT) running = false;
    else edit_key(key, false);
    dirty = true;
}

__attribute__((section(".text.entry"), used))
void app_main(const app_api_t *api)
{
    uint8_t saved[MSG_LEN];
    A = api; held = APP_KEY_INVALID; running = true; dirty = true;
    api->cfg_load(saved, MSG_LEN);
    if (saved[0] == 0xFFu) {
        static const char initial[] = "UV-K5/K1 F4HWN Firmware";
        memcpy(text, initial, MSG_LEN); len = MSG_LEN;
    } else {
        memcpy(text, saved, MSG_LEN); len = MSG_LEN;
        while (len && (text[len - 1u] == ' ' || (uint8_t)text[len - 1u] == 0xFFu)) len--;
    }
    text[len] = 0;
    api->backlight_on();
    while (running) {
        if (dirty) { dirty = false; draw(); }
        uint8_t key = api->get_key();
        if (key == APP_KEY_SAVER || key == APP_KEY_WAKE) {
            held = APP_KEY_INVALID; held_ticks = 0u; fired_long = false; dirty = true;
        } else if (key == APP_KEY_INVALID) {
            if (held != APP_KEY_INVALID && !fired_long) short_key(held);
            held = APP_KEY_INVALID; held_ticks = 0u; fired_long = false;
        } else if (key != held) {
            held = key; held_ticks = 0u; fired_long = false;
        } else if (!fired_long && key <= APP_KEY_9 && ++held_ticks >= HOLD_TICKS) {
            fired_long = true; edit_key(key, true); dirty = true;
        }
        if (has_pending && ++pending_ticks >= COMMIT_TICKS) commit();
        api->battery_sample(); api->delay_ms(10u); api->backlight_update();
    }
}
