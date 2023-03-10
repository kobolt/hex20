#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <curses.h>

#include "console.h"
#include "mem.h"
#include "panic.h"



#define CONSOLE_SCREEN_UPDATE 10000
#define CONSOLE_KEYBOARD_UPDATE 20000
#define CONSOLE_KEYBOARD_RELEASE 500

#define GATE_A 0
#define GATE_B 1

typedef enum {
  SCANCODE_0            = 0x00,
  SCANCODE_1            = 0x01,
  SCANCODE_2            = 0x02,
  SCANCODE_3            = 0x03,
  SCANCODE_4            = 0x04,
  SCANCODE_5            = 0x05,
  SCANCODE_6            = 0x06,
  SCANCODE_7            = 0x07,
  SCANCODE_8            = 0x08,
  SCANCODE_9            = 0x09,
  SCANCODE_COLON        = 0x0A,
  SCANCODE_SEMICOLON    = 0x0B,
  SCANCODE_COMMA        = 0x0C,
  SCANCODE_MINUS        = 0x0D,
  SCANCODE_PERIOD       = 0x0E,
  SCANCODE_SLASH        = 0x0F,
  SCANCODE_AT           = 0x10,
  SCANCODE_A            = 0x11,
  SCANCODE_B            = 0x12,
  SCANCODE_C            = 0x13,
  SCANCODE_D            = 0x14,
  SCANCODE_E            = 0x15,
  SCANCODE_F            = 0x16,
  SCANCODE_G            = 0x17,
  SCANCODE_H            = 0x18,
  SCANCODE_I            = 0x19,
  SCANCODE_J            = 0x1A,
  SCANCODE_K            = 0x1B,
  SCANCODE_L            = 0x1C,
  SCANCODE_M            = 0x1D,
  SCANCODE_N            = 0x1E,
  SCANCODE_O            = 0x1F,
  SCANCODE_P            = 0x20,
  SCANCODE_Q            = 0x21,
  SCANCODE_R            = 0x22,
  SCANCODE_S            = 0x23,
  SCANCODE_T            = 0x24,
  SCANCODE_U            = 0x25,
  SCANCODE_V            = 0x26,
  SCANCODE_W            = 0x27,
  SCANCODE_X            = 0x28,
  SCANCODE_Y            = 0x29,
  SCANCODE_Z            = 0x2A,
  SCANCODE_BRACKETLEFT  = 0x2B,
  SCANCODE_BRACKETRIGHT = 0x2C,
  SCANCODE_BACKSLASH    = 0x2D,
  SCANCODE_RIGHT        = 0x2E,
  SCANCODE_LEFT         = 0x2F,
  SCANCODE_RETURN       = 0x30,
  SCANCODE_SPACE        = 0x31,
  SCANCODE_TAB          = 0x32,
  SCANCODE_NUM          = 0x35,
  SCANCODE_CAPS         = 0x37,
  SCANCODE_CLEAR        = 0x38,
  SCANCODE_SCRN         = 0x39,
  SCANCODE_BREAK        = 0x3A,
  SCANCODE_PAUSE        = 0x3B,
  SCANCODE_DEL          = 0x3C,
  SCANCODE_MENU         = 0x3D,
  SCANCODE_PF1          = 0x40,
  SCANCODE_PF2          = 0x41,
  SCANCODE_PF3          = 0x42,
  SCANCODE_PF4          = 0x43,
  SCANCODE_PF5          = 0x44,
  SCANCODE_FEED         = 0x45,
  SCANCODE_DIP1         = 0x48,
  SCANCODE_DIP2         = 0x49,
  SCANCODE_DIP3         = 0x4A,
  SCANCODE_DIP4         = 0x4B,
  SCANCODE_SHIFT        = 0x4D,
  SCANCODE_CTRL         = 0x4E,
  SCANCODE_PRINTER      = 0x4F,
} scancode_t;

static console_mode_t console_mode = CONSOLE_MODE_NONE;
static console_charset_t console_charset = CONSOLE_CHARSET_US;

static uint8_t console_keyboard[8][2]; /* 8 Lines and Gate A & B for each. */

