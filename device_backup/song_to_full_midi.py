#!/usr/bin/env python3
"""
Convert a EUROTRACKER .song file to a single multi-track MIDI file
using General MIDI instruments.

Interactively prompts for an instrument assignment per track.

Output:
    <song_base>_full.mid

Usage:
    py song_to_full_midi.py song1.song
    py song_to_full_midi.py song1.song --bpm 140 --ppq 960 --out midi/
"""

import os
import sys
import glob
import struct
import argparse
from pathlib import Path

# ---------- Firmware constants ----------

MAGIC = 0x534F4E47
MAX_SONG_LENGTH = 64
NUM_TRACKS = 6
NUM_MELODIC_TRACKS = 4
PATTERN_STEPS = 32

DIVIDER_TICKS = [12, 16, 24, 32, 48, 64, 96, 144, 192]

PITCH_CLASS = {
    'c': 0, 'C': 1, 'd': 2, 'D': 3, 'e': 4, 'f': 5,
    'F': 6, 'g': 7, 'G': 8, 'a': 9, 'A': 10, 'b': 11,
}

SEQUENCER_PPQN = 192
DEFAULT_MIDI_PPQ = 960


# ---------- General MIDI ----------

# Curated list shown to the user before prompting.
GM_SHORTLIST = [
    (0,  "Acoustic Grand Piano"),
    (4,  "Electric Piano 1"),
    (6,  "Harpsichord"),
    (11, "Vibraphone"),
    (16, "Drawbar Organ"),
    (24, "Nylon Guitar"),
    (25, "Steel Guitar"),
    (30, "Distortion Guitar"),
    (32, "Acoustic Bass"),
    (33, "Electric Bass (finger)"),
    (37, "Slap Bass 1"),
    (38, "Synth Bass 1"),
    (39, "Synth Bass 2"),
    (46, "Orchestra Hit"),
    (48, "String Ensemble 1"),
    (50, "Synth Strings 1"),
    (52, "Choir Aahs"),
    (56, "Trumpet"),
    (57, "Trombone"),
    (60, "French Horn"),
    (65, "Alto Sax"),
    (73, "Flute"),
    (80, "Lead 1 (square)"),
    (81, "Lead 2 (sawtooth)"),
    (87, "Lead 8 (bass+lead)"),
    (88, "Pad 1 (new age)"),
    (89, "Pad 2 (warm)"),
    (90, "Pad 3 (polysynth)"),
    (94, "Pad 7 (halo)"),
    (98, "FX 1 (rain)"),
    (103,"FX 8 (sci-fi)"),
]

GM_NAMES = {}
for n, name in GM_SHORTLIST:
    GM_NAMES[n] = name

NAME_ALIASES = {
    "piano":      0,
    "epiano":     4,
    "e-piano":    4,
    "organ":      16,
    "guitar":     25,
    "bass":       33,
    "synthbass":  38,
    "strings":    48,
    "pad":        89,
    "lead":       80,
    "square":     80,
    "saw":        81,
    "trumpet":    56,
    "sax":        65,
    "flute":      73,
    "vibes":      11,
    "drums":      -1,   # special
    "drum":       -1,
    "d":          -1,
    "kit":        -1,
    "skip":       -2,
    "none":       -2,
    "x":          -2,
}

DRUM_CHANNEL = 9   # MIDI channel 10 in 0-indexed form
DRUM_NOTE_5  = 36  # kick
DRUM_NOTE_6  = 38  # snare
DEFAULT_PITCHED_NOTE = 60

DEFAULT_ASSIGNMENTS = [
    (38, False),   # Track 1 — Synth Bass 1
    (80, False),   # Track 2 — Lead 1 (square)
    (89, False),   # Track 3 — Pad 2 (warm)
    (4,  False),   # Track 4 — Electric Piano 1
    (-1, True),    # Track 5 — Drums
    (-1, True),    # Track 6 — Drums
]


# ---------- .song parser ----------

class Reader:
    def __init__(self, data):
        self.data = data; self.pos = 0
    def u8(self):
        v = self.data[self.pos]; self.pos += 1; return v
    def u16(self):
        v = struct.unpack_from('<H', self.data, self.pos)[0]; self.pos += 2; return v
    def u32(self):
        v = struct.unpack_from('<I', self.data, self.pos)[0]; self.pos += 4; return v
    def raw(self, n):
        v = self.data[self.pos:self.pos + n]; self.pos += n; return v
    def skip(self, n): self.pos += n


