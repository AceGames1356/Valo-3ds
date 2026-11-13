/* Shared basic definitions for Valorant 3DS. Pure C99, no platform headers. */
#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#define TOP_W   400
#define BOT_W   320
#define SCR_H   240
#define PI_F    3.14159265f
#define TAU_F   6.28318531f
#define TICK_HZ 30
#define DT      (1.0f / 30.0f)

#define MAP_W   48
#define MAP_H   48
#define NUNITS  10

/* RGB565 colour, as used by the 3DS framebuffers (GSP_RGB565_OES) */
#define RGB(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3)))

/* A render surface. Memory is column-major with 240 rows per column, and
   row 0 (top of the screen) is stored LAST in each column. This is exactly
   how the 3DS LCD framebuffers are laid out, so no conversion is needed. */
typedef struct { uint16_t *p; int w; } Surf;

extern uint32_t g_rng;
static inline uint32_t rnd32(void){ uint32_t x=g_rng; x^=x<<13; x^=x>>17; x^=x<<5; return g_rng=x; }
static inline float rndf(void){ return (float)(rnd32()>>8) * (1.0f/16777216.0f); }
static inline int   rndi(int n){ return n<=1 ? 0 : (int)(rnd32() % (uint32_t)n); }
static inline float rndr(float a,float b){ return a + (b-a)*rndf(); }
static inline float clampf(float v,float a,float b){ return v<a?a:(v>b?b:v); }
static inline int   clampi(int v,int a,int b){ return v<a?a:(v>b?b:v); }
static inline float wrapang(float a){ while(a>PI_F)a-=TAU_F; while(a<-PI_F)a+=TAU_F; return a; }
static inline float sqf(float v){ return v*v; }

#endif
