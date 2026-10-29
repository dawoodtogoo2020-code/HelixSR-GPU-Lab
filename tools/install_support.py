#!/usr/bin/env python3
"""Verify and plan a HelixSR GPU Lab support bundle. No network operations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import sys

# Relative bundle name and the corresponding destination on the console.
FILES = {
    "app/eboot.bin": "/data/homebrew/PPSA99272/eboot.bin",
    "app/sce_module/libc.prx": "/data/homebrew/PPSA99272/sce_module/libc.prx",
    "app/sce_sys/icon0.png": "/data/homebrew/PPSA99272/sce_sys/icon0.png",
    "app/sce_sys/param.json": "/data/homebrew/PPSA99272/sce_sys/param.json",
    "payloads/helixsr-launch.elf": "/data/pldmgr/payloads/local/helixsr-devloop/helixsr-launch.elf",
    "payloads/helixsr-inventory.elf": "/data/pldmgr/payloads/local/helixsr-devloop/helixsr-inventory.elf",
}
MAX_MANIFEST = 65536
MAX_FILE = 128 * 1024 * 1024

class BundleError(ValueError):
    pass

def reject_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise BundleError("Duplicate JSON key: " + key)
        result[key] = value
    return result

def check_path(path, directory=False):
    try:
        info = path.lstat()
    except OSError as exc:
        raise BundleError("Cannot inspect " + str(path)) from exc
    # Windows junctions and other reparse points are not necessarily symlinks.
    if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise BundleError("Links and reparse points are not allowed: " + str(path))
    if directory and not stat.S_ISDIR(info.st_mode):
        raise BundleError("Expected directory: " + str(path))
    if not directory and not stat.S_ISREG(info.st_mode):
        raise BundleError("Expected regular file: " + str(path))
    return info

def safe_root(root):
    # Inspect each existing ancestor as well as the selected bundle root.
    root = Path(os.path.abspath(root))
    for path in reversed((root, *root.parents)):
        check_path(path, directory=True)
    return root

def checked_file(root, relative):
    path = root
    parts = relative.split("/")
    for part in parts[:-1]:
        path = path / part
        check_path(path, directory=True)
    path = path / parts[-1]
    return path, check_path(path)

def same_file_stat(a, b):
    # Some platforms do not provide meaningful inode numbers. Compare them
    # when available. Windows lstat and fstat can report different ctime
    # for the same file, so compare ctime only between two fstat calls.
    if a.st_dev != b.st_dev or (a.st_ino and b.st_ino and a.st_ino != b.st_ino):
        return False
    return (a.st_size, a.st_mtime_ns) == (b.st_size, b.st_mtime_ns)

def read_checked(path, inspected, limit, expected_size=None):
    """Read at most limit + 1 bytes and detect common changes during the read."""
    with path.open("rb") as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or not same_file_stat(inspected, before):
            raise BundleError("File changed before reading: " + str(path))
        if expected_size is not None and before.st_size != expected_size:
            raise BundleError("Size mismatch: " + str(path))
        if before.st_size > limit:
            raise BundleError("File is too large: " + str(path))
        raw = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
    if len(raw) > limit:
        raise BundleError("File is too large: " + str(path))
    if len(raw) != before.st_size or not same_file_stat(before, after) or before.st_ctime_ns != after.st_ctime_ns:
        raise BundleError("File changed during reading: " + str(path))
    if not same_file_stat(after, check_path(path)):
        raise BundleError("File changed after reading: " + str(path))
    return raw
def verify(root, approved_hash):
    if not re.fullmatch(r"[0-9a-fA-F]{64}", approved_hash or ""):
        raise BundleError("A separately trusted manifest SHA-256 is required")
    root = safe_root(root)
    manifest, info = checked_file(root, "manifest.json")
    raw = read_checked(manifest, info, MAX_MANIFEST)
    if hashlib.sha256(raw).hexdigest() != approved_hash.lower():
        raise BundleError("Manifest SHA-256 does not match the supplied value")
    try:
        data = json.loads(raw.decode("utf-8"), object_pairs_hook=reject_duplicates)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise BundleError("Invalid UTF-8 JSON manifest") from exc
    if not isinstance(data, dict) or set(data) != {"format", "files"} or data["format"] != 1 or type(data["format"]) is not int:
        raise BundleError("Expected manifest format 1 and files only")
    entries = data["files"]
    if not isinstance(entries, dict) or set(entries) != set(FILES):
        raise BundleError("Manifest must list exactly the six allowed support files")
    plan = []
    for name, destination in FILES.items():
        entry = entries[name]
        if not isinstance(entry, dict) or set(entry) != {"size", "sha256"}:
            raise BundleError("Invalid entry: " + name)
        size, digest = entry["size"], entry["sha256"]
        if type(size) is not int or not 0 < size <= MAX_FILE or not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise BundleError("Invalid size or hash: " + name)
        path, info = checked_file(root, name)
        raw_file = read_checked(path, info, size, expected_size=size)
        if hashlib.sha256(raw_file).hexdigest() != digest:
            raise BundleError("SHA-256 mismatch: " + name)
        plan.append({"source": name, "destination": destination, "size": size, "sha256": digest})
    return plan

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", help="Path to extracted support bundle")
    parser.add_argument("--manifest-sha256", required=True, help="Manifest SHA-256 obtained separately from a trusted source")
    parser.add_argument("--install", action="store_true", help="Reserved; installation is currently unavailable")
    args = parser.parse_args(argv)
    try:
        plan = verify(args.bundle, args.manifest_sha256)
        if args.install:
            raise BundleError("Installation is unavailable in this preview; no network connection was made")
        print(json.dumps({"mode": "dry-run", "manifest_sha256": args.manifest_sha256.lower(), "files": plan}, indent=2))
        return 0
    except (BundleError, OSError) as exc:
        print("Error: " + str(exc), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
