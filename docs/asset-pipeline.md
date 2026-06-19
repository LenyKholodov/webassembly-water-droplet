# Asset Pipeline

This document describes the full asset story for the **Droplet** WebAssembly demo: where assets live on disk, how each asset class is authored and loaded, how they are baked into the WASM bundle, how C++ code reads them at runtime, and how to add new assets. It also records the attribution/licensing obligations that ship with the art and audio.

---

## 1. Directory Layout

Assets are split into two camps by **delivery mechanism**: files baked into the WASM virtual filesystem at build time, and files served as plain HTTP resources at runtime.

```
test1/
├── media/                         # build-time embedded assets (--embed-file)
│   ├── textures/                  # jpg/png 2D maps + 6-face skybox cubemap
│   │   ├── brickwall_diffuse.jpg  brickwall_normal.jpg  brickwall_specular.jpg
│   │   ├── floor_diffuse.jpg      floor_normal.jpg      floor_specular.jpg
│   │   ├── stone_diffuse.jpg      stone_normal.jpg      stone_specular.jpg
│   │   ├── leaf_color.png         leaf_normal.png       stem_texture_01.png
│   │   ├── ferns_1_diffuse.png    ferns_1_normal.png
│   │   ├── projectile.png
│   │   └── sky_posx.png sky_negx.png sky_posy.png sky_negy.png sky_posz.png sky_negz.png
│   ├── meshes/                    # Wavefront OBJ geometry + MTL materials
│   │   ├── leaf.obj  leaf.mtl     (~880 KB obj; vertex-deduped at load)
│   │   └── fern.obj  fern.mtl
│   └── shaders/                   # combined single-file GLSL programs
│       ├── forward_lighting.glsl  fresnel.glsl  sky.glsl
│       ├── lighting.glsl          phong.glsl    phong_gbuffer.glsl
│       ├── lpp_geometry.glsl      shadow.glsl   simple.glsl
│       └── projectile.glsl
└── dist/
    └── sounds/                    # runtime-fetched audio (NOT embedded)
        ├── music.mp3
        ├── 177156__abstudios__water-drop.wav
        └── 267221__gkillhour__water-droplet.wav
```

The split matters: **`media/*` is compiled into the binary** and is addressed by relative path inside Emscripten's MEMFS; **`dist/sounds/*` is fetched over HTTP** by the browser's `Audio` API. This is why `dist/sounds/` is committed to git while the generated `dist/index.{js,wasm,wasm.map,data}` are git-ignored.

> Housekeeping note: `media/textures/.DS_Store` exists in the tree and **would be embedded** by the `media/textures/*` wildcard. It is harmless but bloats `dist/index.data`; remove it before shipping.

---

## 2. Asset Classes

### 2.1 Textures (`media/textures/`)

**Format & authoring.** JPG and PNG only — these are the two formats whitelisted in the build (`SDL2_IMAGE_FORMATS='["png","jpg"]'` in the [Makefile](../Makefile)). JPGs are used for opaque tiling surfaces (brick/floor/stone), PNGs where alpha or higher fidelity is wanted (leaf, fern, sky, projectile).

**Loading.** On the web target, decoding goes through SDL2_image in [src/media/image.cpp](../src/media/image.cpp):

```cpp
SDL_Surface* image = IMG_Load(path);
...
if (image->format->format != SDL_PIXELFORMAT_ABGR8888)
  SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_ABGR8888, 0);
```

Every image is normalized to RGBA8 (`ABGR8888`) so the GPU layer always receives a consistent layout. The resulting `media::image::Image` is then uploaded by `render::low_level::Device::create_texture2d(...)` / `create_texture_cubemap(...)`. (A native macOS path, [src/media/image.mm](../src/media/image.mm), uses AppKit/CoreImage and is not part of the WASM build.)

**Diffuse / normal / specular convention.** Surface materials follow a three-map naming convention `<name>_diffuse`, `<name>_normal`, `<name>_specular`, wired up by name in the launcher and by MTL fields in the meshes:

