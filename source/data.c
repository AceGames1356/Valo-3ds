#include "data.h"

/* name, cat, slot, type, price, mag, reserve, rof, reload, dmg, head, leg, pellets,
   spread, moveSpread, recoil, falloffStart, falloffMult, auto, zoom, moveMul */
const Weapon g_weapons[W_COUNT]={
 /* sidearms */
 {"CLASSIC","SIDEARM",0,WT_PISTOL,   0,12, 36, 6.75f,1.75f, 26, 78, 22, 1,0.40f,2.5f,0.80f,15,0.80f,false,1.0f,1.00f},
 {"GHOST",  "SIDEARM",0,WT_PISTOL, 500,13, 39, 6.75f,1.50f, 30,105, 25, 1,0.35f,2.5f,0.85f,16,0.85f,false,1.0f,1.00f},
 {"SHERIFF","SIDEARM",0,WT_PISTOL, 800, 6, 24, 4.00f,2.25f, 55,159, 46, 1,0.30f,3.5f,2.60f,18,0.80f,false,1.0f,1.00f},
 {"FRENZY", "SIDEARM",0,WT_PISTOL, 450,15, 45,10.00f,1.50f, 26, 78, 22, 1,0.60f,2.2f,0.55f, 9,0.70f,true, 1.0f,1.00f},
 /* SMGs */
 {"STINGER","SMG",    1,WT_SMG,   1100,20, 60,16.00f,2.25f, 27, 67, 22, 1,0.70f,2.6f,0.40f, 9,0.70f,true, 1.0f,1.00f},
 {"SPECTRE","SMG",    1,WT_SMG,   1600,30, 90,13.33f,2.25f, 26, 78, 22, 1,0.60f,2.4f,0.40f,10,0.75f,true, 1.0f,1.00f},
 /* shotguns */
 {"BUCKY",  "SHOTGUN",1,WT_SHOTGUN,850, 5, 10, 1.10f,2.50f, 18, 36, 14,12,3.60f,1.2f,2.50f, 4,0.30f,false,1.0f,0.97f},
 {"JUDGE",  "SHOTGUN",1,WT_SHOTGUN,1850,5, 12, 3.50f,2.20f, 17, 34, 13,12,3.20f,1.2f,1.50f, 4,0.30f,true, 1.0f,0.97f},
 /* rifles */
 {"BULLDOG","RIFLE",  1,WT_RIFLE, 2050,24, 72, 9.15f,2.50f, 35,116, 29, 1,0.30f,4.2f,0.50f,24,0.85f,true, 1.0f,0.96f},
 {"GUARDIAN","RIFLE", 1,WT_RIFLE, 2250,12, 36, 5.25f,2.50f, 65,195, 49, 1,0.20f,4.8f,1.00f,40,1.00f,false,1.0f,0.96f},
 {"PHANTOM","RIFLE",  1,WT_RIFLE, 2900,30, 90,11.00f,2.50f, 39,156, 33, 1,0.28f,4.4f,0.52f,15,0.90f,true, 1.0f,0.96f},
 {"VANDAL", "RIFLE",  1,WT_RIFLE, 2900,25, 75, 9.75f,2.50f, 40,160, 34, 1,0.25f,4.4f,0.58f,50,1.00f,true, 1.0f,0.96f},
 /* snipers */
 {"MARSHAL","SNIPER", 1,WT_SNIPER, 950, 5, 10, 1.50f,2.50f,101,202, 85, 1,0.10f,7.0f,0.00f,99,1.00f,false,2.0f,1.00f},
 {"OUTLAW", "SNIPER", 1,WT_SNIPER,2400, 2,  9, 2.75f,2.70f,140,238,119, 1,0.10f,7.0f,0.00f,99,1.00f,false,2.5f,0.95f},
 {"OPERATOR","SNIPER",1,WT_SNIPER,4700, 5, 10, 0.60f,3.70f,150,255,120, 1,0.05f,9.0f,0.00f,99,1.00f,false,3.0f,0.90f},
 /* heavies */
 {"ARES",   "HEAVY",  1,WT_HEAVY, 1600,50,100,10.00f,3.25f, 30, 72, 25, 1,0.50f,3.2f,0.38f,20,0.85f,true, 1.0f,0.92f},
 {"ODIN",   "HEAVY",  1,WT_HEAVY, 3200,100,200,12.00f,5.00f, 38, 95, 32, 1,0.60f,3.0f,0.35f,20,0.85f,true, 1.0f,0.90f},
};