static int console_lcd_controller = 0;
static bool console_lcd_command = false;
static bool console_lcd_cmd64_seen = false;
static bool console_lcd_cmd63_seen = false;
static int console_lcd_row = 0;
static int console_lcd_col = 0;
static int console_lcd_pixel_col = 0;
static int console_lcd_pixel_row = 0;
static int console_lcd_clock_tick = 0;
static int console_lcd_serial_cycles_left = 0;



static void console_keyboard_set(scancode_t scancode)
{
  /* Set line and gate according to scancode. */
  if (scancode <= 0x07) {
    console_keyboard[0][GATE_A] &= ~(1 << scancode);
  } else if (scancode >= 0x08 && scancode <= 0x0F) {
    console_keyboard[1][GATE_A] &= ~(1 << (scancode - 0x08));
  } else if (scancode >= 0x10 && scancode <= 0x17) {
    console_keyboard[2][GATE_A] &= ~(1 << (scancode - 0x10));
  } else if (scancode >= 0x18 && scancode <= 0x1F) {
    console_keyboard[3][GATE_A] &= ~(1 << (scancode - 0x18));
  } else if (scancode >= 0x20 && scancode <= 0x27) {
    console_keyboard[4][GATE_A] &= ~(1 << (scancode - 0x20));
  } else if (scancode >= 0x28 && scancode <= 0x2F) {
    console_keyboard[5][GATE_A] &= ~(1 << (scancode - 0x28));
  } else if (scancode >= 0x30 && scancode <= 0x37) {
    console_keyboard[6][GATE_A] &= ~(1 << (scancode - 0x30));
  } else if (scancode >= 0x38 && scancode <= 0x3F) {
    console_keyboard[7][GATE_A] &= ~(1 << (scancode - 0x38));
  } else if (scancode == 0x40) {
    console_keyboard[0][GATE_B] &= 0xFE;
  } else if (scancode == 0x41) {
    console_keyboard[1][GATE_B] &= 0xFE;
  } else if (scancode == 0x42) {
    console_keyboard[2][GATE_B] &= 0xFE;
  } else if (scancode == 0x43) {
    console_keyboard[3][GATE_B] &= 0xFE;
  } else if (scancode == 0x44) {
    console_keyboard[4][GATE_B] &= 0xFE;
  } else if (scancode == 0x45) {
    console_keyboard[5][GATE_B] &= 0xFE;
  } else if (scancode == 0x48) {
    console_keyboard[0][GATE_B] &= 0xFD;
  } else if (scancode == 0x49) {
    console_keyboard[1][GATE_B] &= 0xFD;
  } else if (scancode == 0x4A) {
    console_keyboard[2][GATE_B] &= 0xFD;
  } else if (scancode == 0x4B) {
    console_keyboard[3][GATE_B] &= 0xFD;
  } else if (scancode == 0x4D) {
    console_keyboard[5][GATE_B] &= 0xFD;
  } else if (scancode == 0x4E) {
    console_keyboard[6][GATE_B] &= 0xFD;
  } else if (scancode == 0x4F) {
    console_keyboard[7][GATE_B] &= 0xFD;
  } else {
    panic("Unknown scancode: %d\n", scancode);
  }
}



static void console_keyboard_clear(void)
{
  for (int i = 0; i < 8; i++) {
    console_keyboard[i][GATE_A] = 0xFF;
    console_keyboard[i][GATE_B] = 0xFF;
  }

  switch (console_charset) {
  case CONSOLE_CHARSET_US:
    console_keyboard_set(SCANCODE_DIP1);
    console_keyboard_set(SCANCODE_DIP2);
    console_keyboard_set(SCANCODE_DIP3);
    break;
  case CONSOLE_CHARSET_FR:
    console_keyboard_set(SCANCODE_DIP2);
    console_keyboard_set(SCANCODE_DIP3);
    break;
  case CONSOLE_CHARSET_DE:
    console_keyboard_set(SCANCODE_DIP1);
    console_keyboard_set(SCANCODE_DIP3);
    break;
  case CONSOLE_CHARSET_GB:
    console_keyboard_set(SCANCODE_DIP3);
    break;
  case CONSOLE_CHARSET_DK:
    console_keyboard_set(SCANCODE_DIP1);
    console_keyboard_set(SCANCODE_DIP2);
    break;
  case CONSOLE_CHARSET_SE:
    console_keyboard_set(SCANCODE_DIP2);
    break;
  case CONSOLE_CHARSET_IT:
    console_keyboard_set(SCANCODE_DIP1);
    break;
  case CONSOLE_CHARSET_ES:
    break;
  }
}



