#include "game.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>

uint32_t g_rng=0x1234567u;
bool g_set_aim_assist(void){ return g_set.aimAssist!=0; }
Match g_m;
bool g_test_freeze_ai=false;     /* test hook: bots stand still */

#define PLAYER_R   0.22f
#define BASE_SPEED 3.0f
#define SIDE_ARM 0
#define PRIMARY  1

/* ------------------------------------------------------------------ helpers */
const Weapon *unit_weapon(const Unit *u){ int w=u->wp[u->cur]; return &g_weapons[w<0?W_CLASSIC:w]; }
int game_side_of_squad(int squad){
    int swapped=(g_m.roundsPlayed>=g_m.half)?1:0;
    int base=(squad==0)?g_m.startSide:1-g_m.startSide;
    return base^swapped;
}
int game_alive(int team){ int n=0; for(int i=0;i<NUNITS;i++) if(g_m.u[i].alive&&g_m.u[i].team==team) n++; return n; }
void game_toast(const char *s){ snprintf(g_m.toast,sizeof g_m.toast,"%s",s); g_m.toastT=2.0f; }
void game_banner(const char *s,uint16_t col){ snprintf(g_m.banner,sizeof g_m.banner,"%s",s); g_m.bannerT=2.5f; g_m.bannerCol=col; }
bool unit_busy_spike(int ui){ return g_m.planter==ui||g_m.defuser==ui; }
bool unit_in_spawn(const Unit *u){
    const Rect *r=&g_map.spawn[u->team];
    return u->x>=r->x0&&u->x<=r->x1+1&&u->y>=r->y0&&u->y<=r->y1+1;
}
const Unit *game_view_unit(void){
    const Unit *p=&g_m.u[0];
    if(p->alive) return p;
    if(g_m.spec>=0&&g_m.u[g_m.spec].alive) return &g_m.u[g_m.spec];
    for(int i=1;i<NUNITS;i++) if(g_m.u[i].squad==0&&g_m.u[i].alive) return &g_m.u[i];
    return p;
}

static void zone_clear(void){ memset(g_m.zones,0,sizeof g_m.zones); }
static Zone *zone_add(int type,int team,int owner,float x,float y,float r,float dur){
    int slot=-1;
    for(int i=0;i<MAX_ZONES;i++) if(!g_m.zones[i].active){ slot=i; break; }
    if(slot<0){ float old=1e9f; for(int i=0;i<MAX_ZONES;i++) if(g_m.zones[i].t<old){ old=g_m.zones[i].t; slot=i; } }
    Zone *z=&g_m.zones[slot]; memset(z,0,sizeof *z);
    z->type=type; z->team=team; z->owner=owner; z->x=x; z->y=y; z->r=r; z->t=z->t0=dur; z->active=true;
    return z;
}

bool game_smoke_blocks(float x0,float y0,float x1,float y1){
    for(int i=0;i<MAX_ZONES;i++){
        const Zone *z=&g_m.zones[i];
        if(!z->active||z->type!=Z_SMOKE) continue;
        float dx=x1-x0, dy=y1-y0, l2=dx*dx+dy*dy;
        float t=l2>1e-6f?((z->x-x0)*dx+(z->y-y0)*dy)/l2:0; t=clampf(t,0,1);
        float cx=x0+dx*t-z->x, cy=y0+dy*t-z->y;
        if(cx*cx+cy*cy < sqf(z->r*0.85f)) return true;
    }
    return false;
}

bool game_can_see(const Unit *a,const Unit *b,bool useFov){
    if(a->flashT>0.0f) return false;
    float dx=b->x-a->x, dy=b->y-a->y, d=sqrtf(dx*dx+dy*dy);
    if(d<0.05f) return true;
    if(useFov && fabsf(wrapang(atan2f(dy,dx)-a->yaw))>0.95f) return false;
    float ez=unit_eye(a), ux=dx/d, uy=dy/d;
    float tz=b->z+UNIT_H*0.62f;
    if(mp_trace(a->x,a->y,ez,ux,uy,(tz-ez)/d,d) >= d-0.2f){
        return !game_smoke_blocks(a->x,a->y,b->x,b->y);
    }
    tz=b->z+UNIT_H*0.92f;                       /* head peeking over cover */
    if(mp_trace(a->x,a->y,ez,ux,uy,(tz-ez)/d,d) >= d-0.2f) return !game_smoke_blocks(a->x,a->y,b->x,b->y);
    return false;
}

void game_aim_point(const Unit *u,float range,float *x,float *y){
    float dx=cosf(u->yaw), dy=sinf(u->yaw), eye=unit_eye(u), d=range;
    if(u->pitch<-0.03f){ float fd=eye/tanf(-u->pitch); if(fd<d) d=fd; }
    float wall=mp_trace(u->x,u->y,eye,dx,dy,tanf(u->pitch),d+0.5f);
    if(wall<d) d=wall-0.35f;
    if(d<0.6f) d=0.6f;
    *x=u->x+dx*d; *y=u->y+dy*d;
    if(mp_cell((int)*x,(int)*y)!=C_OPEN) mp_nearest_open(*x,*y,x,y);
}

