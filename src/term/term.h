//------------------------------------------------------------------------------
// Terminal module
//
// Copyright (C)2025 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: core
//> def: _XOPEN_SOURCE _POSIX_C_SOURCE=200809L

#pragma once

//------------------------------------------------------------------------------

#include <core/core.h>

//------------------------------------------------------------------------------
// Terminal information
//------------------------------------------------------------------------------

typedef struct {
    u16 width;
    u16 height;
} TermSize;

TermSize term_size_get(void);

//------------------------------------------------------------------------------
// Terminal interaction API
//------------------------------------------------------------------------------

typedef enum {
    TERM_EVENT_NONE,
    TERM_EVENT_KEY,
    TERM_EVENT_RESIZE,
} TermEventKind;

typedef struct {
    TermEventKind kind;
    union {
        char     key;
        TermSize size;
    };
} TermEvent;

typedef struct Term {
    TermSize size;
    Array(TermEvent) event_queue;
    bool initialised;
    bool running;
} Term;

void      term_init();
void      term_done();
bool      term_loop();
TermEvent term_poll_event();
void      term_cls();

void term_cursor_show();
void term_cursor_hide();

void term_cursor_goto(int x, int y);
void term_cursor_move(int dx, int dy);
void term_cursor_up(int delta);
void term_cursor_down(int delta);
void term_cursor_right(int delta);
void term_cursor_left(int delta);
void term_cursor_home(void);
void term_cursor_colour(u32 ink, u32 paper);

//------------------------------------------------------------------------------
// Terminal frame buffer API
//------------------------------------------------------------------------------

typedef struct {
    u16 x;
    u16 y;
    u16 width;
    u16 height;
} TermRect;

void term_fb_cls(u32 ink, u32 paper);
void term_fb_clip_rect(TermRect  rect,
                       TermRect* out_clipped_rect,
                       TermRect* out_local_rect);

//
// Colouring
//

u32 term_rgb(u8 r, u8 g, u8 b);
u32 term_rgba(u8 r, u8 g, u8 b, u8 a);
u32 term_blend(u32 dest, u32 src, f32 alpha);

// Individual layers
void term_fb_rect_ink(TermRect rect, u32 colour);
void term_fb_rect_paper(TermRect rect, u32 colour);
void term_fb_rect_colour(TermRect rect, u32 ink, u32 paper);
void term_fb_rect_char(TermRect rect, u32 ch);

// Character painting
void term_fb_rect(TermRect rect, u32 ch, u32 ink, u32 paper);

// Writing strings
void term_fb_write_cstr(u16 x, u16 y, cstr string);
void term_fb_write(u16 x, u16 y, string str);
void term_fb_formatv(u16 x, u16 y, cstr fmt, va_list args);
void term_fb_format(u16 x, u16 y, cstr fmt, ...);

//
// Presentation
//

void term_fb_present();

//------------------------------------------------------------------------------
// TermRect utilities
//------------------------------------------------------------------------------

TermRect term_rect(u16 x, u16 y, u16 width, u16 height);
TermRect term_rect_from_points(u16 x0, u16 y0, u16 x1, u16 y1);
TermRect term_rect_union(TermRect a, TermRect b);
TermRect term_rect_intersection(TermRect a, TermRect b);
bool     term_rect_equals(TermRect a, TermRect b);
bool     term_rect_contains(TermRect rect, u16 x, u16 y);
bool     term_rect_overlaps(TermRect a, TermRect b);
bool     term_rect_is_empty(TermRect rect);

bool term_rect_clip(TermRect  a,
                    TermRect  b,
                    TermRect* out_clipped_rect,
                    TermRect* out_local_rect);

//------------------------------------------------------------------------------
// Terminal window
//------------------------------------------------------------------------------

typedef struct {
    u32 ink;
    u32 paper;
    u32 ch;
} TermCell;

typedef struct {
    TermRect rect;
    Array(TermCell) cells;
} TermWindow;

void term_window_init(TermWindow* window, TermRect rect);
void term_window_done(TermWindow* window);
void term_window_clear(const TermWindow* window, u32 ch, u32 ink, u32 paper);
void term_window_rect(const TermWindow* window, TermRect rect, u32 ch);
void term_window_paint(const TermWindow* window,
                       TermRect          rect,
                       u32               ink,
                       u32               paper);
void term_window_paint_rect(
    const TermWindow* window, TermRect rect, u32 ch, u32 ink, u32 paper);
void term_window_9slice(const TermWindow* window,
                        TermRect          rect,
                        cstr              slices,
                        bool              fill_centre);

void term_window_write(TermWindow* window, int x, int y, string str);
void term_window_write_cstr(TermWindow* window, int x, int y, cstr string);

void term_window_formatv(
    TermWindow* window, int x, int y, cstr fmt, va_list args);
