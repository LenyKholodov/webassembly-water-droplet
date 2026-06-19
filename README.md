# Droplet

**A WebAssembly/WebGL water-droplet simulation, written from scratch in C++.**

Droplet is a small, ambient tech demo: water droplets gather on the foliage of two
ferns, run together into rounded blobs, and trickle down through the leaves — growing
new ferns where they pool at the bottom. There's no score and no objective. You orbit
the scene to find the droplets and watch the simulation unfold.

It runs entirely in the browser on a **layered, hand-rolled engine** — no off-the-shelf
game engine, just a thin set of third-party ports (SDL2, GLFW3, Bullet) compiled to
WebAssembly with [Emscripten](https://emscripten.org). The droplets are real rigid
bodies under [Bullet](https://github.com/bulletphysics/bullet3) physics, and their
liquid surface is a metaball signed-distance field raymarched in a fragment shader
(true necking, merging, and concavity, with skybox reflection and screen-space
refraction).

<p align="center">
  <a href="docs/droplet.mp4"><img src="docs/droplet.gif" width="640" alt="Droplet — water droplets gathering on the foliage under a starry sky"></a>
  <br><em>▶ <a href="docs/droplet.mp4">Watch the full video</a> (~45s, higher quality)</em>
</p>

## Play

Droplets spawn on the foliage on their own, every few seconds. Your only job is to
move the camera and watch.

| Action | Desktop | Mobile |
| --- | --- | --- |
| **Orbit** around the two ferns | Drag left/right (mouse held) | Drag left/right (one finger) |
| **Rise / descend** (spiral up the scene) | Drag up/down (mouse held) | Drag up/down (one finger) |
| **Zoom** in / out | Mouse wheel | Two-finger pinch |

The camera moves on a cylinder centred between the two ferns, always looking slightly
downward toward where the droplets fall. Water-drop sounds play as droplets land,
starting on your first interaction (browser autoplay policies block audio before that).

> A live **tuning overlay** in the top-right corner exposes the droplet fluid
> parameters (metaball radius, surface merge/influence, cohesion, particle count, …).
> It writes `window.DROPLET.*`, which the C++ reads every frame — handy for dialing in
> the look without rebuilding.

## Tech stack

| Concern | Choice |
| --- | --- |
| Language | C++17 |
| Toolchain | Emscripten (`emcc`) → WebAssembly |
| Windowing / input | GLFW3 (`-sUSE_GLFW=3`), Emscripten HTML5 touch events |
| Platform / images / audio | SDL2 + SDL_image (`-sUSE_SDL=2`, `-sUSE_SDL_IMAGE=2`) |
| Physics | Bullet (`-sUSE_BULLET=1`) |
| Graphics | OpenGL ES / WebGL 2 via a custom render abstraction |
| Fluid surface | metaball SDF raymarched in GLSL |
| Mesh loading | [fast_obj](https://github.com/thisistherk/fast_obj) (git submodule, MIT) |

The engine is split into layered subsystems, foundation → game:

```
common → math → media → render/low_level → render/scene(+passes) → scene → application → launcher
```

- **common** — zero-dependency foundation: logging, exceptions, strings, a self-registering `Component` plugin mechanism.
- **math** — vectors, matrices, geometry.
- **media** — asset loading (textures, shaders, meshes).
- **render/low_level** — the GPU / WebGL abstraction.
- **render/scene** + **render/scene_passes** — the render pipeline.
- **scene** — the scene graph.
- **application** — window creation and the event loop.
- **launcher** — the Droplet game itself (entry point [src/launcher/main.cpp](src/launcher/main.cpp), simulation [src/launcher/world.cpp](src/launcher/world.cpp)).

## Quick start

You need **[Emscripten](https://emscripten.org/docs/getting_started/downloads.html)**
(`emcc` on your `PATH`), `make`, `git`, and `python3` (for the dev server).

```sh
git clone https://github.com/LenyKholodov/webassembly-water-droplet.git
cd webassembly-water-droplet

# 1. Pull the third-party submodule (fast_obj) — the build fails without it
git submodule update --init --recursive

# 2. Build the WebAssembly bundle into dist/
make -j

# 3. Serve dist/ over HTTP (browsers block file:// XHR, so opening index.html directly won't work)
./run-webserver.sh

# 4. Open the game
open http://localhost:8080/
```

`make` compiles every `src/**/*.cpp`, links `dist/index.js` + `index.wasm`, and
**embeds the contents of `media/`** (textures, shaders, meshes) directly into the
bundle. See [docs/build.md](docs/build.md) for the full pipeline and flags.

**Useful variants:**

- `make RELEASE=1` — lean build: drops the wasm source map and runs a link-time
  `wasm-opt -O3` pass. Switch profiles with a clean build: `make clean && make RELEASE=1`.
- `python3 serve-nocache.py 8080` — alternative dev server that sends aggressive
  no-cache headers (so a reload always picks up a fresh build). Pass a host to expose
  it on your LAN for mobile testing: `python3 serve-nocache.py 8080 0.0.0.0`.

## Documentation

| Document | What's inside |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | Engine layers, subsystem responsibilities, the render pipeline, how a frame is driven. |
| [docs/entities.md](docs/entities.md) | Game objects — ferns, droplets — and how the world simulates them. |
| [docs/droplet-metaball-raymarch.md](docs/droplet-metaball-raymarch.md) | The metaball SDF fluid surface: math, shader, and tuning. |
| [docs/build.md](docs/build.md) | Makefile, Emscripten flags, output layout, running locally. |
| [docs/asset-pipeline.md](docs/asset-pipeline.md) | How `media/` textures, shaders, and meshes are authored, embedded, and loaded. |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | Notable changes over time. |

## Status

Droplet is a working **demo**, not a finished product. The simulation runs end to end
and the engine is reasonably layered, but the source still carries demo-era rough edges
(commented-out experiments, ad-hoc constants, no test suite).

## License & credits

> **Heads-up:** the **code** is MIT-licensed, but a few bundled **art assets** still
> need their provenance confirmed before redistribution (see the note below).

**Code** (`src/`, `include/`) is released under the **MIT License** — see
[LICENSE](LICENSE). The only vendored dependency,
[fast_obj](https://github.com/thisistherk/fast_obj), is MIT (© 2018 thisistherk) — its
license is kept in place under `third-party/fast_obj/`.

**Art and audio assets carry their own licenses, separate from the code.** Required
attributions:

- **Ferns model** and its derived textures/meshes (`media/meshes/fern.obj`, `leaf.obj`,
  `media/textures/ferns_1_*`) — based on
  ["Ferns lowpoly model"](https://sketchfab.com/3d-models/ferns-lowpoly-model-34fffcb1f90d4bb2a3bd362f38abbe80)
  by [adam127](https://sketchfab.com/adam127), licensed under
  [CC-BY-4.0](http://creativecommons.org/licenses/by/4.0/).
- **Skybox** — "The Milky Way panorama" by ESO / S. Brunier, licensed under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), reprojected to a cubemap.
- **Water-drop sound** — ["Water drop"](https://freesound.org/s/177156/) by
  [ABStudios](https://freesound.org/people/abstudios/), licensed under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **Water-droplet sound** — ["Water Droplet"](https://freesound.org/s/267221/) by
  [gkillhour](https://freesound.org/people/gkillhour/), licensed under
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).

> The `brickwall_*` (ground), `leaf_*`, and `stem_texture_*` textures are of
> **unconfirmed origin** and must be verified or replaced before redistribution. The
> background-music track that previously shipped here had unverifiable provenance and
> has already been **removed**.
