//------------------------------------------------------------------------------
// Utility functions
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <gfx/gfx.h>

//------------------------------------------------------------------------------
// frame_new
//
// Create a new Frame object ready to be applied.
//

Frame frame_new(FrameSystem* fs, string title, u64 width, u64 height)
{
    Frame f = {0};

    if (!fs) {
        return f;
    }

    f.system     = fs;
    f.handle     = FRAME_HANDLE_NEW;
    f.title      = title;
    f.width      = width;
    f.height     = height;
    f.resizable  = true;
    f.fullscreen = false;

    return f;
}

//------------------------------------------------------------------------------
// Keyboard event processing

bool frame_event_is_key_pressed(const FrameEvent* ev, FrameKey key)
{
    if (!ev || (ev->kind != FE_KEYDOWN && ev->kind != FE_KEYUP)) {
        return false;
    }
    return ev->keycode == key;
}

u32 frame_event_key_char(const FrameEvent* ev)
{
    if (!ev || (ev->kind != FE_KEYDOWN && ev->kind != FE_KEYUP)) {
        return 0;
    }
    return ev->key_char;
}

bool frame_event_is_shift_pressed(const FrameEvent* ev)
{
    if (!ev || (ev->kind != FE_KEYDOWN && ev->kind != FE_KEYUP)) {
        return false;
    }
    return (ev->modifiers & (KEY_SHIFT_LEFT | KEY_SHIFT_RIGHT)) != 0;
}

bool frame_event_is_ctrl_pressed(const FrameEvent* ev)
{
    if (!ev || (ev->kind != FE_KEYDOWN && ev->kind != FE_KEYUP)) {
        return false;
    }
    return (ev->modifiers & (KEY_CTRL_LEFT | KEY_CTRL_RIGHT)) != 0;
}

bool frame_event_is_alt_pressed(const FrameEvent* ev)
{
    if (!ev || (ev->kind != FE_KEYDOWN && ev->kind != FE_KEYUP)) {
        return false;
    }
    return (ev->modifiers & (KEY_ALT_LEFT | KEY_ALT_RIGHT)) != 0;
}
