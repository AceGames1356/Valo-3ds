/* Software column renderer. Walls: DDA grid raycaster with multi-hit (low cover),
   y-shear pitch, per-distance shaded palettes, procedural textures. Sprites are
   depth-sorted billboards occluded per column. Runs straight on the 3DS framebuffer
   layout (column-major RGB565) so each wall slice is a contiguous write. */
#include "render3d.h"
#include "gfx.h"
#include "settings.h"

#define TEX 32
#define NTEX 10
#define SHADES 16

static uint8_t  s_tex[NTEX][TEX*TEX];            /* [type][u*32+v] palette indices 0..15 */
static uint16_t s_pal[NTEX][SHADES][16];
static uint16_t s_bgTab[480];                    /* ceiling/floor colour by (y-horizon+240) */
static uint8_t  s_unitSpr[32][16];
static float    s_zbuf[TOP_W];
static float    s_lowT[TOP_W][3];
static int16_t  s_lowY[TOP_W][3];
static int      s_horizon;

typedef struct { uint8_t r,g,b; } C3;
static const C3 BASE[NTEX]={
 {150,155,165},{105,125,155},{150,100,85},{235,135,50},{50,185,215},{70,190,110},
 {200,70,70},{70,110,200},{165,120,70},{110,120,130}
};
static const C3 ACCENT[NTEX]={
 {90,95,105},{200,215,235},{90,55,45},{40,30,20},{20,50,70},{20,50,30},{50,20,20},{20,30,60},{110,75,40},{250,200,60}
};
static const C3 FOG={22,28,44};

static uint32_t hash2(int x,int y){ uint32_t h=(uint32_t)x*374761393u+(uint32_t)y*668265263u; h=(h^(h>>13))*1274126177u; return h^(h>>16); }

static void gen_texture(int t){
    uint8_t *p=s_tex[t];
    for(int u=0;u<TEX;u++) for(int v=0;v<TEX;v++){
        int n=(int)(hash2(u+t*31,v)%4);           /* 0..3 speckle */
        int val=8+n; int idx=val;
        switch(t){
        case 0: /* concrete with panel seams */
            if(u%16==0||v%16==0) idx=14; else idx=7+n+((u+v)%16<2?1:0); break;
        case 1: /* panels with rivets */
            idx=9+n/2; if(v%8==0) idx=13; if(u%16==0) idx=14; if((u%16==3||u%16==12)&&(v%8==3)) idx=15; break;
        case 2: { /* brick */
            int row=v/8, off=(row&1)?8:0; int bu=(u+off)%16;
            idx=8+n; if(v%8==0||bu==0) idx=14; } break;
        case 3: case 4: case 5: case 6: case 7: /* hazard stripes */
            idx=(((u+v)/6)&1)?(10+n/2):15; if(v<2||v>29) idx=14; break;
        case 8: /* crate planks */
            idx=9+n/2; if(v%8==0) idx=14; if(u==0||u==31||u==v||u==31-v) idx=13; if(u==15||u==16) idx=12; break;
        default: /* barrier */
            idx=(((u+v)/5)&1)?10:15; if(v<3||v>28) idx=13; break;
        }
        p[u*TEX+v]=(uint8_t)idx;
    }
}
static uint16_t mix565(C3 a,C3 b,float t){
    return RGB((int)(a.r+(b.r-a.r)*t),(int)(a.g+(b.g-a.g)*t),(int)(a.b+(b.b-a.b)*t));
}

