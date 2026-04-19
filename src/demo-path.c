//------------------------------------------------------------------------------
// Path Demo
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: file

#include <file/file.h>
#include <file/internal.h>

//------------------------------------------------------------------------------

void output_root(FileRoot root)
{
    prn(ANSI_BOLD_CYAN STRINGP ANSI_RESET ANSI_FAINT ": " ANSI_RESET
        ANSI_GREEN STRINGP ANSI_RESET,
        STRINGV(root.name),
        STRINGV(root.path));
}

void output_roots(void)
{
    prn(ANSI_BOLD_YELLOW "Registered Roots" ANSI_RESET);
    for (usize i = 0; i < array_count(g_file_system.roots); ++i) {
        output_root(g_file_system.roots[i]);
    }
}

void output(cstr str)
{
    string platform = path_to_platform(S(str), temp_arena());
    if (platform.count == 0) {
        prn(ANSI_BOLD_BLUE "%s" ANSI_RESET ANSI_FAINT ": " ANSI_RESET
            ANSI_BOLD_RED "<unresolved>" ANSI_RESET,
            str);
        return;
    }

    prn(ANSI_BOLD_BLUE "%s" ANSI_RESET ANSI_FAINT ": " ANSI_RESET
        ANSI_WHITE STRINGP ANSI_RESET,
        str,
        STRINGV(platform));
}

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    file_init();
    output_roots();
    prn("");

    prn(ANSI_BOLD_YELLOW "Sample Paths" ANSI_RESET);
    output("sys:/etc");
    output("home:/");
    output("data:/");
    output("temp:/");
    output("cfg:/");
    output("app:/");

    file_done();
    return 0;
}
