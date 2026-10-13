#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../app_api.h"
#include "aprsedit_assets.h"

#define MSG_LEN 23u
#define TX_CFG_LEN 13u
#define EDIT_CFG_LEN (TX_CFG_LEN + MSG_LEN)
#define CFG_MAGIC 0xA8u
#define POS_DIGITS 13u
#define LAT_DIGITS 6u
#define PATH_MAX 2u
#define HOLD_TICKS 40u
#define REPEAT_TICKS 8u
#define COMMIT_TICKS 80u
#define CUR_NS LAT_DIGITS
#define CUR_EW (POS_DIGITS + 1u)
#define CUR_SSID (CUR_EW + 1u)
#define CUR_PATH (CUR_EW + 2u)
#define CUR_SYM (CUR_EW + 3u)

static const app_api_t *A;
static char text[MSG_LEN + 1u];
static uint8_t cfg[EDIT_CFG_LEN], pos[POS_DIGITS];
static uint8_t len, mode, pending, cycle, held, page, cur;
static uint8_t hemi, ssid, path, sym;
static uint16_t pending_ticks, held_ticks;
static bool has_pending, fired_long, dirty, running, from_tx, f_arm, bad_pos;

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d=dst; const uint8_t *s=src;
    while(n--) *d++=*s++;
    return dst;
}

static uint8_t slen(const char *s, uint8_t max)
{
    uint8_t n=0u; while(n<max && s[n]) n++; return n;
}

static void dots(uint8_t y)
{
    for(uint8_t x=1u;x<127u;x+=4u) A->draw_line(A->fb,x,y,(int16_t)(x+1u),y,true);
}

static char *u8toa(char *o, uint8_t v)
{
    if(v>=10u) *o++=(char)('0'+v/10u);
    *o++=(char)('0'+v%10u); *o=0; return o;
}

static void draw_message(void)
{
    char count[6], mode_text[4]="*:B", line[18];
    char *o=u8toa(count,len); *o++='/'; *o++='2'; *o++='3'; *o=0;
    mode_text[2]=mode==0u?'B':mode==1u?'b':'2';
    A->display_clear(); A->status_clear();
    A->print_bold("APRS MESSAGE",0u,127u,0u);
    A->print_tiny(count,(uint8_t)(128u-slen(count,5u)*4u),2u,false,true);
    A->print_tiny(mode_text,1u,2u,false,true); dots(9u);
    for(uint8_t row=0u,off=0u;row<2u && text[off];row++,off+=17u){
        uint8_t n=0u; while(n<17u && text[off+n]){ line[n]=text[off+n]; n++; }
        line[n]=0; A->print_small_y(line,1u,(uint8_t)(13u+row*10u),false);
    }
    dots(46u); A->print_tiny("SAVE",1u,49u,false,true);
    A->print_tiny("PTT:SET",43u,49u,false,true); A->print_tiny("EXIT",112u,49u,false,true);
}

static void put_coord(char *s,const char *label,const uint8_t *d,uint8_t n,char h,int8_t cursor,uint8_t line)
{
    char *o=s; while(*label) *o++=*label++;
    uint8_t col=0u;
    for(uint8_t i=0u;i<n;i++){
        if(i==n-4u) *o++=' ';
        if(i==n-2u) *o++='.';
        if((int8_t)i==cursor) col=(uint8_t)(o-s);
        *o++=(char)('0'+d[i]);
    }
    *o++=' '; if((int8_t)n==cursor) col=(uint8_t)(o-s); *o++=h; *o=0;
    A->print_normal(s,0u,0u,line);
    if((uint8_t)cursor<=n){ char c[2]={s[col],0}; A->print_bold(c,(uint8_t)(col*7u),0u,line); }
}

static void field(const char *label,const char *value,uint8_t line,bool selected)
{
    char s[19]; uint8_t n=0u;
    while(*label) s[n++]=*label++;
    uint8_t x=n; while(*value && n<18u) s[n++]=*value++;
    s[n]=0; A->print_normal(s,0u,0u,line);
    if(selected) A->print_bold(s+x,(uint8_t)(x*7u),0u,line);
}

