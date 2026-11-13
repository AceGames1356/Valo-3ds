/* Entry point (shared by the 3DS build and the host smoke test).
   Fixed 30 Hz simulation with real-time catch-up; input edges are latched
   across 60 Hz polls so no button press or touch is ever lost. */
#include "platform.h"
#include "app.h"
#include <string.h>

int main(int argc,char **argv){
    (void)argc; (void)argv;
    if(!plat_init()) return 1;
    app_init();

    Input raw, acc; memset(&acc,0,sizeof acc);
    uint32_t last=plat_ms(), accMs=0, fpsT=last; int frames=0;

    while(plat_running()){
        plat_poll(&raw);
        acc.held=raw.held; acc.down|=raw.down; acc.up|=raw.up;
        acc.cpx=raw.cpx; acc.cpy=raw.cpy; acc.csx=raw.csx; acc.csy=raw.csy;
        acc.touching=raw.touching; acc.tx=raw.tx; acc.ty=raw.ty; acc.new3ds=raw.new3ds;
        acc.tdown|=raw.tdown; acc.tup|=raw.tup; acc.tdx+=raw.tdx; acc.tdy+=raw.tdy;

        uint32_t now=plat_ms(); accMs+=now-last; last=now;
        if(accMs>100) accMs=100;                      /* long stall (HOME menu): do not fast-forward */
        if(accMs>=33){
            int ticks=(int)(accMs/33); if(ticks>2) ticks=2;
            accMs-=(uint32_t)ticks*33;
            Input in=acc;
            app_frame(&in,ticks);
            plat_present();
            acc.down=acc.up=0; acc.tdown=acc.tup=false; acc.tdx=acc.tdy=0;
            frames++;
        } else {
            plat_wait_frame();
        }
        if(now-fpsT>=1000){ app_set_fps(frames*1000.0f/(float)(now-fpsT)); frames=0; fpsT=now; }
    }
    app_shutdown();
    plat_exit();
    return 0;
}
