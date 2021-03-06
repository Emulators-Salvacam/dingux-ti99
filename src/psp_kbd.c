/*
 *  Copyright (C) 2009 Ludovic Jacomme (ludovic.jacomme@gmail.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>

#include <SDL.h>

#include "global.h"
#include "psp_kbd.h"
#include "psp_menu.h"
#include "psp_sdl.h"
#include "psp_danzeff.h"

# define KBD_MIN_ANALOG_TIME  150000
# define KBD_MIN_START_TIME   800000
# define KBD_MAX_EVENT_TIME   500000
# define KBD_MIN_PENDING_TIME 300000
# define KBD_MIN_HOTKEY_TIME  1000000
# define KBD_MIN_DANZEFF_TIME 150000
# define KBD_MIN_COMMAND_TIME 100000
# define KBD_MIN_BATTCHECK_TIME 90000000 
# define KBD_MIN_AUTOFIRE_TIME   1000000

 static gp2xCtrlData    loc_button_data;
 static unsigned int   loc_last_event_time = 0;
 static unsigned int   loc_last_hotkey_time = 0;
 static long           first_time_stamp = -1;
 static long           first_time_auto_stamp = -1;
 static char           loc_button_press[ KBD_MAX_BUTTONS ]; 
 static char           loc_button_release[ KBD_MAX_BUTTONS ]; 

 unsigned int   kbd_button_mask[ KBD_MAX_BUTTONS ] =
 {
   GP2X_CTRL_UP         , /*  KBD_UP         */
   GP2X_CTRL_RIGHT      , /*  KBD_RIGHT      */
   GP2X_CTRL_DOWN       , /*  KBD_DOWN       */
   GP2X_CTRL_LEFT       , /*  KBD_LEFT       */
   GP2X_CTRL_TRIANGLE   , /*  KBD_TRIANGLE   */
   GP2X_CTRL_CIRCLE     , /*  KBD_CIRCLE     */
   GP2X_CTRL_CROSS      , /*  KBD_CROSS      */
   GP2X_CTRL_SQUARE     , /*  KBD_SQUARE     */
   GP2X_CTRL_SELECT     , /*  KBD_SELECT     */
   GP2X_CTRL_START      , /*  KBD_START      */
   GP2X_CTRL_LTRIGGER   , /*  KBD_LTRIGGER   */
   GP2X_CTRL_RTRIGGER   , /*  KBD_RTRIGGER   */
 };

 static char kbd_button_name[ KBD_ALL_BUTTONS ][20] =
 {
   "UP",
   "RIGHT",
   "DOWN",
   "LEFT",
# if defined(DINGUX_MODE) 
   "X",      // Triangle
   "A",      // Circle
   "B",      // Cross
   "Y",      // Square
# else
   "Y",      // Triangle
   "B",      // Circle
   "X",      // Cross
   "A",      // Square
# endif
   "SELECT",
   "START",
   "LTRIGGER",
   "RTRIGGER",
   "JOY_UP",
   "JOY_RIGHT",
   "JOY_DOWN",
   "JOY_LEFT"
 };

 static char kbd_button_name_L[ KBD_ALL_BUTTONS ][20] =
 {
   "L_UP",
   "L_RIGHT",
   "L_DOWN",
   "L_LEFT",
# if defined(DINGUX_MODE) 
   "L_X",      // Triangle
   "L_A",      // Circle
   "L_B",      // Cross
   "L_Y",      // Square
# else
   "L_Y",      // Triangle
   "L_B",      // Circle
   "L_X",      // Cross
   "L_A",      // Square
# endif
   "L_SELECT",
   "L_START",
   "L_LTRIGGER",
   "L_RTRIGGER",
   "L_JOY_UP",
   "L_JOY_RIGHT",
   "L_JOY_DOWN",
   "L_JOY_LEFT"
 };
 
  static char kbd_button_name_R[ KBD_ALL_BUTTONS ][20] =
 {
   "R_UP",
   "R_RIGHT",
   "R_DOWN",
   "R_LEFT",
# if defined(DINGUX_MODE)
   "R_X",      // Triangle
   "R_A",      // Circle
   "R_B",      // Cross
   "R_Y",      // Square
# else
   "R_Y",      // Triangle
   "R_B",      // Circle
   "R_X",      // Cross
   "R_A",      // Square
# endif
   "R_SELECT",
   "R_START",
   "R_LTRIGGER",
   "R_RTRIGGER",
   "R_JOY_UP",
   "R_JOY_RIGHT",
   "R_JOY_DOWN",
   "R_JOY_LEFT"
 };
 
  struct ti99_key_trans psp_ti99_key_info[TI99K_MAX_KEY]=
  {
    // TI99K            ID               SHIFT  NAME 
    { TI99K_0,          VK_0,             0, 0, "0"         },
    { TI99K_1,          VK_1,             0, 0, "1"         },
    { TI99K_2,          VK_2,             0, 0, "2"         },
    { TI99K_3,          VK_3,             0, 0, "3"         },
    { TI99K_4,          VK_4,             0, 0, "4"         },
    { TI99K_5,          VK_5,             0, 0, "5"         },
    { TI99K_6,          VK_6,             0, 0, "6"         },
    { TI99K_7,          VK_7,             0, 0, "7"         },
    { TI99K_8,          VK_8,             0, 0, "8"         },
    { TI99K_9,          VK_9,             0, 0, "9"         },
    { TI99K_A,          VK_A,             0, 0, "A"         },
    { TI99K_B,          VK_B,             0, 0, "B"         },
    { TI99K_C,          VK_C,             0, 0, "C"         },
    { TI99K_D,          VK_D,             0, 0, "D"         },
    { TI99K_E,          VK_E,             0, 0, "E"         },
    { TI99K_F,          VK_F,             0, 0, "F"         },
    { TI99K_G,          VK_G,             0, 0, "G"         },
    { TI99K_H,          VK_H,             0, 0, "H"         },
    { TI99K_I,          VK_I,             0, 0, "I"         },
    { TI99K_J,          VK_J,             0, 0, "J"         },
    { TI99K_K,          VK_K,             0, 0, "K"         },
    { TI99K_L,          VK_L,             0, 0, "L"         },
    { TI99K_M,          VK_M,             0, 0, "M"         },
    { TI99K_N,          VK_N,             0, 0, "N"         },
    { TI99K_O,          VK_O,             0, 0, "O"         },
    { TI99K_P,          VK_P,             0, 0, "P"         },
    { TI99K_Q,          VK_Q,             0, 0, "Q"         },
    { TI99K_R,          VK_R,             0, 0, "R"         },
    { TI99K_S,          VK_S,             0, 0, "S"         },
    { TI99K_T,          VK_T,             0, 0, "T"         },
    { TI99K_U,          VK_U,             0, 0, "U"         },
    { TI99K_V,          VK_V,             0, 0, "V"         },
    { TI99K_W,          VK_W,             0, 0, "W"         },
    { TI99K_X,          VK_X,             0, 0, "X"         },
    { TI99K_Y,          VK_Y,             0, 0, "Y"         },
    { TI99K_Z,          VK_Z,             0, 0, "Z"         },
    { TI99K_a,          VK_A,             1, 0, "a"         },
    { TI99K_b,          VK_B,             1, 0, "b"         },
    { TI99K_c,          VK_C,             1, 0, "c"         },
    { TI99K_d,          VK_D,             1, 0, "d"         },
    { TI99K_e,          VK_E,             1, 0, "e"         },
    { TI99K_f,          VK_F,             1, 0, "f"         },
    { TI99K_g,          VK_G,             1, 0, "g"         },
    { TI99K_h,          VK_H,             1, 0, "h"         },
    { TI99K_i,          VK_I,             1, 0, "i"         },
    { TI99K_j,          VK_J,             1, 0, "j"         },
    { TI99K_k,          VK_K,             1, 0, "k"         },
    { TI99K_l,          VK_L,             1, 0, "l"         },
    { TI99K_m,          VK_M,             1, 0, "m"         },
    { TI99K_n,          VK_N,             1, 0, "n"         },
    { TI99K_o,          VK_O,             1, 0, "o"         },
    { TI99K_p,          VK_P,             1, 0, "p"         },
    { TI99K_q,          VK_Q,             1, 0, "q"         },
    { TI99K_r,          VK_R,             1, 0, "r"         },
    { TI99K_s,          VK_S,             1, 0, "s"         },
    { TI99K_t,          VK_T,             1, 0, "t"         },
    { TI99K_u,          VK_U,             1, 0, "u"         },
    { TI99K_v,          VK_V,             1, 0, "v"         },
    { TI99K_w,          VK_W,             1, 0, "w"         },
    { TI99K_x,          VK_X,             1, 0, "x"         },
    { TI99K_y,          VK_Y,             1, 0, "y"         },
    { TI99K_z,          VK_Z,             1, 0, "z"         },
    { TI99K_QUOTE,      VK_0,             0, 1, "\'"        },
    { TI99K_COMMA,      VK_COMMA,         0, 0, ","         },
    { TI99K_LESS,       VK_COMMA,         1, 0, "<"         },
    { TI99K_PERIOD,     VK_PERIOD,        0, 0, "."         },
    { TI99K_GREATER,    VK_PERIOD,        1, 0, ">"         },
    { TI99K_SEMICOLON,  VK_SEMICOLON,     0, 0, ";"         },
    { TI99K_COLON,      VK_SEMICOLON,     1, 0, ":"         },
    { TI99K_UNDERSCORE, VK_U,             0, 1, "_"         },
    { TI99K_PIPE,       VK_A,             0, 1, "|"         },
    { TI99K_EQUAL,      VK_EQUALS,        0, 0, "="         },
    { TI99K_PLUS,       VK_EQUALS,        1, 0, "+"         },
    { TI99K_TILDA    ,  VK_W,             0, 1, "~"         },
    { TI99K_DBLQUOTE,   VK_P,             0, 1, "\""         },
    { TI99K_QUESTION,   VK_I,             0, 1, "?"         },
    { TI99K_SLASH,      VK_DIVIDE,        0, 0, "/"         },
    { TI99K_MINUS,      VK_DIVIDE,        1, 0, "-"         },
    { TI99K_LBRACKET,   VK_R,             0, 1, "["         },
    { TI99K_RBRACKET,   VK_T,             0, 1, "]"         },
    { TI99K_LCBRACE,    VK_F,             0, 1, "{"         },
    { TI99K_RCBRACE,    VK_G,             0, 1, "}"         },
    { TI99K_SPACE,      VK_SPACE,         0, 0, "SPACE"     },
    { TI99K_EXCLAMATN,  VK_1,             1, 0, "!"         },
    { TI99K_AT,         VK_2,             1, 0, "@"         },
    { TI99K_HASH,       VK_3,             1, 0, "#"         },
    { TI99K_DOLLAR,     VK_4,             1, 0, "$"         },
    { TI99K_PERCENT,    VK_5,             1, 0, "%"         },
    { TI99K_POWER,      VK_6,             1, 0, "^"         },
    { TI99K_AMPERSAND,  VK_7,             1, 0, "&"         },
    { TI99K_ASTERISK,   VK_8,             1, 0, "*"         },
    { TI99K_LPAREN,     VK_9,             1, 0, "("         },
    { TI99K_RPAREN,     VK_0,             1, 0, ")"         },
    { TI99K_BACKSLASH,  VK_Z,             0, 1, "\\"        },
    { TI99K_BACKQUOTE,  VK_C,             0, 1, "`"         },
    { TI99K_TAB,        VK_7,             0, 1, "TAB"       },
    { TI99K_BACKSPACE,  VK_S,             0, 1, "BACKSPACE" },
    { TI99K_LEFT,       VK_S,             0, 1, "LEFT"      },
    { TI99K_RIGHT,      VK_D,             0, 1, "RIGHT"     },
    { TI99K_UP,         VK_E,             0, 1, "UP"        },
    { TI99K_DOWN,       VK_X,             0, 1, "DOWN"      },
    { TI99K_DELETE,     VK_1,             0, 1, "DELETE"    },
    { TI99K_RETURN,     VK_ENTER,         0, 0, "RETURN"    },
    { TI99K_SHIFT,      VK_SHIFT,         0, 0, "SHIFT"     },
    { TI99K_ALT,        VK_FCTN,          0, 0, "ALT"       },
    { TI99K_CTRL,       VK_CTRL,          0, 0, "CTRL"      },
    { TI99K_CAPSLOCK,   VK_CAPSLOCK,      0, 0, "CAPSLOCK"  },
    { TI99K_JOY_UP,     0,                0, 0, "JOY_UP"    },
    { TI99K_JOY_DOWN,   0,                0, 0, "JOY_DOWN"  },
    { TI99K_JOY_LEFT,   0,                0, 0, "JOY_LEFT"  },
    { TI99K_JOY_RIGHT,  0,                0, 0, "JOY_RIGHT" },
    { TI99K_JOY_FIRE,   0,                0, 0, "JOY_FIRE"  },

    { TI99_C_FPS,       0,    0,   0,    "C_FPS" },
    { TI99_C_JOY,       0,    0,   0,    "C_JOY" },
    { TI99_C_RENDER,    0,    0,   0,    "C_RENDER" },
    { TI99_C_LOAD,      0,    0,   0,    "C_LOAD" },
    { TI99_C_SAVE,      0,    0,   0,    "C_SAVE" },
    { TI99_C_RESET,     0,    0,   0,    "C_RESET" },
    { TI99_C_AUTOFIRE,  0,    0,   0,    "C_AUTOFIRE" },
    { TI99_C_INCFIRE,   0,    0,   0,    "C_INCFIRE" },
    { TI99_C_DECFIRE,   0,    0,   0,    "C_DECFIRE" },
    { TI99_C_SCREEN,    0,    0,   0,    "C_SCREEN" }
  };

  static int loc_default_mapping[ KBD_ALL_BUTTONS ] = {
    TI99K_UP              , /*  KBD_UP         */
    TI99K_RIGHT           , /*  KBD_RIGHT      */
    TI99K_DOWN            , /*  KBD_DOWN       */
    TI99K_LEFT            , /*  KBD_LEFT       */
    TI99K_RETURN          , /*  KBD_TRIANGLE   */
    TI99K_1               , /*  KBD_CIRCLE     */
    TI99K_JOY_FIRE        , /*  KBD_CROSS      */
    TI99K_2               , /*  KBD_SQUARE     */
    -1                    , /*  KBD_SELECT     */
    -1                    , /*  KBD_START      */
    KBD_LTRIGGER_MAPPING  , /*  KBD_LTRIGGER   */
    KBD_RTRIGGER_MAPPING  , /*  KBD_RTRIGGER   */
    TI99K_JOY_UP          , /*  KBD_JOY_UP     */
    TI99K_JOY_RIGHT       , /*  KBD_JOY_RIGHT  */
    TI99K_JOY_DOWN        , /*  KBD_JOY_DOWN   */
    TI99K_JOY_LEFT          /*  KBD_JOY_LEFT   */
  };

  static int loc_default_mapping_L[ KBD_ALL_BUTTONS ] = {
    TI99K_UP              , /*  KBD_UP         */
    TI99_C_RENDER         , /*  KBD_RIGHT      */
    TI99K_DOWN            , /*  KBD_DOWN       */
    TI99_C_RENDER         , /*  KBD_LEFT       */
    TI99_C_LOAD           , /*  KBD_TRIANGLE   */
    TI99_C_JOY            , /*  KBD_CIRCLE     */
    TI99_C_SAVE           , /*  KBD_CROSS      */
    TI99_C_FPS            , /*  KBD_SQUARE     */
    -1                    , /*  KBD_SELECT     */
    -1                    , /*  KBD_START      */
    KBD_LTRIGGER_MAPPING  , /*  KBD_LTRIGGER   */
    KBD_RTRIGGER_MAPPING  , /*  KBD_RTRIGGER   */
    TI99K_JOY_UP          , /*  KBD_JOY_UP     */
    TI99K_JOY_RIGHT       , /*  KBD_JOY_RIGHT  */
    TI99K_JOY_DOWN        , /*  KBD_JOY_DOWN   */
    TI99K_JOY_LEFT          /*  KBD_JOY_LEFT   */
  };

  static int loc_default_mapping_R[ KBD_ALL_BUTTONS ] = {
    TI99K_UP              , /*  KBD_UP         */
    TI99_C_INCFIRE        , /*  KBD_RIGHT      */
    TI99K_DOWN            , /*  KBD_DOWN       */
    TI99_C_DECFIRE        , /*  KBD_LEFT       */
    TI99_C_RESET          , /*  KBD_TRIANGLE   */
    TI99K_4               , /*  KBD_CIRCLE     */
    TI99_C_AUTOFIRE       , /*  KBD_CROSS      */
    TI99K_SPACE           , /*  KBD_SQUARE     */
    -1                    , /*  KBD_SELECT     */
    -1                    , /*  KBD_START      */
    KBD_LTRIGGER_MAPPING  , /*  KBD_LTRIGGER   */
    KBD_RTRIGGER_MAPPING  , /*  KBD_RTRIGGER   */
    TI99K_JOY_UP          , /*  KBD_JOY_UP     */
    TI99K_JOY_RIGHT       , /*  KBD_JOY_RIGHT  */
    TI99K_JOY_DOWN        , /*  KBD_JOY_DOWN   */
    TI99K_JOY_LEFT          /*  KBD_JOY_LEFT   */
  };

