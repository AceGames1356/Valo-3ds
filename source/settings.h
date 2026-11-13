#ifndef SETTINGS_H
#define SETTINGS_H
#include "common.h"

/* Rebindable in-match actions. Each maps to one physical button (B_* index) or -1. */
enum { ACT_FIRE,ACT_JUMP,ACT_RELOAD,ACT_USE,ACT_SWAP,ACT_ABIL1,ACT_ABIL2,ACT_ULT,ACT_ALT,ACT_COUNT };
#define PRESET_COUNT 3

typedef struct {
    uint32_t magic;
    uint16_t version, pad;
    int8_t   bind[ACT_COUNT];
    uint8_t  sensStick;      /* 1..10  C-Stick / D-Pad look speed          */
    uint8_t  sensTouch;      /* 1..10  touch-pad look speed                */
    uint8_t  fov;            /* 0 = 65, 1 = 75, 2 = 85 degrees             */
    uint8_t  quality;        /* 0 = HIGH (400 columns), 1 = FAST (200)     */
    uint8_t  preset;
    uint8_t  invertY, dpadLook, touchLook, aimAssist, showFps;
    uint8_t  lastMap, lastLen, lastDiff, lastSide, lastAgent;
} Settings;

extern Settings g_set;
void        settings_defaults(void);
void        settings_apply_preset(int p);
bool        settings_load(void);
bool        settings_save(void);
const char *btn_name(int b);
const char *act_name(int a);
const char *preset_name(int p);
float       settings_fov_plane(void);   /* tan(fov/2) */
#endif
