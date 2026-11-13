#include "map.h"
#include <stdlib.h>

GameMap g_map;

static void fill_rect(uint8_t a[MAP_H][MAP_W],int x0,int y0,int x1,int y1,uint8_t v){
    for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++)
        if(x>=0&&y>=0&&x<MAP_W&&y<MAP_H) a[y][x]=v;
}

bool mp_nearest_open(float fx,float fy,float *ox,float *oy){
    int cx=(int)fx, cy=(int)fy;
    for(int r=0;r<MAP_W;r++)
        for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++){
            if(r>0 && abs(dx)!=r && abs(dy)!=r) continue;
            if(mp_cell(cx+dx,cy+dy)==C_OPEN){ *ox=cx+dx+0.5f; *oy=cy+dy+0.5f; return true; }
        }
    return false;
}

void mp_load(int id){
    const MapDef *d=&g_maps[id];
    memset(&g_map,0,sizeof g_map);
    g_map.id=id;
    memset(g_map.cell,C_WALL,sizeof g_map.cell);
    for(int i=0;i<d->nops;i++){
        const MapOp *o=&d->ops[i];
        switch(o->op){
        case 'O': fill_rect(g_map.cell,o->x0,o->y0,o->x1,o->y1,C_OPEN); break;
        case 'W': fill_rect(g_map.cell,o->x0,o->y0,o->x1,o->y1,(uint8_t)o->v); break;
        case 'L': for(int y=o->y0;y<=o->y1;y++) for(int x=o->x0;x<=o->x1;x++)
                      if(mp_cell(x,y)==C_OPEN) g_map.cell[y][x]=(uint8_t)o->v;
                  break;
        case 'P': for(int y=o->y0;y<=o->y1;y++) for(int x=o->x0;x<=o->x1;x++)
                      if(mp_is_wall(mp_cell(x,y))) g_map.cell[y][x]=(uint8_t)o->v;
                  break;
        case 'Z': {
            fill_rect(g_map.zone,o->x0,o->y0,o->x1,o->y1,(uint8_t)o->v);
            Rect r={o->x0,o->y0,o->x1,o->y1,0,0};
            float ox,oy;
            mp_nearest_open((o->x0+o->x1+1)*0.5f,(o->y0+o->y1+1)*0.5f,&ox,&oy);
            r.cx=ox; r.cy=oy;
            if(o->v>=1&&o->v<=3){ g_map.site[o->v-1]=r; if(o->v>g_map.siteCount) g_map.siteCount=o->v; }
            else if(o->v==4) g_map.spawn[0]=r;
            else if(o->v==5) g_map.spawn[1]=r;
        } break;
        case 'T':
            if(g_map.tpCount<=2){
                g_map.tp[g_map.tpCount++]=(TpLink){o->x0,o->y0,o->x1,o->y1};
                g_map.tp[g_map.tpCount++]=(TpLink){o->x1,o->y1,o->x0,o->y0};
            }
            break;
        }
    }
    for(int i=0;i<3;i++) if(i<g_map.siteCount){ Rect *r=&g_map.site[i]; mp_nearest_open((r->x0+r->x1+1)*0.5f,(r->y0+r->y1+1)*0.5f,&r->cx,&r->cy); }
    for(int i=0;i<2;i++){ Rect *r=&g_map.spawn[i]; mp_nearest_open((r->x0+r->x1+1)*0.5f,(r->y0+r->y1+1)*0.5f,&r->cx,&r->cy); }
    for(int i=0;i<MAP_W;i++){ g_map.cell[0][i]=1; g_map.cell[MAP_H-1][i]=1; }
    for(int i=0;i<MAP_H;i++){ g_map.cell[i][0]=1; g_map.cell[i][MAP_W-1]=1; }
    nav_clear();
}

