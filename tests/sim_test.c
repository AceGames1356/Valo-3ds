/* Headless simulation test: bots play both sides (unit 0 is put under AI control),
   many full matches on every map/difficulty, with invariant checks every tick. */
#include <stdio.h>
#include <stdlib.h>
#include "../source/game.h"
#include "../source/settings.h"

static int fails;
#define CHECK(c,...) do{ if(!(c)){ if(fails<15){ printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } fails++; } }while(0)

static void invariants(void){
    for(int i=0;i<NUNITS;i++){
        Unit *u=&g_m.u[i];
        CHECK(isfinite(u->x)&&isfinite(u->y)&&isfinite(u->z)&&isfinite(u->yaw)&&isfinite(u->pitch),"unit %d non-finite",i);
        if(u->alive){
            CHECK(u->hp>0&&u->hp<=100,"unit %d hp %d",i,u->hp);
            CHECK(!mp_circle_blocked(u->x,u->y,u->z,0.15f)||u->dashT>0,"unit %d inside geometry at %.2f %.2f z%.2f",i,u->x,u->y,u->z);
            CHECK(u->x>0&&u->x<MAP_W&&u->y>0&&u->y<MAP_H,"unit %d off map",i);
        }
        CHECK(u->armor>=0&&u->armor<=50,"unit %d armor %d",i,u->armor);
        CHECK(u->credits>=0&&u->credits<=CREDITS_MAX,"unit %d credits %d",i,u->credits);
        for(int s=0;s<2;s++){ CHECK(u->ammo[s]>=0&&u->res[s]>=0,"unit %d ammo",i); }
        CHECK(u->wp[0]>=0,"unit %d lost sidearm",i);
        CHECK(u->cur>=0&&u->cur<2&&u->wp[u->cur]>=0,"unit %d bad cur weapon",i);
    }
}

int main(void){
    settings_defaults();
    int total=0, endReason[5]={0}, plants=0, defuses=0, boom=0, timeouts=0, kills=0, shots=0;
    int agentCasts=0;
    for(int diff=0;diff<3;diff++) for(int map=0;map<M_COUNT;map++) for(int rep=0;rep<2;rep++){
        g_rng=0x9E3779B9u*(uint32_t)(diff*100+map*10+rep+1);
        game_new_match(map,3,diff,2,(map+rep)%A_COUNT);
        g_m.u[0].isPlayer=false; ai_round_start();
        long ticks=0; int lastRound=g_m.round, lastPlayed=0;
        while(g_m.phase!=PH_MATCHOVER && ticks<30*60*40){
            Intent none; memset(&none,0,sizeof none); none.abil=-1;
            game_tick(&none); ticks++; invariants();
            if(g_m.round!=lastRound){ lastRound=g_m.round; }
            if(g_m.phase==PH_ENDROUND&&g_m.roundsPlayed!=lastPlayed){ lastPlayed=g_m.roundsPlayed;
                total++;
                if(!strcmp(g_m.endText,"SPIKE DETONATED")){ boom++; plants++; }
                else if(!strcmp(g_m.endText,"SPIKE DEFUSED")){ defuses++; plants++; }
                else if(!strcmp(g_m.endText,"TIME EXPIRED")){ timeouts++; if(getenv("V")){ int c=g_m.carrier; printf("timeout: spike %d carrier %d alive A%d D%d",g_m.spike,c,game_alive(0),game_alive(1)); if(c>=0) printf(" carrier(%.1f,%.1f) st%d goal(%d,%d) site%d",g_m.u[c].x,g_m.u[c].y,g_m.u[c].ai.state,g_m.u[c].ai.goalX,g_m.u[c].ai.goalY,mp_site_at(g_m.u[c].x,g_m.u[c].y)); printf("\n"); } }
                else if(!strcmp(g_m.endText,"ATTACKERS ELIMINATED")) endReason[0]++;
                else if(!strcmp(g_m.endText,"DEFENDERS ELIMINATED")){ endReason[1]++; if(g_m.spike==SP_PLANTED) plants++; }
            }
        }
        for(int i=0;i<NUNITS;i++) kills+=g_m.u[i].kills;
        CHECK(g_m.phase==PH_MATCHOVER,"match %s diff %d did not finish (score %d-%d round %d)",g_maps[map].name,diff,g_m.score[0],g_m.score[1],g_m.round);
        CHECK(g_m.score[0]>=3||g_m.score[1]>=3,"no winner");
        printf("%-8s diff %d rep %d: %d-%d in %d rounds, %.1f min sim, kills %d\n",g_maps[map].name,diff,rep,g_m.score[0],g_m.score[1],g_m.round,ticks/1800.0,
               g_m.u[0].kills+g_m.u[1].kills+g_m.u[2].kills+g_m.u[3].kills+g_m.u[4].kills+g_m.u[5].kills+g_m.u[6].kills+g_m.u[7].kills+g_m.u[8].kills+g_m.u[9].kills);
    }
    (void)shots;(void)agentCasts;
    printf("\nrounds %d | atk elim %d, def elim %d, detonated %d, defused %d, timeouts %d, plants %d, total kills %d\n",total,endReason[0],endReason[1],boom,defuses,timeouts,plants,kills);
    CHECK(plants>0,"bots never planted the spike");
    CHECK(boom+defuses>0,"no spike resolution ever");
    CHECK(kills>total,"too few kills (%d)",kills);
    printf(fails?"SIM TEST FAILED (%d)\n":"SIM TEST OK\n",fails);
    return fails?1:0;
}