# define KBD_MAX_ENTRIES   107

  int kbd_layout[KBD_MAX_ENTRIES][2] = {
    /* Key            Ascii */
    { TI99K_0,          '0' },
    { TI99K_1,          '1' },
    { TI99K_2,          '2' },
    { TI99K_3,          '3' },
    { TI99K_4,          '4' },
    { TI99K_5,          '5' },
    { TI99K_6,          '6' },
    { TI99K_7,          '7' },
    { TI99K_8,          '8' },
    { TI99K_9,          '9' },
    { TI99K_A,          'A' },
    { TI99K_B,          'B' },
    { TI99K_C,          'C' },
    { TI99K_D,          'D' },
    { TI99K_E,          'E' },
    { TI99K_F,          'F' },
    { TI99K_G,          'G' },
    { TI99K_H,          'H' },
    { TI99K_I,          'I' },
    { TI99K_J,          'J' },
    { TI99K_K,          'K' },
    { TI99K_L,          'L' },
    { TI99K_M,          'M' },
    { TI99K_N,          'N' },
    { TI99K_O,          'O' },
    { TI99K_P,          'P' },
    { TI99K_Q,          'Q' },
    { TI99K_R,          'R' },
    { TI99K_S,          'S' },
    { TI99K_T,          'T' },
    { TI99K_U,          'U' },
    { TI99K_V,          'V' },
    { TI99K_W,          'W' },
    { TI99K_X,          'X' },
    { TI99K_Y,          'Y' },
    { TI99K_Z,          'Z' },
    { TI99K_a,          'a' },
    { TI99K_b,          'b' },
    { TI99K_c,          'c' },
    { TI99K_d,          'd' },
    { TI99K_e,          'e' },
    { TI99K_f,          'f' },
    { TI99K_g,          'g' },
    { TI99K_h,          'h' },
    { TI99K_i,          'i' },
    { TI99K_j,          'j' },
    { TI99K_k,          'k' },
    { TI99K_l,          'l' },
    { TI99K_m,          'm' },
    { TI99K_n,          'n' },
    { TI99K_o,          'o' },
    { TI99K_p,          'p' },
    { TI99K_q,          'q' },
    { TI99K_r,          'r' },
    { TI99K_s,          's' },
    { TI99K_t,          't' },
    { TI99K_u,          'u' },
    { TI99K_v,          'v' },
    { TI99K_w,          'w' },
    { TI99K_x,          'x' },
    { TI99K_y,          'y' },
    { TI99K_z,          'z' },
    { TI99K_QUOTE,      '\'' },
    { TI99K_COMMA,      ',' },
    { TI99K_LESS,       '<' },
    { TI99K_PERIOD,     '.' },
    { TI99K_GREATER,    '>' },
    { TI99K_SEMICOLON,  ';' },
    { TI99K_COLON,      ':' },
    { TI99K_UNDERSCORE, '_'  },
    { TI99K_PIPE,       '|' },
    { TI99K_EQUAL,      '=' },
    { TI99K_PLUS,       '+' },
    { TI99K_TILDA    ,  '~' },
    { TI99K_DBLQUOTE,   '"' },
    { TI99K_QUESTION,   '?' },
    { TI99K_SLASH,      '/' },
    { TI99K_MINUS,      '-' },
    { TI99K_LBRACKET,   '[' },
    { TI99K_RBRACKET,   ']' },
    { TI99K_LCBRACE,    '{' },
    { TI99K_RCBRACE,    '}' },
    { TI99K_SPACE,      ' ' },
    { TI99K_EXCLAMATN,  '!' },
    { TI99K_AT,         '@' },
    { TI99K_HASH,       '#' },
    { TI99K_DOLLAR,     '$' },
    { TI99K_PERCENT,    '%' },
    { TI99K_POWER,      '^' },
    { TI99K_AMPERSAND,  '&' },
    { TI99K_ASTERISK,   '*' },
    { TI99K_LPAREN,     '(' },
    { TI99K_RPAREN,     ')' },
    { TI99K_BACKSLASH,  '\\' },
    { TI99K_BACKQUOTE,  '`' },
    { TI99K_TAB,        DANZEFF_TAB },
    { TI99K_BACKSPACE,  DANZEFF_DEL },
    { TI99K_LEFT,       -1  },
    { TI99K_RIGHT,      -1  },
    { TI99K_UP,         -1  },
    { TI99K_DOWN,       -1  },
    { TI99K_DELETE,     DANZEFF_SUPPR  },
    { TI99K_RETURN,     DANZEFF_ENTER    },
    { TI99K_SHIFT,      DANZEFF_SHIFT    },
    { TI99K_ALT,        DANZEFF_ALT  },
    { TI99K_CTRL,       DANZEFF_CONTROL  },
    { TI99K_CAPSLOCK,   DANZEFF_CAPSLOCK }
  };

 int psp_kbd_mapping[ KBD_ALL_BUTTONS ];
 int psp_kbd_mapping_L[ KBD_ALL_BUTTONS ];
 int psp_kbd_mapping_R[ KBD_ALL_BUTTONS ];
 int psp_kbd_presses[ KBD_ALL_BUTTONS ];
 int kbd_ltrigger_mapping_active;
 int kbd_rtrigger_mapping_active;

 static int danzeff_ti99_key     = 0;
 static int danzeff_ti99_pending = 0;
 static int danzeff_mode        = 0;


       char command_keys[ 128 ];
 static int command_mode        = 0;
 static int command_index       = 0;
 static int command_size        = 0;
 static int command_ti99_pending = 0;
 static int command_ti99_key     = 0;
  
 static int loc_joy_mask = 0;
 extern void ti99_HandleKeyPress(int sym_id, int key1_id, int key2_id, int press);
 extern void ti99_HandleJoystick(int joy_mask);