static void console_keyboard_set_from_char(int ch)
{
  switch (ch) {

  /* Numbers */
  case '0':
    console_keyboard_set(SCANCODE_0);
    break;
  case '1':
    console_keyboard_set(SCANCODE_1);
    break;
  case '2':
    console_keyboard_set(SCANCODE_2);
    break;
  case '3':
    console_keyboard_set(SCANCODE_3);
    break;
  case '4':
    console_keyboard_set(SCANCODE_4);
    break;
  case '5':
    console_keyboard_set(SCANCODE_5);
    break;
  case '6':
    console_keyboard_set(SCANCODE_6);
    break;
  case '7':
    console_keyboard_set(SCANCODE_7);
    break;
  case '8':
    console_keyboard_set(SCANCODE_8);
    break;
  case '9':
    console_keyboard_set(SCANCODE_9);
    break;

  /* Shifted Numbers */
  case '_':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_0);
    break;
  case '!':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_1);
    break;
  case '"':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_2);
    break;
  case '#':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_3);
    break;
  case '$':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_4);
    break;
  case '%':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_5);
    break;
  case '&':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_6);
    break;
  case '\'':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_7);
    break;
  case '(':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_8);
    break;
  case ')':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_9);
    break;

  /* Letters */
  case 'A':
    console_keyboard_set(SCANCODE_A);
    break;
  case 'B':
    console_keyboard_set(SCANCODE_B);
    break;
  case 'C':
    console_keyboard_set(SCANCODE_C);
    break;
  case 'D':
    console_keyboard_set(SCANCODE_D);
    break;
  case 'E':
    console_keyboard_set(SCANCODE_E);
    break;
  case 'F':
    console_keyboard_set(SCANCODE_F);
    break;
  case 'G':
    console_keyboard_set(SCANCODE_G);
    break;
  case 'H':
    console_keyboard_set(SCANCODE_H);
    break;
  case 'I':
    console_keyboard_set(SCANCODE_I);
    break;
  case 'J':
    console_keyboard_set(SCANCODE_J);
    break;
  case 'K':
    console_keyboard_set(SCANCODE_K);
    break;
  case 'L':
    console_keyboard_set(SCANCODE_L);
    break;
  case 'M':
    console_keyboard_set(SCANCODE_M);
    break;
  case 'N':
    console_keyboard_set(SCANCODE_N);
    break;
  case 'O':
    console_keyboard_set(SCANCODE_O);
    break;
  case 'P':
    console_keyboard_set(SCANCODE_P);
    break;
  case 'Q':
    console_keyboard_set(SCANCODE_Q);
    break;
  case 'R':
    console_keyboard_set(SCANCODE_R);
    break;
  case 'S':
    console_keyboard_set(SCANCODE_S);
    break;
  case 'T':
    console_keyboard_set(SCANCODE_T);
    break;
  case 'U':
    console_keyboard_set(SCANCODE_U);
    break;
  case 'V':
    console_keyboard_set(SCANCODE_V);
    break;
  case 'W':
    console_keyboard_set(SCANCODE_W);
    break;
  case 'X':
    console_keyboard_set(SCANCODE_X);
    break;
  case 'Y':
    console_keyboard_set(SCANCODE_Y);
    break;
  case 'Z':
    console_keyboard_set(SCANCODE_Z);
    break;

  /* Lowercase Letters */
  case 'a':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_A);
    break;
  case 'b':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_B);
    break;
  case 'c':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_C);
    break;
  case 'd':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_D);
    break;
  case 'e':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_E);
    break;
  case 'f':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_F);
    break;
  case 'g':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_G);
    break;
  case 'h':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_H);
    break;
  case 'i':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_I);
    break;
  case 'j':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_J);
    break;
  case 'k':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_K);
    break;
  case 'l':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_L);
    break;
  case 'm':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_M);
    break;
  case 'n':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_N);
    break;
  case 'o':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_O);
    break;
  case 'p':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_P);
    break;
  case 'q':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_Q);
    break;
  case 'r':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_R);
    break;
  case 's':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_S);
    break;
  case 't':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_T);
    break;
  case 'u':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_U);
    break;
  case 'v':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_V);
    break;
  case 'w':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_W);
    break;
  case 'x':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_X);
    break;
  case 'y':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_Y);
    break;
  case 'z':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_Z);
    break;

  /* Others */
  case ':':
    console_keyboard_set(SCANCODE_COLON);
    break;
  case ';':
    console_keyboard_set(SCANCODE_SEMICOLON);
    break;
  case ',':
    console_keyboard_set(SCANCODE_COMMA);
    break;
  case '-':
    console_keyboard_set(SCANCODE_MINUS);
    break;
  case '.':
    console_keyboard_set(SCANCODE_PERIOD);
    break;
  case '/':
    console_keyboard_set(SCANCODE_SLASH);
    break;
  case '@':
    console_keyboard_set(SCANCODE_AT);
    break;
  case '[':
    console_keyboard_set(SCANCODE_BRACKETLEFT);
    break;
  case ']':
    console_keyboard_set(SCANCODE_BRACKETRIGHT);
    break;
  case '\\':
    console_keyboard_set(SCANCODE_BACKSLASH);
    break;

  /* Shifted Others */
  case '=':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_MINUS);
    break;
  case '+':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_SEMICOLON);
    break;
  case '*':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_COLON);
    break;
  case '?':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_SLASH);
    break;
  case '^':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_AT);
    break;
  case '<':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_COMMA);
    break;
  case '>':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_PERIOD);
    break;
  case '{':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_BRACKETLEFT);
    break;
  case '}':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_BRACKETRIGHT);
    break;
  case '|':
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_BACKSLASH);
    break;

  /* Norwegian/Danish/Swedish (ISO-8859-1) */
  case 0xE6:
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_BRACKETLEFT);
    break;
  case 0xF8:
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_BACKSLASH);
    break;
  case 0xE5:
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_BRACKETRIGHT);
    break;
  case 0xC6:
    console_keyboard_set(SCANCODE_BRACKETLEFT);
    break;
  case 0xD8:
    console_keyboard_set(SCANCODE_BACKSLASH);
    break;
  case 0xC5:
    console_keyboard_set(SCANCODE_BRACKETRIGHT);
    break;

  /* Arrows */
  case KEY_RIGHT:
    console_keyboard_set(SCANCODE_RIGHT);
    break;
  case KEY_LEFT:
    console_keyboard_set(SCANCODE_LEFT);
    break;
  case KEY_DOWN:
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_RIGHT);
    break;
  case KEY_UP:
    console_keyboard_set(SCANCODE_SHIFT);
    console_keyboard_set(SCANCODE_LEFT);
    break;

  /* Whitespace */
  case KEY_ENTER:
  case '\n':
  case '\r':
    console_keyboard_set(SCANCODE_RETURN);
    break;
  case ' ':
    console_keyboard_set(SCANCODE_SPACE);
    break;
  case '\t':
    console_keyboard_set(SCANCODE_TAB);
    break;
  case KEY_BACKSPACE:
  case KEY_DC: /* Delete */
    console_keyboard_set(SCANCODE_DEL);
    break;

  /* Special */
  case KEY_F(1):
    console_keyboard_set(SCANCODE_PF1);
    break;
  case KEY_F(2):
    console_keyboard_set(SCANCODE_PF2);
    break;
  case KEY_F(3):
    console_keyboard_set(SCANCODE_PF3);
    break;
  case KEY_F(4):
    console_keyboard_set(SCANCODE_PF4);
    break;
  case KEY_F(5):
    console_keyboard_set(SCANCODE_PF5);
    break;
  case KEY_F(6):
    console_keyboard_set(SCANCODE_CLEAR);
    break;
  case KEY_F(7):
    console_keyboard_set(SCANCODE_SCRN);
    break;
  case KEY_F(8):
    console_keyboard_set(SCANCODE_MENU);
    break;
  case KEY_F(9):
    console_keyboard_set(SCANCODE_BREAK);
    break;
  case KEY_F(10):
    console_keyboard_set(SCANCODE_PAUSE);
    break;
  case KEY_F(11):
    console_keyboard_set(SCANCODE_FEED);
    break;
  case KEY_F(12):
    console_keyboard_set(SCANCODE_PRINTER);
    break;

  /* Control */
  case 0x00:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_AT);
    break;
  case 0x01:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_A);
    break;
  case 0x02:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_B);
    break;
  case 0x03:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_C);
    break;
  case 0x04:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_D);
    break;
  case 0x05:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_E);
    break;
  case 0x06:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_F);
    break;
  case 0x07:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_G);
    break;
  case 0x08:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_H);
    break;
  case 0x0B:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_K);
    break;
  case 0x0C:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_L);
    break;
  case 0x0E:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_N);
    break;
  case 0x0F:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_O);
    break;
  case 0x10:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_P);
    break;
  case 0x11:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_Q);
    break;
  case 0x12:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_R);
    break;
  case 0x13:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_S);
    break;
  case 0x14:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_T);
    break;
  case 0x15:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_U);
    break;
  case 0x16:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_V);
    break;
  case 0x17:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_W);
    break;
  case 0x18:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_X);
    break;
  case 0x19:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_Y);
    break;
  case 0x1A:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_Z);
    break;
  case 0x1B:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_BRACKETLEFT);
    break;
  case 0x1C:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_BACKSLASH);
    break;
  case 0x1D:
    console_keyboard_set(SCANCODE_CTRL);
    console_keyboard_set(SCANCODE_BRACKETRIGHT);
    break;

  case KEY_RESIZE:
    /* Ignored */
    break;

  default:
    break;
  }
}



