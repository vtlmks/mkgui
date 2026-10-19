#!/bin/bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT

set -e
cd "$(dirname "$0")"
prefix=${PREFIX:-/usr}
destination=${DESTDIR:-}
release=$(cat mkgui.version)
library=libmkgui.so.$release
package=mkgui$release

if [ ! -f "out/linux/$library" ]; then
	printf 'Build mkgui first: ./build.sh library\n' >&2
	exit 1
fi

soname=$(readelf -d "out/linux/$library" | awk '$2 == "(SONAME)" {print $5}')

if [ "$soname" != "[$library]" ]; then
	printf 'Rebuild mkgui: expected SONAME %s, found %s\n' "$library" "$soname" >&2
	exit 1
fi

install -d "$destination$prefix/lib/pkgconfig" "$destination$prefix/include/$package"
install -m 755 "out/linux/$library" "$destination$prefix/lib/$library"
ln -sf "$library" "$destination$prefix/lib/lib$package.so"
install -m 644 out/linux/libmkgui.a "$destination$prefix/lib/lib$package.a"
install -m 644 mkgui.h mkgui_glview.h "$destination$prefix/include/$package/"
cat > "$destination$prefix/lib/pkgconfig/$package.pc" <<PKGCONFIG
prefix=$prefix
libdir=\${prefix}/lib
includedir=\${prefix}/include/$package

Name: $package
Description: Software rendered GUI toolkit for X11 and Win32
Version: $release
Requires.private: mkguiicons
Libs: -L\${libdir} -l$package
Libs.private: -lX11 -lXext $(pkg-config --libs freetype2 fontconfig) -lm
Cflags: -I\${includedir}
PKGCONFIG
printf 'Installed mkgui %s into %s%s\n' "$release" "$destination" "$prefix"
