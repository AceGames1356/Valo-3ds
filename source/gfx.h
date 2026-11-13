#ifndef GFX_H
#define GFX_H
#include "common.h"

static inline void gx_px(Surf s,int x,int y,uint16_t c){
    if((unsigned)x<(unsigned)s.w && (unsigned)y<SCR_H) s.p[x*SCR_H + (SCR_H-1-y)] = c;
}
static inline uint16_t gx_get(Surf s,int x,int y){ return s.p[x*SCR_H + (SCR_H-1-y)]; }
/* t = 0..32 : 0 returns a, 32 returns b */
static inline uint16_t gx_blend(uint16_t a,uint16_t b,int t){
    uint32_t A=(a|((uint32_t)a<<16))&0x07E0F81Fu, B=(b|((uint32_t)b<<16))&0x07E0F81Fu;
    uint32_t r=((((B-A)*(uint32_t)t)>>5)+A)&0x07E0F81Fu;
    return (uint16_t)(r|(r>>16));
}
void gx_clear(Surf s,uint16_t c);
void gx_rect(Surf s,int x,int y,int w,int h,uint16_t c);
void gx_rect_a(Surf s,int x,int y,int w,int h,uint16_t c,int a32);   /* alpha blend */
void gx_frame(Surf s,int x,int y,int w,int h,uint16_t c);
void gx_line(Surf s,int x0,int y0,int x1,int y1,uint16_t c);
void gx_disc(Surf s,int cx,int cy,int r,uint16_t c);
void gx_text(Surf s,int x,int y,const char *t,uint16_t c,int scale);
void gx_textc(Surf s,int cx,int y,const char *t,uint16_t c,int scale);
void gx_textr(Surf s,int rx,int y,const char *t,uint16_t c,int scale);
void gx_texts(Surf s,int x,int y,const char *t,uint16_t c,int scale);  /* with shadow */
int  gx_textw(const char *t,int scale);
#endif
