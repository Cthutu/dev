# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

The user-facing command runner is `just`. All commands bootstrap a Python venv under `build/.venv` via `uv`.

- `just build [project...]` — debug build. No args builds every project.
- `just build-release [project...]` — release build.
- `just run [project]` — list projects (with `//> desc:` text), or build+run `_bin/<project>-debug`.
- `just run-release <project>` — run the release binary `_bin/<project>`.
- `just test [module...]` — build + run test modules in `tests/` (debug).
- `just test -t <filter>` — run only matching `category` or `category:name`. Example: `just test nexus -t nexus:tcp_message_framing_round_trip`.
- `just test-release` — test in release mode.
- `just format` — `clang-format` over `src/` and `tests/`.
- `just clean` — wipes `_bin`, `_obj`, `build/.venv`.
- Override the compiler with `CC=gcc just build ...`. Default is `clang`. The project requires `-std=c23`.

## Architecture

### Projects and modules

A **project** is a top-level `.c` file directly in `src/` (e.g. `src/demo-client.c`, `src/hello.c`). A **module** is a subdirectory under `src/` (e.g. `src/core/`, `src/nexus/`, `src/term/`) containing `.c` files and typically a header named after the module.

Projects pull in modules by declaring dependencies as source-file comment directives — there is no CMake/Make file listing them. The build scripts (`build/build.py`, `build/common.py`) parse these directives, expand the module graph, and compile everything into one executable per project.

### Source annotations (directives)

Lines starting with `//>` in `.c`/`.h` files are build directives. Module `.build` files use the same `command: params` syntax without the `//>` prefix. `src/.build` applies to all projects and forces a full rebuild when touched. Unknown directives are errors.

- `//> use: <modules...>` — include sources from `src/<module>/`. Dependencies are transitive.
- `//> def: NAME` / `//> def: NAME=value` — preprocessor defines.
- `//> lib: <name>` — emit `-l<name>` at link time (e.g. `X11` for `src/gfx/`).
- `//> desc: <text>` — project description shown by `just run` when listing.

### Build outputs

- Executables: `_bin/<name>-debug` (debug) or `_bin/<name>` (release).
- Objects: `_obj/debug/...` or `_obj/release/...`.
- Debug: `-g -O0 -DDEBUG`. Release: `-O2 -DNDEBUG`.
- Builds are incremental based on source + header + build-script dependencies.

### The `core` module

Nearly every project depends on `core` (`src/core/core.h`). It is the foundation layer and defines:

- **Platform detection macros** using `YES`/`NO` rather than `#ifdef`: `OS_WINDOWS`, `OS_LINUX`, `OS_MACOS`, `OS_POSIX`, `COMPILER_CLANG`, `COMPILER_GCC`, `COMPILER_MSVC`, `ARCH_X86_64`, `CONFIG_DEBUG`, `CONFIG_RELEASE`. Use `#if OS_WINDOWS` (not `#ifdef`).
- **Short fixed-width types**: `u8`/`u16`/`u32`/`u64`, `i8`/…/`i64`, `usize`, `isize`, `f32`, `f64`, `cstr`. Prefer these over raw `uint32_t` etc.
- **Tracked allocators**: `ALLOC`/`REALLOC`/`FREE`, `ARRAY_ALLOC` — these capture `__FILE__`/`__LINE__` and (in debug) report leaks on exit via `mem_print_leaks()`.
- **`Array(T)`** — a stb-style header-prefixed dynamic array. Use `array_push`, `array_count`, `array_free`, `array_reserve`, `array_needs`, `array_delete_quick`, etc. Macros rely on `typeof(*(a))` (C23).
- **`Arena`** — OS-reserved paged arena allocator with `arena_init`/`arena_alloc`/`arena_store`+`arena_restore` marks, plus a global temp arena accessed with `temp_arena()` and reset per-frame.
- **`string`** — a `{u8* data; usize count}` slice (not NUL-terminated). Use `S("lit")` literal macro, `STRINGP`/`STRINGV(s)` to format via `printf`-style (`"%.*s"`). `StringBuilder` writes into an arena.
- **`Mutex`** — thin wrapper over `CRITICAL_SECTION` or `pthread_mutex_t`.
- **Output helpers**: `pr`/`prn` (stdout), `epr`/`eprn` (stderr), `ASSERT`, `VERIFY`, plus `ANSI_*` and `UNICODE_*` string constants.
- **`main`** is defined in `src/core/main.c`. Project code implements `int run(int argc, char** argv)`. Core's `main` initialises the temp arena and (on Windows) switches the console to UTF-8. Test binaries compile core with `-DTEST` so their own `main` is used instead.

### Other modules

- `src/nexus/` — message-oriented networking (TCP length-framed, UDP datagrams). Socket kinds: basic / request / reply / telnet. See `src/nexus/README.md` and `docs/how-to/nexus/` for API usage. Public header is `nexus/nexus.h`; internals in `internal.h` split into `transport_*`, `protocol_*`, `socket.c`, `pipe.c`, `message.c`, `url.c`.
- `src/term/` — terminal framebuffer + windowing API (`term.h`). `TermWindow` is incomplete; `term_fb_*` is the active double-buffered primitive path. Themes in `themes.c`.
- `src/thread/` — `Thread`, `CancelToken`, and a ring-buffer `Channel` (SPSC upgrading to MPSC on `channel_tx_clone`). One reserved byte: usable capacity is `buffer_size - 1`.
- `src/log/` — file-append logger keyed by path.
- `src/gfx/` — X11 windowing (`//> lib: X11`). Linux-only.

### Tests

The test framework lives in `build/test.h` (included as `<test.h>`). A test module is either a single file `tests/<name>.c` or a directory `tests/<name>/` of `.c` files compiled together. Each file uses `TEST_CASE(category, name) { TEST_ASSERT_EQ(...); }`. Module code pulls in modules with the same `//> use:` directives as normal projects. The `-t <filter>` arg matches either the category or `category:name`. Test binaries are built into `_bin/tests/...` incrementally.

### Conventions

- Format: 4-space indent, LF line endings, `PointerAlignment: Left`, brace on new line after functions, compound assignments and declarations aligned. `just format` before committing.
- Headers start with a banner comment block and `//>` directives. Public headers use `#pragma once`.
- Current branch is `windows` — ongoing Windows portability work on top of the Linux/POSIX implementation.
