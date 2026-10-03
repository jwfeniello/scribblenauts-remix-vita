"""Read Vita crash registers/stack without unpacking a large ELF onto disk.

Note layouts documented by https://github.com/xyzz/vita-parse-core (core.py).
"""
import argparse
import gzip
from pathlib import Path
import struct


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def cstring(data, offset):
    return data[offset:data.index(b"\0", offset)].decode(errors="replace")


class Core:
    def __init__(self, path):
        data = path.read_bytes()
        if data[:2] == b"\x1f\x8b":
            data = gzip.decompress(data)
        if data[:6] != b"\x7fELF\x01\x01":
            raise ValueError("Expected an ELF32 little-endian Vita core")
        phoff = u32(data, 28)
        phsize, phcount = struct.unpack_from("<HH", data, 42)
        self.notes, self.memory = {}, []
        for i in range(phcount):
            kind, offset, address, _, size, _, _, _ = struct.unpack_from("<8I", data, phoff+i*phsize)
            blob = data[offset:offset+size]
            if kind == 1:
                self.memory.append((address, blob))
            elif kind == 4:
                off = 0
                while off + 12 <= len(blob):
                    namesize, descsize, _ = struct.unpack_from("<3I", blob, off)
                    off += 12
                    name = blob[off:off+namesize].rstrip(b"\0").decode(errors="replace")
                    off += (namesize + 3) & ~3
                    self.notes[name] = blob[off:off+descsize]
                    off += (descsize + 3) & ~3
        self.modules, self.threads, self.regs = [], {}, {}
        blob = self.notes["MODULE_INFO"]
        off = 8
        for _ in range(u32(blob, 4)):
            name, count = cstring(blob, off+0x24), u32(blob, off+0x4c)
            off += 0x50
            for segment in range(count):
                start, size = struct.unpack_from("<II", blob, off+8)
                self.modules.append((start, size, name, segment))
                off += 0x14
            off += 0x10
        blob = self.notes["THREAD_INFO"]
        off = 8
        for _ in range(u32(blob, 4)):
            self.threads[u32(blob, off+4)] = (cstring(blob, off+8), u32(blob, off+0x74), u32(blob, off+0x9c))
            off += u32(blob, off)
        blob = self.notes["THREAD_REG_INFO"]
        off = 8
        for _ in range(u32(blob, 4)):
            self.regs[u32(blob, off+4)] = struct.unpack_from("<16I", blob, off+8)
            off += u32(blob, off)

    def address(self, address):
        for start, size, name, segment in self.modules:
            if start <= address < start+size:
                return f"0x{address:08x} ({name} segment {segment}, base 0x{start:08x}, offset 0x{address-start:x})"
        if 0x98000000 <= address < 0x9b000000:
            return f"0x{address:08x} (Android library + 0x{address-0x98000000:x})"
        return f"0x{address:08x}"

    def read(self, address, size):
        for start, data in self.memory:
            if start <= address and address+size <= start+len(data):
                return data[address-start:address-start+size]
        return None

    def show(self):
        for tid, (name, reason, pc) in self.threads.items():
            if not reason:
                continue
            print(f"Thread {name} id=0x{tid:x} reason=0x{reason:x} PC={self.address(pc)}")
            regs = self.regs[tid]
            for i, value in enumerate(regs):
                print(f"  {('SP','LR','PC')[i-13] if i >= 13 else 'R'+str(i)}: {self.address(value)}")
            print("Stack words (possible return addresses, not an unwound backtrace):")
            for off in range(-16, 256, 4):
                address = regs[13] + off
                value = self.read(address, 4)
                if value is not None:
                    print(f"  SP{off:+4}: {self.address(u32(value, 0))}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core", type=Path)
    Core(parser.parse_args().core).show()