static void draw_settings(void)
{
    char s[22],v[4];
    static const char *const paths[3]={"DIRECT","WIDE1-1","WIDE1-1,2-1"};
    A->display_clear(); A->status_clear(); A->print_bold("APRS SETTINGS",0u,127u,0u);
    if(f_arm) A->print_tiny("F",70u,2u,false,true);
    put_coord(s,"LAT  ",pos,LAT_DIGITS,(hemi&1u)?'S':'N',(int8_t)cur,1u);
    put_coord(s,"LON ",pos+LAT_DIGITS,POS_DIGITS-LAT_DIGITS,(hemi&2u)?'W':'E',
              (int8_t)((int8_t)cur-(int8_t)(CUR_NS+1u)),2u);
    u8toa(v,ssid); field("SSID ",v,3u,cur==CUR_SSID); field("PATH ",paths[path],4u,cur==CUR_PATH);
    A->asset_read((uint16_t)(SYM_CODES+sym*2u),v,2u); v[2]=0; field("SYMB ",v,5u,cur==CUR_SYM);
    A->print_tiny(bad_pos?"INVALID POSITION":"PTT:MESSAGE",1u,49u,false,true);
    A->print_tiny("SAVE",88u,49u,false,true); A->print_tiny("EXIT",112u,49u,false,true);
}

static void draw(void)
{
    if(page) draw_settings(); else draw_message();
    A->blit_status(); A->blit_full();
}

static const char *key_chars(uint8_t key)
{
    static const char *const map[10]={" ",".,?!","ABC","DEF","GHI","JKL","MNO","PQRS","TUV","WXYZ"};
    return key<=APP_KEY_9?map[key]:"";
}

static void commit(void){ has_pending=false; pending=APP_KEY_INVALID; cycle=0u; pending_ticks=0u; }
static char letter_case(char c){ return mode==1u && c>='A' && c<='Z'?(char)(c+('a'-'A')):c; }
static void insert_digit(uint8_t key){ commit(); if(len<MSG_LEN){ text[len++]=(char)('0'+key); text[len]=0; } }

static void edit_message(uint8_t key,bool long_press)
{
    if(long_press && key<=APP_KEY_9){ insert_digit(key); return; }
    if(key==APP_KEY_STAR){ commit(); mode=(uint8_t)((mode+1u)%3u); return; }
    if(key==APP_KEY_F){ if(len) text[--len]=0; commit(); return; }
    if(key>APP_KEY_9) return;
    if(mode==2u){ insert_digit(key); return; }
    const char *chars=key_chars(key); uint8_t count=slen(chars,4u);
    if(has_pending && pending==key && len){ cycle=(uint8_t)((cycle+1u)%count); text[len-1u]=letter_case(chars[cycle]); pending_ticks=0u; return; }
    commit(); if(len<MSG_LEN){ pending=key; has_pending=true; text[len++]=letter_case(chars[0]); text[len]=0; }
}

static bool pos_ok(void)
{
    for(uint8_t i=0u;i<POS_DIGITS;i++) if(pos[i]>9u) return false;
    uint8_t la=(uint8_t)(pos[0]*10u+pos[1]),lm=(uint8_t)(pos[2]*10u+pos[3]);
    uint16_t lo=(uint16_t)(pos[6]*100u+pos[7]*10u+pos[8]); uint8_t om=(uint8_t)(pos[9]*10u+pos[10]);
    if(la>90u || lm>59u || lo>180u || om>59u) return false;
    if(la==90u && (lm|pos[4]|pos[5])) return false;
    if(lo==180u && (om|pos[11]|pos[12])) return false;
    return true;
}

static void save_and_exit(void)
{
    if(!pos_ok()){ bad_pos=true; page=1u; dirty=true; return; }
    cfg[0]=CFG_MAGIC; for(uint8_t i=3u;i<=10u;i++) cfg[i]=0u;
    for(uint8_t i=0u;i<POS_DIGITS;i++) cfg[3u+i/2u]|=(uint8_t)(pos[i]<<((i&1u)*4u));
    cfg[9]|=(uint8_t)(hemi<<4); cfg[10]=(uint8_t)(0x80u|(path<<4)|ssid); cfg[12]=sym;
    for(uint8_t i=0u;i<MSG_LEN;i++) cfg[TX_CFG_LEN+i]=i<len?(uint8_t)text[i]:(uint8_t)' ';
    A->cfg_save(cfg,EDIT_CFG_LEN); if(from_tx) A->chain_app(APP_CHAIN_APRS_TX); running=false;
}