void console_pause(void)
{
  switch (console_mode) {
  case CONSOLE_MODE_NONE:
    break;
  case CONSOLE_MODE_CURSES_ASCII:
  case CONSOLE_MODE_CURSES_PIXEL:
    endwin();
    timeout(-1);
    break;
  }
}



void console_resume(void)
{
  switch (console_mode) {
  case CONSOLE_MODE_NONE:
    break;
  case CONSOLE_MODE_CURSES_ASCII:
  case CONSOLE_MODE_CURSES_PIXEL:
    timeout(0);
    refresh();
    break;
  }
}



void console_exit(void)
{
  switch (console_mode) {
  case CONSOLE_MODE_NONE:
    break;

  case CONSOLE_MODE_CURSES_ASCII:
  case CONSOLE_MODE_CURSES_PIXEL:
    curs_set(1); /* Reveal cursor. */
    endwin();
    break;
  }
}



int console_init(console_mode_t mode, console_charset_t charset)
{
  console_mode = mode;
  console_charset = charset;

  switch (console_mode) {
  case CONSOLE_MODE_NONE:
    break;

  case CONSOLE_MODE_CURSES_ASCII:
  case CONSOLE_MODE_CURSES_PIXEL:
    initscr();
    atexit(console_exit);
    noecho();
    keypad(stdscr, TRUE);
    timeout(0); /* Non-blocking mode. */
    curs_set(0); /* Hide cursor. */
    break;

  default:
    return -1;
  }

  return 0;
}