/* ------------------------------------------------------------------ loadout / economy */
static void give_weapon(Unit *u,int w){
    const Weapon *d=&g_weapons[w];
    u->wp[d->slot]=w; u->ammo[d->slot]=d->mag; u->res[d->slot]=d->reserve;
    u->cur=d->slot; u->reloadT=0; u->swapT=0.3f;
}
static void reset_loadout(Unit *u){
    u->wp[0]=-1; u->wp[1]=-1; u->armor=0; give_weapon(u,W_CLASSIC); u->wp[1]=-1; u->cur=0;
}
int buy_price(int item){
    if(item==BUY_LIGHT) return 400;
    if(item==BUY_HEAVY) return 1000;
    return (item>=0&&item<W_COUNT)?g_weapons[item].price:99999;
}
bool game_buy(int ui,int item){
    Unit *u=&g_m.u[ui];
    g_m.buyMsg[0]=0;
    if(g_m.phase!=PH_BUY||!u->alive){ snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"BUY PHASE IS OVER"); return false; }
    if(!unit_in_spawn(u)){ snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"RETURN TO SPAWN TO BUY"); return false; }
    int price=buy_price(item);
    if(item==BUY_LIGHT||item==BUY_HEAVY){
        int level=(item==BUY_LIGHT)?25:50;
        if(u->armor>=level&&u->refund[2]==0){ snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"ARMOR ALREADY OWNED"); return false; }
        if(u->armor>=level){ snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"ARMOR ALREADY OWNED"); return false; }
        int ref=u->refund[2];
        if(u->credits+ref<price){ snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"NOT ENOUGH CREDITS"); return false; }
        u->credits+=ref-price; u->armor=level; u->refund[2]=price;
        snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"BOUGHT %s SHIELD",item==BUY_LIGHT?"LIGHT":"HEAVY");
        return true;
    }
    if(item<0||item>=W_COUNT) return false;
    const Weapon *w=&g_weapons[item]; int slot=w->slot;
    if(u->wp[slot]==item){ snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"ALREADY OWNED"); return false; }
    int ref=u->refund[slot];
    if(u->credits+ref<price){ snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"NOT ENOUGH CREDITS"); return false; }
    u->credits+=ref-price; u->refund[slot]=price;
    give_weapon(u,item);
    snprintf(g_m.buyMsg,sizeof g_m.buyMsg,"BOUGHT %s",w->name);
    return true;
}
void game_skip_buy(void){ if(g_m.phase==PH_BUY) g_m.phaseT=0.0f; }
void game_set_cmd(int cmd){ g_m.cmd=cmd; }
void game_look(float dyaw,float dpitch){
    Unit *p=&g_m.u[0];
    p->yaw=wrapang(p->yaw+dyaw);
    p->pitch=clampf(p->pitch+dpitch,-0.55f,0.55f);
}

/* ------------------------------------------------------------------ damage / kills */
static void push_feed(int killer,int victim,int weapon,bool head,bool spike){
    memmove(&g_m.feed[1],&g_m.feed[0],sizeof(KillMsg)*5);
    g_m.feed[0]=(KillMsg){killer,victim,weapon,head,spike,6.0f};
}
static void spike_drop(Unit *u,int ui){
    if(g_m.spike==SP_CARRIED&&g_m.carrier==ui){
        g_m.spike=SP_DROPPED; g_m.spikeX=u->x; g_m.spikeY=u->y; g_m.carrier=-1;
        if(g_m.planter==ui){ g_m.planter=-1; g_m.plantProg=0; }
    }
}
static void kill_unit(int vi,int ki,int weapon,bool head,bool spikeKill){
    Unit *v=&g_m.u[vi];
    if(!v->alive) return;
    v->alive=false; v->hp=0; v->deaths++; v->scoped=false; v->ai.target=-1;
    spike_drop(v,vi);
    if(g_m.defuser==vi){ g_m.defuser=-1; g_m.defuseProg=g_m.defuseCp?DEFUSE_TIME*0.5f:0; }
    if(ki>=0&&ki!=vi){
        Unit *k=&g_m.u[ki];
        k->kills++; k->roundKills++; k->score+=200;
        if(!spikeKill){ k->credits=clampi(k->credits+200,0,CREDITS_MAX); }
        if(k->ult<ULT_MAX) k->ult+=1.0f;
        if(ki==0) g_m.killT=1.2f;
    }
    push_feed(ki,vi,weapon,head,spikeKill);
    if(vi==0){ g_m.spec=-1; for(int i=1;i<NUNITS;i++) if(g_m.u[i].squad==0&&g_m.u[i].alive){ g_m.spec=i; break; } }
}
/* raw damage with armor; returns true if the victim died */
static bool deal_damage(int vi,int ki,float dmg,int weapon,int zone,float srcx,float srcy){
    Unit *v=&g_m.u[vi];
    if(!v->alive) return false;
    int d=(int)(dmg+0.5f); if(d<1) d=1;
    if(v->armor>0){ int ab=(int)(d*0.66f+0.5f); if(ab>v->armor) ab=v->armor; v->armor-=ab; d-=ab; }
    v->hp-=d; v->hurtT=0.5f;
    v->hurtDir=atan2f(srcy-v->y,srcx-v->x);
    if(vi==0){
        int s=0; float old=1e9f; for(int i=0;i<4;i++) if(g_m.dmgInd[i]<old){ old=g_m.dmgInd[i]; s=i; }
        g_m.dmgInd[s]=1.2f; g_m.dmgIndAng[s]=wrapang(v->hurtDir-v->yaw);
    }
    if(ki==0&&vi!=0){ g_m.hitT=0.25f; g_m.hitHead=(zone==ZONE_HEAD); }
    if(v->hp<=0){ kill_unit(vi,ki,weapon,zone==ZONE_HEAD,false); return true; }
    return false;
}