/* Original agents. Slot 0/1 = basic abilities (cooldown), slot 2 = ultimate. */
const Agent g_agents[A_COUNT]={
 {"EMBER","DUELIST","Aggressive entry fragger. Dashes into sites and blinds defenders.",RGB(255,120,60),{
   {"BLAZE DASH","Dash forward a short distance.",AB_DASH,12,0.18f,0,4.5f,0},
   {"FLASHPOINT","Pop-flash. Enemies looking at it are blinded.",AB_FLASH,20,2.0f,9,0,0},
   {"OVERDRIVE","10s: faster fire rate, move speed, instant reload.",AB_OVERDRIVE,0,10,0,0,6}}},
 {"VEIL","CONTROLLER","Shapes the fight with smokes and slowing fields.",RGB(150,110,255),{
   {"SMOKE ORB","Place a smoke at your crosshair. Blocks vision.",AB_SMOKE,18,9,1.7f,14,0},
   {"GRAVITY MIRE","Field that slows enemies inside.",AB_FIELD,25,7,3.0f,12,0},
   {"ECLIPSE","Huge long-lasting smoke at your crosshair.",AB_BIGSMOKE,0,14,3.2f,14,7}}},
 {"BASTION","SENTINEL","Locks down areas with traps and an automated turret.",RGB(80,200,160),{
   {"TRIPWIRE","Hidden trap at your feet. Reveals, slows and hurts.",AB_TRAP,22,60,1.1f,0,0},
   {"SENTRY","Deploy a turret that shoots visible enemies.",AB_TURRET,35,25,12,4,0},
   {"REINFORCE","Fully heal and armor yourself and nearby allies.",AB_HEAL,0,0,9,0,7}}},
 {"RADAR","INITIATOR","Gathers information and softens up enemy positions.",RGB(90,170,255),{
   {"SCAN PULSE","Reveal enemies nearby, even through walls.",AB_SCAN,20,4,14,0,0},
   {"STUN BOLT","Thrown at crosshair. Concusses enemies in the blast.",AB_STUN,22,3,3.5f,12,0},
   {"SWEEP","Reveal every enemy on the map for 6s.",AB_REVEALALL,0,6,0,0,6}}},
};

/* ---- maps: rectangle ops. 'W' wall, 'O' open, 'L' low cover, 'P' repaint walls,
        'Z' zone (1..3 site A/B/C, 4 attacker spawn, 5 defender spawn), 'T' teleporter pair ---- */
#define O_(a,b,c,d)   {'O',a,b,c,d,0}
#define W_(a,b,c,d,v) {'W',a,b,c,d,v}
#define L_(a,b,c,d,v) {'L',a,b,c,d,v}
#define P_(a,b,c,d,v) {'P',a,b,c,d,v}
#define Z_(a,b,c,d,v) {'Z',a,b,c,d,v}
#define T_(a,b,c,d)   {'T',a,b,c,d,0}
#define CR C_CRATE
#define BA C_BARRIER

/* CITADEL - Ascent-inspired: two sites, central courtyard with doors, market links. */
static const MapOp CITADEL[]={
 O_(19,41,28,45),Z_(19,41,28,45,4), O_(19,2,28,6),Z_(19,2,28,6,5),
 O_(15,7,32,9),
 O_(22,32,25,40),O_(14,20,33,31),W_(22,24,25,27,2),O_(22,10,25,19),
 O_(4,6,15,17),Z_(4,6,15,17,1),       O_(32,6,43,17),Z_(32,6,43,17,2),
 O_(8,41,18,45),O_(5,18,10,45),       O_(29,41,39,45),O_(37,18,42,45),
 O_(11,18,14,21),O_(33,18,36,21),
 O_(11,34,21,36),O_(26,34,36,36),
 L_(8,10,9,11,CR),L_(12,13,13,13,BA),W_(6,14,7,15,2),L_(10,7,10,8,BA),
 L_(38,12,39,13,CR),L_(34,9,35,9,BA),W_(40,8,41,9,2),L_(36,15,37,16,CR),
 L_(16,22,17,23,CR),L_(30,28,31,29,CR),L_(7,32,8,33,CR),L_(39,28,40,29,CR),
 L_(15,35,16,35,BA),L_(31,35,32,35,BA),L_(23,12,24,12,BA),
 P_(3,5,16,18,4),P_(31,5,44,18,5),P_(17,40,30,46,7),P_(17,1,30,6,8),P_(13,19,34,32,3),
};

