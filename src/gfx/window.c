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
// _fs_query_frame_position

internal void _fs_apply_resizeable(const Frame* frame, Window xid);

internal void
_fs_query_frame_position(FrameSystem* fs, Window xid, i64* out_x, i64* out_y)
{
    if (!out_x || !out_y) {
        return;
    }

    Window child = 0;
    int    x     = 0;
    int    y     = 0;
    if (XTranslateCoordinates(
            fs->x_display, xid, fs->x_root_window, 0, 0, &x, &y, &child)) {
        *out_x = x;
        *out_y = y;
    }
}

//------------------------------------------------------------------------------
// _fs_cache_windowed_geometry

internal void _fs_cache_windowed_geometry(FrameInfo* info, const Frame* frame)
{
    info->windowed_x      = frame->x;
    info->windowed_y      = frame->y;
    info->windowed_width  = frame->width;
    info->windowed_height = frame->height;
}

//------------------------------------------------------------------------------
// _fs_restore_windowed_geometry

internal void _fs_restore_windowed_geometry(Frame* frame, const FrameInfo* info)
{
    frame->x      = info->windowed_x;
    frame->y      = info->windowed_y;
    frame->width  = info->windowed_width;
    frame->height = info->windowed_height;
}

//------------------------------------------------------------------------------
// _fs_clear_resizeable_constraints

