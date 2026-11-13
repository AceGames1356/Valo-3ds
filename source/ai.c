/* Offline bot AI. Works entirely from the shared simulation state. */
#include "game.h"
#include <stdlib.h>

enum { AI_GOTO, AI_HOLD, AI_ENGAGE, AI_SEARCH, AI_COVER, AI_PLANT, AI_DEFUSE };

static const float REACT[3][2]={{0.55f,0.95f},{0.30f,0.55f},{0.15f,0.30f}};
static const float AIMERR[3]={5.0f,2.4f,1.0f};        /* degrees */
static const float TURNR[3]={4.5f,7.5f,12.0f};        /* rad/s   */

static int   s_atkSite, s_stageX, s_stageY, s_lurker, s_entX[3], s_entY[3], s_retriever;
static bool  s_exec;
static float s_execT;
static uint32_t s_lastTick;

/* ------------------------------------------------------------ path helpers */
static void path_cell(float sx,float sy,float gx,float gy,float frac,int *ox,int *oy){
    const uint16_t *f=nav_field((int)gx,(int)gy);
    int cx=(int)sx, cy=(int)sy, total=f[cy*MAP_W+cx];
    if(total==0xFFFF){ *ox=(int)gx; *oy=(int)gy; return; }
    int steps=(int)(total*frac);
    for(int i=0;i<steps;i++){ int nx,ny; if(!nav_step(f,cx,cy,&nx,&ny)) break; cx=nx; cy=ny; }
    *ox=cx; *oy=cy;
}
static void rand_cell_near(float x,float y,float rmin,float rmax,float *ox,float *oy){
    for(int i=0;i<30;i++){
        float a=rndf()*TAU_F, r=rndr(rmin,rmax);
        float px=x+cosf(a)*r, py=y+sinf(a)*r;
        if(mp_cell((int)px,(int)py)==C_OPEN){ *ox=(int)px+0.5f; *oy=(int)py+0.5f; return; }
    }
    mp_nearest_open(x,y,ox,oy);
}
static void set_goal(Ai *a,float x,float y){
    int gx=clampi((int)x,0,MAP_W-1), gy=clampi((int)y,0,MAP_H-1);
    if(mp_cell(gx,gy)!=C_OPEN){ float ox,oy; if(mp_nearest_open(gx+0.5f,gy+0.5f,&ox,&oy)){ gx=(int)ox; gy=(int)oy; } }
    a->goalX=gx; a->goalY=gy; a->repathT=0;
}

/* ------------------------------------------------------------ economy */
static bool try_buy(int ui,int item){ return game_buy(ui,item); }
void ai_buy(int ui){
    Unit *u=&g_m.u[ui];
    int c=u->credits; bool hasPrim=u->wp[1]>=0;
    if(!hasPrim){
        if(c>=3900){ try_buy(ui,rndi(2)?W_VANDAL:W_PHANTOM); try_buy(ui,BUY_HEAVY); }
        else if(c>=3300){ try_buy(ui,rndi(2)?W_VANDAL:W_PHANTOM); try_buy(ui,BUY_LIGHT); }
        else if(c>=2900){ try_buy(ui,rndi(2)?W_VANDAL:W_PHANTOM); }
        else if(c>=2450){ try_buy(ui,rndi(2)?W_BULLDOG:W_SPECTRE); try_buy(ui,BUY_LIGHT); }
        else if(c>=2050){ try_buy(ui,W_BULLDOG); }
        else if(c>=1600){ try_buy(ui,rndi(3)==0?W_ARES:W_SPECTRE); }
        else if(c>=1100&&rndi(3)){ try_buy(ui,W_STINGER); }
        else if(c>=850&&rndi(2)){ try_buy(ui,W_BUCKY); }
        else if(c>=900){ try_buy(ui,rndi(2)?W_GHOST:W_FRENZY); try_buy(ui,BUY_LIGHT); }
        else if(c>=500&&rndi(4)){ try_buy(ui,W_GHOST); }
    }
    if(u->armor==0&&u->credits>=1000+(hasPrim?0:200)&&hasPrim) try_buy(ui,u->credits>=2000?BUY_HEAVY:BUY_LIGHT);
    else if(u->armor==0&&u->credits>=400&&!hasPrim&&u->credits>=400+300&&u->wp[0]!=W_CLASSIC) try_buy(ui,BUY_LIGHT);
}

