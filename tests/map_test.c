#include <stdio.h>
#include "../source/map.h"
uint32_t g_rng=12345;
int main(void){
    static char buf[MAP_W*MAP_H*2+256]; int fail=0;
    for(int m=0;m<M_COUNT;m++){
        mp_load(m);
        mp_dump_ascii(buf,sizeof buf);
        printf("=== %s ===\n%s",g_maps[m].name,buf);
        int ax=(int)g_map.spawn[0].cx, ay=(int)g_map.spawn[0].cy;
        for(int s=0;s<g_map.siteCount;s++){
            int d=nav_dist(ax,ay,(int)g_map.site[s].cx,(int)g_map.site[s].cy);
            int d2=nav_dist((int)g_map.spawn[1].cx,(int)g_map.spawn[1].cy,(int)g_map.site[s].cx,(int)g_map.site[s].cy);
            printf("%s site %c: atk path %d, def path %d\n",g_maps[m].name,'A'+s,d,d2);
            if(d==65535||d2==65535) fail=1;
        }
        int dd=nav_dist(ax,ay,(int)g_map.spawn[1].cx,(int)g_map.spawn[1].cy);
        printf("atk->def spawn %d\n",dd); if(dd==65535) fail=1;
        /* every open cell must be reachable from attacker spawn (no sealed pockets) */
        const uint16_t *f=nav_field(ax,ay); int unreach=0,open=0;
        for(int y=0;y<MAP_H;y++)for(int x=0;x<MAP_W;x++) if(g_map.cell[y][x]==C_OPEN){ open++; if(f[y*MAP_W+x]==0xFFFF) unreach++; }
        printf("open cells %d, unreachable %d\n\n",open,unreach); if(unreach) fail=1;
    }
    printf(fail?"MAP TEST FAILED\n":"MAP TEST OK\n"); return fail;
}
