import struct

def varint_bytes(varint: int) -> bytes:
    if varint < 0:
        raise ValueError("varint_bytes() doesn't work with negative numbers XD")

    vb = bytes()
    while varint > 0:
        c = varint & 0x7f
        varint >>= 7
        if varint != 0:
            c |= 0x80
        vb += struct.pack("B", c)
    return vb if vb else struct.pack("B", 0)

def pack_slot(s):
    return struct.pack("b", s[0]) + varint_bytes(s[1]) + struct.pack("b", s[2]) + s[3]

# window ID
b = struct.pack("B", 4)
# slot index
b += struct.pack(">h", 5)
# FIXME: ignores NBT for now
b += pack_slot((True, 10, 1, bytes(1)))

b = varint_bytes(len(b) + 1) + struct.pack("b", 0x17) + b

with open("set_slot.bin", "wb") as f:
    f.write(b)
