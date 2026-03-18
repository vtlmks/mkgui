#!/bin/bash

CC=gcc
WINCC=x86_64-w64-mingw32-gcc

CFLAGS="-std=c99 "
CFLAGS+="-O2 "
CFLAGS+="-fwrapv "
CFLAGS+="-Wall "
CFLAGS+="-Wextra "
CFLAGS+="-Wno-missing-field-initializers "
CFLAGS+="-Wno-unused-function "
CFLAGS+="-Wno-unused-parameter "
CFLAGS+="-Wformat-truncation "
CFLAGS+="-Wno-stringop-truncation "
CFLAGS+="-Wsign-conversion "

LINUX_LIBS="$(pkg-config --cflags freetype2) -lX11 -lXext $(pkg-config --libs freetype2) -lm "
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
	"clean")
		rm -f demo editor demo.exe editor.exe
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

# Build Linux benchmark
(
	$CC $CFLAGS benchmark.c -o benchmark $LINUX_LIBS
) &

# Build Windows demo
if command -v $WINCC &>/dev/null; then
	(
		$WINCC $CFLAGS demo.c -o demo.exe $WINDOWS_DEMO_LIBS
	) &

	# Build Windows editor
	(
		$WINCC $CFLAGS editor.c -o editor.exe $WINDOWS_LIBS
	) &
fi

# Build gen_icons tool and generate icon pack
(
	$CC -std=c99 -O2 -Wall tools/gen_icons.c -o tools/gen_icons $(pkg-config --cflags freetype2) $(pkg-config --libs freetype2)
) &

wait


./tools/gen_icons ext/materialdesignicons-webfont.ttf 18 mdi_icons.dat
