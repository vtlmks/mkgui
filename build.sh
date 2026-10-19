#!/bin/bash

CC=gcc
WINCC=x86_64-w64-mingw32-gcc
WINAR=x86_64-w64-mingw32-ar

COMMON_CFLAGS="-std=c99 "
COMMON_CFLAGS+="-O2 "
# COMMON_CFLAGS+="-fno-inline -Wframe-larger-than=100000 "
COMMON_CFLAGS+="-march=x86-64-v2 "
COMMON_CFLAGS+="-fwrapv "
# Pairs with --gc-sections (link) to drop dead leaf code. Not -fdata-sections: it
# bloats the mingw .exe by moving zero-init statics into file-backed .data.
COMMON_CFLAGS+="-ffunction-sections "
COMMON_CFLAGS+="-fno-asynchronous-unwind-tables "
COMMON_CFLAGS+="-Wall "
COMMON_CFLAGS+="-Wextra "
COMMON_CFLAGS+="-Wno-missing-field-initializers "
COMMON_CFLAGS+="-Wno-unused-function "
COMMON_CFLAGS+="-Wno-unused-parameter "
COMMON_CFLAGS+="-Wformat-truncation "
COMMON_CFLAGS+="-Wno-stringop-truncation "
COMMON_CFLAGS+="-Wsign-conversion "

# mkwin submodule, #included into the mkgui translation unit; just the include path.
COMMON_CFLAGS+="-Imkwin "

LIB_DEFINES="-DMKGUI_LIBRARY"

LINUX_LIBS="$(pkg-config --cflags freetype2 fontconfig) -lX11 -lXext $(pkg-config --libs freetype2 fontconfig) -lm "
LINUX_DEMO_LIBS="-lGL -lX11 "
WINDOWS_LIBS="-lgdi32 -mwindows "
WINDOWS_DEMO_LIBS="-lopengl32 "
# Static-link winpthread so the .exe doesn't depend on libwinpthread-1.dll.
WINDOWS_PTHREAD="-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic "

LDFLAGS="-Wl,--gc-sections "

# Args are build modes and/or targets. Default: normal mode, all targets. The
# editor rebuilds with everything by default; pass 'core' to skip it.
MODE="normal"
TARGET="all"

for arg in "$@"; do
	case "$arg" in
		normal|release|debug|size|asan)
			MODE="$arg"
			;;
		core|editor|icons|library|all)
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
			echo "  targets: all (default)    | core (skip editor) | editor (editor only) | icons | library"
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

ICON_RELEASE=$(cat mkguiicons.version)
python3 tools/generate_icon_library.py

echo "[icons] out/linux/libmkguiicons.so.$ICON_RELEASE"
$CC -std=c99 -O2 -Wall -Wextra -fPIC -fvisibility=hidden -DMKGUIICONS_BUILD -shared mkguiicons.c mkguiicons_data.S -Wl,-soname,libmkguiicons.so.1 -o out/linux/libmkguiicons.so.$ICON_RELEASE
ln -sf libmkguiicons.so.$ICON_RELEASE out/linux/libmkguiicons.so.1
ln -sf libmkguiicons.so.1 out/linux/libmkguiicons.so

if [ "$WIN_ENABLED" -eq 1 ]; then
	echo "[icons] out/windows/mkguiicons.dll"
	$WINCC -std=c99 -O2 -Wall -Wextra -DMKGUIICONS_BUILD -shared mkguiicons.c mkguiicons_data.S -Wl,--out-implib,out/windows/libmkguiicons.dll.a -static-libgcc -o out/windows/mkguiicons.dll
fi

if [ "$TARGET" = "icons" ]; then
	exit 0
fi

LINUX_LIBS+=" -Lout/linux -lmkguiicons -Wl,-rpath,\$ORIGIN "
WINDOWS_LIBS+=" -Lout/windows -lmkguiicons "

RELEASE=$(cat mkgui.version)

(
	echo "[lib] out/linux/libmkgui.so.$RELEASE and libmkgui.a"
	$CC $CFLAGS $LIB_DEFINES -fPIC -fvisibility=hidden -c mkgui.c -o out/linux/mkgui.o $LINUX_LIBS
	$CC $CFLAGS -shared out/linux/mkgui.o -Wl,-soname,libmkgui.so.$RELEASE -Wl,-z,defs -o out/linux/libmkgui.so.$RELEASE $LINUX_LIBS $LDFLAGS
	ln -sf libmkgui.so.$RELEASE out/linux/libmkgui.so
	ar rcs out/linux/libmkgui.a out/linux/mkgui.o
	rm -f out/linux/mkgui.o
) &

if [ "$WIN_ENABLED" -eq 1 ]; then
	(
		echo "[lib] out/windows/mkgui.dll and libmkgui.a"
		$WINCC $CFLAGS $LIB_DEFINES -fvisibility=hidden -shared mkgui.c -Wl,--out-implib,out/windows/libmkgui.dll.a -static-libgcc -o out/windows/mkgui.dll $WINDOWS_LIBS $WINDOWS_PTHREAD $LDFLAGS
		$WINCC $CFLAGS $LIB_DEFINES -DMKGUI_STATIC -c mkgui.c -o out/windows/mkgui.o $WINDOWS_LIBS
		$WINAR rcs out/windows/libmkgui.a out/windows/mkgui.o
		rm -f out/windows/mkgui.o
	) &
fi