def parse_song(path):
    with open(path, 'rb') as f:
        data = f.read()
    r = Reader(data)

    if r.u32() != MAGIC:
        raise ValueError(f"not a EUROTRACKER song: {path}")
    version = r.u8(); r.skip(3)
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
                    'cv': r.u16(), 'on': bool(r.u8() & 1),
                    'prob': r.u8(), 'gate': r.u8(), 'decay': r.u8(),
                    'attack': r.u8(), 'ratchet': r.u8(), 'micro': r.u8(),
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
        'path': path, 'version': version, 'length': length,
        'dividers': dividers, 'patterns': patterns, 'plens': plens,
        'quantizers': quantizers, 'cv_ranges': cv_ranges,
        'reset_flags': reset_flags, 'swing': swing,
        'q_ranges': q_ranges, 'scale_root': scale_root,
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
    return max(0, min(127, (octave + 1) * 12 + pc))


def cv_to_midi_note(cv, quantizer):
    if quantizer and quantizer['enabled'] and quantizer['notes']:
        best = None; best_diff = 0xFFFF
        for n in quantizer['notes']:
            if not n['in_scale']:
                continue
            diff = abs(n['dac'] - cv)
            if diff < best_diff:
                best_diff = diff; best = n['name']
        if best:
            return note_name_to_midi(best)
    return max(0, min(127, int(cv * 127 / 4095)))


def gate_duration_ticks(step_ticks, gate_percent):
    half = step_ticks / 2.0
    if gate_percent == 0: return 1
    if gate_percent <= 32:
        return max(1, int(gate_percent * half / 32))
    if gate_percent <= 96:
        return max(1, int(half + (gate_percent - 32) * half))
    if gate_percent == 97: return max(1, int(128 * half))
    if gate_percent == 98: return max(1, int(256 * half))
    return max(1, int(512 * half))


# ---------- Raw MIDI writer ----------

def var_len(v):
    if v < 0: v = 0
    buf = [v & 0x7F]; v >>= 7
    while v:
        buf.insert(0, 0x80 | (v & 0x7F)); v >>= 7
    return bytes(buf)


def tempo_event(bpm):
    return b'\xff\x51\x03' + int(60_000_000 / bpm).to_bytes(3, 'big')


def time_sig_event(num, den):
    import math
    return b'\xff\x58\x04' + bytes([num, int(math.log2(den)), 24, 8])


def track_name_event(name):
    data = name.encode('utf-8')[:127]
    return b'\xff\x03' + var_len(len(data)) + data


def program_change(ch, program):
    return bytes([0xC0 | (ch & 0x0F), program & 0x7F])


def note_on(ch, note, vel=100):
    return bytes([0x90 | (ch & 0x0F), note & 0x7F, vel & 0x7F])


def note_off(ch, note):
    return bytes([0x80 | (ch & 0x0F), note & 0x7F, 0])


def build_track(events):
    events.sort(key=lambda e: (e[0], e[1][0]))
    out = bytearray(); last = 0
    for t, data in events:
        out += var_len(t - last); out += data; last = t
    out += var_len(0); out += b'\xff\x2f\x00'
    return bytes(out)


def write_multi_track_midi(path, track_chunks, ppq):
    with open(path, 'wb') as f:
        f.write(b'MThd')
        f.write(struct.pack('>I', 6))
        f.write(struct.pack('>H', 1))
        f.write(struct.pack('>H', len(track_chunks)))
        f.write(struct.pack('>H', ppq))
        for data in track_chunks:
            f.write(b'MTrk')
            f.write(struct.pack('>I', len(data)))
            f.write(data)


# ---------- Track rendering ----------

def render_conductor(bpm, ppq, song_name):
    events = [
        (0, track_name_event(song_name)),
        (0, tempo_event(bpm)),
        (0, time_sig_event(4, 4)),
    ]
    return build_track(events)


