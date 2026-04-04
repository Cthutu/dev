# Build Directives

This project uses a small directive system to declare module dependencies and
compile-time defines.

## Source file directives

Add directives as line comments:

```c
//> use: greet core
//> def: FEATURE_X GREETING=42
//> lib: X11
```

- `use`: list module names (directories under `src/`)
- `def`: list preprocessor symbols (optionally `NAME=VALUE`)
- `lib`: list linker libraries (emits `-l<name>` during the link step)

Unknown directives are errors.

## Module `.build` files

Each module directory may include a `.build` file with directive lines:

```
use: util net
def: LOGGING LOG_LEVEL=2
lib: X11
```

Blank lines are ignored. Non-empty lines must match `command: params`.

## Root `.build`

`src/.build` applies to all projects. Touching it forces a full rebuild.