LIB_FAIL=0
for job in $(jobs -p); do
	if ! wait "$job"; then
		LIB_FAIL=1
	fi
done
if [ "$LIB_FAIL" -ne 0 ]; then
	exit 1
fi
if [ "$TARGET" = "library" ]; then
	exit 0
fi

LINUX_SHARED="-Lout/linux -l:libmkgui.so.$RELEASE -Wl,-rpath,\$ORIGIN"
WINDOWS_SHARED="-Lout/windows -lmkgui"

# Applications use shared libraries; internal tests and the benchmark use unity builds.

if [ "$TARGET" != "editor" ]; then
	# Demo: public header and shared library.
	(
		$CC $CFLAGS demo.c -o out/linux/demo $LINUX_SHARED $LINUX_DEMO_LIBS $LDFLAGS
	) &

	if [ "$WIN_ENABLED" -eq 1 ]; then
		(
			$WINCC $CFLAGS demo.c -o out/windows/demo.exe $WINDOWS_SHARED -mwindows $WINDOWS_DEMO_LIBS $WINDOWS_PTHREAD $LDFLAGS
		) &
	fi

	(
		$CC $CFLAGS -I. tests/test_shared.c -o out/linux/test_shared $LINUX_SHARED $LDFLAGS
		$CC $CFLAGS tests/test_editor_shared.c -o out/linux/test_editor_shared $LINUX_SHARED $LDFLAGS
	) &

	if [ "$WIN_ENABLED" -eq 1 ]; then
		(
			$WINCC $CFLAGS -I. tests/test_shared.c -o out/windows/test_shared.exe $WINDOWS_SHARED -mconsole $LDFLAGS
			$WINCC $CFLAGS tests/test_editor_shared.c -o out/windows/test_editor_shared.exe $WINDOWS_SHARED -mconsole $LDFLAGS
		) &
	fi

	# Remaining tests are unity builds; they use mkgui internals on purpose.
	(
		$CC $CFLAGS tests/test_filedialog.c -o out/linux/test_filedialog $LINUX_LIBS $LDFLAGS
		$CC $CFLAGS tests/test_undo.c -o out/linux/test_undo $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_layout.c -o out/linux/test_layout $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_widgets.c -o out/linux/test_widgets $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_events.c -o out/linux/test_events $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_events_ext.c -o out/linux/test_events_ext $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_smoke.c -o out/linux/test_smoke $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_window_visibility.c -o out/linux/test_window_visibility $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_dynamic.c -o out/linux/test_dynamic $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_lifecycle.c -o out/linux/test_lifecycle $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_clipboard.c -o out/linux/test_clipboard $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_buffering.c -o out/linux/test_buffering $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_features.c -o out/linux/test_features $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS -DMKGUI_TEST_MINIMAL tests/test_features.c -o out/linux/test_features_minimal $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS -DMKGUI_TEST_MINIMAL -DMKGUI_HAS_ALL tests/test_features.c -o out/linux/test_features_all $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_editor_features.c -o out/linux/test_editor_features $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS benchmark.c -o out/linux/mkguibench $LINUX_LIBS $LDFLAGS
	) &

	(
		$CC $CFLAGS tests/test_icon_library.c -o out/linux/test_icon_library $LINUX_LIBS $LDFLAGS
		$CC $CFLAGS tests/test_icon_storage.c -o out/linux/test_icon_storage $LINUX_LIBS $LDFLAGS
	) &

	# Win32 platform-layer test; covers the divergent Win32 windowing surface.
	# Run with: wine out/windows/test_platform_win32.exe
	if [ "$WIN_ENABLED" -eq 1 ]; then
		(
			$WINCC $CFLAGS tests/test_filedialog.c -o out/windows/test_filedialog.exe $WINDOWS_LIBS $WINDOWS_PTHREAD -mconsole $LDFLAGS
			$WINCC $CFLAGS tests/test_undo.c -o out/windows/test_undo.exe $WINDOWS_LIBS $WINDOWS_PTHREAD -mconsole $LDFLAGS
			$WINCC $CFLAGS -DMKGUI_TEST_MINIMAL tests/test_features.c -o out/windows/test_features_minimal.exe $WINDOWS_LIBS $WINDOWS_PTHREAD -mconsole $LDFLAGS
			$WINCC $CFLAGS tests/test_icon_storage.c -o out/windows/test_icon_storage.exe $WINDOWS_LIBS $WINDOWS_PTHREAD -mconsole $LDFLAGS
		) &

		(
			$WINCC $CFLAGS tests/test_platform_win32.c -o out/windows/test_platform_win32.exe $WINDOWS_LIBS $WINDOWS_PTHREAD -mconsole $LDFLAGS
		) &

		(
			$WINCC $CFLAGS benchmark.c -o out/windows/mkguibench.exe $WINDOWS_LIBS $WINDOWS_PTHREAD -mconsole $LDFLAGS
		) &
	fi
fi

if [ "$TARGET" = "editor" ] || [ "$TARGET" = "all" ]; then
	# Editor: private bridge into the shared rendering/layout implementation.
	(
		$CC $CFLAGS editor.c -o out/linux/editor $LINUX_SHARED $LDFLAGS
	) &

	if [ "$WIN_ENABLED" -eq 1 ]; then
		(
			$WINCC $CFLAGS editor.c -o out/windows/editor.exe $WINDOWS_SHARED -mwindows $WINDOWS_PTHREAD $LDFLAGS
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
