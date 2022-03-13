# packet-auto-gen

Generates C code for reading and writing a packet like this over the network:

### Client Settings packet
```
id = 0x05

String(16) locale
Byte view_distance
Enum(VarInt) chat_mode {
	ENABLED
	COMMANDS_ONLY
	HIDDEN
}
Bool chat_colors
UByte displayed_skin_parts
Enum(VarInt) main_hand {
	LEFT
	RIGHT
}
```

The generated code will do basic input validation, like checking that strings are under their max length, enum values are actually valid values, etc.

Now, instead of writing all of this boring stuff by hand and maintaining it, I get to maintain this nightmare. At least this is cool

Meant for use in [chowder](https://github.com/BigHeadGeorge/chowder)
