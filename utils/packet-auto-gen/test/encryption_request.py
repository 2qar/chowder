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

server_id = "this doesnt matter!!"
b = varint_bytes(len(server_id))
b += struct.pack(f"{len(server_id)}s", bytes(server_id, "ascii"))
b += varint_bytes(5)
b += bytes(5)
b += varint_bytes(4)
b += bytes(4)

b = varint_bytes(len(b) + 1) + struct.pack("b", 0x01) + b

with open("encryption_request.bin", "wb") as f:
    f.write(b)
