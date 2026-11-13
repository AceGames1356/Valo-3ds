/* Rough host timing (NOT a 3DS measurement): cost of a render frame and a sim tick. */
#include <stdio.h>
#include <time.h>
#include "../source/app.h"
#include "../source/game.h"
#include "../source/render3d.h"
#include "../source/hud.h"
#include "../source/settings.h"
#include "../source/platform.h"
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e3+t.tv_nsec/1e6; }
int main(void){
    settings_defaults(); rd_init(); plat_init(); g_rng=5; game_new_match(0,4,1,0,0);
    for(int i=0;i<30*25;i++){ Intent in; memset(&in,0,sizeof in); in.abil=-1; game_tick(&in); }   /* into live round */
    Surf top=plat_top(); const Unit *v=game_view_unit(); Camera c; rd_camera_for(v,&c);
    for(int q=0;q<2;q++){
        g_set.quality=q; double t0=now(); int n=300;
        for(int i=0;i<n;i++){ c.yaw+=0.02f; rd_world(top,&c,v); rd_viewmodel(top,v,0); hud_draw(top,v,-1,0); }
        printf("render %s: %.3f ms/frame (host)\n",q?"FAST(200 col)":"HIGH(400 col)",(now()-t0)/n);
    }
    double t0=now(); int n=3000;
    for(int i=0;i<n;i++){ Intent in; memset(&in,0,sizeof in); in.abil=-1; game_tick(&in); if(g_m.phase==PH_MATCHOVER) game_new_match(0,4,1,0,0); }
    printf("sim tick (10 units, bots): %.4f ms/tick (host)\n",(now()-t0)/n);
    return 0;
}
