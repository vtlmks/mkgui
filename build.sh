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
		rm -f demo editor demo.exe editor.exe mkgui.o libmkgui.a
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
if command -v $WINCC &>/dev/null; then
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

# Build gen_icons tool and generate icon pack
(
	$CC -std=c99 -O2 -Wall tools/gen_icons.c -o tools/gen_icons $(pkg-config --cflags freetype2) $(pkg-config --libs freetype2)
) &

wait


./tools/gen_icons ext/materialdesignicons-webfont.ttf 16 mdi_icons.dat
./tools/gen_icons ext/materialdesignicons-webfont.ttf 40 mdi_icons_toolbar.dat