void rd_init(void){
    for(int t=0;t<NTEX;t++){
        gen_texture(t);
        for(int lv=0;lv<SHADES;lv++){
            float near=lv/(float)(SHADES-1);          /* 1 = near/bright, 0 = far */
            for(int k=0;k<16;k++){
                C3 c;
                if(k==15) c=ACCENT[t];
                else { float b=0.38f+0.62f*(k/13.0f); if(k==14) b=0.25f; if(b>1) b=1;
                       c=(C3){(uint8_t)(BASE[t].r*b),(uint8_t)(BASE[t].g*b),(uint8_t)(BASE[t].b*b)}; }
                float lit=0.30f+0.70f*near;
                C3 d={(uint8_t)(c.r*lit),(uint8_t)(c.g*lit),(uint8_t)(c.b*lit)};
                s_pal[t][lv][k]=mix565(FOG,d,0.15f+0.85f*near);
            }
        }
    }
    for(int i=0;i<480;i++){
        int dy=i-240;                                 /* <0: above horizon (ceiling), >=0: floor */
        float d=(float)(dy<0?-dy:dy)/240.0f; if(d>1) d=1;
        if(dy<0) s_bgTab[i]=mix565((C3){18,24,40},(C3){52,70,104},1.0f-d*0.85f);
        else     s_bgTab[i]=mix565((C3){34,38,50},(C3){92,96,110},d);
    }
    /* humanoid sprite, 16x32. 0 clear, 1 skin, 2 body, 3 dark, 4 visor, 5 gun, 6 boots */
    memset(s_unitSpr,0,sizeof s_unitSpr);
    for(int y=0;y<8;y++) for(int x=5;x<11;x++) if(!((y==0||y==7)&&(x==5||x==10))) s_unitSpr[y][x]=1;
    for(int x=6;x<10;x++){ s_unitSpr[3][x]=4; s_unitSpr[4][x]=4; }
    for(int y=8;y<19;y++) for(int x=3;x<13;x++) s_unitSpr[y][x]=2;
    for(int y=9;y<18;y++){ for(int x=1;x<3;x++) s_unitSpr[y][x]=3; for(int x=13;x<15;x++) s_unitSpr[y][x]=3; }
    for(int y=12;y<15;y++) for(int x=9;x<16;x++) s_unitSpr[y][x]=5;
    for(int y=19;y<30;y++){ for(int x=4;x<8;x++) s_unitSpr[y][x]=3; for(int x=8;x<12;x++) s_unitSpr[y][x]=3; }
    for(int y=30;y<32;y++){ for(int x=3;x<8;x++) s_unitSpr[y][x]=6; for(int x=8;x<13;x++) s_unitSpr[y][x]=6; }
}

static int cell_tex(int c){ return c==C_CRATE?8:(c==C_BARRIER?9:(c-1)); }

/* ---------------------------------------------------------------- walls */
typedef struct { float t; uint8_t tex, side; float wx; bool low; } Hit;

static void slab(uint16_t *col,float yTopF,float yBotF,const uint8_t *texcol,const uint16_t *pal){
    int y0=(int)floorf(yTopF), y1=(int)floorf(yBotF);
    float hgt=yBotF-yTopF; if(hgt<1.0f) hgt=1.0f;
    int step=(int)((TEX*65536.0f)/hgt);
    int ys=y0<0?0:y0, ye=y1>SCR_H-1?SCR_H-1:y1;
    int pos=(ys-y0)*step;
    for(int y=ys;y<=ye;y++){ col[SCR_H-1-y]=pal[texcol[(pos>>16)&(TEX-1)]]; pos+=step; }
}

