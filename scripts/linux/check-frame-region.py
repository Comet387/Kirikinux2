#!/usr/bin/env python3
"""Compare an unchanged screen region across real gui-smoke RGB captures.

This reader accepts the exact PNG format emitted by gui-smoke.py (RGB8, filter
zero). It neither synthesizes nor changes frames. Use only where the game is
expected to leave that region unchanged, e.g. the bedroom above the text box.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def pixels(path, region):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not a PNG: ' + str(path))
    pos, blocks = 8, []
    while pos < len(data):
        length = struct.unpack_from('>I', data, pos)[0]
        kind, chunk = data[pos+4:pos+8], data[pos+8:pos+8+length]
        expected = struct.unpack_from('>I', data, pos+8+length)[0]
        if zlib.crc32(kind + chunk) != expected:
            raise ValueError('PNG CRC mismatch: ' + str(path))
        if kind == b'IHDR':
            width, height, depth, color, comp, filt, interlace = struct.unpack('>IIBBBBB', chunk)
            if (depth, color, comp, filt, interlace) != (8, 2, 0, 0, 0):
                raise ValueError('Expected gui-smoke RGB8 PNG')
        elif kind == b'IDAT':
            blocks.append(chunk)
        pos += 12 + length
    raw = zlib.decompress(b''.join(blocks))
    stride = width * 3 + 1
    if len(raw) != stride * height:
        raise ValueError('PNG payload size mismatch')
    left, top, right, bottom = region
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        raise ValueError('Region is outside frame')
    rows = []
    for y in range(top, bottom):
        if raw[y * stride] != 0:
            raise ValueError('Expected filter zero from gui-smoke')
        rows.append(raw[y*stride+1+left*3:y*stride+1+right*3])
    return b''.join(rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--report', type=Path, required=True)
    ap.add_argument('--reference', required=True)
    ap.add_argument('--after', type=float, required=True)
    ap.add_argument('--before', type=float, default=float('inf'))
    ap.add_argument('--region', required=True, help='left:top:right:bottom')
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    region = [int(x) for x in args.region.split(':')]
    if len(region) != 4:
        ap.error('region needs four coordinates')
    root = args.report.parent
    reference = pixels(root / args.reference, region)
    if not any(reference):
        raise ValueError('Reference region is entirely black')
    results = []
    for frame in json.loads(args.report.read_text())['captures']:
        if frame['time'] < args.after or frame['time'] >= args.before:
            continue
        sample = pixels(root / frame['file'], region)
        results.append({'frame': frame['file'], 'time': frame['time'],
                        'identical': sample == reference,
                        'sha256': hashlib.sha256(sample).hexdigest()})
    result = {'region': region, 'reference': args.reference,
              'reference_sha256': hashlib.sha256(reference).hexdigest(),
              'compared_frames': results,
              'passed': bool(results) and all(f['identical'] for f in results)}
    output = json.dumps(result, indent=2) + '\n'
    print(output, end='')
    if args.output:
        args.output.write_text(output)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
