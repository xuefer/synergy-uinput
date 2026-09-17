/* 2013
 * Maciej Szeptuch (Neverous) <neverous@neverous.info>
 *
 * Synergy-uinput.
 * ----------
 *  Synergy client for use with uinput.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <linux/input-event-codes.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>
/* VK_* from winuser.h */
#define VK_BROWSER_SEARCH      0xAA
#define VK_BROWSER_HOME        0xAC
#define VK_VOLUME_MUTE         0xAD
#define VK_VOLUME_DOWN         0xAE
#define VK_VOLUME_UP           0xAF
#define VK_MEDIA_NEXT_TRACK    0xB0
#define VK_MEDIA_PREV_TRACK    0xB1
#define VK_MEDIA_STOP          0xB2
#define VK_MEDIA_PLAY_PAUSE    0xB3
#define VK_LAUNCH_MAIL         0xB4
#define VK_LAUNCH_MEDIA_SELECT 0xB5
#define VK_LAUNCH_APP1         0xB6
#define VK_LAUNCH_APP2         0xB7

#include "log/log.h"
#include "synergy/event.h"
#include "synergy/client.h"
#include "uinput/uinput.h"

// Usage options and info
const char *VERSION = "0.2.1";
const char *HELP = "Usage: synergy-uinput [options]\n\n\
    -h --help                       Display this usage information.\n\
    -V --version                    Display program version.\n\
    -v --verbose                    Increase verbosity level.\n\
    -d --debug LEVEL[=info]         Set verbosity level to LEVEL [error, warning, info, notice, debug].\n\
    -l --log FILE[=stderr]          Set log file\n\
    -c --host HOST[=localhost]      Address of synergy server.\n\
    -p --port PORT[=24800]          Port of synergy server.\n\
    -W --width WIDTH[=1]            Screen width for mouse.\n\
    -H --height HEIGHT[=1]          Screen height for mouse.\n\
    -N --name NAME[=synergy-uinput] SynergyClient name.";

const char *SHORT_OPTIONS = "hVvd:c:p:N:W:H:l:";
const struct option LONG_OPTIONS[] = 
{
    {"help",    no_argument,        0, 'h'}, // display help and usage information
    {"version", no_argument,        0, 'V'}, // display version
    {"verbose", no_argument,        0, 'v'}, // set log level to notice [error, warning, info, notice, debug]
    {"debug",   required_argument,  0, 'd'}, // manually set log level
    {"log",     required_argument,  0, 'l'}, // set log file
    {"host",    required_argument,  0, 'c'}, // connection host
    {"port",    required_argument,  0, 'p'}, // connection port
    {"name",    required_argument,  0, 'N'}, // client name
    {"width",   required_argument,  0, 'W'}, // screen width
    {"height",  required_argument,  0, 'H'}, // screen height
    {0, 0, 0, 0},
};

SynergyClient client = {
    "synergy-uinput",
    "localhost",
    0,
    24800,
    1, 1, 
    {LOG_INFO, 0},
};

uint32_t loglevel;
char *logfile;

int32_t mouse,
        keyboard;

/**
 * @brief Maps a Synergy KeySym ID and hardware button code to a Linux input keycode (KEY_*).
 */

