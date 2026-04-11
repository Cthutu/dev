# Unit Test System

## Overview

The project has a custom C unit test runner driven by:

- `just`
- `build/test.py`
- the lightweight test framework declared in `build/test.h`

Use `just test` in normal workflows.

## Main Commands

- `just test`
  Builds and runs all test modules in debug mode.
- `just test <module>`
  Builds and runs one or more named test modules.
- `just test-release`
  Builds and runs tests in release mode.
- `just test -t <filter>`
  Runs only a matching test category or `category:name`.

Examples:

```bash
just test
just test core
just test nexus
just test -t nexus:tcp_message_framing_round_trip
just test core -t string
```

## Test Discovery

Tests are discovered from the `tests/` directory.

Two layouts are supported:

- single-file test module
  - example: `tests/simple.c`
- multi-file test module in a folder
  - example: `tests/core/*.c`
  - example: `tests/nexus/*.c`

The module name is:

- the filename stem for a top-level test file
- the directory name for a test folder

So these become modules:

- `tests/simple.c` -> `simple`
- `tests/core/` -> `core`
- `tests/nexus/` -> `nexus`

## Writing Tests

Tests use the macros from `build/test.h`.

Typical structure:

```c
#include <test.h>

TEST_CASE(simple, simple)
{
    TEST_ASSERT_EQ(1 + 1, 2);
}
```

For module code, include the required sections using the same source annotation
style as normal builds.

Example:

```c
//> use: core nexus thread
```

## Test Case Naming

`TEST_CASE(category, name)` defines one test.

- `category`
  Groups related tests.
- `name`
  Identifies the individual test.

These names are used by the test filter.

Example:

```bash
just test nexus -t nexus:zero_length_messages_are_valid
```

## Assertions

Common assertion macros include:

- `TEST_ASSERT(...)`
- `TEST_ASSERT_EQ(a, b)`
- `TEST_ASSERT_STR_EQ(a, b)`
- `TEST_ASSERT_NULL(ptr)`
- `TEST_ASSERT_NOT_NULL(ptr)`
- `TEST_ASSERT_GT(a, b)`
- `TEST_ASSERT_LT(a, b)`
- `TEST_ASSERT_GE(a, b)`
- `TEST_ASSERT_LE(a, b)`
- `TEST_ASSERT_MEM_EQ(p1, p2, size)`

When an assertion fails, the runner records:

- category
- test name
- file
- line
- expected expression/details

## How Tests Are Built

The test build flow mirrors the normal build flow:

- test sources are compiled into `_obj/tests/...`
- test executables are linked into `_bin/tests/...`
- section dependencies declared with `//> use:` are expanded automatically

The runner builds only what is out of date, so repeated `just test` runs are
incremental.

## Output And Reporting

The test runner reports:

- per-module pass/fail counts
- assertion totals
- failure details when assertions fail

Internally, test binaries emit structured events which `build/test.py` reads and
summarises into the final console output.

## Tooling Layout

Important files:

- [Justfile](/home/matt/dev/Justfile:1)
  User-facing test commands.
- [build/test.py](/home/matt/dev/build/test.py:1)
  Test discovery, build, execution, and reporting.
- [build/test.h](/home/matt/dev/build/test.h:1)
  Test macros and runtime interface used by test source files.

## Typical Workflow

1. Run everything:

```bash
just test
```

2. Run one module while working:

```bash
just test nexus
```

3. Run one test or category:

```bash
just test nexus -t nexus
just test nexus -t nexus:recv_buffer_too_small_can_drop_pending_message
```

4. Run the full suite in release mode if needed:

```bash
just test-release
```

## Current Convention

For this codebase:

- progress on a module should come with tests
- module-specific tests should generally live under `tests/<module>/`
- small standalone framework checks can live as single files directly under
  `tests/`