/* ------------------------------------------------------------------ shooting */
static float gaussish(void){ return (rndf()+rndf()+rndf()-1.5f)*0.8165f*2.0f; }  /* ~ unit variance */

static void shoot_ray(int ai_,float yaw,float pitch,const Weapon *w,int wid){
    Unit *a=&g_m.u[ai_];
    float dx=cosf(yaw), dy=sinf(yaw), dz=tanf(pitch), ez=unit_eye(a);
    float wall=mp_trace(a->x,a->y,ez,dx,dy,dz,70.0f);
    float bestT=wall; int bestU=-1, bestZ=-1, zone=ZONE_BODY;
    float rad=(ai_==0&&g_set_aim_assist())?0.30f:0.24f;
    for(int i=0;i<NUNITS;i++){
        Unit *e=&g_m.u[i];
        if(!e->alive||e->team==a->team) continue;
        float rx=e->x-a->x, ry=e->y-a->y;
        float t=rx*dx+ry*dy; if(t<=0.1f||t>=bestT) continue;
        float lat=fabsf(rx*dy-ry*dx); if(lat>rad) continue;
        float h=(ez+dz*t-e->z)/UNIT_H;
        if(h<-0.03f||h>1.06f) continue;
        bestT=t; bestU=i; bestZ=-1; zone=(h>0.80f)?ZONE_HEAD:(h>0.38f?ZONE_BODY:ZONE_LEG);
    }
    for(int i=0;i<MAX_ZONES;i++){
        Zone *z=&g_m.zones[i];
        if(!z->active||z->type!=Z_TURRET||z->team==a->team) continue;
        float rx=z->x-a->x, ry=z->y-a->y, t=rx*dx+ry*dy;
        if(t<=0.1f||t>=bestT) continue;
        if(fabsf(rx*dy-ry*dx)>0.32f) continue;
        float h=ez+dz*t; if(h<0.0f||h>0.75f) continue;
        bestT=t; bestU=-1; bestZ=i;
    }
    if(bestU>=0){
        float dmg=(zone==ZONE_HEAD)?w->head:(zone==ZONE_LEG?w->leg:w->dmg);
        if(bestT>w->falloffStart){
            float k=clampf((bestT-w->falloffStart)/w->falloffStart,0,1);
            dmg*=1.0f+(w->falloffMult-1.0f)*k;
        }
        deal_damage(bestU,ai_,dmg,wid,zone,a->x,a->y);
    } else if(bestZ>=0){
        Zone *z=&g_m.zones[bestZ];
        z->hp-=(float)w->dmg;
        if(ai_==0){ g_m.hitT=0.25f; g_m.hitHead=false; }
        if(z->hp<=0) z->active=false;
    }
}

static void fire_weapon(int ui){
    Unit *u=&g_m.u[ui];
    const Weapon *w=unit_weapon(u); int wid=u->wp[u->cur];
    u->ammo[u->cur]--;
    float rof=w->rof*(u->buffT>0?1.3f:1.0f);
    u->fireCd+=1.0f/rof; if(u->fireCd>1.0f/rof) u->fireCd=1.0f/rof;
    float speed=sqrtf(u->vx*u->vx+u->vy*u->vy);
    float sp=w->spread;
    if(u->moving) sp+=w->moveSpread*clampf(speed/BASE_SPEED,0,1);
    if(!u->grounded) sp+=3.5f;
    if(w->zoom>1.0f&&!u->scoped) sp+=4.0f;
    if(u->stunT>0) sp+=2.5f;
    sp+=u->recoil*0.45f;
    for(int p=0;p<w->pellets;p++){
        float ang=rndf()*TAU_F, r=sqrtf(rndf())*sp*(PI_F/180.0f);
        if(w->pellets>1) r=sp*(PI_F/180.0f)*sqrtf(rndf());
        shoot_ray(ui,u->yaw+cosf(ang)*r,u->pitch+sinf(ang)*r,w,wid);
    }
    u->recoil=clampf(u->recoil+w->recoil,0,7.0f);
    u->kick+=w->recoil*(ui==0?0.9f:0.0f);
    u->muzzleT=0.07f; u->noiseT=0.6f; u->lastWeapon=wid;
    u->fireHeld=true;
    (void)gaussish;
}

