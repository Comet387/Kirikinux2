#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Check packaging ownership using fixture AppDirs and an isolated dpkg root."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "copy_distro_notices", ROOT / "scripts/linux/copy-distro-notices.py"
)
notices = importlib.util.module_from_spec(spec)
spec.loader.exec_module(notices)


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        build = ROOT / "build-packaging-tests"
        build.mkdir(exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=build)
        self.addCleanup(self.tmp.cleanup)
        self.work = Path(self.tmp.name)
        self.env = dict(os.environ, TMPDIR=str(self.work))

    def run_tool(self, *args, check=True, env=None):
        return subprocess.run(
            [str(arg) for arg in args], env=env or self.env,
            capture_output=True, text=True, check=check,
        )

    def fixture_appdir(self, legacy=False):
        appdir = self.work / "appdir"
        engine = appdir / "usr/lib/kirikinux/kirikinux"
        write(engine, '#!/bin/sh\nprintf "fixture engine: %s\\n" "$*"\n')
        engine.chmod(0o755)
        launcher = appdir / "usr/bin/kirikinux2"
        write(launcher, (ROOT / "platform/linux/kirikinux-launcher.in").read_text()
              .replace("@KR2_REL_RUNTIME@", "../lib/kirikinux"))
        launcher.chmod(0o755)
        write(appdir / "usr/share/doc/kirikinux2/LICENSE", "project license\n")
        for package in ("acl", "bash"):
            parent = "usr/share/doc" if legacy else "usr/share/doc/kirikinux2/distro"
            write(appdir / parent / package / "copyright", f"bundled {package} notice\n")
        return appdir

    def package(self, appdir, check=True):
        out = self.work / "dist"
        env = dict(self.env, APPDIR=str(appdir), KR2_PACKAGE_DIR=str(out),
                   KR2_DEB_VERSION="1.3.9-1")
        result = self.run_tool("bash", ROOT / "scripts/linux/package-deb.sh", env=env, check=check)
        return out / "kirikinux2_1.3.9-1_amd64.deb", result

    def check_payload(self, deb):
        extracted = self.work / "extracted"
        self.run_tool("dpkg-deb", "--extract", deb, extracted)
        self.assertEqual(
            {path.name for path in (extracted / "usr/share/doc").iterdir()},
            {"kirikinux2"},
        )
        for package in ("acl", "bash"):
            self.assertEqual(
                (extracted / "usr/share/doc/kirikinux2/distro" / package / "copyright").read_text(),
                f"bundled {package} notice\n",
            )
        self.assertEqual((extracted / "usr/share/doc/kirikinux2/LICENSE").read_text(), "project license\n")
        self.assertIn("fixture engine: --help", self.run_tool(extracted / "usr/bin/kirikinux2", "--help").stdout)

    def isolated_root(self):
        root = self.work / "dpkg-root"
        admin = root / "var/lib/dpkg"
        # Model two packages already installed on the user's system. Every
        # dpkg operation below explicitly targets this temporary root.
        status = ""
        for package in ("acl", "bash"):
            path = f"/usr/share/doc/{package}/copyright"
            write(root / path.lstrip("/"), f"system {package} notice\n")
            write(admin / "info" / f"{package}.list", path + "\n")
            status += (f"Package: {package}\nStatus: install ok installed\n"
                       "Priority: optional\nSection: utils\nArchitecture: amd64\n"
                       "Version: 2.3.2-2\nMaintainer: Fixture <fixture@example.invalid>\n"
                       "Description: installed system package fixture\n\n")
        write(admin / "status", status)
        return root

    def test_copy_notices_uses_own_namespace_and_sysroot(self):
        root, appdir = self.work / "sysroot", self.work / "appdir"
        # This absolute path also exists on typical Debian/Ubuntu hosts.
        write(root / "usr/share/doc/bash/copyright", "sysroot bash notice\n")
        acl = root / "usr/share/doc/acl/copyright"
        acl.parent.mkdir(parents=True)
        acl.symlink_to("/usr/share/doc/bash/copyright")
        relative = root / "usr/share/doc/relative/copyright"
        relative.parent.mkdir(parents=True)
        relative.symlink_to("../acl/copyright")
        missing = root / "usr/share/doc/missing/copyright"
        missing.parent.mkdir(parents=True)
        missing.symlink_to("/nonexistent-copyright")
        write(appdir / "usr/share/doc/kirikinux2/LICENSE", "project license\n")
        notices.copy_notices(root, appdir)
        for package in ("acl", "bash", "relative"):
            target = appdir / "usr/share/doc/kirikinux2/distro" / package / "copyright"
            self.assertEqual(target.read_text(), "sysroot bash notice\n")
            self.assertFalse(target.is_symlink())
        self.assertFalse((appdir / "usr/share/doc/kirikinux2/distro/missing").exists())
        self.assertEqual({p.name for p in (appdir / "usr/share/doc").iterdir()}, {"kirikinux2"})

    def test_new_appdir_keeps_namespaced_notices(self):
        deb, _ = self.package(self.fixture_appdir())
        self.check_payload(deb)

    def test_legacy_appdir_reproduces_conflict_then_unpacks_cleanly(self):
        appdir = self.fixture_appdir(legacy=True)
        original = self.work / "original-root"
        shutil.copytree(appdir, original)
        write(original / "DEBIAN/control",
              "Package: kirikinux2\nVersion: 1.3.9\nArchitecture: amd64\n"
              "Maintainer: Fixture <fixture@example.invalid>\nDescription: legacy fixture\n")
        broken = self.work / "broken.deb"
        self.run_tool("dpkg-deb", "--root-owner-group", "--build", original, broken)
        root = self.isolated_root()
        result = self.run_tool("dpkg", f"--root={root}", "--force-not-root",
                               "--path-include=/usr/share/doc/*", "--unpack", broken, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("trying to overwrite", result.stderr)
        self.assertIn("/usr/share/doc/acl/copyright", result.stderr)
        deb, _ = self.package(appdir)
        self.check_payload(deb)
        # Override host/container doc exclusions for a complete payload check.
        self.run_tool("dpkg", f"--root={root}", "--force-not-root",
                      "--path-include=/usr/share/doc/*", "--unpack", deb)
        self.assertEqual((root / "usr/share/doc/kirikinux2/LICENSE").read_text(), "project license\n")
        for package in ("acl", "bash"):
            self.assertEqual((root / "usr/share/doc" / package / "copyright").read_text(),
                             f"system {package} notice\n")
            # Only the staging copy is migrated.
            self.assertEqual((appdir / "usr/share/doc" / package / "copyright").read_text(),
                             f"bundled {package} notice\n")
            self.assertEqual((root / "usr/share/doc/kirikinux2/distro" / package / "copyright").read_text(),
                             f"bundled {package} notice\n")

    def test_conflicting_legacy_and_new_notices_are_rejected(self):
        appdir = self.fixture_appdir(legacy=True)
        write(appdir / "usr/share/doc/kirikinux2/distro/acl/copyright", "different notice\n")
        deb, result = self.package(appdir, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("conflicting distribution notices", result.stderr)
        self.assertFalse(deb.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
