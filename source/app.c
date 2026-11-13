/* Application layer: screens, touch UI, input -> intent mapping. */
#include "app.h"
#include "game.h"
#include "gfx.h"
#include "hud.h"
#include "render3d.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>

typedef enum { SC_TITLE,SC_SETUP,SC_AGENTS,SC_SETTINGS,SC_CONTROLS,SC_HOWTO,SC_PLAY,SC_PAUSE,SC_RESULT } Screen;

#define C_BG      RGB(14,17,26)
#define C_PANEL   RGB(28,33,48)
#define C_BTN     RGB(46,54,80)
#define C_RED     RGB(236,72,86)
#define C_TXT     RGB(240,244,250)
#define C_DIM     RGB(140,150,172)
#define C_GOLD    RGB(255,210,90)
#define C_GREEN   RGB(90,220,130)
#define C_ALLY    RGB(100,205,225)
#define C_ENEMY   RGB(240,95,95)

static Screen s_scr=SC_TITLE, s_ret=SC_TITLE;
static const Input *I;
static Surf B,T;
static float s_fps, s_time, s_bob;
static int  s_map, s_side, s_len, s_diff, s_agent, s_buyTab, s_buySel=-1, s_rebind=-1, s_howPage, s_confirmQuit;
static bool s_lookTouch;
static int  s_uiAbil=-1; static bool s_uiSwap;
static int  s_loadedMap=-1;
static float s_yawAttract;

static const char *MAPN[M_COUNT];
static const char *SIDEN[3]={"ATTACK","DEFEND","RANDOM"};
static const char *LENN[3]={"QUICK  FIRST TO 4","STANDARD  FIRST TO 7","FULL  FIRST TO 13"};
static const int   LENT[3]={4,7,13};
static const char *DIFFN[3]={"EASY","NORMAL","HARD"};

/* ------------------------------------------------------------------ UI primitives */
static bool in_rect(int x,int y,int w,int h){ return I->tx>=x&&I->tx<x+w&&I->ty>=y&&I->ty<y+h; }
static bool btn(int x,int y,int w,int h,const char *label,uint16_t col,bool enabled){
    bool hot=enabled&&I->touching&&in_rect(x,y,w,h);
    uint16_t c=enabled?(hot?gx_blend(col,RGB(255,255,255),10):col):RGB(36,40,52);
    gx_rect(B,x,y,w,h,c); gx_frame(B,x,y,w,h,enabled?RGB(120,130,160):RGB(60,64,78));
    int sc=(gx_textw(label,2)<=w-8&&h>=22)?2:1;
    gx_textc(B,x+w/2,y+(h-7*sc)/2,label,enabled?C_TXT:RGB(90,96,112),sc);
    return enabled&&I->tdown&&in_rect(x,y,w,h);
}
static void title_bar(const char *t){
    gx_rect(B,0,0,BOT_W,18,C_PANEL); gx_text(B,8,5,t,C_GOLD,1); gx_rect(B,0,18,BOT_W,1,C_RED);
}
static int cycle(int v,int n,int dir){ return (v+dir+n)%n; }
/* row with < value > arrows. returns -1,0,+1 */
static int arrow_row(int y,const char *label,const char *value){
    int r=0;
    gx_text(B,8,y+6,label,C_DIM,1);
    if(btn(150,y,22,18,"<",C_BTN,true)) r=-1;
    gx_rect(B,174,y,112,18,C_PANEL); gx_textc(B,230,y+5,value,C_TXT,1);
    if(btn(288,y,22,18,">",C_BTN,true)) r=1;
    return r;
}
static void wrap_text(Surf s,int x,int y,int maxChars,const char *t,uint16_t c,int lh){
    char line[80]; int n=0; const char *p=t;
    while(*p){
        const char *w=p; int wl=0; while(w[wl]&&w[wl]!=' ') wl++;
        if(n+wl>maxChars&&n>0){ line[n]=0; gx_text(s,x,y,line,c,1); y+=lh; n=0; }
        for(int i=0;i<wl&&n<78;i++) line[n++]=w[i];
        if(w[wl]==' '){ if(n<78) line[n++]=' '; p=w+wl+1; } else p=w+wl;
    }
    if(n){ line[n]=0; gx_text(s,x,y,line,c,1); }
}