static void edit_settings(uint8_t key)
{
    bad_pos=false;
    if(key==APP_KEY_F){ f_arm=!f_arm; return; }
    bool back=f_arm; f_arm=false; uint8_t c=cur; bool lon=c>CUR_NS && c<=CUR_EW;
    if(key<=APP_KEY_9){
        if(c<CUR_NS || (lon && c<CUR_EW)){ pos[c-lon]=key; if(++c==CUR_NS) c++; if(c==CUR_EW) c--; cur=c; }
    } else if(key==APP_KEY_STAR){
        if(c<=CUR_EW) hemi^=(uint8_t)(lon?2u:1u);
        else if(c==CUR_SSID) ssid=(uint8_t)((ssid+(back?15u:1u))&15u);
        else if(c==CUR_PATH) path=back?(uint8_t)(path?path-1u:PATH_MAX):(uint8_t)(path<PATH_MAX?path+1u:0u);
        else sym=back?(uint8_t)(sym?sym-1u:SYM_COUNT-1u):(uint8_t)(sym+1u<SYM_COUNT?sym+1u:0u);
    } else { int8_t d=A->nav_dir(key); if(d<0 && cur) cur--; if(d>0 && cur<CUR_SYM) cur++; }
}

static void short_key(uint8_t key)
{
    A->backlight_on();
    if(key==APP_KEY_MENU) save_and_exit();
    else if(key==APP_KEY_EXIT){ if(from_tx) A->chain_app(APP_CHAIN_APRS_TX); running=false; }
    else if(key==APP_KEY_PTT){ page^=1u; f_arm=false; commit(); }
    else if(page) edit_settings(key); else edit_message(key,false);
    dirty=true;
}

__attribute__((section(".text.entry"),used))
void app_main(const app_api_t *api)
{
    A=api; held=APP_KEY_INVALID; running=true; dirty=true;
    from_tx=api->launch_shortcut && api->launch_shortcut()==APP_LAUNCH_FROM_APRS_TX;
    api->cfg_load(cfg,EDIT_CFG_LEN);
    if(cfg[0]!=CFG_MAGIC){
        cfg[0]=CFG_MAGIC; cfg[1]=66u; cfg[2]=0u; cfg[10]=0x97u; cfg[11]=0u; cfg[12]=SYM_DEFAULT;
        api->asset_read(POS_DEFAULT,pos,POS_DIGITS); hemi=HEMI_DEFAULT; ssid=7u; path=1u; sym=SYM_DEFAULT;
    } else {
        for(uint8_t i=0u;i<POS_DIGITS;i++) pos[i]=(uint8_t)((cfg[3u+i/2u]>>((i&1u)*4u))&15u);
        hemi=(uint8_t)(cfg[9]>>4); ssid=(uint8_t)(cfg[10]&15u); path=(uint8_t)((cfg[10]>>4)&3u); sym=cfg[12];
        if(hemi>3u || !pos_ok()){ api->asset_read(POS_DEFAULT,pos,POS_DIGITS); hemi=HEMI_DEFAULT; }
        if((cfg[10]&0xC0u)!=0x80u || path>PATH_MAX){ ssid=7u; path=1u; }
        if(sym>=SYM_COUNT) sym=SYM_DEFAULT;
    }
    const uint8_t *saved=cfg+TX_CFG_LEN;
    if(saved[0]==0xFFu){ static const char initial[]="UV-K5/K1 F4HWN Firmware"; memcpy(text,initial,MSG_LEN); len=MSG_LEN; }
    else { memcpy(text,saved,MSG_LEN); len=MSG_LEN; while(len && (text[len-1u]==' ' || (uint8_t)text[len-1u]==0xFFu)) len--; }
    text[len]=0; api->backlight_on();
    while(running){
        if(dirty){ dirty=false; draw(); }
        uint8_t key=api->get_key();
        if(key==APP_KEY_SAVER || key==APP_KEY_WAKE){ held=APP_KEY_INVALID; held_ticks=0u; fired_long=false; dirty=true; }
        else if(key==APP_KEY_INVALID){ if(held!=APP_KEY_INVALID && !fired_long) short_key(held); held=APP_KEY_INVALID; held_ticks=0u; fired_long=false; }
        else if(key!=held){ held=key; held_ticks=0u; fired_long=false; }
        else if(!page){
            held_ticks++;
            if(!fired_long && key<=APP_KEY_9 && held_ticks>=HOLD_TICKS){ fired_long=true; edit_message(key,true); dirty=true; }
            else if(key==APP_KEY_F && held_ticks>=HOLD_TICKS && (uint16_t)(held_ticks-HOLD_TICKS)%REPEAT_TICKS==0u){ fired_long=true; edit_message(key,false); dirty=true; }
        }
        if(has_pending && ++pending_ticks>=COMMIT_TICKS) commit();
        api->battery_sample(); api->delay_ms(10u); api->backlight_update();
    }
}
