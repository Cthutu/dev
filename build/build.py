from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

from common import (
    BuildProgressTracker,
    CommandFailure,
    GREEN,
    GREY,
    RED,
    available_top_level_c_files,
    banner,
    colour,
    compile_source,
    config_deps_for_sections,
    executable_needs_relink,
    expand_sections,
    headers_for_source,
    libs_for_sections,
    link_executable,
    needs_rebuild,
    obj_path,
    parse_sections_and_defines,
    print_command_failure,
    prefix,
    section_sources,
    select_cflags,
    source_defines_for_dir,
    unique,
)

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "src"
BIN_DIR = ROOT / "_bin"
OBJ_BASE = ROOT / "_obj"
SCRIPT_PATH = Path(__file__).resolve()
COMMON_PATH = SCRIPT_PATH.parent / "common.py"

CC = os.environ.get("CC", "clang")
INCLUDE_FLAGS = ["-Isrc"]
LDFLAGS: list[str] = []


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build nerd projects.")
    parser.add_argument("projects", nargs="*", help="Project names (omit to build all)")
    parser.add_argument(
        "-r", "--release", action="store_true", help="Build release profile"
    )
    return parser.parse_args(argv[1:])


def available_projects() -> list[str]:
    return available_top_level_c_files(SRC_DIR)


def sources_for_project(project: str) -> tuple[list[Path], list[str], list[str], list[Path]]:
    root_src = SRC_DIR / f"{project}.c"
    if not root_src.exists():
        raise SystemExit(
            colour(f"Unknown project '{project}' (missing {root_src})", RED)
        )

    sections, defines, libs = parse_sections_and_defines(root_src, SRC_DIR)
    sections = expand_sections(sections, SRC_DIR)
    sources: list[Path] = [root_src]
    for section in sections:
        sources.extend(section_sources(section, SRC_DIR))
    ordered_libs: list[str] = []
    for lib in [*libs_for_sections(sections, SRC_DIR), *libs]:
        if lib not in ordered_libs:
            ordered_libs.append(lib)
    link_deps = [root_src, *config_deps_for_sections(sections, SRC_DIR)]
    return unique(sources), defines, ordered_libs, unique(link_deps)


def executable_path(project: str, profile: str) -> Path:
    suffix = "-debug" if profile == "debug" else ""
    extension = ".exe" if os.name == "nt" else ""
    return BIN_DIR / f"{project}{suffix}{extension}"


