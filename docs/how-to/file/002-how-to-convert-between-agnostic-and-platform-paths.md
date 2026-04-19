# How to convert between agnostic and platform paths

This guide shows how to cross the boundary between the file module's agnostic
paths and the host platform's native paths.

## Goal

Convert a host path like `/home/matt/dev/data/demo-path` into an agnostic path,
and convert an agnostic path like `data:/demo.txt` back into a host path.

## Steps

1. Call `file_init()`.
2. Register the roots you want to simplify against with the `file_add_*_root()` helpers.
3. Use `path_from_platform()` when a path comes from the OS or another library.
4. Use `path_to_platform()` when you need a host path for syscalls or external APIs.
5. Treat those two functions as the platform boundary.
6. Call `file_done()` when finished.

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

    string agnostic = path_from_platform(S("/home/matt/dev/data/demo-path"), temp_arena());
    string host     = path_to_platform(S("data:/demo.txt"), temp_arena());

    prn("agnostic: " STRINGP, STRINGV(agnostic));
    prn("host: " STRINGP, STRINGV(host));

    file_done();
    return 0;
}
```

## Notes

- The more roots you register, the more `path_from_platform()` can simplify.
- `file_init()` alone only enables the module. Named roots are available only
  after you add them explicitly.
- `path_from_platform()` produces a `sys:/...` path first, then simplifies it
  against known roots.
- `path_to_platform()` expands roots recursively until it reaches `sys:/...`,
  then converts that to the host path.
- Internally, the file module should stay in agnostic-path space whenever possible.

## Related files

- [file.h](/home/matt/dev/src/file/file.h)
- [posix.c](/home/matt/dev/src/file/posix.c)
