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
b = struct.pack("B", 12)
# slots count
b += struct.pack(">h", 2)
# FIXME: ignores NBT for now
b += pack_slot((True, 4, 5, bytes(1)))
b += pack_slot((True, 10, 1, bytes(1)))

b = varint_bytes(len(b) + 1) + struct.pack("b", 0x15) + b

with open("window_items.bin", "wb") as f:
    f.write(b)