def main(argv: list[str] | None = None) -> None:
    argv = argv or sys.argv
    args = parse_args(argv)

    profile = "release" if args.release else "debug"
    cflags = select_cflags(profile)
    obj_dir = OBJ_BASE / profile

    projects = args.projects or available_projects()
    if not projects:
        raise SystemExit(colour("No projects found in src/", RED))

    project_sources: dict[str, list[Path]] = {}
    project_defines: dict[str, list[str]] = {}
    project_libs: dict[str, list[str]] = {}
    project_link_deps: dict[str, list[Path]] = {}
    for name in projects:
        sources, defines, libs, link_deps = sources_for_project(name)
        project_sources[name] = sources
        project_defines[name] = defines
        project_libs[name] = libs
        project_link_deps[name] = link_deps

    all_sources = unique(src for sources in project_sources.values() for src in sources)
    banner(profile, projects, "projects", CC)
    if not all_sources:
        raise SystemExit(colour("No C sources found in src/", RED))

    extra_flags_by_source: dict[Path, list[str]] = {}
    header_deps_by_source: dict[Path, list[Path]] = {}
    for src in all_sources:
        defines = source_defines_for_dir(src.parent, SRC_DIR)
        extra_flags_by_source[src] = [f"-D{define}" for define in defines]
        header_deps_by_source[src] = headers_for_source(src, SRC_DIR, SRC_DIR)

    for project in projects:
        root_src = SRC_DIR / f"{project}.c"
        defines = project_defines.get(project, [])
        if defines:
            extra_flags_by_source[root_src] = [
                *extra_flags_by_source.get(root_src, []),
                *[f"-D{define}" for define in defines],
            ]

    compiled: dict[Path, Path] = {}
    skipped_sources = 0
    compile_work: dict[Path, bool] = {}
    for src in all_sources:
        obj = obj_path(src, obj_dir, SRC_DIR)
        compile_work[src] = needs_rebuild(
            src,
            obj,
            header_deps=header_deps_by_source.get(src, []),
            extra_deps=[SCRIPT_PATH, COMMON_PATH],
            local_build_root=SRC_DIR,
        )
        compiled[src] = obj
        if not compile_work[src]:
            skipped_sources += 1

    link_work: dict[str, bool] = {}
    for project, sources in project_sources.items():
        objects = [compiled[src] for src in sources]
        link_work[project] = executable_needs_relink(
            executable_path(project, profile),
            objects,
            extra_deps=[SCRIPT_PATH, COMMON_PATH, *project_link_deps[project]],
        )
        if any(compile_work.get(src, False) for src in sources):
            link_work[project] = True

    module_phase_sources: dict[str, list[Path]] = {}
    module_phase_order: list[str] = []
    for project in projects:
        root_src = SRC_DIR / f"{project}.c"
        for src in project_sources[project]:
            if src == root_src:
                continue
            module_name = src.relative_to(SRC_DIR).parts[0]
            if module_name not in module_phase_sources:
                module_phase_sources[module_name] = []
                module_phase_order.append(module_name)
            if src not in module_phase_sources[module_name]:
                module_phase_sources[module_name].append(src)

    phases: list[tuple[str, str, list[tuple[str, Path]], bool]] = []
    for module_name in module_phase_order:
        steps = [("compile", src) for src in module_phase_sources[module_name]]
        had_work = any(compile_work.get(src, False) for src in module_phase_sources[module_name])
        phases.append((module_name, "module", steps, had_work))

    for project in projects:
        root_src = SRC_DIR / f"{project}.c"
        steps = [("compile", root_src), ("link", executable_path(project, profile))]
        had_work = compile_work.get(root_src, False) or link_work.get(project, False)
        phases.append((project, "project", steps, had_work))

    with BuildProgressTracker(len(phases), noun="Build Phases") as tracker:
        for phase_name, phase_kind, steps, had_work in phases:
            tracker.start_target(phase_name, len(steps), kind=phase_kind)
            for kind, path in steps:
                if kind == "compile":
                    tracker.step(path.name)
                    if compile_work.get(path, False):
                        obj, _ = compile_source(
                            cc=CC,
                            cflags=cflags,
                            include_flags=INCLUDE_FLAGS,
                            obj_dir=obj_dir,
                            src=path,
                            relative_to=SRC_DIR,
                            display_root=SRC_DIR,
                            extra_flags=extra_flags_by_source.get(path, []),
                            header_deps=header_deps_by_source.get(path, []),
                            extra_deps=[SCRIPT_PATH, COMMON_PATH],
                            local_build_root=SRC_DIR,
                            announce=False,
                        )
                        compiled[path] = obj
                else:
                    tracker.step(path.name)
                    if link_work.get(phase_name, False):
                        objects = [compiled[src] for src in project_sources[phase_name]]
                        link_executable(
                            cc=CC,
                            ldflags=[*LDFLAGS, *[f"-l{lib}" for lib in project_libs[phase_name]]],
                            bin_dir=BIN_DIR,
                            root=ROOT,
                            objects=objects,
                            executable=path,
                            extra_deps=[SCRIPT_PATH, COMMON_PATH, *project_link_deps[phase_name]],
                            announce=False,
                        )
                tracker.advance_step()
            tracker.finish_target(phase_name, had_work=had_work)

    print(f"{prefix('skip', GREY)} {skipped_sources} source file(s) up to date")
    finish_bar = colour("=" * 48, GREEN)
    print(finish_bar)
    print(colour(">> Build complete. Go be nerdy! \\o/ <<", GREEN))
    print(finish_bar)


if __name__ == "__main__":
    try:
        main()
    except CommandFailure as error:
        print_command_failure(error)
        raise SystemExit(error.returncode)
