from pathlib import Path
import hashlib
import os
import struct
import subprocess
import sys
import time
import zlib

SECTOR_SIZE = 512
IMAGE_SIZE = 64 * 1024 * 1024

BOOT_LBA = 0
LOADER_LBA = 1
LOADER_SECTORS = 15
KERNEL_HEADER_LBA = 16
KERNEL_PAYLOAD_LBA = 17

KERNEL_LOAD_ADDRESS = 0x10000
KERNEL_ENTRY_ADDRESS = 0x10000
KERNEL_MAX_BYTES = 512 * 1024
KERNEL_MAX_SECTORS = 1024

KERNEL_MAGIC = 0x34364B53
KERNEL_VERSION = 1
KERNEL_HEADER_SIZE = 32

FS_LBA = 2048
FS_MAGIC = 0x53525033
FS_VERSION = 5
FS_ENTRY_SIZE = 512
FS_ENTRY_EXTENTS = 256
FS_MAX_FILES = 1024
FS_ENTRIES_LBA = FS_LBA + 1
FS_ENTRIES_SECTORS = FS_MAX_FILES * FS_ENTRY_SIZE // SECTOR_SIZE
FS_BITMAP_LBA = FS_ENTRIES_LBA + FS_ENTRIES_SECTORS
FS_BITMAP_SECTORS = 32
FS_DATA_LBA = FS_BITMAP_LBA + FS_BITMAP_SECTORS
FS_DISK_SECTORS = IMAGE_SIZE // SECTOR_SIZE
FS_DATA_SECTORS = FS_DISK_SECTORS - FS_DATA_LBA
FS_FILE_MAX = 0x100000
FS_PATH_MAX = 232
FS_ENTRY_OWNER = 248
FS_ENTRY_MODE = 250
FS_FLAG_USED = 1
FS_FLAG_DIRECTORY = 2

BIN_DIR = Path("build/bin")
TEST_SOURCES = Path("tests/programs")
SOURCE_ROOT = "/usr/src/serpentis"
SOURCE_SKIP = ("build/",)
SOURCE_BINARY = (".png", ".img", ".bin")
SELF_HOSTED = ("files", "clock", "graphcalc", "editor")

# only the test image has users, a real disk asks for them on first boot
USERS = (("root", 0, "/root", "root"), ("guest", 1000, "/home/guest", ""))

with_tests = "--tests" in sys.argv
image_path = Path("build/serpentis-os-test.img" if with_tests else "build/serpentis-os.img")

boot = Path("build/boot64.bin").read_bytes()
loader = Path("build/loader64.bin").read_bytes()
kernel = Path("build/kernel64.bin").read_bytes()

if len(boot) != SECTOR_SIZE:
    raise SystemExit("boot64.bin must be exactly 512 bytes")
if len(loader) != LOADER_SECTORS * SECTOR_SIZE:
    raise SystemExit("loader64.bin must be exactly 7680 bytes")
if not kernel:
    raise SystemExit("kernel64.bin is empty")
if len(kernel) > KERNEL_MAX_BYTES:
    raise SystemExit(f"kernel64.bin is {len(kernel)} bytes; maximum is {KERNEL_MAX_BYTES}")

kernel_sectors = (len(kernel) + SECTOR_SIZE - 1) // SECTOR_SIZE
if kernel_sectors > KERNEL_MAX_SECTORS:
    raise SystemExit("kernel requires too many sectors")

image = bytearray(IMAGE_SIZE)


def timestamp():
    now = time.gmtime()
    return ((((((now.tm_year - 2000) & 63) << 4 | now.tm_mon) << 5 | now.tm_mday) << 5 | now.tm_hour) << 6
            | now.tm_min) << 6 | min(now.tm_sec, 59)