/* ------------------------------------------------------------------ abilities */
bool game_cast(int ui,int slot,float ax,float ay){
    Unit *u=&g_m.u[ui];
    const Ability *a=&g_agents[u->agent].ab[slot];
    if(!u->alive||g_m.phase!=PH_LIVE) return false;
    if(slot<2){ if(u->cd[slot]>0) return false; }
    else if(u->ult<a->cost) return false;
    float fx=cosf(u->yaw), fy=sinf(u->yaw);
    switch(a->type){
    case AB_DASH: {
        float mx=u->vx, my=u->vy, m=sqrtf(mx*mx+my*my);
        if(m>1.0f){ fx=mx/m; fy=my/m; }
        u->dashT=a->range/25.0f; u->dashDx=fx; u->dashDy=fy;
    } break;
    case AB_FLASH: {
        float px=u->x+fx*1.2f, py=u->y+fy*1.2f;
        if(mp_cell((int)px,(int)py)!=C_OPEN){ px=u->x; py=u->y; }
        for(int i=0;i<NUNITS;i++){
            Unit *e=&g_m.u[i]; if(!e->alive||e->team==u->team) continue;
            float dx=px-e->x, dy=py-e->y, d=sqrtf(dx*dx+dy*dy);
            if(d>a->radius) continue;
            float dz=(e->z+EYE_H-0.6f)/(d>0.1f?d:0.1f);
            if(d>0.3f&&mp_trace(e->x,e->y,e->z+EYE_H,dx/d,dy/d,-dz,d)<d-0.3f) continue;
            float facing=cosf(wrapang(atan2f(dy,dx)-e->yaw));
            float t=facing>0.0f?a->dur:a->dur*0.3f;
            if(t>e->flashT) e->flashT=t;
        }
    } break;
    case AB_SMOKE: case AB_BIGSMOKE: zone_add(Z_SMOKE,u->team,ui,ax,ay,a->radius,a->dur); break;
    case AB_FIELD: zone_add(Z_FIELD,u->team,ui,ax,ay,a->radius,a->dur); break;
    case AB_TRAP: {
        int n=0,oldest=-1; float ot=1e9f;
        for(int i=0;i<MAX_ZONES;i++) if(g_m.zones[i].active&&g_m.zones[i].type==Z_TRAP&&g_m.zones[i].owner==ui){ n++; if(g_m.zones[i].t<ot){ ot=g_m.zones[i].t; oldest=i; } }
        if(n>=2&&oldest>=0) g_m.zones[oldest].active=false;
        Zone *z=zone_add(Z_TRAP,u->team,ui,u->x,u->y,a->radius,a->dur); z->armed=false;
    } break;
    case AB_TURRET: {
        for(int i=0;i<MAX_ZONES;i++) if(g_m.zones[i].active&&g_m.zones[i].type==Z_TURRET&&g_m.zones[i].owner==ui) g_m.zones[i].active=false;
        Zone *z=zone_add(Z_TURRET,u->team,ui,ax,ay,a->radius,a->dur); z->hp=80; z->cd=0.6f; z->yaw=u->yaw;
    } break;
    case AB_SCAN:
        for(int i=0;i<NUNITS;i++){ Unit *e=&g_m.u[i]; if(!e->alive||e->team==u->team) continue;
            if(sqf(e->x-u->x)+sqf(e->y-u->y)<=sqf(a->radius)) e->revealT=a->dur; }
        break;
    case AB_STUN:
        for(int i=0;i<NUNITS;i++){ Unit *e=&g_m.u[i]; if(!e->alive||e->team==u->team) continue;
            if(sqf(e->x-ax)+sqf(e->y-ay)<=sqf(a->radius)){ e->stunT=a->dur; e->slowT=a->dur; } }
        break;
    case AB_OVERDRIVE: {
        u->buffT=a->dur; u->reloadT=0;
        const Weapon *w=unit_weapon(u); int need=w->mag-u->ammo[u->cur]; if(need>u->res[u->cur]) need=u->res[u->cur];
        u->ammo[u->cur]+=need; u->res[u->cur]-=need;
    } break;
    case AB_HEAL:
        for(int i=0;i<NUNITS;i++){ Unit *e=&g_m.u[i]; if(!e->alive||e->squad!=u->squad) continue;
            if(sqf(e->x-u->x)+sqf(e->y-u->y)<=sqf(a->radius)){ e->hp=100; e->armor=50; } }
        break;
    case AB_REVEALALL:
        for(int i=0;i<NUNITS;i++){ Unit *e=&g_m.u[i]; if(e->alive&&e->team!=u->team) e->revealT=a->dur; }
        break;
    }
    if(slot<2) u->cd[slot]=a->cd; else u->ult=0;
    if(ui==0) game_toast(a->name);
    return true;
}

/* ------------------------------------------------------------------ unit update */
float unit_speed_mul(const Unit *u){
    const Weapon *w=unit_weapon(u);
    float m=w->moveMul;
    if(u->scoped) m*=0.6f;
    if(u->slowT>0) m*=0.5f;
    if(u->buffT>0) m*=1.2f;
    return m;
}

static void unit_move(int ui,Unit *u,const Intent *in){
    if(mp_circle_blocked(u->x,u->y,u->z,PLAYER_R-0.04f)){          /* failsafe: never stay embedded */
        float nx,ny; if(mp_nearest_open(u->x,u->y,&nx,&ny)){ u->x=nx; u->y=ny; }
    }
    float cs=cosf(u->yaw), sn=sinf(u->yaw);
    float f=clampf(in->fwd,-1,1), s=clampf(in->strafe,-1,1);
    float mag=sqrtf(f*f+s*s); if(mag>1.0f){ f/=mag; s/=mag; }
    bool busy=unit_busy_spike(ui);
    float spd=BASE_SPEED*unit_speed_mul(u)*(in->alt&&u->scoped==false&&unit_weapon(u)->zoom<=1.0f?0.55f:1.0f);
    float tvx=(cs*f-sn*s)*spd, tvy=(sn*f+cs*s)*spd;
    if(busy||u->phaseLock) { tvx=0; tvy=0; }
    if(u->dashT>0){ tvx=u->dashDx*25.0f; tvy=u->dashDy*25.0f; u->vx=tvx; u->vy=tvy; u->dashT-=DT; }
    else { float k=clampf((u->grounded?14.0f:3.0f)*DT,0,1); u->vx+=(tvx-u->vx)*k; u->vy+=(tvy-u->vy)*k; }
    float dx=u->vx*DT, dy=u->vy*DT, len=sqrtf(dx*dx+dy*dy);
    int n=(int)ceilf(len/0.15f); if(n<1) n=1;
    for(int i=0;i<n;i++){
        float sx=dx/n, sy=dy/n;
        if(!mp_circle_blocked(u->x+sx,u->y,u->z,PLAYER_R)) u->x+=sx; else u->vx*=0.2f;
        if(!mp_circle_blocked(u->x,u->y+sy,u->z,PLAYER_R)) u->y+=sy; else u->vy*=0.2f;
    }
    float speed=sqrtf(u->vx*u->vx+u->vy*u->vy);
    u->moving=speed>0.6f;
    if(u->moving&&speed>2.2f&&!in->alt&&u->noiseT<0.25f) u->noiseT=0.25f;
    /* vertical */
    if(in->jump&&u->grounded&&!busy&&!u->phaseLock){ u->vz=4.8f; u->grounded=false; }
    u->vz-=12.0f*DT; u->z+=u->vz*DT;
    float fl=mp_floor_circle(u->x,u->y,PLAYER_R-0.02f);
    if(u->z<=fl){ u->z=fl; u->vz=0; u->grounded=true; } else if(u->z>fl+0.02f) u->grounded=false;
    /* teleporters */
    if(u->tpCd>0) u->tpCd-=DT;
    else { TpLink l; if(u->grounded&&mp_tp_at((int)u->x,(int)u->y,&l)){ u->x=l.dx+0.5f; u->y=l.dy+0.5f; u->tpCd=2.5f; u->vx=u->vy=0; if(ui==0) game_toast("PORTAL"); } }
}