static uint16_t synergyKeyToLinuxKey(SynergyClient *client, uint32_t id)
{
	// 1. Map lower-case ASCII characters ('a' - 'z')
	if (id >= 'a' && id <= 'z')
	{
		static const uint16_t alpha_map[] = {
			KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
			KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
			KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
		};
		return alpha_map[id - 'a'];
	}

	// 2. Map upper-case ASCII characters ('A' - 'Z')
	if (id >= 'A' && id <= 'Z')
	{
		static const uint16_t alpha_map[] = {
			KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
			KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
			KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
		};
		return alpha_map[id - 'A'];
	}

	// 3. Map digit keys ('0' - '9')
	if (id >= '0' && id <= '9')
	{
		static const uint16_t num_map[] = {
			KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9
		};
		return num_map[id - '0'];
	}

	// 4. Synergy Windows Raw VK (0xE000 - 0xE0FF) 显式兼容处理
	if ((id & 0xFF00) == 0xE000)
	{
		switch (id & 0xFF)
		{
			case VK_BROWSER_SEARCH      : return KEY_SEARCH;
			case VK_BROWSER_HOME        : return KEY_HOMEPAGE;
			case VK_VOLUME_MUTE         : return KEY_MUTE;
			case VK_VOLUME_DOWN         : return KEY_VOLUMEDOWN;
			case VK_VOLUME_UP           : return KEY_VOLUMEUP;
			case VK_LAUNCH_MAIL         : return KEY_MAIL;
			case VK_LAUNCH_MEDIA_SELECT : return KEY_MEDIA;
			case VK_LAUNCH_APP1         : return KEY_COMPUTER;
			case VK_LAUNCH_APP2         : return KEY_CALC;
			case VK_MEDIA_NEXT_TRACK    : return KEY_NEXTSONG;
			case VK_MEDIA_PREV_TRACK    : return KEY_PREVIOUSSONG;
			case VK_MEDIA_STOP          : return KEY_STOPCD;
			case VK_MEDIA_PLAY_PAUSE    : return KEY_PLAYPAUSE;
			default: break;
		}
	}

	// 5. Common ASCII punctuation & Shifted symbols
	switch (id)
	{
		case '!': return KEY_1;
		case '@': return KEY_2;
		case '#': return KEY_3;
		case '$': return KEY_4;
		case '%': return KEY_5;
		case '^': return KEY_6;
		case '&': return KEY_7;
		case '*': return KEY_8;
		case '(': return KEY_9;
		case ')': return KEY_0;

		case ' ':  return KEY_SPACE;
		case '-':  case '_': return KEY_MINUS;
		case '=':  case '+': return KEY_EQUAL;
		case '[':  case '{': return KEY_LEFTBRACE;
		case ']':  case '}': return KEY_RIGHTBRACE;
		case '\\': case '|': return KEY_BACKSLASH;
		case ';':  case ':': return KEY_SEMICOLON;
		case '\'': case '"': return KEY_APOSTROPHE;
		case '`':  case '~': return KEY_GRAVE;
		case ',':  case '<': return KEY_COMMA;
		case '.':  case '>': return KEY_DOT;
		case '/':  case '?': return KEY_SLASH;
		default:   break;
	}

	// 6. Extract lower 16 bits to strip 0xEF00 prefix (0xEFxx -> 0xFFxx)
	uint32_t base_id = (id & 0xFF00) == 0xEF00 ? (id & 0x00FF) | 0xFF00 : id;

	// 7. Standard X11 KeySyms
	switch (base_id)
	{
		// Control & Lock keys
		case XK_BackSpace:        return KEY_BACKSPACE;
		case XK_Tab:              return KEY_TAB;
		case XK_Return:           return KEY_ENTER;
		case XK_Escape:           return KEY_ESC;
		case XK_Pause:            return KEY_PAUSE;
		case XK_Scroll_Lock:      return KEY_SCROLLLOCK;
		case XK_Sys_Req:          return KEY_SYSRQ;
		case XK_Caps_Lock:        return KEY_CAPSLOCK;
		case XK_Num_Lock:         return KEY_NUMLOCK;
		case XK_Delete:           return KEY_DELETE;

		// Navigation keys
		case XK_Home:             return KEY_HOME;
		case XK_Left:             return KEY_LEFT;
		case XK_Up:               return KEY_UP;
		case XK_Right:            return KEY_RIGHT;
		case XK_Down:             return KEY_DOWN;
		case XK_Page_Up:          return KEY_PAGEUP;
		case XK_Page_Down:        return KEY_PAGEDOWN;
		case XK_End:              return KEY_END;
		case XK_Print:            return KEY_SYSRQ;
		case XK_Insert:           return KEY_INSERT;
		case XK_Menu:             return KEY_MENU;

		// Function keys (F1 - F12)
		case XK_F1:               return KEY_F1;
		case XK_F2:               return KEY_F2;
		case XK_F3:               return KEY_F3;
		case XK_F4:               return KEY_F4;
		case XK_F5:               return KEY_F5;
		case XK_F6:               return KEY_F6;
		case XK_F7:               return KEY_F7;
		case XK_F8:               return KEY_F8;
		case XK_F9:               return KEY_F9;
		case XK_F10:              return KEY_F10;
		case XK_F11:              return KEY_F11;
		case XK_F12:              return KEY_F12;

		// Modifier keys
		case XK_Shift_L:          return KEY_LEFTSHIFT;
		case XK_Shift_R:          return KEY_RIGHTSHIFT;
		case XK_Control_L:        return KEY_LEFTCTRL;
		case XK_Control_R:        return KEY_RIGHTCTRL;
		case XK_Alt_L:            return KEY_LEFTALT;
		case XK_Alt_R:            return KEY_RIGHTALT;
		case XK_Mode_switch:      return KEY_RIGHTALT;
		case XK_Super_L:          return KEY_LEFTMETA;
		case XK_Super_R:          return KEY_RIGHTMETA;

		// Keypad Operations
		case XK_KP_Space:         return KEY_SPACE;
		case XK_KP_Tab:           return KEY_TAB;
		case XK_KP_Enter:         return KEY_KPENTER;
		case XK_KP_Multiply:      return KEY_KPASTERISK;
		case XK_KP_Add:           return KEY_KPPLUS;
		case XK_KP_Separator:
		case XK_KP_Decimal:       return KEY_KPDOT;
		case XK_KP_Subtract:      return KEY_KPMINUS;
		case XK_KP_Divide:        return KEY_KPSLASH;
		case XK_KP_Equal:         return KEY_KPEQUAL;

		// Keypad Navigation (NumLock OFF)
		case XK_KP_Home:          return KEY_KP7;
		case XK_KP_Left:          return KEY_KP4;
		case XK_KP_Up:            return KEY_KP8;
		case XK_KP_Right:         return KEY_KP6;
		case XK_KP_Down:          return KEY_KP2;
		case XK_KP_Page_Up:       return KEY_KP9;
		case XK_KP_Page_Down:     return KEY_KP3;
		case XK_KP_End:           return KEY_KP1;
		case XK_Clear:            return KEY_KP5;
		case XK_KP_Begin:         return KEY_KP5;
		case XK_KP_Insert:        return KEY_KP0;
		case XK_KP_Delete:        return KEY_KPDOT;

		// Keypad Digits (NumLock ON)
		case XK_KP_0:             return KEY_KP0;
		case XK_KP_1:             return KEY_KP1;
		case XK_KP_2:             return KEY_KP2;
		case XK_KP_3:             return KEY_KP3;
		case XK_KP_4:             return KEY_KP4;
		case XK_KP_5:             return KEY_KP5;
		case XK_KP_6:             return KEY_KP6;
		case XK_KP_7:             return KEY_KP7;
		case XK_KP_8:             return KEY_KP8;
		case XK_KP_9:             return KEY_KP9;

		default: break;
	}

	return KEY_RESERVED;
}