void term_window_format(TermWindow* window, int x, int y, cstr fmt, ...);

void term_window_draw(const TermWindow* window);

//------------------------------------------------------------------------------
// Terminal information dumping
//------------------------------------------------------------------------------

void dump_term_size();
void dump_term_size_raw();

//------------------------------------------------------------------------------

//
// Basic colours
//
#define COLOUR_BLACK 0x000000
#define COLOUR_RED 0x800000
#define COLOUR_GREEN 0x008000
#define COLOUR_YELLOW 0x808000
#define COLOUR_BLUE 0x000080
#define COLOUR_MAGENTA 0x800080
#define COLOUR_CYAN 0x008080
#define COLOUR_BRIGHT_GREY 0xc0c0c0

//
// Bright colours
//
#define COLOUR_GREY 0x808080
#define COLOUR_BRIGHT_RED 0xFF0000
#define COLOUR_BRIGHT_GREEN 0x00FF00
#define COLOUR_BRIGHT_YELLOW 0xFFFF00
#define COLOUR_BRIGHT_BLUE 0x0000FF
#define COLOUR_BRIGHT_MAGENTA 0xFF00FF
#define COLOUR_BRIGHT_CYAN 0x00FFFF
#define COLOUR_WHITE 0xFFFFFF

//
// Other colours
//
#define COLOUR_ORANGE 0xFFA500
#define COLOUR_BROWN 0x8B4513
#define COLOUR_PINK 0xFFC0CB
#define COLOUR_PASTEL_BLACK 0x2B2B2B
#define COLOUR_PASTEL_RED 0xFF6961
#define COLOUR_PASTEL_GREEN 0x77DD77
#define COLOUR_PASTEL_YELLOW 0xFDFD96
#define COLOUR_PASTEL_BLUE 0x779ECB
#define COLOUR_PASTEL_MAGENTA 0xCB99C9
#define COLOUR_PASTEL_CYAN 0xAEC6CF
#define COLOUR_CHARCOAL 0x282828
#define COLOUR_WARM_IVORY 0xEBDBB2
#define COLOUR_AMBER 0xD79921
#define COLOUR_DARK_UMBER 0x3C3836
#define COLOUR_DEEP_TEAL 0x002B36
#define COLOUR_MISTY_SLATE 0x839496
#define COLOUR_SLATE_TEAL 0x073642
#define COLOUR_MUSTARD 0xB58900
#define COLOUR_NIGHTFALL 0x2E3440
#define COLOUR_ICY_WHITE 0xD8DEE9
#define COLOUR_STEEL 0x4C566A
#define COLOUR_PALE_CYAN 0x88C0D0
#define COLOUR_SHADOW 0x282A36
#define COLOUR_SOFT_WHITE 0xF8F8F2
#define COLOUR_DUSTY_BLUE 0x44475A
#define COLOUR_NEON_GREEN 0x50FA7B
#define COLOUR_DUSK_PURPLE 0x1E1E2E
#define COLOUR_PALE_LILAC 0xCDD6F4
#define COLOUR_GRAPHITE_PURPLE 0x313244
#define COLOUR_ROSE 0xF38BA8
#define COLOUR_OLIVE_BLACK 0x272822
#define COLOUR_ASH_OLIVE 0x49483E
#define COLOUR_LIME 0xA6E22E
#define COLOUR_INK_STORM 0x1F1F28
#define COLOUR_FADED_IVORY 0xDCD7BA
#define COLOUR_IRON_PURPLE 0x2A2A37
#define COLOUR_SAGE 0x98BB6C
#define COLOUR_HORIZON 0x181820
#define COLOUR_SOFT_GOLD 0xD8C89B
#define COLOUR_POWDER_BLUE 0x7E9CD8
#define COLOUR_NIGHT_STONE 0x2D2F3A

//------------------------------------------------------------------------------
// Themes

#define THEME_DEFAULT 0
#define THEME_BLUE 1
#define THEME_GREEN 2
#define THEME_GRUVBOX 3
#define THEME_SOLARIZED_DARK 4
#define THEME_NORD 5
#define THEME_DRACULA 6
#define THEME_CATPPUCCIN_MOCHA 7
#define THEME_MONOKAI 8
#define THEME_HELIX_DARK 9
#define THEME_HELIX_DEFAULT 10
#define THEME_COUNT 11

typedef struct {
    u32 ink;
    u32 paper;
    u32 highlight_ink;
    u32 highlight_paper;
    u32 cursor;
    u32 gutter;
    u32 selection;
    u32 status_fg;
    u32 status_bg;
    u32 accent_error;
    u32 accent_warning;
    u32 accent_info;
    u32 link;
    u32 link_visited;
} ColourTheme;

extern ColourTheme g_themes[];

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
