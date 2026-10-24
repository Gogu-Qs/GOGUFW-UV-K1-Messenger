/* Sonde MTS01 for UV-K1 / UV-K5 v3, GPL-3.0-or-later.
 * Application shell based on Apache-2.0 Sonde M20 by Armel F4HWN;
 * MTS01 framing/CRC follows projecthorus/radiosonde_auto_rx.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../app_api.h"
#include "decmts01.c"

static const app_api_t *Gdiv;
static const gogu_api_t *G;
uint64_t __aeabi_uidivmod(uint32_t n, uint32_t d){ return Gdiv->uidivmod(n, d); }
uint64_t __aeabi_uidiv(uint32_t n, uint32_t d) __attribute__((alias("__aeabi_uidivmod")));
uint64_t __aeabi_idivmod(int32_t n, int32_t d){ return Gdiv->idivmod(n, d); }
uint64_t __aeabi_idiv(int32_t n, int32_t d) __attribute__((alias("__aeabi_idivmod")));

#define FS 9600u
#define CYC_PER_SAMPLE (48000000u / FS)
#define CFG_MAGIC 0x31u
#define SCAN_LO 40015000u
#define SCAN_HI 40600000u
#define SCAN_START 40190000u /* Isparta MTS01 priority: 402.000 MHz */
#define SCAN_STEP 1000u       /* frequency units are 10 Hz: 10 kHz */
#define SCAN_THRESHOLD (-116)
#define PROBE_MS 2500u
#define LOST_MS 20000u

static struct {
    const app_api_t *A;
    mts01_info_t info;
    uint32_t freq;
    uint32_t scan_freq;
    uint16_t frames;
    int16_t rssi;
    uint8_t prev_key, speaker;
    bool running, scanning;
} g;

static char *put(char *o, const char *s){ while (*s) *o++ = *s++; return o; }
static char *putu(char *o, uint32_t v)
{
    char t[10]; uint8_t n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *o++ = t[--n];
    return o;
}
static char *puti(char *o, int32_t v)
{ if (v < 0) { *o++ = '-'; v = -v; } return putu(o, (uint32_t)v); }
static char *put1(char *o, int32_t v)
{
    if (v < 0) { *o++ = '-'; v = -v; }
    o = putu(o, (uint32_t)v / 10u); *o++ = '.'; *o++ = (char)('0' + v % 10); return o;
}
static char *putpos(char *o, int32_t v, char plus, char minus)
{
    uint32_t a = (uint32_t)(v < 0 ? -v : v);
    o = putu(o, a / 100000u); *o++ = '.';
    uint32_t r = a % 100000u, d = 10000u;
    while (d) { *o++ = (char)('0' + (r / d) % 10u); d /= 10u; }
    *o++ = v < 0 ? minus : plus; return o;
}
static char *putfreq(char *o, uint32_t f)
{
    o = putu(o, f / 100000u); *o++ = '.'; f %= 100000u;
    *o++ = (char)('0' + f / 10000u); *o++ = (char)('0' + (f / 1000u) % 10u);
    *o++ = (char)('0' + (f / 100u) % 10u); return o;
}
static void row(char *s, char *e, uint8_t line)
{ *e = 0; g.A->print_normal(s, 0, 0, line); }

static void draw(void)
{
    char s[34], *o;
    g.A->display_clear(); g.A->status_clear(); g.A->draw_battery();
    g.A->print_inverse("Sonde MTS01", 2, 0, true, true, 46);
    if (g.scanning) g.A->print_bold("SCANNING", 0, 0, 0);
    else if (g.frames && (g.info.flags & MTS01_HAS_ID)) g.A->print_bold(g.info.id, 0, 0, 0);
    else g.A->print_bold("WAITING", 0, 0, 0);
    if (g.frames) {
        o = putpos(s, g.info.lat_e5, 'N', 'S'); *o++ = ' ';
        o = putpos(o, g.info.lon_e5, 'E', 'W'); row(s, o, 1);
        o = put(s, "ALT "); o = puti(o, g.info.alt_m); o = put(o, "m  V ");
        o = put1(o, g.info.speed_10); o = put(o, "m/s"); row(s, o, 2);
        o = put(s, "HDG "); o = put1(o, g.info.heading_10);
        if (g.info.flags & MTS01_HAS_TEMP) { o = put(o, "  T "); o = put1(o, g.info.temp_10); o = put(o, "C"); }
        row(s, o, 3);
        o = put(s, "BAT "); o = put1(o, g.info.batt_mv / 100u); o = put(o, "V  #");
        o = putu(o, g.info.frame); row(s, o, 4);
    }
    for (uint8_t x = 0; x < 128u; x += 2u) g.A->fb[5][x] |= 1u;
    o = putfreq(s, g.freq); *o = 0; g.A->print_tiny(s, 0, 43, false, true);
    o = puti(s, g.rssi); o = put(o, "dBm"); *o = 0;
    g.A->print_tiny(s, (uint8_t)(128u - (uint8_t)(o - s) * 4u), 43, false, true);
    o = put(s, "FRAMES "); o = putu(o, g.frames); *o = 0; g.A->print_tiny(s, 0, 49, false, true);
    g.A->print_tiny(g.speaker ? "1 SPK ON" : "1 SPK OFF", 84, 49, false, true);
}