static void
ti99_handle_joystick(int joy_id, int press) 
{
  if (press) loc_joy_mask |=  (1 << joy_id);
  else       loc_joy_mask &= ~(1 << joy_id);
  ti99_HandleJoystick( loc_joy_mask );
}

int
ti99_key_event(int ti99_idx, int press)
{
  int ti99_id    = 0;
  int ti99_shift = 0;
  int ti99_fctn  = 0;

  if ((ti99_idx >=           0) && 
      (ti99_idx < TI99K_JOY_UP )) {
    ti99_id = psp_ti99_key_info[ti99_idx].ti99_id;

    if (press) {
      ti99_shift = psp_ti99_key_info[ti99_idx].shift;
      ti99_fctn  = psp_ti99_key_info[ti99_idx].fctn;

      if (ti99_shift) {
        ti99_HandleKeyPress(ti99_idx, VK_SHIFT, ti99_id, 1);
      } else 
      if (ti99_fctn) {
        ti99_HandleKeyPress(ti99_idx, VK_FCTN , ti99_id, 1);
      } else {
        ti99_HandleKeyPress(ti99_idx, ti99_id , 0      , 1);
      }

    } else {
      ti99_HandleKeyPress(ti99_idx, 0, 0, 0);
    }

  } else
  if ((ti99_idx >= TI99K_JOY_UP) &&
      (ti99_idx <= TI99K_JOY_FIRE)) {
    ti99_handle_joystick(ti99_idx - TI99K_JOY_UP, press);
  } else
  if ((ti99_idx >= TI99_C_FPS) &&
      (ti99_idx <= TI99_C_SCREEN)) {

    if (press) {
      gp2xCtrlData c;
      gp2xCtrlPeekBufferPositive(&c, 1);
      if ((c.TimeStamp - loc_last_hotkey_time) > KBD_MIN_HOTKEY_TIME) {
        loc_last_hotkey_time = c.TimeStamp;
        ti99_treat_command_key(ti99_idx);
      }
    }
  }
  return 0;
}