/* ------------------------------------------------------------ goals */
static bool is_atk(const Unit *u){ return u->team==TEAM_ATK; }

static void guard_goal(Ai *a,Unit *u){
    float gx,gy;
    for(int i=0;i<12;i++){
        rand_cell_near(g_m.spikeX,g_m.spikeY,2.5f,6.5f,&gx,&gy);
        float dx=g_m.spikeX-gx, dy=g_m.spikeY-gy, d=sqrtf(dx*dx+dy*dy);
        if(d>0.1f&&mp_trace(gx,gy,EYE_H,dx/d,dy/d,0,d)>=d-0.2f) break;
    }
    set_goal(a,gx,gy); (void)u;
}

static void assign_goal(int ui){
    Unit *u=&g_m.u[ui]; Ai *a=&u->ai; float gx,gy;
    int cmd=(u->squad==0&&ui!=0)?g_m.cmd:CMD_AUTO;
    a->state=AI_GOTO; a->goalWait=0;
    int sc=g_map.siteCount;
    if(is_atk(u)){
        int site=s_atkSite;
        if(cmd>=CMD_SITE_A&&cmd-CMD_SITE_A<sc) site=cmd-CMD_SITE_A;
        if(g_m.spike==SP_PLANTED){ guard_goal(a,u); return; }
        if(cmd==CMD_FOLLOW){ rand_cell_near(g_m.u[0].x,g_m.u[0].y,1.5f,3.5f,&gx,&gy); set_goal(a,gx,gy); return; }
        if(cmd==CMD_HOLD){ set_goal(a,u->x,u->y); a->state=AI_HOLD; a->goalWait=999; return; }
        if(g_m.spike==SP_DROPPED&&ui==s_retriever){ set_goal(a,g_m.spikeX,g_m.spikeY); return; }
        if(ui==s_lurker&&ui!=g_m.carrier&&g_m.roundT>ROUND_TIME-35.0f&&cmd==CMD_AUTO){
            int lx,ly; path_cell(g_map.spawn[0].cx,g_map.spawn[0].cy,g_map.site[s_atkSite].cx,g_map.site[s_atkSite].cy,0.5f,&lx,&ly);
            rand_cell_near(lx+0.5f,ly+0.5f,0,2.0f,&gx,&gy); set_goal(a,gx,gy); return;
        }
        if(!s_exec&&cmd==CMD_AUTO){ rand_cell_near(s_stageX+0.5f,s_stageY+0.5f,0,3.0f,&gx,&gy); set_goal(a,gx,gy); return; }
        if(ui==g_m.carrier){ set_goal(a,g_map.site[site].cx,g_map.site[site].cy); return; }
        if(mp_rand_open_in(&g_map.site[site],&gx,&gy)) set_goal(a,gx,gy);
    } else {
        if(g_m.spike==SP_PLANTED){
            int defuser=-1;
            for(int i=0;i<NUNITS;i++) if(g_m.u[i].alive&&g_m.u[i].team==TEAM_DEF&&!g_m.u[i].isPlayer){ defuser=i; break; }
            if(ui==defuser){ set_goal(a,g_m.spikeX,g_m.spikeY); a->role=2; }
            else { guard_goal(a,u); }
            return;
        }
        int site=a->holdSite;
        if(cmd>=CMD_SITE_A&&cmd-CMD_SITE_A<sc) site=cmd-CMD_SITE_A;
        if(cmd==CMD_FOLLOW){ rand_cell_near(g_m.u[0].x,g_m.u[0].y,1.5f,3.5f,&gx,&gy); set_goal(a,gx,gy); return; }
        if(cmd==CMD_HOLD){ set_goal(a,u->x,u->y); a->state=AI_HOLD; a->goalWait=999; return; }
        if(rndi(2)){ rand_cell_near(s_entX[site]+0.5f,s_entY[site]+0.5f,0,2.5f,&gx,&gy); set_goal(a,gx,gy); }
        else if(mp_rand_open_in(&g_map.site[site],&gx,&gy)) set_goal(a,gx,gy);
    }
}