/* ------------------------------------------------------------------ map drawing */
static void draw_map(Surf s,int x0,int y0,int sc,const Unit *view){
    static const uint16_t SITE[3]={RGB(120,70,30),RGB(30,90,110),RGB(40,100,60)};
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++){
        int c=g_map.cell[y][x]; uint16_t col;
        if(mp_is_wall(c)) col=RGB(62,70,92); else if(mp_is_low(c)) col=RGB(120,96,60); else {
            int z=g_map.zone[y][x]; col=RGB(24,28,40);
            if(z>=1&&z<=3) col=SITE[z-1]; else if(z==4) col=RGB(80,34,38); else if(z==5) col=RGB(34,48,84);
        }
        gx_rect(s,x0+x*sc,y0+y*sc,sc,sc,col);
    }
    for(int i=0;i<g_map.siteCount;i++){ char l[2]={(char)('A'+i),0}; gx_textc(s,x0+(int)(g_map.site[i].cx*sc),y0+(int)(g_map.site[i].cy*sc)-3,l,C_TXT,1); }
    for(int i=0;i<g_map.tpCount;i+=2){ gx_disc(s,x0+(int)((g_map.tp[i].x+0.5f)*sc),y0+(int)((g_map.tp[i].y+0.5f)*sc),sc>2?2:1,RGB(170,110,255));
        gx_disc(s,x0+(int)((g_map.tp[i].dx+0.5f)*sc),y0+(int)((g_map.tp[i].dy+0.5f)*sc),sc>2?2:1,RGB(170,110,255)); }
    if(!view) return;
    for(int i=0;i<MAX_ZONES;i++){ const Zone *z=&g_m.zones[i]; if(!z->active) continue;
        int px=x0+(int)(z->x*sc), py=y0+(int)(z->y*sc);
        if(z->type==Z_SMOKE) gx_disc(s,px,py,(int)(z->r*sc),RGB(150,154,166));
        else if(z->type==Z_FIELD) gx_disc(s,px,py,(int)(z->r*sc),RGB(100,60,150));
        else if(z->team==view->team&&(z->type==Z_TURRET||z->type==Z_TRAP)) gx_rect(s,px-1,py-1,3,3,RGB(90,240,200)); }
    if(g_m.spike==SP_DROPPED||g_m.spike==SP_PLANTED){
        int px=x0+(int)(g_m.spikeX*sc), py=y0+(int)(g_m.spikeY*sc);
        gx_disc(s,px,py,3,((g_m.tick/6)&1)?RGB(255,70,60):RGB(255,210,90)); }
    for(int i=0;i<NUNITS;i++){ const Unit *u=&g_m.u[i]; if(!u->alive) continue;
        bool ally=(u->squad==view->squad);
        if(!ally&&!u->spotted) continue;
        int px=x0+(int)(u->x*sc), py=y0+(int)(u->y*sc);
        uint16_t col=ally?RGB(90,210,235):RGB(250,90,90);
        if(u==view) col=RGB(255,240,90);
        gx_disc(s,px,py,sc>2?2:1,col);
        gx_line(s,px,py,px+(int)(cosf(u->yaw)*5),py+(int)(sinf(u->yaw)*5),col);
        if(i==g_m.carrier&&g_m.spike==SP_CARRIED&&ally) gx_rect(s,px-1,py-4,3,2,RGB(255,70,60));
    }
}

/* ------------------------------------------------------------------ input -> intent */
static bool act_held(int act){ int b=g_set.bind[act]; if(b<0) return false; if(g_set.dpadLook&&b>=B_DUP) return false; return (I->held>>b)&1; }
static bool act_down(int act){ int b=g_set.bind[act]; if(b<0) return false; if(g_set.dpadLook&&b>=B_DUP) return false; return (I->down>>b)&1; }
static float dz(int v,int d){ if(v>-d&&v<d) return 0; float f=(v>0?v-d:v+d)/(float)(150-d); return clampf(f,-1,1); }

static void apply_look(bool first){
    const Unit *p=&g_m.u[0]; if(!p->alive) return;
    float rate=0.5f+g_set.sensStick*0.35f;
    float zoom=p->scoped?unit_weapon(p)->zoom:1.0f;
    float dyaw=dz(I->csx,12)*rate*DT, dpit=dz(I->csy,12)*rate*DT*0.8f;
    if(g_set.dpadLook){
        if(I->held&BM(B_DLEFT)) dyaw-=rate*0.8f*DT;
        if(I->held&BM(B_DRIGHT)) dyaw+=rate*0.8f*DT;
        if(I->held&BM(B_DUP)) dpit+=rate*0.6f*DT;
        if(I->held&BM(B_DDOWN)) dpit-=rate*0.6f*DT;
    }
    if(first&&g_set.touchLook&&s_lookTouch&&I->touching){
        float k=0.002f+0.0012f*g_set.sensTouch;
        dyaw+=I->tdx*k; dpit+=-I->tdy*k*0.8f;
    }
    if(g_set.invertY) dpit=-dpit;
    game_look(dyaw/zoom,dpit/zoom);
}

static Intent build_intent(bool first){
    Intent in; memset(&in,0,sizeof in); in.abil=-1;
    in.fwd=dz(I->cpy,14); in.strafe=dz(I->cpx,14);
    in.fire=act_held(ACT_FIRE); in.jump=act_held(ACT_JUMP); in.use=act_held(ACT_USE); in.alt=act_held(ACT_ALT);
    if(first){
        in.reload=act_down(ACT_RELOAD); in.swap=act_down(ACT_SWAP)||s_uiSwap;
        if(act_down(ACT_ABIL1)) in.abil=0; else if(act_down(ACT_ABIL2)) in.abil=1; else if(act_down(ACT_ULT)) in.abil=2;
        if(s_uiAbil>=0) in.abil=s_uiAbil;
    }
    return in;
}

/* ------------------------------------------------------------------ screens: title / setup / agents */
static void load_menu_map(int m){ if(s_loadedMap!=m){ mp_load(m); s_loadedMap=m; } }