static void render_walls(Surf s,const Camera *c){
    float planeLen=settings_fov_plane()/c->zoom;
    float f=(TOP_W*0.5f)/planeLen;
    float fx=cosf(c->yaw), fy=sinf(c->yaw), px=-fy*planeLen, py=fx*planeLen;
    s_horizon=(int)(SCR_H*0.5f+tanf(c->pitch)*f);
    int step=g_set.quality?2:1;
    for(int x=0;x<TOP_W;x+=step){
        float camX=2.0f*(x+step*0.5f)/TOP_W-1.0f;
        float rdx=fx+px*camX, rdy=fy+py*camX;
        int mx=(int)floorf(c->x), my=(int)floorf(c->y);
        float ddx=rdx==0?1e30f:fabsf(1.0f/rdx), ddy=rdy==0?1e30f:fabsf(1.0f/rdy);
        int sx,sy; float sdx,sdy;
        if(rdx<0){ sx=-1; sdx=(c->x-mx)*ddx; } else { sx=1; sdx=(mx+1-c->x)*ddx; }
        if(rdy<0){ sy=-1; sdy=(c->y-my)*ddy; } else { sy=1; sdy=(my+1-c->y)*ddy; }
        Hit hits[6]; int nh=0; bool wall=false;
        for(int it=0;it<90&&!wall;it++){
            int side;
            if(sdx<sdy){ sdx+=ddx; mx+=sx; side=0; } else { sdy+=ddy; my+=sy; side=1; }
            int cell=mp_cell(mx,my);
            if(cell==C_OPEN) continue;
            bool isWall=mp_is_wall(cell);
            if(!isWall&&nh>=5) continue;
            float t=(side==0)?sdx-ddx:sdy-ddy; if(t<0.03f) t=0.03f;
            float wx=(side==0)?c->y+t*rdy:c->x+t*rdx; wx-=floorf(wx);
            if((side==0&&rdx>0)||(side==1&&rdy<0)) wx=1.0f-wx;
            hits[nh++]=(Hit){t,(uint8_t)cell_tex(cell),(uint8_t)side,wx,!isWall};
            if(isWall) wall=true;
        }
        float zwall=wall?hits[nh-1].t:60.0f;
        for(int k=0;k<step&&x+k<TOP_W;k++){ s_zbuf[x+k]=zwall; for(int j=0;j<3;j++){ s_lowT[x+k][j]=1e9f; s_lowY[x+k][j]=SCR_H; } }
        uint16_t *col=s.p+x*SCR_H;
        float camZ=c->eye;
        float yTopW=s_horizon-(WALL_H-camZ)*f/zwall, yBotW=s_horizon+camZ*f/zwall;
        int top=(int)floorf(yTopW), bot=(int)floorf(yBotW);
        if(!wall){ top=bot=s_horizon; }
        /* Perspective-mapped environment: ceiling panels above, tiled concrete
           floor below. This adds real world-space depth cues instead of a flat gradient. */
        for(int y=0;y<top&&y<SCR_H;y++){
            int i=y-s_horizon+240; uint16_t c=s_bgTab[i<0?0:(i>479?479:i)];
            int panel=((x/32)+(y/24))&1;
            if((x%32==0)||(y%24==0)) c=RGB(27,36,52);
            else if(panel) c=gx_blend(c,RGB(78,91,116),5);
            col[SCR_H-1-y]=c;
        }
        for(int y=(bot<0?0:bot+1);y<SCR_H;y++){
            int i=y-s_horizon+240;
            float dist=camZ*f/(float)(y-s_horizon);
            float wx=c->x+rdx*dist, wy=c->y+rdy*dist;
            int tx=(int)floorf(wx), ty=(int)floorf(wy);
            float ux=wx-floorf(wx), uy=wy-floorf(wy);
            int zone=mp_cell(tx,ty);
            C3 base={72,78,88};
            int z=g_map.zone[clampi(ty,0,MAP_H-1)][clampi(tx,0,MAP_W-1)];
            if(z==1) base=(C3){91,67,57};
            else if(z==2) base=(C3){57,78,91};
            else if(z==3) base=(C3){62,83,67};
            if(((tx+ty)&1)!=0){ base.r=(uint8_t)(base.r*0.91f); base.g=(uint8_t)(base.g*0.91f); base.b=(uint8_t)(base.b*0.91f); }
            if(ux<0.035f||uy<0.035f||ux>0.965f||uy>0.965f){ base.r*=0.48f; base.g*=0.48f; base.b*=0.48f; }
            if(mp_is_low(zone)){ base.r=105; base.g=86; base.b=62; }
            float fog=clampf(1.0f-dist/34.0f,0.12f,1.0f);
            col[SCR_H-1-y]=mix565(FOG,base,fog);
        }
        int lowN=0;
        for(int k=nh-1;k>=0;k--){
            const Hit *h=&hits[k];
            float hh=h->low?LOW_H:WALL_H;
            float yT=s_horizon-(hh-camZ)*f/h->t, yB=s_horizon+camZ*f/h->t;
            float shade=1.0f-h->t/26.0f; if(shade<0) shade=0;
            int lv=(int)(shade*(SHADES-1)); if(h->side) lv=(lv*7)/8;
            slab(col,yT,yB,&s_tex[h->tex][((int)(h->wx*TEX)&(TEX-1))*TEX],s_pal[h->tex][lv]);
        }
        for(int k=0;k<nh;k++) if(hits[k].low&&lowN<3){   /* nearest first; record for sprite clipping */
            float yT=s_horizon-(LOW_H-camZ)*f/hits[k].t;
            for(int q=0;q<step&&x+q<TOP_W;q++){ s_lowT[x+q][lowN]=hits[k].t; s_lowY[x+q][lowN]=(int16_t)clampi((int)yT,0,SCR_H); }
            lowN++;
        }
        for(int k=1;k<step&&x+k<TOP_W;k++) memcpy(s.p+(x+k)*SCR_H,col,SCR_H*sizeof(uint16_t));
    }
}