static void raw_rx(void)
{
    g.A->bk_write(0x2B, (uint16_t)((g.A->bk_read(0x2B)|0x0700u)&~0x0007u));
    g.A->bk_write(0x73, (uint16_t)(g.A->bk_read(0x73)|0x0010u));
    g.A->set_af(APP_AF_FM); g.A->bk_write(0x47, 0x6942u);
}

static bool key(void)
{
    uint8_t k = g.A->get_key();
    if (k == APP_KEY_INVALID || k == g.prev_key) { g.prev_key = k; return false; }
    g.prev_key = k;
    if (k == APP_KEY_EXIT) g.running = false;
    else if (k == APP_KEY_1) { g.speaker ^= 1u; g.A->audio_path(g.speaker); }
    else if (k == APP_KEY_2) { g.frames = 0; }
    else return false;
    return true;
}

static void receive(void)
{
    decmts01_t d; decmts01_init(&d); g.A->af_capture_open();
    uint32_t next=g.A->cycles_48mhz(); uint16_t idle=0;
    uint32_t last=g.A->ticks_ms(); bool locked=false;
    while (g.running) {
        while((int32_t)(g.A->cycles_48mhz()-next)<0) {}
        next += CYC_PER_SAMPLE;
        bool redraw=false;
        if (decmts01_push(&d, g.A->af_capture_sample())) {
            mts01_info_t info;
            if (decmts01_parse(&d, &info)) {
                g.info=info; g.frames++; redraw=true; locked=true;
                last=g.A->ticks_ms(); g.A->led(true);
            }
            decmts01_rearm(&d);
        }
        if (decmts01_busy(&d)) continue;
        if ((uint32_t)(g.A->ticks_ms()-last) >= (locked ? LOST_MS : PROBE_MS)) break;
        if (!redraw && ++idle < 2400u) continue;
        idle=0;
        bool changed=key(); if (!g.running) break;
        if (redraw || changed) {
            g.rssi=g.A->rssi_dbm(); g.A->af_capture_close(); g.A->battery_sample(); g.A->af_capture_open();
            draw(); g.A->blit_status(); g.A->blit_full(); g.A->backlight_on(); g.A->led(false);
        }
        decmts01_rearm(&d); next=g.A->cycles_48mhz()+CYC_PER_SAMPLE;
    }
    g.A->af_capture_close(); g.A->led(false);
}

static void scan_once(void)
{
    uint32_t f=g.scan_freq, best_f=0; int16_t best=-127; bool band=false;
    g.scanning=true; g.A->audio_path(false);
    for (uint16_t n=0; n <= (SCAN_HI-SCAN_LO)/SCAN_STEP && g.running; n++) {
        g.freq=f; G->radio_tune(f); g.A->delay_ms(15); g.rssi=g.A->rssi_dbm();
        if (g.rssi >= SCAN_THRESHOLD) {
            band=true; if (g.rssi > best) { best=g.rssi; best_f=f; }
        } else if (band) {
            g.scan_freq=f; g.freq=best_f; g.scanning=false;
            G->radio_tune(best_f); raw_rx(); g.A->delay_ms(40);
            g.A->audio_path(g.speaker); draw(); g.A->blit_status(); g.A->blit_full();
            receive(); return;
        }
        if (!(n%40u)) {
            if (key()) break;
            g.A->battery_sample(); draw(); g.A->blit_status(); g.A->blit_full();
        }
        f += SCAN_STEP; if (f > SCAN_HI) f=SCAN_LO;
    }
    g.scan_freq=f; g.scanning=false;
    /* Do not drop a candidate whose occupied region crosses the scan wrap. */
    if (band && g.running) {
        g.freq=best_f; G->radio_tune(best_f); raw_rx(); g.A->delay_ms(40);
        g.A->audio_path(g.speaker); draw(); g.A->blit_status(); g.A->blit_full();
        receive();
    }
}

__attribute__((section(".text.entry"), used))
void app_main(const app_api_t *api)
{
    uint8_t cfg[2];
    Gdiv=api; g.A=api; G=APP_GoguApi(api); if(!G)return; g.prev_key=APP_KEY_INVALID; g.freq=SCAN_START; g.scan_freq=SCAN_START;
    api->cfg_load(cfg, 2); g.speaker = cfg[0]==CFG_MAGIC && cfg[1]==1u;
    uint16_t r47=api->bk_read(0x47); api->audio_path(false); api->backlight_on();
    g.running=true; g.rssi=api->rssi_dbm(); draw(); api->blit_status(); api->blit_full();
    while (g.running) scan_once();
    api->bk_write(0x47, r47); api->set_af(APP_AF_MUTE); api->audio_path(false);
    cfg[0]=CFG_MAGIC; cfg[1]=g.speaker; api->cfg_save(cfg, 2);
}
