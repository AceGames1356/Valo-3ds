#ifndef MAP_MODULE_H
#define MAP_MODULE_H
#include "common.h"
#include "data.h"

typedef struct { int x0,y0,x1,y1; float cx,cy; } Rect;
typedef struct { int x,y,dx,dy; } TpLink;
typedef struct {
    uint8_t cell[MAP_H][MAP_W];
    uint8_t zone[MAP_H][MAP_W];      /* 0 none, 1..3 site A..C, 4 attacker spawn, 5 defender spawn */
    int     id, siteCount;
    Rect    site[3];
    Rect    spawn[2];                /* [0] attackers, [1] defenders */
    int     tpCount;
    TpLink  tp[4];
} GameMap;
extern GameMap g_map;

void  mp_load(int id);
static inline int mp_cell(int x,int y){ return ((unsigned)x>=MAP_W||(unsigned)y>=MAP_H)?C_WALL:g_map.cell[y][x]; }
static inline bool mp_is_wall(int c){ return c>=C_WALL && c<=C_WALL_LAST; }
static inline bool mp_is_low(int c){ return c==C_CRATE||c==C_BARRIER; }
bool  mp_solid(float x,float y,float z);                 /* blocked for something with feet at height z */
bool  mp_circle_blocked(float x,float y,float z,float r);
float mp_floor(float x,float y);
float mp_floor_circle(float x,float y,float r);   /* highest floor under the unit's body */                         /* walkable floor height at x,y */
/* Ray trace. (dx,dy) must be a unit vector. dz = height change per unit of horizontal travel.
   Returns the distance to the first obstruction (or maxd). */
float mp_trace(float ox,float oy,float oz,float dx,float dy,float dz,float maxd);
int   mp_site_at(float x,float y);                       /* 0 or site index 1..3 */
bool  mp_rand_open_in(const Rect *r,float *x,float *y);
bool  mp_nearest_open(float x,float y,float *ox,float *oy);
bool  mp_tp_at(int cx,int cy,TpLink *out);

/* navigation: BFS distance fields over open cells (8-way, no corner cutting, teleporters linked) */
const uint16_t *nav_field(int gx,int gy);
bool  nav_step(const uint16_t *f,int cx,int cy,int *nx,int *ny);
void  nav_clear(void);
int   nav_dist(int sx,int sy,int gx,int gy);             /* 65535 if unreachable */

/* debug / tests */
void  mp_dump_ascii(char *out,size_t n);
#endif