static void top_attract(void){
    load_menu_map(s_map);
    Camera c; s_yawAttract+=0.012f;
    int sidx=(int)(s_time*0.07f)%(g_map.siteCount>0?g_map.siteCount:1);
    c.x=g_map.site[sidx].cx; c.y=g_map.site[sidx].cy; c.eye=EYE_H; c.yaw=s_yawAttract; c.pitch=0; c.zoom=1.0f;
    rd_world(T,&c,NULL);
    gx_rect_a(T,0,0,TOP_W,SCR_H,RGB(6,8,14),14);
}
static void title_top(void){
    top_attract();
    gx_rect_a(T,0,64,TOP_W,84,RGB(6,8,14),24);
    gx_textc(T,TOP_W/2+2,78,"VALORANT 3DS",RGB(0,0,0),4); gx_textc(T,TOP_W/2,76,"VALORANT 3DS",C_RED,4);
    gx_textc(T,TOP_W/2,112,"OFFLINE 5V5 TACTICAL SHOOTER",C_TXT,1);
    gx_textc(T,TOP_W/2,126,"UNOFFICIAL FAN PROJECT - ORIGINAL ASSETS",C_DIM,1);
    { char b[48]; snprintf(b,sizeof b,"V1.0   MAP: %s",MAPN[s_map]); gx_textc(T,TOP_W/2,SCR_H-14,b,C_DIM,1); }
}
static void screen_title(void){
    title_top();
    gx_clear(B,C_BG); title_bar("MAIN MENU");
    if(btn(40,32,240,36,"PLAY OFFLINE MATCH",C_RED,true)){ s_scr=SC_SETUP; }
    if(btn(40,74,240,30,"SETTINGS",C_BTN,true)){ s_ret=SC_TITLE; s_scr=SC_SETTINGS; }
    if(btn(40,110,240,30,"CONTROLS",C_BTN,true)){ s_ret=SC_TITLE; s_scr=SC_CONTROLS; }
    if(btn(40,146,240,30,"HOW TO PLAY",C_BTN,true)){ s_howPage=0; s_scr=SC_HOWTO; }
    if(btn(40,182,240,30,"QUIT",C_BTN,true)){ plat_request_quit(); }
    gx_textc(B,BOT_W/2,222,"4 MAPS  17 WEAPONS  4 AGENTS",C_DIM,1);
}

static void screen_setup(void){
    load_menu_map(s_map);
    gx_clear(T,C_BG);
    draw_map(T,8,24,4,NULL);
    gx_text(T,208,24,MAPN[s_map],C_GOLD,2);
    gx_text(T,208,44,g_maps[s_map].inspired,C_DIM,1);
    wrap_text(T,208,60,32,g_maps[s_map].blurb,C_TXT,10);
    char b[48]; snprintf(b,sizeof b,"BOMB SITES: %d",g_map.siteCount); gx_text(T,208,100,b,C_TXT,1);
    gx_text(T,208,114,g_map.tpCount?"PORTALS: YES":"PORTALS: NO",C_TXT,1);
    gx_text(T,208,140,"YOUR AGENT",C_DIM,1); gx_text(T,208,152,g_agents[s_agent].name,g_agents[s_agent].color,2);
    gx_text(T,208,170,g_agents[s_agent].role,C_DIM,1);
    gx_text(T,208,196,"ORANGE=A CYAN=B GREEN=C",C_DIM,1);
    gx_text(T,208,208,"RED=ATTACK SPAWN BLUE=DEFENSE",C_DIM,1);

    gx_clear(B,C_BG); title_bar("MATCH SETUP");
    int r;
    if((r=arrow_row(28,"MAP",MAPN[s_map]))) s_map=cycle(s_map,M_COUNT,r);
    if((r=arrow_row(52,"SIDE",SIDEN[s_side]))) s_side=cycle(s_side,3,r);
    if((r=arrow_row(76,"LENGTH",LENN[s_len]))) s_len=cycle(s_len,3,r);
    if((r=arrow_row(100,"BOTS",DIFFN[s_diff]))) s_diff=cycle(s_diff,3,r);
    gx_text(B,8,130,"AGENT",C_DIM,1);
    if(btn(150,124,160,22,g_agents[s_agent].name,C_BTN,true)) s_scr=SC_AGENTS;
    gx_textc(B,BOT_W/2,158,"YOU + 4 BOT ALLIES VS 5 BOTS",C_DIM,1);
    gx_textc(B,BOT_W/2,170,"SIDES SWITCH AT HALF TIME",C_DIM,1);
    if(btn(8,184,120,40,"BACK",C_BTN,true)){ s_scr=SC_TITLE; g_set.lastMap=(uint8_t)s_map; g_set.lastSide=(uint8_t)s_side; g_set.lastLen=(uint8_t)s_len; g_set.lastDiff=(uint8_t)s_diff; g_set.lastAgent=(uint8_t)s_agent; settings_save(); }
    if(btn(136,184,176,40,"START MATCH",C_RED,true)){
        g_set.lastMap=(uint8_t)s_map; g_set.lastSide=(uint8_t)s_side; g_set.lastLen=(uint8_t)s_len; g_set.lastDiff=(uint8_t)s_diff; g_set.lastAgent=(uint8_t)s_agent; settings_save();
        g_rng^=(uint32_t)plat_ms()*2654435761u; if(!g_rng) g_rng=1;
        game_new_match(s_map,LENT[s_len],s_diff,s_side,s_agent);
        s_loadedMap=-1; s_buyTab=0; s_buySel=-1; s_bob=0; s_scr=SC_PLAY;
    }
}

