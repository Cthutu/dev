//------------------------------------------------------------------------------
// Window API implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <gfx/gfx.h>

//------------------------------------------------------------------------------
// _fs_find_frame_info

internal FrameInfo* _fs_find_frame_info(FrameSystem* fs, u64 handle)
{
    for (u64 i = 0; i < array_count(fs->frames); i++) {
        FrameInfo* info = &fs->frames[i];
        if (info->handle == handle) {
            return info;
        }
    }
    return NULL;
}

//------------------------------------------------------------------------------
// _fs_find_frame_info_by_xid

internal FrameInfo* _fs_find_frame_info_by_xid(FrameSystem* fs, Window xid)
{
    for (u64 i = 0; i < array_count(fs->frames); i++) {
        FrameInfo* info = &fs->frames[i];
        if (info->xid == xid) {
            return info;
        }
    }
    return NULL;
}

//------------------------------------------------------------------------------
// _fs_new_frame_info

internal FrameInfo* _fs_new_frame_info(FrameSystem* fs, u64 handle)
{
    FrameInfo new_info = {.handle = handle};
    array_push(fs->frames, new_info);
    return &fs->frames[array_count(fs->frames) - 1];
}

//------------------------------------------------------------------------------
// _fs_delete_frame_info

internal void _fs_delete_frame_info(FrameSystem* fs, u64 handle)
{
    for (u64 i = 0; i < array_count(fs->frames); i++) {
        if (fs->frames[i].handle == handle) {
            array_delete_quick(fs->frames, i);
            return;
        }
    }
}

//------------------------------------------------------------------------------
// _fs_push_event

internal void _fs_push_event(FrameSystem* fs, FrameEvent event)
{
    array_push(fs->events, event);
}

//------------------------------------------------------------------------------
// _fs_pop_event

internal bool _fs_pop_event(FrameSystem* fs, FrameEvent* event)
{
    if (array_count(fs->events) > 0) {
        *event = array_pop(fs->events);
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// _fs_create_frame
//
// This is a platform-specific function that creates a frame based on the
// information in the Frame struct.  It should set the handle field of the
// Frame struct.
//

internal u64 _fs_create_frame(const Frame* frame)
{
    FrameSystem* fs  = frame->system;

    Window new_frame = XCreateSimpleWindow(
        fs->x_display,
        fs->x_root_window,
        0,
        0,
        frame->width,
        frame->height,
        0,
        BlackPixel(fs->x_display, DefaultScreen(fs->x_display)),
        WhitePixel(fs->x_display, DefaultScreen(fs->x_display)));
    XSelectInput(fs->x_display,
                 new_frame,
                 KeyPressMask | KeyReleaseMask | PointerMotionMask |
                     ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);
    XMapWindow(fs->x_display, new_frame);
    u64 handle = fs->next_handle++;

    for (;;) {
        XEvent event;
        XNextEvent(fs->x_display, &event);
        if (event.type == MapNotify && event.xmap.window == new_frame) {
            break;
        }
    }

    FrameInfo* info = _fs_new_frame_info(fs, handle);
    info->handle    = handle;
    info->xid       = new_frame;

    return handle;
}

//------------------------------------------------------------------------------
// _fs_close_frame
//
// This is a platform-specific function that manually closes a frame.  It should
// clean up any resources associated with the frame and remove it from the
// FrameSystem's list of frames.
//

internal void _fs_close_frame(const Frame* frame)
{
    FrameInfo* info = _fs_find_frame_info(frame->system, frame->handle);
    if (info) {
        XUnmapWindow(frame->system->x_display, info->xid);
        XDestroyWindow(frame->system->x_display, info->xid);
    }
    _fs_delete_frame_info(frame->system, frame->handle);
}

//------------------------------------------------------------------------------
// _fs_update_frame
//
// This is a platform-specific function that updates the state of an existing
// frame based on the information in the Frame struct.
//

internal void _fs_update_frame(Frame* frame)
{
    // Platform-specific frame update code goes here. This might involve
    // changing the frame's title, resizing it, or other updates based on the
    // fields in the Frame struct.

    UNUSED(frame);
}

//------------------------------------------------------------------------------
// fs_init

void fs_init(FrameSystem* fs)
{
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        kill("Failed to open X display");
    }
    Window root_window = DefaultRootWindow(display);

    *fs                = (FrameSystem){
                       .frames        = 0,
                       .events        = 0,
                       .next_handle   = 2, // 0 and 1 are reserved for NEW and CLOSED
                       .x_display     = display,
                       .x_root_window = root_window,
    };
}

//------------------------------------------------------------------------------
// fs_apply

void fs_apply(Frame* frame)
{
    if (frame->handle == FRAME_HANDLE_NEW) {
        // Create a new frame
        frame->handle = _fs_create_frame(frame);
    } else if (frame->handle != FRAME_HANDLE_CLOSED) {
        // Update an existing frame
        _fs_update_frame(frame);
    }
}
//------------------------------------------------------------------------------
// fs_done

void fs_done(Frame* frame)
{
    _fs_close_frame(frame);
    frame->handle = FRAME_HANDLE_CLOSED;
}

//------------------------------------------------------------------------------
// fs_loop

bool fs_loop(FrameSystem* fs, FrameEvent* event)
{
    // Platform-specific event polling code goes here.
    // This should fill in the `event` struct with the latest event
    // information and return true if the loop should continue running, or
    // false if it should exit.

    UNUSED(fs);

    event->kind = FE_NONE;

    time_sleep_ms(2000);

    if (array_count(fs->frames) == 0) {
        array_free(fs->frames);
        array_free(fs->events);
        XCloseDisplay(fs->x_display);
        return false;
    }

    return true;
}
