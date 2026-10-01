#!/usr/bin/env python3
"""
Convert EUROTRACKER .song files to MIDI.

For each song file, one MIDI file is generated per track:
    <song_base>_track_<n>.mid

Usage:
    py song_to_midi.py song1.song
    py song_to_midi.py extracted/*.song
    py song_to_midi.py song1.song --bpm 140 --out midi/
"""

import os
import sys
import glob
import struct
import argparse
from pathlib import Path

# ---------- Constants matching the firmware ----------

MAGIC = 0x534F4E47
MAX_SONG_LENGTH = 64
NUM_TRACKS = 6
NUM_MELODIC_TRACKS = 4
PATTERN_STEPS = 32

# Divider table: ticks per step at 192 PPQN
DIVIDER_TICKS = [12, 16, 24, 32, 48, 64, 96, 144, 192]

PITCH_CLASS = {
    'c': 0, 'C': 1, 'd': 2, 'D': 3, 'e': 4, 'f': 5,
    'F': 6, 'g': 7, 'G': 8, 'a': 9, 'A': 10, 'b': 11,
}

SEQUENCER_PPQN = 192
DEFAULT_MIDI_PPQ = 960
MIDI_MULT = DEFAULT_MIDI_PPQ // SEQUENCER_PPQN   # 5

# Gate-only track MIDI notes (drums)
GATE_TRACK_NOTES = {4: 36, 5: 38}   # track 5 -> kick, track 6 -> snare


# ---------- .song parser ----------

class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def u8(self):
        v = self.data[self.pos]; self.pos += 1; return v

    def u16(self):
        v = struct.unpack_from('<H', self.data, self.pos)[0]
        self.pos += 2; return v

    def u32(self):
        v = struct.unpack_from('<I', self.data, self.pos)[0]
        self.pos += 4; return v

    def raw(self, n):
        v = self.data[self.pos:self.pos + n]; self.pos += n; return v

    def skip(self, n):
        self.pos += n


def parse_song(path):
    with open(path, 'rb') as f:
        data = f.read()
    r = Reader(data)

    if r.u32() != MAGIC:
        raise ValueError(f"not a EUROTRACKER song: {path}")
    version = r.u8()
    r.skip(3)
    length = min(r.u32(), MAX_SONG_LENGTH)

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
            plens[t][s] = min(r.u8(), PATTERN_STEPS)

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

    cv_ranges = [(r.u16(), r.u16()) for _ in range(4)]
    reset_flags = [r.u8() != 0 for _ in range(NUM_TRACKS)]
    swing = [r.u8() for _ in range(NUM_TRACKS)]
    q_ranges = [(r.u16(), r.u16(), r.u8()) for _ in range(4)]
    scale_root = [(r.u8(), r.u8()) for _ in range(4)]

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


# ---------- Pitch mapping ----------

def note_name_to_midi(name):
    if not name or len(name) < 2:
        return 60
    pc = PITCH_CLASS.get(name[0], 0)
    try:
        octave = int(name[1:])
    except ValueError:
        octave = 4
    midi = (octave + 1) * 12 + pc
    return max(0, min(127, midi))


def cv_to_midi_note(cv, quantizer):
    if quantizer and quantizer['enabled'] and quantizer['notes']:
        best = None
        best_diff = 0xFFFF
        for n in quantizer['notes']:
            if not n['in_scale']:
                continue
            diff = abs(n['dac'] - cv)
            if diff < best_diff:
                best_diff = diff
                best = n['name']
        if best:
            return note_name_to_midi(best)
    # Linear fallback: 0-4095 -> 0-127
    return max(0, min(127, int(cv * 127 / 4095)))


# ---------- Gate duration (firmware mapping, in MIDI ticks) ----------

def gate_duration_ticks(step_ticks, gate_percent):
    half_step = step_ticks / 2.0
    if gate_percent == 0:
        return 1
    if gate_percent <= 32:
        return max(1, int(gate_percent * half_step / 32))
    if gate_percent <= 96:
        return max(1, int(half_step + (gate_percent - 32) * half_step))
    if gate_percent == 97:
        return max(1, int(128 * half_step))
    if gate_percent == 98:
        return max(1, int(256 * half_step))
    return max(1, int(512 * half_step))


# ---------- Raw MIDI writer ----------

def var_len(v):
    if v < 0:
        v = 0
    buf = [v & 0x7F]
    v >>= 7
    while v:
        buf.insert(0, 0x80 | (v & 0x7F))
        v >>= 7
    return bytes(buf)


def tempo_event(bpm):
    us_per_qn = int(60_000_000 / bpm)
    return b'\xff\x51\x03' + us_per_qn.to_bytes(3, 'big')


def time_sig_event(numerator, denominator):
    import math
    den_log = int(math.log2(denominator))
    return b'\xff\x58\x04' + bytes([numerator, den_log, 24, 8])


def note_on(ch, note, vel=100):
    return bytes([0x90 | (ch & 0x0F), note & 0x7F, vel & 0x7F])


def note_off(ch, note):
    return bytes([0x80 | (ch & 0x0F), note & 0x7F, 0])


