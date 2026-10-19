#!/usr/bin/env python3
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT

import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
RELEASES = ("0.0.3", "0.0.4", "1.0.0", "1.0.1", "2.0.0")


class InstallationTests(unittest.TestCase):

	@classmethod
	def setUpClass(cls):
		cls.temporary = tempfile.TemporaryDirectory(prefix="mkgui-install-")
		cls.work = Path(cls.temporary.name)
		cls.prefix = cls.work / "installed"
		cls.sources = cls.work / "sources"
		cls.sources.mkdir()
		cls.environment = os.environ.copy()
		cls.environment["PKG_CONFIG_PATH"] = str(cls.prefix / "lib/pkgconfig")
		cls.environment["LD_LIBRARY_PATH"] = str(cls.prefix / "lib")
		metadata = cls.prefix / "lib/pkgconfig"
		metadata.mkdir(parents=True)
		(metadata / "mkguiicons.pc").write_text("Name: mkguiicons\nDescription: Installation test dependency\nVersion: 1.0.0\nLibs:\nCflags:\n")
		cls.consumer = cls.work / "consumer.c"
		cls.consumer.write_text('''#include <stdio.h>
#include <string.h>
#include <mkgui.h>
#include <mkgui_glview.h>

// [=]===^=[ main ]=============================================================================[=]
int main(void) {
	if(strcmp(MKGUI_FIXTURE_RELEASE, MKGUI_FIXTURE_GL_RELEASE) || strcmp(MKGUI_FIXTURE_RELEASE, mkgui_fixture_release())) {
		return 1;

	}

	puts(MKGUI_FIXTURE_RELEASE);
	return 0;

}
''')
		cls.snapshots = {}
		cls.executables = {}

		for release in RELEASES:
			fixture = cls.sources / release
			output = fixture / "out/linux"
			output.mkdir(parents=True)
			shutil.copy2(ROOT / "install.sh", fixture)
			(fixture / "mkgui.version").write_text(release + "\n")
			(fixture / "mkgui.h").write_text(f'#define MKGUI_FIXTURE_RELEASE "{release}"\nchar *mkgui_fixture_release(void);\n')
			(fixture / "mkgui_glview.h").write_text(f'#include "mkgui.h"\n#define MKGUI_FIXTURE_GL_RELEASE "{release}"\n')
			implementation = fixture / "fixture.c"
			implementation.write_text(f'''// [=]===^=[ mkgui_fixture_release ]=============================================================[=]
char *mkgui_fixture_release(void) {{
	return "{release}";

}}
''')
			cls.run_command(["gcc", "-std=c99", "-O2", "-Wall", "-Wextra", "-fPIC", "-c", str(implementation), "-o", str(output / "fixture.o")])
			cls.run_command(["ar", "rcs", str(output / "libmkgui.a"), str(output / "fixture.o")])
			cls.run_command(["gcc", "-shared", str(output / "fixture.o"), f"-Wl,-soname,libmkgui.so.{release}", "-o", str(output / f"libmkgui.so.{release}")])
			cls.run_command([str(fixture / "install.sh")], environment={**cls.environment, "PREFIX": str(cls.prefix), "DESTDIR": ""})
			package = "mkgui" + release
			flags = shlex.split(cls.run_command(["pkg-config", "--cflags", "--libs", package]).stdout)
			executable = cls.work / package
			cls.run_command(["gcc", "-std=c99", "-O2", "-Wall", "-Wextra", str(cls.consumer), *flags, "-o", str(executable)])
			cls.executables[release] = executable
			cls.snapshots[release] = cls.snapshot(release)

			for installed, original in cls.snapshots.items():
				if cls.snapshot(installed) != original:
					raise RuntimeError(f"Installing {release} changed files belonging to {installed}")

			for installed, application in cls.executables.items():
				if cls.run_command([str(application)]).stdout.strip() != installed:
					raise RuntimeError(f"Application for {installed} changed after installing {release}")


	@classmethod
	def tearDownClass(cls):
		cls.temporary.cleanup()


	@classmethod
	def run_command(cls, command, environment=None, check=True):
		return subprocess.run(command, env=cls.environment if environment is None else environment, capture_output=True, text=True, check=check)


	@classmethod
	def snapshot(cls, release):
		package = "mkgui" + release
		files = [cls.prefix / f"lib/libmkgui.so.{release}", cls.prefix / f"lib/lib{package}.a", cls.prefix / f"lib/pkgconfig/{package}.pc", cls.prefix / f"include/{package}/mkgui.h", cls.prefix / f"include/{package}/mkgui_glview.h"]
		return tuple(hashlib.sha256(path.read_bytes()).hexdigest() for path in files) + (os.readlink(cls.prefix / f"lib/lib{package}.so"),)


	def test_all_release_files_survive(self):
		for release in RELEASES:
			with self.subTest(release=release):
				self.assertEqual(self.snapshot(release), self.snapshots[release])


	def test_full_release_sonames_and_dependencies(self):
		for release, executable in self.executables.items():
			with self.subTest(release=release):
				library = self.run_command(["readelf", "-d", str(self.prefix / f"lib/libmkgui.so.{release}")]).stdout
				application = self.run_command(["readelf", "-d", str(executable)]).stdout
				self.assertIn(f"Library soname: [libmkgui.so.{release}]", library)
				self.assertIn(f"Shared library: [libmkgui.so.{release}]", application)
				self.assertEqual(self.run_command([str(executable)]).stdout.strip(), release)


	def test_no_shared_toolkit_development_files(self):
		for relative in ("lib/libmkgui.so", "lib/libmkgui.so.0", "lib/libmkgui.so.1", "lib/libmkgui.so.2", "lib/libmkgui.a", "lib/pkgconfig/mkgui.pc", "include/mkgui.h", "include/mkgui_glview.h"):
			with self.subTest(path=relative):
				self.assertFalse((self.prefix / relative).exists())


	def test_static_archive_selection(self):
		for release in RELEASES:
			with self.subTest(release=release):
				package = "mkgui" + release
				flags = shlex.split(self.run_command(["pkg-config", "--cflags", package]).stdout)
				libdir = self.run_command(["pkg-config", "--variable=libdir", package]).stdout.strip()
				executable = self.work / (package + "-static")
				self.run_command(["gcc", "-std=c99", "-O2", "-Wall", "-Wextra", str(self.consumer), *flags, str(Path(libdir) / f"lib{package}.a"), "-o", str(executable)])
				self.assertEqual(self.run_command([str(executable)]).stdout.strip(), release)
				self.assertNotIn("Shared library: [libmkgui", self.run_command(["readelf", "-d", str(executable)]).stdout)


	def test_destdir_preserves_installed_prefix(self):
		staging = self.work / "stage"
		release = RELEASES[0]
		self.run_command([str(self.sources / release / "install.sh")], environment={**self.environment, "PREFIX": "/usr", "DESTDIR": str(staging)})
		metadata = (staging / f"usr/lib/pkgconfig/mkgui{release}.pc").read_text()
		self.assertIn("prefix=/usr\n", metadata)
		self.assertNotIn(str(staging), metadata)
		self.assertTrue((staging / f"usr/include/mkgui{release}/mkgui.h").is_file())
		self.assertTrue((staging / f"usr/lib/libmkgui{release}.so").is_file())


	def test_rejects_major_only_soname_before_installation(self):
		fixture = self.sources / RELEASES[0]
		library = fixture / f"out/linux/libmkgui.so.{RELEASES[0]}"
		self.run_command(["gcc", "-shared", str(fixture / "out/linux/fixture.o"), "-Wl,-soname,libmkgui.so.0", "-o", str(library)])
		destination = self.work / "rejected"
		result = self.run_command([str(fixture / "install.sh")], environment={**self.environment, "PREFIX": str(destination), "DESTDIR": ""}, check=False)
		self.assertNotEqual(result.returncode, 0)
		self.assertIn("expected SONAME", result.stderr)
		self.assertFalse(destination.exists())


if __name__ == "__main__":
	unittest.main()