int 
ti99_kbd_reset()
{
  loc_joy_mask = 0;
  ti99_HandleJoystick( 0 );
  ti99_reset_keyboard();
  return 0;
}

int
ti99_get_key_from_ascii(int key_ascii)
{
  int index;
  for (index = 0; index < KBD_MAX_ENTRIES; index++) {
   if (kbd_layout[index][1] == key_ascii) return kbd_layout[index][0];
  }
  return -1;
}

void
psp_kbd_default_settings()
{
  memcpy(psp_kbd_mapping  , loc_default_mapping, sizeof(loc_default_mapping));
  memcpy(psp_kbd_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_L));
  memcpy(psp_kbd_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));
}

int
psp_kbd_reset_hotkeys(void)
{
  int index;
  int key_id;
  for (index = 0; index < KBD_ALL_BUTTONS; index++) {
    key_id = loc_default_mapping[index];
    if ((key_id >= TI99_C_FPS) && (key_id <= TI99_C_SCREEN)) {
      psp_kbd_mapping[index] = key_id;
    }
    key_id = loc_default_mapping_L[index];
    if ((key_id >= TI99_C_FPS) && (key_id <= TI99_C_SCREEN)) {
      psp_kbd_mapping_L[index] = key_id;
    }
    key_id = loc_default_mapping_R[index];
    if ((key_id >= TI99_C_FPS) && (key_id <= TI99_C_SCREEN)) {
      psp_kbd_mapping_R[index] = key_id;
    }
  }
  return 0;
}