static void screen_agents(void){
    gx_clear(T,C_BG);
    const Agent *a=&g_agents[s_agent];
    gx_text(T,10,10,a->name,a->color,3); gx_text(T,10,38,a->role,C_DIM,1);
    wrap_text(T,10,52,62,a->blurb,C_TXT,10);
    for(int i=0;i<3;i++){
        int y=78+i*52; const Ability *ab=&a->ab[i];
        gx_rect(T,10,y,380,46,C_PANEL); gx_rect(T,10,y,4,46,a->color);
        char b[64]; snprintf(b,sizeof b,"%s%s",ab->name,i==2?"  (ULTIMATE)":"");
        gx_text(T,22,y+5,b,C_TXT,1);
        if(i<2) snprintf(b,sizeof b,"COOLDOWN %.0fS",ab->cd); else snprintf(b,sizeof b,"COSTS %d ULT POINTS (KILLS + ROUNDS)",ab->cost);
        gx_textr(T,382,y+5,b,C_GOLD,1);
        wrap_text(T,22,y+19,60,ab->desc,C_DIM,10);
    }
    gx_clear(B,C_BG); title_bar("SELECT AGENT");
    for(int i=0;i<A_COUNT;i++){
        int x=8+(i%2)*156, y=28+(i/2)*76;
        uint16_t col=(i==s_agent)?gx_blend(C_BTN,g_agents[i].color,14):C_BTN;
        if(btn(x,y,148,68,"",col,true)) s_agent=i;
        gx_textc(B,x+74,y+14,g_agents[i].name,g_agents[i].color,2);
        gx_textc(B,x+74,y+38,g_agents[i].role,C_TXT,1);
        if(i==s_agent) gx_textc(B,x+74,y+52,"SELECTED",C_GREEN,1);
    }
    if(btn(8,190,304,40,"DONE",C_RED,true)) s_scr=SC_SETUP;
}

/* ------------------------------------------------------------------ settings / controls / how-to */
static void screen_settings(void){
    gx_clear(T,C_BG);
    gx_text(T,12,12,"SETTINGS",C_GOLD,2);
    wrap_text(T,12,40,62,"LOOK SPEED applies to the C-Stick and D-Pad. TOUCH LOOK lets you drag on the minimap area during a match to aim. D-PAD LOOK turns the D-Pad into look controls (disable it if you bind actions to the D-Pad).",C_TXT,11);
    wrap_text(T,12,100,62,"AIM ASSIST slightly widens enemy hitboxes for your shots. QUALITY FAST renders 200 columns and doubles them, which helps on Old Nintendo 3DS hardware.",C_DIM,11);
    wrap_text(T,12,140,62,"Settings are saved to the SD card (3ds/valorant3ds folder) and restored on launch.",C_DIM,11);
    gx_clear(B,C_BG); title_bar("SETTINGS");
    char v[24]; int r, y=22;
    snprintf(v,sizeof v,"%d",g_set.sensStick); if((r=arrow_row(y,"LOOK SPEED",v))) g_set.sensStick=(uint8_t)clampi(g_set.sensStick+r,1,10);
    y+=20;
    snprintf(v,sizeof v,"%d",g_set.sensTouch); if((r=arrow_row(y,"TOUCH SPEED",v))) g_set.sensTouch=(uint8_t)clampi(g_set.sensTouch+r,1,10);
    y+=20;
    if((r=arrow_row(y,"INVERT Y",g_set.invertY?"ON":"OFF"))) g_set.invertY^=1;
    y+=20;
    if((r=arrow_row(y,"D-PAD LOOK",g_set.dpadLook?"ON":"OFF"))) g_set.dpadLook^=1;
    y+=20;
    if((r=arrow_row(y,"TOUCH LOOK",g_set.touchLook?"ON":"OFF"))) g_set.touchLook^=1;
    y+=20;
    if((r=arrow_row(y,"AIM ASSIST",g_set.aimAssist?"ON":"OFF"))) g_set.aimAssist^=1;
    y+=20;
    static const char *FOVN[3]={"65","75","85"};
    if((r=arrow_row(y,"FIELD OF VIEW",FOVN[g_set.fov]))) g_set.fov=(uint8_t)cycle(g_set.fov,3,r);
    y+=20;
    if((r=arrow_row(y,"QUALITY",g_set.quality?"FAST":"HIGH"))) g_set.quality^=1;
    y+=20;
    if((r=arrow_row(y,"SHOW FPS",g_set.showFps?"ON":"OFF"))) g_set.showFps^=1;
    y+=20;
    if(btn(8,208,150,26,"RESET DEFAULTS",C_BTN,true)){ uint8_t lm=g_set.lastMap,ll=g_set.lastLen,ld=g_set.lastDiff,ls=g_set.lastSide,la=g_set.lastAgent; settings_defaults(); g_set.lastMap=lm; g_set.lastLen=ll; g_set.lastDiff=ld; g_set.lastSide=ls; g_set.lastAgent=la; }
    if(btn(166,208,146,26,"BACK",C_RED,true)){ settings_save(); s_scr=s_ret; }
}

