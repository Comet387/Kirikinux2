#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Generate Kirikinux2's 32x32 arrow cursor using the engine's BMP CUR format."""
from pathlib import Path
import struct


def inside(x, y):
    polygon = [(1, 1), (1, 25), (7, 19), (12, 29), (17, 26), (12, 17), (22, 17)]
    hit = False
    previous = polygon[-1]
    for point in polygon:
        x1, y1 = previous
        x2, y2 = point
        if (y1 > y) != (y2 > y) and x < (x2-x1)*(y-y1)/(y2-y1)+x1:
            hit = not hit
        previous = point
    return hit


def generate():
    pixels, mask = bytearray(), bytearray()
    for y in reversed(range(32)):
        row = 0
        for x in range(32):
            opaque = inside(x+.5, y+.5)
            outline = opaque and not all(inside(x+.5+dx, y+.5+dy)
                                         for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            color = 0 if outline else 255
            pixels.extend((color, color, color, 255 if opaque else 0))
            row = (row << 1) | int(not opaque)
        mask.extend(row.to_bytes(4, 'big'))
    bitmap = struct.pack('<IiiHHIIiiII', 40, 32, 64, 1, 32, 0, len(pixels), 0, 0, 0, 0)
    image = bitmap + pixels + mask
    header = struct.pack('<HHH', 0, 2, 1)
    directory = struct.pack('<BBBBHHII', 32, 32, 0, 0, 1, 1, len(image), 22)
    target = Path(__file__).resolve().parents[2] / 'cocos/kr2/Resources/default.cur'
    target.write_bytes(header + directory + image)
    print(target)


if __name__ == '__main__':
    generate()
