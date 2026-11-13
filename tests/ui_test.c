/* Drives the real app (touch UI + controls) with synthetic input and checks results. */
#include <stdio.h>
#include <stdlib.h>
#include "../source/app.h"
#include "../source/game.h"
#include "../source/settings.h"
#include "../source/platform.h"
void host_dump_ppm(Surf s,const char *path);

static int fails; static int shot;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL line %d: ",__LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

static Input cur;
static void frame(int ticks){ app_frame(&cur,ticks); cur.down=cur.up=0; cur.tdown=cur.tup=false; cur.tdx=cur.tdy=0; }
static int tapTicks=0;
static void tap(int x,int y){
    cur.touching=true; cur.tdown=true; cur.tx=x; cur.ty=y; frame(tapTicks);
    cur.touching=false; cur.tup=true; frame(tapTicks);
}
static void snap(const char *name){
    char p[96]; snprintf(p,sizeof p,"/tmp/ui_%02d_%s_top.ppm",shot,name); host_dump_ppm(plat_top(),p);
    snprintf(p,sizeof p,"/tmp/ui_%02d_%s_bot.ppm",shot,name); host_dump_ppm(plat_bottom(),p); shot++;
}
static void press(int b){ cur.down|=BM(b); cur.held|=BM(b); }
static void release(int b){ cur.up|=BM(b); cur.held&=~BM(b); }

