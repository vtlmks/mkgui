#!/bin/bash
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT

set -e
cd "$(dirname "$0")"
prefix=${PREFIX:-/usr}
destination=${DESTDIR:-}
release=$(cat mkguiicons.version)
library=libmkguiicons.so.$release

if [ ! -f "out/linux/$library" ]; then
	printf 'Build the icon library first: ./build.sh icons\n' >&2
	exit 1
fi

install -d "$destination$prefix/lib/pkgconfig" "$destination$prefix/include" "$destination$prefix/share/licenses/mkguiicons"
install -m 755 "out/linux/$library" "$destination$prefix/lib/$library"
ln -sf "$library" "$destination$prefix/lib/libmkguiicons.so.1"
ln -sf libmkguiicons.so.1 "$destination$prefix/lib/libmkguiicons.so"
install -m 644 mkguiicons.h mkguiicons_names.h "$destination$prefix/include/"
install -m 644 fluentui-system-icons/LICENSE fluentui-system-icons/NOTICE "$destination$prefix/share/licenses/mkguiicons/"
cat > "$destination$prefix/lib/pkgconfig/mkguiicons.pc" <<PKGCONFIG
prefix=$prefix
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: mkguiicons
Description: Shared Fluent SVG icon catalogue
Version: $release
Libs: -L\${libdir} -lmkguiicons
Cflags: -I\${includedir}
PKGCONFIG
printf 'Installed mkguiicons into %s%s\n' "$destination" "$prefix"