/* ---------------------------------------------------------------- sprites */
typedef struct { int kind; int idx; float depth; } Drawable;
enum { D_UNIT, D_SMOKE, D_FIELD, D_TURRET, D_TRAP, D_BEACON, D_SPIKE, D_PAD };

typedef struct { float fx,fy,px,py,f,invDet,camX,camY,camZ; } Proj;
static bool project(const Proj *p,float wx,float wy,float *sx,float *depth){
    float rx=wx-p->camX, ry=wy-p->camY;
    float tx=p->invDet*(p->fy*rx-p->fx*ry);
    float ty=p->invDet*(-p->py*rx+p->px*ry);
    if(ty<0.12f) return false;
    *depth=ty; *sx=(TOP_W*0.5f)*(1.0f+tx/ty); return true;
}
static inline int col_clip(int x,float depth){        /* lowest visible y for this column, 240 = no clip */
    int clip=SCR_H;
    for(int j=0;j<3;j++) if(s_lowT[x][j]<depth&&s_lowY[x][j]<clip) clip=s_lowY[x][j];
    return clip;
}

static void draw_box(Surf s,const Proj *p,float wx,float wy,float z0,float z1,float halfW,uint16_t color,uint16_t edge,float depth,float sx){
    float y0=s_horizon-(z1-p->camZ)*p->f/depth, y1=s_horizon-(z0-p->camZ)*p->f/depth;
    int xl=(int)(sx-halfW*p->f/depth), xr=(int)(sx+halfW*p->f/depth);
    for(int x=xl;x<=xr;x++){
        if((unsigned)x>=TOP_W||depth>=s_zbuf[x]) continue;
        int clip=col_clip(x,depth);
        int ya=(int)y0, yb=(int)y1; if(yb>clip) yb=clip;
        uint16_t c=(x==xl||x==xr)?edge:color;
        for(int y=ya<0?0:ya;y<yb&&y<SCR_H;y++) gx_px(s,x,y,(y==ya)?edge:c);
    }
    (void)wx;(void)wy;
}

