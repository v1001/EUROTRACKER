#!/usr/bin/env python3
"""
Read EUROTRACKER .song files and print them as readable text.

Usage:
    py song_dump.py song1.song
    py song_dump.py *.song
    py song_dump.py extracted/*.song > songs.txt
"""

import os
import sys
import struct
import argparse
import glob

MAGIC = 0x534F4E47
MAX_SONG_LENGTH = 64
NUM_TRACKS = 6
NUM_MELODIC_TRACKS = 4
PATTERN_STEPS = 32

DIVIDERS = [
    ("x4", 12), ("x3", 16), ("x2", 24),
    ("x1.5", 32), ("x1", 48), ("/1.5", 64),
    ("/2", 96), ("/3", 144), ("/4", 192),
]

SCALES = [
    "Chromatic", "Major", "Nat. Min", "Pent. Maj", "Blues",
    "Harm. Min", "Melod. Min", "Dorian", "Phrygian", "Lydian",
    "Mixolydian", "Locrian", "Whole Tone", "Diminished",
    "Augmented", "Hungr. Min", "Lydian Dom", "Super Locr",
]

NOTE_NAMES = ["c", "C", "d", "D", "e", "f", "F", "g", "G", "a", "A", "b"]


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def u8(self):
        v = self.data[self.pos]
        self.pos += 1
        return v

    def u16(self):
        v = struct.unpack_from('<H', self.data, self.pos)[0]
        self.pos += 2
        return v

    def u32(self):
        v = struct.unpack_from('<I', self.data, self.pos)[0]
        self.pos += 4
        return v

    def raw(self, n):
        v = self.data[self.pos:self.pos + n]
        self.pos += n
        return v

    def skip(self, n):
        self.pos += n


def parse(path):
    with open(path, 'rb') as f:
        data = f.read()

    r = Reader(data)

    magic = r.u32()
    if magic != MAGIC:
        raise ValueError(f"bad magic 0x{magic:08x} (expected 0x{MAGIC:08x})")
    version = r.u8()
    r.skip(3)

    length = r.u32()
    if length > MAX_SONG_LENGTH:
        length = MAX_SONG_LENGTH

    dividers = [[0] * MAX_SONG_LENGTH for _ in range(NUM_TRACKS)]
    for t in range(NUM_TRACKS):
        for s in range(length):
            dividers[t][s] = r.u8()

    patterns = [[None] * MAX_SONG_LENGTH for _ in range(NUM_TRACKS)]
    for t in range(NUM_TRACKS):
        for s in range(length):
            steps = []
            for _ in range(PATTERN_STEPS):
                steps.append({
                    'cv':      r.u16(),
                    'on':      bool(r.u8() & 1),
                    'prob':    r.u8(),
                    'gate':    r.u8(),
                    'decay':   r.u8(),
                    'attack':  r.u8(),
                    'ratchet': r.u8(),
                    'micro':   r.u8(),
                })
            patterns[t][s] = steps

    plens = [[32] * MAX_SONG_LENGTH for _ in range(NUM_TRACKS)]
    for t in range(NUM_TRACKS):
        for s in range(length):
            n = r.u8()
            plens[t][s] = min(n, PATTERN_STEPS)

    quantizers = []
    for _ in range(4):
        enabled = r.u8() != 0
        num_notes = r.u8()
        notes = []
        for _ in range(num_notes):
            raw = r.raw(4)
            name = raw.split(b'\x00')[0].decode('ascii', 'replace')
            dac = r.u16()
            in_scale = r.u8() != 0
            notes.append({'name': name, 'dac': dac, 'in_scale': in_scale})
        quantizers.append({'enabled': enabled, 'notes': notes})

    cv_ranges = []
    for _ in range(NUM_MELODIC_TRACKS):
        cv_ranges.append((r.u16(), r.u16()))

    reset_flags = [r.u8() != 0 for _ in range(NUM_TRACKS)]
    swing = [r.u8() for _ in range(NUM_TRACKS)]

    q_ranges = []
    for _ in range(4):
        q_ranges.append((r.u16(), r.u16(), r.u8()))

    scale_root = []
    for _ in range(4):
        scale_root.append((r.u8(), r.u8()))

    return {
        'path':        path,
        'version':     version,
        'length':      length,
        'dividers':    dividers,
        'patterns':    patterns,
        'plens':       plens,
        'quantizers':  quantizers,
        'cv_ranges':   cv_ranges,
        'reset_flags': reset_flags,
        'swing':       swing,
        'q_ranges':    q_ranges,
        'scale_root':  scale_root,
    }


