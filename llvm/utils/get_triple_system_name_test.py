#!/usr/bin/env python3
# ===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ===----------------------------------------------------------------------===##
"""Tests for get_triple_system_name.

These guard against the Python .def parser drifting from TripleNames.def
The C++ side is kept in sync by the compiler (the
generated switch cases must cover every enum), so the risk here is the regex
silently dropping a row the C++ accepts. We check that every TRIPLE_OS /
TRIPLE_OS_ALIAS / TRIPLE_ENV macro line in the .def is captured by the parser.
"""

import re
import unittest

import get_triple_system_name as g


class ParseTest(unittest.TestCase):
    def setUp(self):
        self.def_path = g.find_def_file()
        self.names = g.parse(self.def_path)
        with open(self.def_path) as f:
            self.lines = f.readlines()

    def _count(self, macro):
        # Count real macro invocations, skipping the #define lines.
        pat = re.compile(r"^\s*" + macro + r"\(")
        return sum(
            1 for line in self.lines if pat.match(line) and "#define" not in line
        )

    def test_all_os_rows_parsed(self):
        expected = self._count("TRIPLE_OS") + self._count("TRIPLE_OS_ALIAS")
        self.assertEqual(len(self.names.os), expected)

    def test_all_env_rows_parsed(self):
        self.assertEqual(len(self.names.env), self._count("TRIPLE_ENV"))

    def test_no_empty_names(self):
        for name, _ in self.names.os:
            self.assertTrue(name)
        for name, _ in self.names.env:
            self.assertTrue(name)

    def test_prefix_ordering(self):
        # A name that is a prefix of an earlier name is fine; a name that is a
        # prefix of a LATER name would shadow it under StartsWith matching.
        for table in (self.names.os, self.names.env):
            names = [n for n, _ in table]
            for i, short in enumerate(names):
                for longer in names[i + 1 :]:
                    self.assertFalse(
                        longer.startswith(short) and longer != short,
                        f"'{short}' precedes and shadows '{longer}'",
                    )

    def test_known_triples(self):
        cases = {
            "x86_64-unknown-linux-gnu": "Linux",
            "arm64-apple-darwin": "Darwin",
            "s390x-ibm-zos": "OS390",
            "aarch64-unknown-linux-android": "Android",
            "x86_64-pc-windows-cygnus": "CYGWIN",
            "wasm32-unknown-wasip1": "WASI",
            "arm64-apple-ios": "iOS",
            "riscv64-unknown-elf": "Generic",
        }
        for triple, want in cases.items():
            self.assertEqual(g.system_name(self.names, triple), want, triple)

    def test_non_canonical_triples(self):
        # Vendor omitted / non-standard, os or env not in the fixed
        # position, version suffixes, and GCC-legacy spellings
        cases = {
            "aarch64-linux-android21": "Android",
            "aarch64-unknown-linux-android21": "Android",
            "arm-linux-androideabi": "Android",
            "armv7a-linux-androideabi29": "Android",
            # Vendor omitted, os in the vendor slot.
            "x86_64-linux-gnu": "Linux",
            "riscv64-linux-gnu": "Linux",
            # OS version suffix.
            "x86_64-apple-macosx10.15": "Darwin",
            "armv7-apple-ios13.0": "iOS",
            # Non-standard vendor, still resolvable by os component.
            "x86_64-pc-freebsd14": "FreeBSD",
        }
        for triple, want in cases.items():
            self.assertEqual(g.system_name(self.names, triple), want, triple)

    def test_matches_normalize_funky_triples(self):
        # The "real-world funky triples" from TripleTest.cpp's Normalization
        # test. system_name must classify the same OS that Triple::normalize
        # picks, including os-in-vendor-slot and GCC-legacy windows spellings
        # (mingw* -> windows-gnu, cygwin*/msys -> windows-cygnus).
        cases = {
            "i386-mingw32": "Windows",  # -> i386-unknown-windows-gnu
            "x86_64-linux-gnu": "Linux",  # -> x86_64-unknown-linux-gnu
            "i486-linux-gnu": "Linux",  # -> i486-unknown-linux-gnu
            "i386-redhat-linux": "Linux",  # -> i386-redhat-linux
            "i686-linux": "Linux",  # -> i686-unknown-linux
            "arm-none-eabi": "Generic",  # -> arm-unknown-none-eabi (no OS)
            "ve-linux": "Linux",  # -> ve-unknown-linux
            "wasm32-wasi": "WASI",  # -> wasm32-unknown-wasi
            "wasm64-wasi": "WASI",  # -> wasm64-unknown-wasi
            "x86_64-pc-cygwin": "CYGWIN",  # -> x86_64-pc-windows-cygnus
            "x86_64-pc-msys": "CYGWIN",  # -> x86_64-pc-windows-cygnus
            "x86_64-w64-mingw32": "Windows",  # -> x86_64-w64-windows-gnu
            "i686-w64-mingw32": "Windows",
        }
        for triple, want in cases.items():
            self.assertEqual(g.system_name(self.names, triple), want, triple)

    def test_normalize_special_cases(self):
        # The OS/environment special-cases that Triple::normalize applies, one
        # per category, to ensure the script tracks each rewrite it performs.
        cases = {
            # Win32 normalizes to "windows" regardless of the incoming spelling
            # or msvc/gnu environment.
            "x86_64-pc-windows": "Windows",
            "x86_64-pc-windows-msvc": "Windows",
            "x86_64-pc-windows-gnu": "Windows",
            # mingw* -> windows-gnu; cygwin*/msys -> windows-cygnus.
            "x86_64-w64-mingw32": "Windows",
            "x86_64-pc-cygwin": "CYGWIN",
            "x86_64-pc-msys": "CYGWIN",
            # androideabi keeps the Android environment override.
            "arm-linux-androideabi": "Android",
            # The full Apple OS family. driverkit/bridgeos map to Darwin
            # directly in the table (they are always Apple); firmware has no
            # dedicated CMake name and maps to Darwin via the apple-vendor
            # catch-all.
            "x86_64-apple-macosx": "Darwin",
            "arm-apple-darwin": "Darwin",
            "arm-apple-ios": "iOS",
            "arm-apple-tvos": "tvOS",
            "arm-apple-watchos": "watchOS",
            "arm-apple-xros": "visionOS",
            "arm64-apple-visionos": "visionOS",
            "arm-apple-driverkit": "Darwin",
            "arm-apple-bridgeos": "Darwin",
            "arm-apple-firmware": "Darwin",
        }
        for triple, want in cases.items():
            self.assertEqual(g.system_name(self.names, triple), want, triple)

    def test_firmware_requires_apple_vendor(self):
        # firmware has no dedicated CMake name and only maps to Darwin
        # for the apple vendor. With any other vendor it is
        # Generic. (For a non-apple vendor Triple::normalize actually
        # fatal-errors; here we only need to ensure we never claim
        # Darwin.)
        for vendor in ("none", "unknown", "pc"):
            triple = f"arm-{vendor}-firmware"
            self.assertEqual(g.system_name(self.names, triple), "Generic", triple)

    def test_driverkit_bridgeos_always_darwin(self):
        # driverkit and bridgeos are always Apple, so they map to Darwin
        # directly in the table regardless of the vendor component.
        for os in ("driverkit", "bridgeos"):
            for vendor in ("apple", "none", "unknown", "pc"):
                triple = f"arm-{vendor}-{os}"
                self.assertEqual(g.system_name(self.names, triple), "Darwin", triple)

    def test_bare_os_no_arch(self):
        # A lone os component with no arch is a valid clang target (e.g.
        # --target=darwin -> unknown-unknown-darwin), so it must classify by
        # the first (and only) component rather than being skipped as the arch.
        cases = {
            "darwin": "Darwin",
            "linux": "Linux",
            "ios": "iOS",
            "freebsd": "FreeBSD",
            "wasi": "WASI",
            "zos": "OS390",
            "mingw32": "Windows",
            "cygwin": "CYGWIN",
        }
        for triple, want in cases.items():
            self.assertEqual(g.system_name(self.names, triple), want, triple)

    def test_too_few_components_returns_none(self):
        # Only when there is no classifiable OS in any component does the caller
        # fall back to the host system name.
        self.assertIsNone(g.system_name(self.names, "x86_64"))
        self.assertIsNone(g.system_name(self.names, "arm-none"))


if __name__ == "__main__":
    unittest.main()