int
psp_kbd_load_mapping(char *kbd_filename)
{
  FILE    *KbdFile;
  int      error = 0;

  KbdFile = fopen(kbd_filename, "r");
  error   = 1;

  if (KbdFile != (FILE*)0) {
  psp_kbd_load_mapping_file(KbdFile);
  error = 0;
    fclose(KbdFile);
  }

  kbd_ltrigger_mapping_active = 0;
  kbd_rtrigger_mapping_active = 0;
    
  return error;
}

int
psp_kbd_load_mapping_file(FILE *KbdFile)
{
  char     Buffer[512];
  char    *Scan;
  int      tmp_mapping[KBD_ALL_BUTTONS];
  int      tmp_mapping_L[KBD_ALL_BUTTONS];
  int      tmp_mapping_R[KBD_ALL_BUTTONS];
  int      ti99_key_id = 0;
  int      kbd_id = 0;

  memcpy(tmp_mapping  , loc_default_mapping  , sizeof(loc_default_mapping));
  memcpy(tmp_mapping_L, loc_default_mapping_L, sizeof(loc_default_mapping_R));
  memcpy(tmp_mapping_R, loc_default_mapping_R, sizeof(loc_default_mapping_R));

  while (fgets(Buffer,512,KbdFile) != (char *)0) {
      
      Scan = strchr(Buffer,'\n');
      if (Scan) *Scan = '\0';
      /* For this #@$% of windows ! */
      Scan = strchr(Buffer,'\r');
      if (Scan) *Scan = '\0';
      if (Buffer[0] == '#') continue;

      Scan = strchr(Buffer,'=');
      if (! Scan) continue;
    
      *Scan = '\0';
      ti99_key_id = atoi(Scan + 1);

      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,kbd_button_name[kbd_id])) {
          tmp_mapping[kbd_id] = ti99_key_id;
          //break;
        }
      }
      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,kbd_button_name_L[kbd_id])) {
          tmp_mapping_L[kbd_id] = ti99_key_id;
          //break;
        }
      }
      for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++) {
        if (!strcasecmp(Buffer,kbd_button_name_R[kbd_id])) {
          tmp_mapping_R[kbd_id] = ti99_key_id;
          //break;
        }
      }
  }

  memcpy(psp_kbd_mapping, tmp_mapping, sizeof(psp_kbd_mapping));
  memcpy(psp_kbd_mapping_L, tmp_mapping_L, sizeof(psp_kbd_mapping_L));
  memcpy(psp_kbd_mapping_R, tmp_mapping_R, sizeof(psp_kbd_mapping_R));
  
  return 0;
}