static void screen_controls(void){
    gx_clear(T,C_BG);
    gx_text(T,12,10,"CONTROLS",C_GOLD,2);
    gx_text(T,12,34,"MOVE ........... CIRCLE PAD",C_TXT,1);
    gx_text(T,12,46,"AIM ............ C-STICK / TOUCH DRAG",C_TXT,1);
    gx_text(T,12,58,"FIRE ........... R",C_TXT,1);
    gx_text(T,12,70,"AIM / SCOPE .... SELECT (SNIPERS)",C_TXT,1);
    gx_text(T,12,82,"JUMP ........... A",C_TXT,1);
    gx_text(T,12,94,"RELOAD ......... B",C_TXT,1);
    gx_text(T,12,106,"USE / PLANT .... Y",C_TXT,1);
    gx_text(T,12,118,"SWAP WEAPON .... X",C_TXT,1);
    gx_text(T,12,130,"ABILITY 1 ...... L",C_TXT,1);
    gx_text(T,12,142,"ABILITY 2 ...... ZL",C_TXT,1);
    gx_text(T,12,154,"ULTIMATE ....... ZR",C_TXT,1);
    gx_text(T,12,166,"PAUSE .......... START",C_TXT,1);
    for(int i=0;i<ACT_COUNT;i++){
        char b[64]; snprintf(b,sizeof b,"%-13s ... %s",act_name(i),g_set.bind[i]<0?"TOUCH SCREEN ONLY":btn_name(g_set.bind[i]));
        gx_text(T,12,76+i*12,b,(g_set.bind[i]>=B_DUP&&g_set.dpadLook)?RGB(255,120,90):C_TXT,1);
    }
    wrap_text(T,12,190,62,"Old 3DS: use the on-screen ability and weapon buttons. Touch the minimap and drag to aim. Every action can be rebound below.",C_DIM,10);
    gx_clear(B,C_BG); title_bar("BUTTON LAYOUT");
    for(int p=0;p<PRESET_COUNT;p++){
        char b[20]; snprintf(b,sizeof b,"%s",preset_name(p));
        if(btn(6+p*104,22,100,22,b,g_set.preset==p?C_RED:C_BTN,true)){ settings_apply_preset(p); s_rebind=-1; }
    }
    for(int i=0;i<ACT_COUNT;i++){
        int y=50+i*17; bool waiting=(s_rebind==i);
        gx_rect(B,6,y,308,15,waiting?RGB(90,70,20):C_PANEL);
        gx_text(B,12,y+4,act_name(i),C_TXT,1);
        const char *vn=waiting?"PRESS A BUTTON":btn_name(g_set.bind[i]);
        if(btn(168,y,106,15,vn,waiting?RGB(120,90,20):C_BTN,true)){ s_rebind=waiting?-1:i; }
        if(btn(278,y,36,15,"CLEAR",C_BTN,true)){ g_set.bind[i]=-1; s_rebind=-1; g_set.preset=0; }
    }
    if(s_rebind>=0){
        for(int b=0;b<B_COUNT;b++) if((I->down>>b)&1){
            if(b==B_START) break;
            g_set.bind[s_rebind]=(int8_t)b; s_rebind=-1; break; }
    }
    if(g_set.dpadLook) gx_textc(B,BOT_W/2,206,"D-PAD LOOK IS ON: D-PAD BINDS IGNORED",RGB(255,150,90),1);
    else gx_textc(B,BOT_W/2,206,"D-PAD LOOK IS OFF: D-PAD CAN BE BOUND",C_DIM,1);
    if(btn(8,216,304,20,"BACK",C_RED,true)){ settings_save(); s_rebind=-1; s_scr=s_ret; }
}

