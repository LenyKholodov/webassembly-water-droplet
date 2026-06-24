# Droplet

**Droplet** is a small WebAssembly/WebGL demo game written in C++ and compiled to the browser with [Emscripten](https://emscripten.org). It runs on a layered, hand-rolled engine — there is no off-the-shelf game engine here, only a thin set of third-party ports (SDL2, GLFW3, Bullet, OpenGL ES / WebGL).

## Gameplay

You **drag leaves** to stagger them, carving a path for physics-driven **water droplets** to fall. Guide the droplets all the way down and they fill the ground with **growing ferns**. The simulation is real — droplets are rigid bodies falling under [Bullet](https://github.com/bulletphysics/bullet3) physics, bouncing off whatever leaf geometry you have arranged.

The game logic lives in [src/launcher/world.cpp](../src/launcher/world.cpp); the entry point and main loop are in [src/launcher/main.cpp](../src/launcher/main.cpp).

## Tech stack

| Concern | Choice |
| --- | --- |
| Language | C++17 |
| Toolchain | Emscripten (`emcc`) → WebAssembly |
| Windowing / input | GLFW3 (`-sUSE_GLFW=3`) |
| Platform / images / audio | SDL2 + SDL_image (`-sUSE_SDL=2`, `-sUSE_SDL_IMAGE=2`) |
| Physics | Bullet (`-sUSE_BULLET=1`) |
| Graphics | OpenGL ES / WebGL via a custom render abstraction |
| Mesh loading | [fast_obj](https://github.com/thisistherk/fast_obj) (git submodule) |

The engine is split into layered subsystems, from foundation to game:

```
common  →  math  →  media  →  render/low_level  →  render/scene(+passes)  →  scene  →  application  →  launcher
```

- **common** — zero-dependency foundation: logging, exceptions, strings, the self-registering `Component` plugin mechanism.
- **math** — vectors, matrices, geometry.
- **media** — asset loading (textures, shaders, meshes).
- **render/low_level** — GPU/WebGL abstraction.
- **render/scene** + **render/scene_passes** — the render pipeline.
- **scene** — the scene graph.
- **application** — window creation and the event loop.
- **launcher** — the actual Droplet game.

## Controls

> Taken from [dist/index.html](../dist/index.html).

| Action | Desktop | Mobile |
| --- | --- | --- |
| Drag leaves | Left mouse button — click & drag | Touch & drag |
| Move camera | `W` / `A` / `S` / `D` + right mouse button held | — |

## Quick start

```sh
# 1. Pull the third-party submodule (fast_obj)
git submodule update --init --recursive

# 2. Build the WebAssembly bundle into dist/ (requires Emscripten on PATH)
make -j

# 3. Serve dist/ over HTTP (file:// XHR is blocked by browsers)
./run-webserver.sh

# 4. Open the game
open http://localhost:8080/
```

`make` compiles every `src/**/*.cpp` and links `dist/index.js` + `index.wasm`, embedding the contents of `media/` (textures, shaders, meshes) directly into the bundle. See [build.md](build.md) for the full pipeline.

## Documentation

| Document | What's inside |
| --- | --- |
| [architecture.md](architecture.md) | Engine layers, subsystem responsibilities, render pipeline, and how a frame is driven. |
| [entities.md](entities.md) | Game objects — leaves, droplets, ferns — and how the world simulates them. |
| [build.md](build.md) | Makefile, Emscripten flags, output layout, and how to run locally. |
| [asset-pipeline.md](asset-pipeline.md) | How textures, shaders, and meshes in `media/` are authored, embedded, and loaded. |
| [CHANGELOG.md](CHANGELOG.md) | Notable changes over time. |
| [open-source-readiness.md](open-source-readiness.md) | What remains before this is a clean public release. |
| [plan.md](plan.md) | Prioritized backlog of physics/fluid-sim correctness, model, and performance fixes. |

## Project status

Droplet is a working **demo**, not a finished product. The gameplay loop runs end to end and the engine is reasonably layered, but the source still carries demo-era rough edges (commented-out experiments, ad-hoc constants, no test suite). Before treating this as a polished open-source release, read [open-source-readiness.md](open-source-readiness.md) for the outstanding cleanup, licensing, and packaging work.
