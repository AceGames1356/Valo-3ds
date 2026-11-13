#ifndef APP_H
#define APP_H
#include "platform.h"
void app_init(void);
/* Called once per presented frame: 'ticks' = number of 30 Hz simulation steps to run (0..2). */
void app_frame(const Input *in,int ticks);
void app_set_fps(float fps);
void app_shutdown(void);
#endif