class FileSystem:
    def __init__(self):
        self.entries = bytearray(FS_MAX_FILES * FS_ENTRY_SIZE)
        self.bitmap = bytearray(FS_BITMAP_SECTORS * SECTOR_SIZE)
        self.paths = {}
        struct.pack_into("<IHHHIIIH", image, FS_LBA * SECTOR_SIZE, FS_MAGIC, FS_VERSION,
                         FS_ENTRY_SIZE, FS_MAX_FILES, FS_DATA_LBA, FS_DATA_SECTORS,
                         FS_BITMAP_LBA, FS_BITMAP_SECTORS)
        self.create("/", FS_FLAG_USED | FS_FLAG_DIRECTORY, 0)
        self.next_free_sector = 0

    def save(self):
        start = FS_ENTRIES_LBA * SECTOR_SIZE
        image[start:start + len(self.entries)] = self.entries
        start = FS_BITMAP_LBA * SECTOR_SIZE
        image[start:start + len(self.bitmap)] = self.bitmap

    def create(self, path, flags, parent):
        encoded = path.encode("ascii")
        if len(encoded) >= FS_PATH_MAX:
            raise SystemExit("path too long: " + path)
        index = len(self.paths)
        if index >= FS_MAX_FILES:
            raise SystemExit("filesystem is full, cannot add " + path)
        offset = index * FS_ENTRY_SIZE
        struct.pack_into("<BxHIII", self.entries, offset, flags, parent, 0, timestamp(), 0)
        self.entries[offset + 16:offset + 16 + len(encoded)] = encoded
        if flags & FS_FLAG_DIRECTORY or path.endswith(".serp"):
            mode = 0o755
        else:
            mode = 0o644
        struct.pack_into("<HH", self.entries, offset + FS_ENTRY_OWNER, 0, mode)
        self.paths[path] = index
        return index

    def own(self, path, owner, mode):
        offset = self.paths[path] * FS_ENTRY_SIZE
        struct.pack_into("<HH", self.entries, offset + FS_ENTRY_OWNER, owner, mode)

    def directory(self, path):
        if path in self.paths:
            return self.paths[path]
        parent = self.directory(path.rsplit("/", 1)[0] or "/")
        return self.create(path, FS_FLAG_USED | FS_FLAG_DIRECTORY, parent)

    def write(self, index, data):
        if len(data) > FS_FILE_MAX:
            raise SystemExit("file too large: %d bytes" % len(data))
        sectors = (len(data) + SECTOR_SIZE - 1) // SECTOR_SIZE
        first = 0
        if sectors:
            first = FS_DATA_LBA + self.next_free_sector
            if self.next_free_sector + sectors > FS_DATA_SECTORS:
                raise SystemExit("filesystem has no space left")
            for sector in range(self.next_free_sector, self.next_free_sector + sectors):
                self.bitmap[sector >> 3] |= 1 << (sector & 7)
            self.next_free_sector += sectors
            start = first * SECTOR_SIZE
            image[start:start + len(data)] = data
        offset = index * FS_ENTRY_SIZE
        struct.pack_into("<I", self.entries, offset + 4, len(data))
        struct.pack_into("<I", self.entries, offset + 12, sectors)
        if sectors:
            self.entries[offset + 1] = 1
            struct.pack_into("<II", self.entries, offset + FS_ENTRY_EXTENTS, first, sectors)

    def install(self, path, data):
        directory, name = path.rsplit("/", 1)
        parent = self.directory(directory or "/")
        index = self.create(path, FS_FLAG_USED, parent)
        self.write(index, data)


def test_programs():
    return {source.stem + ".serp" for source in TEST_SOURCES.glob("*.asm")}


def install_programs(fs):
    tests = test_programs()
    for program in sorted(BIN_DIR.glob("*.serp")):
        if program.name in tests and not with_tests:
            continue
        fs.install("/bin/" + program.name, program.read_bytes())