void ai_on_event_plant(void){
    for(int i=0;i<NUNITS;i++) if(g_m.u[i].alive&&!g_m.u[i].isPlayer){
        Unit *u=&g_m.u[i]; u->ai.usedA0=u->ai.usedA1=false;
        assign_goal(i);
    }
}

void ai_round_start(void){
    s_atkSite=rndi(g_map.siteCount); s_exec=false; s_execT=rndr(11.0f,17.0f); s_lurker=-1; s_retriever=-1; s_lastTick=0xFFFFFFFFu;
    path_cell(g_map.spawn[0].cx,g_map.spawn[0].cy,g_map.site[s_atkSite].cx,g_map.site[s_atkSite].cy,0.58f,&s_stageX,&s_stageY);
    for(int s=0;s<g_map.siteCount;s++)
        path_cell(g_map.spawn[0].cx,g_map.spawn[0].cy,g_map.site[s].cx,g_map.site[s].cy,0.8f,&s_entX[s],&s_entY[s]);
    int cand[NUNITS],n=0,defs[NUNITS],nd=0;
    for(int i=0;i<NUNITS;i++){ Unit *u=&g_m.u[i];
        if(u->team==TEAM_ATK&&i!=g_m.carrier&&!u->isPlayer) cand[n++]=i;
        if(u->team==TEAM_DEF) defs[nd++]=i; }
    if(n>=3) s_lurker=cand[rndi(n)];
    for(int i=nd-1;i>0;i--){ int j=rndi(i+1); int t=defs[i]; defs[i]=defs[j]; defs[j]=t; }
    for(int k=0;k<nd;k++) g_m.u[defs[k]].ai.holdSite=k%g_map.siteCount;
    for(int i=0;i<NUNITS;i++){
        Unit *u=&g_m.u[i]; if(u->isPlayer) continue;
        u->ai.target=-1; u->ai.cmdSeen=0; assign_goal(i); ai_buy(i);
    }
}

static void team_update(void){
    /* execute trigger for attackers */
    if(!s_exec&&g_m.phase==PH_LIVE){
        int near=0,tot=0;
        for(int i=0;i<NUNITS;i++){ Unit *u=&g_m.u[i];
            if(!u->alive||u->team!=TEAM_ATK||u->isPlayer||i==s_lurker) continue;
            tot++; if(sqf(u->x-s_stageX)+sqf(u->y-s_stageY)<7.0f*7.0f) near++; }
        s_execT-=DT;
        if(s_execT<=0||(tot>0&&near*10>=tot*7)||(g_m.spike==SP_PLANTED)){
            s_exec=true;
            for(int i=0;i<NUNITS;i++) if(g_m.u[i].alive&&!g_m.u[i].isPlayer&&g_m.u[i].team==TEAM_ATK) assign_goal(i);
        }
    }
    /* dropped spike: pick the nearest attacker bot as retriever */
    if(g_m.spike==SP_DROPPED){
        if(s_retriever<0||!g_m.u[s_retriever].alive){
            float bd=1e9f; s_retriever=-1;
            for(int i=0;i<NUNITS;i++){ Unit *u=&g_m.u[i]; if(!u->alive||u->team!=TEAM_ATK||u->isPlayer) continue;
                float d=sqf(u->x-g_m.spikeX)+sqf(u->y-g_m.spikeY); if(d<bd){ bd=d; s_retriever=i; } }
            if(s_retriever>=0) assign_goal(s_retriever);
        }
    } else if(s_retriever>=0){ int r=s_retriever; s_retriever=-1; if(g_m.u[r].alive) assign_goal(r); }
}

