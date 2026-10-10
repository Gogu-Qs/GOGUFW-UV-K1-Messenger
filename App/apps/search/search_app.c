#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../app_api.h"

#define SC_FREQ 0x01u
#define SC_TONE 0x08u
#define REG_FREQ_H 0x0du
#define REG_FREQ_L 0x0eu
#define REG_SCAN   0x32u
#define REG_FREQ_LSB 0x38u
#define REG_FREQ_MSB 0x39u
#define REG_RX     0x30u
#define REG_CSS    0x51u
#define REG_CTCSS  0x68u
#define REG_DCS_H  0x69u
#define REG_DCS_L  0x6au

static const app_api_t *A;
static uint8_t mode;
static uint32_t freq;
static uint16_t tone;
static uint8_t found;
static uint32_t scan_prev;
static uint16_t last_ctcss;
static uint8_t scan_hits;
static uint8_t tone_hits;

static const uint16_t dcs_codes[104] = {
  0x013,0x015,0x016,0x019,0x01a,0x01e,0x023,0x027,0x029,0x02b,0x02c,0x035,0x039,
  0x03a,0x03b,0x03c,0x04c,0x04d,0x04e,0x052,0x055,0x059,0x05a,0x05c,0x063,0x065,
  0x06a,0x06d,0x06e,0x072,0x075,0x07a,0x07c,0x085,0x08a,0x093,0x095,0x096,0x0a3,
  0x0a4,0x0a5,0x0a6,0x0a9,0x0aa,0x0ad,0x0b1,0x0b3,0x0b5,0x0b6,0x0b9,0x0bc,0x0c6,
  0x0c9,0x0cd,0x0d5,0x0d9,0x0da,0x0e3,0x0e6,0x0e9,0x0ee,0x0f4,0x0f5,0x0f9,0x109,
  0x10a,0x10b,0x113,0x119,0x11a,0x125,0x126,0x12a,0x12c,0x12d,0x132,0x134,0x135,
  0x136,0x143,0x146,0x14e,0x153,0x156,0x15a,0x166,0x175,0x186,0x18a,0x194,0x197,
  0x199,0x19a,0x1ac,0x1b2,0x1b4,0x1c3,0x1ca,0x1d3,0x1d9,0x1da,0x1dc,0x1e3,0x1ec
};

static const uint16_t ctcss_codes[50] = {
   670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
   948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
  1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
  1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
  2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541
};

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst; const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int value, size_t n)
{
    uint8_t *d = dst;
    while (n--) *d++ = (uint8_t)value;
    return dst;
}

static void u32(char *p, uint32_t v, uint8_t digits)
{
    while (digits--) { p[digits] = (char)('0' + v % 10u); v /= 10u; }
}

static uint32_t golay(uint16_t code)
{
    uint32_t word = (uint32_t)code + 0x800u, check = word;
    for (uint8_t i = 0; i < 12u; i++) { check <<= 1; if (check & 0x1000u) check ^= 0x08eau; }
    return word | ((check & 0x0ffeu) << 11);
}

static uint16_t decode_dcs(uint32_t raw)
{
    for (uint8_t rot = 0; rot < 23u; rot++) {
        if (((raw >> 9) & 7u) == 4u) {
            const uint16_t code = (uint16_t)(raw & 0x1ffu);
            for (uint8_t i = 0; i < 104u; i++) if (dcs_codes[i] == code && golay(code) == raw) return code;
        }
        raw = (raw >> 1) | ((raw & 1u) ? 0x400000u : 0u);
    }
    return 0xffffu;
}

static uint16_t normalize_ctcss(uint16_t measured)
{
    uint16_t best = ctcss_codes[0];
    uint16_t smallest = measured > best ? measured - best : best - measured;
    for (uint8_t i = 1u; i < 50u; i++) {
        const uint16_t candidate = ctcss_codes[i];
        const uint16_t delta = measured > candidate ? measured - candidate : candidate - measured;
        if (delta < smallest) { smallest = delta; best = candidate; }
    }
    return best;
}

static void dcs_text(char *p, uint16_t code)
{
    p[0] = (char)('0' + ((code >> 6) & 7u));
    p[1] = (char)('0' + ((code >> 3) & 7u));
    p[2] = (char)('0' + (code & 7u));
    p[3] = 0;
}

static char *append_uint(char *p, uint32_t value)
{
    uint32_t div = 1u;
    while (value / div >= 10u) div *= 10u;
    do { *p++ = (char)('0' + value / div); value %= div; div /= 10u; } while (div);
    return p;
}

static void draw_dots(uint8_t y)
{
    for (uint8_t x = 1; x < 127; x += 4) A->draw_line(A->fb, x, y, x + 1, y, true);
}

static void small_y(const char *text, uint8_t x, uint8_t y)
{
    if (A->api_level >= 3u &&
        A->api_size >= offsetof(app_api_t, print_small_y) + sizeof(A->print_small_y) &&
        A->print_small_y) {
        A->print_small_y(text, x, y, false);
    } else {
        A->print_normal(text, x, 127u, (uint8_t)((y + 4u) / 8u));
    }
}