def install_libraries(fs):
    shared = [line for line in Path("config.inc").read_text().splitlines()
              if line.startswith(("FS_FLAG_", "FS_PATH_MAX", "FS_FILE_BYTES", "PIT_FREQUENCY", "HEAP_BASE"))]
    defs = Path("include/syscall.inc").read_text().rstrip("\n") + "\n\n" + "\n".join(shared) + "\n"
    fs.install("/lib/defs.inc", defs.encode())
    for name in ("syscall.inc", "string.inc", "memory.inc", "io.inc", "errors.inc", "library.inc",
                 "draw.inc", "gui.inc", "screen.inc", "screen_bss.inc", "users.inc", "users_bss.inc"):
        fs.install("/lib/" + name, Path("apps/runtime", name).read_bytes())
    fs.install("/lib/start.inc", Path("apps/runtime/start.inc").read_bytes())
    for source in sorted(Path("examples/asm").glob("*")):
        fs.install("/src/" + source.name, source.read_bytes())
    for source in sorted(Path("libc/include").glob("*.h")):
        fs.install("/lib/c/include/" + source.name, source.read_bytes())
    for source in sorted(Path("libc").glob("*.c")):
        fs.install("/lib/c/" + source.name, source.read_bytes())
    for source in sorted(Path("examples/c").glob("*.c")):
        fs.install("/src/c/" + source.name, source.read_bytes())
    if with_tests:
        for source in sorted(Path("tests/sources").glob("*")):
            fs.install("/tests/" + source.name, source.read_bytes())
        for name in SELF_HOSTED:
            for source in sorted(Path("apps", name).glob("*.*")):
                text = source.read_text().replace("'../runtime/program.inc'", "'/lib/start.inc'")
                text = text.replace("'../runtime/", "'/lib/")
                fs.install("/tests/selfhost/%s/%s" % (name, source.name), text.encode())


def install_sources(fs):
    listed = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True)
    for name in sorted(listed.stdout.split("\n")):
        if not name or name.startswith(SOURCE_SKIP) or name.endswith(SOURCE_BINARY):
            continue
        source = Path(name)
        if source.is_file():
            fs.install(SOURCE_ROOT + "/" + name, source.read_bytes())


def install_share(fs):
    wallpaper = Path("build/wallpaper.raw")
    if wallpaper.exists():
        fs.install("/share/wallpaper.raw", wallpaper.read_bytes())
    fs.directory("/etc")
    fs.directory("/tmp")
    fs.own("/tmp", 0, 0o777)
    fs.install("/etc/theme", b"")
    fs.own("/etc/theme", 0, 0o666)


def install_users(fs):
    fs.directory("/root")
    fs.own("/root", 0, 0o700)
    fs.directory("/home")
    if not with_tests:
        return
    passwd = ""
    shadow = ""
    for name, uid, home, password in USERS:
        passwd += "%s:%d:%s\n" % (name, uid, home)
        salt = os.urandom(8)
        digest = hashlib.sha256(salt + password.encode()).hexdigest()
        shadow += "%s:%s:%s\n" % (name, salt.hex(), digest)
        fs.directory(home)
        fs.own(home, uid, 0o700 if uid == 0 else 0o755)
    fs.install("/etc/passwd", passwd.encode())
    fs.install("/etc/shadow", shadow.encode())
    fs.own("/etc/shadow", 0, 0o600)


filesystem = FileSystem()
install_programs(filesystem)
install_libraries(filesystem)
install_share(filesystem)
install_users(filesystem)
install_sources(filesystem)
filesystem.save()

header = bytearray(SECTOR_SIZE)
checksum = zlib.crc32(kernel) & 0xFFFFFFFF
struct.pack_into("<IHHIIIIII", header, 0, KERNEL_MAGIC, KERNEL_VERSION, KERNEL_HEADER_SIZE,
                 len(kernel), kernel_sectors, KERNEL_LOAD_ADDRESS, KERNEL_ENTRY_ADDRESS,
                 FS_LBA, checksum)

image[BOOT_LBA * SECTOR_SIZE:BOOT_LBA * SECTOR_SIZE + len(boot)] = boot
image[LOADER_LBA * SECTOR_SIZE:LOADER_LBA * SECTOR_SIZE + len(loader)] = loader
image[KERNEL_HEADER_LBA * SECTOR_SIZE:KERNEL_HEADER_LBA * SECTOR_SIZE + len(header)] = header

kernel_offset = KERNEL_PAYLOAD_LBA * SECTOR_SIZE
if kernel_offset + kernel_sectors * SECTOR_SIZE > FS_LBA * SECTOR_SIZE:
    raise SystemExit("kernel overlaps the filesystem")
image[kernel_offset:kernel_offset + len(kernel)] = kernel

image_path.write_bytes(image)
print(f"Created: {image_path} ({len(filesystem.paths)} entries, "
      f"{filesystem.next_free_sector * SECTOR_SIZE // 1024} KB of files)")