/* ------------------------------------------------------------ perception */
static int pick_target(int ui){
    Unit *u=&g_m.u[ui]; int best=-1; float bd=1e9f;
    for(int i=0;i<NUNITS;i++){ const Unit *e=&g_m.u[i];
        if(!e->alive||e->team==u->team) continue;
        float d2=sqf(e->x-u->x)+sqf(e->y-u->y); if(d2>45.0f*45.0f||d2>=bd) continue;
        if(game_can_see(u,e,true)){ bd=d2; best=i; } }
    if(best>=0) return best;
    for(int i=0;i<MAX_ZONES;i++){ const Zone *z=&g_m.zones[i];
        if(!z->active||z->type!=Z_TURRET||z->team==u->team) continue;
        float dx=z->x-u->x, dy=z->y-u->y, d=sqrtf(dx*dx+dy*dy);
        if(d>13.0f||d<0.2f||fabsf(wrapang(atan2f(dy,dx)-u->yaw))>0.95f||u->flashT>0) continue;
        if(mp_trace(u->x,u->y,unit_eye(u),dx/d,dy/d,(0.4f-unit_eye(u))/d,d)>=d-0.3f&&!game_smoke_blocks(u->x,u->y,z->x,z->y)) return 100+i;
    }
    return -1;
}
static bool target_pos(int t,float *x,float *y,float *z){
    if(t>=100){ const Zone *zn=&g_m.zones[t-100]; if(!zn->active) return false; *x=zn->x; *y=zn->y; *z=0.4f; return true; }
    const Unit *e=&g_m.u[t]; if(!e->alive) return false; *x=e->x; *y=e->y; *z=e->z+UNIT_H*0.68f; return true;
}
static bool still_visible(int ui,int t){
    Unit *u=&g_m.u[ui];
    if(t>=100) return pick_target(ui)==t;
    return g_m.u[t].alive&&game_can_see(u,&g_m.u[t],false);
}

/* ------------------------------------------------------------ cover */
static bool find_cover(int ui,float ex,float ey,float *ox,float *oy){
    Unit *u=&g_m.u[ui]; float bd=1e9f; bool found=false;
    for(int i=0;i<14;i++){
        float cx,cy; rand_cell_near(u->x,u->y,1.5f,8.0f,&cx,&cy);
        float dx=ex-cx, dy=ey-cy, d=sqrtf(dx*dx+dy*dy); if(d<1.0f) continue;
        if(mp_trace(cx,cy,EYE_H,dx/d,dy/d,0,d)>=d-0.3f) continue;     /* still exposed */
        float my=sqf(cx-u->x)+sqf(cy-u->y);
        if(my<bd){ bd=my; *ox=cx; *oy=cy; found=true; }
    }
    return found;
}