int
psp_kbd_save_mapping(char *kbd_filename)
{
  FILE    *KbdFile;
  int      kbd_id = 0;
  int      error = 0;

  KbdFile = fopen(kbd_filename, "w");
  error   = 1;

  if (KbdFile != (FILE*)0) {

    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", kbd_button_name[kbd_id], psp_kbd_mapping[kbd_id]);
    }
    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", kbd_button_name_L[kbd_id], psp_kbd_mapping_L[kbd_id]);
    }
    for (kbd_id = 0; kbd_id < KBD_ALL_BUTTONS; kbd_id++)
    {
      fprintf(KbdFile, "%s=%d\n", kbd_button_name_R[kbd_id], psp_kbd_mapping_R[kbd_id]);
    }
    error = 0;
    fclose(KbdFile);
  }

  return error;
}

int
psp_kbd_enter_command()
{
  gp2xCtrlData  c;

  unsigned int command_key = 0;
  int          ti99_key     = 0;

  gp2xCtrlPeekBufferPositive(&c, 1);

  if (command_ti99_pending) 
  {
    if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_COMMAND_TIME) {
      loc_last_event_time = c.TimeStamp;
      command_ti99_pending = 0;
      ti99_key_event(command_ti99_key, 0);
    }

    return 0;
  }

  if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_COMMAND_TIME) {
    loc_last_event_time = c.TimeStamp;

    if (command_index >= command_size) {

      command_mode  = 0;
      command_index = 0;
      command_size  = 0;

      command_ti99_pending = 0;
      command_ti99_key     = 0;

      return 0;
    }
  
    command_key = command_keys[command_index++];
    ti99_key = ti99_get_key_from_ascii(command_key);

    if (ti99_key != -1) {
      command_ti99_key     = ti99_key;
      command_ti99_pending = 1;
      ti99_key_event(command_ti99_key, 1);
    }

    return 1;
  }

  return 0;
}