static void unit_update(int ui,const Intent *in){
    Unit *u=&g_m.u[ui];
    if(u->muzzleT>0) u->muzzleT-=DT;
    if(u->hurtT>0) u->hurtT-=DT;
    if(u->noiseT>0) u->noiseT-=DT;
    if(u->flashT>0) u->flashT-=DT;
    if(u->slowT>0) u->slowT-=DT;
    if(u->stunT>0) u->stunT-=DT;
    if(u->revealT>0) u->revealT-=DT;
    if(u->buffT>0) u->buffT-=DT;
    if(u->spotT>0) u->spotT-=DT;
    if(u->kick>0){ u->kick-=14.0f*DT; if(u->kick<0) u->kick=0; }
    if(!u->alive) return;
    u->phaseLock=false;
    for(int s=0;s<2;s++) if(u->cd[s]>0&&g_m.phase!=PH_BUY){ u->cd[s]-=DT; if(u->cd[s]<0) u->cd[s]=0; }
    u->recoil-=7.0f*DT; if(u->recoil<0) u->recoil=0;
    if(u->fireCd>-DT) u->fireCd-=DT;
    if(u->swapT>0) u->swapT-=DT;

    unit_move(ui,u,in);
    if(g_m.phase==PH_BUY){                      /* stay inside the spawn barrier */
        const Rect *r=&g_map.spawn[u->team];
        u->x=clampf(u->x,r->x0+0.3f,r->x1+0.7f); u->y=clampf(u->y,r->y0+0.3f,r->y1+0.7f);
    }
    const Weapon *w=unit_weapon(u);
    u->scoped=(w->zoom>1.0f&&in->alt&&u->reloadT<=0&&u->swapT<=0);

    if(in->swap&&u->swapT<=0){
        int other=1-u->cur;
        if(u->wp[other]>=0){ u->cur=other; u->swapT=0.4f; u->reloadT=0; u->scoped=false; w=unit_weapon(u); }
    }
    if(u->reloadT>0){
        u->reloadT-=DT;
        if(u->reloadT<=0){
            int need=w->mag-u->ammo[u->cur]; if(need>u->res[u->cur]) need=u->res[u->cur];
            u->ammo[u->cur]+=need; u->res[u->cur]-=need; u->reloadT=0;
        }
    } else if((in->reload||(in->fire&&u->ammo[u->cur]<=0))&&u->ammo[u->cur]<w->mag&&u->res[u->cur]>0&&u->swapT<=0&&g_m.phase!=PH_ENDROUND){
        u->reloadT=u->buffT>0?0.35f:w->reload; u->scoped=false;
    }
    if(in->fire&&g_m.phase==PH_LIVE&&u->reloadT<=0&&u->swapT<=0&&u->ammo[u->cur]>0&&u->fireCd<=0.0f&&(w->autofire||!u->fireHeld)){
        if(u->dashT<=0&&!unit_busy_spike(ui)) fire_weapon(ui);
    }
    if(!in->fire) u->fireHeld=false;
    if(in->abil>=0){
        float ax,ay; const Ability *a=&g_agents[u->agent].ab[in->abil];
        if(in->aimGiven){ ax=in->aimX; ay=in->aimY; } else game_aim_point(u,a->range>0?a->range:4.0f,&ax,&ay);
        game_cast(ui,in->abil,ax,ay);
    }
}

