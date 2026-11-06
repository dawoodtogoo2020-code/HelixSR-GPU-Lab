# SPDX-License-Identifier: MIT
"""Guided setup of the single-load Lite + A53 payload; never injects payloads."""
import json
from pathlib import Path

from ps5_console import Refused, need
from . import integrate
from .build_bridge import REFERENCE_BYTES, REFERENCE_SHA256

PAYLOAD_ROOT = '/data/pldmgr/payloads'
SLOT = 'local/gpulab-kstuff-lite-a53-1160'
FILENAME = 'Kstuff-Lite-v1.11-A53-11.60.elf'
OUTPUT_SHA256 = 'd02d0cd18d422ad63af0e573a040f8a26284320ce6a6f259bd230f8383de40a7'
OUTPUT_BYTES = 3592568
BUNDLED_FILE = Path(__file__).resolve().parents[2] / 'payloads' / 'lite1160' / FILENAME


def read_local(path, size, digest, label):
    path = Path(str(path).strip().strip('"').strip("'"))
    need(path.is_file() and path.stat().st_size == size,
         f'{label}: select the exact original ELF file ({size:,} bytes).')
    raw = path.read_bytes()
    need(integrate.sha(raw) == digest, f'{label}: this file is a different build; no console files were changed.')
    return raw


def find_input(c, size, digest):
    """Search only the payload manager's two bounded, known slot directories."""
    for category in ('local', 'repo'):
        parent = PAYLOAD_ROOT + '/' + category
        rows = c.listing(parent)
        if rows is None:
            continue
        need(len(rows) <= 512, 'Payload list is too large to inspect safely.')
        for name, info in sorted(rows.items()):
            if not info['dir']:
                continue
            folder = parent + '/' + name
            files = c.listing(folder)
            need(files is not None and len(files) <= 128, 'Cannot inspect payload folder: ' + folder)
            for filename, entry in sorted(files.items()):
                if entry['dir'] or not filename.lower().endswith('.elf') or entry['bytes'] != size:
                    continue
                raw = c.read(folder + '/' + filename, size + 1)
                if len(raw) == size and integrate.sha(raw) == digest:
                    return raw
    return None


def acquire(c, ui, explicit, size, digest, label):
    if explicit:
        return read_local(explicit, size, digest, label)
    raw = find_input(c, size, digest)
    if raw is not None:
        print(f'  Found verified {label} in Payload Manager.', flush=True)
        return raw
    path = ui.ask(f'{label} was not found on the PS5. Drag the original ELF file here '
                  '(or paste its full path); Enter stops setup:')
    need(bool(path), f'{label} is required. Obtain the matching build from its maintainer, '
         'then run setup again. Existing payloads have not been changed.')
    return read_local(path, size, digest, label)


def prepare(c, ui, test5=None, lite=None):
    """Verify/build locally before the main installer changes console files."""
    rebuilding = bool(test5 or lite)
    if rebuilding:
        reference = acquire(c, ui, test5, REFERENCE_BYTES, REFERENCE_SHA256, 'Test5 A53 reference')
        original_lite = acquire(c, ui, lite, integrate.LITE_BYTES, integrate.LITE_SHA256, 'K-Stuff Lite v1.11')
        payload, receipt = integrate.build(reference, original_lite)
        receipt['payload_source'] = 'explicit local rebuild'
    else:
        need(BUNDLED_FILE.is_file(),
             'The bundled Lite + A53 payload is missing. Download and extract the complete GPU Lab release ZIP, '
             'then run setup-kstuff-lite again. The source ZIP does not include console binaries.')
        payload = read_local(BUNDLED_FILE, OUTPUT_BYTES, OUTPUT_SHA256, 'Bundled Lite + A53 payload')
        receipt = {'format': 1, 'firmware': '11.60 only', 'output_sha256': OUTPUT_SHA256,
                   'output_bytes': OUTPUT_BYTES, 'payload_source': 'bundled release',
                   'separate_bridge_required': False}
        print('  Verified bundled payload. No Test5 file or build is needed.', flush=True)
    need(integrate.sha(payload) == OUTPUT_SHA256, 'The integrated output did not match the qualified build.')
    return payload, receipt, rebuilding


def preflight(c, journal, helpers, prepared):
    payload, _, _ = prepared
    need(integrate.sha(payload) == OUTPUT_SHA256, 'The prepared payload did not match the qualified build.')
    journal.data.setdefault('lite_setup_utc', helpers.now())
    journal.save()
    meta = {
        'schema': 2, 'kind': 'local', 'filename': FILENAME,
        'installed_at': journal.data['lite_setup_utc'], 'origin': 'manual',
        'origin_detail': 'GPU Lab integrated Lite v1.11 + A53; FW11.60 only. '
                         'Load once instead of plain Lite, Test5 or the separate bridge. '
                         'Rest-mode/resume unqualified. SHA256 ' + OUTPUT_SHA256,
    }
    files = [(FILENAME, payload), ('meta.json', (json.dumps(meta, indent=2) + '\n').encode())]
    for name, data in files:
        helpers.preflight_put(c, PAYLOAD_ROOT + '/' + SLOT + '/' + name,
                              data, [(len(data), integrate.sha(data))])
    return files


def install(c, journal, ui, helpers, test5=None, lite=None, prepared=None):
    c.gate()
    print('Installing K-Stuff Lite + A53 for firmware 11.60.\n'
          'This installs a new Local entry. Existing payloads and autoload settings are preserved.', flush=True)
    prepared = prepared if prepared is not None else prepare(c, ui, test5, lite)
    payload, receipt, rebuilding = prepared
    receipt = dict(receipt)
    files = preflight(c, journal, helpers, prepared)
    local = journal.dir / 'lite1160'
    local.mkdir(parents=True, exist_ok=True)
    if rebuilding:
        output = local / FILENAME
        if output.exists():
            need(output.is_file() and integrate.sha(output.read_bytes()) == OUTPUT_SHA256,
                 'A different file occupies the local output path; move it before trying again.')
        else:
            output.write_bytes(payload)
        need(integrate.sha(output.read_bytes()) == OUTPUT_SHA256, 'Local output verification failed.')
    for name, data in files:
        helpers.put(c, journal, PAYLOAD_ROOT + '/' + SLOT + '/' + name,
                    data, [(len(data), integrate.sha(data))])
    receipt['console_slot'] = SLOT
    receipt['installed_utc'] = helpers.now()
    receipt['injected'] = False
    (local / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('\nInstalled and verified: ' + FILENAME + '\n'
          'Current boot: do not load it over an existing K-Stuff.\n'
          'Next full restart: jailbreak -> ShadowMount -> this integrated Local payload once.\n'
          'No separate A53 bridge or second K-Stuff is needed.\n'
          'To undo the files installed by GPU Lab, use its normal uninstall command.\n'
          'Rest-mode/resume has not been qualified.', flush=True)
    return receipt


def run(args, ui, helpers):
    host = helpers.connect(args, ui)
    journal = helpers.Journal(host)
    with helpers.CONSOLE(host) as c:
        helpers.wait_closed(c, ui)
        install(c, journal, ui, helpers, getattr(args, 'test5', None), getattr(args, 'lite', None))
    return 0