static const char *HOW[][11]={
 {"OBJECTIVE","","ATTACKERS: carry the SPIKE to a bomb site, plant it (hold","USE for 4s) and protect it until it detonates (45s).","DEFENDERS: stop the plant, or defuse the spike (hold USE","for 7s, progress checkpoints at half).","","A round ends when a team is eliminated, the time runs out","(defenders win), or the spike detonates / is defused.","First team to the target score wins. Sides switch at half.",""},
 {"CONTROLS","","CIRCLE PAD  move       C-STICK / D-PAD / TOUCH  look","R fire   A jump   B reload   Y use (plant/defuse)","X swap weapon  L ability 1  ZL ability 2  ZR ultimate","SELECT walk (slow, quiet) / scope with sniper rifles","START pause.  Old 3DS: use touch buttons for abilities.","Drag on the minimap to look around (touch look).","Everything is rebindable in CONTROLS.","","" },
 {"BUY PHASE & ECONOMY","","Round start: 20s buy phase, you cannot leave spawn.","Tap a category tab, then tap a weapon to buy it.","Armor: light 400 (25) / heavy 1000 (50). It absorbs 2/3","of incoming damage until depleted.","Win +3000. Loss +1900 (+500 per loss streak, max 2900).","Kill +200. Plant +300 for losing attackers. Max 9000.","If you survive you keep your gear for the next round.","Buying a replacement in the same phase refunds the old gun.",""},
 {"AGENTS & ABILITIES","","EMBER duelist: dash and flash.  VEIL controller: smoke","and slowing field.  BASTION sentinel: tripwire, turret,","team heal.  RADAR initiator: scan, stun, global reveal.","Basic abilities use cooldowns. The ultimate needs points","from kills, plants/defuses and finished rounds.","","TEAM COMMANDS (bottom screen): FOLLOW makes bots stay near","you, HOLD stops them, A/B/C sends them to that site.","AUTO lets them follow their own plan."},
 {"TIPS","","Headshots deal far more damage. Aim high.","Moving makes you inaccurate. Stop to shoot, or WALK.","Low crates can be jumped over, and you can shoot over them.","Smokes block vision but not bullets.","Listen: running and gunfire can be heard by bots too.","Bots react to sound, so quiet-walk when flanking.","Play with FOLLOW if you want your team to stay close.","Portals (Bind-style map) rotate you across the map instantly.","Difficulty changes bot reaction time and aim accuracy."}
};
#define HOW_PAGES 5
static void screen_howto(void){
    gx_clear(T,C_BG);
    gx_text(T,12,10,HOW[s_howPage][0],C_GOLD,2);
    for(int i=1;i<11;i++) gx_text(T,12,34+(i-1)*17,HOW[s_howPage][i],C_TXT,1);
    char b[24]; snprintf(b,sizeof b,"PAGE %d / %d",s_howPage+1,HOW_PAGES); gx_textr(T,TOP_W-10,SCR_H-14,b,C_DIM,1);
    gx_clear(B,C_BG); title_bar("HOW TO PLAY");
    if(btn(8,40,144,60,"< PREV",C_BTN,s_howPage>0)) s_howPage--;
    if(btn(168,40,144,60,"NEXT >",C_BTN,s_howPage<HOW_PAGES-1)) s_howPage++;
    gx_textc(B,BOT_W/2,120,HOW[s_howPage][0],C_GOLD,1);
    if(btn(8,180,304,44,"BACK",C_RED,true)) s_scr=SC_TITLE;
}

/* ------------------------------------------------------------------ in-match */
static void draw_play_top(const Unit *view){
    Camera c; rd_camera_for(view,&c);
    rd_world(T,&c,view);
    if(view->alive&&view->scoped) rd_scope(T); else if(view->alive) rd_viewmodel(T,view,s_bob);
    hud_draw(T,view,s_buySel,s_fps);
}

static void buy_item_row(int y,int item,const Unit *me){
    char b[48]; const Weapon *w=(item<W_COUNT)?&g_weapons[item]:NULL;
    int price=buy_price(item);
    bool owned=false, afford=me->credits+(w?me->refund[w->slot]:me->refund[2])>=price;
    if(w) owned=(me->wp[w->slot]==item);
    else owned=(me->armor>=(item==BUY_LIGHT?25:50));
    uint16_t col=owned?RGB(34,70,52):(afford?C_BTN:RGB(32,34,44));
    const char *nm=w?w->name:(item==BUY_LIGHT?"LIGHT SHIELD":"HEAVY SHIELD");
    if(btn(6,y,308,32,"",col,true)){
        if(w) s_buySel=item;
        if(!owned) game_buy(0,item); else snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"ALREADY OWNED");
    }
    gx_text(B,12,y+4,nm,afford||owned?C_TXT:RGB(110,116,132),2);
    snprintf(b,sizeof b,owned?"OWNED":"$%d",price); gx_textr(B,308,y+4,b,owned?C_GREEN:(afford?C_GOLD:RGB(150,90,90)),2);
    if(w){ snprintf(b,sizeof b,"DMG %d  HEAD %d  ROF %.1f  MAG %d",w->dmg*(w->pellets>1?w->pellets:1),w->head*(w->pellets>1?w->pellets:1),w->rof,w->mag); gx_text(B,12,y+21,b,C_DIM,1); }
    else gx_text(B,12,y+21,item==BUY_LIGHT?"25 ARMOR POINTS":"50 ARMOR POINTS",C_DIM,1);
}

static void play_buy_ui(const Unit *me){
    static const char *TAB[7]={"SIDE","SMG","SHOT","RIFLE","SNIPER","HEAVY","ARMOR"};
    static const int TABITEMS[7][4]={{W_CLASSIC,W_GHOST,W_SHERIFF,W_FRENZY},{W_STINGER,W_SPECTRE,-1,-1},{W_BUCKY,W_JUDGE,-1,-1},
        {W_BULLDOG,W_GUARDIAN,W_PHANTOM,W_VANDAL},{W_MARSHAL,W_OUTLAW,W_OPERATOR,-1},{W_ARES,W_ODIN,-1,-1},{BUY_LIGHT,BUY_HEAVY,-1,-1}};
    char b[48];
    gx_rect(B,0,0,BOT_W,18,C_PANEL); gx_rect(B,0,18,BOT_W,1,C_GOLD);
    snprintf(b,sizeof b,"BUY PHASE %d",(int)ceilf(g_m.phaseT)); gx_text(B,8,5,b,C_GOLD,1);
    snprintf(b,sizeof b,"CREDITS %d",me->credits); gx_textr(B,250,5,b,C_TXT,1);
    if(btn(256,1,60,16,"PAUSE",C_BTN,true)){ s_scr=SC_PAUSE; s_confirmQuit=0; }
    for(int t=0;t<7;t++) if(btn(4+t*44,22,43,20,TAB[t],t==s_buyTab?C_RED:C_BTN,true)) s_buyTab=t;
    int y=46;
    for(int k=0;k<4;k++){ int it=TABITEMS[s_buyTab][k]; if(it<0) continue; buy_item_row(y,it,me); y+=35; }
    gx_text(B,8,188,g_m.buyMsg,C_GOLD,1);
    gx_text(B,8,200,"ABILITIES ARE FREE. BOTS BUY AUTOMATICALLY.",C_DIM,1);
    if(btn(6,212,150,24,"READY",C_GREEN,true)) game_skip_buy();
    snprintf(b,sizeof b,"GUN %s",g_weapons[me->wp[1]>=0?me->wp[1]:me->wp[0]].name); gx_textr(B,314,220,b,C_DIM,1);
}

