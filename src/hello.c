//> use: gfx

#include <gfx/gfx.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    FrameSystem fs;
    fs_init(&fs);

    Frame main_frame = (Frame){
        .handle = FRAME_HANDLE_NEW,
        .system = &fs,
        .title  = string_from_cstr("Hello, World!"),
        .width  = 800,
        .height = 600,
    };
    fs_apply(&main_frame);

    FrameEvent event;
    while (fs_loop(&fs, &event)) {
        fs_update(&main_frame);
        switch (event.kind) {
        case FE_KEYDOWN:
            if (event.keycode == KEY_ESCAPE || event.keycode == KEY_Q) {
                fs_done(&main_frame);
            }
            if (event.keycode == KEY_F1) {
                main_frame.width += 100;
                fs_apply(&main_frame);
            }
            break;

        case FE_MOVE:
            prn("Window moved to %d, %d", event.x, event.y);
            break;

        case FE_RESIZE:
            prn("Window resized to %d x %d", event.width, event.height);
            break;

        case FE_CLOSE:
            prn("Window closed!");
            break;

        default:
            break;
        }
    }

    return 0;
}
