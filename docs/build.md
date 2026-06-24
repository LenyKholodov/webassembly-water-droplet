# Build &amp; Run Guide

This project ("Droplet") is a C++17 WebGL game compiled to WebAssembly with
[Emscripten](https://emscripten.org/). The entire build is driven by a single
[Makefile](../Makefile): every `.cpp` under `src/` is compiled with `emcc` into
a parallel `tmp/` object tree, then linked into `dist/index.js` + `dist/index.wasm`.
Art assets under `media/` are baked into the WASM virtual filesystem at link time.

This guide covers prerequisites, cloning (including the `fast_obj` submodule), the
Makefile in detail, build outputs, running locally, and deployment.

---

## Quickstart

```bash
# 1. Install & activate Emscripten (once)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh          # adds emcc/em++ to PATH for this shell

# 2. Clone this repo with its submodule
git clone <this-repo-url> droplet
cd droplet
git submodule update --init    # pulls third-party/fast_obj

# 3. Build (parallel)
make -j

# 4. Run the local dev server
./run-webserver.sh             # serves dist/ on http://localhost:8080/

# 5. Open the game
open http://localhost:8080/    # macOS; use your browser otherwise
```

> Remember to `source <emsdk>/emsdk_env.sh` in **every new shell** before running
> `make` — `emcc` is not installed system-wide.

---

## Prerequisites

| Tool | Purpose | Notes |
| --- | --- | --- |
| **Emscripten / emsdk** | C++ → WebAssembly compiler (`emcc`) | Provides SDL2, SDL2_image, Bullet, GLFW3 as *ports* (downloaded on first build) |
| **Python 3** | Local dev web server | Used by [run-webserver.sh](../run-webserver.sh) via `python3 -m http.server` |
| **git** | Clone + submodule | `third-party/fast_obj` is a git submodule, not vendored |
| **GNU make** | Drives the build | Uses `$(wildcard …)` globbing and pattern rules |
| **A WebGL2 browser** | Runs the game | Chrome/Firefox/Safari; see the `file://` caveat under [Running](#running-locally) |

### Installing &amp; activating Emscripten

The Makefile invokes `emcc` directly, so it must be on your `PATH`. The standard
way is via the emsdk:

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest      # downloads a toolchain (clang/wasm/node)
./emsdk activate latest     # marks it as the default
source ./emsdk_env.sh       # exports EMSDK + adds emcc to PATH (current shell only)
```

Verify the toolchain is live:

```bash
emcc --version
```

The first `make` after install will additionally download the Emscripten **ports**
this project links against — SDL2 (`USE_SDL=2`), SDL2_image (`USE_SDL_IMAGE=2`),
Bullet physics (`USE_BULLET=1`), and GLFW3 (`USE_GLFW=3`). These are **not vendored
in the repo**; expect the first build to be slower while they fetch and compile.

### Python (dev server)

Any Python 3 works. [run-webserver.sh](../run-webserver.sh) simply does:

```bash
cd dist
python3 -m http.server 8080
```

The port `8080` matters: the WASM source map is linked with
`--source-map-base "http://localhost:8080/"` (see [Link flags](#link-flags-link_flags)),
so debugging maps only resolve when served from that exact origin.

---

## Cloning &amp; the `fast_obj` submodule

The OBJ/MTL parser lives in [third-party/fast_obj](../third-party/fast_obj) and is a
**git submodule** declared in [.gitmodules](../.gitmodules):

```ini
[submodule "third-party/fast_obj"]
	path = third-party/fast_obj
	url = https://github.com/thisistherk/fast_obj.git
```

A plain `git clone` leaves `third-party/fast_obj/` **empty**, and the build will fail
because [src/media/geometry_mesh_obj_model.cpp](../src/media/geometry_mesh_obj_model.cpp)
`#include`s `third-party/fast_obj/fast_obj.h` (compiled inline via
`#define FAST_OBJ_IMPLEMENTATION`). Always initialize the submodule:

```bash
git clone <this-repo-url> droplet
cd droplet
git submodule update --init
```

Or clone recursively in one step:

```bash
git clone --recurse-submodules <this-repo-url> droplet
```

---

## The Makefile in detail

The whole build is [Makefile](../Makefile) (37 lines). Below is what each piece does.

### Targets

```make
.PHONY: all build clean
```

| Target | Effect |
| --- | --- |
| `all` | Default goal — just an alias for `build`. |
| `build` | Depends on `$(TARGET)` = `dist/index.js`. Compiles every source to `tmp/…/*.o`, then links the JS/WASM bundle. |
| `clean` | `rm -rf tmp dist/index.js` — removes the object tree and the JS entry point. (It does **not** delete `dist/index.wasm`, `.wasm.map`, or `.data`; those are regenerated on the next link.) |

```
make        → all → build → dist/index.js
make build  → dist/index.js
make clean  → rm -rf tmp/  dist/index.js
```

### Source discovery &amp; object layout

```make
TMP_DIR := tmp
SRCS := $(wildcard src/**/*.cpp) $(wildcard src/**/**/*.cpp) \
        $(wildcard src/**/*.c)   $(wildcard src/*.cpp) $(wildcard src/*.c)
OBJS := $(patsubst %.cpp,$(TMP_DIR)/%.o,$(SRCS))
```

- Sources are gathered with `$(wildcard …)`. Note that `**` is **not** a recursive
  globstar in GNU make — `src/**/*.cpp` expands as `src/*/*.cpp` (two path segments,
  e.g. `src/media/image.cpp`) and `src/**/**/*.cpp` as `src/*/*/*.cpp` (three segments,
  e.g. `src/launcher/hull/hull.cpp`). Together they cover the current tree, but any
  source nested **4+ levels deep**, or placed directly in `src/` as a one-off, would
  be silently dropped. The patterns are intentionally flat-ish and fragile.
- Each object is built into a **parallel `tmp/` tree** that mirrors the source path:
  `src/render/low_level/device.cpp` → `tmp/src/render/low_level/device.o`.

The pattern rule that does the compile:

```make
$(TMP_DIR)/%.o: %.cpp
	@echo Compiling $(notdir $<)...
	@mkdir -p $(dir $@)
	@emcc $(CC_FLAGS) -c $< -o $@
```

`mkdir -p $(dir $@)` creates the mirrored sub-directory on demand, so the `tmp/`
tree is built lazily as objects are compiled.

> The `$(SRCS)→$(OBJS)` `patsubst` only rewrites `.cpp`. The `.c` patterns in `SRCS`
> are collected but produce no `.o` rule, so any C source would not be compiled —
> currently there are none in `src/`, so this is latent.

### The link rule

```make
OUT_DIR := dist
TARGET  := $(OUT_DIR)/index.js

$(TARGET): $(OBJS) $(MEDIA_FILES)
	@echo Linking $(notdir $@)...
	@emcc $(LINK_FLAGS) $(OBJS) -o $@
```

`$(MEDIA_FILES)` is a prerequisite, so **touching any embedded asset relinks** the bundle.

### Compile flags (`CC_FLAGS`)

```make
INCLUDE_DIRS := include .
CC_FLAGS := -std=c++17 ${INCLUDE_DIRS:%=-I%}
CC_FLAGS += -O3 -Wbad-function-cast -Wcast-function-type
CC_FLAGS += $(COMMON_FLAGS)
#CC_FLAGS += -g3 --tracing #remove, only for debug info
```

| Flag | Meaning |
| --- | --- |
| `-std=c++17` | The engine uses C++17 (structured bindings, `if constexpr`, etc.). |
| `-Iinclude -I.` | Adds `include/` (so `<media/geometry.h>` resolves) **and** the repo root (so `<third-party/fast_obj/fast_obj.h>` resolves). |
| `-O3` | Full optimization. The math library's small fixed-size loops rely on the optimizer to unroll (the SSE path is disabled on WASM). |
| `-Wbad-function-cast -Wcast-function-type` | Extra warnings around the C-style callback casts (GLFW/Emscripten/Bullet trampolines). |
| `$(COMMON_FLAGS)` | Shared compile **and** link flags — see below. The SDL/Bullet ports must be on the compile line too, since their headers are included by `src/media/image.cpp` and the physics code. |
| `-g3 --tracing` *(commented)* | Debug-info toggle. Uncomment to get DWARF debug info and Emscripten tracing; left off for release size. |

### Common flags (`COMMON_FLAGS`) — used for both compile &amp; link

```make
COMMON_FLAGS += -s USE_SDL=2 -sUSE_SDL_IMAGE=2 \
                -s SDL2_IMAGE_FORMATS='["png","jpg"]' -s USE_BULLET=1
```

| Flag | Meaning |
| --- | --- |
| `-s USE_SDL=2` | Pull the SDL2 Emscripten port. |
| `-sUSE_SDL_IMAGE=2` | Pull the SDL2_image port. Used by [src/media/image.cpp](../src/media/image.cpp) (`IMG_Load`, then `SDL_ConvertSurfaceFormat` to `SDL_PIXELFORMAT_ABGR8888`). |
| `-s SDL2_IMAGE_FORMATS='["png","jpg"]'` | Compile SDL2_image with **only** PNG and JPG decoders — matching the texture formats under `media/textures/`. Smaller than the default format set. |
| `-s USE_BULLET=1` | Pull the Bullet physics port. The launcher's rigid-body simulation ([src/launcher/world.cpp](../src/launcher/world.cpp)) is built entirely on Bullet (`btDiscreteDynamicsWorld`, convex-hull tessellation, etc.). |

These appear in both `CC_FLAGS` and `LINK_FLAGS` so the port headers are visible
at compile time and the port libraries are linked in.

### Link flags (`LINK_FLAGS`)

```make
LINK_FLAGS := -s WASM=1  -sNO_DISABLE_EXCEPTION_CATCHING \
              -s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]'
LINK_FLAGS += -sALLOW_MEMORY_GROWTH
LINK_FLAGS += -gsource-map --source-map-base "http://localhost:8080/"
LINK_FLAGS += $(COMMON_FLAGS)
#LINK_FLAGS += -sSTACK_SIZE=50MB -sINITIAL_MEMORY=120MB
LINK_FLAGS += $(MEDIA_FILES:%=--embed-file "%") -s USE_GLFW=3
```

| Flag | Meaning |
| --- | --- |
| `-s WASM=1` | Emit a WebAssembly binary (`index.wasm`) rather than asm.js. |
| `-sNO_DISABLE_EXCEPTION_CATCHING` | **Keep C++ exception catching enabled.** The engine throws `engine::common::Exception` pervasively (asset loading, shader compile/link in [src/render/low_level/shader.cpp](../src/render/low_level/shader.cpp) and `device.cpp`, `engine_check*` guards). Without this, `catch` blocks are stripped and those throws would `abort`. There is a runtime size/perf cost to enabling exceptions in WASM. |
| `-s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]'` | Export `Module.UTF8ToString` to JS so `EM_ASM` blocks can convert C string pointers. Used by [src/launcher/sound_player.cpp](../src/launcher/sound_player.cpp) to turn C audio-path strings into JS strings for the browser `Audio` API. (The logging bridge in [src/common/log.cpp](../src/common/log.cpp) also relies on `Module.UTF8ToString`.) |
| `-sALLOW_MEMORY_GROWTH` | Let the WASM heap grow at runtime instead of failing on a fixed cap. The droplet/hull simulation allocates dynamically per frame. |
| `-gsource-map` | Emit a source map (`index.wasm.map`) for in-browser debugging. |
| `--source-map-base "http://localhost:8080/"` | Base URL the browser uses to fetch map/source files. **Hard-coded to the local dev server** — see the deploy caveat below. |
| `$(COMMON_FLAGS)` | The SDL/SDL_image/Bullet ports (link side). |
| `-s USE_GLFW=3` | Pull the GLFW3 port for windowing/input. The [application](../src/application) subsystem creates the window and routes input through GLFW3; on web the loop is driven by the browser. |
| `--embed-file "<path>"` (per asset) | Bake assets into the WASM virtual filesystem — see below. |
| `-sSTACK_SIZE=50MB -sINITIAL_MEMORY=120MB` *(commented)* | Prior memory tuning, left disabled. Re-enable if you hit stack/heap pressure. |

### Asset packaging (`--embed-file`)

```make
MEDIA_FILES := $(wildcard media/textures/*) $(wildcard media/shaders/*) \
               $(wildcard media/meshes/*)
# ...
LINK_FLAGS += $(MEDIA_FILES:%=--embed-file "%") ...
```

Every file under `media/textures/`, `media/shaders/`, and `media/meshes/` is embedded
into the WASM virtual filesystem (Emscripten MEMFS), **preserving its relative path**.
This is what produces `dist/index.data` (~2 MB). At runtime the C++ code opens assets
by literal relative path and they "just work" via MEMFS — no `fetch` required:

- shaders: `"media/shaders/forward_lighting.glsl"`, `"media/shaders/fresnel.glsl"`, …
- meshes:  `"media/meshes/leaf.obj"`, `"media/meshes/fern.obj"` (+ their `.mtl`)
- textures: `"media/textures/brickwall_diffuse.jpg"`, the `sky_*.png` cubemap faces, …

Because `MEDIA_FILES` is also a link prerequisite, **editing any asset triggers a relink**.

> **Audio is deliberately NOT embedded.** `dist/sounds/*` (music + WAV SFX) is excluded
> from `--embed-file` and served as ordinary HTTP assets, played by the browser via
> `new Audio(path)` inside `EM_ASM` JS in [src/launcher/sound_player.cpp](../src/launcher/sound_player.cpp).
> That is why `dist/sounds/` is committed to git while the generated bundle files are
> git-ignored.
>
> A stray `media/textures/.DS_Store` (if present) would be embedded by the
> `media/textures/*` wildcard — harmless, but it bloats `index.data`.

### Parallel builds (`make -j`)

Each object file has an independent pattern rule (`$(TMP_DIR)/%.o: %.cpp`) with no
inter-object dependencies, so the compile stage parallelizes cleanly:

```bash
make -j        # use all available cores
make -j8       # cap at 8 parallel jobs
```

GNU make schedules the per-`.cpp` compiles concurrently; only the final link
(`$(TARGET): $(OBJS) …`) waits for all objects. Incremental rebuilds recompile only
the sources whose `.o` is older than the `.cpp`, then relink.

---

## Build outputs

A successful `make` produces these in `dist/`:

| File | What it is |
| --- | --- |
| `dist/index.js` | Emscripten JS glue — streams the WASM, mounts the embedded FS, calls `main()`. The `make` target. |
| `dist/index.wasm` | The compiled WebAssembly binary (the engine + game). |
| `dist/index.wasm.map` | Source map for the WASM (from `-gsource-map`). |
| `dist/index.data` | The embedded virtual-FS blob (all of `media/textures`, `media/shaders`, `media/meshes`). |

These four are **git-ignored** via the repo-root [.gitignore](../.gitignore):

```gitignore
dist/index.js
dist/index.wasm
dist/index.wasm.map
dist/index.data
tmp/
.DS_Store
```

Everything else in `dist/` **is** committed and required to run:

- `dist/index.html` — the HTML shell (see below).
- `dist/sounds/` — the runtime-fetched audio.

### The HTML shell (`dist/index.html`)

[dist/index.html](../dist/index.html) is intentionally tiny. It declares a
full-viewport `<canvas id="canvas">` (with `oncontextmenu` suppressed so the right
mouse button can drive the camera), some instruction text, and — crucially — defines
the Emscripten `Module` object **before** loading the glue:

```js
var canv = document.getElementById('canvas');
var isMusicPl = false;
var Module = {
    canvas: canv,
    isMusicPlaying: isMusicPl
};
```

`Module.canvas` tells Emscripten/GLFW3/SDL2 which DOM canvas to render into;
`Module.isMusicPlaying` is a custom flag the C++ side reads/writes via `EM_ASM`
([src/launcher/sound_player.cpp](../src/launcher/sound_player.cpp)) to gate music
autoplay behind the first user gesture. `<script src="index.js">` then boots the module.

---

## Running locally

Use the helper script, which serves the `dist/` directory:

```bash
./run-webserver.sh
# → cd dist && python3 -m http.server 8080
```

Then open:

```
http://localhost:8080/
```

> **Do not open `index.html` via `file://`.** The Emscripten glue loads `index.wasm`
> and `index.data` over XHR; Chrome blocks `XMLHttpRequest` against `file://` URLs for
> security, so the module fails to start. You must serve over HTTP — the dev server
> above is the simplest way.

The port `8080` is significant: it must match the `--source-map-base` baked into the
WASM map for browser source-mapping to resolve. If you change the port, source maps
break (the game still runs).

### Controls (from the in-page instructions)

- **Drag leaves** to stagger them and create a path for droplets — touch+drag on
  mobile, or left mouse button on desktop.
- **Camera** (desktop): `W/A/S/D` + right mouse button held.

---

## Deployment (`deploy.sh`)

> **`deploy.sh` is untracked** (it does not appear in git history) and contains a
> **hard-coded host**. Treat it as a personal convenience script, not a maintained
> deploy pipeline.

Its entire contents:

```bash
scp -r dist/* root@134.209.73.43:/var/www/html/droplet
```

It copies everything in `dist/` (the JS/WASM bundle **plus** `index.html` and `sounds/`)
to a specific DigitalOcean droplet over `scp`.

### Caveats &amp; recommendation

1. **Hard-coded host &amp; path.** The `root@134.209.73.43:/var/www/html/droplet`
   destination is wired in. Parameterize it before reusing:

   ```bash
   #!/usr/bin/env bash
   set -euo pipefail
   HOST="${DEPLOY_HOST:?set DEPLOY_HOST, e.g. root@example.com}"
   DEST="${DEPLOY_DEST:-/var/www/html/droplet}"
   scp -r dist/* "${HOST}:${DEST}"
   ```

   …then `DEPLOY_HOST=user@host ./deploy.sh`.

2. **Source-map base mismatch.** The WASM map is linked with
   `--source-map-base "http://localhost:8080/"`. On a production host the browser will
   try to fetch sources from `localhost:8080` and fail. For a real deploy, either change
   `--source-map-base` in the [Makefile](../Makefile) to the production origin, or drop
   `-gsource-map`/`--source-map-base` entirely (and don't ship `index.wasm.map`).

3. **Untracked file.** Because it is not in git, `deploy.sh` is easy to lose. Consider
   committing a parameterized version.

---

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| `emcc: command not found` | `emsdk_env.sh` not sourced in this shell — `source <emsdk>/emsdk_env.sh`. |
| `fatal error: 'third-party/fast_obj/fast_obj.h' file not found` | Submodule not initialized — `git submodule update --init`. |
| First build is very slow / fetches ports | Expected — SDL2/SDL2_image/Bullet/GLFW3 ports download on first use. |
| Page is blank / "failed to load" over `file://` | Serve over HTTP (`./run-webserver.sh`); Chrome blocks `file://` XHR. |
| Source maps don't resolve | Server not on port `8080`, or `--source-map-base` doesn't match the serving origin. |
| A new source file isn't compiled | It's nested 4+ levels under `src/` or directly in `src/` — the `**` globs miss it; add a wildcard pattern in `SRCS`. |
| Asset change not reflected | Relink: `make` (asset edits trigger a relink via the `$(MEDIA_FILES)` prerequisite). For a clean slate, `make clean && make`. |
