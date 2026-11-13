/* Top-screen heads-up display. */
#include "hud.h"
#include "gfx.h"
#include "settings.h"
#include <stdio.h>

#define COL_ALLY  RGB(100,205,225)
#define COL_ENEMY RGB(240,95,95)
#define COL_TXT   RGB(240,244,250)
#define COL_DIM   RGB(150,158,175)
#define COL_GOLD  RGB(255,210,90)

static void bar(Surf s,int x,int y,int w,int h,float v,uint16_t c){
    gx_rect(s,x,y,w,h,RGB(20,22,30));
    gx_rect(s,x,y,(int)(w*clampf(v,0,1)),h,c);
    gx_frame(s,x-1,y-1,w+2,h+2,RGB(0,0,0));
}

static void draw_crosshair(Surf s,const Unit *u){
    int cx=TOP_W/2, cy=SCR_H/2;
    const Weapon *w=unit_weapon(u);
    int gap=3+(int)(u->recoil*0.9f)+(u->moving?2:0)+(!u->grounded?3:0)+(int)(w->type==WT_SHOTGUN?5:0);
    uint16_t c=RGB(120,255,170);
    if(g_m.hitT>0) c=g_m.hitHead?RGB(255,80,80):RGB(255,255,255);
    gx_rect(s,cx-gap-6,cy,6,1,c); gx_rect(s,cx+gap+1,cy,6,1,c);
    gx_rect(s,cx,cy-gap-6,1,6,c); gx_rect(s,cx,cy+gap+1,1,6,c);
    if(g_m.hitT>0){
        uint16_t h=g_m.hitHead?RGB(255,60,60):RGB(255,255,255);
        gx_line(s,cx-9,cy-9,cx-5,cy-5,h); gx_line(s,cx+9,cy-9,cx+5,cy-5,h);
        gx_line(s,cx-9,cy+9,cx-5,cy+5,h); gx_line(s,cx+9,cy+9,cx+5,cy+5,h);
    }
}

static void draw_top_bar(Surf s,const Unit *view){
    char b[32];
    int mySquad=g_m.u[0].squad;
    int myScore=g_m.score[mySquad], opScore=g_m.score[1-mySquad];
    gx_rect_a(s,TOP_W/2-72,0,144,34,RGB(8,10,16),22);
    snprintf(b,sizeof b,"%d",myScore); gx_textc(s,TOP_W/2-52,3,b,COL_ALLY,2);
    snprintf(b,sizeof b,"%d",opScore); gx_textc(s,TOP_W/2+52,3,b,COL_ENEMY,2);
    if(g_m.phase==PH_BUY){ int t=(int)ceilf(g_m.phaseT); snprintf(b,sizeof b,"BUY %d",t); gx_textc(s,TOP_W/2,5,b,COL_GOLD,2); }
    else if(g_m.spike==SP_PLANTED){ snprintf(b,sizeof b,"%d.%d",(int)g_m.fuseT,(int)(g_m.fuseT*10)%10); gx_textc(s,TOP_W/2,5,b,(g_m.tick/6)&1?RGB(255,90,70):RGB(255,200,90),2); }
    else { int t=(int)ceilf(g_m.roundT<0?0:g_m.roundT); snprintf(b,sizeof b,"%d:%02d",t/60,t%60); gx_textc(s,TOP_W/2,5,b,COL_TXT,2); }
    /* alive pips */
    for(int i=0;i<NUNITS;i++){
        const Unit *u=&g_m.u[i]; int sq=u->squad;
        int idx=i%5; int x=(sq==mySquad)?TOP_W/2-70+idx*8:TOP_W/2+30+idx*8;
        gx_rect(s,x,24,6,6,u->alive?(sq==mySquad?COL_ALLY:COL_ENEMY):RGB(55,58,70));
    }
    const char *role=(view->team==TEAM_ATK)?"ATTACKING":"DEFENDING";
    gx_textc(s,TOP_W/2,36,role,COL_DIM,1);
    if(g_m.spike==SP_PLANTED){ snprintf(b,sizeof b,"SPIKE SITE %c",'A'+g_m.plantSite-1); gx_textc(s,TOP_W/2,46,b,RGB(255,120,90),1); }
    else if(g_m.spike==SP_DROPPED) gx_textc(s,TOP_W/2,46,"SPIKE DROPPED",COL_GOLD,1);
    else if(g_m.carrier==0&&view==&g_m.u[0]) gx_textc(s,TOP_W/2,46,"YOU HAVE THE SPIKE",COL_GOLD,1);
}

