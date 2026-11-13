/* Platform abstraction. platform_3ds.c implements this with libctru;
   platform_host.c implements it for desktop testing. The game itself
   (everything else in source/) never includes <3ds.h>. */
#ifndef PLATFORM_H
#define PLATFORM_H
#include "common.h"

enum { B_A,B_B,B_X,B_Y,B_L,B_R,B_ZL,B_ZR,B_START,B_SELECT,B_DUP,B_DDOWN,B_DLEFT,B_DRIGHT,B_COUNT };
#define BM(b) (1u<<(b))

typedef struct {
    uint32_t held, down, up;      /* BM(B_*) masks; down/up are latched edges   */
    int cpx, cpy;                 /* Circle Pad, about -156..156                 */
    int csx, csy;                 /* C-Stick (New 3DS), 0 if not available       */
    bool touching, tdown, tup;    /* bottom-screen touch state / edges           */
    int tx, ty, tdx, tdy;         /* touch position (0..319,0..239) and delta    */
    bool new3ds;
} Input;

bool      plat_init(void);
void      plat_exit(void);
bool      plat_running(void);
void      plat_poll(Input *in);          /* fills one raw 60 Hz sample           */
Surf      plat_top(void);                /* writable top framebuffer (400x240)   */
Surf      plat_bottom(void);             /* writable bottom framebuffer (320x240)*/
void      plat_present(void);            /* flush + swap buffers                  */
void      plat_wait_frame(void);         /* wait for vblank                       */
uint32_t  plat_ms(void);                 /* monotonic milliseconds                */
bool      plat_load(const char *name, void *buf, size_t n);   /* settings file   */
bool      plat_save(const char *name, const void *buf, size_t n);
void      plat_request_quit(void);

#endif
