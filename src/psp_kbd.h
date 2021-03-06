/*
 *  Copyright (C) 2006 Ludovic Jacomme (ludovic.jacomme@gmail.com)
 *
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

# ifndef _KBD_H_
# define _KBD_H_
# ifdef __cplusplus
extern "C" {
# endif

# define PSP_ALL_BUTTON_MASK 0xFFFF
# define GP2X_ALL_BUTTON_MASK 0xFFFF

#ifndef __cplusplus
  enum ti99_vk_enum {

    VK_NONE = 0,
    VK_ENTER, VK_SPACE, VK_COMMA, VK_PERIOD, VK_DIVIDE,
    VK_SEMICOLON, VK_EQUALS, VK_CAPSLOCK,
    VK_SHIFT, VK_CTRL, VK_FCTN,
    VK_0, VK_1, VK_2, VK_3, VK_4,
    VK_5, VK_6, VK_7, VK_8, VK_9,
    VK_A, VK_B, VK_C, VK_D, VK_E, VK_F, VK_G,
    VK_H, VK_I, VK_J, VK_K, VK_L, VK_M, VK_N,
    VK_O, VK_P, VK_Q, VK_R, VK_S, VK_T, VK_U,
    VK_V, VK_W, VK_X, VK_Y, VK_Z,
    VK_MAX
 };
#endif

 enum ti99_keys_emum {

   TI99K_0,          
   TI99K_1,          
   TI99K_2,          
   TI99K_3,          
   TI99K_4,          
   TI99K_5,          
   TI99K_6,          
   TI99K_7,          
   TI99K_8,          
   TI99K_9,          

   TI99K_A,          
   TI99K_B,          
   TI99K_C,          
   TI99K_D,          
   TI99K_E,          
   TI99K_F,          
   TI99K_G,          
   TI99K_H,          
   TI99K_I,          
   TI99K_J,          
   TI99K_K,          
   TI99K_L,          
   TI99K_M,          
   TI99K_N,          
   TI99K_O,          
   TI99K_P,          
   TI99K_Q,          
   TI99K_R,          
   TI99K_S,          
   TI99K_T,          
   TI99K_U,          
   TI99K_V,          
   TI99K_W,          
   TI99K_X,          
   TI99K_Y,          
   TI99K_Z,          

   TI99K_a,          
   TI99K_b,          
   TI99K_c,          
   TI99K_d,          
   TI99K_e,          
   TI99K_f,          
   TI99K_g,          
   TI99K_h,          
   TI99K_i,          
   TI99K_j,          
   TI99K_k,          
   TI99K_l,          
   TI99K_m,          
   TI99K_n,          
   TI99K_o,          
   TI99K_p,          
   TI99K_q,          
   TI99K_r,          
   TI99K_s,          
   TI99K_t,          
   TI99K_u,          
   TI99K_v,          
   TI99K_w,          
   TI99K_x,          
   TI99K_y,          
   TI99K_z,          

   TI99K_QUOTE,
   TI99K_COMMA,
   TI99K_LESS,       
   TI99K_PERIOD,     
   TI99K_GREATER,    
   TI99K_SEMICOLON,  
   TI99K_COLON,  
   TI99K_UNDERSCORE, 
   TI99K_PIPE,
   TI99K_EQUAL,
   TI99K_PLUS,
   TI99K_TILDA,
   TI99K_DBLQUOTE,
   TI99K_QUESTION,
   TI99K_SLASH,
   TI99K_MINUS,  
   TI99K_LBRACKET,
   TI99K_RBRACKET,
   TI99K_LCBRACE,
   TI99K_RCBRACE,
   TI99K_SPACE,
   TI99K_EXCLAMATN,
   TI99K_AT,
   TI99K_HASH,
   TI99K_DOLLAR,
   TI99K_PERCENT,
   TI99K_POWER,
   TI99K_AMPERSAND,
   TI99K_ASTERISK,
   TI99K_LPAREN,
   TI99K_RPAREN,
   TI99K_BACKSLASH,
   TI99K_BACKQUOTE,

   TI99K_TAB,        
   TI99K_BACKSPACE,        
   TI99K_LEFT,        
   TI99K_RIGHT,        
   TI99K_UP,        
   TI99K_DOWN,        
   TI99K_DELETE,        
   TI99K_RETURN,        
   TI99K_SHIFT,        
   TI99K_ALT,        
   TI99K_CTRL,        
   TI99K_CAPSLOCK,        
   
   TI99K_JOY_UP,     
   TI99K_JOY_DOWN,   
   TI99K_JOY_LEFT,   
   TI99K_JOY_RIGHT,  
   TI99K_JOY_FIRE,  

   TI99_C_FPS,
   TI99_C_JOY,
   TI99_C_RENDER,
   TI99_C_LOAD,
   TI99_C_SAVE,
   TI99_C_RESET,
   TI99_C_AUTOFIRE,
   TI99_C_INCFIRE,
   TI99_C_DECFIRE,
   TI99_C_SCREEN,
    
   TI99K_MAX_KEY      

  };

# define KBD_UP           0
# define KBD_RIGHT        1
# define KBD_DOWN         2
# define KBD_LEFT         3
# define KBD_TRIANGLE     4
# define KBD_CIRCLE       5
# define KBD_CROSS        6
# define KBD_SQUARE       7
# define KBD_SELECT       8
# define KBD_START        9
# define KBD_LTRIGGER    10
# define KBD_RTRIGGER    11

# define KBD_MAX_BUTTONS 12

# define KBD_JOY_UP      12
# define KBD_JOY_RIGHT   13
# define KBD_JOY_DOWN    14
# define KBD_JOY_LEFT    15

# define KBD_ALL_BUTTONS 16

# define KBD_UNASSIGNED         -1

# define KBD_LTRIGGER_MAPPING   -2
# define KBD_RTRIGGER_MAPPING   -3
# define KBD_NORMAL_MAPPING     -1

 struct ti99_key_trans {
   int  key;
   int  ti99_id;
   int  shift;
   int  fctn;
   char name[10];
 };
  

  extern int psp_screenshot_mode;
  extern int psp_kbd_mapping[ KBD_ALL_BUTTONS ];
  extern int psp_kbd_mapping_L[ KBD_ALL_BUTTONS ];
  extern int psp_kbd_mapping_R[ KBD_ALL_BUTTONS ];
  extern int psp_kbd_presses[ KBD_ALL_BUTTONS ];
  extern int kbd_ltrigger_mapping_active;
  extern int kbd_rtrigger_mapping_active;

  extern struct ti99_key_trans psp_ti99_key_info[TI99K_MAX_KEY];

  extern void psp_kbd_default_settings();
  extern int  psp_update_keys(void);
  extern void kbd_wait_start(void);
  extern void psp_init_keyboard(void);
  extern void psp_kbd_wait_no_button(void);
  extern int  psp_kbd_is_danzeff_mode(void);
  extern int psp_kbd_load_mapping(char *kbd_filename);
  extern int psp_kbd_save_mapping(char *kbd_filename);
  extern void psp_kbd_display_active_mapping(void);
#ifdef __cplusplus
}
#endif
# endif