static void draw_feed(Surf s){
    int y=4;
    for(int i=0;i<6;i++){
        const KillMsg *k=&g_m.feed[i]; if(k->t<=0) continue;
        char b[64]; const char *kn=k->killer>=0?g_m.u[k->killer].name:"SPIKE";
        const char *wn=k->weapon>=0?g_weapons[k->weapon].name:"ABILITY";
        if(k->spike) wn="SPIKE";
        snprintf(b,sizeof b,"%s > %s %s%s",kn,g_m.u[k->victim].name,wn,k->head?" HS":"");
        int w=gx_textw(b,1)+8; int x=TOP_W-w-4;
        gx_rect_a(s,x,y,w,11,RGB(8,10,16),22);
        uint16_t c=(k->killer>=0&&g_m.u[k->killer].squad==g_m.u[0].squad)?COL_ALLY:COL_ENEMY;
        if(k->killer==0||k->victim==0) gx_frame(s,x,y,w,11,COL_GOLD);
        gx_text(s,x+4,y+2,b,c,1); y+=13;
    }
}

static void ability_box(Surf s,int x,int y,const Unit *u,int slot,int act){
    const Ability *a=&g_agents[u->agent].ab[slot];
    bool ready; float frac;
    if(slot<2){ ready=u->cd[slot]<=0; frac=ready?0:u->cd[slot]/a->cd; }
    else { ready=u->ult>=a->cost; frac=ready?0:1.0f-u->ult/a->cost; }
    uint16_t base=g_agents[u->agent].color;
    gx_rect(s,x,y,32,32,RGB(14,16,24));
    gx_rect(s,x+2,y+2,28,28,ready?base:RGB(60,62,72));
    /* icon letter */
    char l[2]={a->name[0],0}; gx_textc(s,x+16,y+9,l,ready?RGB(10,12,20):RGB(130,134,148),2);
    if(frac>0) gx_rect_a(s,x+2,y+2,28,(int)(28*frac),RGB(0,0,0),22);
    gx_frame(s,x,y,32,32,ready?COL_TXT:RGB(80,84,98));
    const char *bn=(act>=0)?btn_name(g_set.bind[act]):"TAP";
    if(g_set.bind[act]<0) bn="TOUCH";
    gx_textc(s,x+16,y+34,bn,COL_DIM,1);
    if(slot<2&&u->cd[slot]>0){ char c[8]; snprintf(c,sizeof c,"%d",(int)ceilf(u->cd[slot])); gx_textc(s,x+16,y+11,c,COL_TXT,1); }
    if(slot==2){ char c[8]; snprintf(c,sizeof c,"%d/%d",(int)u->ult,a->cost); gx_textc(s,x+16,y+-8,c,ready?COL_GOLD:COL_DIM,1); }
}

static void draw_prompts(Surf s,const Unit *u){
    char b[48]; int cx=TOP_W/2;
    const char *use=btn_name(g_set.bind[ACT_USE]); if(g_set.bind[ACT_USE]<0) use="TOUCH";
    if(g_m.phase!=PH_LIVE||!u->alive) return;
    if(g_m.spike==SP_CARRIED&&g_m.carrier==0&&u==&g_m.u[0]&&mp_site_at(u->x,u->y)>0&&g_m.planter!=0){
        snprintf(b,sizeof b,"HOLD %s TO PLANT",use); gx_texts(s,cx-gx_textw(b,1)/2,150,b,COL_GOLD,1); }
    if(g_m.planter==0){ bar(s,cx-40,160,80,6,g_m.plantProg/PLANT_TIME,COL_GOLD); gx_textc(s,cx,150,"PLANTING...",COL_GOLD,1); }
    if(g_m.spike==SP_PLANTED&&u->team==TEAM_DEF){
        float d2=sqf(u->x-g_m.spikeX)+sqf(u->y-g_m.spikeY);
        if(d2<1.6f*1.6f&&g_m.defuser!=0){ snprintf(b,sizeof b,"HOLD %s TO DEFUSE",use); gx_texts(s,cx-gx_textw(b,1)/2,150,b,COL_GOLD,1); }
        if(g_m.defuser==0){ bar(s,cx-40,160,80,6,g_m.defuseProg/DEFUSE_TIME,RGB(90,230,140)); gx_textc(s,cx,150,"DEFUSING...",RGB(90,230,140),1); }
    }
    if(g_m.spike==SP_PLANTED&&g_m.defuser>=0&&u->team==TEAM_ATK) gx_textc(s,cx,150,"SPIKE BEING DEFUSED!",RGB(255,90,90),1);
    if(g_m.spike==SP_PLANTED&&g_m.defuser>=0&&g_m.defuser!=0&&u->team==TEAM_DEF) gx_textc(s,cx,150,"ALLY DEFUSING",RGB(90,230,140),1);
    int site=mp_site_at(u->x,u->y);
    if(site) { snprintf(b,sizeof b,"SITE %c",'A'+site-1); gx_text(s,6,SCR_H-100,b,COL_GOLD,1); }
    const Weapon *w=unit_weapon(u);
    if(u->reloadT>0){ bar(s,cx-30,SCR_H/2+22,60,4,1.0f-u->reloadT/w->reload,COL_TXT); gx_textc(s,cx,SCR_H/2+30,"RELOADING",COL_DIM,1); }
    else if(u->ammo[u->cur]==0&&u->res[u->cur]==0) gx_textc(s,cx,SCR_H/2+30,"NO AMMO - SWAP",RGB(255,120,90),1);
    else if(u->ammo[u->cur]<=w->mag/4) gx_textc(s,cx,SCR_H/2+30,"LOW AMMO",RGB(255,200,90),1);
}

