# Build System

## Overview

The project uses:

- `just` as the user-facing command runner
- Python scripts in `build/` as the actual build orchestration layer
- `clang` as the C compiler by default

In normal use, you do not call the Python scripts directly. Use `just`.

## Main Commands

- `just build <project>`
  Builds one or more projects in debug mode.
- `just build`
  Builds all top-level projects found in `src/`.
- `just build-release <project>`
  Builds one or more projects in release mode.
- `just run <project>`
  Builds the project in debug mode if needed, then runs `_bin/<project>-debug`.
- `just run-release <project>`
  Builds the project in release mode if needed, then runs `_bin/<project>`.
- `just format`
  Runs `clang-format` over `.c` and `.h` files under `src/` and `tests/`.
- `just clean`
  Removes `_bin`, `_obj`, and the Python virtual environment under `build/.venv`.

## Build Profiles

There are two build profiles:

- debug
  Uses `-g -O0 -DDEBUG`
- release
  Uses `-O2 -DNDEBUG`

Outputs are separated by profile:

- executables:
  - debug: `_bin/<name>-debug`
  - release: `_bin/<name>`
- object files:
  - debug: `_obj/debug/...`
  - release: `_obj/release/...`

## What Counts As A Project

A buildable project is a top-level `.c` file directly under `src/`.

Examples:

- `src/demo-client.c`
- `src/demo-server.c`

Running `just build demo-client` builds `src/demo-client.c` as the root of that
project.

## Modules And Sections

Projects pull in module code via source annotations in the root source file.

Example:

```c
//> use: core nexus
```

This tells the build system to include sources from:

- `src/core/`
- `src/nexus/`

The build scripts expand these sections, collect the required `.c` files, and
compile them into the final executable.

## Source Annotations

The build system reads metadata from comments in source files.

Common forms:

- `//> use: core nexus`
  Declares module/section dependencies.
- `//> def: NAME=value`
  Adds preprocessor definitions for that source/module.

These are parsed by the build tooling and folded into compile and link steps.

## Incremental Build Behaviour

The build is incremental.

For each source file, the build scripts consider:

- the source file itself
- discovered header dependencies
- relevant build-script dependencies

If nothing affecting an object file has changed, that source is skipped. The
link step is also skipped when the executable is already up to date.

## Tooling Layout

Important files:

- [Justfile](/home/matt/dev/Justfile:1)
  User-facing commands.
- [build/build.py](/home/matt/dev/build/build.py:1)
  Project build orchestration.
- [build/common.py](/home/matt/dev/build/common.py:1)
  Shared build logic such as dependency parsing, compile/link helpers, and
  progress reporting.
- [build/format.py](/home/matt/dev/build/format.py:1)
  Formatting entry point for `just format`.

## Environment

`just` bootstraps a Python environment via:

- `just python-env`

That creates and uses `build/.venv` for the build scripts and supporting Python
packages.

The compiler defaults to `clang`, but can be overridden with `CC`.

Example:

```bash
CC=gcc just build demo-client
```

## Typical Workflow

1. Build a project:

```bash
just build demo-server
```

2. Run it:

```bash
just run demo-server
```

3. Format code:

```bash
just format
```

4. Clean generated outputs if needed:

```bash
just clean
```