static void frequency_text(char out[18])
{
    char *p = out;
    *p++='F'; *p++='R'; *p++='E'; *p++='Q'; *p++=':';
    if (!freq) {
        *p++='-'; *p++='-'; *p++='-'; *p++='.';
        for (uint8_t i=0; i<5u; i++) *p++='-';
    } else {
        p = append_uint(p, freq / 100000u); *p++='.';
        u32(p, freq % 100000u, 5u); p += 5;
    }
    *p=0;
}

static void tone_text(char out[18])
{
    char *p=out;
    *p++='T'; *p++='O'; *p++='N'; *p++='E'; *p++=':';
    if (found == 1u) {
        p=append_uint(p, tone / 10u); *p++='.'; *p++=(char)('0' + tone % 10u); *p++='H'; *p++='Z';
    } else if (found == 2u) {
        *p++='D'; dcs_text(p, tone); p += 3; *p++='N';
    } else { *p++='-'; *p++='-'; *p++='-'; }
    *p=0;
}

static void draw(void)
{
    char f[18], t[18];
    A->display_clear();
    A->print_bold(mode == SC_FREQ ? "SEARCH FREQ" : "SEARCH TONE", 0, 127, 0);
    draw_dots(9);
    small_y(found ? "SCAN COMPLETE" : "SCANNING", 0, 12);
    frequency_text(f); small_y(f, 0, 23);
    tone_text(t); small_y(t, 0, 34);
    draw_dots(46); A->print_tiny(found ? "SAVE" : "SCANNING", 1, 49, false, true);
    A->print_tiny("EXIT", 112, 49, false, true);
    A->blit_full();
}

static void tune_css(uint32_t f)
{
    A->bk_write(REG_FREQ_LSB, (uint16_t)f);
    A->bk_write(REG_FREQ_MSB, (uint16_t)(f >> 16));
    A->bk_write(REG_CSS, 0x0300u);
    /* BK4829 RX_TurnOn value.  0x1f0f is the older BK4819 sequence and leaves
     * CTCSS/CDCSS scanning unable to complete on this target. */
    A->bk_write(0x37u, 0x9f1fu);
    A->bk_write(REG_RX, 0u); A->bk_write(REG_RX, 0xbff1u);
}

static void service_radio(void)
{
    if (A->api_level >= 3u &&
        A->api_size >= offsetof(app_api_t, radio_service) + sizeof(A->radio_service) &&
        A->radio_service)
        A->radio_service();
}

static bool save_api(void)
{
    if (A->api_level < 3u ||
        A->api_size < offsetof(app_api_t, search_save) + sizeof(A->search_save) ||
        !A->search_default_channel || !A->search_channel_info || !A->search_save)
        return false;
    return true;
}

static uint8_t get_key_press(void)
{
    const uint8_t key = A->get_key();
    if (key == APP_KEY_INVALID || key == APP_KEY_SAVER || key == APP_KEY_WAKE)
        return key;
    while (A->get_key() == key) {
        service_radio();
        A->delay_ms(10);
    }
    return key;
}