static void draw_unit(Surf s,const Proj *p,const Unit *u,const Unit *view,float sx,float depth){
    bool friendly=(u->squad==view->squad);
    uint16_t body=friendly?RGB(70,190,210):RGB(215,70,70), dark=friendly?RGB(30,80,95):RGB(100,30,35);
    uint16_t pal[7]={0,RGB(235,190,150),body,dark,friendly?RGB(180,255,255):RGB(255,230,120),RGB(40,40,46),RGB(25,25,30)};
    if(u->hurtT>0.35f) pal[2]=RGB(255,255,255);
    float hgt=UNIT_H*p->f/depth, wid=hgt*0.5f;
    float yTop=s_horizon-(u->z+UNIT_H-p->camZ)*p->f/depth;
    int xl=(int)(sx-wid*0.5f), xr=(int)(sx+wid*0.5f);
    bool ghost=u->revealT>0&&!friendly;
    for(int x=xl;x<=xr;x++){
        if((unsigned)x>=TOP_W) continue;
        bool hidden=depth>=s_zbuf[x]-0.05f;
        if(hidden&&!ghost) continue;
        int clip=hidden?SCR_H:col_clip(x,depth);
        int sc=(int)((x-xl)*16/(wid>1?wid:1)); if(sc<0)sc=0; if(sc>15)sc=15;
        float stp=32.0f/hgt;
        int ya=(int)yTop, yb=(int)(yTop+hgt);
        for(int y=ya<0?0:ya;y<yb&&y<SCR_H&&y<clip;y++){
            int sr=(int)((y-yTop)*stp); if(sr<0)sr=0; if(sr>31)sr=31;
            uint8_t k=s_unitSpr[sr][sc]; if(!k) continue;
            if(hidden){ if((x+y)&1) continue; gx_px(s,x,y,RGB(255,60,60)); continue; }
            uint16_t c=pal[k];
            if(k==5&&u->muzzleT>0&&sc>13) c=RGB(255,230,120);
            gx_px(s,x,y,c);
        }
    }
    if(u->muzzleT>0&&!ghost){      /* muzzle flash */
        int fxp=(int)(sx+wid*0.55f), fyp=(int)(yTop+hgt*0.42f), r=(int)(hgt*0.08f)+1;
        if((unsigned)fxp<TOP_W&&depth<s_zbuf[fxp]) gx_disc(s,fxp,fyp,r,RGB(255,240,150));
    }
}

static void draw_smoke_like(Surf s,const Proj *p,const Zone *z,float depthC,bool dense){
    float px0=p->camX, py0=p->camY;
    float planeLen=(TOP_W*0.5f)/p->f;
    float fx=p->fx, fy=p->fy, rx=p->px, ry=p->py; (void)planeLen;
    float cx=px0-z->x, cy=py0-z->y;
    float R=z->r*(dense?1.0f:1.0f);
    float fade=1.0f; if(z->t<1.5f) fade=z->t/1.5f; if(z->t0-z->t<0.6f) fade=(z->t0-z->t)/0.6f;
    float hTop=dense?WALL_H*1.05f:0.55f;
    uint16_t col=dense?RGB(188,192,204):RGB(130,80,190);
    float aMax=dense?32.0f:11.0f;
    float sxc,dummy; (void)dummy;
    if(!project(p,z->x,z->y,&sxc,&dummy)) { if(!(cx*cx+cy*cy<R*R)) return; sxc=TOP_W*0.5f; }
    float ang=R*p->f/(depthC>0.3f?depthC:0.3f);
    int xl=(int)(sxc-ang*1.4f), xr=(int)(sxc+ang*1.4f);
    if(cx*cx+cy*cy<R*R){ xl=0; xr=TOP_W-1; }
    for(int x=xl<0?0:xl;x<=xr&&x<TOP_W;x++){
        float camX=2.0f*(x+0.5f)/TOP_W-1.0f;
        float dx=fx+rx*camX, dy=fy+ry*camX;
        float a=dx*dx+dy*dy, b=2.0f*(dx*cx+dy*cy), cc=cx*cx+cy*cy-R*R;
        float disc=b*b-4*a*cc; if(disc<=0) continue;
        float sq=sqrtf(disc), s1=(-b-sq)/(2*a), s2=(-b+sq)/(2*a);
        if(s2<0.05f) continue;
        if(s1<0.05f) s1=0.05f;
        if(s1>=s_zbuf[x]) continue;
        float chord=(s2-s1)*sqrtf(a);
        float alpha=clampf(chord/(R*1.3f),0,1)*aMax*fade;
        float yT=s_horizon-(hTop-p->camZ)*p->f/s1, yB=s_horizon+p->camZ*p->f/s1;
        int ya=(int)yT, yb=(int)yB; if(ya<0)ya=0; if(yb>SCR_H)yb=SCR_H;
        for(int y=ya;y<yb;y++){
            int a32=(int)alpha+(int)(((x*7+y*13)&7))-3; if(a32>32)a32=32; if(a32<1) continue;
            gx_px(s,x,y,gx_blend(gx_get(s,x,y),col,a32));
        }
    }
}

