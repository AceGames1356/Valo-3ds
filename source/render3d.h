#ifndef RENDER3D_H
#define RENDER3D_H
#include "common.h"
#include "game.h"

typedef struct { float x,y,eye,yaw,pitch,zoom; } Camera;

void rd_init(void);
/* Draws the 3D world into the top surface. 'view' may be NULL (menu scene: no units). */
void rd_world(Surf s,const Camera *cam,const Unit *view);
void rd_viewmodel(Surf s,const Unit *u,float bobPhase);
void rd_scope(Surf s);
void rd_camera_for(const Unit *u,Camera *c);
#endif
