/* Static game data: weapons, agents/abilities, map definitions. */
#ifndef DATA_H
#define DATA_H
#include "common.h"

/* ---------- weapons ---------- */
enum { W_CLASSIC,W_GHOST,W_SHERIFF,W_FRENZY,W_STINGER,W_SPECTRE,W_BUCKY,W_JUDGE,W_BULLDOG,
       W_GUARDIAN,W_PHANTOM,W_VANDAL,W_MARSHAL,W_OUTLAW,W_OPERATOR,W_ARES,W_ODIN,W_COUNT };
enum { WT_PISTOL,WT_SMG,WT_SHOTGUN,WT_RIFLE,WT_SNIPER,WT_HEAVY };

typedef struct {
    const char *name;
    const char *cat;
    uint8_t slot;          /* 0 = sidearm, 1 = primary                     */
    uint8_t type;
    uint16_t price;
    uint8_t mag, reserve;
    float rof;             /* shots per second                              */
    float reload;          /* seconds                                       */
    uint16_t dmg, head, leg;  /* damage per bullet/pellet                  */
    uint8_t pellets;
    float spread;          /* degrees, standing still                       */
    float moveSpread;      /* extra degrees while running                   */
    float recoil;          /* degrees added per shot (decays)               */
    float falloffStart;    /* world units                                   */
    float falloffMult;     /* damage multiplier beyond 2x falloffStart      */
    bool  autofire;
    float zoom;            /* 1 = none, >1 = scope                          */
    float moveMul;         /* movement speed multiplier while held          */
} Weapon;
extern const Weapon g_weapons[W_COUNT];

/* ---------- abilities / agents ---------- */
enum { AB_DASH,AB_FLASH,AB_SMOKE,AB_FIELD,AB_TRAP,AB_TURRET,AB_SCAN,AB_STUN,
       AB_OVERDRIVE,AB_HEAL,AB_BIGSMOKE,AB_REVEALALL };
typedef struct {
    const char *name;
    const char *desc;
    int type;
    float cd;        /* cooldown seconds (basic abilities)                   */
    float dur;       /* effect duration seconds                              */
    float radius;    /* effect radius, world units                           */
    float range;     /* max placement/throw distance                         */
    int   cost;      /* ultimate points required (0 for basic abilities)     */
} Ability;
typedef struct {
    const char *name;
    const char *role;
    const char *blurb;
    uint16_t color;
    Ability ab[3];   /* [0],[1] basic, [2] ultimate */
} Agent;
#define A_COUNT 4
extern const Agent g_agents[A_COUNT];

/* ---------- maps ---------- */
typedef struct { char op; int8_t x0,y0,x1,y1; int8_t v; } MapOp;
typedef struct {
    const char *name;
    const char *blurb;
    const char *inspired;
    const MapOp *ops;
    int nops;
} MapDef;
#define M_COUNT 4
extern const MapDef g_maps[M_COUNT];

/* cell codes */
enum { C_OPEN=0, C_WALL=1, C_WALL_LAST=8, C_CRATE=10, C_BARRIER=11 };
#define WALL_H 1.5f
#define LOW_H  0.5f
#define EYE_H  0.82f
#define UNIT_H 0.9f

/* economy / match constants */
#define CREDITS_MAX     9000
#define CREDITS_START   800
#define BUY_TIME        20.0f
#define ROUND_TIME      100.0f
#define PLANT_TIME      4.0f
#define DEFUSE_TIME     7.0f
#define SPIKE_FUSE      45.0f
#define ENDROUND_TIME   5.0f
#define ULT_MAX         7

#endif