static void draw_beacon(Surf s,const Proj *p,int site,float sx,float depth){
    static const uint16_t SC[3]={RGB(255,150,50),RGB(60,200,230),RGB(90,220,120)};
    static const char *L[3]={"A","B","C"};
    float y0=s_horizon-(1.9f-p->camZ)*p->f/depth, y1=s_horizon-(0.0f-p->camZ)*p->f/depth;
    int x=(int)sx;
    if((unsigned)x<TOP_W&&depth<s_zbuf[x]){
        for(int y=(int)y0<0?0:(int)y0;y<(int)y1&&y<SCR_H;y++) if(y&1) gx_px(s,x,y,SC[site]);
        int sz=depth<8?2:1;
        gx_texts(s,x-3*sz,(int)y0-8*sz,L[site],SC[site],sz);
    }
}

void rd_world(Surf s,const Camera *cam,const Unit *view){
    render_walls(s,cam);
    if(!view) return;
    float planeLen=settings_fov_plane()/cam->zoom, f=(TOP_W*0.5f)/planeLen;
    Proj p; p.fx=cosf(cam->yaw); p.fy=sinf(cam->yaw); p.px=-p.fy*planeLen; p.py=p.fx*planeLen;
    p.f=f; p.invDet=1.0f/(p.px*p.fy-p.fx*p.py); p.camX=cam->x; p.camY=cam->y; p.camZ=cam->eye;
    Drawable list[48]; int n=0; float sx,d;
    for(int i=0;i<NUNITS;i++){ const Unit *u=&g_m.u[i];
        if(!u->alive||u==view) continue;
        if(project(&p,u->x,u->y,&sx,&d)&&n<48) list[n++]=(Drawable){D_UNIT,i,d}; }
    for(int i=0;i<MAX_ZONES;i++){ const Zone *z=&g_m.zones[i]; if(!z->active) continue;
        int k=-1; bool own=(z->team==view->team);
        if(z->type==Z_SMOKE) k=D_SMOKE; else if(z->type==Z_FIELD) k=D_FIELD; else if(z->type==Z_TURRET) k=D_TURRET;
        else if(z->type==Z_TRAP&&own) k=D_TRAP;
        if(k<0) continue;
        float dd; if(project(&p,z->x,z->y,&sx,&dd)||(k==D_SMOKE||k==D_FIELD)){ if(!project(&p,z->x,z->y,&sx,&dd)) dd=0.2f; if(n<48) list[n++]=(Drawable){k,i,dd}; } }
    for(int i=0;i<g_map.siteCount;i++) if(project(&p,g_map.site[i].cx,g_map.site[i].cy,&sx,&d)&&n<48) list[n++]=(Drawable){D_BEACON,i,d};
    for(int i=0;i<g_map.tpCount;i++) if(project(&p,g_map.tp[i].x+0.5f,g_map.tp[i].y+0.5f,&sx,&d)&&n<48) list[n++]=(Drawable){D_PAD,i,d};
    if((g_m.spike==SP_DROPPED||g_m.spike==SP_PLANTED)&&project(&p,g_m.spikeX,g_m.spikeY,&sx,&d)&&n<48) list[n++]=(Drawable){D_SPIKE,0,d};
    for(int i=1;i<n;i++){ Drawable t=list[i]; int j=i-1; while(j>=0&&list[j].depth<t.depth){ list[j+1]=list[j]; j--; } list[j+1]=t; }
    for(int k=0;k<n;k++){
        Drawable *dr=&list[k];
        switch(dr->kind){
        case D_UNIT: { const Unit *u=&g_m.u[dr->idx]; project(&p,u->x,u->y,&sx,&d); draw_unit(s,&p,u,view,sx,d); } break;
        case D_SMOKE: draw_smoke_like(s,&p,&g_m.zones[dr->idx],dr->depth,true); break;
        case D_FIELD: draw_smoke_like(s,&p,&g_m.zones[dr->idx],dr->depth,false); break;
        case D_TURRET: { const Zone *z=&g_m.zones[dr->idx]; project(&p,z->x,z->y,&sx,&d);
            bool fr=(z->team==view->team); uint16_t c=fr?RGB(60,170,140):RGB(210,80,80);
            draw_box(s,&p,z->x,z->y,0.0f,0.22f,0.17f,RGB(60,60,70),RGB(25,25,30),d,sx);
            draw_box(s,&p,z->x,z->y,0.22f,0.50f,0.13f,c,RGB(20,30,30),d,sx);
            if(z->cd<0.15f) draw_box(s,&p,z->x,z->y,0.38f,0.43f,0.24f,RGB(255,230,120),RGB(255,230,120),d,sx); } break;
        case D_TRAP: { const Zone *z=&g_m.zones[dr->idx]; project(&p,z->x,z->y,&sx,&d);
            draw_box(s,&p,z->x,z->y,0.0f,0.07f,0.28f,z->armed?RGB(90,240,200):RGB(60,120,110),RGB(20,60,50),d,sx); } break;
        case D_BEACON: draw_beacon(s,&p,dr->idx,(project(&p,g_map.site[dr->idx].cx,g_map.site[dr->idx].cy,&sx,&d),sx),dr->depth); break;
        case D_PAD: { project(&p,g_map.tp[dr->idx].x+0.5f,g_map.tp[dr->idx].y+0.5f,&sx,&d);
            draw_box(s,&p,0,0,0.0f,1.1f,0.22f,((g_m.tick/4)&1)?RGB(150,90,255):RGB(100,160,255),RGB(220,200,255),d,sx); } break;
        case D_SPIKE: { project(&p,g_m.spikeX,g_m.spikeY,&sx,&d);
            bool blink=(g_m.spike==SP_PLANTED)&&((int)(g_m.fuseT*(g_m.fuseT<10?4.0f:2.0f))&1);
            draw_box(s,&p,0,0,0.0f,0.2f,0.2f,blink?RGB(255,80,60):RGB(170,40,40),RGB(255,200,120),d,sx); } break;
        }
    }
}

