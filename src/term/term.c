//------------------------------------------------------------------------------
// Terminal API
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
#include <term/term.h>

//------------------------------------------------------------------------------

#include <locale.h>

Term g_term;
bool g_cursor_visible      = true;
bool g_cursor_dirty        = true;
int  g_cursor_x            = 0;
int  g_cursor_y            = 0;
u32  g_cursor_ink          = 0xFFFFFF;
u32  g_cursor_paper        = 0x000000;

Array(u32) g_term_fb_chars = NULL;
Array(u32) g_term_fb_ink   = NULL;
Array(u32) g_term_fb_paper = NULL;
Array(u8) g_term_fb_dirty  = NULL;
TermSize g_term_fb_size    = {0};
Arena    g_term_arena;

internal void _term_queue_event(TermEvent event);
internal void _term_queue_mouse(u16 x, u16 y, i16 wheel, u8 buttons);
internal void _term_queue_key_char(char c, u8 modifiers);
internal void _term_queue_key_special(TermKeyCode code, u8 modifiers);
internal void _term_alt_enter();
internal void _term_alt_leave();
internal void _term_raw_enter();
internal void _term_raw_leave();

void _term_fb_resize(u16 width, u16 height);
void _term_fb_done();
bool _term_fb_has_dirty(void);
void _term_fb_present_now(void);

internal void _term_start(void);
internal void _term_stop(void);
internal void _term_check_resize(void);
internal void _term_maybe_present(void);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// WINDOWS IMPLEMENTATION
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#if OS_WINDOWS

global_variable HANDLE g_term_console_input         = INVALID_HANDLE_VALUE;
global_variable DWORD  g_term_console_input_mode    = 0;
global_variable bool   g_term_console_mode_captured = false;

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_size_get
//
// Query the current terminal dimensions on Windows.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_size_get
//
// Query the current terminal dimensions on POSIX systems.
//------------------------------------------------------------------------------

TermSize term_size_get(void)
{
    TermSize term_size = {0};

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        term_size.width  = info.srWindow.Right - info.srWindow.Left + 1;
        term_size.height = info.srWindow.Bottom - info.srWindow.Top + 1;
    } else {
        eprn("Failed to get terminal size on Windows");
        abort();
    }

    return term_size;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_platform_init
//
// Enable Windows virtual terminal output and seed the initial terminal size and
// resize event state.
//------------------------------------------------------------------------------

