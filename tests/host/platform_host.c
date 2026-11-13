/* Desktop implementation of platform.h. Used ONLY by the automated tests in tests/ -
   it is not part of the Nintendo 3DS build. Framebuffers are plain memory. */
#include "../../source/platform.h"
#include <stdio.h>
#include <stdlib.h>

static uint16_t s_top[TOP_W*SCR_H], s_bot[BOT_W*SCR_H];
static Input    s_in;
static bool     s_quit;
static uint32_t s_ms;
static long     s_polls, s_maxPolls=-1;

bool plat_init(void){ s_quit=false; return true; }
void plat_exit(void){}
bool plat_running(void){ if(s_maxPolls<0){ const char *e=getenv("V3DS_HOST_POLLS"); s_maxPolls=e?atol(e):0x7fffffff; } return !s_quit&&s_polls<s_maxPolls; }
void plat_poll(Input *in){ s_polls++; s_ms+=16; *in=s_in; s_in.down=s_in.up=0; s_in.tdown=s_in.tup=false; s_in.tdx=s_in.tdy=0; }
Surf plat_top(void){ Surf s={s_top,TOP_W}; return s; }
Surf plat_bottom(void){ Surf s={s_bot,BOT_W}; return s; }
void plat_present(void){}
void plat_wait_frame(void){}
uint32_t plat_ms(void){ return s_ms; }
void plat_request_quit(void){ s_quit=true; }
bool plat_load(const char *name,void *buf,size_t n){
    char p[128]; snprintf(p,sizeof p,"/tmp/v3ds_%s",name);
    FILE *f=fopen(p,"rb"); if(!f) return false;
    size_t r=fread(buf,1,n,f); fclose(f); return r==n;
}
bool plat_save(const char *name,const void *buf,size_t n){
    char p[128]; snprintf(p,sizeof p,"/tmp/v3ds_%s",name);
    FILE *f=fopen(p,"wb"); if(!f) return false;
    size_t w=fwrite(buf,1,n,f); fclose(f); return w==n;
}

/* test helpers */
void host_set_input(const Input *in){ s_in=*in; }
void host_press(uint32_t mask){ s_in.down|=mask; s_in.held|=mask; }
void host_release(uint32_t mask){ s_in.up|=mask; s_in.held&=~mask; }
void host_touch(int x,int y,bool down){
    if(down){ if(!s_in.touching) s_in.tdown=true; s_in.touching=true; s_in.tx=x; s_in.ty=y; }
    else { if(s_in.touching) s_in.tup=true; s_in.touching=false; }
}
void host_advance(uint32_t ms){ s_ms+=ms; }
void host_dump_ppm(Surf s,const char *path){
    FILE *f=fopen(path,"wb"); if(!f) return;
    fprintf(f,"P6\n%d %d\n255\n",s.w,SCR_H);
    for(int y=0;y<SCR_H;y++) for(int x=0;x<s.w;x++){
        uint16_t c=s.p[x*SCR_H+(SCR_H-1-y)];
        fputc(((c>>11)&31)*255/31,f); fputc(((c>>5)&63)*255/63,f); fputc((c&31)*255/31,f);
    }
    fclose(f);
}
