# SPDX-License-Identifier: MIT
"""FTP access to one jailbroken PS5 (ftpsrv on port 2121, the payload manager pldmgr on port 8084).

Writes need a fresh, complete process list with no game or app running. Reads of a running game's files (to build
HelixSR from your own copy) are allowed while it runs; nothing is ever written there.
"""
import ftplib
import hashlib
import io
import json
import urllib.request
from pathlib import Path


class Refused(RuntimeError):
    """The console is not in a state this tool will touch; nothing was changed by the refused step."""


def need(condition, message):
    if not condition:
        raise Refused(message)


class Console:
    def __init__(self, host, ftp_port=2121, pldmgr_port=8084, timeout=60):
        self.host, self.ftp_port, self.pldmgr_port, self.timeout = host, ftp_port, pldmgr_port, timeout
        self.ftp = None

    # ---- connection
    def _connect(self):
        ftp = ftplib.FTP()
        ftp.connect(self.host, self.ftp_port, timeout=self.timeout)
        ftp.login()
        ftp.voidcmd('TYPE I')
        return ftp

    def __enter__(self):
        try:
            self.ftp = self._connect()
        except OSError as e:
            raise Refused(f'Cannot reach the FTP server on {self.host}:{self.ftp_port} ({e}). Load ftpsrv on the PS5 '
                          'and check the IP address (Settings > Network > Connection Status).')
        return self

    def __exit__(self, *exc):
        try:
            self.ftp.quit()
        except (OSError, ftplib.Error):
            self.ftp.close()

    # ---- processes
    def processes(self):
        req = urllib.request.Request(f'http://{self.host}:{self.pldmgr_port}/processes_list',
                                     headers={'X-PLDMGR-Request': '1', 'Cache-Control': 'no-cache'})
        try:
            with urllib.request.urlopen(req, timeout=10) as r:
                raw = r.read(262145)
        except OSError as e:
            raise Refused(f'Cannot reach the payload manager (pldmgr) on {self.host}:{self.pldmgr_port} ({e}). '
                          'Load pldmgr on the PS5.')
        need(len(raw) <= 262144, 'Oversized process list')
        try:
            document = json.loads(raw)
        except (ValueError, UnicodeError):
            raise Refused('The payload manager returned an invalid process list') from None
        need(isinstance(document, dict) and isinstance(document.get('processes'), list),
             'The payload manager returned an invalid process list')
        return document['processes']

    def running(self):
        """Names of running games/apps, after checking the list is a full console list."""
        rows = self.processes()
        need(isinstance(rows, list) and 40 <= len(rows) <= 2048 and
             all(isinstance(p, dict) and type(p.get('pid')) is int and p['pid'] >= 0 and
                 isinstance(p.get('name'), str) and p['name'] and '\0' not in p['name'] for p in rows),
             'The payload manager returned a partial process list')
        names = {p['pid']: p['name'] for p in rows}
        need(len(names) == len(rows), 'The payload manager returned duplicate process IDs')
        need(names.get(0) == 'kernel' and 'SceShellUI' in names.values(), 'The process list is not a full console list')
        return [p['name'] for p in rows if 'eboot' in p['name'].casefold()]

    def gate(self):
        running = self.running()
        need(not running, 'A game or app is running on the PS5: close it first')

    # ---- reads
    def listing(self, folder):
        """{name: {'dir': bool, 'bytes': int}}, or None when the folder does not exist."""
        lines = []
        try:
            self.ftp.retrlines('LIST ' + folder, lines.append)
        except ftplib.error_perm:
            return None
        out = {}
        for line in lines:
            f = line.split(maxsplit=8)
            need(len(f) == 9 and len(f[0]) == 10 and f[0][0] in ('d', '-') and f[4].isdigit(),
                 'Unrecognized FTP directory entry in ' + folder)
            if f[8] not in ('.', '..'):
                need(f[8] not in out and '/' not in f[8], 'Ambiguous FTP directory entry in ' + folder)
                out[f[8]] = {'dir': f[0][0] == 'd', 'bytes': int(f[4])}
        return out

    def stat(self, path):
        """{'dir', 'bytes'} or None."""
        parent, name = path.rstrip('/').rsplit('/', 1)
        parent = parent or '/'
        listing = self.listing(parent)
        if listing is None:
            if parent == '/':
                raise Refused('Cannot inspect the console root directory')
            ancestor = self.stat(parent)
            need(ancestor is None, 'Cannot inspect directory: ' + parent)
            return None
        return listing.get(name)

    def exists(self, path):
        return self.stat(path) is not None

    def read(self, path, cap, progress=None):
        data = bytearray()

        def got(chunk):
            need(len(data) + len(chunk) <= cap, 'Unexpectedly large file: ' + path)
            data.extend(chunk)
            if progress:
                progress(len(data))
        self.ftp.retrbinary('RETR ' + path, got, blocksize=262144)
        return bytes(data)

    def read_raw_self(self, path, cap, progress=None):
        """Read the owner's native SELF on a dedicated connection; leave normal ELF readback unchanged."""
        ftp = self._connect()
        data = bytearray()
        try:
            need('disabled' in ftp.sendcmd('SELF').lower(), 'FTP did not disable SELF conversion')
            def got(chunk):
                need(len(data) + len(chunk) <= cap, 'Unexpectedly large native SELF: ' + path)
                data.extend(chunk)
                if progress:
                    progress(len(data))
            ftp.retrbinary('RETR ' + path, got, blocksize=262144)
            need(data[:4] in (bytes.fromhex('5414f5ee'), bytes.fromhex('4f153d1d')),
                 'FTP did not return a native SELF')
            return bytes(data)
        finally:
            ftp.close()

    def read_range(self, path, offset, length):
        """Bytes [offset, offset + length) of a file, on a separate connection (the transfer is cut short)."""
        ftp = self._connect()
        try:
            conn = ftp.transfercmd('RETR ' + path, rest=offset)
            try:
                data = bytearray()
                while len(data) < length:
                    block = conn.recv(min(262144, length - len(data)))
                    if not block:
                        break
                    data.extend(block)
            finally:
                conn.close()
        finally:
            ftp.close()
        need(len(data) == length, 'Short read of ' + path)
        return bytes(data)

    def hash(self, path, cap):
        h, n = hashlib.sha256(), [0]

        def got(chunk):
            n[0] += len(chunk)
            need(n[0] <= cap, 'Unexpectedly large file: ' + path)
            h.update(chunk)
        self.ftp.retrbinary('RETR ' + path, got, blocksize=262144)
        return n[0], h.hexdigest()

    # ---- writes (each behind the gate)
    def mkdir(self, path):
        self.gate()
        self.ftp.mkd(path)

    def write(self, path, source):
        self.gate()
        with (io.BytesIO(source) if isinstance(source, (bytes, bytearray)) else Path(source).open('rb')) as f:
            self.ftp.storbinary('STOR ' + path, f, blocksize=262144)
        try:
            self.ftp.sendcmd('SITE CHMOD 777 ' + path)
        except ftplib.Error:
            pass

    def rename(self, old, new):
        self.gate()
        need(not self.exists(new), 'Rename target exists: ' + new)
        self.ftp.rename(old, new)

    def delete(self, path):
        self.gate()
        try:
            self.ftp.delete(path)
        except ftplib.error_reply as exc:
            # ftpsrv 0.21.1 reports successful DELE with 226; ftplib expects 200/250.
            if not str(exc).startswith('226 '):
                raise
            need(not self.exists(path), 'FTP reported deletion but the file remains: ' + path)

    def rmdir(self, path):
        self.gate()
        self.ftp.rmd(path)
