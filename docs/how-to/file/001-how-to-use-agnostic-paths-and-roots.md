# How to use agnostic paths and roots

This guide explains the path model used by the `file` module.

## Goal

Work with portable paths like `home:/notes.txt` and `data:/image.png` instead of
hard-coding host-specific paths.

## Steps

1. Call `file_init()` once before using the path API.
2. Use agnostic paths in the form `root:/path/to/file`.
3. Use the built-in roots such as `home:/`, `data:/`, `temp:/`, `cfg:/`, `app:/`,
   and `sys:/`.
4. Pass those agnostic paths to the file/path helpers.
5. Call `file_done()` during shutdown.

## Example

```c
//> use: file

#include <file/file.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    file_init();

    string config = S("cfg:/settings.json");
    string asset  = S("data:/splash.txt");
    string cache  = S("temp:/demo/cache.bin");

    prn("config path valid: %s", path_is_valid(config) ? "yes" : "no");
    prn("asset path valid: %s", path_is_valid(asset) ? "yes" : "no");
    prn("cache path valid: %s", path_is_valid(cache) ? "yes" : "no");

    file_done();
    return 0;
}
```

## Notes

- `sys:/` is always available. It is the canonical path space used internally.
- Other roots are shortcuts layered on top of `sys:/` or other agnostic roots.
- Paths must be absolute and must not contain `.` or `..` components.
- Root paths stored by the file system remain agnostic, not platform-specific.

## Related files

- [file.h](/home/matt/dev/src/file/file.h)
- [demo-path.c](/home/matt/dev/src/demo-path.c)
