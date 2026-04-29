# InterPlanetary

Small local 2-player C++ prototype of a realtime turn-based voxel duel on a sealed cubic planet.

## Build on macOS

Requirements:

- Xcode Command Line Tools
- CMake 3.21+

This repo already includes Raylib under `third_party/raylib`, so no package manager setup is required.

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Run:

```bash
./build/interplanetary
```

## Prototype Notes

- The game uses a visible 2D slice through a 3D voxel cube.
- Both players interact on the middle Z slice, but the data structures stay 3D so the project can expand into fuller voxel views later.
- The turn timer alternates between `Mine / Build` and `Attack / Defend` every 90 seconds.

## Controls

Player 1:

- `W A S D`: move
- `Q / E`: previous or next tool
- `F`: use current tool
- `R`: place selected structure
- `T / G`: aim angle up or down during missile fire
- `Y / H`: missile power up or down

Player 2:

- Arrow keys: move
- `, / .`: previous or next tool
- `O`: use current tool
- `P`: place selected structure
- `I / K`: aim angle up or down during missile fire
- `U / J`: missile power up or down

General:

- `TAB`: toggle world slice overlay help
- `ENTER`: restart after a winner is declared

## Tool Meanings

- `Mine`: manual mining during Mine / Build only
- `Wall`: cheap defence
- `Armour`: tougher defence
- `Silo`: missile silo
- `Fire`: launch from a nearby owned silo during Attack / Defend only

Mining is manual only in the current prototype. Face a solid block, switch to `Mine`, and hold the use key to dig it out.
