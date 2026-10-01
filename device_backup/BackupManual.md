# EUROTRACKER — Backup & Song Swap Manual

This manual covers full backup, full restore, and swapping song files between backups. All operations use a Windows PC and a USB connection to the device.

---

## Prerequisites

- Windows PC with Python 3.10 or later
- `esptool`: `py -m pip install esptool`
- `littlefs-python`: `py -m pip install littlefs-python`
- Device connected via USB
- COM port number (visible in Device Manager under Ports)

---

## Part 1 — Full Backup

### 1.1 Dump the flash

```
py -m esptool --port COM12 read_flash 0x0 0x400000 full_backup.bin
```

- Replace `COM12` with the actual port.
- Runtime: 30–60 seconds.
- Output: exactly 4,194,304 bytes.

Verify size:

```
py -c "import os; print(os.path.getsize('full_backup.bin'))"
```

### 1.2 Extract song files

Save the script below as `extract_littlefs.py` in the same folder as `full_backup.bin`.

```python
import os
import sys
import shutil
import littlefs

BLOCK_SIZE = 4096
BACKUP_NAME = 'full_backup.bin'
BACKUP_COPY = 'full_backup.work'
OUT_DIR_NAME = 'extracted'


def find_backup():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, BACKUP_NAME)
    if not os.path.isfile(path):
        print(f'ERROR: {BACKUP_NAME} not found in {here}')
        sys.exit(1)
    return path


def find_littlefs_partition(backup_path):
    with open(backup_path, 'rb') as f:
        f.seek(0x8000)
        pt = f.read(0x1000)
    for i in range(0, 0x1000, 32):
        entry = pt[i:i + 32]
        if len(entry) < 32 or entry[0] != 0xAA or entry[1] != 0x50:
            break
        ptype   = entry[2]
        subtype = entry[3]
        offset  = int.from_bytes(entry[4:8],  'little')
        size    = int.from_bytes(entry[8:12], 'little')
        if ptype == 0x01 and subtype == 0x82:
            return offset, size
    raise RuntimeError('LittleFS partition not found')


class BackupDisk:
    def __init__(self, path, base, size):
        self.f = open(path, 'r+b')
        self.base = base
        self.size = size

    def read(self, cfg, block, off, size):
        self.f.seek(self.base + block * BLOCK_SIZE + off)
        return self.f.read(size)

    def prog(self, cfg, block, off, data):
        self.f.seek(self.base + block * BLOCK_SIZE + off)
        self.f.write(data)
        return 0

    def erase(self, cfg, block):
        self.f.seek(self.base + block * BLOCK_SIZE)
        self.f.write(b'\xff' * BLOCK_SIZE)
        return 0

    def sync(self, cfg):
        self.f.flush()
        return 0


def extract(backup_path, out_dir):
    shutil.copyfile(backup_path, BACKUP_COPY)
    offset, size = find_littlefs_partition(BACKUP_COPY)
    block_count = size // BLOCK_SIZE
    print(f'LittleFS: offset=0x{offset:08x} size=0x{size:08x} '
          f'block_size={BLOCK_SIZE} block_count={block_count}')

    os.makedirs(out_dir, exist_ok=True)

    disk = BackupDisk(BACKUP_COPY, offset, size)
    try:
        fs = littlefs.LittleFS(
            context=disk,
            block_size=BLOCK_SIZE,
            block_count=block_count,
        )
        names = list(fs.listdir('/'))
        if not names:
            print('WARNING: filesystem is empty (mount likely failed).')
            return 1

        for name in names:
            path = '/' + name.lstrip('/')
            try:
                with fs.open(path, 'rb') as src:
                    data = src.read()
                out_path = os.path.join(out_dir, name.lstrip('/'))
                with open(out_path, 'wb') as dst:
                    dst.write(data)
                print(f'  {name}  {len(data)} bytes')
            except Exception as e:
                print(f'  {name}  ERROR: {e}')
        fs.unmount()
    finally:
        disk.f.close()

    print(f'Extracted to: {out_dir}')
    return 0


if __name__ == '__main__':
    backup = find_backup()
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, OUT_DIR_NAME)
    sys.exit(extract(backup, out))
```

Run it:

```
py extract_littlefs.py
```

The folder `extracted/` will contain:

```
current_project.song
song1.song
...
song8.song
global.settings
```

### 1.3 Archive

Store together with date and firmware version in the name:

```
backup_2026-10-01_v1.2/
  full_backup.bin
  extracted/
    ...
```

`full_backup.bin` alone can restore the device completely. The extracted files allow inspection and swapping.

---

## Part 2 — Full Restore

Restores everything: bootloader, partition table, firmware, and filesystem.

```
py -m esptool --port COM12 write_flash 0x0 full_backup.bin
```

Reset the device after completion.

---

## Part 3 — Song Swap

Swapping lets you replace individual song files inside a backup and flash the modified image back, leaving everything else intact.

### 3.1 Prepare files

Create a folder `songs_to_write` next to the scripts. Place the song files to install in it, named exactly as the device expects:

```
song1.song
song2.song
...
song8.song
current_project.song
global.settings   (optional)
```

### 3.2 Swap script

Save as `swap_songs.py`.