/* ------------------------------------------------------------------ zones */
static void update_zones(void){
    for(int i=0;i<MAX_ZONES;i++){
        Zone *z=&g_m.zones[i];
        if(!z->active) continue;
        z->t-=DT;
        if(z->t<=0){ z->active=false; continue; }
        if(z->type==Z_FIELD){
            for(int k=0;k<NUNITS;k++){ Unit *e=&g_m.u[k];
                if(e->alive&&e->team!=z->team&&sqf(e->x-z->x)+sqf(e->y-z->y)<sqf(z->r)){ if(e->slowT<0.3f) e->slowT=0.3f; } }
        } else if(z->type==Z_TRAP){
            if(!z->armed&&z->t0-z->t>1.0f) z->armed=true;
            if(!z->armed) continue;
            for(int k=0;k<NUNITS;k++){ Unit *e=&g_m.u[k];
                if(!e->alive||e->team==z->team||e->z>0.3f) continue;
                if(sqf(e->x-z->x)+sqf(e->y-z->y)<sqf(z->r)){
                    e->revealT=5.0f; e->slowT=3.0f;
                    deal_damage(k,z->owner,20.0f,-1,ZONE_BODY,z->x,z->y);
                    if(g_m.u[z->owner].squad==0) game_toast("TRIPWIRE TRIGGERED");
                    z->active=false; break;
                }
            }
        } else if(z->type==Z_TURRET){
            z->cd-=DT;
            if(z->cd>0) continue;
            int best=-1; float bd=z->r*z->r;
            for(int k=0;k<NUNITS;k++){ Unit *e=&g_m.u[k];
                if(!e->alive||e->team==z->team) continue;
                float dx=e->x-z->x, dy=e->y-z->y, d2=dx*dx+dy*dy; if(d2>=bd) continue;
                float d=sqrtf(d2); if(d<0.2f) d=0.2f;
                if(mp_trace(z->x,z->y,0.55f,dx/d,dy/d,((e->z+UNIT_H*0.6f)-0.55f)/d,d)<d-0.25f) continue;
                if(game_smoke_blocks(z->x,z->y,e->x,e->y)) continue;
                bd=d2; best=k;
            }
            if(best>=0){
                Unit *e=&g_m.u[best]; z->yaw=atan2f(e->y-z->y,e->x-z->x); z->cd=0.28f;
                if(rndf()<0.55f) deal_damage(best,z->owner,9.0f,-1,ZONE_BODY,z->x,z->y);
            } else z->cd=0.1f;
        }
    }
}

/* ------------------------------------------------------------------ spike */
static void apply_credits_cap(void){ for(int i=0;i<NUNITS;i++) g_m.u[i].credits=clampi(g_m.u[i].credits,0,CREDITS_MAX); }

static void end_round(int winSquad,const char *why){
    if(g_m.phase==PH_ENDROUND||g_m.phase==PH_MATCHOVER) return;
    g_m.phase=PH_ENDROUND; g_m.phaseT=ENDROUND_TIME;
    g_m.roundWinSquad=winSquad; snprintf(g_m.endText,sizeof g_m.endText,"%s",why);
    g_m.score[winSquad]++; g_m.roundsPlayed++;
    int loseSquad=1-winSquad;
    g_m.lossStreak[winSquad]=0; if(g_m.lossStreak[loseSquad]<4) g_m.lossStreak[loseSquad]++;
    static const int LOSS[5]={1900,1900,2400,2900,2900};
    for(int i=0;i<NUNITS;i++){
        Unit *u=&g_m.u[i];
        if(u->squad==winSquad) u->credits+=3000;
        else {
            u->credits+=LOSS[g_m.lossStreak[loseSquad]];
            if(u->team==TEAM_ATK&&(g_m.spike==SP_PLANTED||g_m.spike==SP_EXPLODED)) u->credits+=300;
        }
        if(u->ult<ULT_MAX) u->ult+=1.0f;
    }
    apply_credits_cap();
    if(winSquad==0) game_banner("ROUND WON",RGB(90,230,140)); else game_banner("ROUND LOST",RGB(255,90,90));
    if(g_m.score[winSquad]>=g_m.target){ g_m.matchWon=(winSquad==0); }
}

static void spike_explode(void){
    g_m.spike=SP_EXPLODED;
    for(int i=0;i<NUNITS;i++){
        Unit *u=&g_m.u[i]; if(!u->alive) continue;
        float d=sqrtf(sqf(u->x-g_m.spikeX)+sqf(u->y-g_m.spikeY));
        if(d<=3.5f) kill_unit(i,-1,-1,false,true);
        else if(d<=6.0f){ u->hp-=60; if(u->hp<=0) kill_unit(i,-1,-1,false,true); }
    }
    int atkSquad=(game_side_of_squad(0)==TEAM_ATK)?0:1;
    end_round(atkSquad,"SPIKE DETONATED");
}