static char *append_text(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

static void save_status(char out[20], uint16_t channel, bool confirm,
                        const uint8_t digits[4], uint8_t digit_count)
{
    char *p = append_text(out, confirm ? "SAVE?" : "SAVE:");
    if (digit_count) {
        for (uint8_t i = 0; i < digit_count; i++) *p++ = (char)('0' + digits[i]);
        *p = 0;
        return;
    }
    char name[11];
    const bool used = A->search_channel_info(channel, name, sizeof(name));
    if (!used) p = append_text(p, "CH-");
    u32(p, (uint32_t)channel + 1u, 4u); p += 4;
    if (used) {
        *p++ = ' ';
        p = append_text(p, name[0] ? name : "USED");
        if (p > out + 18) p = out + 18;
    }
    *p = 0;
}

static void draw_save(uint16_t channel, bool confirm,
                      const uint8_t digits[4], uint8_t digit_count)
{
    char s[20], f[18], t[18];
    A->display_clear();
    A->print_bold(mode == SC_FREQ ? "SEARCH FREQ" : "SEARCH TONE", 0, 127, 0);
    draw_dots(9);
    save_status(s, channel, confirm, digits, digit_count); small_y(s, 0, 12);
    frequency_text(f); small_y(f, 0, 23);
    tone_text(t); small_y(t, 0, 34);
    draw_dots(46); A->print_tiny("SAVE", 1, 49, false, true);
    A->print_tiny("EXIT", 112, 49, false, true);
    A->blit_full();
}

static bool save_dialog(void)
{
    if (!save_api()) return false;
    uint16_t channel = A->search_default_channel();
    uint8_t digits[4] = {0};
    uint8_t digit_count = 0u;
    bool confirm = false;

    for (;;) {
        draw_save(channel, confirm, digits, digit_count);
        service_radio();
        const uint8_t k = get_key_press();
        if (k == APP_KEY_EXIT) {
            if (digit_count) digit_count--;
            else if (confirm) confirm = false;
            else { draw(); return false; }
        } else if (!confirm && k <= APP_KEY_9) {
            if (digit_count < 4u) digits[digit_count++] = k;
            if (digit_count == 4u) {
                const uint16_t entered = (uint16_t)(((digits[0] * 10u + digits[1]) * 10u
                                             + digits[2]) * 10u + digits[3]);
                digit_count = 0u;
                if (entered >= 1u && entered <= 1024u) channel = (uint16_t)(entered - 1u);
            }
        } else if (!confirm && (k == APP_KEY_UP || k == APP_KEY_DOWN)) {
            const int8_t direction = A->nav_dir(k);
            digit_count = 0u;
            if (direction > 0) channel = channel == 1023u ? 0u : (uint16_t)(channel + 1u);
            else if (direction < 0) channel = channel == 0u ? 1023u : (uint16_t)(channel - 1u);
        } else if (k == APP_KEY_MENU && !digit_count) {
            if (!confirm) confirm = true;
            else if (A->search_save(freq, tone, found, mode == SC_TONE, channel)) return true;
        }
        A->delay_ms(20);
    }
}

static void start(void)
{
    found = 0; tone = 0;
    scan_prev = 0u; last_ctcss = 0u; scan_hits = 0u; tone_hits = 0u;
    if (mode == SC_FREQ) { freq = 0; A->bk_write(REG_SCAN, 0x0245u); }
    else { freq = A->rx_freq(); tune_css(freq); }
    draw();
}

static void poll(void)
{
    if (mode == SC_FREQ && !freq) {
        uint16_t h = A->bk_read(REG_FREQ_H);
        if (!(h & 0x8000u)) {
            uint32_t f = ((uint32_t)(h & 0x07ffu) << 16) | A->bk_read(REG_FREQ_L);
            uint32_t d = f > scan_prev ? f - scan_prev : scan_prev - f;
            scan_hits = d < 100u ? (uint8_t)(scan_hits + 1u) : 0u; scan_prev = f;
            A->bk_write(REG_SCAN, 0x0244u);
            if (scan_hits >= 3u) { freq = f; tune_css(freq); draw(); }
            else A->bk_write(REG_SCAN, 0x0245u);
        }
        return;
    }
    if (found) return;
    uint16_t h = A->bk_read(REG_DCS_H);
    if (!(h & 0x8000u)) {
        uint32_t raw = ((uint32_t)(h & 0x0fffu) << 12) | (A->bk_read(REG_DCS_L) & 0x0fffu);
        uint16_t decoded = decode_dcs(raw);
        A->bk_write(REG_RX, 0u);
        if (decoded != 0xffffu) { tone = decoded; found = 2u; draw(); }
        else tune_css(freq);
        return;
    }
    uint16_t c = A->bk_read(REG_CTCSS);
    if (!(c & 0x8000u)) {
        const uint16_t measured = normalize_ctcss((uint16_t)(((uint32_t)(c & 0x1fffu) * 4843u) / 10000u));
        A->bk_write(REG_RX, 0u);
        if (measured == last_ctcss) {
            if (++tone_hits >= 2u) { tone = measured; found = 1u; draw(); return; }
        } else {
            last_ctcss = measured;
            tone_hits = 0u;
        }
        tune_css(freq);
    }
}

static uint8_t launch_mode(void)
{
    if (A->api_level >= 3u && A->api_size >= offsetof(app_api_t, launch_shortcut) + sizeof(A->launch_shortcut) && A->launch_shortcut)
        return A->launch_shortcut();
    return 0u;
}

__attribute__((section(".text.entry"))) void app_main(const app_api_t *api)
{
    A = api; mode = launch_mode();
    if (mode != SC_FREQ && mode != SC_TONE) {
        mode = SC_FREQ;
        for (;;) {
            A->display_clear(); A->print_bold("SEARCH", 0, 127, 0); draw_dots(9);
            small_y(mode == SC_FREQ ? "> FREQUENCY" : "  FREQUENCY", 0, 16);
            small_y(mode == SC_TONE ? "> TONE" : "  TONE", 0, 29);
            draw_dots(46); A->print_tiny("SELECT", 1, 49, false, true); A->print_tiny("EXIT", 112, 49, false, true); A->blit_full();
            service_radio();
            uint8_t k = get_key_press();
            if (k == APP_KEY_EXIT) return;
            if (k == APP_KEY_UP || k == APP_KEY_DOWN) mode = mode == SC_FREQ ? SC_TONE : SC_FREQ;
            if (k == APP_KEY_MENU) break;
        }
    }
    start();
    for (;;) {
        service_radio();
        uint8_t k = get_key_press();
        if (k == APP_KEY_EXIT || k == APP_KEY_PTT) break;
        if (k == APP_KEY_MENU && found) {
            if (save_dialog()) return;
        } else if (k == APP_KEY_STAR) {
            start();
        }
        poll(); A->delay_ms(20);
    }
    A->bk_write(REG_SCAN, 0x0244u); A->bk_write(REG_RX, 0u);
}

uint64_t __aeabi_uidivmod(uint32_t n, uint32_t d) { return A->uidivmod(n, d); }
uint32_t __aeabi_uidiv(uint32_t n, uint32_t d) { return (uint32_t)A->uidivmod(n, d); }
