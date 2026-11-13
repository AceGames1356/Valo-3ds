/* Deterministic rule tests: weapons, armor, spike, economy, abilities, movement. */
#include <stdio.h>
#include <stdlib.h>
#include "../source/game.h"
#include "../source/settings.h"

extern bool g_test_freeze_ai;
static int fails, checks;
#define CHECK(c,...) do{ checks++; if(!(c)){ printf("FAIL line %d: ",__LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

static Intent idle(void){ Intent i; memset(&i,0,sizeof i); i.abil=-1; return i; }
static void tick(Intent in){ game_tick(&in); }
static void ticks(Intent in,int n){ for(int i=0;i<n;i++) game_tick(&in); }
static void place(int ui,float x,float y,float yaw){ Unit *u=&g_m.u[ui]; u->x=x;u->y=y;u->z=0;u->vx=u->vy=u->vz=0;u->yaw=yaw;u->pitch=0;u->grounded=true; }
static void fresh(int map,int side){
    g_rng=1234567; game_new_match(map,4,1,side,0); g_test_freeze_ai=true;
    g_m.phase=PH_LIVE; g_m.phaseT=0; g_m.roundT=ROUND_TIME;
    for(int i=0;i<NUNITS;i++){ g_m.u[i].fireCd=0; g_m.u[i].swapT=0; }
}
static void give(int ui,int w){
    Unit *u=&g_m.u[ui]; const Weapon *d=&g_weapons[w];
    u->wp[d->slot]=w; u->ammo[d->slot]=d->mag; u->res[d->slot]=d->reserve; u->cur=d->slot; u->fireCd=0; u->swapT=0; u->reloadT=0; u->recoil=0; u->fireHeld=false;
}
static void aim_at(int from,int to,float h){
    Unit *a=&g_m.u[from],*e=&g_m.u[to];
    float dx=e->x-a->x,dy=e->y-a->y,d=sqrtf(dx*dx+dy*dy);
    a->yaw=atan2f(dy,dx); a->pitch=atan2f(e->z+h*UNIT_H-unit_eye(a),d);
}

