#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check tracked publication paths and local document/site links."""
from html.parser import HTMLParser
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
BLOCKED_SUFFIXES = {'.elf', '.bin', '.prx', '.pkg', '.dll', '.exe', '.mp4', '.mkv', '.zip', '.7z'}
PRIVATE = re.compile(r'(?i)(?:[cd]:[/\\](?:users|codex|downloads)[/\\]|/mnt/[a-z]/|192[.]168[.]|gh[pousr]_[A-Za-z0-9]{20,}|-----BEGIN [A-Z ]*PRIVATE KEY-----)')

class Site(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.links = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            if attrs['id'] in self.ids:
                raise ValueError('Duplicate HTML id: ' + attrs['id'])
            self.ids.add(attrs['id'])
        for key in ('href', 'src'):
            if key in attrs:
                self.links.append(attrs[key])

def main():
    try:
        names = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT, stderr=subprocess.DEVNULL).decode().split('\0')
    except (OSError, subprocess.CalledProcessError):
        names = [str(p.relative_to(ROOT)).replace('\\', '/') for p in ROOT.rglob('*')
                 if p.is_file() and not any(x in {'.git', '.build', '__pycache__', 'dist'} for x in p.parts)]
    names = [n for n in names if n]
    if not names:
        raise ValueError('No publication files found')
    errors = []
    for name in names:
        path = ROOT / name
        if path.suffix.lower() in BLOCKED_SUFFIXES:
            errors.append('Unexpected binary/archive: ' + name)
            continue
        if path.stat().st_size > 1024 * 1024:
            errors.append('Review file over 1 MiB: ' + name)
        content = path.read_text(encoding='utf-8')
        if name != 'tools/check_public_tree.py' and PRIVATE.search(content):
            errors.append('Private path/address/credential pattern: ' + name)
        if path.suffix == '.md':
            for target in re.findall(r'\]\(([^\s)]+)\)', content):
                link = urlsplit(target)
                if not link.scheme and link.path and not (path.parent / unquote(link.path)).is_file():
                    errors.append('Broken Markdown link: ' + name + ' -> ' + target)
    html = Site()
    html.feed((ROOT / 'site/index.html').read_text(encoding='utf-8'))
    for target in html.links:
        link = urlsplit(target)
        if link.scheme:
            if link.scheme != 'https':
                errors.append('Unexpected external link scheme: ' + target)
            continue
        if link.path and not (ROOT / 'site' / unquote(link.path)).is_file():
            errors.append('Missing site asset: ' + target)
        if not link.path and link.fragment and link.fragment not in html.ids:
            errors.append('Missing section: ' + target)
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'Checked {len(names)} publication files and {len(html.links)} site links.')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