void console_execute(hd6301_t *cpu, mem_t *mem)
{
  static int cycle = 0;
  int row, col;
  uint16_t address;
  int ch;

  if (console_mode == CONSOLE_MODE_NONE) {
    return;
  }

  /* Lower the keyboard interrupt line if mask is closed again. */
  if ((mem->ram[MASTER_IO_PORT_26_FB] & 0x10) == 0) {
    mem->ram[HD6301_REG_PORT_1] |= 0x20; /* Set port P15. */
  }

  /* Respond to keyboard scanning: */
  switch (mem->ram[MASTER_IO_KSC_GATE]) {
  case 0x00:
  case 0xFF:
    mem->ram[MASTER_IO_KRTN_GATE_A] = 0xFF;
    mem->ram[MASTER_IO_KRTN_GATE_B] = 0xFF;
    break;

  case 0xFE: /* L0 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[0][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[0][GATE_B];
    break;

  case 0xFD: /* L1 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[1][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[1][GATE_B];
    break;

  case 0xFB: /* L2 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[2][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[2][GATE_B];
    break;

  case 0xF7: /* L3 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[3][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[3][GATE_B];
    break;

  case 0xEF: /* L4 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[4][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[4][GATE_B];
    break;

  case 0xDF: /* L5 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[5][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[5][GATE_B];
    break;

  case 0xBF: /* L6 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[6][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[6][GATE_B];
    break;

  case 0x7F: /* L7 */
    mem->ram[MASTER_IO_KRTN_GATE_A] = console_keyboard[7][GATE_A];
    mem->ram[MASTER_IO_KRTN_GATE_B] = console_keyboard[7][GATE_B];
    break;

  default:
    panic("Invalid keyboard scanning line: 0x%02x\n",
      mem->ram[MASTER_IO_KSC_GATE]);
    break;
  }

  /* Serial read of LCD data to get pixel at position: */
  if (console_lcd_serial_cycles_left > 0) {
    if (console_lcd_clock_tick > 4) {
      if (console_mode == CONSOLE_MODE_CURSES_PIXEL) {
        /* Data arrives on the BUSY (a.k.a. SO) pin from the LCD. */
        if (mvinch(console_lcd_row + (12 - console_lcd_clock_tick),
          console_lcd_col) == ' ') {
          mem->ram[MASTER_IO_KRTN_GATE_B] &= ~0x80;
        } else {
          mem->ram[MASTER_IO_KRTN_GATE_B] |= 0x80;
        }
      }
    }
    console_lcd_serial_cycles_left--;
  }

  /* Release the key after a certain amount of cycles: */
  if (cycle > CONSOLE_KEYBOARD_RELEASE) {
    console_keyboard_clear();
  }

  /* Only refresh screen every X cycle: */
  cycle++;
  if ((cycle % CONSOLE_SCREEN_UPDATE) != 0) {
    return;
  }

  if (console_mode == CONSOLE_MODE_CURSES_ASCII) {
    /* Update screen according to "PSBUF": */
    for (row = 0; row < 4; row++) {
      for (col = 0; col < 20; col++) {
        address = 0x220 + (row * 20) + col;
        if (isprint(mem->ram[address])) {
          mvaddch(row, col, mem->ram[address]);
        } else {
          mvaddch(row, col, ' ');
        }
      }
    }

    /* Move cursor according to "CURY" and "CURX": */
    if (mem->ram[0x279] < 4 && mem->ram[0x278] < 20) {
      move(mem->ram[0x279], mem->ram[0x278]);
    }
  }

  /* Check for keypress, but only every X cycle: */
  if ((cycle % CONSOLE_KEYBOARD_UPDATE) != 0) {
    ch = getch();
    if (ch != ERR) {
      console_keyboard_clear();
      console_keyboard_set_from_char(ch);
      cycle = 0; /* Reset, to be used for holding down the key. */

      /* Signal IRQ and prepare for scanning: */
      if (mem->ram[MASTER_IO_PORT_26_FB] & 0x10) { /* Check mask in P264. */
        mem->ram[HD6301_REG_PORT_1] &= ~0x20; /* Reset port P15. */
        hd6301_irq(cpu, mem, HD6301_VECTOR_IRQ_LOW, HD6301_VECTOR_IRQ_HIGH);
      }
    }
  }

  refresh();
}