bool mp_solid(float x,float y,float z){
    int c=mp_cell((int)floorf(x),(int)floorf(y));
    if(c==C_OPEN) return false;
    if(mp_is_wall(c)) return true;
    return z < LOW_H-0.02f;
}
bool mp_circle_blocked(float x,float y,float z,float r){
    return mp_solid(x-r,y-r,z)||mp_solid(x+r,y-r,z)||mp_solid(x-r,y+r,z)||mp_solid(x+r,y+r,z);
}
float mp_floor(float x,float y){
    int c=mp_cell((int)floorf(x),(int)floorf(y));
    return mp_is_low(c)?LOW_H:0.0f;
}
float mp_floor_circle(float x,float y,float r){
    float f=mp_floor(x-r,y-r), g=mp_floor(x+r,y-r); if(g>f) f=g;
    g=mp_floor(x-r,y+r); if(g>f) f=g;
    g=mp_floor(x+r,y+r); if(g>f) f=g;
    return f;
}

float mp_trace(float ox,float oy,float oz,float dx,float dy,float dz,float maxd){
    int mx=(int)floorf(ox), my=(int)floorf(oy);
    float ddx=(dx==0.0f)?1e30f:fabsf(1.0f/dx);
    float ddy=(dy==0.0f)?1e30f:fabsf(1.0f/dy);
    int sx,sy; float sdx,sdy;
    if(dx<0){ sx=-1; sdx=(ox-mx)*ddx; } else { sx=1; sdx=(mx+1-ox)*ddx; }
    if(dy<0){ sy=-1; sdy=(oy-my)*ddy; } else { sy=1; sdy=(my+1-oy)*ddy; }
    for(int i=0;i<160;i++){
        float tEnter;
        if(sdx<sdy){ tEnter=sdx; sdx+=ddx; mx+=sx; } else { tEnter=sdy; sdy+=ddy; my+=sy; }
        if(tEnter>=maxd) return maxd;
        int c=mp_cell(mx,my);
        if(mp_is_wall(c)) return tEnter;
        if(mp_is_low(c)){
            float tExit=(sdx<sdy)?sdx:sdy;
            float z1=oz+dz*tEnter, z2=oz+dz*tExit;
            if((z1<z2?z1:z2) < LOW_H) return tEnter;
        }
    }
    return maxd;
}

int mp_site_at(float x,float y){
    int z=g_map.zone[clampi((int)y,0,MAP_H-1)][clampi((int)x,0,MAP_W-1)];
    return (z>=1&&z<=3)?z:0;
}

bool mp_rand_open_in(const Rect *r,float *x,float *y){
    for(int i=0;i<60;i++){
        int cx=r->x0+rndi(r->x1-r->x0+1), cy=r->y0+rndi(r->y1-r->y0+1);
        if(mp_cell(cx,cy)==C_OPEN){ *x=cx+0.5f; *y=cy+0.5f; return true; }
    }
    return mp_nearest_open(r->cx,r->cy,x,y);
}

bool mp_tp_at(int cx,int cy,TpLink *out){
    for(int i=0;i<g_map.tpCount;i++) if(g_map.tp[i].x==cx&&g_map.tp[i].y==cy){ if(out)*out=g_map.tp[i]; return true; }
    return false;
}

/* ---------------- navigation ---------------- */
#define NAV_SLOTS 24
typedef struct { int goal; uint32_t used; uint16_t d[MAP_H*MAP_W]; } NavSlot;
static NavSlot s_nav[NAV_SLOTS];
static uint32_t s_navClock;
static const int DX8[8]={1,-1,0,0,1,1,-1,-1}, DY8[8]={0,0,1,-1,1,-1,1,-1};

void nav_clear(void){ for(int i=0;i<NAV_SLOTS;i++) s_nav[i].goal=-1; s_navClock=0; }
static inline bool navpass(int x,int y){ return mp_cell(x,y)==C_OPEN; }