static void spike_logic(const bool use[NUNITS]){
    /* pickup */
    if(g_m.spike==SP_DROPPED){
        for(int i=0;i<NUNITS;i++){ Unit *u=&g_m.u[i];
            if(u->alive&&u->team==TEAM_ATK&&sqf(u->x-g_m.spikeX)+sqf(u->y-g_m.spikeY)<0.8f){
                g_m.spike=SP_CARRIED; g_m.carrier=i; if(u->squad==0) game_toast("SPIKE PICKED UP"); break; }
        }
    }
    /* plant */
    if(g_m.spike==SP_CARRIED&&g_m.carrier>=0){
        Unit *c=&g_m.u[g_m.carrier]; int ci=g_m.carrier;
        bool ok=c->alive&&use[ci]&&c->grounded&&mp_site_at(c->x,c->y)>0&&!c->moving&&c->dashT<=0;
        if(ok){
            g_m.planter=ci; g_m.plantProg+=DT; c->phaseLock=true;
            if(g_m.plantProg>=PLANT_TIME){
                g_m.spike=SP_PLANTED; g_m.spikeX=c->x; g_m.spikeY=c->y; g_m.fuseT=SPIKE_FUSE;
                g_m.plantSite=mp_site_at(c->x,c->y); g_m.planter=-1; g_m.plantProg=0; g_m.carrier=-1;
                g_m.defuseProg=0; g_m.defuseCp=false;
                if(c->ult<ULT_MAX) c->ult+=1.0f;
                char b[40]; snprintf(b,sizeof b,"SPIKE PLANTED - SITE %c",'A'+g_m.plantSite-1);
                game_banner(b,RGB(255,200,70));
                ai_on_event_plant();
            }
        } else { g_m.planter=-1; g_m.plantProg=0; }
    }
    /* planted: fuse + defuse */
    if(g_m.spike==SP_PLANTED){
        g_m.fuseT-=DT;
        int who=-1;
        for(int i=0;i<NUNITS;i++){ Unit *u=&g_m.u[i];
            if(u->alive&&u->team==TEAM_DEF&&use[i]&&u->grounded&&!u->moving&&sqf(u->x-g_m.spikeX)+sqf(u->y-g_m.spikeY)<1.6f*1.6f){ who=i; break; } }
        if(who>=0){
            g_m.defuser=who; g_m.defuseProg+=DT; g_m.u[who].phaseLock=true;
            if(g_m.defuseProg>=DEFUSE_TIME*0.5f) g_m.defuseCp=true;
            if(g_m.defuseProg>=DEFUSE_TIME){
                g_m.spike=SP_DEFUSED; g_m.defuser=-1;
                if(g_m.u[who].ult<ULT_MAX) g_m.u[who].ult+=1.0f;
                int defSquad=(game_side_of_squad(0)==TEAM_DEF)?0:1;
                end_round(defSquad,"SPIKE DEFUSED"); return;
            }
        } else { g_m.defuser=-1; g_m.defuseProg=g_m.defuseCp?DEFUSE_TIME*0.5f:0.0f; }
        if(g_m.fuseT<=0){ spike_explode(); }
    }
}

/* ------------------------------------------------------------------ rounds */
static void spawn_units(void){
    for(int team=0;team<2;team++){
        float px[5],py[5]; int n=0;
        for(int i=0;i<NUNITS;i++){
            Unit *u=&g_m.u[i]; if(u->team!=team) continue;
            float x=0,y=0; bool ok=false;
            for(int tries=0;tries<200&&!ok;tries++){
                mp_rand_open_in(&g_map.spawn[team],&x,&y); ok=true;
                for(int k=0;k<n;k++) if(sqf(px[k]-x)+sqf(py[k]-y)<1.1f) ok=false;
            }
            px[n]=x; py[n]=y; n++;
            u->x=x; u->y=y; u->z=0; u->vz=0; u->vx=u->vy=0; u->grounded=true;
            u->yaw=atan2f(MAP_H*0.5f-y,MAP_W*0.5f-x); u->pitch=0;
        }
    }
}

static void start_round(void){
    g_m.round++;
    bool swapNow=(g_m.roundsPlayed==g_m.half&&g_m.half>0);
    if(swapNow) { game_banner("SIDES SWITCHED",RGB(255,255,255)); g_m.lossStreak[0]=g_m.lossStreak[1]=0; }
    for(int i=0;i<NUNITS;i++){
        Unit *u=&g_m.u[i];
        u->team=game_side_of_squad(u->squad);
        if(swapNow){ u->credits=CREDITS_START; reset_loadout(u); u->ult=0; }
        else if(!u->alive){ reset_loadout(u); }
        else { for(int s=0;s<2;s++) if(u->wp[s]>=0){ u->ammo[s]=g_weapons[u->wp[s]].mag; u->res[s]=g_weapons[u->wp[s]].reserve; } }
        u->alive=true; u->hp=100; u->roundKills=0;
        u->cd[0]=u->cd[1]=0; u->dashT=u->flashT=u->slowT=u->stunT=u->revealT=u->buffT=u->tpCd=u->hurtT=u->noiseT=0;
        u->reloadT=0; u->fireCd=0; u->recoil=0; u->kick=0; u->scoped=false; u->fireHeld=false; u->swapT=0.2f;
        u->refund[0]=u->refund[1]=u->refund[2]=0; u->cur=(u->wp[1]>=0)?1:0;
        memset(&u->ai,0,sizeof u->ai); u->ai.target=-1;
    }
    spawn_units();
    zone_clear();
    g_m.phase=PH_BUY; g_m.phaseT=BUY_TIME; g_m.roundT=ROUND_TIME;
    g_m.spike=SP_CARRIED; g_m.planter=g_m.defuser=-1; g_m.plantProg=g_m.defuseProg=0; g_m.defuseCp=false; g_m.fuseT=0; g_m.plantSite=0;
    int atk[NUNITS],na=0; for(int i=0;i<NUNITS;i++) if(g_m.u[i].team==TEAM_ATK) atk[na++]=i;
    g_m.carrier=atk[rndi(na)];
    g_m.cmd=CMD_AUTO; g_m.spec=-1; g_m.buyMsg[0]=0;
    memset(g_m.feed,0,sizeof g_m.feed); memset(g_m.dmgInd,0,sizeof g_m.dmgInd);
    g_m.hitT=g_m.killT=0;
    char b[40]; snprintf(b,sizeof b,"ROUND %d - %s",g_m.round,g_m.u[0].team==TEAM_ATK?"ATTACK":"DEFEND");
    if(!swapNow) game_banner(b,RGB(255,255,255));
    ai_round_start();
}