void sigbreak(int signal)
{
    INFO(&client.log, "Catched signal. Closing.");
    sDisconnect(&client, "catched signal");
    uClose(mouse);
    uClose(keyboard);
    lClose(&client.log);
    exit(0);
}

int32_t main(int32_t argc, char **argv)
{
    signal(SIGINT, sigbreak);
    signal(SIGKILL, sigbreak);
    int32_t o;

    while((o = getopt_long(argc, argv, SHORT_OPTIONS, LONG_OPTIONS, 0)) != -1) switch(o)
    {
        case 'h': puts(HELP);
            return 0;

        case 'V': printf("synergy-uinput %s\n", VERSION);
            return 0;

        case 'v': loglevel = LOG_NOTICE;
            break;

        case 'd':
            if(memcmp(optarg, "error", 5) == 0)         loglevel = LOG_ERROR;
            else if(memcmp(optarg, "warning", 7) == 0)  loglevel = LOG_WARNING;
            else if(memcmp(optarg, "info", 4) == 0)     loglevel = LOG_INFO;
            else if(memcmp(optarg, "notice", 6) == 0)   loglevel = LOG_NOTICE;
            else if(memcmp(optarg, "debug", 5) == 0)    loglevel = LOG_DEBUG;
            else                                        {fputs(HELP, stderr); return 1;}
            break;

        case 'c': client.host = optarg;
            break;

        case 'p': client.port = atoi(optarg);
            break;

        case 'N': client.name = optarg;
            break;

        case 'W': client.width = atoi(optarg);
            break;

        case 'H': client.height = atoi(optarg);
            break;

        case 'l': logfile = optarg;
            break;

        case '?': fputs(HELP, stderr);
            return 1;
    }

    lOpen(&client.log, logfile, loglevel);
    NOTICE(&client.log, "Synergy-uinput configured with %s name, %dx%d resolution and %s:%d address.", client.name, client.width, client.height, client.host, client.port);
    keyboard = uInitializeKeyboard();
    mouse = uInitializeMouse(client.width, client.height);
    if(keyboard < 0 || mouse < 0)
    {
        ERROR(&client.log, "Cannot create uinput devices.");
        return 1;
    }

    sConnect(&client);
    sProcess(&client);
    sDisconnect(&client, "client exit");
    uClose(mouse);
    uClose(keyboard);
    lClose(&client.log);
    return 0;
}

