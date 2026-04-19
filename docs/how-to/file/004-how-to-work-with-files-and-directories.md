# How to work with files and directories

This guide shows the basic file and directory checks available through the path API.

## Goal

Create directories, test for files, inspect metadata, and remove paths while
staying in agnostic-path space.

## Steps

1. Call `file_init()`.
2. Register the roots needed by the paths you plan to use.
3. Build the target path with `path_join()` or a literal agnostic path.
4. Use `path_exists()`, `path_is_directory()`, or `path_is_file()` to inspect it.
5. Use `path_create_directory()` to create directories.
6. Use `path_get_file_size()` or `path_get_last_modified_time()` for metadata.
7. Use `path_delete()` to remove a file or directory tree.

## Example

```c
//> use: file

#include <file/file.h>

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    file_init();
    file_add_temp_root();

    string dir  = S("temp:/demo-path");
    string file = path_join(dir, S("example.txt"), temp_arena());

    if (!path_exists(dir)) {
        path_create_directory(dir);
    }

    prn("dir exists: %s", path_exists(dir) ? "yes" : "no");
    prn("dir is directory: %s", path_is_directory(dir) ? "yes" : "no");
    prn("file exists: %s", path_exists(file) ? "yes" : "no");

    path_delete(dir);

    file_done();
    return 0;
}
```

## Notes

- These helpers accept agnostic paths and do the platform conversion internally.
- `file_init()` does not add `temp:/` or any other named root automatically.
- `path_create_directory()` creates parent directories as needed.
- `path_delete()` removes directories recursively.
- File metadata calls return `false` when the target does not exist or has the wrong type.

## Related files

- [file.h](/home/matt/dev/src/file/file.h)
- [posix.c](/home/matt/dev/src/file/posix.c)
