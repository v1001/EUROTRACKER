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
    raise RuntimeError('LittleFS partition not found in partition table')


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
    # Work on a copy so the original is never modified.
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