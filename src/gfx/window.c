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
    FrameSystem* fs          = frame->system;

    XSetWindowAttributes swa = {
        .event_mask = KeyPressMask | KeyReleaseMask | PointerMotionMask |
                      ButtonPressMask | ButtonReleaseMask | StructureNotifyMask,
        .background_pixel =
            BlackPixel(fs->x_display, DefaultScreen(fs->x_display)),
    };

    Window new_frame = XCreateWindow(
        fs->x_display,     // Display
        fs->x_root_window, // Parent window
        frame->x,          // X position
        frame->y,          // Y position
        frame->width,      // Inner width (in pixels)
        frame->height,     // Inner height (in pixels)
        0,                 // Border width
        CopyFromParent,    // Depth (same as parent)
        InputOutput,       // Class of window (interact with input and output)
        CopyFromParent,    // Visual (same as parent)
        CWEventMask | CWBackPixel, // What fields to take from swa
        &swa                       // Attributes structure
    );

    XMapWindow(fs->x_display, new_frame);
    u64 handle     = fs->next_handle++;

    // Register interest in detecting the close button on the window
    Atom wm_delete = XInternAtom(fs->x_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(fs->x_display, new_frame, &wm_delete, 1);

    // Convert the title string into a c-string and set title.
    cstr title_c_string =
        (cstr)arena_format(temp_arena(), STRINGP, STRINGV(frame->title));
    arena_null_terminate(temp_arena());
    XStoreName(fs->x_display, new_frame, title_c_string);

    // TODO: Will there be an issue of missing important events?
    bool mapped = false;
    while (!mapped) {
        XEvent event;
        XWindowEvent(fs->x_display, new_frame, StructureNotifyMask, &event);

        if (event.type == MapNotify) {
            mapped = true;
        }
    }

    FrameInfo* info = _fs_new_frame_info(fs, handle);
    info->handle    = handle;
    info->xid       = new_frame;

    XWindowAttributes attrs;
    if (XGetWindowAttributes(fs->x_display, new_frame, &attrs)) {
        info->x      = attrs.x;
        info->y      = attrs.y;
        info->width  = (u64)attrs.width;
        info->height = (u64)attrs.height;
    } else {
        info->x      = frame->x;
        info->y      = frame->y;
        info->width  = frame->width;
        info->height = frame->height;
    }

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

// TODO: Implement this function to look at the state of the Frame and update
// the frame accordingly.  This might involve changing the title, resizing the
// window, or other updates based on the fields in the Frame struct.
//
// Note, that changes should only be made if they actually change the current
// state.  This avoids firing of resizing events, for example, if the window
// hasn't actually changed.  We can do this by tracking the state in FrameInfo
// when events happen, or ask for the state in this function.  You can decide on
// which is best.
//
// With either approach, we might be able to leverage the fs_update function by
// setting up a Frame with the same handle and calling it.  We can then compare
// this Frame to the given Frame to decide what needs to change.  This means
// that either fs_update asks for the current state within, or it fetchs it from
// cached information in FrameInfo.
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
    temp_arena_init();

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
    FrameEvent close_event = {
        .kind         = FE_CLOSE,
        .frame_handle = frame->handle,
    };
    _fs_push_event(frame->system, close_event);
    frame->handle = FRAME_HANDLE_CLOSED;
}

//------------------------------------------------------------------------------
// fs_update

// TODO: Implement this to update the Frame structure to match the actual
// current state of the frame.
//
// There are two ways we can do this and you should decide which way is better.
// You can either 1) track any events related to the state in Frame and store
// that information in the associated FrameInfo; or 2) call any querying API to
// get the state directly at his time.
//
// The first relies on accurate tracking of all necessary events (like resizing,
// title change etc), and the second one might be slower since we querying all
// the time.  If the query functions in X11 are fast, this might be fine and
// preferable to the first solution, which requires sync code to maintained.
void fs_update(Frame* frame) { UNUSED(frame); }

//------------------------------------------------------------------------------
// fs_loop

bool fs_loop(FrameSystem* fs, FrameEvent* out_event)
{
    temp_arena_reset();

    //
    // Collect any OS events and add them to our event queue
    //

    FrameEvent event;
    event.kind = FE_NONE;

    XEvent xev;
    while (XPending(fs->x_display) > 0) {
        bool should_send_event = true;
        XNextEvent(fs->x_display, &xev);
        FrameInfo* info = _fs_find_frame_info_by_xid(fs, xev.xany.window);
        if (!info) {
            continue;
        }

        switch (xev.type) {
        case KeyPress:
            event.kind    = FE_KEYDOWN;
            event.keycode = xev.xkey.keycode;
            break;

        case KeyRelease:
            event.kind    = FE_KEYUP;
            event.keycode = xev.xkey.keycode;
            break;

        case MotionNotify:
            event.kind = FE_MOUSEMOVE;
            event.x    = xev.xmotion.x;
            event.y    = xev.xmotion.y;
            break;

        case ButtonPress:
            event.kind = FE_MOUSEBUTTONDOWN;
            break;

        case ButtonRelease:
            event.kind = FE_MOUSEBUTTONUP;
            break;

        case ConfigureNotify:
            event.kind   = FE_RESIZE;
            event.width  = xev.xconfigure.width;
            event.height = xev.xconfigure.height;
            break;

        case ClientMessage:
            should_send_event = false;
            fs_done(&(Frame){
                .handle = info->handle,
                .system = fs,
            });
            break;

        default:
            should_send_event = false;
            break;
        }

        if (should_send_event) {
            array_push(fs->events, event);
        }
    }

    //
    // If there are no more frames or events, we need to signal an exit from the
    // main loop
    //

    if ((array_count(fs->frames) == 0) && (array_count(fs->events) == 0)) {
        array_free(fs->frames);
        array_free(fs->events);
        XCloseDisplay(fs->x_display);
        temp_arena_done();
        return false;
    }

    //
    // Grab one event, if any, off our queue
    //

    if (array_count(fs->events) > 0) {
        *out_event = array_pop(fs->events);
    } else {
        out_event->kind = FE_NONE;
    }

    //
    // Otherwise, keep looping
    //

    return true;
}