void console_lcd_select(uint8_t value)
{
  console_lcd_controller = value & 0x07;
  console_lcd_command = (value >> 3) & 0x01;
}



static void console_lcd_update_row_col(uint8_t value)
{
  switch (console_lcd_controller) {
  case 1:
    if (value < 0xC0) {
      console_lcd_row = 0;
      console_lcd_col = value - 0x80;
    } else {
      console_lcd_row = 8;
      console_lcd_col = value - 0xC0;
    }
    break;

  case 2:
    if (value < 0xC0) {
      console_lcd_row = 0;
      console_lcd_col = (value - 0x80) + 40;
    } else {
      console_lcd_row = 8;
      console_lcd_col = (value - 0xC0) + 40;
    }
    break;

  case 3:
    if (value < 0xC0) {
      console_lcd_row = 0;
      console_lcd_col = (value - 0x80) + 80;
    } else {
      console_lcd_row = 8;
      console_lcd_col = (value - 0xC0) + 80;
    }
    break;

  case 4:
    if (value < 0xC0) {
      console_lcd_row = 16;
      console_lcd_col = value - 0x80;
    } else {
      console_lcd_row = 24;
      console_lcd_col = value - 0xC0;
    }
    break;

  case 5:
    if (value < 0xC0) {
      console_lcd_row = 16;
      console_lcd_col = (value - 0x80) + 40;
    } else {
      console_lcd_row = 24;
      console_lcd_col = (value - 0xC0) + 40;
    }
    break;

  case 6:
    if (value < 0xC0) {
      console_lcd_row = 16;
      console_lcd_col = (value - 0x80) + 80;
    } else {
      console_lcd_row = 24;
      console_lcd_col = (value - 0xC0) + 80;
    }
    break;

  default:
    break;
  }
}