/* ------------------------------------------------------------ abilities */
static void bot_abilities(int ui,Intent *out,int target){
    Unit *u=&g_m.u[ui]; Ai *a=&u->ai;
    if(g_m.phase!=PH_LIVE||out->abil>=0) return;
    if(a->abilT>0){ a->abilT-=DT; return; }
    float tx=0,ty=0,tz=0,td=0; bool vis=false;
    if(target>=0&&target_pos(target,&tx,&ty,&tz)){ vis=true; td=sqrtf(sqf(tx-u->x)+sqf(ty-u->y)); }
    int site=s_atkSite;
    float sd=sqrtf(sqf(g_map.site[site].cx-u->x)+sqf(g_map.site[site].cy-u->y));
    float spd=sqrtf(sqf(g_m.spikeX-u->x)+sqf(g_m.spikeY-u->y));
    bool planted=g_m.spike==SP_PLANTED;
    for(int s=2;s>=0;s--){
        const Ability *ab=&g_agents[u->agent].ab[s];
        if(s<2?u->cd[s]>0:u->ult<ab->cost) continue;
        float ax=u->x,ay=u->y; bool go=false;
        switch(ab->type){
        case AB_DASH: go=is_atk(u)&&!vis&&!planted&&s_exec&&sd<10&&sd>4&&rndi(60)==0; if(a->state==AI_COVER&&rndi(10)==0) go=true; break;
        case AB_FLASH: go=vis&&td>2.5f&&td<ab->radius&&a->reactT>0.05f; break;
        case AB_SMOKE: case AB_BIGSMOKE:
            if(is_atk(u)&&!planted&&s_exec&&sd<15&&sd>5&&!vis){ go=true; ax=g_map.site[site].cx; ay=g_map.site[site].cy; rand_cell_near(ax,ay,0,2.5f,&ax,&ay); }
            else if(!is_atk(u)&&planted&&spd<14&&u->ai.role==2){ go=true; ax=g_m.spikeX; ay=g_m.spikeY; }
            if(ab->type==AB_BIGSMOKE&&rndi(3)) go=false;
            break;
        case AB_FIELD: if(vis&&td<ab->range){ go=true; ax=tx; ay=ty; } break;
        case AB_TRAP: go=!is_atk(u)&&a->state==AI_HOLD&&!a->usedA0; if(planted&&is_atk(u)&&a->state==AI_HOLD) go=true; break;
        case AB_TURRET: if(a->state==AI_HOLD&&!a->usedA1&&(!is_atk(u)||planted)){ go=true; game_aim_point(u,ab->range,&ax,&ay); } break;
        case AB_SCAN: go=(is_atk(u)&&!planted&&s_exec&&sd<16&&!vis)||(!is_atk(u)&&planted&&spd<14&&!vis); break;
        case AB_STUN: if(vis&&td>2.5f&&td<ab->range){ go=true; ax=tx; ay=ty; } break;
        case AB_OVERDRIVE: go=vis&&td<20; break;
        case AB_HEAL: { int low=0; for(int i=0;i<NUNITS;i++){ Unit *e=&g_m.u[i]; if(e->alive&&e->squad==u->squad&&e->hp<50&&sqf(e->x-u->x)+sqf(e->y-u->y)<sqf(ab->radius)) low++; }
                        go=low>=2||(u->hp<45&&(vis||u->hurtT>0)); } break;
        case AB_REVEALALL: go=(is_atk(u)&&!planted&&s_exec&&sd<12)||(!is_atk(u)&&planted&&spd<12); break;
        }
        if(go){
            out->abil=s; out->aimGiven=true; out->aimX=ax; out->aimY=ay;
            if(ab->type==AB_TRAP) a->usedA0=true;
            if(ab->type==AB_TURRET) a->usedA1=true;
            a->abilT=rndr(0.6f,1.5f); return;
        }
    }
}

/* ------------------------------------------------------------ main think */
static void move_toward(Unit *u,float tx,float ty,float *mx,float *my){
    float dx=tx-u->x, dy=ty-u->y, d=sqrtf(dx*dx+dy*dy);
    if(d<0.05f){ *mx=*my=0; return; }
    *mx=dx/d; *my=dy/d;
}
static bool path_dir(Unit *u,Ai *a,float *mx,float *my,float arrive){
    float gx=a->goalX+0.5f, gy=a->goalY+0.5f;
    if(sqf(gx-u->x)+sqf(gy-u->y)<arrive*arrive){ *mx=*my=0; return true; }
    const uint16_t *f=nav_field(a->goalX,a->goalY);
    int nx,ny;
    if(nav_step(f,(int)u->x,(int)u->y,&nx,&ny)){
        float tx=nx+0.5f, ty=ny+0.5f;
        /* when the next cell is straight ahead keep heading for the one after for smoother corners */
        move_toward(u,tx,ty,mx,my);
        return false;
    }
    move_toward(u,gx,gy,mx,my);
    return sqf(gx-u->x)+sqf(gy-u->y)<1.0f;
}
static void turn_toward(Unit *u,float wy,float wp,float rate){
    float dy=wrapang(wy-u->yaw), m=rate*DT;
    u->yaw=wrapang(u->yaw+clampf(dy,-m,m));
    u->pitch+=clampf(wp-u->pitch,-m,m);
}

