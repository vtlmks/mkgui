#!/bin/bash

CC=gcc
WINCC=x86_64-w64-mingw32-gcc
WINAR=x86_64-w64-mingw32-ar

COMMON_CFLAGS="-std=c99 "
COMMON_CFLAGS+="-O2 "
# COMMON_CFLAGS+="-fno-inline -Wframe-larger-than=100000 "
COMMON_CFLAGS+="-march=x86-64-v2 "
COMMON_CFLAGS+="-fwrapv "
COMMON_CFLAGS+="-Wall "
COMMON_CFLAGS+="-Wextra "
COMMON_CFLAGS+="-Wno-missing-field-initializers "
COMMON_CFLAGS+="-Wno-unused-function "
COMMON_CFLAGS+="-Wno-unused-parameter "
COMMON_CFLAGS+="-Wformat-truncation "
COMMON_CFLAGS+="-Wno-stringop-truncation "
COMMON_CFLAGS+="-Wsign-conversion "

# Baked into libmkgui.a so consumers get the same icon capacity the
# unity-built tools used to get via their own #define.
LIB_DEFINES="-DMKGUI_LIBRARY -DMKGUI_MAX_ICONS=32768"

LINUX_LIBS="$(pkg-config --cflags freetype2 fontconfig) -lX11 -lXext $(pkg-config --libs freetype2 fontconfig) -lm "
LINUX_DEMO_LIBS="-lGL "
WINDOWS_LIBS="-lgdi32 -mwindows "
WINDOWS_DEMO_LIBS="-lopengl32 "

# Each argument is either a build mode or a build target. Defaults: normal mode,
# core target (library + demo + tests + extract_icons; no editor).
MODE="normal"
TARGET="core"

for arg in "$@"; do
	case "$arg" in
		normal|release|debug|size|asan)
			MODE="$arg"
			;;
		editor|all)
			TARGET="$arg"
			;;
		clean)
			rm -rf out
			exit 0
			;;
		*)
			echo "Unknown argument: $arg"
			echo "Usage: $0 [mode] [target]"
			echo "  modes:   normal (default) | release | debug | size | asan | clean"
			echo "  targets: core (default)   | editor | all"
			exit 1
			;;
	esac
done

MODE_FLAGS=""
SKIP_WINDOWS=""
case "$MODE" in
	"normal")
		MODE_FLAGS="-ggdb -fno-omit-frame-pointer "
		;;
	"release")
		MODE_FLAGS="-s -Wl,--strip-all "
		;;
	"debug")
		MODE_FLAGS="-g -O0 "
		;;
	"size")
		MODE_FLAGS="-Os -s -Wl,--strip-all "
		;;
	"asan")
		MODE_FLAGS="-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer "
		SKIP_WINDOWS=1
		;;
esac

CFLAGS="$COMMON_CFLAGS $MODE_FLAGS"

set -e
mkdir -p out/linux out/windows

WIN_ENABLED=0
if [ -z "$SKIP_WINDOWS" ] && command -v $WINCC &>/dev/null; then
	WIN_ENABLED=1
fi

# -----------------------------------------------------------------------------
# Phase 1: static libraries (sequential - this is the heavy compile).
# Building once and linking everyone else against it avoids saturating a
# low-core laptop with many simultaneous mkgui.c compilations. Both static
# libraries share the canonical name libmkgui.a; platform is encoded in the
# parent directory (out/linux/, out/windows/).
# Editor is special (unity build, uses internals) and does not depend on the
# library, so we skip this phase when target=editor.
# -----------------------------------------------------------------------------

if [ "$TARGET" != "editor" ]; then
	echo "[lib] out/linux/libmkgui.a"
	$CC $CFLAGS $LIB_DEFINES -c mkgui.c -o out/linux/mkgui.o $LINUX_LIBS
	ar rcs out/linux/libmkgui.a out/linux/mkgui.o
	rm -f out/linux/mkgui.o

	if [ "$WIN_ENABLED" -eq 1 ]; then
		echo "[lib] out/windows/libmkgui.a"
		$WINCC $CFLAGS $LIB_DEFINES -c mkgui.c -o out/windows/mkgui.o $WINDOWS_LIBS
		$WINAR rcs out/windows/libmkgui.a out/windows/mkgui.o
		rm -f out/windows/mkgui.o
	fi
fi

# -----------------------------------------------------------------------------
# Phase 2: consumers (parallel). Demo and tests are tiny; editor is the only
# heavy compile left and only builds when explicitly asked for.
# -----------------------------------------------------------------------------

if [ "$TARGET" != "editor" ]; then
	# Demo: small app, links libmkgui.a
	(
		$CC $CFLAGS demo.c out/linux/libmkgui.a -o out/linux/demo $LINUX_LIBS $LINUX_DEMO_LIBS
	) &

	if [ "$WIN_ENABLED" -eq 1 ]; then
		(
			$WINCC $CFLAGS demo.c out/windows/libmkgui.a -o out/windows/demo.exe $WINDOWS_LIBS $WINDOWS_DEMO_LIBS
		) &
	fi

	# Tests: unity builds (they poke mkgui internals on purpose). Kept unity
	# rather than polluting the public library with test-only exports.
	(
		$CC $CFLAGS tests/test_layout.c -o out/linux/test_layout $LINUX_LIBS
	) &

	(
		$CC $CFLAGS tests/test_widgets.c -o out/linux/test_widgets $LINUX_LIBS
	) &

	(
		$CC $CFLAGS tests/test_events.c -o out/linux/test_events $LINUX_LIBS
	) &

	(
		$CC $CFLAGS tests/test_events_ext.c -o out/linux/test_events_ext $LINUX_LIBS
	) &

	(
		$CC $CFLAGS tests/test_smoke.c -o out/linux/test_smoke $LINUX_LIBS
	) &

	(
		$CC $CFLAGS tests/test_window_visibility.c -o out/linux/test_window_visibility $LINUX_LIBS
	) &

	# extract_icons is independent (doesn't use mkgui)
	(
		$CC -std=gnu99 -O2 -Wall -Wextra -Wno-stringop-truncation -Wno-format-truncation tools/extract_icons.c -o out/linux/extract_icons
	) &
fi

if [ "$TARGET" = "editor" ] || [ "$TARGET" = "all" ]; then
	# Editor: unity build (shares mkgui's rendering/layout internals)
	(
		$CC $CFLAGS editor.c -o out/linux/editor $LINUX_LIBS
	) &

	if [ "$WIN_ENABLED" -eq 1 ]; then
		(
			$WINCC $CFLAGS editor.c -o out/windows/editor.exe $WINDOWS_LIBS
		) &
	fi
fi

# Collect any parallel-job failure. `set -e` does not propagate from &
# subshells, so walk the job table explicitly.
FAIL=0
for job in $(jobs -p); do
	if ! wait "$job"; then
		FAIL=1
	fi
done
exit $FAIL