| Suffix | Role | Sampler uniform | Example |
|---|---|---|---|
| `_diffuse` | base/albedo color | `diffuseTexture` | `brickwall_diffuse.jpg` |
| `_normal` | tangent-space normal map | `normalTexture` | `brickwall_normal.jpg` |
| `_specular` | specular intensity | `specularTexture` | `brickwall_specular.jpg` |

The launcher binds the brick set explicitly ([src/launcher/main.cpp](../src/launcher/main.cpp)):

```cpp
Texture model_diffuse_texture  = render_device.create_texture2d("media/textures/brickwall_diffuse.jpg");
Texture model_normal_texture   = render_device.create_texture2d("media/textures/brickwall_normal.jpg");
Texture model_specular_texture = render_device.create_texture2d("media/textures/brickwall_specular.jpg");
```

The shaders consume those samplers by name — e.g. [phong.glsl](../media/shaders/phong.glsl) and [phong_gbuffer.glsl](../media/shaders/phong_gbuffer.glsl) sample `diffuseTexture`/`normalTexture`/`specularTexture` and build a tangent frame from screen-space derivatives. The normal map is unpacked the standard way: `texture(normalTexture, texCoord).xyz * 2.0 - 1.0`.

**The `sky_*` cubemap.** The skybox is six PNG faces, one per cube direction. They are loaded as a *single* cubemap from a **base path that does not exist on disk** ([src/launcher/world.cpp](../src/launcher/world.cpp)):

```cpp
const char* SKY_TEXTURE_PATH = "media/textures/sky.png";   // no such file
...
Texture sky_texture = render_device.create_texture_cubemap(SKY_TEXTURE_PATH);
```

`Device::create_texture_cubemap` ([src/render/low_level/device.cpp](../src/render/low_level/device.cpp)) splits the base path at its last `.` and synthesizes the six real filenames by inserting a face suffix before the extension:

```cpp
static const char* FACES[] = { "_posx", "_negx", "_posy", "_negy", "_posz", "_negz" };
// "media/textures/sky.png" -> "media/textures/sky_posx.png", "media/textures/sky_negx.png", ...
```

So passing the non-existent `sky.png` base is **intentional**, not a bug — it expands to the on-disk `sky_posx.png` … `sky_negz.png`. All six faces must be the same dimensions (asserted via `engine_check`). The cubemap is then bound to the `"sky"` material as its `diffuseTexture` and sampled in [sky.glsl](../media/shaders/sky.glsl) (`textureCube(diffuseTexture, ...) * 0.05`), and reused as the `environmentMap` for the refractive droplet shader.

### 2.2 Meshes (`media/meshes/`)

**Format & authoring.** Wavefront `.obj` geometry paired with `.mtl` materials, exported from Blender. Two models ship: `leaf.{obj,mtl}` (the draggable leaves and stems — large at ~880 KB) and `fern.{obj,mtl}` (the ground ferns). Paths are referenced by literal in [src/launcher/world.cpp](../src/launcher/world.cpp):

```cpp
const char* LEAF_MESH  = "media/meshes/leaf.obj";
const char* PLANT_MESH = "media/meshes/fern.obj";
```

**Loading.** `MeshFactory::load_obj_model` ([src/media/geometry_mesh_obj_model.cpp](../src/media/geometry_mesh_obj_model.cpp)) parses OBJ/MTL via the vendored single-header **`third-party/fast_obj`** submodule (compiled inline with `#define FAST_OBJ_IMPLEMENTATION`). The loader:

- Rebuilds a single interleaved `media::geometry::Vertex` buffer (position/normal/color/tex_coord) by hashing each `(position, texcoord, normal)` index tuple so duplicate "wedges" collapse — this is the main cost mitigation for the big `leaf.obj`.
- Splits faces into a `Primitive` per material run within each OBJ group; each primitive is named after its group.
- Maps MTL fields to the engine material model:

| MTL field | Engine property / texture |
|---|---|
| `Kd` | `diffuseColor` |
| `Ka` | `ambientColor` |
| `Ks` | `specularColor` |
| `Ke` | `emissionColor` |
| `Ns` | `shininess` |
| `map_Kd` | `diffuseTexture` |
| `map_Ka` | `ambientTexture` |
| `map_Ks` | `specularTexture` |
| `map_Ke` | `emissionTexture` |
| `map_Bump` / `map_bump` | `normalTexture` |

**MTL texture paths are relative to the mesh file** and reach back into the textures directory, e.g. [media/meshes/leaf.mtl](../media/meshes/leaf.mtl):

```
map_Kd   ../textures/stem_texture_01.png
map_Ks   ../textures/stem_texture_01.png
map_Bump ../textures/leaf_normal.png
```

> Gotcha: the gameplay code keys off **exact OBJ primitive/group names**. Leaves usable as draggable physics bodies must be named `leave_*`, with `leaf_*` primitives used as pivot references (see [src/launcher/world.cpp](../src/launcher/world.cpp)). Renaming geometry in the OBJ silently breaks leaf physics. Also note `Mesh::index_type` is `uint16_t`, capping any single mesh at 65 535 vertices.

### 2.3 Shaders (`media/shaders/`)

Shaders are **combined single-file GLSL programs**: one `.glsl` file holds both the vertex and pixel stage, delimited by a custom `#shader` pragma. There is no general `#include` system — the only preprocessor conventions are the `#shader` splitter plus standard GLSL `#define`/`#ifdef`/`#extension`.

```glsl
#shader vertex
... vertex stage source ...

#shader pixel
... pixel stage source ...
```

**Parsing & `#line` injection.** `Device::create_program_from_file` reads the file with `common::load_file_as_string` (opened `"rt"`, text mode) then hands it to `create_program_from_source` ([src/render/low_level/device.cpp](../src/render/low_level/device.cpp)). A hand-rolled `strstr_with_line_numbers` scans for each `#shader` tag while counting `\n`, so the **original line number** of each stage is captured (`source.lineno`). When the stage is compiled, `ShaderImpl` ([src/render/low_level/shader.cpp](../src/render/low_level/shader.cpp)) prepends a `#line <offset>` directive:

```cpp
engine::common::xsnprintf(line_number_buffer, sizeof line_number_buffer, "#line %d\n", lineno_offset);
... glShaderSource(shader_id, 2, sources, sources_length);  // [ "#line N\n", stage_body ]
```

so GLSL compiler errors report line numbers matching the original `.glsl` file rather than the extracted fragment.

**Cross-target GLSL.** Some shaders branch on `#ifndef GL_ES` to emit desktop `#version 410 core` (`in`/`out`/`texture`) vs. WebGL/GLES ES (`precision mediump float`, `attribute`/`varying`, `gl_FragColor`/`texture2D`). Common aliasing helpers appear throughout: `#define outColor gl_FragColor`, `#define texture texture2D`.

**Reflection-driven binding.** After link, `Program::Impl` enumerates `GL_ACTIVE_UNIFORMS`, strips trailing `[0]` from array names, hashes each name, and maps GL types to engine `PropertyType_*` (flagging `GL_SAMPLER_*` as samplers). This reflection table is what lets materials/passes drive uniforms **by name** — the renderer hardcodes only a handful of built-in transform uniforms (`MVP`, `modelMatrix`, `viewMatrix`, `modelViewMatrix`, `worldViewPosition`, …) and the four fixed attribute names (`vPosition`/`vNormal`/`vColor`/`vTexCoord`).

**Material → shader dispatch.** Shaders are loaded by hard-coded path in the scene-pass constructors and registered in a `PassGroup` under a string **shader tag**. At render time, the material's `shader_tags()` selects the matching pass (`""`/default, `"fresnel"`, `"sky"`). For example `droplet_material.set_shader_tags("fresnel")` and `sky_material.set_shader_tags("sky")` in [src/launcher/world.cpp](../src/launcher/world.cpp).

**The shader set:**

