//------------------------------------------------------------------------------
// Graphics module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: core
//> lib: X11

#include <core/core.h>

#include <X11/Xlib.h>

//------------------------------------------------------------------------------
// Frame System

#define FRAME_HANDLE_NEW 0
#define FRAME_HANDLE_CLOSED 1

typedef struct FrameSystem_t FrameSystem;

typedef struct {
    u64          handle;
    FrameSystem* system;
    string       title;
    i64          x;
    i64          y;
    u64          width;
    u64          height;
} Frame;

typedef struct {
    u64    handle;
    Window xid;
    u8*    title_heap;
    i64    x;
    i64    y;
    u64    width;
    u64    height;
} FrameInfo;

typedef enum {
    FE_NONE,
    FE_KEYDOWN,
    FE_KEYUP,
    FE_MOUSEMOVE,
    FE_MOUSEBUTTONDOWN,
    FE_MOUSEBUTTONUP,
    FE_MOVE,
    FE_RESIZE,
    FE_CLOSE,
} FrameEventKind;

typedef enum {
    KEY_ESCAPE      = 9,

    KEY_1           = 10,
    KEY_2           = 11,
    KEY_3           = 12,
    KEY_4           = 13,
    KEY_5           = 14,
    KEY_6           = 15,
    KEY_7           = 16,
    KEY_8           = 17,
    KEY_9           = 18,
    KEY_0           = 19,
    KEY_MINUS       = 20,
    KEY_EQUAL       = 21,
    KEY_BACKSPACE   = 22,
    KEY_TAB         = 23,

    KEY_Q           = 24,
    KEY_W           = 25,
    KEY_E           = 26,
    KEY_R           = 27,
    KEY_T           = 28,
    KEY_Y           = 29,
    KEY_U           = 30,
    KEY_I           = 31,
    KEY_O           = 32,
    KEY_P           = 33,
    KEY_LBRACKET    = 34,
    KEY_RBRACKET    = 35,
    KEY_ENTER       = 36,
    KEY_LCTRL       = 37,

    KEY_A           = 38,
    KEY_S           = 39,
    KEY_D           = 40,
    KEY_F           = 41,
    KEY_G           = 42,
    KEY_H           = 43,
    KEY_J           = 44,
    KEY_K           = 45,
    KEY_L           = 46,
    KEY_SEMICOLON   = 47,
    KEY_APOSTROPHE  = 48,
    KEY_GRAVE       = 49,
    KEY_LSHIFT      = 50,
    KEY_BACKSLASH   = 51,

    KEY_Z           = 52,
    KEY_X           = 53,
    KEY_C           = 54,
    KEY_V           = 55,
    KEY_B           = 56,
    KEY_N           = 57,
    KEY_M           = 58,
    KEY_COMMA       = 59,
    KEY_PERIOD      = 60,
    KEY_SLASH       = 61,
    KEY_RSHIFT      = 62,
    KEY_KP_MULTIPLY = 63,
    KEY_LALT        = 64,
    KEY_SPACE       = 65,
    KEY_CAPSLOCK    = 66,

    KEY_F1          = 67,
    KEY_F2          = 68,
    KEY_F3          = 69,
    KEY_F4          = 70,
    KEY_F5          = 71,
    KEY_F6          = 72,
    KEY_F7          = 73,
    KEY_F8          = 74,
    KEY_F9          = 75,
    KEY_F10         = 76,
    KEY_NUMLOCK     = 77,
    KEY_SCROLLLOCK  = 78,

    KEY_KP_7        = 79,
    KEY_KP_8        = 80,
    KEY_KP_9        = 81,
    KEY_KP_SUBTRACT = 82,
    KEY_KP_4        = 83,
    KEY_KP_5        = 84,
    KEY_KP_6        = 85,
    KEY_KP_ADD      = 86,
    KEY_KP_1        = 87,
    KEY_KP_2        = 88,
    KEY_KP_3        = 89,
    KEY_KP_0        = 90,
    KEY_KP_DECIMAL  = 91,

    KEY_F11         = 95,
    KEY_F12         = 96,

    KEY_RCTRL       = 105,
    KEY_KP_DIVIDE   = 106,
    KEY_PRINT       = 107,
    KEY_RALT        = 108,
    KEY_HOME        = 110,
    KEY_UP          = 111,
    KEY_PAGEUP      = 112,
    KEY_LEFT        = 113,
    KEY_RIGHT       = 114,
    KEY_END         = 115,
    KEY_DOWN        = 116,
    KEY_PAGEDOWN    = 117,
    KEY_INSERT      = 118,
    KEY_DELETE      = 119,

    KEY_PAUSE       = 127,
    KEY_LSUPER      = 133,
    KEY_RSUPER      = 134,
    KEY_MENU        = 135,
} Keycode;

typedef struct {
    FrameEventKind kind;
    u64            frame_handle;
    union {
        struct {
            Keycode keycode;
        };
        struct {
            i64 x;
            i64 y;
        };
        struct {
            u64 width;
            u64 height;
        };
    };
} FrameEvent;

struct FrameSystem_t {
    Array(FrameInfo) frames;
    Array(FrameEvent) events;
    u64 next_handle;

    Display* x_display;
    Window   x_root_window;
};

// Initialise the frame system.
//
// When `fs_loop` returns false, it will have cleaned up the resources for
// FrameSystem, so no explicit clean up required.
//
void fs_init(FrameSystem* fs);

// Apply the current state of a Frame struct.
//
// This applies any changes in the Frame struct resulting in changing of frame
// state or creating it in the first place.  The `handle` field must be 0 (or
// FRAME_HANDLE_NEW) to first create a frame.  When the frame is first
// created, the `handle` field is written to so the Frame struct can be reused
// for mutation with this function.  Additionally, the FrameSystem reference
// will be written to it too.
//
void fs_apply(Frame* frame);

// Manually close the frame down.
//
// You must pass a Frame with a valid handle and FrameSystem reference.  After
// closing, the Frame struct's handle is set to a particular value
// (FRAME_HANDLE_CLOSED) that marks it as closed.  To bring this frame back,
// just set the handle to FRAME_HANDLE_NEW and apply again.
//
void fs_done(Frame* frame);

// Get the latest state of a frame.
//
// Ensure that the `system` field is set to the correct FrameSystem and that
// the `handle` field is set too.  This identifies the frame and this function
// fills in all the information about it.
//
// Before handling events that can change the frame state, you should call this
// function to get the latest update as frame state can be changed by the user
// directly through the desktop.
//
void fs_update(Frame* frame);

// Loop the frame system and fetch the next event.
//
// If there are no events, FE_NONE is returned.
//
// This function will return true if you need to keep looping, therefore this
// should be called within a `while` loop.  When the function returns false, it
// means that all frames have been closed, all events have been processed and
// all resources have been reclaimed.  There is no further work to do with the
// frame system.  The FrameSystem instance itself will be cleaned up and
// cannot be used again.
//
// Because this is a main loop, it will call `temp_arena_reset()` each time it
// is called.
//
bool fs_loop(FrameSystem* fs, FrameEvent* event);
