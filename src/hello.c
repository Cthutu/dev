//> use: gfx

#include <gfx/gfx.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    FrameSystem fs;
    fs_init(&fs);

    Frame main_frame =
        frame_new(&fs, string_from_cstr("Hello, World!"), 800, 600);
    fs_apply(&main_frame);

    FrameEvent event;
    while (fs_loop(&fs, &event)) {
        fs_update(&main_frame);
        switch (event.kind) {
        case FE_KEYDOWN:
            if (frame_event_is_key_pressed(&event, KEY_ESCAPE) ||
                frame_event_is_key_pressed(&event, KEY_Q)) {
                fs_done(&main_frame);
            }

            if (frame_event_is_alt_pressed(&event) &&
                frame_event_is_key_pressed(&event, KEY_ENTER)) {
                main_frame.fullscreen = !main_frame.fullscreen;
                fs_apply(&main_frame);
            }

            if (frame_event_is_key_pressed(&event, KEY_R)) {
                main_frame.resizable = !main_frame.resizable;
                fs_apply(&main_frame);
            }

            prn("Key down: %c (code: %d, modifiers: %d)",
                event.key_char,
                event.keycode,
                event.modifiers);
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