int 
psp_kbd_is_danzeff_mode()
{
  return danzeff_mode;
}

int
psp_kbd_enter_danzeff()
{
  unsigned int danzeff_key = 0;
  int          ti99_key     = 0;
  gp2xCtrlData  c;

  if (! danzeff_mode) {
    psp_init_keyboard();
    danzeff_mode = 1;
  }

  gp2xCtrlPeekBufferPositive(&c, 1);

  if (danzeff_ti99_pending) 
  {
    if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_PENDING_TIME) {
      loc_last_event_time = c.TimeStamp;
      danzeff_ti99_pending = 0;
      ti99_key_event(danzeff_ti99_key, 0);
    }

    return 0;
  }

  if ((c.TimeStamp - loc_last_event_time) > KBD_MIN_DANZEFF_TIME) {
    loc_last_event_time = c.TimeStamp;
  
    gp2xCtrlPeekBufferPositive(&c, 1);
    danzeff_key = danzeff_readInput(&c);
  }

  if (danzeff_key > DANZEFF_START) {
    ti99_key = ti99_get_key_from_ascii(danzeff_key);

    if (ti99_key != -1) {
      danzeff_ti99_key     = ti99_key;
      danzeff_ti99_pending = 1;
      ti99_key_event(danzeff_ti99_key, 1);
    }

    return 1;

  } else if (danzeff_key == DANZEFF_START) {
    danzeff_mode       = 0;
    danzeff_ti99_pending = 0;
    danzeff_ti99_key     = 0;

    psp_kbd_wait_no_button();

  } else if (danzeff_key == DANZEFF_SELECT) {
    danzeff_mode       = 0;
    danzeff_ti99_pending = 0;
    danzeff_ti99_key     = 0;
    psp_main_menu();
    psp_init_keyboard();

    psp_kbd_wait_no_button();
  }

  return 0;
}


int
ti99_decode_key(int psp_b, int button_pressed)
{
  int wake = 0;
  int reverse_analog = ! TI99.psp_reverse_analog;

  if (reverse_analog) {
    if ((psp_b >= KBD_JOY_UP  ) &&
        (psp_b <= KBD_JOY_LEFT)) {
      psp_b = psp_b - KBD_JOY_UP + KBD_UP;
    } else
    if ((psp_b >= KBD_UP  ) &&
        (psp_b <= KBD_LEFT)) {
      psp_b = psp_b - KBD_UP + KBD_JOY_UP;
    }
  }

  if (psp_b == KBD_START) {
     if (button_pressed) psp_kbd_enter_danzeff();
  } else
  if (psp_b == KBD_SELECT) {
    if (button_pressed) {
      psp_main_menu();
      psp_init_keyboard();
    }
  } else {
 
    if (psp_kbd_mapping[psp_b] >= 0) {
      wake = 1;
      if (button_pressed) {
        // Determine which buton to press first (ie which mapping is currently active)
        if (kbd_ltrigger_mapping_active) {
          // Use ltrigger mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping_L[psp_b];
          ti99_key_event(psp_kbd_presses[psp_b], button_pressed);
        } else
        if (kbd_rtrigger_mapping_active) {
          // Use rtrigger mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping_R[psp_b];
          ti99_key_event(psp_kbd_presses[psp_b], button_pressed);
        } else {
          // Use standard mapping
          psp_kbd_presses[psp_b] = psp_kbd_mapping[psp_b];
          ti99_key_event(psp_kbd_presses[psp_b], button_pressed);
        }
      } else {
          // Determine which button to release (ie what was pressed before)
          ti99_key_event(psp_kbd_presses[psp_b], button_pressed);
      }

    } else {
      if (psp_kbd_mapping[psp_b] == KBD_LTRIGGER_MAPPING) {
        kbd_ltrigger_mapping_active = button_pressed;
        kbd_rtrigger_mapping_active = 0;
      } else
      if (psp_kbd_mapping[psp_b] == KBD_RTRIGGER_MAPPING) {
        kbd_rtrigger_mapping_active = button_pressed;
        kbd_ltrigger_mapping_active = 0;
      }
    }
  }
  return 0;
}