/* Event handlers */
void eventConnected(SynergyClient *client, const char *host, const uint16_t port){}
void eventDisconnected(SynergyClient *client, const char *host, const uint16_t port, const char *reason){}
void eventOptionsReset(SynergyClient *client){}
void eventOptionsSet(SynergyClient *client, const uint32_t *options){}
void eventFocusOut(SynergyClient *client){}

void eventFocusIn(SynergyClient *client, const uint16_t x, const uint16_t y, const uint16_t mask, const uint16_t seq)
{
    uMouseMotion(mouse, x, y);
    return;
}


void eventMouseMotion(SynergyClient *client, const uint16_t x, const uint16_t y)
{
    uMouseMotion(mouse, x, y);
    return;
}

void eventMouseRelativeMotion(SynergyClient *client, const int16_t dx, const int16_t dy)
{
    uMouseRelativeMotion(mouse, dx, dy);
    return;
}

void eventMouseWheel(SynergyClient *client, const uint16_t dx, const uint16_t dy)
{
    uMouseWheel(mouse, dx, dy);
    return;
}

void eventMouseButtonDown(SynergyClient *client, const uint16_t button)
{
    uMouseButton(mouse, button, 1);
    return;
}

void eventMouseButtonUp(SynergyClient *client, const uint16_t button)
{
    uMouseButton(mouse, button, 0);
    return;
}

static void eventKey(SynergyClient *client, const uint16_t key, const uint16_t mask, const uint16_t button, const uint8_t state)
{
    uint16_t linux_code = synergyKeyToLinuxKey(client, key);
    if (linux_code != KEY_RESERVED) {
        uKey(keyboard, linux_code, state);
    } else {
        ERROR(&client->log, "Unhandled key: id=0x%X", key);
    }
}

void eventKeyDown(SynergyClient *client, const uint16_t key, const uint16_t mask, const uint16_t button)
{
    eventKey(client, key, mask, button, 1);
}

void eventKeyUp(SynergyClient *client, const uint16_t key, const uint16_t mask, const uint16_t button)
{
    eventKey(client, key, mask, button, 0);
}

void eventKeyRepeat(SynergyClient *client, const uint16_t key, const uint16_t mask, const uint16_t count, const uint16_t button)
{
    eventKey(client, key, mask, button, 2);
}
