/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <png.h>

#include "global.h"
#include "psp_sdl.h"
#include "psp_danzeff.h"
#include "psp_ti99.h"

int psp_screenshot_mode = 0;
int ti99_in_menu = 0;

# if defined(GP2X_MODE) || defined(DINGUX_MODE) || defined(LINUX_MODE)
static void
ti99_render_normal()
{
  u32 *src_pixel = (u32*)blit_surface->pixels;
  u32 *dst_pixel = (u32*)back_surface->pixels;
  dst_pixel += ((320 - TI99_SCREEN_W) / 2) / 2;
  dst_pixel += (((240 - TI99_SCREEN_H) * PSP_LINE_SIZE / 2)) / 2;
  int h = TI99_SCREEN_H;
  while (h-- > 0) {
    int w = TI99_SCREEN_W / 2;
    while (w-- > 0) {
      *dst_pixel++ = *src_pixel++;
    }
    dst_pixel += (PSP_LINE_SIZE - TI99_SCREEN_W) / 2;
  }
}
# else
static void
ti99_render_normal()
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = TI99_WIDTH;
  srcRect.h = TI99_HEIGHT;
  dstRect.x = ( 320 - TI99_WIDTH  ) / 2;
  dstRect.y = ( 240 - TI99_HEIGHT ) / 2;
  dstRect.w = TI99_WIDTH;
  dstRect.h = TI99_HEIGHT;

  SDL_SoftStretch( blit_surface, &srcRect, back_surface, &dstRect );
}
# endif

# if defined(GP2X_MODE) || defined(DINGUX_MODE) || defined(LINUX_MODE)
/* 

  LUDO: 16-bit HiColor (565 format) 
  see http://www.compuphase.com/graphic/scale3.htm

 */
static inline u16 loc_coloraverage(u16 a, u16 b)
{
  return (u16)(((a ^ b) & 0xf7deU) >> 1) + (a & b);
}

static inline void 
ti99_X125_pixel(u16 *dist, const u16 *src)
{
  dist[0] = src[0];
  dist[1] = loc_coloraverage(src[0], src[1]);
  dist[2] = src[1];
  dist[3] = src[2];
  dist[4] = src[3];
}

static void
ti99_render_fit()
{
  int step_y = 1;
  u16 *src_line = (u16*)blit_surface->pixels;
  u16 *tgt_line = (u16*)back_surface->pixels;
  int  h = 240;
  while (h-- > 0) {
    int w = 320 / 5;
    u16* scan_src_line = src_line;
    while (w-- > 0) {
      ti99_X125_pixel(tgt_line, scan_src_line);
      tgt_line += 5;
      scan_src_line += 4;
    }
    step_y++;
    if (step_y < 5) {
      src_line += TI99_SCREEN_W;
    } else  step_y = 0;
  }
}
# else

static void
ti99_render_fit()
{
  SDL_Rect srcRect;
  SDL_Rect dstRect;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = TI99_WIDTH;
  srcRect.h = TI99_HEIGHT;
  dstRect.x = 0;
  dstRect.y = 0;
  dstRect.w = 320;
  dstRect.h = 240;

  SDL_SoftStretch( blit_surface, &srcRect, back_surface, &dstRect );
}
# endif


void
ti99_synchronize(void)
{
  static u32 nextclock = 1;
  static u32 next_sec_clock = 0;
  static u32 cur_num_frame = 0;

  u32 curclock = SDL_GetTicks();

  if (TI99.ti99_speed_limiter) {
    while (curclock < nextclock) {
     curclock = SDL_GetTicks();
    }
    u32 f_period = 1000 / TI99.ti99_speed_limiter;
    nextclock += f_period;
    if (nextclock < curclock) nextclock = curclock + f_period;
  }

  if (TI99.ti99_view_fps) {
    cur_num_frame++;
    if (curclock > next_sec_clock) {
      next_sec_clock = curclock + 1000;
      TI99.ti99_current_fps = cur_num_frame;
      cur_num_frame = 0;
    }
  }
}

void
psp_sdl_render()
{
  if (ti99_in_menu) return;

  if (TI99.psp_skip_cur_frame <= 0) {

    TI99.psp_skip_cur_frame = TI99.psp_skip_max_frame;

    if (TI99.ti99_render_mode == TI99_RENDER_NORMAL) ti99_render_normal();
    else                                             ti99_render_fit();

    if (psp_kbd_is_danzeff_mode()) {

      danzeff_moveTo(-50, -50);
      danzeff_render( TI99.danzeff_trans );
    }

    if (TI99.ti99_view_fps) {
      char buffer[32];
      sprintf(buffer, "%03d %3d", TI99.ti99_current_clock, (int)TI99.ti99_current_fps );
      psp_sdl_fill_print(0, 0, buffer, 0xffffff, 0 );
    }

    psp_sdl_flip();
  
    if (psp_screenshot_mode) {
      psp_screenshot_mode--;
      if (psp_screenshot_mode <= 0) {
        psp_sdl_save_screenshot();
        psp_screenshot_mode = 0;
      }
    }

  } else if (TI99.psp_skip_max_frame) {
    TI99.psp_skip_cur_frame--;
  }

  ti99_synchronize();
}
