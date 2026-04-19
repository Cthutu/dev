# How to use agnostic paths and roots

This guide explains the path model used by the `file` module.

## Goal

Work with portable paths like `home:/notes.txt` and `data:/image.png` instead of
hard-coding host-specific paths.

## Steps

1. Call `file_init()` once before using the path API.
2. Register only the roots your application needs with the `file_add_*_root()` helpers.
3. Use agnostic paths in the form `root:/path/to/file`.
4. Use the built-in roots such as `home:/`, `data:/`, `temp:/`, `cfg:/`, `app:/`,
   and `sys:/`.
5. Pass those agnostic paths to the file/path helpers.
6. Call `file_done()` during shutdown.

## Example

```c
//> use: file

#include <file/file.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    file_init();
    file_add_home_root();
    file_add_data_root();
    file_add_temp_root();
    file_add_cfg_root();
    file_add_app_root();

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

- `file_init()` only sets up the file system module itself. It does not add
  `home`, `data`, `temp`, `cfg`, or `app` automatically.
- Register only the roots your application wants to expose. For example, if you
  do not call `file_add_cfg_root()`, then `cfg:/` is not available and no
  configuration directory is created for that application.
- `sys:/` is always available. It is the canonical path space used internally.
- Other roots are shortcuts layered on top of `sys:/` or other agnostic roots.
- Paths must be absolute and must not contain `.` or `..` components.
- Root paths stored by the file system remain agnostic, not platform-specific.

## Related files

- [file.h](/home/matt/dev/src/file/file.h)
- [demo-path.c](/home/matt/dev/src/demo-path.c)