| Shader | Role | Target / notes |
|---|---|---|
| [forward_lighting.glsl](../media/shaders/forward_lighting.glsl) | **The shipping web lighting pass.** Per-fragment Phong over ≤32 point + ≤2 spot lights, alpha-test `discard` below `0.01`, spot shadowing via projected `spotLightShadowMatrices` + PCF. | GLES `mediump`. WebGL1 lacks `dFdx/dFdy`, so its `CotangentFrame` falls back to a faked derivative helper `dfd()` — tangent frame is **approximate** on web. |
| [lighting.glsl](../media/shaders/lighting.glsl) | Deferred/light-pre-pass resolve: same lighting math but samples `positionTexture`/`normalTexture`/`albedoTexture`/`specularTexture`/`shadowTexture` from a G-buffer instead of model attributes. | Dual ES/desktop. Marked `//TODO optimize`. |
| [phong.glsl](../media/shaders/phong.glsl) | Standard forward Phong with `dFdx/dFdy` cotangent-frame normal mapping; outputs a single lit color. | Desktop `#version 410 core`. |
| [phong_gbuffer.glsl](../media/shaders/phong_gbuffer.glsl) | Deferred **G-buffer fill**: MRT outputs `outPosition`/`outNormal`/`outAlbedo`/`outSpecular`. `shininess` packed via `SHININESS_NORMALIZER=1000.0` (RGBA8 precision workaround). | Desktop only. |
| [lpp_geometry.glsl](../media/shaders/lpp_geometry.glsl) | Light-pre-pass geometry stage: writes mapped normals only (uses `#extension GL_OES_standard_derivatives`). | GLES `mediump`. |
| [shadow.glsl](../media/shaders/shadow.glsl) | Shadow-map depth pass: position-only vertex stage, **empty fragment** (depth-only render). | Dual ES/desktop. |
| [sky.glsl](../media/shaders/sky.glsl) | Skybox: samples `samplerCube diffuseTexture` by normalized position, dimmed `*0.05`. Material tag `"sky"`. | GLES `mediump`. |
| [simple.glsl](../media/shaders/simple.glsl) | Debug/test G-buffer fill writing constant albedo/specular. | Desktop only. |
| [fresnel.glsl](../media/shaders/fresnel.glsl) | **The water-droplet shader.** Per-vertex Schlick fresnel (`F=0.05`, `fresnelPower=5.0`, `eta=0.0`), reflect/refract dirs sampled from `samplerCube environmentMap`, `mix`ed by fresnel then blended with diffuse lighting by `envFactor=0.85`. Material tag `"fresnel"`. | GLES `mediump`. |
| [projectile.glsl](../media/shaders/projectile.glsl) | Screen-space droplet/"projectile" decal: projects `projectileTexture` through `shadowMatrix` with PCF, modulated by `projectileColor`, blended additively into the G-buffer. | Desktop only; depends on the deferred G-buffer. The fragment reads its own `outNormal` output (undefined behavior) — flag if reusing. |

In the actual in-browser build, only **`forward_lighting`**, **`fresnel`** and **`sky`** are exercised (the launcher enables `"Forward Lighting"` + `"Mirrors"`); the deferred/LPP/projectile shaders are part of the desktop path and disabled in the web/forward build.

### 2.4 Sounds (`dist/sounds/`)