void ai_think(int ui,Intent *out){
    Unit *u=&g_m.u[ui]; Ai *a=&u->ai; int diff=g_m.diff;
    out->abil=-1;
    if(g_m.tick!=s_lastTick){ s_lastTick=g_m.tick; team_update(); }
    if(g_m.phase==PH_BUY||g_m.phase==PH_ENDROUND){ turn_toward(u,u->yaw,0,3); return; }

    /* command changes from the player */
    if(u->squad==0&&a->cmdSeen!=g_m.cmd){ a->cmdSeen=g_m.cmd; assign_goal(ui); }

    /* ---- perception ---- */
    int prev=a->target;
    if(prev>=0&&!still_visible(ui,prev)){
        a->seeT-=DT*2; if(a->seeT<-0.3f){ a->target=-1; a->seeT=0; if(a->memT>0) a->state=AI_SEARCH; }
    }
    if(a->target<0&&((g_m.tick+(uint32_t)ui)%3==0)){
        int t=pick_target(ui);
        if(t>=0){ a->target=t; a->seeT=0.3f; a->reactT=rndr(REACT[diff][0],REACT[diff][1]); a->state=AI_ENGAGE; }
    }
    if(a->target>=0&&a->seeT>-0.3f) a->seeT=clampf(a->seeT+DT,-0.3f,5.0f);
    if(a->target>=0){ float x,y,z; if(!target_pos(a->target,&x,&y,&z)) a->target=-1; else { a->memX=x; a->memY=y; a->memT=5.0f; } }
    if(a->target<0) a->seeT=0;
    if(a->memT>0) a->memT-=DT;
    /* hearing */
    if(a->target<0){
        for(int i=0;i<NUNITS;i++){ const Unit *e=&g_m.u[i];
            if(!e->alive||e->team==u->team||e->noiseT<=0) continue;
            float r=e->noiseT>0.3f?16.0f:6.0f, d2=sqf(e->x-u->x)+sqf(e->y-u->y);
            if(d2<r*r&&a->memT<2.5f){ a->memX=e->x; a->memY=e->y; a->memT=4.0f; a->alertT=2.0f; } }
        if(u->hurtT>0.4f&&a->memT<2.5f){ a->memX=u->x+cosf(u->hurtDir)*6.0f; a->memY=u->y+sinf(u->hurtDir)*6.0f; a->memT=3.0f; a->alertT=2.0f; }
    }
    if(a->alertT>0) a->alertT-=DT;
    if(a->reactT>0) a->reactT-=DT;
    if(a->jitT<=0){ float e=AIMERR[diff]*(PI_F/180.0f); a->jitYaw=(rndf()*2-1)*e; a->jitPitch=(rndf()*2-1)*e*0.7f; a->jitT=rndr(0.25f,0.5f); } else a->jitT-=DT;

    float mx=0,my=0; bool moveWanted=false, wantFire=false, wantUse=false, wantReload=false;
    const Weapon *w=unit_weapon(u);
    float wantYaw=u->yaw, wantPitch=0, tx=0,ty=0,tz=0, td=0; bool engaged=false;

    if(a->target>=0&&target_pos(a->target,&tx,&ty,&tz)){
        engaged=true;
        td=sqrtf(sqf(tx-u->x)+sqf(ty-u->y));
        float rawYaw=atan2f(ty-u->y,tx-u->x);
        float rawPitch=atan2f(tz-unit_eye(u),td>0.1f?td:0.1f);
        float spdT=0; if(a->target<100) spdT=sqrtf(sqf(g_m.u[a->target].vx)+sqf(g_m.u[a->target].vy));
        float k=1.0f+spdT*0.25f;
        wantYaw=rawYaw+a->jitYaw*k; wantPitch=rawPitch+a->jitPitch*k;
        float rate=TURNR[diff]*(a->reactT>0?0.6f:1.0f);
        turn_toward(u,wantYaw,wantPitch,rate);
        if(u->hp<30&&a->state!=AI_COVER&&a->coverT<=0&&rndi(40)==0){
            float cx,cy; if(find_cover(ui,tx,ty,&cx,&cy)){ set_goal(a,cx,cy); a->state=AI_COVER; a->coverT=rndr(2.0f,3.5f); }
        }
        if(u->ammo[u->cur]<=0&&u->res[u->cur]<=0){ out->swap=(u->wp[1-u->cur]>=0); }
        if(a->state==AI_COVER){
            moveWanted=!path_dir(u,a,&mx,&my,0.5f);
            a->coverT-=DT; if(a->coverT<=0){ a->state=AI_ENGAGE; }
            if(u->ammo[u->cur]<w->mag/2) wantReload=true;
        } else {
            float err=fabsf(wrapang(wantYaw-u->yaw)), perr=fabsf(wantPitch-u->pitch);
            float tol=(w->type==WT_SNIPER)?0.025f:(td<4?0.12f:0.05f);
            bool aligned=err<tol&&perr<tol*1.5f&&a->reactT<=0&&u->flashT<=0;
            if(a->pauseT>0){ a->pauseT-=DT; wantFire=false; }
            else { wantFire=aligned; a->burstT-=DT; if(a->burstT<=0){ a->burstT=rndr(0.3f,0.9f); a->pauseT=rndr(0.1f,0.3f)+(diff==DIFF_EASY?0.25f:0.0f); } }
            if(!w->autofire&&u->fireHeld) wantFire=false;
            /* movement: stop to shoot, strafe in the gaps */
            if(a->strafeT<=0){ a->strafeDir=rndi(2)?1.0f:-1.0f; a->strafeT=rndr(0.4f,1.1f); } else a->strafeT-=DT;
            if(a->pauseT>0||!aligned){
                float px=-sinf(rawYaw)*a->strafeDir, py=cosf(rawYaw)*a->strafeDir;
                mx=px; my=py; moveWanted=true;
                if(td>16.0f&&w->type!=WT_SNIPER){ mx+=cosf(rawYaw)*0.7f; my+=sinf(rawYaw)*0.7f; }
                if(w->type==WT_SHOTGUN&&td>5.0f){ mx+=cosf(rawYaw); my+=sinf(rawYaw); }
                if(td<3.0f&&w->type!=WT_SHOTGUN){ mx-=cosf(rawYaw)*0.8f; my-=sinf(rawYaw)*0.8f; }
            }
            out->alt=(w->zoom>1.0f);
        }
    } else {
        /* ---- not fighting: objectives & movement ---- */
        a->coverT-=DT;
        if(a->state==AI_COVER||a->state==AI_ENGAGE) a->state=(a->memT>0)?AI_SEARCH:AI_GOTO;
        if(u->ammo[u->cur]<w->mag*0.5f&&u->res[u->cur]>0) wantReload=true;
        if(a->state==AI_SEARCH){
            if(a->memT<=0){ a->state=AI_GOTO; assign_goal(ui); }
            else { set_goal(a,a->memX,a->memY); bool arrived=path_dir(u,a,&mx,&my,1.2f); moveWanted=!arrived; if(arrived) a->memT=0; }
        }
        bool spikeCarrier=(g_m.spike==SP_CARRIED&&g_m.carrier==ui);
        bool defuserNow=(g_m.spike==SP_PLANTED&&!is_atk(u)&&a->role==2);
        /* attackers rush a defuse attempt */
        if(g_m.spike==SP_PLANTED&&is_atk(u)&&g_m.defuser>=0&&a->state!=AI_SEARCH){
            a->memX=g_m.spikeX; a->memY=g_m.spikeY; a->memT=6.0f; a->state=AI_SEARCH; }
        if(a->state==AI_GOTO||a->state==AI_PLANT||a->state==AI_DEFUSE){
            bool arrived=path_dir(u,a,&mx,&my,defuserNow?0.7f:0.6f);
            if(spikeCarrier&&s_exec&&mp_site_at(u->x,u->y)>0&&u->ai.seeT<=0){ wantUse=true; a->state=AI_PLANT; }
            else if(a->state==AI_PLANT&&!spikeCarrier) a->state=AI_GOTO;
            if(defuserNow&&sqf(u->x-g_m.spikeX)+sqf(u->y-g_m.spikeY)<1.2f*1.2f&&a->alertT<=0){ wantUse=true; a->state=AI_DEFUSE; mx=my=0; }
            else if(arrived){ if(a->state==AI_GOTO){ a->state=AI_HOLD; a->goalWait=rndr(4.0f,9.0f); } }
            moveWanted=!arrived&&a->state!=AI_PLANT&&a->state!=AI_DEFUSE;
            if(a->state==AI_DEFUSE||a->state==AI_PLANT){ moveWanted=false; wantUse=true; }
        }
        if(a->state==AI_HOLD){
            a->goalWait-=DT;
            if(spikeCarrier&&s_exec){ assign_goal(ui); }
            else if(a->goalWait<=0&&!(u->squad==0&&g_m.cmd==CMD_HOLD)){
                /* ~wait at the stage point until the team executes, otherwise reposition */
                if(is_atk(u)&&!s_exec&&ui!=s_lurker&&g_m.spike!=SP_PLANTED) a->goalWait=2.0f; else assign_goal(ui);
            }
            /* hold: slowly look toward the likely threat */
            float lx=is_atk(u)?g_map.site[s_atkSite].cx:g_map.site[a->holdSite].cx, ly=is_atk(u)?g_map.site[s_atkSite].cy:g_map.site[a->holdSite].cy;
            if(g_m.spike==SP_PLANTED){ lx=g_m.spikeX; ly=g_m.spikeY; }
            if(a->alertT>0){ lx=a->memX; ly=a->memY; }
            wantYaw=atan2f(ly-u->y,lx-u->x);
        } else if(moveWanted){ wantYaw=atan2f(my,mx); }
        else if(a->state==AI_PLANT||a->state==AI_DEFUSE){ /* keep facing */ wantYaw=u->yaw; }
        if(a->state==AI_SEARCH||a->alertT>0) turn_toward(u,wantYaw,0,TURNR[diff]*0.7f); else turn_toward(u,wantYaw,0,4.0f);
        if(u->squad==0&&g_m.cmd==CMD_FOLLOW&&a->state==AI_HOLD&&sqf(u->x-g_m.u[0].x)+sqf(u->y-g_m.u[0].y)>16.0f) assign_goal(ui);
    }

    /* ---- stuck detection ---- */
    if(moveWanted){
        a->stuckT+=DT;
        if(a->stuckT>0.6f){
            float moved=sqf(u->x-a->stuckX)+sqf(u->y-a->stuckY);
            if(moved<0.06f*0.06f*18.0f){ a->strafeDir=rndi(2)?1.0f:-1.0f; a->strafeT=0.5f; mx+=-my*a->strafeDir; my+=mx*a->strafeDir*0.3f; if(rndi(3)==0) out->jump=true; if(a->stuckT>2.5f){ assign_goal(ui); a->stuckT=0; } }
            else a->stuckT=0;
            a->stuckX=u->x; a->stuckY=u->y;
        }
    } else a->stuckT=0;

    /* ---- write intent (world move vector -> local frame) ---- */
    float m=sqrtf(mx*mx+my*my);
    if(moveWanted&&m>0.01f){
        mx/=m; my/=m;
        out->fwd=mx*cosf(u->yaw)+my*sinf(u->yaw);
        out->strafe=-mx*sinf(u->yaw)+my*cosf(u->yaw);
        if(!engaged&&a->alertT>0&&!is_atk(u)) { out->fwd*=0.6f; out->strafe*=0.6f; out->alt=true; }
    }
    out->fire=wantFire; out->use=wantUse; out->reload=wantReload;
    /* jump over low cover that blocks the straight path (rare, navmesh avoids it) */
    bot_abilities(ui,out,a->target>=0?a->target:-1);
}

/* test/debug hook */
int ai_debug_exec(void){ return s_exec?1:0; }
float ai_debug_execT(void){ return s_execT; }
