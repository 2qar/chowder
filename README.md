# chowder

A Minecraft 1.15.2 server, because I'm bored and wish the default server was faster.

In it's current state, it's not really functional.

## Dependencies
- GNU make
- OpenSSL
- libcurl
- zlib

## Building
1. Clone the submodules by either specifying `--recurse-submodules` when cloning
the repo, or run `git submodules update --init` after cloning.
2. Run `make`.

## Running
Currently world generation isn't implemented, so you'll have to pre-generate
a world and copy it here. The path it checks is "levels/default", which can
be changed by changing the value of `LEVEL_PATH` in `src/main.c` and recompiling.

Configuration sucks right now. I'll change it later, I swear.
