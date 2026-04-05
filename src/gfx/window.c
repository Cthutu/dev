//------------------------------------------------------------------------------
// Window API implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <gfx/gfx.h>

#include <X11/Xutil.h>
#include <X11/keysym.h>

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
            ARRAY_FREE(fs->frames[i].title_heap);
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
// _fs_apply_resizeable

internal void _fs_apply_resizeable(const Frame* frame, Window xid)
{
    XSizeHints hints = {0};
    if (frame->resizable) {
        hints.flags = 0;
    } else {
        hints.flags      = PMinSize | PMaxSize;
        hints.min_width  = (i32)frame->width;
        hints.min_height = (i32)frame->height;
        hints.max_width  = (i32)frame->width;
        hints.max_height = (i32)frame->height;
    }

    XSetWMNormalHints(frame->system->x_display, xid, &hints);
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

    _fs_apply_resizeable(frame, new_frame);

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
    info->resizable = frame->resizable;

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

internal void _fs_update_frame(Frame* frame)
{
    FrameInfo* info = _fs_find_frame_info(frame->system, frame->handle);
    if (!info) {
        return;
    }

    Frame current = {
        .handle = frame->handle,
        .system = frame->system,
    };
    fs_update(&current);

    bool moved = current.x != frame->x || current.y != frame->y;
    bool resized =
        current.width != frame->width || current.height != frame->height;
    bool resizable_changed = info->resizable != frame->resizable;
    if (moved && resized) {
        XMoveResizeWindow(frame->system->x_display,
                          info->xid,
                          (i32)frame->x,
                          (i32)frame->y,
                          (u32)frame->width,
                          (u32)frame->height);
    } else if (moved) {
        XMoveWindow(
            frame->system->x_display, info->xid, (i32)frame->x, (i32)frame->y);
    } else if (resized) {
        XResizeWindow(frame->system->x_display,
                      info->xid,
                      (u32)frame->width,
                      (u32)frame->height);
    }

    if (resized || resizable_changed) {
        _fs_apply_resizeable(frame, info->xid);
    }
    info->resizable = frame->resizable;

    bool title_changed = current.title.count != frame->title.count;
    if (!title_changed && current.title.count > 0) {
        title_changed = memcmp(current.title.data,
                               frame->title.data,
                               current.title.count) != 0;
    }

    if (title_changed) {
        cstr title_c_string =
            (cstr)arena_format(temp_arena(), STRINGP, STRINGV(frame->title));
        arena_null_terminate(temp_arena());
        XStoreName(frame->system->x_display, info->xid, title_c_string);
    }
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

void fs_update(Frame* frame)
{
    FrameInfo* info = _fs_find_frame_info(frame->system, frame->handle);
    if (!info) {
        return;
    }

    XWindowAttributes attrs;
    if (XGetWindowAttributes(frame->system->x_display, info->xid, &attrs)) {
        frame->x      = attrs.x;
        frame->y      = attrs.y;
        frame->width  = (u64)attrs.width;
        frame->height = (u64)attrs.height;
        info->x       = frame->x;
        info->y       = frame->y;
        info->width   = frame->width;
        info->height  = frame->height;
    }

    char* title = NULL;
    if (XFetchName(frame->system->x_display, info->xid, &title) > 0 && title) {
        usize title_len = 0;
        while (title[title_len] != '\0') {
            title_len++;
        }

        usize required_size = title_len + 1;
        if (!info->title_heap) {
            info->title_heap = ARRAY_ALLOC(u8, required_size);
        } else if (mem_size(info->title_heap) < required_size) {
            info->title_heap = ARRAY_REALLOC(info->title_heap, u8, required_size);
        }

        memcpy(info->title_heap, title, title_len);
        info->title_heap[title_len] = '\0';
        frame->title = string_from(info->title_heap, title_len);
        XFree(title);
    }
}

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

        event.frame_handle = info->handle;

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
        {
            bool moved =
                info->x != xev.xconfigure.x || info->y != xev.xconfigure.y;
            bool resized = info->width != (u64)xev.xconfigure.width ||
                           info->height != (u64)xev.xconfigure.height;

            info->x      = xev.xconfigure.x;
            info->y      = xev.xconfigure.y;
            info->width  = (u64)xev.xconfigure.width;
            info->height = (u64)xev.xconfigure.height;

            should_send_event = false;

            if (moved) {
                _fs_push_event(fs,
                               (FrameEvent){
                                   .kind         = FE_MOVE,
                                   .frame_handle = info->handle,
                                   .x            = info->x,
                                   .y            = info->y,
                               });
            }

            if (resized) {
                _fs_push_event(fs,
                               (FrameEvent){
                                   .kind         = FE_RESIZE,
                                   .frame_handle = info->handle,
                                   .width        = info->width,
                                   .height       = info->height,
                               });
            }
        } break;

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