```python
import os
import sys
import shutil
import littlefs

BLOCK_SIZE = 4096
SRC_BACKUP = 'full_backup.bin'
DST_BACKUP = 'full_backup_modified.bin'
SONGS_DIR  = 'songs_to_write'
REMOVE_FILES = []


def find_littlefs_partition(path):
    with open(path, 'rb') as f:
        f.seek(0x8000)
        pt = f.read(0x1000)
    for i in range(0, 0x1000, 32):
        entry = pt[i:i + 32]
        if len(entry) < 32 or entry[0] != 0xAA or entry[1] != 0x50:
            break
        ptype   = entry[2]
        subtype = entry[3]
        offset  = int.from_bytes(entry[4:8],  'little')
        size    = int.from_bytes(entry[8:12], 'little')
        if ptype == 0x01 and subtype == 0x82:
            return offset, size
    raise RuntimeError('LittleFS partition not found')


class BackupDisk:
    def __init__(self, path, base, size):
        self.f = open(path, 'r+b')
        self.base = base
        self.size = size

    def read(self, cfg, block, off, size):
        self.f.seek(self.base + block * BLOCK_SIZE + off)
        return self.f.read(size)

    def prog(self, cfg, block, off, data):
        self.f.seek(self.base + block * BLOCK_SIZE + off)
        self.f.write(data)
        return 0

    def erase(self, cfg, block):
        self.f.seek(self.base + block * BLOCK_SIZE)
        self.f.write(b'\xff' * BLOCK_SIZE)
        return 0

    def sync(self, cfg):
        self.f.flush()
        return 0


def write_songs(src_backup, dst_backup, songs_dir, remove_files):
    here = os.path.dirname(os.path.abspath(__file__))
    src = os.path.join(here, src_backup)
    dst = os.path.join(here, dst_backup)
    sdir = os.path.join(here, songs_dir)

    if not os.path.isfile(src):
        print(f'ERROR: {src_backup} not found')
        return 1
    if not os.path.isdir(sdir):
        print(f'ERROR: folder {songs_dir} not found')
        return 1

    shutil.copyfile(src, dst)

    offset, size = find_littlefs_partition(dst)
    block_count = size // BLOCK_SIZE
    print(f'LittleFS: offset=0x{offset:08x} size=0x{size:08x} '
          f'block_count={block_count}')

    disk = BackupDisk(dst, offset, size)
    try:
        fs = littlefs.LittleFS(
            context=disk,
            block_size=BLOCK_SIZE,
            block_count=block_count,
        )

        for name in remove_files:
            path = '/' + name.lstrip('/')
            try:
                fs.remove(path)
                print(f'  removed  {name}')
            except Exception as e:
                print(f'  remove {name}  skipped: {e}')

        for name in os.listdir(sdir):
            if not name.endswith('.song') and name != 'global.settings':
                continue
            src_path = os.path.join(sdir, name)
            with open(src_path, 'rb') as f:
                data = f.read()
            fs_path = '/' + name
            try:
                fs.remove(fs_path)
            except Exception:
                pass
            with fs.open(fs_path, 'wb') as dst_f:
                dst_f.write(data)
            print(f'  wrote    {name}  {len(data)} bytes')

        fs.unmount()
    finally:
        disk.f.close()

    print(f'Modified image written to: {dst}')
    return 0


if __name__ == '__main__':
    sys.exit(write_songs(SRC_BACKUP, DST_BACKUP, SONGS_DIR, REMOVE_FILES))
```

### 3.3 Run the swap

```
py swap_songs.py
```

Output:

```
LittleFS: offset=0x00290000 size=0x00160000 block_count=352
  wrote    song1.song  8192 bytes
  wrote    song2.song  8192 bytes
  ...
Modified image written to: full_backup_modified.bin
```

### 3.4 Flash the modified image

```
py -m esptool --port COM12 write_flash 0x0 full_backup_modified.bin
```

Reset the device. The swapped songs appear in the corresponding slots; everything else is unchanged.

---

## Part 4 — Flash Safety

### Operations that wipe the filesystem

- `pio run -t uploadfs`
- Firmware calling `LittleFS.begin(true)` on a non-LittleFS partition
- `esptool erase_flash`
- `esptool write_flash 0x0 <merged_bin>` with an empty filesystem

### Operations that preserve the filesystem

- `pio run -t upload` (app partition only)
- `esptool write_flash 0x10000 firmware.bin`
- `esptool write_flash 0x290000 littlefs.bin`

Always run `read_flash` before any risky operation.

---

## Part 5 — Quick Reference

**Backup:**

```
py -m esptool --port COM12 read_flash 0x0 0x400000 full_backup.bin
py extract_littlefs.py
```

**Restore everything:**

```
py -m esptool --port COM12 write_flash 0x0 full_backup.bin
```

**Swap songs:**

```
# place files into songs_to_write/
py swap_songs.py
py -m esptool --port COM12 write_flash 0x0 full_backup_modified.bin
```

---

## Part 6 — Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `read_flash` times out | USB link unstable | Different cable or port |
| Backup size wrong | Interrupted read | Re-run the read |
| Extraction reports "filesystem is empty" | Mount failed | Original backup is untouched; retry |
| `LittleFSError -84` | Wrong block size | Verify `block_size=4096` |
| `AttributeError: 'BackupDisk' has no attribute 'prog'` | Wrong callback name | Use `prog(self, cfg, block, off, data)` |
| `write_flash` fails | Device not in bootloader | Hold BOOT, tap RESET, release BOOT |
| Swap script cannot find folder | Folder name mismatch | Ensure `songs_to_write/` exists next to the script |