void console_lcd_data(uint8_t value)
{
  if (console_mode != CONSOLE_MODE_CURSES_PIXEL) {
    return;
  }

  if (console_lcd_command) { /* Command */
    if (value == 0x64) {
      console_lcd_cmd64_seen = true;

    } else if (value == 0x63) {
      console_lcd_cmd63_seen = true;

    } else {
      if (console_lcd_cmd64_seen) {
        /* Request to write to LCD. */
        console_lcd_update_row_col(value);
        console_lcd_cmd64_seen = false;

      } else if (console_lcd_cmd63_seen) {
        /* Request to read from LCD. */
        console_lcd_update_row_col(value);
        console_lcd_serial_cycles_left = 10000;
        console_lcd_clock_tick = 0;
        console_lcd_cmd63_seen = false;

      } else {

        /* Direct pixel exact row and on/off setting: */
        if (console_lcd_pixel_col >= 0) {
          if (value >= 0x20 && value <= 0x3C) {
            console_lcd_pixel_row += (value - 0x20) / 4;
            mvaddch(console_lcd_pixel_row, console_lcd_pixel_col, ' ');
          } else if (value >= 0x40 && value <= 0x5C) {
            console_lcd_pixel_row += (value - 0x40) / 4;
            mvaddch(console_lcd_pixel_row, console_lcd_pixel_col, '%');
          }
          console_lcd_pixel_col = -1;

        } else {
          /* Direct pixel column and base row: */
          if (value >= 0x80 && value <= 0xA7) {
            switch (console_lcd_controller) {
            case 1:
              console_lcd_pixel_col = value - 0x80;
              console_lcd_pixel_row = 0;
              break;
            case 2:
              console_lcd_pixel_col = (value - 0x80) + 40;
              console_lcd_pixel_row = 0;
              break;
            case 3:
              console_lcd_pixel_col = (value - 0x80) + 80;
              console_lcd_pixel_row = 0;
              break;
            case 4:
              console_lcd_pixel_col = value - 0x80;
              console_lcd_pixel_row = 16;
              break;
            case 5:
              console_lcd_pixel_col = (value - 0x80) + 40;
              console_lcd_pixel_row = 16;
              break;
            case 6:
              console_lcd_pixel_col = (value - 0x80) + 80;
              console_lcd_pixel_row = 16;
              break;
            }

          } else if (value >= 0xC0 && value <= 0xE7) {
            switch (console_lcd_controller) {
            case 1:
              console_lcd_pixel_col = value - 0xC0;
              console_lcd_pixel_row = 8;
              break;
            case 2:
              console_lcd_pixel_col = (value - 0xC0) + 40;
              console_lcd_pixel_row = 8;
              break;
            case 3:
              console_lcd_pixel_col = (value - 0xC0) + 80;
              console_lcd_pixel_row = 8;
              break;
            case 4:
              console_lcd_pixel_col = value - 0xC0;
              console_lcd_pixel_row = 24;
              break;
            case 5:
              console_lcd_pixel_col = (value - 0xC0) + 40;
              console_lcd_pixel_row = 24;
              break;
            case 6:
              console_lcd_pixel_col = (value - 0xC0) + 80;
              console_lcd_pixel_row = 24;
              break;
            }
          }
        }
      }
    }

  } else { /* Data */

    for (int i = 0; i < 8; i++) {
      mvaddch(console_lcd_row + i, console_lcd_col,
        (value >> i) & 1 ? '#' : ' ');
    }
    console_lcd_col++; /* Automatically incremented for each data package. */
  }
}



void console_lcd_clock(void)
{
  /* Tick the clock, which is used for serial transfer. */
  console_lcd_clock_tick++;
}



