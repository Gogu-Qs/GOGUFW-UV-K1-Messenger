#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../decmts01.c"

static uint16_t make_crc(const uint8_t *p, uint8_t len)
{
    uint16_t rem=0xffff, rev=0;
    for (uint8_t i=0;i<len;i++) { rem^=(uint16_t)p[i]<<8; for(uint8_t j=0;j<8;j++) rem=(uint16_t)((rem<<1)^((rem&0x8000)?0x8005:0)); }
    for(uint8_t j=0;j<16;j++) if(rem&(1u<<(15-j))) rev|=1u<<j;
    return rev;
}

static void make_frame(uint8_t *f)
{
    static const char text[]="MTS12345,X,42,261021123456,3700,41.12345,29.54321,1234,182.5,12.3,X,15.0,15.0,50";
    memset(f,0,MTS01_FRAME_LEN); f[0]=0x80; memcpy(f+1,text,sizeof text-1);
    uint16_t c=make_crc(f+1,128); f[129]=(uint8_t)c; f[130]=(uint8_t)(c>>8);
}

static int feed(int invert)
{
    uint8_t f[MTS01_FRAME_LEN]; make_frame(f);
    decmts01_t d; decmts01_init(&d);
    mts01_info_t out;
    uint32_t sync=0xAAAAB42B;
    for (unsigned n=0;n<32+MTS01_FRAME_LEN*8;n++) {
        unsigned bit=n<32 ? ((sync>>(31-n))&1u) : ((f[(n-32)/8]>>(7-((n-32)&7)))&1u);
        if(invert) bit^=1;
        for(unsigned k=0;k<8;k++) if(decmts01_push(&d,bit?3072:1024)) goto done;
    }
    /* The integrator emits on the recovered symbol boundary, so let the last
     * symbol run into the following idle interval. */
    for (unsigned k=0;k<8;k++) if(decmts01_push(&d,(f[130]&1u)^invert?3072:1024)) break;
done:
    if(!decmts01_parse(&d,&out)) { fprintf(stderr,"state=%u bits=%u sync=%08x first=%02x\n",d.state,d.bit_count,(unsigned)d.sync,d.frame[0]); return 1; }
    if(strcmp(out.id,"MTS12345") || out.frame!=42 || out.lat_e5!=4112345 || out.lon_e5!=2954321 || out.alt_m!=1234 || out.heading_10!=1825 || out.speed_10!=123 || out.batt_mv!=3700 || out.temp_10!=0) return 2;
    return 0;
}

int main(void)
{
    int a=feed(0), b=feed(1);
    if(a||b) { fprintf(stderr,"MTS01 test failed: normal=%d inverted=%d\n",a,b); return 1; }
    uint8_t f[MTS01_FRAME_LEN]; make_frame(f); f[20]^=1u;
    decmts01_t d; decmts01_init(&d); memcpy(d.frame,f,sizeof f);
    mts01_info_t out;
    if (decmts01_parse(&d,&out)) { fputs("bad CRC accepted\n",stderr); return 1; }
    puts("MTS01 decoder: normal/inverted frames OK; bad CRC rejected");
    return 0;
}
