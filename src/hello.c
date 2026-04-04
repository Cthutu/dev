//> use: gfx

#include <gfx/gfx.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    WindowSystem ws;
    ws_init(&ws);

    Window main_win = (Window){
        .handle = WINDOW_HANDLE_NEW,
        .system = &ws,
        .title  = string_from_cstr("Hello, World!"),
        .width  = 800,
        .height = 600,
    };
    ws_apply(&main_win);

    WindowEvent event;
    while (ws_loop(&ws, &event)) {
        switch (event.kind) {
        case WE_KEYDOWN:
            if (event.keycode == KEY_ESCAPE) {
                ws_done(&main_win);
            }
            break;

        default:
            break;
        }
    }

    return 0;
}