int main(void){
    settings_defaults(); g_set.aimAssist=0;

    /* ---------- weapon damage, zones, rate, reload ---------- */
    for(int w=0;w<W_COUNT;w++){
        const Weapon *wp=&g_weapons[w];
        float dist=(wp->type==WT_SHOTGUN)?2.0f:5.0f;
        struct { float h; int expect; const char *n; } Z[3]={{0.55f,wp->dmg,"body"},{0.20f,wp->leg,"leg"},{0.93f,wp->head,"head"}};
        for(int z=0;z<3;z++){
            fresh(0,0);
            place(0,18.5f,8.5f,0); place(5,18.5f+dist,8.5f,0); g_m.u[5].armor=0; g_m.u[5].hp=100;
            give(0,w); aim_at(0,5,Z[z].h);
            Intent in=idle(); in.fire=true; tick(in);
            int lost=100-g_m.u[5].hp; if(!g_m.u[5].alive) lost=100;
            int exp=Z[z].expect*(wp->pellets>1?wp->pellets:1); if(exp>100) exp=100;
            if(wp->pellets>1) CHECK(lost>=Z[z].expect&&(lost<=exp||!g_m.u[5].alive),"%s %s: lost %d (pellet %d)",wp->name,Z[z].n,lost,Z[z].expect);
            else CHECK(lost==exp,"%s %s: lost %d expected %d",wp->name,Z[z].n,lost,exp);
            CHECK(g_m.u[0].ammo[wp->slot]==wp->mag-1,"%s ammo not consumed",wp->name);
        }
        /* fire rate over 1 second (re-pressing for semi-auto) */
        fresh(0,0); place(0,18.5f,8.5f,3.14159f); give(0,w); g_m.u[0].res[wp->slot]=0;
        int mag0=g_m.u[0].ammo[wp->slot]; int fired=0, lastA=mag0;
        for(int t=0;t<30;t++){ Intent in=idle(); in.fire=wp->autofire?true:(t%2==0); tick(in); if(g_m.u[0].ammo[wp->slot]<lastA){ fired+=lastA-g_m.u[0].ammo[wp->slot]; lastA=g_m.u[0].ammo[wp->slot]; } if(g_m.u[0].ammo[wp->slot]==0) break; }
        float exprate=wp->rof; if(exprate>mag0) exprate=(float)mag0;
        if(g_m.u[0].ammo[wp->slot]>0) CHECK(fired>=(int)(exprate-2.6f)&&fired<=(int)(exprate+1.6f),"%s fired %d in 1s, rof %.2f",wp->name,fired,wp->rof);
        /* semi-auto must not fire while fire is held */
        if(!wp->autofire){ fresh(0,0); place(0,18.5f,8.5f,3.14159f); give(0,w); Intent in=idle(); in.fire=true; ticks(in,30);
            CHECK(g_m.u[0].ammo[wp->slot]==wp->mag-1,"%s semi-auto fired %d while held",wp->name,wp->mag-g_m.u[0].ammo[wp->slot]); }
        /* reload takes reload time and refills from reserve */
        fresh(0,0); place(0,18.5f,8.5f,3.14159f); give(0,w); g_m.u[0].ammo[wp->slot]=0;
        Intent rl=idle(); rl.reload=true; tick(rl);
        int n=0; while(g_m.u[0].reloadT>0&&n<400){ tick(idle()); n++; }
        float secs=n*DT; CHECK(fabsf(secs-wp->reload)<0.12f,"%s reload %.2fs expected %.2f",wp->name,secs,wp->reload);
        int expAmmo=wp->mag<wp->reserve?wp->mag:wp->reserve; CHECK(g_m.u[0].ammo[wp->slot]==expAmmo,"%s reload ammo %d",wp->name,g_m.u[0].ammo[wp->slot]);
    }
    /* damage falloff */
    fresh(0,0); place(0,16.5f,8.5f,0); place(5,16.5f+30.0f,8.5f,0); give(0,W_STINGER); aim_at(0,5,0.55f); { Intent in=idle(); in.fire=true; tick(in); }
    CHECK(g_m.u[5].hp>100-27&&g_m.u[5].hp<=100,"falloff not applied (hp %d)",g_m.u[5].hp);
    /* armor: light shield absorbs up to 2/3 of damage */
    fresh(0,0); place(0,18.5f,8.5f,0); place(5,23.5f,8.5f,0); g_m.u[5].armor=25; give(0,W_VANDAL); aim_at(0,5,0.55f); { Intent in=idle(); in.fire=true; tick(in); }
    CHECK(g_m.u[5].armor==0&&g_m.u[5].hp==85,"armor absorb wrong hp %d armor %d",g_m.u[5].hp,g_m.u[5].armor);
    /* sniper can be scoped, cannot hit through a wall */
    fresh(0,0); place(0,18.5f,8.5f,0); place(5,40.5f,40.5f,0); give(0,W_OPERATOR); aim_at(0,5,0.55f); { Intent in=idle(); in.fire=true; tick(in); }
    CHECK(g_m.u[5].hp==100,"shot through walls");
    fresh(0,0); place(0,18.5f,8.5f,0); give(0,W_OPERATOR); { Intent in=idle(); in.alt=true; tick(in); CHECK(g_m.u[0].scoped,"operator not scoped"); }

    /* ---------- buy rules ---------- */
    fresh(0,0); g_m.phase=PH_BUY; g_m.phaseT=10; g_m.u[0].credits=3000;
    CHECK(game_buy(0,W_VANDAL)&&g_m.u[0].credits==100&&g_m.u[0].wp[1]==W_VANDAL,"vandal purchase failed");
    CHECK(!game_buy(0,W_OPERATOR),"bought operator without credits");
    CHECK(!game_buy(0,W_VANDAL),"double purchase allowed");
    CHECK(game_buy(0,W_CLASSIC)==false,"owned classic purchase allowed");
    g_m.phase=PH_LIVE; CHECK(!game_buy(0,W_PHANTOM),"bought outside buy phase");

    /* ---------- spike: plant, interrupt, explode, defuse, checkpoint ---------- */
    fresh(0,0);                                     /* player attacks */
    CHECK(g_m.u[0].team==TEAM_ATK,"player not attacking");
    g_m.carrier=0; g_m.spike=SP_CARRIED;
    place(0,g_map.site[0].cx,g_map.site[0].cy,0);
    Intent use=idle(); use.use=true;
    ticks(use,(int)(2.0f/DT)); CHECK(g_m.spike==SP_CARRIED&&g_m.plantProg>1.5f,"plant progress %.2f",g_m.plantProg);
    ticks(idle(),2); CHECK(g_m.plantProg==0,"plant not reset on release");
    ticks(use,(int)(PLANT_TIME/DT)+3); CHECK(g_m.spike==SP_PLANTED&&g_m.plantSite==1,"spike not planted (state %d)",g_m.spike);
    CHECK(game_alive(TEAM_DEF)==5,"defenders died?");
    /* cannot plant outside a site */
    fresh(0,0); g_m.carrier=0; place(0,23.5f,38.5f,0); ticks(use,(int)(PLANT_TIME/DT)+10); CHECK(g_m.spike==SP_CARRIED,"planted outside site");
    /* explosion ends the round for attackers and kills nearby units */
    fresh(0,0); g_m.carrier=0; place(0,g_map.site[0].cx,g_map.site[0].cy,0); ticks(use,(int)(PLANT_TIME/DT)+3);
    place(0,g_map.site[0].cx-12,g_map.site[0].cy+4,0); if(mp_cell((int)g_m.u[0].x,(int)g_m.u[0].y)!=C_OPEN) place(0,g_map.spawn[0].cx,g_map.spawn[0].cy,0);
    place(5,g_m.spikeX+0.5f,g_m.spikeY,0); int sc0=g_m.score[0];
    g_m.fuseT=0.05f; ticks(idle(),3);
    CHECK(g_m.spike==SP_EXPLODED&&g_m.phase==PH_ENDROUND,"spike did not detonate (spike %d phase %d)",g_m.spike,g_m.phase);
    CHECK(!g_m.u[5].alive,"unit at spike survived explosion"); CHECK(g_m.score[0]==sc0+1,"attackers not awarded round");
    /* defuse with checkpoint (player defends) */
    fresh(0,1); CHECK(g_m.u[0].team==TEAM_DEF,"player not defending");
    g_m.spike=SP_PLANTED; g_m.spikeX=g_map.site[0].cx; g_m.spikeY=g_map.site[0].cy; g_m.fuseT=40; g_m.plantSite=1; g_m.carrier=-1;
    place(0,g_m.spikeX+0.7f,g_m.spikeY,3.14159f); if(mp_cell((int)g_m.u[0].x,(int)g_m.u[0].y)!=C_OPEN) place(0,g_m.spikeX-0.7f,g_m.spikeY,0);
    ticks(use,(int)(2.0f/DT)); float p1=g_m.defuseProg; CHECK(p1>1.5f&&g_m.spike==SP_PLANTED,"defuse progress %.2f",p1);
    ticks(idle(),3); CHECK(g_m.defuseProg==0,"defuse should reset before checkpoint (%.2f)",g_m.defuseProg);
    ticks(use,(int)(4.0f/DT)); ticks(idle(),3); CHECK(fabsf(g_m.defuseProg-DEFUSE_TIME*0.5f)<0.01f,"checkpoint not kept (%.2f)",g_m.defuseProg);
    ticks(use,(int)(3.8f/DT)); CHECK(g_m.spike==SP_DEFUSED&&g_m.phase==PH_ENDROUND,"spike not defused (spike %d phase %d prog %.2f)",g_m.spike,g_m.phase,g_m.defuseProg);
    CHECK(g_m.roundWinSquad==0,"defusers did not win");
    /* spike drop + pickup */
    fresh(0,0); g_m.carrier=1; Unit *c=&g_m.u[1]; place(1,18.5f,8.5f,0); c->hp=1; place(5,23.5f,8.5f,3.14159f); give(5,W_VANDAL); aim_at(5,1,0.55f);
    { Intent in=idle(); in.fire=true; for(int i=0;i<5&&c->alive;i++) game_tick(&in); }
    /* unit 5 is an enemy: it is driven by pin (isPlayer false => idle intent) so shoot directly via damage path instead */
    fresh(0,0); g_m.carrier=0; place(0,18.5f,8.5f,0); place(5,23.5f,8.5f,3.14159f); g_m.u[5].isPlayer=true; give(5,W_VANDAL); aim_at(5,0,0.55f); g_m.u[0].hp=1;
    { Intent in=idle(); in.fire=true; tick(in); }
    CHECK(!g_m.u[0].alive,"carrier survived lethal shot"); CHECK(g_m.spike==SP_DROPPED,"spike not dropped on death (%d)",g_m.spike);
    place(1,g_m.spikeX,g_m.spikeY,0); g_m.u[5].isPlayer=false; tick(idle()); CHECK(g_m.spike==SP_CARRIED&&g_m.carrier==1,"spike not picked up by ally");

    /* ---------- round outcomes & economy ---------- */
    fresh(0,0); for(int i=5;i<10;i++){ g_m.u[i].alive=false; } g_m.u[0].credits=g_m.u[5].credits=1000; tick(idle());
    CHECK(g_m.phase==PH_ENDROUND&&g_m.roundWinSquad==0,"elimination win not detected");
    CHECK(g_m.u[0].credits==4000&&g_m.u[5].credits==2900,"credits win/lose: %d / %d",g_m.u[0].credits,g_m.u[5].credits);
    fresh(0,0); for(int i=0;i<5;i++){ g_m.u[i].alive=false; } g_m.u[0].credits=1000; tick(idle());
    CHECK(g_m.roundWinSquad==1&&g_m.u[0].credits==2900,"loss bonus %d",g_m.u[0].credits);
    fresh(0,0); g_m.roundT=0.01f; ticks(idle(),2); CHECK(g_m.phase==PH_ENDROUND&&g_m.roundWinSquad==1&&!strcmp(g_m.endText,"TIME EXPIRED"),"time expiry should favour defenders");
    fresh(0,0); g_m.u[0].credits=8900; g_m.u[0].alive=true; for(int i=5;i<10;i++) g_m.u[i].alive=false; tick(idle()); CHECK(g_m.u[0].credits==CREDITS_MAX,"credit cap %d",g_m.u[0].credits);
    /* kill reward */
    fresh(0,0); place(0,18.5f,8.5f,0); place(5,23.5f,8.5f,0); g_m.u[5].hp=10; g_m.u[0].credits=0; give(0,W_VANDAL); aim_at(0,5,0.55f); { Intent in=idle(); in.fire=true; tick(in); }
    CHECK(!g_m.u[5].alive&&g_m.u[0].credits==200&&g_m.u[0].kills==1,"kill reward %d",g_m.u[0].credits);

    /* ---------- half-time swap and match end ---------- */
    g_rng=99; game_new_match(0,4,1,0,0); g_test_freeze_ai=false; int sideBefore=g_m.u[0].team;
    for(int r=0;r<3;r++){
        g_m.phase=PH_LIVE; for(int i=5;i<10;i++) g_m.u[i].alive=false; tick(idle());
        CHECK(g_m.phase==PH_ENDROUND,"round %d did not end",r);
        ticks(idle(),(int)(ENDROUND_TIME/DT)+3);
    }
    CHECK(g_m.round==4&&g_m.score[0]==3,"round/score after 3 wins: %d / %d",g_m.round,g_m.score[0]);
    CHECK(g_m.u[0].team!=sideBefore,"sides did not switch at half-time");
    CHECK(g_m.u[0].credits==CREDITS_START&&g_m.u[5].credits<=CREDITS_START,"half-time credit reset %d / %d",g_m.u[0].credits,g_m.u[5].credits);
    CHECK(g_m.u[0].wp[1]==-1,"loadout not reset at half-time");
    g_m.phase=PH_LIVE; for(int i=5;i<10;i++) g_m.u[i].alive=false; tick(idle()); ticks(idle(),(int)(ENDROUND_TIME/DT)+3);
    CHECK(g_m.phase==PH_MATCHOVER&&g_m.matchWon&&g_m.score[0]==4,"match did not end with a win (phase %d)",g_m.phase);

    /* ---------- abilities (every type) ---------- */
    fresh(0,0); g_m.phase=PH_LIVE;
    { /* EMBER dash + flash + overdrive */
        g_m.u[0].agent=0; place(0,18.5f,8.5f,0); Intent in=idle(); in.abil=0; tick(in); ticks(idle(),8);
        CHECK(g_m.u[0].x>21.0f,"dash moved only to %.2f",g_m.u[0].x); CHECK(g_m.u[0].cd[0]>0,"dash cooldown not set");
        place(0,18.5f,8.5f,0); place(5,24.5f,8.5f,3.14159f); in=idle(); in.abil=1; tick(in);
        CHECK(g_m.u[5].flashT>1.0f,"flash did not blind enemy facing it (%.2f)",g_m.u[5].flashT);
        CHECK(!game_can_see(&g_m.u[5],&g_m.u[0],false),"blinded enemy can still see");
        g_m.u[0].ult=6; give(0,W_VANDAL); g_m.u[0].ammo[1]=3; in=idle(); in.abil=2; tick(in);
        CHECK(g_m.u[0].buffT>5&&g_m.u[0].ammo[1]==25&&g_m.u[0].ult==0,"overdrive failed");
    }
    fresh(0,0);
    { /* VEIL smoke blocks vision, field slows, eclipse */
        g_m.u[0].agent=1; place(0,18.5f,8.5f,0); place(5,26.5f,8.5f,3.14159f); place(6,34.0f,20.0f,0);
        CHECK(game_can_see(&g_m.u[0],&g_m.u[5],true),"no LOS without smoke");
        Intent in=idle(); in.abil=0; in.aimGiven=true; in.aimX=22.5f; in.aimY=8.5f; tick(in);
        CHECK(!game_can_see(&g_m.u[0],&g_m.u[5],true),"smoke does not block vision");
        in=idle(); in.abil=1; in.aimGiven=true; in.aimX=26.5f; in.aimY=8.5f; tick(in); ticks(idle(),2);
        CHECK(g_m.u[5].slowT>0,"field did not slow enemy");
        g_m.u[0].ult=7; in=idle(); in.abil=2; in.aimGiven=true; in.aimX=29.5f; in.aimY=8.5f; tick(in);
        int big=0; for(int i=0;i<MAX_ZONES;i++) if(g_m.zones[i].active&&g_m.zones[i].type==Z_SMOKE&&g_m.zones[i].r>3.0f) big=1; CHECK(big,"eclipse smoke missing");
    }
    fresh(0,0);
    { /* BASTION tripwire + turret + heal */
        g_m.u[0].agent=2; place(0,18.5f,8.5f,0); place(5,30.5f,8.5f,3.14159f);
        Intent in=idle(); in.abil=0; tick(in); ticks(idle(),35);                         /* arm */
        place(5,18.5f,8.5f,0); g_m.u[5].hp=100; g_m.u[5].armor=0; place(0,30.5f,8.5f,3.14159f); tick(idle());
        CHECK(g_m.u[5].hp==80&&g_m.u[5].revealT>3,"tripwire did not trigger (hp %d)",g_m.u[5].hp);
        place(0,18.5f,8.5f,0); in=idle(); in.abil=1; in.aimGiven=true; in.aimX=20.5f; in.aimY=8.5f; tick(in);
        for(int i=5;i<10;i++){ g_m.u[i].hp=100; g_m.u[i].armor=0; } place(5,26.5f,8.5f,0); ticks(idle(),90);
        { int dmg=0; for(int i=5;i<10;i++) dmg+=100-(g_m.u[i].alive?g_m.u[i].hp:0); CHECK(dmg>0,"turret never damaged an enemy"); }
        g_m.u[0].ult=7; g_m.u[0].hp=30; g_m.u[1].hp=40; place(1,19.5f,8.5f,0); in=idle(); in.abil=2; tick(in);
        CHECK(g_m.u[0].hp==100&&g_m.u[1].hp==100&&g_m.u[0].armor==50,"heal ult failed");
    }
    fresh(0,0);
    { /* RADAR scan, stun, sweep */
        g_m.u[0].agent=3; place(0,18.5f,8.5f,0); place(5,28.5f,8.5f,0); place(6,36.0f,20.0f,0);
        Intent in=idle(); in.abil=0; tick(in); CHECK(g_m.u[5].revealT>2,"scan did not reveal");
        in=idle(); in.abil=1; in.aimGiven=true; in.aimX=28.5f; in.aimY=8.5f; tick(in); CHECK(g_m.u[5].stunT>1,"stun failed");
        g_m.u[0].ult=6; in=idle(); in.abil=2; tick(in); CHECK(g_m.u[6].revealT>4&&g_m.u[9].revealT>4,"sweep did not reveal everyone");
        g_m.u[0].ult=3; in=idle(); in.abil=2; tick(in); CHECK(g_m.u[0].ult==3,"ultimate fired without enough points");
    }
    /* cooldown gating */
    fresh(0,0); g_m.u[0].agent=1; { Intent in=idle(); in.abil=0; in.aimGiven=true; in.aimX=22.5f; in.aimY=8.5f; place(0,18.5f,8.5f,0); tick(in); int n=0; for(int i=0;i<MAX_ZONES;i++) n+=g_m.zones[i].active; tick(in); int m=0; for(int i=0;i<MAX_ZONES;i++) m+=g_m.zones[i].active; CHECK(n==1&&m==1,"cooldown not enforced (%d,%d)",n,m); }

    /* ---------- movement: low cover, jumping, portals ---------- */
    fresh(0,0);
    { /* crate at (38..39, 12..13) in Citadel site B */
        CHECK(mp_cell(38,12)==C_CRATE,"expected crate");
        place(0,36.8f,12.5f,0); Intent in=idle(); in.fwd=1; ticks(in,40); CHECK(g_m.u[0].x<37.9f,"walked through crate (x=%.2f)",g_m.u[0].x);
        place(0,36.8f,12.5f,0); in=idle(); in.fwd=1; in.jump=true; ticks(in,60);
        CHECK(g_m.u[0].x>38.2f,"could not jump onto crate (x=%.2f)",g_m.u[0].x);
        place(0,12.5f,9.5f,0); in=idle(); in.fwd=1; ticks(in,10); CHECK(g_m.u[0].x>13.0f,"basic movement failed (x=%.2f)",g_m.u[0].x);
        place(0,24.0f,8.5f,0); in=idle(); in.jump=true; tick(in); CHECK(g_m.u[0].z>0&&!g_m.u[0].grounded,"jump did not leave ground"); ticks(idle(),40); CHECK(g_m.u[0].grounded&&g_m.u[0].z==0,"did not land");
    }
    fresh(2,0);
    { /* Portals */
        CHECK(g_map.tpCount==2,"portal map has no portals"); TpLink l=g_map.tp[0];
        place(0,l.x-1.5f+0.5f,l.y+0.5f,0); Intent in=idle(); in.fwd=1; ticks(in,25);
        CHECK(fabsf(g_m.u[0].x-(l.dx+0.5f))<2.5f&&fabsf(g_m.u[0].y-(l.dy+0.5f))<2.5f,"portal did not teleport (%.1f,%.1f) expected near (%d,%d)",g_m.u[0].x,g_m.u[0].y,l.dx,l.dy);
    }
    /* spectate / dead player no longer shoots or moves */
    fresh(0,0); g_m.u[0].alive=false; { Intent in=idle(); in.fire=true; in.fwd=1; float x=g_m.u[0].x; ticks(in,10); CHECK(g_m.u[0].x==x,"dead unit moved"); CHECK(game_view_unit()!=&g_m.u[0],"dead player not spectating"); }

    printf("%d checks, %d failures\n",checks,fails);
    printf(fails?"RULES TEST FAILED\n":"RULES TEST OK\n");
    return fails?1:0;
}