internal void _term_platform_init()
{
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console != INVALID_HANDLE_VALUE) {
        DWORD mode;
        if (GetConsoleMode(console, &mode)) {
            SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    TermSize size = term_size_get();
    g_term.size   = size;
    _term_fb_resize(size.width, size.height);

    TermEvent event;
    event.kind = TERM_EVENT_RESIZE;
    event.size = size;
    _term_queue_event(event);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_loop
//
// Poll Windows terminal input, dispatch resize, key, and mouse events, and
// present the framebuffer when needed.
//------------------------------------------------------------------------------

bool term_loop()
{
    if (g_term.running) {

        // Windows-specific event polling can be added here
        // We need to check any console events and handle them
        HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
        if (console != INVALID_HANDLE_VALUE) {
            DWORD events;
            GetNumberOfConsoleInputEvents(console, &events);
            while (events > 0) {
                INPUT_RECORD record;
                DWORD        read;
                ReadConsoleInputA(console, &record, 1, &read);

                switch (record.EventType) {
                case WINDOW_BUFFER_SIZE_EVENT:
                    {
                        g_term.size = term_size_get();
                        TermEvent event;
                        event.kind = TERM_EVENT_RESIZE;
                        event.size = g_term.size;
                        _term_queue_event(event);
                        _term_fb_resize(g_term.size.width, g_term.size.height);
                    }
                    break;

                case KEY_EVENT:
                    if (record.Event.KeyEvent.bKeyDown) {
                        KEY_EVENT_RECORD key = record.Event.KeyEvent;
                        u8 modifiers = 0;
                        if ((key.dwControlKeyState &
                             (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0) {
                            modifiers |= TERM_KEYMOD_CTRL;
                        }
                        if ((key.dwControlKeyState &
                             (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0) {
                            modifiers |= TERM_KEYMOD_ALT;
                        }
                        if ((key.dwControlKeyState & SHIFT_PRESSED) != 0) {
                            modifiers |= TERM_KEYMOD_SHIFT;
                        }

                        switch (key.wVirtualKeyCode) {
                        case VK_LEFT:
                            _term_queue_key_special(TERM_KEY_LEFT, modifiers);
                            break;
                        case VK_RIGHT:
                            _term_queue_key_special(TERM_KEY_RIGHT, modifiers);
                            break;
                        case VK_UP:
                            _term_queue_key_special(TERM_KEY_UP, modifiers);
                            break;
                        case VK_DOWN:
                            _term_queue_key_special(TERM_KEY_DOWN, modifiers);
                            break;
                        case VK_HOME:
                            _term_queue_key_special(TERM_KEY_HOME, modifiers);
                            break;
                        case VK_END:
                            _term_queue_key_special(TERM_KEY_END, modifiers);
                            break;
                        case VK_DELETE:
                            _term_queue_key_special(TERM_KEY_DELETE, modifiers);
                            break;
                        case VK_BACK:
                            _term_queue_key_special(TERM_KEY_BACKSPACE, modifiers);
                            break;
                        case VK_RETURN:
                            _term_queue_key_special(TERM_KEY_ENTER, modifiers);
                            break;
                        default:
                            if (key.uChar.AsciiChar != 0) {
                                _term_queue_key_char(key.uChar.AsciiChar, modifiers);
                            }
                            break;
                        }
                    }
                    break;

                case MOUSE_EVENT:
                    {
                        MOUSE_EVENT_RECORD mouse = record.Event.MouseEvent;
                        u8                 buttons = 0;
                        i16                wheel   = 0;

                        if (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                            buttons |= 0x1;
                        }
                        if (mouse.dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
                            buttons |= 0x2;
                        }
                        if (mouse.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) {
                            buttons |= 0x4;
                        }

                        if ((mouse.dwEventFlags & MOUSE_WHEELED) != 0) {
                            wheel = (SHORT)HIWORD(mouse.dwButtonState) > 0 ? 1 : -1;
                        }

                        _term_queue_mouse((u16)mouse.dwMousePosition.X,
                                          (u16)mouse.dwMousePosition.Y,
                                          wheel,
                                          buttons);
                    }
                    break;
                }
                events--;
            }
        }
        _term_maybe_present();
        return true;
    } else {
        _term_stop();
        return false;
    }
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_raw_enter
//
// Put the Windows console input handle into raw event mode.
//------------------------------------------------------------------------------

internal void _term_raw_enter(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE) {
        eprn("Failed to get console input handle");
        abort();
    }

    DWORD mode = 0;
    if (!GetConsoleMode(input, &mode)) {
        eprn("Failed to get console input mode");
        abort();
    }

    g_term_console_input         = input;
    g_term_console_input_mode    = mode;
    g_term_console_mode_captured = true;

    DWORD raw_mode               = mode;
    raw_mode &=
        ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    raw_mode |= (ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT);
    raw_mode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE);

    if (!SetConsoleMode(input, raw_mode)) {
        eprn("Failed to set raw console input mode");
        abort();
    }

    FlushConsoleInputBuffer(input);
}

//------------------------------------------------------------------------------
// _term_raw_leave
//
// Restore the original Windows console input mode.
//------------------------------------------------------------------------------

internal void _term_raw_leave(void)
{
    if (!g_term_console_mode_captured ||
        g_term_console_input == INVALID_HANDLE_VALUE) {
        return;
    }

    if (!SetConsoleMode(g_term_console_input, g_term_console_input_mode)) {
        eprn("Failed to restore console input mode");
        abort();
    }

    g_term_console_mode_captured = false;
}

//------------------------------------------------------------------------------

#endif // OS_WINDOWS

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// LINUX/MACOSX IMPLEMENTATION
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#if OS_POSIX

#    include <signal.h>
#    include <string.h>
#    include <sys/ioctl.h>
#    include <termios.h>

// Signals when the terminal resizes
global_variable volatile sig_atomic_t g_term_resize_signal = 0;
global_variable struct termios        g_term_original_tios;
global_variable char                  g_term_pending_bytes[32];
global_variable usize                 g_term_pending_count = 0;
global_variable usize                 g_term_pending_index = 0;
global_variable char                  g_term_escape_bytes[32];
global_variable usize                 g_term_escape_count = 0;
global_variable char                  g_term_test_read_bytes[128];
global_variable usize                 g_term_test_read_count = 0;
global_variable usize                 g_term_test_read_index = 0;
global_variable bool                  g_term_test_read_enabled = false;

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_queue_key
//
// Queue a plain character key event with no modifier flags.
//------------------------------------------------------------------------------

internal void _term_queue_key(char c)
{
    _term_queue_key_char(c, 0);
}

//------------------------------------------------------------------------------
// _term_queue_key_char
//
// Queue a character key event and infer common special-key codes for control
// characters such as enter and backspace.
//------------------------------------------------------------------------------

internal void _term_queue_key_char(char c, u8 modifiers)
{
    TermEvent event = {0};
    event.kind          = TERM_EVENT_KEY;
    event.key           = c;
    event.key_modifiers = modifiers;

    if (c == '\r' || c == '\n') {
        event.key_code = TERM_KEY_ENTER;
    } else if (c == 8 || c == 127 || c == 23) {
        event.key_code = TERM_KEY_BACKSPACE;
    }

    _term_queue_event(event);
}

//------------------------------------------------------------------------------
// _term_queue_key_special
//
// Queue a non-character key event such as arrows, home, or delete.
//------------------------------------------------------------------------------

internal void _term_queue_key_special(TermKeyCode code, u8 modifiers)
{
    TermEvent event = {0};
    event.kind          = TERM_EVENT_KEY;
    event.key_code      = code;
    event.key_modifiers = modifiers;
    _term_queue_event(event);
}

//------------------------------------------------------------------------------
// _term_queue_mouse
//
// Queue a mouse event using terminal cell coordinates and button state.
//------------------------------------------------------------------------------

internal void _term_queue_mouse(u16 x, u16 y, i16 wheel, u8 buttons)
{
    TermEvent event;
    event.kind         = TERM_EVENT_MOUSE;
    event.mouse.x      = x;
    event.mouse.y      = y;
    event.mouse.wheel  = wheel;
    event.mouse.buttons = buttons;
    _term_queue_event(event);
}

//------------------------------------------------------------------------------
// _term_posix_try_read_byte
//
// Read a single byte from stdin or the deterministic test input stream.
//------------------------------------------------------------------------------

internal bool _term_posix_try_read_byte(char* out_c)
{
    if (g_term_test_read_enabled) {
        if (g_term_test_read_index < g_term_test_read_count) {
            *out_c = g_term_test_read_bytes[g_term_test_read_index++];
            if (g_term_test_read_index == g_term_test_read_count) {
                g_term_test_read_count = 0;
                g_term_test_read_index = 0;
            }
            return true;
        }
        return false;
    }

    int nread = read(STDIN_FILENO, out_c, 1);
    return nread == 1;
}

internal bool _term_parse_u16_bytes(const char* bytes, usize count, u16* out_value);

//------------------------------------------------------------------------------
// _term_parse_modifiers
//
// Convert terminal CSI modifier numbering into term modifier bit flags.
//------------------------------------------------------------------------------

internal bool _term_parse_modifiers(u16 value, u8* out_modifiers)
{
    if (value < 2 || value > 8) {
        return false;
    }

    u8 modifiers = 0;
    if (value == 2 || value == 4 || value == 6 || value == 8) {
        modifiers |= TERM_KEYMOD_SHIFT;
    }
    if (value == 3 || value == 4 || value == 7 || value == 8) {
        modifiers |= TERM_KEYMOD_ALT;
    }
    if (value == 5 || value == 6 || value == 7 || value == 8) {
        modifiers |= TERM_KEYMOD_CTRL;
    }

    *out_modifiers = modifiers;
    return true;
}

//------------------------------------------------------------------------------
// _term_posix_decode_key_csi
//
// Decode a CSI key sequence into a structured key event.
//------------------------------------------------------------------------------

internal bool _term_posix_decode_key_csi(const char* bytes,
                                         usize       count,
                                         usize*      out_consumed)
{
    if (count < 3 || bytes[0] != '\033' || bytes[1] != '[') {
        return false;
    }

    usize final_pos = 0;
    for (usize i = 2; i < count; i++) {
        char final = bytes[i];
        if (final >= '@' && final <= '~') {
            final_pos = i;
            break;
        }
    }
    if (final_pos == 0) {
        return false;
    }

    char body[32] = {0};
    usize body_count = final_pos - 2;
    if (body_count >= sizeof(body)) {
        return false;
    }
    memcpy(body, bytes + 2, body_count);
    char final = bytes[final_pos];

    u8 modifiers = 0;
    TermKeyCode code = TERM_KEY_NONE;

    if ((final == 'A' || final == 'B' || final == 'C' || final == 'D' ||
         final == 'H' || final == 'F') &&
        body_count == 0) {
        switch (final) {
        case 'A': code = TERM_KEY_UP; break;
        case 'B': code = TERM_KEY_DOWN; break;
        case 'C': code = TERM_KEY_RIGHT; break;
        case 'D': code = TERM_KEY_LEFT; break;
        case 'H': code = TERM_KEY_HOME; break;
        case 'F': code = TERM_KEY_END; break;
        }
    } else if ((final == 'A' || final == 'B' || final == 'C' || final == 'D' ||
                final == 'H' || final == 'F') &&
               body_count > 0) {
        u16 params[3] = {0};
        usize param_count = 0;
        usize start = 0;
        for (usize i = 0; i <= body_count; i++) {
            if (i == body_count || body[i] == ';') {
                if (param_count >= ARRAY_COUNT(params) ||
                    !_term_parse_u16_bytes(body + start, i - start, &params[param_count])) {
                    return false;
                }
                param_count++;
                start = i + 1;
            }
        }

        if (param_count >= 2) {
            _term_parse_modifiers(params[1], &modifiers);
        }

        switch (final) {
        case 'A': code = TERM_KEY_UP; break;
        case 'B': code = TERM_KEY_DOWN; break;
        case 'C': code = TERM_KEY_RIGHT; break;
        case 'D': code = TERM_KEY_LEFT; break;
        case 'H': code = TERM_KEY_HOME; break;
        case 'F': code = TERM_KEY_END; break;
        }
    } else if (final == '~') {
        u16 params[3] = {0};
        usize param_count = 0;
        usize start = 0;
        for (usize i = 0; i <= body_count; i++) {
            if (i == body_count || body[i] == ';') {
                if (param_count >= ARRAY_COUNT(params) ||
                    !_term_parse_u16_bytes(body + start, i - start, &params[param_count])) {
                    return false;
                }
                param_count++;
                start = i + 1;
            }
        }

        if (param_count >= 2) {
            _term_parse_modifiers(params[1], &modifiers);
        }

        switch (params[0]) {
        case 1:
        case 7: code = TERM_KEY_HOME; break;
        case 4:
        case 8: code = TERM_KEY_END; break;
        case 3: code = TERM_KEY_DELETE; break;
        default: return false;
        }
    } else {
        return false;
    }

    if (code == TERM_KEY_NONE) {
        return false;
    }

    _term_queue_key_special(code, modifiers);
    *out_consumed = final_pos + 1;
    return true;
}

//------------------------------------------------------------------------------
// _term_posix_buffer_pending
//
// Stash plain bytes that should be emitted as ordinary key input on later raw
// key ticks.
//------------------------------------------------------------------------------

internal void _term_posix_buffer_pending(const char* bytes, usize count)
{
    if (count == 0) {
        g_term_pending_count = 0;
        g_term_pending_index = 0;
        return;
    }

    count = MIN(count, ARRAY_COUNT(g_term_pending_bytes));
    memcpy(g_term_pending_bytes, bytes, count);
    g_term_pending_count = count;
    g_term_pending_index = 0;
}

//------------------------------------------------------------------------------
// _term_parse_u16_bytes
//
// Parse an unsigned 16-bit integer from an ASCII digit span.
//------------------------------------------------------------------------------

internal bool _term_parse_u16_bytes(const char* bytes, usize count, u16* out_value)
{
    u64 value = 0;
    if (count == 0) {
        return false;
    }

    for (usize i = 0; i < count; i++) {
        char c = bytes[i];
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (u64)(c - '0');
        if (value > 65535) {
            return false;
        }
    }

    *out_value = (u16)value;
    return true;
}

//------------------------------------------------------------------------------
// _term_posix_shift_escape
//
// Remove consumed bytes from the front of the escape-sequence buffer.
//------------------------------------------------------------------------------

internal void _term_posix_shift_escape(usize consumed)
{
    if (consumed >= g_term_escape_count) {
        g_term_escape_count = 0;
        return;
    }

    memmove(g_term_escape_bytes,
            g_term_escape_bytes + consumed,
            g_term_escape_count - consumed);
    g_term_escape_count -= consumed;
}

//------------------------------------------------------------------------------
// _term_posix_decode_mouse_sgr
//
// Decode an SGR mouse report sequence.
//------------------------------------------------------------------------------

internal bool _term_posix_decode_mouse_sgr(const char* bytes,
                                           usize       count,
                                           usize*      out_consumed)
{
    if (count < 6 || bytes[0] != '\033' || bytes[1] != '[' || bytes[2] != '<') {
        return false;
    }

    usize first_sep  = 0;
    usize second_sep = 0;
    usize final_pos  = 0;
    for (usize i = 3; i < count; i++) {
        if (bytes[i] == ';') {
            if (first_sep == 0) {
                first_sep = i;
            } else if (second_sep == 0) {
                second_sep = i;
            }
        } else if (bytes[i] == 'M' || bytes[i] == 'm') {
            final_pos = i;
            break;
        }
    }

    if (first_sep == 0 || second_sep == 0 || final_pos == 0) {
        return false;
    }

    u16 cb = 0;
    u16 cx = 0;
    u16 cy = 0;
    if (!_term_parse_u16_bytes(bytes + 3, first_sep - 3, &cb) ||
        !_term_parse_u16_bytes(bytes + first_sep + 1,
                               second_sep - first_sep - 1,
                               &cx) ||
        !_term_parse_u16_bytes(
            bytes + second_sep + 1, final_pos - second_sep - 1, &cy)) {
        return false;
    }

    u8  buttons = 0;
    i16 wheel   = 0;
    u16 base    = cb & 0x3;
    if ((cb & 0x40) != 0) {
        wheel = base == 0 ? 1 : -1;
    } else if (bytes[final_pos] == 'M') {
        if (base == 0) {
            buttons = 0x1;
        } else if (base == 1) {
            buttons = 0x4;
        } else if (base == 2) {
            buttons = 0x2;
        }
    }

    _term_queue_mouse(cx > 0 ? cx - 1 : 0, cy > 0 ? cy - 1 : 0, wheel, buttons);
    *out_consumed = final_pos + 1;
    return true;
}

//------------------------------------------------------------------------------
// _term_posix_decode_mouse_1015
//
// Decode a legacy UTF-8 extended mouse report in xterm 1015 format.
//------------------------------------------------------------------------------

internal bool _term_posix_decode_mouse_1015(const char* bytes,
                                            usize       count,
                                            usize*      out_consumed)
{
    if (count < 7 || bytes[0] != '\033' || bytes[1] != '[') {
        return false;
    }
    if (bytes[2] < '0' || bytes[2] > '9') {
        return false;
    }

    usize first_sep  = 0;
    usize second_sep = 0;
    usize final_pos  = 0;
    for (usize i = 2; i < count; i++) {
        if (bytes[i] == ';') {
            if (first_sep == 0) {
                first_sep = i;
            } else if (second_sep == 0) {
                second_sep = i;
            }
        } else if (bytes[i] == 'M') {
            final_pos = i;
            break;
        }
    }

    if (first_sep == 0 || second_sep == 0 || final_pos == 0) {
        return false;
    }

    u16 cb = 0;
    u16 cx = 0;
    u16 cy = 0;
    if (!_term_parse_u16_bytes(bytes + 2, first_sep - 2, &cb) ||
        !_term_parse_u16_bytes(bytes + first_sep + 1,
                               second_sep - first_sep - 1,
                               &cx) ||
        !_term_parse_u16_bytes(
            bytes + second_sep + 1, final_pos - second_sep - 1, &cy)) {
        return false;
    }

    u8  buttons = 0;
    i16 wheel   = 0;
    u16 base    = cb & 0x3;
    if ((cb & 0x40) != 0) {
        wheel = base == 0 ? 1 : -1;
    } else if (base == 0) {
        buttons = 0x1;
    } else if (base == 1) {
        buttons = 0x4;
    } else if (base == 2) {
        buttons = 0x2;
    }

    _term_queue_mouse(cx > 0 ? cx - 1 : 0, cy > 0 ? cy - 1 : 0, wheel, buttons);
    *out_consumed = final_pos + 1;
    return true;
}

//------------------------------------------------------------------------------
// _term_posix_decode_mouse_x10
//
// Decode an X10 mouse report sequence.
//------------------------------------------------------------------------------

internal bool _term_posix_decode_mouse_x10(const char* bytes,
                                           usize       count,
                                           usize*      out_consumed)
{
    if (count < 6 || bytes[0] != '\033' || bytes[1] != '[' || bytes[2] != 'M') {
        return false;
    }

    u8 cb = (u8)bytes[3] - 32;
    u8 cx = (u8)bytes[4] - 32;
    u8 cy = (u8)bytes[5] - 32;

    u8  buttons = 0;
    i16 wheel   = 0;
    u8  base    = cb & 0x3;
    if ((cb & 0x40) != 0) {
        wheel = base == 0 ? 1 : -1;
    } else if (base == 0) {
        buttons = 0x1;
    } else if (base == 1) {
        buttons = 0x4;
    } else if (base == 2) {
        buttons = 0x2;
    }

    _term_queue_mouse(cx > 0 ? (u16)(cx - 1) : 0,
                      cy > 0 ? (u16)(cy - 1) : 0,
                      wheel,
                      buttons);
    *out_consumed = 6;
    return true;
}

//------------------------------------------------------------------------------
// _term_posix_swallow_mouse_tail
//
// Detect and discard stray mouse-sequence tail fragments so they do not leak
// into ordinary key input.
//------------------------------------------------------------------------------

internal bool _term_posix_swallow_mouse_tail(char first)
{
    if (first != ';') {
        return false;
    }

    char  tail[32];
    usize count = 0;
    tail[count++] = first;

    char next;
    while (count < ARRAY_COUNT(tail) && _term_posix_try_read_byte(&next)) {
        tail[count++] = next;
    }

    if (count < 3) {
        _term_queue_key(first);
        if (count > 1) {
            _term_posix_buffer_pending(tail + 1, count - 1);
        }
        return true;
    }

    if (tail[count - 1] != 'M') {
        _term_queue_key(first);
        if (count > 1) {
            _term_posix_buffer_pending(tail + 1, count - 1);
        }
        return true;
    }

    for (usize i = 1; i + 1 < count; i++) {
        if ((tail[i] < '0' || tail[i] > '9') && tail[i] != ';') {
            _term_queue_key(first);
            _term_posix_buffer_pending(tail + 1, count - 1);
            return true;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// _term_posix_drain_escape_buffer
//
// Consume one complete buffered escape sequence, buffering leftovers when they
// resolve to plain input.
//------------------------------------------------------------------------------

internal bool _term_posix_drain_escape_buffer(void)
{
    if (g_term_escape_count > 0 && g_term_escape_bytes[0] != '\033') {
        _term_posix_buffer_pending(g_term_escape_bytes, g_term_escape_count);
        g_term_escape_count = 0;
        return true;
    }

    if (g_term_escape_count < 2 || g_term_escape_bytes[0] != '\033') {
        return false;
    }

    if (g_term_escape_count == 1) {
        return false;
    }

    if (g_term_escape_bytes[1] == 'O') {
        for (usize i = 2; i < g_term_escape_count; i++) {
            char final = g_term_escape_bytes[i];
            if (final >= '@' && final <= '~') {
                switch (final) {
                case 'H':
                    _term_queue_key_special(TERM_KEY_HOME, 0);
                    break;
                case 'F':
                    _term_queue_key_special(TERM_KEY_END, 0);
                    break;
                default:
                    break;
                }
                _term_posix_shift_escape(i + 1);
                return true;
            }
        }
        return false;
    }

    if (g_term_escape_bytes[1] != '[') {
        _term_queue_key(g_term_escape_bytes[0]);
        _term_posix_buffer_pending(
            g_term_escape_bytes + 1, g_term_escape_count - 1);
        g_term_escape_count = 0;
        return true;
    }

    usize consumed = 0;

    if (g_term_escape_count >= 6 && g_term_escape_bytes[2] == 'M' &&
        _term_posix_decode_mouse_x10(
            g_term_escape_bytes, g_term_escape_count, &consumed)) {
        _term_posix_shift_escape(consumed);
        return true;
    }

    if (g_term_escape_count >= 3 && g_term_escape_bytes[2] == '<') {
        for (usize i = 3; i < g_term_escape_count; i++) {
            if (g_term_escape_bytes[i] == 'm' || g_term_escape_bytes[i] == 'M') {
                if (_term_posix_decode_mouse_sgr(
                        g_term_escape_bytes, g_term_escape_count, &consumed)) {
                    _term_posix_shift_escape(consumed);
                }
                return true;
            }
        }
        return false;
    }

    if (g_term_escape_count >= 3 &&
        g_term_escape_bytes[2] >= '0' && g_term_escape_bytes[2] <= '9') {
        for (usize i = 2; i < g_term_escape_count; i++) {
            if (g_term_escape_bytes[i] == 'M') {
                if (_term_posix_decode_mouse_1015(
                        g_term_escape_bytes, g_term_escape_count, &consumed)) {
                    _term_posix_shift_escape(consumed);
                }
                return true;
            }
        }
    }

    if (g_term_escape_count >= 3) {
        if (_term_posix_decode_key_csi(
                g_term_escape_bytes, g_term_escape_count, &consumed)) {
            _term_posix_shift_escape(consumed);
            return true;
        }
        for (usize i = 2; i < g_term_escape_count; i++) {
            char final = g_term_escape_bytes[i];
            if (final >= '@' && final <= '~') {
                _term_posix_shift_escape(i + 1);
                return true;
            }
        }
    }

    return false;
}

//------------------------------------------------------------------------------
// _term_posix_try_drain_escape
//
// Extend the escape buffer from stdin and attempt to decode one sequence.
//------------------------------------------------------------------------------

internal bool _term_posix_try_drain_escape(void)
{
    char next;
    while (g_term_escape_count < ARRAY_COUNT(g_term_escape_bytes) &&
           _term_posix_try_read_byte(&next)) {
        g_term_escape_bytes[g_term_escape_count++] = next;
    }
    return _term_posix_drain_escape_buffer();
}

//------------------------------------------------------------------------------

TermSize term_size_get(void)
{
    TermSize term_size = {0};

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        term_size.width  = w.ws_col;
        term_size.height = w.ws_row;
    } else {
        eprn("Failed to get terminal size on Unix");
        abort();
    }

    return term_size;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_raw_enter
//
// Put the POSIX terminal into raw mode and clear buffered input state.
//------------------------------------------------------------------------------

internal void _term_raw_enter(void)
{
    tcgetattr(STDIN_FILENO, &g_term_original_tios);
    struct termios raw_tios = g_term_original_tios;

    raw_tios.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw_tios.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw_tios.c_oflag &= ~(OPOST);
    raw_tios.c_cflag |= (CS8);

    // Set read timeout
    raw_tios.c_cc[VMIN]  = 0;
    raw_tios.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_tios);
    g_term_pending_count = 0;
    g_term_pending_index = 0;
    g_term_escape_count  = 0;
}

//------------------------------------------------------------------------------
// _term_raw_leave
//
// Restore the original POSIX terminal mode.
//------------------------------------------------------------------------------

internal void _term_raw_leave(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_term_original_tios);
}

//------------------------------------------------------------------------------
// _term_raw_key
//
// Read and decode one unit of POSIX raw input, handling buffered plain bytes
// and escape sequences.
//------------------------------------------------------------------------------

internal void _term_raw_key(void)
{
    if (g_term_pending_index < g_term_pending_count) {
        _term_queue_key(g_term_pending_bytes[g_term_pending_index++]);
        if (g_term_pending_index == g_term_pending_count) {
            g_term_pending_count = 0;
            g_term_pending_index = 0;
        }
        return;
    }

    if (g_term_escape_count > 0) {
        _term_posix_try_drain_escape();
        return;
    }

    char first;
    if (!_term_posix_try_read_byte(&first)) {
        return;
    }

    if (first != '\033') {
        if (_term_posix_swallow_mouse_tail(first)) {
            return;
        }
        _term_queue_key(first);
        return;
    }

    g_term_escape_bytes[0] = first;
    g_term_escape_count    = 1;
    _term_posix_try_drain_escape();
}

//------------------------------------------------------------------------------
// _term_test_posix_clear_input_buffers
//
// Reset deterministic POSIX test input state.
//------------------------------------------------------------------------------

void _term_test_posix_clear_input_buffers(void)
{
    g_term_pending_count = 0;
    g_term_pending_index = 0;
    g_term_escape_count  = 0;
    g_term_test_read_count   = 0;
    g_term_test_read_index   = 0;
    g_term_test_read_enabled = false;
}

//------------------------------------------------------------------------------
// _term_test_posix_set_read_bytes
//
// Seed the deterministic POSIX test input stream.
//------------------------------------------------------------------------------

void _term_test_posix_set_read_bytes(const char* bytes, usize count)
{
    count = MIN(count, ARRAY_COUNT(g_term_test_read_bytes));
    memcpy(g_term_test_read_bytes, bytes, count);
    g_term_test_read_count   = count;
    g_term_test_read_index   = 0;
    g_term_test_read_enabled = true;
}

//------------------------------------------------------------------------------
// _term_test_posix_set_escape_buffer
//
// Seed the escape buffer directly for deterministic decoder tests.
//------------------------------------------------------------------------------

void _term_test_posix_set_escape_buffer(const char* bytes, usize count)
{
    count = MIN(count, ARRAY_COUNT(g_term_escape_bytes));
    memcpy(g_term_escape_bytes, bytes, count);
    g_term_escape_count = count;
}

//------------------------------------------------------------------------------
// _term_test_posix_drain_escape_buffer_once
//
// Execute a single deterministic escape-buffer drain step for tests.
//------------------------------------------------------------------------------

bool _term_test_posix_drain_escape_buffer_once(void)
{
    return _term_posix_drain_escape_buffer();
}

//------------------------------------------------------------------------------
// _term_test_posix_raw_key_once
//
// Execute one deterministic raw-key step for tests.
//------------------------------------------------------------------------------

void _term_test_posix_raw_key_once(void)
{
    _term_raw_key();
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_on_winch
//
// Signal handler that marks terminal size as stale after SIGWINCH.
//------------------------------------------------------------------------------

internal void _term_on_winch(int sig)
{
    UNUSED(sig);
    g_term_resize_signal = 1;
}

//------------------------------------------------------------------------------
// _term_install_resize_handler
//
// Install the SIGWINCH resize handler used by the term loop.
//------------------------------------------------------------------------------

internal void _term_install_resize_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _term_on_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);
}

//------------------------------------------------------------------------------
// _term_remove_resize_handler
//
// Restore the default SIGWINCH handler.
//------------------------------------------------------------------------------

internal void _term_remove_resize_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGWINCH, &sa, NULL);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_platform_init
//
// Initialise POSIX-specific signal handling and schedule the initial resize
// event.
//------------------------------------------------------------------------------

internal void _term_platform_init(void)
{
    _term_install_resize_handler();

    // Force the sending of at least one resize event so we can react to the
    // initial size.  Windows does this already, we make POSIX systems do the
    // same.
    g_term_resize_signal = 1;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_loop
//
// Poll POSIX terminal input, handle resizes, and present the framebuffer when
// needed.
//------------------------------------------------------------------------------

bool term_loop(void)
{
    if (g_term.running) {

        // Check for terminal resize signal
        _term_check_resize();
        _term_raw_key();
        _term_maybe_present();
        return true;
    } else {
        // Unregister the signal handler
        _term_remove_resize_handler();
        _term_stop();
        return false;
    }
}

//------------------------------------------------------------------------------

#endif // OS_POSIX

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// S H A R E D   I M P L E M E N T A T I O N
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// dump_term_size
//
// Print a formatted summary of the current terminal size using normal terminal
// output.
//------------------------------------------------------------------------------

void dump_term_size(void)
{
    prn(ANSI_YELLOW "┌──────────────────┬──────────┐");
    pr(ANSI_YELLOW "│ " ANSI_CYAN "Terminal Columns");
    prn(ANSI_YELLOW " │ " ANSI_GREEN "%5u " ANSI_YELLOW "   │",
        g_term.size.width);
    pr(ANSI_YELLOW "│ " ANSI_CYAN "Terminal Rows");
    prn(ANSI_YELLOW "    │ " ANSI_GREEN "%5u " ANSI_YELLOW "   │",
        g_term.size.height);
    prn(ANSI_YELLOW "└──────────────────┴──────────┘");
}

//------------------------------------------------------------------------------
// dump_term_size_raw
//
// Print a formatted summary of the current terminal size using explicit cursor
// movement.
//------------------------------------------------------------------------------

void dump_term_size_raw(void)
{
    term_cursor_goto(0, 0);
    pr(ANSI_YELLOW "┌──────────────────┬──────────┐");
    term_cursor_goto(0, 1);
    pr(ANSI_YELLOW "│ " ANSI_CYAN "Terminal Columns");
    prn(ANSI_YELLOW " │ " ANSI_GREEN "%5u " ANSI_YELLOW "   │",
        g_term.size.width);
    term_cursor_goto(0, 2);
    pr(ANSI_YELLOW "│ " ANSI_CYAN "Terminal Rows");
    prn(ANSI_YELLOW "    │ " ANSI_GREEN "%5u " ANSI_YELLOW "   │",
        g_term.size.height);
    term_cursor_goto(0, 3);
    prn(ANSI_YELLOW "└──────────────────┴──────────┘");
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_queue_event
//
// Append an event to the shared terminal event queue.
//------------------------------------------------------------------------------

internal void _term_queue_event(TermEvent event)
{
    array_push(g_term.event_queue, event);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_alt_enter
//
// Enter the terminal alternate screen and enable mouse reporting.
//------------------------------------------------------------------------------

internal void _term_alt_enter(void)
{
    pr("\x1b[?1049h\x1b[?1007l\x1b[?1002h\x1b[?1006h");
}

//------------------------------------------------------------------------------
// _term_alt_leave
//
// Disable mouse reporting and leave the terminal alternate screen.
//------------------------------------------------------------------------------

internal void _term_alt_leave(void)
{
    pr("\x1b[?1002l\x1b[?1006l\x1b[?1007h\x1b[?1049l");
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_start
//
// Perform platform-specific startup, enter the alternate screen, and enable raw
// input handling.
//------------------------------------------------------------------------------

internal void _term_start(void)
{
    _term_platform_init();
    _term_alt_enter();
    _term_raw_enter();
}

//------------------------------------------------------------------------------
// _term_stop
//
// Tear down raw input, framebuffer state, and alternate-screen state.
//------------------------------------------------------------------------------

internal void _term_stop(void)
{
    array_free(g_term.event_queue);
    _term_fb_done();
    term_cursor_show();
    _term_alt_leave();
    _term_raw_leave();
    arena_done(&g_term_arena);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// _term_check_resize
//
// Detect a terminal size change and queue a resize event when dimensions have
// changed.
//------------------------------------------------------------------------------

internal void _term_check_resize()
{
    TermSize new_size = term_size_get();
    if (new_size.width != g_term.size.width ||
        new_size.height != g_term.size.height) {
        g_term.size = new_size;
        TermEvent event;
        event.kind = TERM_EVENT_RESIZE;
        event.size = g_term.size;
        _term_queue_event(event);
        _term_fb_resize(new_size.width, new_size.height);
    }
}

//------------------------------------------------------------------------------
// _term_maybe_present
//
// Present the framebuffer when either cursor state or framebuffer contents are
// dirty.
//------------------------------------------------------------------------------

internal void _term_maybe_present(void)
{
    if (g_cursor_dirty || _term_fb_has_dirty()) {
        _term_fb_present_now();
        g_cursor_dirty           = false;
    }
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_init
//
// Initialise the term module and enter terminal control mode.
//------------------------------------------------------------------------------

void term_init(void)
{
    if (g_term.initialised) {
        return;
    }

    setlocale(LC_CTYPE, "");

    // Initialise the terminal, ready for a loop
    g_term.size        = (TermSize){0};
    g_term.running     = true;
    g_term.initialised = true;

    arena_init(&g_term_arena, .reserved_size = MB(128), .grow_rate = 1);

    _term_start();
    _term_check_resize();
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_done
//
// Request shutdown of the term loop.
//------------------------------------------------------------------------------

void term_done(void) { g_term.running = false; }

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_cls
//
// Clear the real host terminal screen and scrollback.
//------------------------------------------------------------------------------

void term_cls(void) { pr("\x1b[2J\x1b[3J\x1b[H"); }

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_poll_event
//
// Pop the next queued terminal event, returning `TERM_EVENT_NONE` when no
// events are pending.
//------------------------------------------------------------------------------

TermEvent term_poll_event(void)
{
    TermEvent event;
    if (array_count(g_term.event_queue) > 0) {
        event = g_term.event_queue[0];
        array_delete(g_term.event_queue, 0);
        if (event.kind == TERM_EVENT_KEY) {
            g_term.key_modifiers = event.key_modifiers;
        }
    } else {
        event.kind = TERM_EVENT_NONE;
    }
    return event;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_key_modifiers
//
// Return the modifier bits attached to the most recently polled key event.
//------------------------------------------------------------------------------

u8 term_key_modifiers(void) { return g_term.key_modifiers; }

//------------------------------------------------------------------------------
// term_key_modifier_pressed
//
// Test whether a modifier bit was present on the most recently polled key
// event.
//------------------------------------------------------------------------------

bool term_key_modifier_pressed(u8 modifier)
{
    return (g_term.key_modifiers & modifier) != 0;
}

//------------------------------------------------------------------------------
// term_key_ctrl_pressed
//
// Test whether Ctrl was present on the most recently polled key event.
//------------------------------------------------------------------------------

bool term_key_ctrl_pressed(void)
{
    return term_key_modifier_pressed(TERM_KEYMOD_CTRL);
}

//------------------------------------------------------------------------------
// term_key_alt_pressed
//
// Test whether Alt was present on the most recently polled key event.
//------------------------------------------------------------------------------

bool term_key_alt_pressed(void)
{
    return term_key_modifier_pressed(TERM_KEYMOD_ALT);
}

//------------------------------------------------------------------------------
// term_key_shift_pressed
//
// Test whether Shift was present on the most recently polled key event.
//------------------------------------------------------------------------------

bool term_key_shift_pressed(void)
{
    return term_key_modifier_pressed(TERM_KEYMOD_SHIFT);
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_cursor_show
//
// Show the host terminal cursor and mark cursor state dirty.
//------------------------------------------------------------------------------

void term_cursor_show(void)
{
    pr("\x1b[?25h");
    g_cursor_visible = true;
    g_cursor_dirty   = true;
}

//------------------------------------------------------------------------------
// term_cursor_hide
//
// Hide the host terminal cursor and mark cursor state dirty.
//------------------------------------------------------------------------------

void term_cursor_hide(void)
{
    pr("\x1b[?25l");
    g_cursor_visible = false;
    g_cursor_dirty   = true;
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// term_cursor_goto
//
// Move the host terminal cursor to an absolute cell position.
//------------------------------------------------------------------------------

void term_cursor_goto(int x, int y)
{
    if (y < 0 || x < 0) {
        TermSize size = term_size_get();
        if (y < 0) {
            y = size.height + y;
        }
        if (x < 0) {
            x = size.width + x;
        }
    }
    g_cursor_x = x;
    g_cursor_y = y;
    g_cursor_dirty = true;
    ++x;
    ++y;
    pr("\x1b[%d;%dH", y, x);
}

//------------------------------------------------------------------------------
// term_cursor_move
//
// Move the host terminal cursor by a relative delta.
//------------------------------------------------------------------------------

void term_cursor_move(int dx, int dy)
{
    if (dy > 0) {
        term_cursor_down(dy);
    } else if (dy < 0) {
        term_cursor_up(-dy);
    }

    if (dx > 0) {
        term_cursor_right(dx);
    } else if (dx < 0) {
        term_cursor_left(-dx);
    }
}

//------------------------------------------------------------------------------
// term_cursor_home
//
// Move the host terminal cursor to the home position.
//------------------------------------------------------------------------------

void term_cursor_home(void) { pr("\x1b[H"); }

//------------------------------------------------------------------------------
// term_cursor_up
//
// Move the host terminal cursor up by the given number of rows.
//------------------------------------------------------------------------------

void term_cursor_up(int delta)
{
    if (delta < 0) {
        term_cursor_down(-delta);
    } else {
        pr("\x1b[%dA", delta);
    }
}

//------------------------------------------------------------------------------
// term_cursor_down
//
// Move the host terminal cursor down by the given number of rows.
//------------------------------------------------------------------------------

void term_cursor_down(int delta)
{
    if (delta < 0) {
        term_cursor_up(-delta);
    } else {
        pr("\x1b[%dB", delta);
    }
}

//------------------------------------------------------------------------------
// term_cursor_right
//
// Move the host terminal cursor right by the given number of columns.
//------------------------------------------------------------------------------

void term_cursor_right(int delta)
{
    if (delta < 0) {
        term_cursor_left(-delta);
    } else {
        pr("\x1b[%dC", delta);
    }
}

//------------------------------------------------------------------------------
// term_cursor_left
//
// Move the host terminal cursor left by the given number of columns.
//------------------------------------------------------------------------------

void term_cursor_left(int delta)
{
    if (delta < 0) {
        term_cursor_right(-delta);
    } else {
        pr("\x1b[%dD", delta);
    }
}

//------------------------------------------------------------------------------
// term_cursor_colour
//
// Set the host terminal cursor colours and mark cursor state dirty.
//------------------------------------------------------------------------------

void term_cursor_colour(u32 ink, u32 paper)
{
    g_cursor_ink   = ink;
    g_cursor_paper = paper;
    g_cursor_dirty = true;
    u8 ink_r   = (ink >> 16) & 0xFF;
    u8 ink_g   = (ink >> 8) & 0xFF;
    u8 ink_b   = (ink >> 0) & 0xFF;
    u8 paper_r = (paper >> 16) & 0xFF;
    u8 paper_g = (paper >> 8) & 0xFF;
    u8 paper_b = (paper >> 0) & 0xFF;
    pr("\x1b[38;2;%d;%d;%dm", ink_r, ink_g, ink_b);
    pr("\x1b[48;2;%d;%d;%dm", paper_r, paper_g, paper_b);
}
