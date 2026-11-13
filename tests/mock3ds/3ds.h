/* MINIMAL MOCK of the libctru API surface used by source/platform_3ds.c.
   It exists only so the platform layer can be syntax/type-checked with a host
   compiler where devkitPro is not installed. It is NOT libctru; the real build
   uses the genuine <3ds.h>. Declarations were written from the libctru docs. */
#ifndef MOCK_3DS_H
#define MOCK_3DS_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64; typedef int16_t s16;
#define BIT(n) (1U<<(n))
enum { KEY_A=BIT(0),KEY_B=BIT(1),KEY_SELECT=BIT(2),KEY_START=BIT(3),KEY_DRIGHT=BIT(4),KEY_DLEFT=BIT(5),KEY_DUP=BIT(6),KEY_DDOWN=BIT(7),
       KEY_R=BIT(8),KEY_L=BIT(9),KEY_X=BIT(10),KEY_Y=BIT(11),KEY_ZL=BIT(14),KEY_ZR=BIT(15),KEY_TOUCH=BIT(20) };
typedef enum { GSP_RGB565_OES=2 } GSPGPU_FramebufferFormat;
typedef enum { GFX_TOP=0, GFX_BOTTOM=1 } gfxScreen_t;
typedef enum { GFX_LEFT=0, GFX_RIGHT=1 } gfx3dSide_t;
typedef struct { s16 dx, dy; } circlePosition;
typedef struct { u16 px, py; } touchPosition;
void gfxInit(GSPGPU_FramebufferFormat top,GSPGPU_FramebufferFormat bottom,bool vram);
void gfxExit(void);
void gfxSetDoubleBuffering(gfxScreen_t s,bool d);
void gfxSet3D(bool enable);
u8*  gfxGetFramebuffer(gfxScreen_t s,gfx3dSide_t side,u16 *width,u16 *height);
void gfxFlushBuffers(void);
void gfxSwapBuffers(void);
void gspWaitForVBlank(void);
bool aptMainLoop(void);
int  APT_CheckNew3DS(bool *out);
void osSetSpeedupEnable(bool enable);
u64  osGetTime(void);
void hidScanInput(void);
u32  hidKeysHeld(void); u32 hidKeysDown(void); u32 hidKeysUp(void);
void hidCircleRead(circlePosition *pos);
void hidTouchRead(touchPosition *pos);
void hidCstickRead(circlePosition *pos);
int  irrstInit(void); void irrstExit(void); void irrstScanInput(void); void irrstCstickRead(circlePosition *pos);
#endif