void hud_weapon_panel(Surf s,int x,int y,int wid){
    if(wid<0||wid>=W_COUNT) return;
    const Weapon *w=&g_weapons[wid]; char b[48];
    gx_rect_a(s,x,y,170,128,RGB(8,10,16),24); gx_frame(s,x,y,170,128,RGB(90,96,115));
    gx_text(s,x+6,y+5,w->name,COL_TXT,2);
    snprintf(b,sizeof b,"%s  $%d",w->cat,w->price); gx_text(s,x+6,y+22,b,COL_GOLD,1);
    int ty=y+36;
    struct { const char *n; float v,max; uint16_t c; } rows[]={
        {"BODY",(float)w->dmg*w->pellets,255,RGB(120,200,255)},{"HEAD",(float)w->head*w->pellets,255,RGB(255,120,100)},
        {"FIRE",w->rof,16,RGB(255,220,100)},{"MAG",(float)w->mag,100,RGB(150,220,150)},{"RELOAD",5.0f-w->reload,4,RGB(190,150,255)}};
    for(int i=0;i<5;i++){
        gx_text(s,x+6,ty,rows[i].n,COL_DIM,1);
        bar(s,x+50,ty,96,5,rows[i].v/rows[i].max,rows[i].c);
        ty+=13;
    }
    snprintf(b,sizeof b,"%.1f/S  %.1fS RELOAD  %d MAG",w->rof,w->reload,w->mag); gx_text(s,x+6,y+104,b,COL_DIM,1);
    if(w->pellets>1){ snprintf(b,sizeof b,"%d PELLETS X %d DMG",w->pellets,w->dmg); gx_text(s,x+6,y+114,b,COL_DIM,1); }
    else if(w->zoom>1) { snprintf(b,sizeof b,"SCOPE X%.1f",w->zoom); gx_text(s,x+6,y+114,b,COL_DIM,1); }
}

void hud_scoreboard(Surf s,int x,int y){
    gx_text(s,x,y,"NAME      AGENT     K  D  SCORE",COL_DIM,1);
    int row=1;
    for(int sq=0;sq<2;sq++) for(int i=0;i<NUNITS;i++){
        const Unit *u=&g_m.u[i]; if(u->squad!=sq) continue;
        char b[64]; snprintf(b,sizeof b,"%-9s %-9s %2d %2d  %5d",u->name,g_agents[u->agent].name,u->kills,u->deaths,u->score);
        gx_text(s,x,y+row*12,b,sq==g_m.u[0].squad?COL_ALLY:COL_ENEMY,1); row++;
        if(i%5==4) row++;
    }
}