int main(void){
    plat_init(); remove("/tmp/v3ds_settings.bin"); app_init();
    memset(&cur,0,sizeof cur); cur.new3ds=true;
    frame(0); snap("title");
    tap(160,50);                       /* PLAY OFFLINE MATCH */
    frame(0); snap("setup");
    tap(300,36);                       /* map > (second map) */
    tap(300,60);                       /* side > */
    tap(300,108);                      /* bots > hard */
    frame(0); snap("setup2");
    tap(230,135); frame(0); snap("agents");   /* agent screen */
    tap(238,62);                       /* pick VEIL (index 1) */
    frame(0);
    tap(160,210);                      /* done */
    CHECK(g_set.lastAgent==0,"agent not saved until start");
    tap(224,204);                      /* START MATCH */
    CHECK(g_m.round==1,"match did not start (round %d)",g_m.round);
    CHECK(g_m.mapId==1,"map select failed (%d)",g_m.mapId);
    CHECK(g_m.u[0].agent==1,"agent select failed (%d)",g_m.u[0].agent);
    CHECK(g_m.diff==2,"difficulty select failed (%d)",g_m.diff);
    CHECK(g_m.phase==PH_BUY,"expected buy phase");
    g_m.u[0].credits=5000;
    for(int i=0;i<3;i++) frame(1);
    snap("buy_side");
    tap(157,32);                       /* RIFLE tab */
    frame(1); snap("buy_rifles");
    tap(160,167);                      /* 4th rifle = VANDAL */
    CHECK(g_m.u[0].wp[1]==W_VANDAL,"buy failed, primary %d msg '%s'",g_m.u[0].wp[1],g_m.buyMsg);
    CHECK(g_m.u[0].credits==5000-2900,"credits %d",g_m.u[0].credits);
    tap(160,62);                       /* switch to BULLDOG: should refund Vandal */
    CHECK(g_m.u[0].wp[1]==W_BULLDOG&&g_m.u[0].credits==5000-2050,"refund swap failed (%d,%d)",g_m.u[0].wp[1],g_m.u[0].credits);
    tap(160,167);                      /* back to vandal */
    tap(310,32);                       /* ARMOR tab (x 4+6*44=268..311) */
    tap(160,62+0);                     /* light shield */
    CHECK(g_m.u[0].armor==25,"armor %d",g_m.u[0].armor);
    frame(1); snap("buy_armor");
    tap(81,224);                       /* READY */
    for(int i=0;i<4;i++) frame(1);
    CHECK(g_m.phase==PH_LIVE,"not live after ready (phase %d)",g_m.phase);
    /* movement + shooting via buttons */
    float x0=g_m.u[0].x,y0=g_m.u[0].y; int ammo0=g_m.u[0].ammo[1];
    cur.cpy=150; press(B_R);
    for(int i=0;i<30;i++) frame(1);
    release(B_R); cur.cpy=0;
    float moved=hypotf(g_m.u[0].x-x0,g_m.u[0].y-y0);
    CHECK(moved>0.8f||g_m.u[0].alive==false,"player did not move with circle pad (%.2f)",moved);
    CHECK(g_m.u[0].ammo[1]<ammo0||!g_m.u[0].alive,"fire button did not shoot (%d->%d)",ammo0,g_m.u[0].ammo[1]);
    /* look */
    float yaw0=g_m.u[0].yaw; cur.csx=156; for(int i=0;i<10;i++) frame(1); cur.csx=0;
    CHECK(fabsf(wrapang(g_m.u[0].yaw-yaw0))>0.2f,"c-stick look failed");
    yaw0=g_m.u[0].yaw; press(B_DRIGHT); for(int i=0;i<10;i++) frame(1); release(B_DRIGHT);
    CHECK(fabsf(wrapang(g_m.u[0].yaw-yaw0))>0.1f,"d-pad look failed");
    /* touch look: drag on minimap */
    yaw0=g_m.u[0].yaw; cur.touching=true; cur.tdown=true; cur.tx=60; cur.ty=100; frame(1);
    cur.tdx=30; cur.tdy=0; frame(1); cur.touching=false; cur.tup=true; frame(1);
    CHECK(fabsf(wrapang(g_m.u[0].yaw-yaw0))>0.05f,"touch look failed");
    /* ability via touch + via button */
    for(int i=0;i<3;i++) frame(1);
    g_m.u[0].alive=true; g_m.u[0].hp=100;
    float cd0=g_m.u[0].cd[0]; tapTicks=1; tap(230,39);
    CHECK(g_m.u[0].cd[0]>cd0,"touch ability 1 did not fire (cd %.1f)",g_m.u[0].cd[0]);
    press(B_ZL); frame(1); release(B_ZL);
    CHECK(g_m.u[0].cd[1]>0,"ZL ability 2 did not fire");
    frame(1); snap("live");
    tap(230,175);                      /* team command: FOLLOW (index1 at x=212..262,y=160) */
    tap(212+25,172); CHECK(g_m.cmd==CMD_FOLLOW,"command not set (%d)",g_m.cmd);
    /* pause */
    press(B_START); frame(1); release(B_START); frame(0);
    snap("pause");
    uint32_t t0=g_m.tick; for(int i=0;i<5;i++) frame(2);
    CHECK(g_m.tick==t0,"sim advanced while paused");
    tap(160,125);                      /* CONTROLS from pause */
    snap("controls");
    tap(220,57);                       /* rebind FIRE */
    press(B_X); frame(0); release(B_X);
    CHECK(g_set.bind[ACT_FIRE]==B_X,"rebind failed (%d)",g_set.bind[ACT_FIRE]);
    tapTicks=0; tap(160,33);                   /* preset 1 (OLD 3DS) */
    CHECK(g_set.preset==1&&g_set.bind[ACT_FIRE]==B_R,"preset failed");
    tap(160,226);                      /* back */
    tap(160,94);                       /* settings from pause */
    snap("settings");
    int s0=g_set.sensStick; tap(298,31); CHECK(g_set.sensStick==s0+1,"sens arrow failed");
    tap(160,222);                      /* back (BACK at 166,208,146,26) */
    tap(240,221);
    tap(160,50);                       /* resume */
    for(int i=0;i<3;i++) frame(1);
    /* persistence */
    Settings keep=g_set; memset(&g_set,0,sizeof g_set); CHECK(settings_load(),"settings did not load back");
    CHECK(g_set.sensStick==keep.sensStick&&g_set.preset==keep.preset,"settings mismatch after reload");
    /* finish a match quickly through app: let bots+autopilot-less player run */
    g_m.score[0]=3; g_m.score[1]=0; g_m.u[1].alive=g_m.u[2].alive=g_m.u[3].alive=g_m.u[4].alive=g_m.u[0].alive=true;
    for(int i=5;i<10;i++) g_m.u[i].alive=false;
    for(int i=0;i<400&&g_m.phase!=PH_MATCHOVER;i++) frame(1);
    snap("endround");
    for(int i=0;i<300;i++) frame(1);
    CHECK(g_m.phase==PH_MATCHOVER,"match did not end (phase %d)",g_m.phase);
    frame(0); snap("result");
    tap(160,102);                      /* rematch */
    CHECK(g_m.round==1&&g_m.score[0]==0,"rematch failed");
    for(int i=0;i<10;i++) frame(1);
    printf(fails?"UI TEST FAILED (%d)\n":"UI TEST OK\n",fails);
    return fails?1:0;
}
