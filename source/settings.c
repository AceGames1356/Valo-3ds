#include "settings.h"
#include "platform.h"

Settings g_set;
#define SET_MAGIC 0x56334453u   /* 'V3DS' */
#define SET_FILE  "settings.bin"

static const char *BTN[B_COUNT]={"A","B","X","Y","L","R","ZL","ZR","START","SELECT","D-UP","D-DOWN","D-LEFT","D-RIGHT"};
static const char *ACT[ACT_COUNT]={"FIRE","JUMP","RELOAD","USE / PLANT","SWAP WEAPON","ABILITY 1","ABILITY 2","ULTIMATE","WALK / SCOPE"};
static const char *PRE[PRESET_COUNT]={"STANDARD","OLD 3DS","SOUTHPAW"};

const char *btn_name(int b){ return (b<0||b>=B_COUNT)?"---":BTN[b]; }
const char *act_name(int a){ return ACT[a]; }
const char *preset_name(int p){ return PRE[p]; }

void settings_apply_preset(int p){
    static const int8_t T[PRESET_COUNT][ACT_COUNT]={
        /* FIRE JUMP RELOAD USE SWAP A1   A2    ULT   ALT */
        { B_R, B_A, B_B,   B_Y, B_X, B_L, B_ZL, B_ZR, B_SELECT },       /* standard (New 3DS friendly)  */
        { B_R, B_A, B_B,   B_Y, B_X, B_L, B_SELECT, -1, -1 },           /* old 3DS: touch buttons too   */
        { B_L, B_DDOWN, B_DLEFT, B_DRIGHT, B_DUP, B_R, B_A, B_B, B_Y }, /* southpaw: D-pad = actions    */
    };
    p=clampi(p,0,PRESET_COUNT-1);
    for(int i=0;i<ACT_COUNT;i++) g_set.bind[i]=T[p][i];
    g_set.preset=(uint8_t)p;
    g_set.dpadLook = (p==2)?0:1;
}

void settings_defaults(void){
    memset(&g_set,0,sizeof g_set);
    g_set.magic=SET_MAGIC; g_set.version=1;
    g_set.sensStick=5; g_set.sensTouch=5; g_set.fov=1; g_set.quality=0;
    g_set.invertY=0; g_set.touchLook=1; g_set.aimAssist=1; g_set.showFps=0;
    g_set.lastMap=0; g_set.lastLen=0; g_set.lastDiff=1; g_set.lastSide=0; g_set.lastAgent=0;
    settings_apply_preset(0);
}

bool settings_load(void){
    Settings t;
    if(!plat_load(SET_FILE,&t,sizeof t)) return false;
    if(t.magic!=SET_MAGIC||t.version!=1) return false;
    for(int i=0;i<ACT_COUNT;i++) if(t.bind[i]<-1||t.bind[i]>=B_COUNT) return false;
    if(t.sensStick<1||t.sensStick>10||t.sensTouch<1||t.sensTouch>10||t.fov>2||t.quality>1||t.preset>=PRESET_COUNT) return false;
    if(t.lastMap>=4||t.lastLen>2||t.lastDiff>2||t.lastSide>2||t.lastAgent>=4) return false;
    g_set=t; return true;
}
bool settings_save(void){ return plat_save(SET_FILE,&g_set,sizeof g_set); }

float settings_fov_plane(void){
    static const float DEG[3]={65.0f,75.0f,85.0f};
    return tanf(DEG[g_set.fov>2?1:g_set.fov]*0.5f*PI_F/180.0f);
}
