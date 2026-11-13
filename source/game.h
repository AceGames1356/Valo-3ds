/* Core match simulation: units, weapons, abilities, spike, economy, rounds. */
#ifndef GAME_H
#define GAME_H
#include "common.h"
#include "data.h"
#include "map.h"

enum { TEAM_ATK=0, TEAM_DEF=1 };
enum { PH_BUY, PH_LIVE, PH_ENDROUND, PH_MATCHOVER };
enum { SP_CARRIED, SP_DROPPED, SP_PLANTED, SP_DEFUSED, SP_EXPLODED };
enum { Z_NONE, Z_SMOKE, Z_FIELD, Z_TRAP, Z_TURRET };
enum { CMD_AUTO, CMD_FOLLOW, CMD_HOLD, CMD_SITE_A, CMD_SITE_B, CMD_SITE_C };
enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD };
enum { BUY_LIGHT=100, BUY_HEAVY=101 };
enum { ZONE_BODY, ZONE_HEAD, ZONE_LEG };

typedef struct {
    float fwd, strafe;                /* -1..1 in the unit's local frame            */
    bool  fire, jump, reload, use, swap, alt;
    int   abil;                       /* -1 none, else 0,1,2 (edge: cast this tick) */
    bool  aimGiven; float aimX, aimY; /* bots give an explicit ability target       */
} Intent;

typedef struct {
    int   state;                      /* AI_* behaviour state                        */
    int   goalX, goalY;
    float goalWait, thinkT, repathT;
    int   role, holdSite;
    int   target;                     /* unit index, 100+n turret, -1 none           */
    float seeT, reactT, memX, memY, memT, alertT;
    float stuckT, stuckX, stuckY, strafeDir, strafeT, burstT, pauseT, coverT, abilT;
    float wantYaw, wantPitch;
    float noticeDelay;
    int   cmdSeen;
    float jitYaw, jitPitch, jitT;
    bool  usedA0, usedA1;
} Ai;

typedef struct {
    float x,y,z,vz, yaw,pitch, vx,vy;
    int   hp, armor;
    bool  alive, isPlayer, grounded;
    int   squad, team, agent;
    int   wp[2], ammo[2], res[2], cur;
    bool  fireHeld;
    int   refund[3];                  /* credits refundable this buy phase: sidearm, primary, armor */
    float fireCd, reloadT, swapT, recoil, kick, muzzleT;
    float cd[2]; float ult;
    float dashT, dashDx, dashDy;
    float flashT, slowT, stunT, revealT, buffT, tpCd, hurtT, hurtDir, noiseT, spotT;
    bool  scoped, moving, spotted, phaseLock;
    int   credits, kills, deaths, score, roundKills;
    int   lastWeapon;
    char  name[10];
    Ai    ai;
} Unit;

typedef struct { int type, team, owner; float x,y,r,t,t0,hp,cd,yaw; bool armed, active; } Zone;
typedef struct { int killer, victim, weapon; bool head, spike; float t; } KillMsg;
#define MAX_ZONES 24

typedef struct {
    int mapId, target, diff, startSide, playerAgent;
    Unit u[NUNITS];
    int phase; float phaseT, roundT;
    int round, score[2], lossStreak[2], roundsPlayed, half;
    int spike; float spikeX, spikeY, plantProg, defuseProg, fuseT; int plantSite, carrier, planter, defuser; bool defuseCp;
    int roundWinSquad; char endText[40];
    Zone zones[MAX_ZONES];
    KillMsg feed[6];
    float hitT; bool hitHead; float killT;
    float dmgInd[4]; float dmgIndAng[4];
    char banner[40]; float bannerT; uint16_t bannerCol;
    char toast[48]; float toastT;
    int cmd; int spec; uint32_t tick;
    char buyMsg[40];
    bool matchWon;                    /* valid in PH_MATCHOVER: player's squad won  */
} Match;
extern Match g_m;

void  game_new_match(int map,int target,int diff,int startSide,int agent);
void  game_tick(const Intent *pin);
bool  game_buy(int ui,int item);
void  game_skip_buy(void);
void  game_set_cmd(int cmd);
void  game_look(float dyaw,float dpitch);
const Weapon *unit_weapon(const Unit *u);
static inline float unit_eye(const Unit *u){ return u->z+EYE_H; }
int   game_alive(int team);
const Unit *game_view_unit(void);
int   game_side_of_squad(int squad);        /* TEAM_ATK/TEAM_DEF for squad this round */
bool  game_can_see(const Unit *a,const Unit *b,bool useFov);
bool  game_smoke_blocks(float x0,float y0,float x1,float y1);
bool  game_cast(int ui,int slot,float ax,float ay);
void  game_aim_point(const Unit *u,float range,float *x,float *y);
bool  unit_busy_spike(int ui);
int   buy_price(int item);
void  game_toast(const char *s);
bool  g_set_aim_assist(void);
void  game_banner(const char *s,uint16_t col);
bool  unit_in_spawn(const Unit *u);
float unit_speed_mul(const Unit *u);

/* ai.c */
void  ai_round_start(void);
void  ai_think(int ui,Intent *out);
void  ai_buy(int ui);
void  ai_on_event_plant(void);
#endif
