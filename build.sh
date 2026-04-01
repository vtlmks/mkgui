#!/bin/bash

CC=gcc
WINCC=x86_64-w64-mingw32-gcc

CFLAGS="-std=c99 "
CFLAGS+="-O2 "
CFLAGS+="-march=x86-64-v2 "
CFLAGS+="-fwrapv "
CFLAGS+="-Wall "
CFLAGS+="-Wextra "
CFLAGS+="-Wno-missing-field-initializers "
CFLAGS+="-Wno-unused-function "
CFLAGS+="-Wno-unused-parameter "
CFLAGS+="-Wformat-truncation "
CFLAGS+="-Wno-stringop-truncation "
CFLAGS+="-Wsign-conversion "

LINUX_LIBS="$(pkg-config --cflags freetype2 fontconfig) -lX11 -lXext $(pkg-config --libs freetype2 fontconfig) -lm "
LINUX_DEMO_LIBS="$LINUX_LIBS -lGL "
WINDOWS_LIBS="-lgdi32 -mwindows "
WINDOWS_DEMO_LIBS="$WINDOWS_LIBS -lopengl32 "

BUILD_TYPE=$1
if [ -z "$BUILD_TYPE" ]; then
	BUILD_TYPE="normal"
fi

case "$BUILD_TYPE" in
	"normal")
		CFLAGS+="-ggdb -fno-omit-frame-pointer "
		;;
	"release")
		CFLAGS+="-s -Wl,--strip-all "
		;;
	"debug")
		CFLAGS+="-g -O0 "
		;;
	"asan")
		CFLAGS+="-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer "
		SKIP_WINDOWS=1
		;;
	"clean")
		rm -f demo editor demo.exe editor.exe mkgui.o libmkgui.a tools/extract_icons tests/test_layout
		exit 0
		;;
	*)
		echo "Unknown build type: $BUILD_TYPE"
		exit 1
		;;
esac

set -e

# Build Linux demo
(
	$CC $CFLAGS demo.c -o demo $LINUX_DEMO_LIBS
) &

# Build Linux editor
(
	$CC $CFLAGS editor.c -o editor $LINUX_LIBS
) &

# Build Windows demo
if [ -z "$SKIP_WINDOWS" ] && command -v $WINCC &>/dev/null; then
	(
		$WINCC $CFLAGS demo.c -o demo.exe $WINDOWS_DEMO_LIBS
	) &

	# Build Windows editor
	(
		$WINCC $CFLAGS editor.c -o editor.exe $WINDOWS_LIBS
	) &
fi

# Build static library (unity-built, but with exported symbols)
(
	$CC $CFLAGS -DMKGUI_LIBRARY -c mkgui.c -o mkgui.o $LINUX_LIBS
	ar rcs libmkgui.a mkgui.o
	rm -f mkgui.o
) &

# Build extract_icons tool
(
	$CC -std=gnu99 -O2 -Wall -Wextra -Wno-stringop-truncation -Wno-format-truncation tools/extract_icons.c -o tools/extract_icons
) &

# Build layout tests
(
	$CC $CFLAGS tests/test_layout.c -o tests/test_layout $LINUX_LIBS
) &

# Build widget behavior tests
(
	$CC $CFLAGS tests/test_widgets.c -o tests/test_widgets $LINUX_LIBS
) &

wait