def dac_to_note(dac, q):
    if not q or not q['enabled'] or not q['notes']:
        return None
    best = None
    best_diff = 0xFFFF
    for n in q['notes']:
        if not n['in_scale']:
            continue
        diff = abs(n['dac'] - dac)
        if diff < best_diff:
            best_diff = diff
            best = n['name']
    return best


def print_legend():
    print("Legend:")
    print("  Div = clock divider for the step")
    print("  Prb = probability (0-99)")
    print("  Gat = gate length (0-99)")
    print("  Atk = attack (0-99)")
    print("  Dcy = decay (0-99)")
    print("  Rch = ratchet (1-4)")
    print("  Mcr = microtiming (0-99)")
    print("  X   = step enabled, . = step disabled")
    print()


def print_pattern(steps, plen, is_melodic, quantizer, indent='    '):
    if all(not steps[i]['on'] for i in range(plen)):
        print(f"{indent}(empty)")
        return

    print(f"{indent}#  On  Note          Prb Gat Atk Dcy Rch Mcr")
    for i in range(plen):
        st = steps[i]
        if not st['on']:
            print(f"{indent}{i + 1:02d} .")
            continue
        if is_melodic:
            note = dac_to_note(st['cv'], quantizer)
            if note:
                cv_str = f"{note:<4}({st['cv']:4d})"
            else:
                cv_str = f"    ({st['cv']:4d})"
        else:
            cv_str = "           "
        print(f"{indent}{i + 1:02d} X   {cv_str}  "
              f"{st['prob']:3d} {st['gate']:3d} {st['attack']:3d} "
              f"{st['decay']:3d} {st['ratchet']:3d} {st['micro']:3d}")


def print_song(song):
    print(f"=== {os.path.basename(song['path'])} ===")
    print(f"Format version: {song['version']}")
    print(f"Song length:    {song['length']}")
    print()

    for t in range(NUM_TRACKS):
        is_melodic = t < NUM_MELODIC_TRACKS
        kind = "CV/Gate" if is_melodic else "Gate"
        print(f"--- Track {t + 1} ({kind}) ---")
        print(f"  Reset: {'RESET' if song['reset_flags'][t] else 'KEEP'}"
              f"   Swing: {song['swing'][t]}%")

        if is_melodic:
            mn, mx = song['cv_ranges'][t]
            q = song['quantizers'][t]
            si, ri = song['scale_root'][t]
            scale_name = SCALES[si] if si < len(SCALES) else f"#{si}"
            root = NOTE_NAMES[ri] if ri < 12 else '?'
            q_state = 'ON' if q['enabled'] else 'OFF'
            print(f"  CV range: {mn}..{mx}")
            print(f"  Quantizer: {q_state}", end='')
            if q['enabled'] and q['notes']:
                in_scale_count = sum(1 for n in q['notes'] if n['in_scale'])
                print(f"   Scale: {root} {scale_name}"
                      f"   Notes: {len(q['notes'])} "
                      f"({in_scale_count} in scale)")
            else:
                print()
        print()

        for s in range(song['length']):
            div_idx = song['dividers'][t][s]
            plen = song['plens'][t][s]
            div_text = DIVIDERS[div_idx][0] if div_idx < len(DIVIDERS) else f"#{div_idx}"
            print(f"  Song step {s + 1:02d}   div={div_text:<4}   length={plen:2d}")
            steps = song['patterns'][t][s]
            q = song['quantizers'][t] if is_melodic else None
            print_pattern(steps, plen, is_melodic, q)
            print()

    print()


def main():
    parser = argparse.ArgumentParser(description='Print .song files as text.')
    parser.add_argument('files', nargs='+', help='one or more .song files')
    parser.add_argument('-n', '--no-legend', action='store_true',
                        help='skip the legend at the top')
    args = parser.parse_args()

    # Expand any glob patterns the shell did not expand
    paths = []
    for arg in args.files:
        expanded = glob.glob(arg)
        if expanded:
            paths.extend(expanded)
        else:
            paths.append(arg)

    if not paths:
        print('No files matched.', file=sys.stderr)
        sys.exit(1)

    if not args.no_legend:
        print_legend()

    for path in paths:
        try:
            song = parse(path)
            print_song(song)
        except Exception as e:
            print(f"ERROR: {path}: {e}", file=sys.stderr)


if __name__ == '__main__':
    main()