void rd_camera_for(const Unit *u,Camera *c){
    c->x=u->x; c->y=u->y; c->eye=unit_eye(u)-(u->hp<=0?0.5f:0.0f); c->yaw=u->yaw;
    c->pitch=clampf(u->pitch+u->kick*0.012f,-0.8f,0.8f);
    c->zoom=1.0f;
    const Weapon *w=unit_weapon(u); if(u->scoped) c->zoom=w->zoom;
}

/* ---------------------------------------------------------------- viewmodel / scope */
void rd_viewmodel(Surf s,const Unit *u,float bob){
    const Weapon *w=unit_weapon(u);
    int bx=(int)(sinf(bob)*3.0f), by=(int)(fabsf(cosf(bob))*3.0f);
    if(u->swapT>0) by+=(int)(u->swapT*40.0f);
    if(u->reloadT>0) by+=(int)(sinf(clampf(u->reloadT/w->reload,0,1)*PI_F)*34.0f);
    int kick=(int)(u->kick*1.5f); by+=kick;
    uint16_t dk=RGB(24,27,34), md=RGB(58,64,76), lt=RGB(132,140,154);
    uint16_t edge=RGB(92,103,120), grip=RGB(34,39,49), hand=RGB(220,174,137);
    int x0=250+bx, y0=SCR_H-70+by;
    /* Layered weapon silhouettes: receiver, barrel, rail, sights and grips. */
    if(w->type==WT_PISTOL){
        gx_rect(s,x0+34,y0+24,78,17,md); gx_rect(s,x0+42,y0+20,60,5,lt);
        gx_rect(s,x0+94,y0+27,44,7,dk); gx_rect(s,x0+133,y0+28,12,4,edge);
        gx_rect(s,x0+46,y0+41,25,29,grip); gx_line(s,x0+48,y0+45,x0+67,y0+65,edge);
        gx_rect(s,x0+82,y0+18,7,6,dk); gx_rect(s,x0+87,y0+15,3,4,edge);
        gx_rect(s,x0+38,y0+36,18,5,hand);
    } else {
        int length=(w->type==WT_SNIPER)?164:((w->type==WT_SHOTGUN)?145:132);
        int rx=x0-4;
        gx_rect(s,rx+12,y0+23,length-28,17,md);
        gx_rect(s,rx+24,y0+19,length-54,5,lt);
        gx_rect(s,rx+length-24,y0+27,42,7,dk);
        gx_rect(s,rx+length+14,y0+29,9,3,edge);
        gx_rect(s,rx+4,y0+28,20,7,dk);
        gx_rect(s,rx+30,y0+18,24,4,edge);
        gx_rect(s,rx+54,y0+14,35,5,dk);
        gx_rect(s,rx+61,y0+10,21,4,edge);
        gx_rect(s,rx+42,y0+40,17,34,grip);
        gx_rect(s,rx+83,y0+40,23,13,hand);
        gx_line(s,rx+44,y0+48,rx+55,y0+66,edge);
        gx_rect(s,rx+16,y0+42,22,8,hand);
        gx_rect(s,rx+18,y0+50,15,22,grip);
        if(w->type==WT_SNIPER){
            gx_rect(s,rx+68,y0+4,44,9,dk); gx_rect(s,rx+76,y0+1,28,4,edge);
            gx_rect(s,rx+91,y0+13,5,5,lt);
        } else if(w->type==WT_SHOTGUN){
            gx_rect(s,rx+6,y0+17,112,5,dk); gx_rect(s,rx+12,y0+44,76,7,grip);
        } else if(w->type==WT_HEAVY){
            gx_rect(s,rx+22,y0+7,74,9,dk); gx_rect(s,rx+30,y0+3,56,4,edge);
            gx_rect(s,rx+102,y0+41,18,22,grip);
        } else {
            gx_rect(s,rx+20,y0+12,48,6,dk); gx_rect(s,rx+26,y0+8,34,4,edge);
            gx_rect(s,rx+70,y0+40,18,26,grip);
        }
    }
    if(u->muzzleT>0){ int mx=x0+(w->type==WT_PISTOL?112:150), my=y0+(w->type==WT_PISTOL?20:24);
        gx_disc(s,mx+6,my,10,RGB(255,200,80)); gx_disc(s,mx+6,my,5,RGB(255,250,200)); }
}
void rd_scope(Surf s){
    for(int x=0;x<TOP_W;x++){
        int dx=x-TOP_W/2; int h2=118*118-dx*dx;
        int half=h2>0?(int)sqrtf((float)h2):0;
        int y0=SCR_H/2-half, y1=SCR_H/2+half;
        uint16_t *col=s.p+x*SCR_H;
        for(int y=0;y<y0;y++) col[SCR_H-1-y]=0;
        for(int y=(y1<0?0:y1);y<SCR_H;y++) col[SCR_H-1-y]=0;
    }
    gx_rect(s,0,SCR_H/2,TOP_W,1,RGB(10,10,10)); gx_rect(s,TOP_W/2,0,1,SCR_H,RGB(10,10,10));
    gx_disc(s,TOP_W/2,SCR_H/2,2,RGB(255,60,60));
}