def build_track(events):
    events.sort(key=lambda e: (e[0], e[1][0]))
    out = bytearray()
    last = 0
    for t, data in events:
        out += var_len(t - last)
        out += data
        last = t
    out += var_len(0)
    out += b'\xff\x2f\x00'
    return bytes(out)


def write_midi_file(path, track_data, ppq):
    with open(path, 'wb') as f:
        f.write(b'MThd')
        f.write(struct.pack('>I', 6))
        f.write(struct.pack('>H', 1))
        f.write(struct.pack('>H', 1))
        f.write(struct.pack('>H', ppq))
        f.write(b'MTrk')
        f.write(struct.pack('>I', len(track_data)))
        f.write(track_data)


# ---------- Track rendering ----------

def render_track(song, track_idx, bpm, ppq):
    is_melodic = track_idx < NUM_MELODIC_TRACKS
    channel = track_idx if is_melodic else 9

    mult = ppq // SEQUENCER_PPQN

    events = []
    events.append((0, tempo_event(bpm)))
    events.append((0, time_sig_event(4, 4)))

    tick = 0
    for song_step in range(song['length']):
        # Song-step duration = max across all tracks
        song_step_seqticks = 0
        for t in range(NUM_TRACKS):
            div_idx = song['dividers'][t][song_step]
            div_val = DIVIDER_TICKS[div_idx]
            plen = song['plens'][t][song_step]
            total = div_val * plen
            if total > song_step_seqticks:
                song_step_seqticks = total

        song_step_midi = song_step_seqticks * mult
        song_step_end = tick + song_step_midi

        div_idx = song['dividers'][track_idx][song_step]
        div_val = DIVIDER_TICKS[div_idx]
        step_midi = div_val * mult

        plen = song['plens'][track_idx][song_step]
        pattern = song['patterns'][track_idx][song_step]

        swing = song['swing'][track_idx]
        quantizer = song['quantizers'][track_idx] if is_melodic else None

        pos = tick
        while pos < song_step_end:
            for step_idx in range(plen):
                if pos >= song_step_end:
                    break
                st = pattern[step_idx]
                if st['on']:
                    ratchet = max(1, st['ratchet'])

                    offset = 0
                    if st['micro'] > 0:
                        offset = (step_midi * st['micro']) // (100 * ratchet)
                    elif step_idx % 2 == 1 and swing > 0:
                        offset = (step_midi * swing) // 200

                    hit_base = pos + offset

                    if is_melodic:
                        note = cv_to_midi_note(st['cv'], quantizer)
                    else:
                        note = GATE_TRACK_NOTES.get(track_idx, 36)

                    gate_ticks = gate_duration_ticks(step_midi, st['gate'])

                    on_t = min(hit_base, song_step_end - 1)
                    events.append((on_t, note_on(channel, note)))
                    events.append((on_t + gate_ticks, note_off(channel, note)))

                    if ratchet > 1:
                        spacing = step_midi // ratchet
                        for h in range(1, ratchet):
                            ht = pos + h * spacing
                            if ht >= song_step_end:
                                break
                            hg = max(1, spacing // 2)
                            events.append((ht, note_on(channel, note)))
                            events.append((ht + hg, note_off(channel, note)))

                pos += step_midi

        tick = song_step_end

    return build_track(events)


# ---------- Main ----------

def convert(song_path, bpm, out_dir, ppq):
    song = parse_song(song_path)
    base = Path(song_path).stem

    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"{song_path}  (version {song['version']}, "
          f"length {song['length']}, BPM {bpm})")

    for t in range(NUM_TRACKS):
        track_data = render_track(song, t, bpm, ppq)
        out_name = f"{base}_track_{t + 1}.mid"
        out_path = out_dir / out_name
        write_midi_file(out_path, track_data, ppq)
        kind = "CV/Gate" if t < NUM_MELODIC_TRACKS else "Gate"
        print(f"  -> {out_name}  ({kind}, {len(track_data)} bytes)")


def main():
    parser = argparse.ArgumentParser(
        description="Convert EUROTRACKER .song files to MIDI.")
    parser.add_argument('files', nargs='+', help='one or more .song files')
    parser.add_argument('--bpm', type=float, default=120.0,
                        help='tempo for the export (default 120)')
    parser.add_argument('--ppq', type=int, default=DEFAULT_MIDI_PPQ,
                        help=f'MIDI ticks per quarter note '
                             f'(default {DEFAULT_MIDI_PPQ}, must be a multiple of 192)')
    parser.add_argument('--out', default='.',
                        help='output directory (default: current folder)')
    args = parser.parse_args()

    if args.ppq % SEQUENCER_PPQN != 0:
        print(f"ERROR: --ppq must be a multiple of {SEQUENCER_PPQN}",
              file=sys.stderr)
        sys.exit(1)

    paths = []
    for arg in args.files:
        expanded = glob.glob(arg)
        paths.extend(expanded if expanded else [arg])

    if not paths:
        print('No files matched.', file=sys.stderr)
        sys.exit(1)

    for path in paths:
        try:
            convert(path, args.bpm, args.out, args.ppq)
        except Exception as e:
            print(f"ERROR: {path}: {e}", file=sys.stderr)


if __name__ == '__main__':
    main()