def render_track(song, track_idx, assignment, bpm, ppq):
    program, is_drum = assignment
    is_melodic = track_idx < NUM_MELODIC_TRACKS

    if is_drum:
        channel = DRUM_CHANNEL
        default_note = DRUM_NOTE_5 if track_idx == 4 else DRUM_NOTE_6
    else:
        # Assign melodic tracks to channels 0..3, gate tracks to 4..5
        channel = track_idx if is_melodic else (track_idx - 2)
        if channel == DRUM_CHANNEL:
            channel = 5   # avoid clashing with drum channel
        default_note = DEFAULT_PITCHED_NOTE

    mult = ppq // SEQUENCER_PPQN
    events = []
    events.append((0, track_name_event(f"Track {track_idx + 1}")))
    if not is_drum and program >= 0:
        events.append((0, program_change(channel, program)))

    quantizer = song['quantizers'][track_idx] if is_melodic else None
    swing = song['swing'][track_idx]

    tick = 0
    for song_step in range(song['length']):
        # Song-step length = max across tracks
        ss_seqticks = 0
        for t in range(NUM_TRACKS):
            total = DIVIDER_TICKS[song['dividers'][t][song_step]] * song['plens'][t][song_step]
            if total > ss_seqticks:
                ss_seqticks = total
        ss_midi = ss_seqticks * mult
        ss_end = tick + ss_midi

        div_idx = song['dividers'][track_idx][song_step]
        step_midi = DIVIDER_TICKS[div_idx] * mult
        plen = song['plens'][track_idx][song_step]
        pattern = song['patterns'][track_idx][song_step]

        pos = tick
        while pos < ss_end:
            for step_idx in range(plen):
                if pos >= ss_end:
                    break
                st = pattern[step_idx]
                if st['on']:
                    ratchet = max(1, st['ratchet'])

                    offset = 0
                    if st['micro'] > 0:
                        offset = (step_midi * st['micro']) // (100 * ratchet)
                    elif step_idx % 2 == 1 and swing > 0:
                        offset = (step_midi * swing) // 200

                    base = pos + offset
                    if is_melodic:
                        note = cv_to_midi_note(st['cv'], quantizer)
                    else:
                        note = default_note

                    gate_ticks = gate_duration_ticks(step_midi, st['gate'])

                    on_t = min(base, ss_end - 1)
                    events.append((on_t, note_on(channel, note)))
                    events.append((on_t + gate_ticks, note_off(channel, note)))

                    if ratchet > 1:
                        spacing = step_midi // ratchet
                        for h in range(1, ratchet):
                            ht = pos + h * spacing
                            if ht >= ss_end:
                                break
                            hg = max(1, spacing // 2)
                            events.append((ht, note_on(channel, note)))
                            events.append((ht + hg, note_off(channel, note)))

                pos += step_midi

        tick = ss_end

    return build_track(events)


# ---------- Interactive assignment ----------

def print_shortlist():
    print()
    print("Common General MIDI instruments:")
    for num, name in GM_SHORTLIST:
        print(f"  {num:3d}  {name}")
    print()
    print("Special values:")
    print("  drums / d / kit  — GM drum kit (channel 10)")
    print("  skip / none / x  — omit this track from the MIDI file")
    print()


def parse_assignment(text):
    """Returns (program, is_drum) or None for skip, or raises ValueError."""
    t = text.strip().lower()
    if not t:
        return 'default'
    if t in NAME_ALIASES:
        v = NAME_ALIASES[t]
        if v == -1: return (0, True)
        if v == -2: return None
        return (v, False)
    try:
        n = int(t)
        if 0 <= n <= 127:
            return (n, False)
    except ValueError:
        pass
    raise ValueError(f"cannot parse '{text}'")


def prompt_assignments():
    print()
    print("Assign an instrument to each track.")
    print("Press Enter to keep the default shown in brackets.")
    print()

    assignments = []
    for i in range(NUM_TRACKS):
        default_prog, default_drum = DEFAULT_ASSIGNMENTS[i]
        if default_drum:
            default_label = "drums"
        else:
            default_label = GM_NAMES.get(default_prog, f"program {default_prog}")

        prompt = f"  Track {i + 1} [{default_label}]: "
        while True:
            try:
                text = input(prompt)
            except EOFError:
                text = ""
            try:
                result = parse_assignment(text)
            except ValueError as e:
                print(f"    {e}")
                continue
            if result == 'default':
                assignments.append((default_prog, default_drum))
            elif result is None:
                assignments.append(None)
            else:
                assignments.append(result)
            break

    print()
    return assignments


# ---------- Main ----------

def convert(song_path, bpm, out_dir, ppq, assignments):
    song = parse_song(song_path)
    base = Path(song_path).stem
    out_dir = Path(out_dir); out_dir.mkdir(parents=True, exist_ok=True)

    conductor = render_conductor(bpm, ppq, base)

    chunks = [conductor]
    for t in range(NUM_TRACKS):
        if assignments[t] is None:
            continue
        chunks.append(render_track(song, t, assignments[t], bpm, ppq))

    out_path = out_dir / f"{base}_full.mid"
    write_multi_track_midi(str(out_path), chunks, ppq)

    print(f"Wrote: {out_path}")
    print(f"  {len(chunks) - 1} track(s), BPM {bpm}, PPQ {ppq}")


def main():
    parser = argparse.ArgumentParser(
        description="Convert a EUROTRACKER song to a full multi-track MIDI file.")
    parser.add_argument('files', nargs='+', help='one or more .song files')
    parser.add_argument('--bpm', type=float, default=120.0)
    parser.add_argument('--ppq', type=int, default=DEFAULT_MIDI_PPQ,
                        help=f'MIDI PPQ, must be a multiple of {SEQUENCER_PPQN}')
    parser.add_argument('--out', default='.', help='output folder')
    parser.add_argument('--no-prompt', action='store_true',
                        help='use default assignments without asking')
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

    if args.no_prompt:
        assignments = [(p, d) if p != -1 else (0, True)
                       for p, d in DEFAULT_ASSIGNMENTS]
    else:
        print_shortlist()
        assignments = prompt_assignments()

    for path in paths:
        try:
            convert(path, args.bpm, args.out, args.ppq, assignments)
        except Exception as e:
            print(f"ERROR: {path}: {e}", file=sys.stderr)


if __name__ == '__main__':
    main()