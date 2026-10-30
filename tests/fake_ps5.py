# SPDX-License-Identifier: MIT
"""An in-memory PS5 for testing gpulab.py: files and folders, a process list, a running game's sandbox (app0), and
executables that read back in another form (as the console serves a SELF)."""
import hashlib

from ps5_console import Refused, need


class Sparse:
    """A large game file of which only one byte range is known (enough for range reads)."""
    def __init__(self, size, at, data):
        self.size, self.at, self.data = size, at, data

    def __len__(self):
        return self.size


class FakePS5:
    def __init__(self):
        self.files, self.dirs = {}, {'/'}
        self.game = None             # title of the running game
        self.app0 = {}               # title -> {relative path: bytes}
        self.readback = {}           # sha256 of uploaded bytes -> bytes the console serves instead
        self.writes = 0

    # ---- set-up helpers
    def add_dir(self, path):
        parts = path.strip('/').split('/')
        for i in range(1, len(parts) + 1):
            self.dirs.add('/' + '/'.join(parts[:i]))

    def add_file(self, path, data):
        self.add_dir(path.rsplit('/', 1)[0] or '/')
        self.files[path] = bytes(data)

    def snapshot(self):
        return dict(self.files), set(self.dirs)

    # ---- the Console interface
    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def _served(self, path):
        if self.game and path.startswith(f'/mnt/sandbox/{self.game}_000/app0/'):
            rel = path[len(f'/mnt/sandbox/{self.game}_000/app0/'):]
            return self.app0[self.game].get(rel)
        data = self.files.get(path)
        if data is None:
            return None
        return self.readback.get(hashlib.sha256(data).hexdigest(), data)

    def processes(self):
        rows = [dict(pid=0, name='kernel'), dict(pid=1, name='SceShellUI')] + \
               [dict(pid=10 + i, name=f'Sce{i}') for i in range(60)]
        if self.game:
            rows.append(dict(pid=500, name='eboot.bin'))
        return rows

    def running(self):
        return ['eboot.bin'] if self.game else []

    def gate(self):
        need(not self.game, 'A game or app is running on the PS5: close it first')

    def listing(self, folder):
        folder = folder.rstrip('/') or '/'
        if self.game and folder.startswith(f'/mnt/sandbox/{self.game}_000/app0'):
            base = f'/mnt/sandbox/{self.game}_000/app0'
            rel = folder[len(base):].strip('/')
            out = {}
            for p, d in self.app0[self.game].items():
                if rel and not p.startswith(rel + '/'):
                    continue
                rest = p[len(rel) + 1:] if rel else p
                head = rest.split('/', 1)[0]
                out[head] = {'dir': '/' in rest, 'bytes': 0 if '/' in rest else len(d)}
            return out or None
        if folder not in self.dirs:
            return None
        out = {}
        prefix = folder.rstrip('/') + '/'
        for d in self.dirs:
            if d != folder and d.startswith(prefix) and '/' not in d[len(prefix):]:
                out[d[len(prefix):]] = {'dir': True, 'bytes': 0}
        for p in self.files:
            if p.startswith(prefix) and '/' not in p[len(prefix):]:
                out[p[len(prefix):]] = {'dir': False, 'bytes': len(self._served(p))}
        return out

    def stat(self, path):
        parent, name = path.rstrip('/').rsplit('/', 1)
        listing = self.listing(parent or '/')
        return None if listing is None else listing.get(name)

    def exists(self, path):
        return self.stat(path) is not None

    def read(self, path, cap, progress=None):
        data = self._served(path)
        if data is None or isinstance(data, Sparse):
            raise Refused('no such file ' + path)
        need(len(data) <= cap, 'Unexpectedly large file: ' + path)
        return data

    def read_range(self, path, offset, length):
        data = self._served(path)
        need(data is not None and offset + length <= len(data), 'Short read of ' + path)
        if isinstance(data, Sparse):
            need(offset == data.at and length == len(data.data), 'Unknown range of ' + path)
            return data.data
        return data[offset:offset + length]

    def hash(self, path, cap):
        data = self.read(path, cap)
        return len(data), hashlib.sha256(data).hexdigest()

    def mkdir(self, path):
        self.gate()
        need(path.rsplit('/', 1)[0] in self.dirs or path.count('/') == 1, 'no parent for ' + path)
        need(path not in self.dirs and path not in self.files, 'exists ' + path)
        self.dirs.add(path)

    def write(self, path, data):
        self.gate()
        need(path.rsplit('/', 1)[0] in self.dirs, 'no folder for ' + path)
        self.files[path] = bytes(data)
        self.writes += 1

    def rename(self, old, new):
        self.gate()
        need(not self.exists(new), 'Rename target exists: ' + new)
        need(new.rsplit('/', 1)[0] in self.dirs, 'no folder for ' + new)
        if old in self.files:
            self.files[new] = self.files.pop(old)
            return
        need(old in self.dirs, 'no such path ' + old)
        for d in sorted(x for x in self.dirs if x == old or x.startswith(old + '/')):
            self.dirs.discard(d)
            self.dirs.add(new + d[len(old):])
        for p in [x for x in self.files if x.startswith(old + '/')]:
            self.files[new + p[len(old):]] = self.files.pop(p)

    def delete(self, path):
        self.gate()
        need(path in self.files, 'no such file ' + path)
        del self.files[path]

    def rmdir(self, path):
        self.gate()
        need(not any(p.startswith(path + '/') for p in list(self.files) + list(self.dirs)), 'not empty ' + path)
        self.dirs.discard(path)
