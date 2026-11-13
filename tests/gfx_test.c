/* Renders the whole font + blend test to a PPM so the glyph table can be inspected. */
#include <stdio.h>
#include <stdlib.h>
#include "../source/gfx.h"
uint32_t g_rng=1;
int main(void){
    static uint16_t buf[TOP_W*SCR_H]; Surf s={buf,TOP_W};
    gx_clear(s,RGB(20,24,34));
    gx_text(s,8,8,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",RGB(255,255,255),2);
    gx_text(s,8,30,"0123456789 !\"#$%&'()*+,-./:;<=>?@",RGB(255,220,120),2);
    gx_text(s,8,52,"[\\]^_ SENTENCE TEST: ROUND 12  CREDITS 9000",RGB(120,220,255),1);
    gx_rect(s,8,80,100,30,RGB(255,0,0)); gx_rect_a(s,40,90,100,30,RGB(0,0,255),16);
    gx_line(s,8,130,150,200,RGB(0,255,0)); gx_disc(s,200,150,20,RGB(255,160,0));
    FILE*f=fopen("/tmp/gfx.ppm","wb"); fprintf(f,"P6\n%d %d\n255\n",TOP_W,SCR_H);
    for(int y=0;y<SCR_H;y++)for(int x=0;x<TOP_W;x++){uint16_t c=gx_get(s,x,y);
        fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}
    fclose(f); return 0;}
