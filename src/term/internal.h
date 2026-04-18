//------------------------------------------------------------------------------
// Internal shared data for Term module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#pragma once

#include <term/term.h>

//------------------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------------------

extern Term g_term;
extern bool g_cursor_visible;
extern bool g_cursor_dirty;
extern int  g_cursor_x;
extern int  g_cursor_y;
extern u32  g_cursor_ink;
extern u32  g_cursor_paper;

extern Array(u32) g_term_fb_chars;
extern Array(u32) g_term_fb_ink;
extern Array(u32) g_term_fb_paper;
extern Array(u8) g_term_fb_dirty;
extern TermSize g_term_fb_size;
extern Arena    g_term_arena;

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

enum { TERM_FB_CHAR_WIDE_TAIL = 0xFFFFFFFFu };

bool _term_fb_has_dirty(void);
void _term_fb_present_now(void);

void term_utf8_next(cstr* s, u32* out_char, usize* out_bytes, usize* out_width);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