void game_new_match(int map,int target,int diff,int startSide,int agent){
    memset(&g_m,0,sizeof g_m);
    g_m.mapId=clampi(map,0,M_COUNT-1); g_m.target=clampi(target,1,13); g_m.diff=clampi(diff,0,2);
    if(startSide==2) startSide=rndi(2);
    g_m.startSide=startSide; g_m.playerAgent=clampi(agent,0,A_COUNT-1);
    g_m.half=g_m.target-1; if(g_m.half<1) g_m.half=1;
    mp_load(g_m.mapId);
    static const char *NAMES[NUNITS]={"YOU","KITE","MOSS","ORCA","PIKE","RUNE","TALON","VEGA","WREN","ZEAL"};
    int pool[A_COUNT]; for(int i=0;i<A_COUNT;i++) pool[i]=i;
    for(int i=0;i<NUNITS;i++){
        Unit *u=&g_m.u[i];
        u->squad=(i<5)?0:1; u->isPlayer=(i==0);
        u->agent=(i==0)?g_m.playerAgent:pool[rndi(A_COUNT)];
        snprintf(u->name,sizeof u->name,"%s",NAMES[i]);
        u->credits=CREDITS_START; u->alive=true; reset_loadout(u); u->ult=0;
    }
    g_m.round=0; g_m.roundsPlayed=0;
    start_round();
}

/* ------------------------------------------------------------------ main tick */
static void update_spotting(void){
    for(int i=0;i<NUNITS;i++){ Unit *e=&g_m.u[i]; if(e->squad==0) { e->spotted=true; continue; }
        bool seen=e->revealT>0;
        if(!seen&&e->alive) for(int k=0;k<NUNITS&&!seen;k++){ const Unit *a=&g_m.u[k];
            if(a->squad==0&&a->alive&&sqf(a->x-e->x)+sqf(a->y-e->y)<30.0f*30.0f&&game_can_see(a,e,true)) seen=true; }
        if(seen) e->spotT=0.8f;
        e->spotted=e->spotT>0;
    }
}

void game_tick(const Intent *pin){
    if(g_m.phase==PH_MATCHOVER) return;
    g_m.tick++;
    if(g_m.hitT>0) g_m.hitT-=DT;
    if(g_m.killT>0) g_m.killT-=DT;
    if(g_m.bannerT>0) g_m.bannerT-=DT;
    if(g_m.toastT>0) g_m.toastT-=DT;
    for(int i=0;i<4;i++) if(g_m.dmgInd[i]>0) g_m.dmgInd[i]-=DT;
    for(int i=0;i<6;i++) if(g_m.feed[i].t>0) g_m.feed[i].t-=DT;

    bool use[NUNITS]; memset(use,0,sizeof use);
    for(int i=0;i<NUNITS;i++){
        Intent in; memset(&in,0,sizeof in); in.abil=-1;
        Unit *u=&g_m.u[i];
        if(u->isPlayer){ if(pin&&u->alive) in=*pin; }
        else if(u->alive&&g_m.phase!=PH_MATCHOVER&&!g_test_freeze_ai) ai_think(i,&in);
        if(g_m.phase==PH_ENDROUND){ in.fire=false; in.abil=-1; in.use=false; }
        use[i]=in.use;
        unit_update(i,&in);
    }
    /* soft separation so units do not stack */
    for(int i=0;i<NUNITS;i++) for(int j=i+1;j<NUNITS;j++){
        Unit *a=&g_m.u[i],*b=&g_m.u[j]; if(!a->alive||!b->alive) continue;
        float dx=b->x-a->x, dy=b->y-a->y, d2=dx*dx+dy*dy;
        if(d2<0.36f&&d2>1e-6f&&fabsf(a->z-b->z)<0.6f){
            float d=sqrtf(d2), push=(0.6f-d)*0.25f, nx=dx/d*push, ny=dy/d*push;
            if(!mp_circle_blocked(a->x-nx,a->y-ny,a->z,PLAYER_R)){ a->x-=nx; a->y-=ny; }
            if(!mp_circle_blocked(b->x+nx,b->y+ny,b->z,PLAYER_R)){ b->x+=nx; b->y+=ny; }
        }
    }
    update_zones();
    update_spotting();

    switch(g_m.phase){
    case PH_BUY:
        g_m.phaseT-=DT;
        if(g_m.phaseT<=0){ g_m.phase=PH_LIVE; g_m.phaseT=0; game_banner("FIGHT!",RGB(255,255,255)); }
        break;
    case PH_LIVE: {
        if(g_m.spike!=SP_PLANTED) g_m.roundT-=DT;
        spike_logic(use);
        if(g_m.phase!=PH_LIVE) break;
        int aa=game_alive(TEAM_ATK), da=game_alive(TEAM_DEF);
        int atkSquad=(game_side_of_squad(0)==TEAM_ATK)?0:1, defSquad=1-atkSquad;
        if(g_m.spike==SP_PLANTED){ if(da==0) end_round(atkSquad,"DEFENDERS ELIMINATED"); }
        else if(aa==0) end_round(defSquad,"ATTACKERS ELIMINATED");
        else if(da==0) end_round(atkSquad,"DEFENDERS ELIMINATED");
        else if(g_m.roundT<=0) end_round(defSquad,"TIME EXPIRED");
    } break;
    case PH_ENDROUND:
        g_m.phaseT-=DT;
        if(g_m.phaseT<=0){
            if(g_m.score[0]>=g_m.target||g_m.score[1]>=g_m.target){ g_m.phase=PH_MATCHOVER; g_m.matchWon=(g_m.score[0]>=g_m.target); }
            else start_round();
        }
        break;
    default: break;
    }
}
