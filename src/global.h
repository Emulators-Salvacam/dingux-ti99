#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#include "gp2x_psp.h"
#include "gp2x_cpu.h"
#include <time.h>
#include <dirent.h>

# ifndef CLK_TCK
# define CLK_TCK  CLOCKS_PER_SEC
# endif

#include <SDL.h>

# define TI99_RENDER_NORMAL      0
# define TI99_RENDER_FIT         1
# define TI99_LAST_RENDER        1

# define MAX_PATH            256
# define TI99_MAX_SAVE_STATE    5

# define TI99_LOAD_K7_MODE      0
# define TI99_LOAD_DISK_MODE    1
# define TI99_MAX_LOAD_MODE     1

# define TI99_SCREEN_W    256
# define TI99_SCREEN_H    192
# define TI99_HEIGHT      TI99_SCREEN_H
# define TI99_WIDTH       TI99_SCREEN_W
# define SNAP_WIDTH   (TI99_SCREEN_W/2)
# define SNAP_HEIGHT  (TI99_SCREEN_H/2)

# define TI99_RAM_SIZE 0x10000


# define TI99_MAX_CHEAT    10

#define TI99_CHEAT_NONE    0
#define TI99_CHEAT_ENABLE  1
#define TI99_CHEAT_DISABLE 2

#define TI99_CHEAT_COMMENT_SIZE 25

#define TI99_MAX_RAM_PAGE  32
  
  typedef struct TI99_cheat_t {
    unsigned char  type;
    unsigned short addr;
    unsigned char  value;
    char           comment[TI99_CHEAT_COMMENT_SIZE];
  } TI99_cheat_t;

  typedef struct TI99_save_t {

    SDL_Surface    *surface;
    char            used;
    char            thumb;
    time_t          date;

  } TI99_save_t;

  typedef struct TI99_t {
 
    TI99_save_t ti99_save_state[TI99_MAX_SAVE_STATE];
    TI99_cheat_t ti99_cheat[TI99_MAX_CHEAT];

    int        comment_present;
    char       ti99_save_name[MAX_PATH];
    char       ti99_home_dir[MAX_PATH];
    int        psp_screenshot_id;
    int        psp_cpu_clock;
    int        psp_reverse_analog;
    int        ti99_view_fps;
    int        ti99_current_fps;
    int        ti99_current_clock;
    int        ti99_snd_enable;
    char       psp_irdajoy_type;
    char       psp_irdajoy_debug;
    int        ti99_render_mode;
    int        ti99_vsync;
    int        danzeff_trans;
    int        psp_skip_max_frame;
    int        psp_skip_cur_frame;
    int        ti99_speed_limiter;
    int        ti99_auto_fire;
    int        ti99_auto_fire_pressed;
    int        ti99_auto_fire_period;

  } TI99_t;

  extern int ti99_in_menu;
  extern int psp_exit_now;

  extern u8 CpuMemory[ 0x10000 ];
  extern u8 MemFlags[ 0x10000 ];

  extern TI99_t TI99;

  extern void ti99_global_init();

//END_LUDO:
#ifdef __cplusplus
}
#endif

# endif