static void play_live_ui(const Unit *view){
    char b[48];
    gx_rect(B,0,0,BOT_W,18,C_PANEL); gx_rect(B,0,18,BOT_W,1,C_RED);
    snprintf(b,sizeof b,"ROUND %d    %d - %d",g_m.round,g_m.score[g_m.u[0].squad],g_m.score[1-g_m.u[0].squad]); gx_text(B,8,5,b,C_TXT,1);
    if(btn(256,1,60,16,"PAUSE",C_BTN,true)){ s_scr=SC_PAUSE; s_confirmQuit=0; }
    draw_map(B,6,26,3,view);
    gx_frame(B,5,25,146,146,RGB(80,90,120));
    if(g_set.touchLook) gx_textc(B,78,174,"DRAG MAP TO LOOK",C_DIM,1);
    const Unit *me=&g_m.u[0];
    snprintf(b,sizeof b,"CREDITS %d",me->credits); gx_text(B,6,188,b,C_GOLD,1);
    const char *sp="SPIKE: WITH ATTACKERS";
    if(g_m.spike==SP_PLANTED) { snprintf(b,sizeof b,"SPIKE PLANTED SITE %c",'A'+g_m.plantSite-1); sp=b; }
    else if(g_m.spike==SP_DROPPED) sp="SPIKE: DROPPED";
    else if(g_m.carrier==0) sp="SPIKE: YOU HAVE IT";
    gx_text(B,6,200,sp,g_m.spike==SP_PLANTED?RGB(255,120,90):C_DIM,1);
    snprintf(b,sizeof b,"ALIVE  %d vs %d",game_alive(me->team),game_alive(1-me->team)); gx_text(B,6,212,b,C_DIM,1);
    if(!me->alive){
        gx_text(B,160,32,"YOU ARE OUT",RGB(255,100,100),2);
        snprintf(b,sizeof b,"WATCHING %s",view->name); gx_text(B,160,54,b,C_TXT,1);
        if(btn(160,70,152,30,"NEXT PLAYER",C_BTN,true)){
            int cur=g_m.spec; if(cur<0) cur=0;
            for(int k=1;k<=NUNITS;k++){ int j=(cur+k)%NUNITS; if(g_m.u[j].alive&&g_m.u[j].squad==0){ g_m.spec=j; break; } }
        }
        return;
    }
    for(int i=0;i<3;i++){
        const Ability *a=&g_agents[me->agent].ab[i];
        bool ready=(i<2)?me->cd[i]<=0:me->ult>=a->cost;
        char l[40]; int act=(i==0)?ACT_ABIL1:((i==1)?ACT_ABIL2:ACT_ULT);
        const char *key=g_set.bind[act]>=0?btn_name(g_set.bind[act]):"TOUCH";
        if(i<2) snprintf(l,sizeof l,"%s  %s%s",key,a->name,ready?"":" ...");
        else snprintf(l,sizeof l,"%s  %s %d/%d",key,a->name,(int)me->ult,a->cost);
        uint16_t col=ready?gx_blend(C_BTN,g_agents[me->agent].color,12):RGB(36,38,50);
        if(btn(160,26+i*30,154,26,l,col,true)&&ready) s_uiAbil=i;
        if(i<2&&me->cd[i]>0){ int w=(int)(154*me->cd[i]/a->cd); gx_rect_a(B,160,26+i*30,w,26,RGB(0,0,0),14); }
    }
    { char sw[32]; snprintf(sw,sizeof sw,"%s  SWAP WEAPON",g_set.bind[ACT_SWAP]>=0?btn_name(g_set.bind[ACT_SWAP]):"TOUCH");
      if(btn(160,118,154,24,sw,C_BTN,me->wp[0]>=0&&me->wp[1]>=0)) s_uiSwap=true; }
    gx_text(B,160,148,"TEAM COMMAND",C_DIM,1);
    static const char *CN[6]={"AUTO","FOLLOW","HOLD","SITE A","SITE B","SITE C"};
    for(int i=0;i<6;i++){
        bool en=(i<3)||(i-3<g_map.siteCount);
        if(btn(160+(i%3)*52,160+(i/3)*26,50,24,CN[i],g_m.cmd==i?C_RED:C_BTN,en)) game_set_cmd(i);
    }
    gx_text(B,160,214,g_m.cmd==CMD_AUTO?"BOTS FOLLOW OWN PLAN":"ORDER ACTIVE",C_DIM,1);
}