const uint16_t *nav_field(int gx,int gy){
    gx=clampi(gx,0,MAP_W-1); gy=clampi(gy,0,MAP_H-1);
    int goal=gy*MAP_W+gx, victim=0; uint32_t oldest=0xFFFFFFFFu;
    for(int i=0;i<NAV_SLOTS;i++){
        if(s_nav[i].goal==goal){ s_nav[i].used=++s_navClock; return s_nav[i].d; }
        if(s_nav[i].goal<0){ victim=i; oldest=0; }
        else if(s_nav[i].used<oldest){ oldest=s_nav[i].used; victim=i; }
    }
    NavSlot *s=&s_nav[victim]; s->goal=goal; s->used=++s_navClock;
    for(int i=0;i<MAP_W*MAP_H;i++) s->d[i]=0xFFFF;
    static int16_t q[MAP_W*MAP_H]; int qh=0,qt=0;
    if(!navpass(gx,gy)){ float ox,oy; if(!mp_nearest_open(gx+.5f,gy+.5f,&ox,&oy)) return s->d; gx=(int)ox; gy=(int)oy; }
    s->d[gy*MAP_W+gx]=0; q[qt++]=(int16_t)(gy*MAP_W+gx);
    while(qh<qt){
        int c=q[qh++], cx=c%MAP_W, cy=c/MAP_W; uint16_t nd=(uint16_t)(s->d[c]+1);
        for(int k=0;k<8;k++){
            int nx=cx+DX8[k], ny=cy+DY8[k];
            if(!navpass(nx,ny)) continue;
            if(k>=4 && (!navpass(cx+DX8[k],cy)||!navpass(cx,cy+DY8[k]))) continue;
            int ni=ny*MAP_W+nx;
            if(s->d[ni]>nd){ s->d[ni]=nd; q[qt++]=(int16_t)ni; }
        }
        for(int t=0;t<g_map.tpCount;t++) if(g_map.tp[t].x==cx&&g_map.tp[t].y==cy){
            int ni=g_map.tp[t].dy*MAP_W+g_map.tp[t].dx;
            if(s->d[ni]>nd){ s->d[ni]=nd; q[qt++]=(int16_t)ni; }
        }
    }
    return s->d;
}

bool nav_step(const uint16_t *f,int cx,int cy,int *nx,int *ny){
    cx=clampi(cx,0,MAP_W-1); cy=clampi(cy,0,MAP_H-1);
    uint16_t cur=f[cy*MAP_W+cx], best=cur; int bx=cx,by=cy;
    if(cur==0xFFFF||cur==0) return false;
    for(int k=0;k<8;k++){
        int x=cx+DX8[k], y=cy+DY8[k];
        if(!navpass(x,y)) continue;
        if(k>=4 && (!navpass(cx+DX8[k],cy)||!navpass(cx,cy+DY8[k]))) continue;
        uint16_t d=f[y*MAP_W+x];
        if(d<best){ best=d; bx=x; by=y; }
    }
    for(int t=0;t<g_map.tpCount;t++) if(g_map.tp[t].x==cx&&g_map.tp[t].y==cy){
        uint16_t d=f[g_map.tp[t].dy*MAP_W+g_map.tp[t].dx];
        if(d<best){ best=d; bx=g_map.tp[t].dx; by=g_map.tp[t].dy; }
    }
    if(best>=cur) return false;
    *nx=bx; *ny=by; return true;
}

int nav_dist(int sx,int sy,int gx,int gy){
    const uint16_t *f=nav_field(gx,gy);
    return f[clampi(sy,0,MAP_H-1)*MAP_W+clampi(sx,0,MAP_W-1)];
}

void mp_dump_ascii(char *out,size_t n){
    size_t p=0;
    for(int y=0;y<MAP_H&&p+MAP_W+2<n;y++){
        for(int x=0;x<MAP_W;x++){
            int c=g_map.cell[y][x]; char ch='.';
            if(mp_is_wall(c)) ch='#'; else if(c==C_CRATE) ch='c'; else if(c==C_BARRIER) ch='b';
            else { int z=g_map.zone[y][x]; if(z>=1&&z<=3) ch='A'+z-1; else if(z==4) ch='^'; else if(z==5) ch='v'; }
            for(int t=0;t<g_map.tpCount;t++) if(g_map.tp[t].x==x&&g_map.tp[t].y==y) ch='T';
            out[p++]=ch;
        }
        out[p++]='\n';
    }
    out[p]=0;
}
