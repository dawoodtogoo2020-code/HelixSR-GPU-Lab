import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace

MODULE = Path(__file__).resolve().parents[1] / "tools" / "install_support.py"
spec = importlib.util.spec_from_file_location("install_support", MODULE)
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)

class BundleTests(unittest.TestCase):
    def setUp(self):
        build = Path(__file__).resolve().parents[1] / ".build"
        build.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=build)
        self.root = Path(self.temp.name)
        self.entries = {}
        for name in installer.FILES:
            file = self.root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            content = (name + "\n").encode()
            file.write_bytes(content)
            self.entries[name] = {"size": len(content), "sha256": hashlib.sha256(content).hexdigest()}
        self.write_manifest()

    def tearDown(self):
        self.temp.cleanup()

    def write_manifest(self):
        raw = json.dumps({"format": 1, "files": self.entries}).encode()
        (self.root / "manifest.json").write_bytes(raw)
        self.hash = hashlib.sha256(raw).hexdigest()

    def test_valid_plan_and_install_disabled(self):
        plan = installer.verify(self.root, self.hash)
        self.assertEqual(len(plan), 6)
        self.assertTrue(all(row["destination"].startswith("/data/") for row in plan))
        self.assertEqual(installer.main([str(self.root), "--manifest-sha256", self.hash, "--install"]), 1)

    def test_reject_wrong_manifest_hash_and_modified_file(self):
        with self.assertRaises(installer.BundleError):
            installer.verify(self.root, "0" * 64)
        (self.root / "app/eboot.bin").write_bytes(b"bad")
        with self.assertRaises(installer.BundleError):
            installer.verify(self.root, self.hash)

    def test_reject_extra_and_traversal_entries(self):
        self.entries["../game/eboot.bin"] = {"size": 1, "sha256": "0" * 64}
        self.write_manifest()
        with self.assertRaises(installer.BundleError):
            installer.verify(self.root, self.hash)

    def test_reject_duplicate_json_keys(self):
        raw = b'{"format":1,"format":1,"files":{}}'
        (self.root / "manifest.json").write_bytes(raw)
        with self.assertRaises(installer.BundleError):
            installer.verify(self.root, hashlib.sha256(raw).hexdigest())

    def test_reject_boolean_size(self):
        self.entries["app/eboot.bin"]["size"] = True
        self.write_manifest()
        with self.assertRaises(installer.BundleError):
            installer.verify(self.root, self.hash)

    def test_path_and_open_stat_can_report_different_ctime(self):
        path_stat = SimpleNamespace(st_dev=1, st_ino=2, st_size=3, st_mtime_ns=4, st_ctime_ns=5)
        open_stat = SimpleNamespace(st_dev=1, st_ino=2, st_size=3, st_mtime_ns=4, st_ctime_ns=6)
        self.assertTrue(installer.same_file_stat(path_stat, open_stat))
        open_stat.st_ino = 9
        self.assertFalse(installer.same_file_stat(path_stat, open_stat))
    def test_reject_oversized_manifest(self):
        raw = b" " * (installer.MAX_MANIFEST + 1)
        (self.root / "manifest.json").write_bytes(raw)
        with self.assertRaises(installer.BundleError):
            installer.verify(self.root, hashlib.sha256(raw).hexdigest())

    def test_reject_file_changed_while_open(self):
        target = self.root / "app/eboot.bin"
        original = installer.same_file_stat
        changed = False
        def mutate_after_open(a, b):
            nonlocal changed
            if not changed and a.st_size == len(("app/eboot.bin\n").encode()):
                changed = True
                with target.open("ab") as stream:
                    stream.write(b"changed")
            return original(a, b)
        with mock.patch.object(installer, "same_file_stat", side_effect=mutate_after_open):
            with self.assertRaises(installer.BundleError):
                installer.verify(self.root, self.hash)
        self.assertTrue(changed)
    def test_reject_symlink(self):
        file = self.root / "app/eboot.bin"
        target = self.root / "target"
        target.write_bytes(file.read_bytes())
        file.unlink()
        try:
            file.symlink_to(target)
        except OSError:
            self.skipTest("Creating symbolic links is unavailable")
        with self.assertRaises(installer.BundleError):
            installer.verify(self.root, self.hash)

if __name__ == "__main__":
    unittest.main()
