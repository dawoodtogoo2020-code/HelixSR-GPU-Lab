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
        return json.loads(raw)['processes']

    def running(self):
        """Names of running games/apps, after checking the list is a full console list."""
        rows = self.processes()
        need(isinstance(rows, list) and 40 <= len(rows) <= 2048 and
             all(isinstance(p, dict) and isinstance(p.get('name'), str) for p in rows),
             'The payload manager returned a partial process list')
        names = {p.get('pid'): p['name'] for p in rows}
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
            if len(f) == 9 and len(f[0]) == 10 and f[8] not in ('.', '..'):
                out[f[8]] = {'dir': f[0][0] == 'd', 'bytes': int(f[4])}
        return out

    def stat(self, path):
        """{'dir', 'bytes'} or None."""
        parent, name = path.rstrip('/').rsplit('/', 1)
        listing = self.listing(parent or '/')
        return None if listing is None else listing.get(name)

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
        self.ftp.delete(path)

    def rmdir(self, path):
        self.gate()
        self.ftp.rmd(path)
