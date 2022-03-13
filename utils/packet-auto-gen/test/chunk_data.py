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

# x, y
b = struct.pack(">2i", 2, 5)
# full
b += struct.pack("b", True)
# section bitmask
b += struct.pack("b", 0x1)
# heightmaps nbt
root_name = "MOTION_BLOCKING"
b += struct.pack("b", 0x0a)
b += struct.pack(">H", 0)
b += struct.pack("b", 0x0c)
b += struct.pack(">H", len(root_name))
b += struct.pack(f"{len(root_name)}s", bytes(root_name, "ascii"))
b += struct.pack(">i", 36)
b += struct.pack(">q", 0) * 36
b += struct.pack("b", 0x00)
# biomes
b += struct.pack(">i", 1) * 1024

## section data
# block count
data = struct.pack(">h", 16*16)
# bits per block
data += struct.pack(">B", 4)

palette = [varint_bytes(0), varint_bytes(420)]
data += varint_bytes(len(palette))
for id_bytes in palette:
    data += id_bytes
data_array_len = (4096 * 4) // 64
data += varint_bytes(data_array_len)
data += struct.pack(">q", 0x1111111111111111) * 16
data += struct.pack(">q", 0) * (data_array_len - 16)

# size
b += varint_bytes(len(data))
# duh
b += data
# block entities array len
b += struct.pack("b", 0)

# packet id
b = struct.pack("b", 0x22) + b
# len
b = varint_bytes(len(b)) + b

with open("chunk_data.bin", "wb") as f:
    f.write(b)