/* TRINITY - Haven-inspired: THREE sites (A, B, C), three lanes, cross connectors. */
static const MapOp TRINITY[]={
 O_(19,42,28,45),Z_(19,42,28,45,4), O_(4,38,43,41),
 O_(4,14,8,37),O_(39,14,43,37),
 O_(3,3,14,13),Z_(3,3,14,13,1), O_(19,3,28,12),Z_(19,3,28,12,2), O_(33,3,44,13),Z_(33,3,44,13,3),
 O_(20,16,27,21),Z_(20,16,27,21,5),
 O_(21,13,26,15),
 O_(9,17,19,19),O_(9,14,12,16), O_(28,17,38,19),O_(35,14,38,16),
 O_(20,32,27,37),O_(20,25,27,31),W_(23,27,24,29,2),O_(22,22,25,24),L_(22,23,25,23,BA),
 O_(9,26,19,28),O_(28,26,38,28),
 O_(16,10,18,25),
 O_(15,6,18,9),O_(29,6,32,9),
 L_(6,7,7,8,CR),L_(10,9,11,10,BA),W_(4,11,5,12,2),
 L_(22,6,23,7,CR),L_(25,9,26,9,BA),
 L_(40,7,41,8,CR),L_(36,10,37,11,BA),W_(43,11,44,12,2),
 L_(6,32,7,33,CR),L_(40,32,41,33,CR),L_(23,34,24,34,BA),L_(13,18,14,18,BA),L_(33,18,34,18,BA),
 P_(2,2,15,14,4),P_(18,2,29,13,5),P_(32,2,45,14,6),P_(17,40,30,46,7),P_(19,15,28,22,8),
};

/* PORTALS - Bind-inspired: no mid, long/short lanes, two linked teleporters. */
static const MapOp PORTALS[]={
 O_(19,41,28,45),Z_(19,41,28,45,4), O_(3,41,44,45),
 W_(11,42,12,43,2),W_(35,42,36,43,2),W_(23,44,24,45,2),
 O_(3,16,6,40),O_(41,16,44,40),
 O_(4,6,15,15),Z_(4,6,15,15,1), O_(32,6,43,15),Z_(32,6,43,15,2),
 O_(10,16,15,30),O_(7,28,9,30), O_(32,16,37,30),O_(38,28,40,30),
 T_(12,22,35,22),
 O_(20,2,27,6),Z_(20,2,27,6,5), O_(16,5,31,9),
 L_(8,9,9,10,CR),L_(12,12,13,12,BA),W_(5,7,6,8,2),
 L_(38,9,39,10,CR),L_(34,12,35,12,BA),W_(41,7,42,8,2),
 L_(4,26,5,27,CR),L_(42,26,43,27,CR),L_(12,18,13,19,BA),L_(34,18,35,19,BA),
 L_(22,8,25,8,BA),L_(20,34,20,34,BA),
 P_(2,5,16,16,4),P_(31,5,45,16,5),P_(2,40,45,46,7),P_(16,1,31,10,8),
};

/* RAMPARTS - Split-inspired: strong central mid with a choke, ramps and side mains. */
static const MapOp RAMPARTS[]={
 O_(19,42,28,45),Z_(19,42,28,45,4), O_(3,41,44,45),
 O_(19,2,28,5),Z_(19,2,28,5,5),
 O_(21,6,26,11),O_(21,12,26,40),
 W_(21,22,22,24,2),W_(25,22,26,24,2),W_(23,15,24,16,2),W_(23,33,24,34,2),
 O_(33,17,36,31),O_(27,27,32,29),O_(40,17,44,41),O_(33,4,44,16),Z_(33,4,44,16,1),
 O_(11,17,14,31),O_(15,27,20,29),O_(3,17,7,41),O_(3,4,14,16),Z_(3,4,14,16,2),
 O_(29,3,32,7),O_(15,3,18,7),
 L_(37,8,38,9,CR),W_(41,10,42,11,2),L_(34,13,35,13,BA),L_(36,5,36,5,BA),
 L_(9,8,10,9,CR),W_(5,10,6,11,2),L_(12,13,13,13,BA),L_(11,5,11,5,BA),
 L_(41,28,42,29,CR),L_(5,28,6,29,CR),L_(22,37,25,37,BA),L_(22,19,25,19,BA),
 P_(31,3,45,17,4),P_(2,3,16,17,5),P_(2,40,45,46,7),P_(17,1,30,6,8),P_(20,6,27,40,3),
};

#define MD(n,b,i,arr) {n,b,i,arr,(int)(sizeof(arr)/sizeof(arr[0]))}
const MapDef g_maps[M_COUNT]={
 MD("CITADEL","Two sites, a pillared courtyard mid and market links.","ASCENT-INSPIRED",CITADEL),
 MD("TRINITY","Three sites (A B C) with long lanes and rotations.","HAVEN-INSPIRED",TRINITY),
 MD("PORTALS","No mid. Long and short lanes with two linked portals.","BIND-INSPIRED",PORTALS),
 MD("RAMPARTS","Choked central mid, ramps and heavy side mains.","SPLIT-INSPIRED",RAMPARTS),
};