static void screen_play(int ticks){
    const Unit *view;
    s_uiAbil=-1; s_uiSwap=false;
    gx_clear(B,C_BG);
    if(I->down&BM(B_START)){ s_scr=SC_PAUSE; s_confirmQuit=0; }
    /* touch-look tracking */
    if(I->tdown) s_lookTouch=in_rect(5,25,146,146)&&g_m.phase!=PH_BUY;
    if(I->tup||!I->touching) s_lookTouch=false;
    view=game_view_unit();
    if(g_m.phase==PH_BUY&&g_m.u[0].alive) play_buy_ui(&g_m.u[0]); else play_live_ui(view);
    for(int k=0;k<ticks&&g_m.phase!=PH_MATCHOVER;k++){
        apply_look(k==0);
        Intent in=build_intent(k==0);
        if(k>0) in.abil=-1;
        game_tick(&in);
        if(g_m.u[0].alive&&g_m.u[0].moving) s_bob+=0.22f;
    }
    if(g_m.phase==PH_MATCHOVER){ s_scr=SC_RESULT; }
    view=game_view_unit();
    draw_play_top(view);
}

static void screen_pause(void){
    draw_play_top(game_view_unit());
    gx_rect_a(T,0,0,TOP_W,SCR_H,RGB(4,6,12),22);
    gx_textc(T,TOP_W/2,90,"PAUSED",C_TXT,4);
    gx_clear(B,C_BG); title_bar("PAUSE");
    if(btn(40,32,240,36,"RESUME",C_RED,true)||(I->down&BM(B_START))) s_scr=SC_PLAY;
    if(btn(40,76,240,30,"SETTINGS",C_BTN,true)){ s_ret=SC_PAUSE; s_scr=SC_SETTINGS; }
    if(btn(40,112,240,30,"CONTROLS",C_BTN,true)){ s_ret=SC_PAUSE; s_scr=SC_CONTROLS; }
    if(btn(40,148,240,36,s_confirmQuit?"TAP AGAIN TO QUIT MATCH":"QUIT TO MENU",s_confirmQuit?C_RED:C_BTN,true)){
        if(s_confirmQuit){ s_scr=SC_TITLE; s_loadedMap=-1; s_confirmQuit=0; } else s_confirmQuit=1;
    }
    gx_textc(B,BOT_W/2,200,"MATCH IS PAUSED",C_DIM,1);
}

static void screen_result(void){
    gx_clear(T,C_BG);
    bool won=g_m.matchWon;
    gx_textc(T,TOP_W/2,12,won?"VICTORY":"DEFEAT",won?C_GREEN:C_ENEMY,4);
    char b[48]; int ms=g_m.u[0].squad; snprintf(b,sizeof b,"%d - %d",g_m.score[ms],g_m.score[1-ms]); gx_textc(T,TOP_W/2,48,b,C_TXT,3);
    hud_scoreboard(T,40,84);
    gx_clear(B,C_BG); title_bar("MATCH COMPLETE");
    snprintf(b,sizeof b,"YOUR K/D: %d / %d",g_m.u[0].kills,g_m.u[0].deaths); gx_textc(B,BOT_W/2,40,b,C_TXT,1);
    snprintf(b,sizeof b,"ROUNDS PLAYED: %d",g_m.roundsPlayed); gx_textc(B,BOT_W/2,54,b,C_DIM,1);
    if(btn(40,80,240,44,"REMATCH",C_RED,true)){
        g_rng^=(uint32_t)plat_ms()*2654435761u; if(!g_rng) g_rng=1;
        game_new_match(s_map,LENT[s_len],s_diff,s_side,s_agent); s_buyTab=0; s_buySel=-1; s_scr=SC_PLAY; }
    if(btn(40,134,240,44,"MAIN MENU",C_BTN,true)){ s_scr=SC_TITLE; s_loadedMap=-1; }
}

/* ------------------------------------------------------------------ entry points */
void app_init(void){
    rd_init();
    settings_defaults(); settings_load();
    s_map=g_set.lastMap; s_side=g_set.lastSide; s_len=g_set.lastLen; s_diff=g_set.lastDiff; s_agent=g_set.lastAgent;
    for(int i=0;i<M_COUNT;i++) MAPN[i]=g_maps[i].name;
    g_rng=plat_ms()*2654435761u+12345u; if(!g_rng) g_rng=1;
    s_scr=SC_TITLE;
}
void app_set_fps(float f){ s_fps=f; }
void app_shutdown(void){ settings_save(); }

void app_frame(const Input *in,int ticks){
    I=in; T=plat_top(); B=plat_bottom(); s_time+=DT*(ticks>0?ticks:1)*0.5f;
    switch(s_scr){
    case SC_TITLE: screen_title(); break;
    case SC_SETUP: screen_setup(); break;
    case SC_AGENTS: screen_agents(); break;
    case SC_SETTINGS: screen_settings(); break;
    case SC_CONTROLS: screen_controls(); break;
    case SC_HOWTO: screen_howto(); break;
    case SC_PLAY: screen_play(ticks); break;
    case SC_PAUSE: screen_pause(); break;
    case SC_RESULT: screen_result(); break;
    }
}