void hud_draw(Surf s,const Unit *view,int buySel,float fps){
    char b[48];
    bool me=(view==&g_m.u[0]);
    /* flash / stun / damage overlays */
    if(view->flashT>0){ int a=(int)(clampf(view->flashT/1.2f,0,1)*32); gx_rect_a(s,0,0,TOP_W,SCR_H,RGB(255,255,255),a); }
    if(view->stunT>0) gx_rect_a(s,0,0,TOP_W,SCR_H,RGB(80,120,255),6);
    if(view->hurtT>0&&me) gx_rect_a(s,0,0,TOP_W,SCR_H,RGB(255,0,0),(int)(view->hurtT*20));
    for(int i=0;i<4;i++) if(g_m.dmgInd[i]>0&&me){
        float a=g_m.dmgIndAng[i]; int x=TOP_W/2+(int)(sinf(a)*60), y=SCR_H/2-(int)(cosf(a)*60);
        gx_disc(s,x,y,3,RGB(255,60,60)); }
    if(view->alive&&!view->scoped) draw_crosshair(s,view);
    draw_top_bar(s,view);
    draw_feed(s);
    /* economy (buy phase) */
    if(g_m.phase==PH_BUY){
        snprintf(b,sizeof b,"$%d",g_m.u[0].credits); gx_texts(s,6,6,b,COL_GOLD,2);
        gx_texts(s,6,26,"BUY: BOTTOM SCREEN",COL_TXT,1);
        hud_weapon_panel(s,226,52,buySel);
    } else { snprintf(b,sizeof b,"$%d",g_m.u[0].credits); gx_texts(s,6,6,b,COL_GOLD,1); }
    /* health / armor */
    if(view->alive){
        snprintf(b,sizeof b,"%d",view->hp); gx_texts(s,8,SCR_H-34,b,view->hp>30?COL_TXT:RGB(255,90,90),3);
        bar(s,60,SCR_H-30,70,5,view->hp/100.0f,RGB(90,220,130));
        bar(s,60,SCR_H-20,70,5,view->armor/50.0f,RGB(110,170,255));
        gx_text(s,60,SCR_H-12,view->armor>0?"SHIELD":"",RGB(110,170,255),1);
        /* weapon + ammo */
        const Weapon *w=unit_weapon(view);
        gx_textr(s,TOP_W-8,SCR_H-52,w->name,COL_DIM,1);
        snprintf(b,sizeof b,"%d",view->ammo[view->cur]); gx_texts(s,TOP_W-100,SCR_H-40,b,view->ammo[view->cur]>0?COL_TXT:RGB(255,90,90),3);
        snprintf(b,sizeof b,"/ %d",view->res[view->cur]); gx_text(s,TOP_W-52,SCR_H-26,b,COL_DIM,1);
        if(view->wp[1-view->cur]>=0){ gx_textr(s,TOP_W-8,SCR_H-12,g_weapons[view->wp[1-view->cur]].name,RGB(110,116,135),1); }
        /* abilities */
        if(me){ int ax=TOP_W/2-56; ability_box(s,ax,SCR_H-50,view,0,ACT_ABIL1); ability_box(s,ax+40,SCR_H-50,view,1,ACT_ABIL2); ability_box(s,ax+80,SCR_H-50,view,2,ACT_ULT);
                if(view->buffT>0){ snprintf(b,sizeof b,"OVERDRIVE %d",(int)ceilf(view->buffT)); gx_textc(s,TOP_W/2,SCR_H-70,b,COL_GOLD,1); } }
        if(me) draw_prompts(s,view);
    } else {
        gx_textc(s,TOP_W/2,SCR_H-60,"ELIMINATED",RGB(255,90,90),2);
        snprintf(b,sizeof b,"SPECTATING %s",view->name); gx_textc(s,TOP_W/2,SCR_H-38,b,COL_TXT,1);
    }
    if(g_m.toastT>0){ gx_textc(s,TOP_W/2,SCR_H-84,g_m.toast,COL_TXT,1); }
    if(g_m.bannerT>0){
        int w=gx_textw(g_m.banner,2)+24; int y=70;
        gx_rect_a(s,TOP_W/2-w/2,y-6,w,28,RGB(8,10,16),clampi((int)(g_m.bannerT*40),0,26));
        gx_textc(s,TOP_W/2,y,g_m.banner,g_m.bannerCol,2);
        if(g_m.phase==PH_ENDROUND) gx_textc(s,TOP_W/2,y+18,g_m.endText,COL_DIM,1);
    }
    if(g_m.phase==PH_ENDROUND&&g_m.bannerT<=0){ gx_textc(s,TOP_W/2,76,g_m.endText,COL_TXT,1); }
    if(view->scoped==false&&view->alive&&view->flashT<=0&&view->hp<100&&view->hp>0&&view->hp<25&&me) gx_rect_a(s,0,0,TOP_W,SCR_H,RGB(120,0,0),4);
    if(g_set.showFps){ snprintf(b,sizeof b,"%.0f FPS",fps); gx_text(s,TOP_W-50,SCR_H-100,b,COL_DIM,1); }
}