**Format & authoring.** One music track (`music.mp3`) and two SFX `.wav` files sourced from [Freesound](https://freesound.org/). Audio is **not** part of the C++/WASM filesystem; it is played directly by the browser. Paths are relative to `dist/` ([src/launcher/sound_player.cpp](../src/launcher/sound_player.cpp)):

```cpp
const char* MUSIC_PATH               = "sounds/music.mp3";
const char* SOUND_DROPLET_GROUND_PATH = "sounds/177156__abstudios__water-drop.wav";   // ground impact
const char* SOUND_DROPLET_LEAF_PATH   = "sounds/267221__gkillhour__water-droplet.wav"; // leaf impact
```

`SoundPlayer` is Emscripten-only and plays through the browser `Audio` API via `EM_ASM`/`EM_JS`. Autoplay is gated behind the first user gesture and the `Module.isMusicPlaying` flag set up in [dist/index.html](../dist/index.html) — browsers/mobile block audio until a user interaction resolves an `audio.play()` promise. The Freesound numeric prefixes (`177156`, `267221`) are the original Freesound sound IDs and double as attribution anchors (see §5).

---

## 3. How Assets Get Into the WASM Bundle

The embed mechanism lives entirely in the [Makefile](../Makefile). Three asset directories are globbed and each file is passed to `emcc` via `--embed-file`:

```makefile
MEDIA_FILES := $(wildcard media/textures/*) $(wildcard media/shaders/*) $(wildcard media/meshes/*)
...
LINK_FLAGS += $(MEDIA_FILES:%=--embed-file "%") -s USE_GLFW=3
```

At link time, `emcc` bakes every matched file into Emscripten's **virtual filesystem (MEMFS)**, *preserving its relative path* (`media/...`). The byte blob is emitted as `dist/index.data` (~2 MB), which the generated `dist/index.js` glue mounts into MEMFS before calling `main()`. Because `MEDIA_FILES` is also a link prerequisite (`$(TARGET): $(OBJS) $(MEDIA_FILES)`), editing any asset forces a relink.

**How C++ reads them by path.** Since the embedded files keep their `media/...` path, runtime code simply opens them by literal relative path and they "just work" through MEMFS — no `fetch`, no async:

- Shaders → `common::load_file_as_string("media/shaders/forward_lighting.glsl")` via `fopen(path, "rt")` in [src/common/file.cpp](../src/common/file.cpp).
- Meshes → `MeshFactory::load_obj_model("media/meshes/leaf.obj")` via `fast_obj_read`.
- Textures → `IMG_Load("media/textures/brickwall_diffuse.jpg")` via SDL2_image.

```
                 build time                                   runtime (browser)
  media/textures/*  ─┐
  media/shaders/*   ─┼─ emcc --embed-file ──► dist/index.data ──► MEMFS ──► fopen()/IMG_Load()
  media/meshes/*    ─┘                                                       by "media/..." path

  dist/sounds/*  ───────────── (NOT embedded) ──────────────► HTTP GET ──► new Audio("sounds/...")
```

**Build flags relevant to assets** (from the Makefile):

| Flag | Purpose |
|---|---|
| `-sUSE_SDL=2 -sUSE_SDL_IMAGE=2` | SDL2 + SDL2_image ports (image decode) |
| `-s SDL2_IMAGE_FORMATS='["png","jpg"]'` | restricts the decoder to PNG + JPG |
| `--embed-file "<path>"` | bakes a file into MEMFS at its relative path |
| `-s USE_GLFW=3` | GLFW3 windowing/input port |
| `-s USE_BULLET=1` | Bullet physics port |
| `-s 'EXPORTED_RUNTIME_METHODS=["UTF8ToString"]'` | lets `EM_ASM` audio JS read C string paths |
| `-sALLOW_MEMORY_GROWTH` | grow heap as assets/data load |

> Gotcha: the source globbing for **code** (`SRCS`) uses non-recursive `**` (resolves to `src/*/*.cpp` and `src/*/*/*.cpp`); the asset globs above are flat single-level `dir/*` wildcards. A new asset added directly under `media/textures/`, `media/shaders/`, or `media/meshes/` is picked up automatically; a new **sub-directory** of assets would not be (the wildcard does not descend).

---

## 4. How to Add a New Asset

### Add a new texture
1. Drop the `.png` or `.jpg` into `media/textures/` (PNG/JPG only — other formats won't decode). Follow the `<name>_diffuse` / `_normal` / `_specular` convention if it's a surface material.
2. Reference it by path in code: `render_device.create_texture2d("media/textures/<name>_diffuse.jpg")`, or list it as a `map_Kd`/`map_Bump`/`map_Ks` in an MTL (relative to the mesh, e.g. `../textures/<name>.png`).
3. Bind it into a `Material`'s texture list under the sampler name the shader expects (`diffuseTexture`, `normalTexture`, `specularTexture`, …).
4. Rebuild (`make`) — the `media/textures/*` wildcard embeds it automatically and the relink picks up the new file.

For a **cubemap**, add six faces `name_posx.png … name_negz.png` and call `create_texture_cubemap("media/textures/name.png")` with the (non-existent) base path; the loader expands the suffixes.

### Add a new mesh
1. Export `model.obj` + `model.mtl` into `media/meshes/`. Keep MTL texture paths relative (`../textures/...`).
2. Name primitives/groups deliberately if gameplay depends on them (`leave_*` for draggable leaves, `leaf_*` for pivots).
3. Keep meshes under 65 535 vertices (16-bit indices). The loader auto-dedups vertices.
4. Load via `MeshFactory::load_obj_model("media/meshes/model.obj")`.
5. Rebuild — the `media/meshes/*` wildcard embeds both the OBJ and MTL.

### Add a new shader
1. Create `media/shaders/<name>.glsl` with a `#shader vertex` section and a `#shader pixel` section.
2. For dual-target support, guard desktop GLSL with `#ifndef GL_ES` and provide a GLES path; use `#define texture texture2D` / `#define outColor gl_FragColor` aliases as needed. WebGL1 has no `dFdx/dFdy` — provide a fallback (see `dfd()` in `forward_lighting.glsl`).
3. Consume textures/params by **uniform name** (they bind via active-uniform reflection); rely on the built-in transform uniforms (`MVP`, `modelMatrix`, …) for matrices and the fixed attributes `vPosition`/`vNormal`/`vColor`/`vTexCoord`.
4. Load it in a scene pass via `device.create_program_from_file("media/shaders/<name>.glsl")`, register the pass in a `PassGroup` under a shader tag, and set that tag on the material (`material.set_shader_tags("<tag>")`).
5. Rebuild — the `media/shaders/*` wildcard embeds it. Compiler errors will report original line numbers thanks to `#line` injection.

### Add a new sound
1. Place the `.mp3`/`.wav` in `dist/sounds/` (this directory is committed to git; audio is **not** WASM-embedded).
2. Add a path constant in [src/launcher/sound_player.cpp](../src/launcher/sound_player.cpp) (relative to `dist/`, e.g. `"sounds/<file>.wav"`) and a `SoundId` enum value in [src/launcher/shared.h](../src/launcher/shared.h); wire it into the playback `EM_ASM`/`EM_JS` JS.
3. Record its source/license in the attribution table below — especially for Freesound assets.
4. No relink is required for audio; it is fetched over HTTP at runtime. Just deploy the file alongside the page.

---

## 5. Attribution & Licensing Obligations

These third-party assets carry attribution/license obligations that **must** be honored before publishing or redistributing the demo. Confirm the exact license of each on its source page and reproduce the required credit.

| Asset | Source | Obligation |
|---|---|---|
| `media/textures/ferns_1_diffuse.png`, `ferns_1_normal.png` (fern art) | Third-party fern texture set, CC-BY | **CC-BY: attribution required.** Credit the original author and link the source/license in the published docs and in-app/README credits. |
| `dist/sounds/177156__abstudios__water-drop.wav` | [Freesound #177156](https://freesound.org/s/177156/) by *abstudios* | Honor the per-sound Freesound license (CC0 / CC-BY / CC-Sampling+). If attribution-bearing, credit *abstudios* and the Freesound ID. |
| `dist/sounds/267221__gkillhour__water-droplet.wav` | [Freesound #267221](https://freesound.org/s/267221/) by *gkillhour* | Honor the per-sound Freesound license. If attribution-bearing, credit *gkillhour* and the Freesound ID. |
| `dist/sounds/music.mp3` | (music track) | Verify the license before redistribution; add a credit line. |

Freesound filenames already embed the sound ID and uploader (`<id>__<user>__<title>.wav`), which is the canonical attribution anchor — preserve those names so credits stay traceable.

Ensure every attribution entry above is preserved before the repository is made public.
