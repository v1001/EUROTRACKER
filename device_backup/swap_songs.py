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