void
kbd_change_auto_fire(int auto_fire)
{
  TI99.ti99_auto_fire = auto_fire;
  if (TI99.ti99_auto_fire_pressed) {
    ti99_key_event(TI99K_JOY_FIRE, 0);
    TI99.ti99_auto_fire_pressed = 0;
  }
}

static int 
kbd_reset_button_status(void)
{
  int b = 0;
  /* Reset Button status */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    loc_button_press[b]   = 0;
    loc_button_release[b] = 0;
  }
  psp_init_keyboard();
  return 0;
}

int
kbd_scan_keyboard(void)
{
  gp2xCtrlData c;
  long        delta_stamp;
  int         event;
  int         b;

  event = 0;
  gp2xCtrlPeekBufferPositive( &c, 1 );

  if (TI99.ti99_auto_fire) {
    delta_stamp = c.TimeStamp - first_time_auto_stamp;
    if ((delta_stamp < 0) || 
        (delta_stamp > (KBD_MIN_AUTOFIRE_TIME / (1 + TI99.ti99_auto_fire_period)))) {
      first_time_auto_stamp = c.TimeStamp;
      ti99_key_event(TI99K_JOY_FIRE, TI99.ti99_auto_fire_pressed);
      TI99.ti99_auto_fire_pressed = ! TI99.ti99_auto_fire_pressed;
    }
  }

  for (b = 0; b < KBD_MAX_BUTTONS; b++) 
  {
    if (c.Buttons & kbd_button_mask[b]) {
      if (!(loc_button_data.Buttons & kbd_button_mask[b])) {
        loc_button_press[b] = 1;
        event = 1;
      }
    } else {
      if (loc_button_data.Buttons & kbd_button_mask[b]) {
        loc_button_release[b] = 1;
        loc_button_press[b] = 0;
        event = 1;
      }
    }
  }
  memcpy(&loc_button_data,&c,sizeof(gp2xCtrlData));

  return event;
}

void
psp_kbd_wait_start(void)
{
  while (1)
  {
    gp2xCtrlData c;
    gp2xCtrlReadBufferPositive(&c, 1);
    if (c.Buttons & GP2X_CTRL_START) break;
  }
  psp_kbd_wait_no_button();
}

void
psp_init_keyboard(void)
{
  ti99_kbd_reset();
  kbd_ltrigger_mapping_active = 0;
  kbd_rtrigger_mapping_active = 0;
}

void
psp_kbd_wait_no_button(void)
{
  gp2xCtrlData c;

  do {
   gp2xCtrlPeekBufferPositive(&c, 1);
  } while (c.Buttons != 0);
} 

void
psp_kbd_wait_button(void)
{
  gp2xCtrlData c;

  do {
   gp2xCtrlReadBufferPositive(&c, 1);
  } while (c.Buttons == 0);
} 

int
psp_update_keys(void)
{
  gp2xCtrlData c;
  int          b;

  static char first_time = 1;
  static int release_pending = 0;

  if (first_time) {

    gp2xCtrlPeekBufferPositive(&c, 1);
    if (first_time_stamp == -1) first_time_stamp = c.TimeStamp;

    first_time      = 0;
    release_pending = 0;

    for (b = 0; b < KBD_MAX_BUTTONS; b++) {
      loc_button_release[b] = 0;
      loc_button_press[b] = 0;
    }
    gp2xCtrlPeekBufferPositive(&loc_button_data, 1);

    psp_main_menu();
    psp_init_keyboard();

    return 0;
  }

  ti99_apply_cheats();

  if (command_mode) {
    return psp_kbd_enter_command();
  }

  if (danzeff_mode) {
    return psp_kbd_enter_danzeff();
  }

  if (release_pending)
  {
    release_pending = 0;
    for (b = 0; b < KBD_MAX_BUTTONS; b++) {
      if (loc_button_release[b]) {
        loc_button_release[b] = 0;
        loc_button_press[b] = 0;
        ti99_decode_key(b, 0);
      }
    }
  }

  kbd_scan_keyboard();

  /* check press event */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    if (loc_button_press[b]) {
      loc_button_press[b] = 0;
      release_pending     = 0;
      ti99_decode_key(b, 1);
    }
  }
  /* check release event */
  for (b = 0; b < KBD_MAX_BUTTONS; b++) {
    if (loc_button_release[b]) {
      release_pending = 1;
      break;
    } 
  }

  return 0;
}
