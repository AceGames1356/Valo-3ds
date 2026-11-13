/* Nintendo 3DS platform layer (libctru). Top/bottom screens run in RGB565 and the
   game draws straight into the native column-major framebuffers. */
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "platform.h"

static bool s_quit;
static bool s_new3ds;

bool plat_init(void){
    gfxInit(GSP_RGB565_OES,GSP_RGB565_OES,false);
    gfxSetDoubleBuffering(GFX_TOP,true);
    gfxSetDoubleBuffering(GFX_BOTTOM,true);
    gfxSet3D(false);
    s_new3ds=false;
    APT_CheckNew3DS(&s_new3ds);
    osSetSpeedupEnable(true);                 /* New 3DS: 804 MHz + L2 cache; harmless on Old 3DS */
#if defined(V3DS_CSTICK_IRRST)
    irrstInit();
#endif
    mkdir("sdmc:/3ds",0777);
    mkdir("sdmc:/3ds/valorant3ds",0777);
    s_quit=false;
    return true;
}

void plat_exit(void){
#if defined(V3DS_CSTICK_IRRST)
    irrstExit();
#endif
    gfxExit();
}

bool plat_running(void){ return !s_quit && aptMainLoop(); }
void plat_request_quit(void){ s_quit=true; }

static const struct { uint32_t key; int bit; } KMAP[]={
    {KEY_A,B_A},{KEY_B,B_B},{KEY_X,B_X},{KEY_Y,B_Y},{KEY_L,B_L},{KEY_R,B_R},{KEY_ZL,B_ZL},{KEY_ZR,B_ZR},
    {KEY_START,B_START},{KEY_SELECT,B_SELECT},{KEY_DUP,B_DUP},{KEY_DDOWN,B_DDOWN},{KEY_DLEFT,B_DLEFT},{KEY_DRIGHT,B_DRIGHT}
};

static uint32_t convert(uint32_t keys){
    uint32_t m=0;
    for(unsigned i=0;i<sizeof KMAP/sizeof KMAP[0];i++) if(keys&KMAP[i].key) m|=BM(KMAP[i].bit);
    return m;
}

void plat_poll(Input *in){
    static int lastTx,lastTy; static bool wasTouch;
    hidScanInput();
    u32 held=hidKeysHeld(), down=hidKeysDown(), up=hidKeysUp();
    in->held=convert(held); in->down=convert(down); in->up=convert(up);
    circlePosition cp; hidCircleRead(&cp);
    in->cpx=cp.dx; in->cpy=cp.dy;
    in->csx=in->csy=0;
#if defined(V3DS_CSTICK_HID)
    { circlePosition cs; hidCstickRead(&cs); in->csx=cs.dx; in->csy=cs.dy; }
#elif defined(V3DS_CSTICK_IRRST)
    { circlePosition cs; irrstScanInput(); irrstCstickRead(&cs); in->csx=cs.dx; in->csy=cs.dy; }
#endif
    in->touching=(held&KEY_TOUCH)!=0;
    in->tdown=(down&KEY_TOUCH)!=0; in->tup=(up&KEY_TOUCH)!=0;
    in->tdx=in->tdy=0;
    if(in->touching){
        touchPosition tp; hidTouchRead(&tp);
        in->tx=tp.px; in->ty=tp.py;
        if(wasTouch&&!in->tdown){ in->tdx=tp.px-lastTx; in->tdy=tp.py-lastTy; }
        lastTx=tp.px; lastTy=tp.py;
    }
    wasTouch=in->touching;
    in->new3ds=s_new3ds;
}

Surf plat_top(void){ Surf s={(uint16_t*)gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL),TOP_W}; return s; }
Surf plat_bottom(void){ Surf s={(uint16_t*)gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,NULL,NULL),BOT_W}; return s; }
void plat_present(void){ gfxFlushBuffers(); gfxSwapBuffers(); }
void plat_wait_frame(void){ gspWaitForVBlank(); }
uint32_t plat_ms(void){ return (uint32_t)osGetTime(); }

static void path_for(const char *name,char *out,size_t n){ snprintf(out,n,"sdmc:/3ds/valorant3ds/%s",name); }
bool plat_load(const char *name,void *buf,size_t n){
    char p[160]; path_for(name,p,sizeof p);
    FILE *f=fopen(p,"rb"); if(!f) return false;
    size_t r=fread(buf,1,n,f); fclose(f); return r==n;
}
bool plat_save(const char *name,const void *buf,size_t n){
    char p[160]; path_for(name,p,sizeof p);
    FILE *f=fopen(p,"wb"); if(!f) return false;
    size_t w=fwrite(buf,1,n,f); fclose(f); return w==n;
}
