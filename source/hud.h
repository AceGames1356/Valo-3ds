#ifndef HUD_H
#define HUD_H
#include "common.h"
#include "game.h"
void hud_draw(Surf s,const Unit *view,int buySel,float fps);
void hud_weapon_panel(Surf s,int x,int y,int wid);
void hud_scoreboard(Surf s,int x,int y);
#endif
