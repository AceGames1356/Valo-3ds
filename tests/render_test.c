/* Renders a few 3D scenes to PPM files for visual inspection. */
#include <stdio.h>
#include <stdlib.h>
#include "../source/game.h"
#include "../source/render3d.h"
#include "../source/settings.h"
#include "../source/platform.h"
void host_dump_ppm(Surf s,const char *path);
int main(int argc,char **argv){
    settings_defaults(); rd_init(); plat_init();
    g_rng=4242; game_new_match(0,3,1,0,1);
    Surf top=plat_top();
    /* put the player in the courtyard facing the pillar, enemies spread in view */
    Unit *p=&g_m.u[0];
    float px[]={22.5f,16.0f,8.5f}, py[]={36.0f,28.5f,15.5f}, yw[]={-1.5708f,0.0f,-0.6f};
    for(int sc=0;sc<3;sc++){
        p->x=px[sc]; p->y=py[sc]; p->yaw=yw[sc]; p->z=0; p->pitch=0;
        for(int i=1;i<NUNITS;i++){ g_m.u[i].alive=false; }
        /* a couple of visible enemies */
        g_m.u[5].alive=true; g_m.u[5].squad=1; g_m.u[5].x=p->x+cosf(p->yaw)*7.0f; g_m.u[5].y=p->y+sinf(p->yaw)*7.0f; g_m.u[5].z=0; g_m.u[5].revealT=0;
        g_m.u[1].alive=true; g_m.u[1].x=p->x+cosf(p->yaw+0.3f)*4.0f; g_m.u[1].y=p->y+sinf(p->yaw+0.3f)*4.0f;
        g_m.u[6].alive=true; g_m.u[6].x=p->x+cosf(p->yaw-0.2f)*11.0f; g_m.u[6].y=p->y+sinf(p->yaw-0.2f)*11.0f; g_m.u[6].muzzleT=0.05f;
        if(sc==1){ g_m.zones[0]=(Zone){Z_SMOKE,0,0,p->x+cosf(p->yaw)*5.0f,p->y+sinf(p->yaw)*5.0f+1.0f,1.7f,8,9,0,0,0,false,true}; }
        Camera c; rd_camera_for(p,&c);
        rd_world(top,&c,p); rd_viewmodel(top,p,0.3f);
        char path[64]; snprintf(path,sizeof path,"/tmp/scene%d.ppm",sc); host_dump_ppm(top,path);
    }
    return 0;
}
