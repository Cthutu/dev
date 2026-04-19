# How to add a custom root

This guide shows how to create your own root on top of the built-in roots.

## Goal

Register a shortcut like `logs:/` or `cache:/` so the rest of your code can use
portable paths instead of repeating a longer base path.

## Steps

1. Call `file_init()`.
2. Register any built-in roots your custom root depends on.
3. Pick a unique root name.
4. Build the root target as an agnostic path.
5. Call `path_add_root(name, path)`.
6. Use the new root everywhere else as a normal agnostic path.

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

    if (!path_add_root(S("logs"), S("home:/dev/logs"))) {
        prn("failed to add logs root");
        file_done();
        return 1;
    }

    string log_path = path_join(S("logs:/"), S("latest.txt"), temp_arena());
    string host     = path_to_platform(log_path, temp_arena());

    prn("log path: " STRINGP, STRINGV(log_path));
    prn("host path: " STRINGP, STRINGV(host));

    file_done();
    return 0;
}
```

## Notes

- The root target should be an agnostic path, not a platform path.
- Root paths are copied into the file system arena when they are stored.
- If the target directory does not exist, `path_add_root()` creates it.
- A custom root can be layered on another root such as `home:/` or `data:/`.

## Related files

- [file.h](/home/matt/dev/src/file/file.h)
- [path.c](/home/matt/dev/src/file/path.c)
