from __future__ import annotations

import argparse
import subprocess
import sys

import build as build_script
from common import (
    RICH_CONSOLE,
    CommandFailure,
    available_top_level_c_paths,
    parse_description,
    print_command_failure,
)
from rich import box
from rich.table import Table

SRC_DIR = build_script.SRC_DIR


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build and run a project, or list projects when omitted."
    )
    parser.add_argument("-r", "--release", action="store_true", help="Run release profile")
    parser.add_argument("project", nargs="?", help="Project name")
    parser.add_argument(
        "args", nargs=argparse.REMAINDER, help="Arguments passed to the executable"
    )
    return parser.parse_args(argv[1:])


def print_project_table() -> None:
    sources = available_top_level_c_paths(SRC_DIR)
    if not sources:
        raise SystemExit("No projects found in src/")

    table = Table(title="Projects", box=box.ROUNDED, header_style="bold cyan")
    table.add_column("Project", style="bold green", no_wrap=True)
    table.add_column("Description", overflow="fold")

    for src in sources:
        table.add_row(
            src.stem,
            parse_description(src) or src.relative_to(SRC_DIR).as_posix(),
        )

    RICH_CONSOLE.print(table)


def main(argv: list[str] | None = None) -> None:
    argv = argv or sys.argv
    args = parse_args(argv)
    if args.project is None:
        print_project_table()
        return

    build_argv = [str(build_script.SCRIPT_PATH)]
    if args.release:
        build_argv.append("--release")
    build_argv.append(args.project)
    try:
        build_script.main(build_argv)
    except CommandFailure as error:
        print_command_failure(error)
        raise SystemExit(error.returncode)

    profile = "release" if args.release else "debug"
    executable = build_script.executable_path(args.project, profile)
    result = subprocess.run([str(executable), *args.args], check=False)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