internal void _fs_clear_resizeable_constraints(FrameSystem* fs, Window xid)
{
    Frame unconstrained = {
        .system    = fs,
        .resizable = true,
    };
    _fs_apply_resizeable(&unconstrained, xid);
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
// _fs_apply_fullscreen

internal void _fs_apply_fullscreen(const Frame* frame, Window xid, bool enable)
{
    static Atom wm_state        = None;
    static Atom fullscreen_atom = None;

    if (wm_state == None) {
        wm_state =
            XInternAtom(frame->system->x_display, "_NET_WM_STATE", False);
    }
    if (fullscreen_atom == None) {
        fullscreen_atom = XInternAtom(
            frame->system->x_display, "_NET_WM_STATE_FULLSCREEN", False);
    }

    XEvent xev               = {0};
    xev.type                 = ClientMessage;
    xev.xclient.window       = xid;
    xev.xclient.message_type = wm_state;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = enable ? 1 : 0;
    xev.xclient.data.l[1]    = fullscreen_atom;
    xev.xclient.data.l[2]    = 0;
    xev.xclient.data.l[3]    = 1;

    XSendEvent(frame->system->x_display,
               DefaultRootWindow(frame->system->x_display),
               False,
               SubstructureRedirectMask | SubstructureNotifyMask,
               &xev);
    XFlush(frame->system->x_display);
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

    FrameInfo* info                = _fs_new_frame_info(fs, handle);
    info->handle                   = handle;
    info->xid                      = new_frame;
    info->resizable                = frame->resizable;
    info->fullscreen               = false;
    info->restore_windowed_pending = false;

    XWindowAttributes attrs;
    if (XGetWindowAttributes(fs->x_display, new_frame, &attrs)) {
        info->width  = (u64)attrs.width;
        info->height = (u64)attrs.height;
    } else {
        info->x      = frame->x;
        info->y      = frame->y;
        info->width  = frame->width;
        info->height = frame->height;
    }
    _fs_query_frame_position(fs, new_frame, &info->x, &info->y);
    _fs_cache_windowed_geometry(info,
                                &(Frame){
                                    .x      = info->x,
                                    .y      = info->y,
                                    .width  = info->width,
                                    .height = info->height,
                                });

    if (frame->fullscreen) {
        if (!frame->resizable) {
            _fs_clear_resizeable_constraints(fs, new_frame);
        }
        _fs_apply_fullscreen(frame, new_frame, true);
        info->fullscreen = true;
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
    bool resizable_changed  = info->resizable != frame->resizable;
    bool fullscreen_changed = info->fullscreen != frame->fullscreen;

    if (!info->fullscreen && !info->restore_windowed_pending) {
        _fs_cache_windowed_geometry(info, &current);
    }

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

    if (fullscreen_changed) {
        if (frame->fullscreen && !frame->resizable) {
            _fs_clear_resizeable_constraints(frame->system, info->xid);
        }

        if (frame->fullscreen) {
            _fs_cache_windowed_geometry(info, &current);
        }

        _fs_apply_fullscreen(frame, info->xid, frame->fullscreen);

        if (!frame->fullscreen) {
            _fs_restore_windowed_geometry(frame, info);
            info->restore_windowed_pending = true;
        }

        info->fullscreen = frame->fullscreen;
    }

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
// Keyboard utilties

internal u32 _frame_x11_keysym_to_char(XEvent xev)
{
    u32 key_char = 0;
    if (xev.type == KeyPress) {
        char buf[8];
        int  len = XLookupString(&xev.xkey, buf, (int)sizeof(buf), NULL, NULL);
        if (len > 0) {
            key_char = (u8)buf[0];
        }
    }

    return key_char;
}

internal inline FrameKey _frame_x11_keysym_to_key(KeySym sym)
{
    if (sym >= XK_a && sym <= XK_z) {
        return (FrameKey)(KEY_A + (sym - XK_a));
    }
    if (sym >= XK_A && sym <= XK_Z) {
        return (FrameKey)(KEY_A + (sym - XK_A));
    }
    if (sym >= XK_1 && sym <= XK_9) {
        return (FrameKey)(KEY_1 + (sym - XK_1));
    }
    if (sym == XK_0) {
        return KEY_0;
    }
    if (sym >= XK_F1 && sym <= XK_F24) {
        return (FrameKey)(KEY_F1 + (sym - XK_F1));
    }
    if (sym >= XK_KP_0 && sym <= XK_KP_9) {
        return (FrameKey)(KEY_KP_0 + (sym - XK_KP_0));
    }

    switch (sym) {
    case XK_Return:
        return KEY_ENTER;
    case XK_Escape:
        return KEY_ESCAPE;
    case XK_BackSpace:
        return KEY_BACKSPACE;
    case XK_Tab:
        return KEY_TAB;
    case XK_space:
        return KEY_SPACE;
    case XK_minus:
        return KEY_MINUS;
    case XK_equal:
        return KEY_EQUALS;
    case XK_bracketleft:
        return KEY_LEFTBRACKET;
    case XK_bracketright:
        return KEY_RIGHTBRACKET;
    case XK_backslash:
        return KEY_BACKSLASH;
    case XK_semicolon:
        return KEY_SEMICOLON;
    case XK_apostrophe:
        return KEY_APOSTROPHE;
    case XK_grave:
        return KEY_GRAVE;
    case XK_comma:
        return KEY_COMMA;
    case XK_period:
        return KEY_PERIOD;
    case XK_slash:
        return KEY_SLASH;

    case XK_KP_Add:
        return KEY_KP_PLUS;
    case XK_KP_Subtract:
        return KEY_KP_MINUS;
    case XK_KP_Multiply:
        return KEY_KP_MULTIPLY;
    case XK_KP_Divide:
        return KEY_KP_DIVIDE;
    case XK_KP_Enter:
        return KEY_KP_ENTER;
    case XK_KP_Decimal:
        return KEY_KP_PERIOD;
    case XK_KP_Equal:
        return KEY_KP_EQUALS;
    case XK_KP_Separator:
        return KEY_KP_COMMA;

    case XK_Caps_Lock:
        return KEY_CAPSLOCK;
    case XK_Num_Lock:
        return KEY_NUMLOCKCLEAR;
    case XK_Print:
        return KEY_PRINTSCREEN;
    case XK_Scroll_Lock:
        return KEY_SCROLLLOCK;
    case XK_Pause:
        return KEY_PAUSE;

    case XK_Insert:
        return KEY_INSERT;
    case XK_Delete:
        return KEY_DELETE;
    case XK_Home:
        return KEY_HOME;
    case XK_End:
        return KEY_END;
    case XK_Prior:
        return KEY_PAGEUP;
    case XK_Next:
        return KEY_PAGEDOWN;

    case XK_Left:
        return KEY_LEFT;
    case XK_Right:
        return KEY_RIGHT;
    case XK_Up:
        return KEY_UP;
    case XK_Down:
        return KEY_DOWN;

    case XK_Shift_L:
        return KEY_LSHIFT;
    case XK_Shift_R:
        return KEY_RSHIFT;
    case XK_Control_L:
        return KEY_LCTRL;
    case XK_Control_R:
        return KEY_RCTRL;
    case XK_Alt_L:
        return KEY_LALT;
    case XK_Alt_R:
        return KEY_RALT;
    case XK_Super_L:
        return KEY_LGUI;
    case XK_Super_R:
        return KEY_RGUI;
    case XK_Menu:
        return KEY_MENU;

    default:
        return KEY_UNKNOWN;
    }
}

internal inline FrameKeyShift _frame_x11_modifiers(unsigned int state,
                                                   KeySym       sym)
{
    FrameKeyShift mods = 0;
    if (state & ShiftMask) {
        mods |= KEY_SHIFT_LEFT;
    }
    if (state & ControlMask) {
        mods |= KEY_CTRL_LEFT;
    }
    if (state & Mod1Mask) {
        mods |= KEY_ALT_LEFT;
    }

    switch (sym) {
    case XK_Shift_L:
        mods |= KEY_SHIFT_LEFT;
        break;
    case XK_Shift_R:
        mods |= KEY_SHIFT_RIGHT;
        break;
    case XK_Control_L:
        mods |= KEY_CTRL_LEFT;
        break;
    case XK_Control_R:
        mods |= KEY_CTRL_RIGHT;
        break;
    case XK_Alt_L:
        mods |= KEY_ALT_LEFT;
        break;
    case XK_Alt_R:
        mods |= KEY_ALT_RIGHT;
        break;
    default:
        break;
    }

    return mods;
}
//------------------------------------------------------------------------------
// fs_init

void fs_init(FrameSystem* fs)
{
    temp_arena_init();

    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fatal_error("Failed to open X display");
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

    frame->x = info->x;
    frame->y = info->y;

    XWindowAttributes attrs;
    if (XGetWindowAttributes(frame->system->x_display, info->xid, &attrs)) {
        frame->width  = (u64)attrs.width;
        frame->height = (u64)attrs.height;
        info->width   = frame->width;
        info->height  = frame->height;
    }
    _fs_query_frame_position(frame->system, info->xid, &frame->x, &frame->y);
    info->x     = frame->x;
    info->y     = frame->y;

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
            info->title_heap =
                ARRAY_REALLOC(info->title_heap, u8, required_size);
        }

        memcpy(info->title_heap, title, title_len);
        info->title_heap[title_len] = '\0';
        frame->title                = string_from(info->title_heap, title_len);
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
        case KeyRelease:
            event.kind      = (xev.type == KeyPress) ? FE_KEYDOWN : FE_KEYUP;
            KeySym keysym   = XLookupKeysym(&xev.xkey, 0);
            event.keycode   = _frame_x11_keysym_to_key(keysym);
            event.modifiers = _frame_x11_modifiers(xev.xkey.state, keysym);
            event.key_char  = _frame_x11_keysym_to_char(xev);
            break;

        case MotionNotify:
            event.kind = FE_MOUSEMOVE;
            event.x    = xev.xmotion.x;
            event.y    = xev.xmotion.y;
            break;

        case ButtonPress:
        case ButtonRelease:
            event.kind    = (xev.type == ButtonPress) ? FE_MOUSEBUTTONDOWN
                                                      : FE_MOUSEBUTTONUP;
            event.mouse_x = xev.xbutton.x;
            event.mouse_y = xev.xbutton.y;
            event.button  = 0;
            switch (xev.xbutton.button) {
            case Button1:
                event.button = MOUSE_BUTTON_LEFT;
                break;
            case Button2:
                event.button = MOUSE_BUTTON_MIDDLE;
                break;
            case Button3:
                event.button = MOUSE_BUTTON_RIGHT;
                break;
            case Button4:
                event.button = MOUSE_BUTTON_SIDE_1;
                break;
            case Button5:
                event.button = MOUSE_BUTTON_SIDE_2;
                break;
            default:
                should_send_event = false;
            }
            break;

        case ConfigureNotify:
            {
                bool moved   = false;
                bool resized = info->width != (u64)xev.xconfigure.width ||
                               info->height != (u64)xev.xconfigure.height;

                i64 new_x = info->x;
                i64 new_y = info->y;
                _fs_query_frame_position(fs, info->xid, &new_x, &new_y);
                moved             = info->x != new_x || info->y != new_y;

                info->x           = new_x;
                info->y           = new_y;
                info->width       = (u64)xev.xconfigure.width;
                info->height      = (u64)xev.xconfigure.height;

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

                if (info->restore_windowed_pending) {
                    bool restored = info->x == info->windowed_x &&
                                    info->y == info->windowed_y &&
                                    info->width == info->windowed_width &&
                                    info->height == info->windowed_height;

                    if (restored) {
                        info->restore_windowed_pending = false;
                        if (!info->resizable) {
                            Frame restore_frame = {
                                .system    = fs,
                                .width     = info->windowed_width,
                                .height    = info->windowed_height,
                                .resizable = false,
                            };
                            _fs_apply_resizeable(&restore_frame, info->xid);
                        }
                    } else {
                        XMoveResizeWindow(fs->x_display,
                                          info->xid,
                                          (i32)info->windowed_x,
                                          (i32)info->windowed_y,
                                          (u32)info->windowed_width,
                                          (u32)info->windowed_height);
                    }
                }
            }
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
