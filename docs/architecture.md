# System Architecture

This document describes the architecture of **Droplet** — a small WebAssembly browser game built on a custom C++ rendering/physics engine. It covers the high-level design, the layered subsystem structure, the application and frame loop, the end-to-end rendering pipeline, the physics/gameplay loop, the recurring engine idioms, and a directory map.

For the gameplay-level data model (leaves, droplets, particles, plants, water surface, hull reconstruction), see [entities.md](entities.md).

---

## 1. High-level overview & tech stack

Droplet renders an interactive scene of physically-simulated leaves through which water droplets fall. Droplets are particle clusters whose surfaces are reconstructed each frame into smooth blobs and shaded with a Fresnel/environment-map water material; where water reaches the ground, ferns grow; a procedural water plane ripples in the background.

The engine is a self-contained C++17 codebase with no engine framework dependencies — only the standard library and a handful of platform/third-party libraries. It targets the browser via Emscripten (the shipping target) and has a parallel native macOS path for development.

| Concern | Technology |
|---|---|
| Language / standard | C++17 (`-std=c++17`, see [Makefile](../Makefile)) |
| Primary build target | WebAssembly via Emscripten (`emcc`) |
| Windowing / input | GLFW3 (Emscripten port `-s USE_GLFW=3`; native Cocoa via vendored config) |
| Graphics API | WebGL 2 / OpenGL ES on web; OpenGL 4.1 core + GLAD on desktop |
| Physics | Bullet (`-s USE_BULLET=1`) |
| Image decoding | SDL2 + SDL2_image (web); AppKit/CoreImage (macOS) |
| Mesh import | `third-party/fast_obj` (git submodule) for Wavefront OBJ |
| Audio | Browser `Audio` API via `EM_ASM`/`EM_JS` (Emscripten only) |
| Math | Header-only template linear-algebra library (`include/math/`) |
| Asset delivery | Emscripten `--embed-file` MEMFS for textures/shaders/meshes; HTTP fetch for audio |

The codebase is split into a reusable **engine** (everything under `engine::common`, `engine::math`, `engine::media`, `engine::render`, `engine::scene`, `engine::application`) and a **game** (`src/launcher/`, the `main()` entry point and `World` simulation). Nothing in the engine depends on the launcher.

---

## 2. Layered architecture

The system is strictly layered: each layer depends only on layers below it. The `engine::common` foundation and `engine::math` library sit at the bottom with no engine dependencies; the launcher sits at the top and wires everything together.

```
                       ┌──────────────────────────────┐
                       │   launcher / game (Droplet)   │   src/launcher/*
                       │  World sim · hull · sound · main()
                       └───────────────┬──────────────┘
                                       │
              ┌────────────────────────┼────────────────────────┐
              ▼                        ▼                         ▼
   ┌────────────────────┐  ┌────────────────────────┐  ┌──────────────────┐
   │    application     │  │  render/scene +        │  │   scene graph    │
   │  Application,Window│  │  render/scene_passes   │  │  Node, Mesh,     │
   │  (GLFW/Emscripten) │  │  SceneRenderer, passes │  │  Camera, Light   │
   └─────────┬──────────┘  └───────────┬────────────┘  └────────┬─────────┘
             │                         │                        │
             │                         ▼                        │
             │              ┌────────────────────────┐          │
             │              │   render/low_level     │          │
             │              │  Device, Pass, Texture,│          │
             │              │  FrameBuffer, Program  │          │
             │              └───────────┬────────────┘          │
             │                          │                       │
             │                          ▼                       ▼
             │              ┌────────────────────────────────────────┐
             │              │            media (asset model)          │
             │              │  geometry::Mesh/Material/Model, image    │
             │              └───────────────────┬────────────────────┘
             │                                  │
             ▼                                  ▼
   ┌────────────────────────────────────────────────────────────────┐
   │                          common (foundation)                    │
   │   Exception, log, string, Component, PropertyMap, NamedDict…    │
   └───────────────────────────────┬────────────────────────────────┘
                                   │ (property_map only)
                                   ▼
   ┌────────────────────────────────────────────────────────────────┐
   │                       math (header-only)                        │
   │            vector, matrix, quat, plane, angle, utility           │
   └────────────────────────────────────────────────────────────────┘
```

### Layer responsibilities

| Layer | Namespace | Depends on | Role |
|---|---|---|---|
| **math** | `engine::math` | stdlib only | Header-only vectors/matrices/quaternions/planes/angles + transform builders. The lowest numerical layer. |
| **common** | `engine::common` | stdlib (+ `math` only via `property_map.h`) | Zero-engine-dependency utilities: exceptions/assertions, logging, strings, hashing, `PropertyMap`, `NamedDictionary`, `UninitializedStorage`, file IO, and the self-registering `Component` plugin mechanism. |
| **media** | `engine::media` | `math`, `common` | CPU-side, API-agnostic asset model: `geometry::Vertex/Mesh/Material/Model`, OBJ loading, procedural primitives, `image::Image`. Holds no GPU resources. |
| **render/low_level** | `engine::render::low_level` | `application`, `media`, `common`, `math` | Thin GPU abstraction over WebGL/GL ES: `Device` (resource factory), `Pass`, `Texture`, `FrameBuffer`, `Program`, `Mesh`, name-based uniform binding via `BindingContext`. |
| **render/scene + scene_passes** | `engine::render::scene` (+ `::passes`) | `render/low_level`, `scene`, `media`, `common`, `math` | Pipeline orchestration: `SceneRenderer`, the per-frame pass DAG (`FrameNode`), `SceneVisitor`, and the concrete passes (deferred, forward, light-pre-pass, shadows, mirrors, projectiles), each a self-registering `Component`. |
| **scene** | `engine::scene` | `math`, `common`, `media` | Pure-CPU node hierarchy: `Node` with lazy world transforms, `Mesh`/`Entity`, `Camera`, `Light`, `Projectile`, and the `ISceneVisitor` double-dispatch traversal. |
| **application** | `engine::application` | `common`, GLFW, Emscripten/Cocoa | Platform/window/main-loop facade. Isolates the rest of the engine from GLFW and Emscripten. |
| **launcher** | (game) | all of the above + Bullet | The game `World`, hull reconstruction, sound, and `main()`. Depended on by nothing. |

> Note the one upward-looking edge: `render/low_level` depends on `application` for `Window` (it needs `handle()` and framebuffer dimensions). Apart from that, dependencies flow strictly downward.

---

## 3. The application & frame / main loop

The shell lives in `engine::application` ([application.h](../include/application/application.h), [window.h](../include/application/window.h)). It exposes exactly two facades, both PIMPL'd behind `std::shared_ptr<Impl>`:

- **`Application`** — process lifecycle, timing (`static double time()`, a thin `glfwGetTime()` wrapper), exit signaling (`exit()`/`has_exited()`), and the run loop (`main_loop(const IdleHandler&)`). `IdleHandler` is `std::function<size_t()>` returning a requested sleep in **milliseconds**.
- **`Window`** — GLFW window creation, `swap_buffers()`, logical vs. framebuffer dimensions (distinct for HiDPI), and three `std::function` input handlers (`KeyHandler`, `MouseButtonHandler`, `MouseMoveHandler`) with an engine-owned `Key`/`MouseButton` vocabulary decoupled from GLFW numbering.

### Dual main-loop dispatch

`Application::main_loop` (see [application.cpp](../src/application/application.cpp)) builds the loop body once into `impl->loop`, then branches on `__EMSCRIPTEN__`:

```
                    main_loop(idle_fn)
                          │
          builds impl->loop = { call idle_fn(); glfwWaitEventsTimeout(...); }
                          │
        ┌─────────────────┴──────────────────┐
   __EMSCRIPTEN__                         native
        │                                    │
   emscripten_set_main_loop_arg(        while (!has_exited())
     &Impl::main_loop, &*impl,             impl->loop();
     0 /*rAF*/, true /*infinite*/)
        │
   loop body checks has_exited() →
   emscripten_cancel_main_loop()
```

- **Web:** `emscripten_set_main_loop_arg(&Impl::main_loop, &*impl, 0, true)` — fps `0` means the browser drives via `requestAnimationFrame`; `simulate_infinite_loop = true`. The static `Impl::main_loop(void*)` trampoline (required by the Emscripten C API) casts the `void*` back to `Impl*` and invokes the stored `std::function`. The loop body caps `max_timeout` at `1000/60` (~16 ms) to stay frame-paced and calls `emscripten_cancel_main_loop()` when `has_exited()`.
- **Native:** a plain `while (!has_exited())` loop; after each idle call it blocks in `glfwWaitEventsTimeout(max_timeout/1000.0)` (event-driven, sleeps until input or timeout), with `max_timeout` capped at 1000 ms.

In both targets the idle handler's returned timeout can only *lower* the per-iteration cap, never raise it. Exceptions thrown by the idle function are caught and logged inside the loop, never propagated.

### The game's idle callback

`main()` in [main.cpp](../src/launcher/main.cpp) installs the per-frame callback via `app.main_loop([&]{ ... return TIMEOUT_MS; })` (`TIMEOUT_MS = 10`). Each frame it:

1. polls `window.should_close()` → `app.exit()`;
2. handles the Android first-interaction music-autoplay workaround (`force_music_play_started`);
3. runs `world.inputDrag(...)` then `world.update()` (the whole simulation step);
4. runs `sound_player.update()`;
5. lazily registers render passes on the first frame (`add_pass("Forward Lighting")`, `add_pass("Mirrors")` — deferred/LPP/projectile passes are commented out);
6. integrates WASD camera movement using `dt = app.time() - last_time`;
7. animates the spotlight;
8. calls `scene_renderer.render(scene_viewport)`;
9. `window.swap_buffers()`.

Input is routed through the three window handlers: the keyboard handler also gates music playback (`sound_player.play_music()`) to satisfy browser/mobile gesture-locked audio; the mouse handlers un-project the cursor into a world-space ray for leaf grab/drag and drive `world.inputGrab/inputDrag/inputRelease`.

---

## 4. The rendering pipeline, end-to-end

Rendering is orchestrated by `engine::render::scene` ([scene_render.h](../include/render/scene_render.h)) on top of the GPU abstraction in `engine::render::low_level` ([device.h](../include/render/device.h)). The pipeline turns a `scene::Node` tree plus a camera into ordered draw calls.

### 4.1 Key abstractions

| Type | Header | Role |
|---|---|---|
| `SceneRenderer` | scene_render.h | Owns the `Device`, the resolved pass list, shared `TextureList`/`MaterialList`/`PropertyMap`/`FrameNodeList`, and the render queue. Entry point: `render(viewport)`. |
| `SceneViewport` | scene_render.h | A framebuffer + viewport + view node + projection/subview matrices + per-viewport properties/textures + `ScenePassOptions`. |
| `ScenePassContext` | scene_render.h | Per-viewport object handed to every pass; carries frame/enumeration IDs, the `BindingContext`, view/projection TMs, and the root `FrameNode`. |
| `IScenePass` | scene_render.h | Pass interface: `get_dependencies()`, `prerender()`, `render()`. |
| `ScenePassFactory` | scene_render.h | Static name → `ScenePassCreator` registry. Passes self-register via `register_scene_pass(...)`. |
| `FrameNode` | scene_render.h | A node in the per-frame render DAG: passes + priorities + dependency frame nodes + properties/textures. |
| `low_level::Pass` / `PassGroup` | device.h | A framebuffer + program + GPU state + per-frame primitive queue; `PassGroup` dispatches by material shader-tag. |
| `BindingContext` | device.h | Parent-chained, name-based property/texture lookup node (up to two parents). |

### 4.2 Scene graph → visitor → draw calls

Each `scene::Mesh` carries a `media::geometry::Mesh` by value. Traversal uses a hard-coded double-dispatch visitor: `Node::traverse(ISceneVisitor&)` does pre-order DFS, and each concrete `visit` override chains base-class `visit` calls so a visitor can hook at any level of generality (see [visitor.h](../include/scene/visitor.h), [node.cpp](../src/scene/node.cpp)). The pipeline's `SceneVisitor` ([scene_visitor.cpp](../src/render/scene_passes/scene_visitor.cpp)) implements only the leaf overloads, binning nodes into typed buckets: `meshes`, `point_lights`, `spot_lights`, `projectiles`, `prerender_entities`. It honors `ScenePassOptions::excluded_nodes` (the mechanism that prevents a mirror from rendering itself).

End-to-end, a frame proceeds:

```
SceneRenderer::render(viewport)
  ├─ ++current_frame_id
  ├─ PRERENDER phase: each pass.prerender(context)
  │     • ShadowPass builds spot/projectile shadow maps
  │     • MirrorsPrerenderPass queues nested child renders (one per cubemap face)
  ├─ RENDER phase (is_in_rendering = true):
  │     • recurse queued children depth-first  (env-map faces render first)
  │     • each pass.render(context):
  │         SceneVisitor.traverse(root_node, &options)  → typed node buckets
  │         for each scene::Mesh:  RenderableMesh.get() → low_level::Mesh
  │                                Pass.add_mesh(gpu_mesh, world_tm, ...)
  │         pack point/spot lights into fixed-size uniform arrays
  │         append low_level::Pass(es) to a per-pass FrameNode
  │         root_frame_node().add_dependency(frame)
  │     • root_frame_node().render(context)  → walks DAG, Pass::render → GL draws
```

### 4.3 Pass resolution and the frame DAG

`SceneRenderer::add_pass(name, priority)` resolves passes by string name through `ScenePassFactory`, recursively instantiating each pass's declared string dependencies (DFS with cycle detection that throws a formatted "dependency loop" exception). Dependencies become parent→child edges, all sorted by priority via `std::stable_sort` (lower priority = earlier).

At render time the assembled **frame DAG** (`FrameNode::render`, [frame_node.cpp](../src/render/scene/frame_node.cpp)) is traversed post-order: dependency frames render first, then each pass builds a nested `BindingContext` and calls `low_level::Pass::render`. The DAG is **destructive** — after rendering it clears its `deps`/`passes`, so every pass must re-`add_pass`/`add_dependency` each frame. An **enumeration-ID** counter memoizes shared dependencies (e.g. one G-buffer referenced by several passes) so they run at most once per enumeration.

### 4.4 Binding-context stacking

Uniforms and samplers are bound **by name** from a parent-chained `BindingContext`, never hardcoded. Layers stack from broad to specific:

```
shared_textures / shared_properties (renderer)
  → viewport properties / textures
    → context self-properties  (viewMatrix, projectionMatrix, worldViewPosition)
      → frame properties / textures
        → pass-group properties
          → per-primitive properties
```

This is how passes communicate: the G-buffer pass registers `positionTexture`/`normalTexture`/`albedoTexture`/`specularTexture` into shared textures, and the lighting pass reads them by name. The low-level `Pass` reflects each program's active uniforms (stripping `[0]` from array names, mapping GL types to engine `PropertyType`, flagging samplers) and binds them from the chain, injecting the built-in transforms `viewMatrix`, `projectionMatrix`, `viewProjectionMatrix`, `MVP`, `modelMatrix`, `modelViewMatrix`.

### 4.5 The lighting passes

All lighting passes traverse the scene, then pack point lights (cap `MAX_LIGHTS_COUNT = 32`) and spot lights (cap `2`) into parallel uniform arrays, **zero-padded to the fixed cap** so shaders always see a constant-size array.

- **Deferred** ([deferred_render_passes.cpp](../src/render/scene_passes/deferred_render_passes.cpp)): `GBufferPass` renders into an MRT framebuffer (position/normal `RGB16F`, albedo/specular `RGBA8`, `D24` depth) registered as the `g_buffer` frame node; `DeferredLightingPass` draws a full-screen quad reading the G-buffer. **Desktop-only** — the component registers itself `#ifndef __EMSCRIPTEN__`, since WebGL1 MRT is unsupported by this abstraction.
- **Forward** ([forward_render_passes.cpp](../src/render/scene_passes/forward_render_passes.cpp)): **the shipping path.** `ForwardLightingPass` uses a `low_level::PassGroup` of three sub-passes dispatched by material shader-tag: default `forward_lighting`, `"fresnel"` (water droplets), and `"sky"` (skybox, culling off). `add_mesh` routes a mesh by its material's tags; meshes carrying an `EnvironmentMap` get the `"environmentMap"` cubemap bound for reflections.
- **Light-pre-pass** ([light_pre_pass.cpp](../src/render/scene_passes/light_pre_pass.cpp)): only `lpp::GeometryPass` is live — a thin normal (`RGBA8`) + `D16` depth buffer registered as `lpp_geometry_buffer`/`normalTexture`. The LPP lighting half is `#if 0`.

### 4.6 Shadows

`ShadowPass` ([shadow_render_passes.cpp](../src/render/scene_passes/shadow_render_passes.cpp)) runs only in `prerender`. For each spot light and each projectile it lazily creates a `Shadow` (depth-only `D24` map, `SHADOW_MAP_SIZE = 1024`, `media/shaders/shadow.glsl`), sets the light's view (`inverse(world_tm)`) and projection, caches `shadow_tm = projection * view`, renders all collected meshes into the depth pass, and exposes its `shadow_frame` so lighting passes can `add_dependency` on it. It guards on `current_frame_id` to avoid rebuilding shadows across sub-renders of one frame.

### 4.7 Environment-map / mirror prerendering

`MirrorsPrerenderPass` ([mirrors_render_pass.cpp](../src/render/scene_passes/mirrors_render_pass.cpp)) drives the reflective/refractive look. For each entity where `is_environment_map_required()`, it builds/fetches an `EnvironmentMap` (one 512² RGBA8 cubemap + shared `D16` depth + 6 per-face framebuffers), and for each of the 6 faces constructs a 90° perspective projection and a per-face `subview_tm`, then issues a **nested** `context.renderer().render(scene_viewport)` with `excluded_nodes` containing the entity itself.

This nesting is the **queue-a-child** mechanism: because `prerender` runs while `is_in_rendering` is false, the nested `render()` call appends a child `SceneRenderQueueEntry` rather than recursing immediately, and the children render depth-first in the subsequent render phase (so the env-map faces are ready before the main view samples them). Nested depth is capped at `MAX_NESTED_RENDER_DEPTH = 1`, so mirrors do not recurse into mirrors.

### 4.8 Projectile / water Fresnel

Two distinct effects produce the "water" look:

- **Fresnel droplet material** (`fresnel.glsl`, tagged `"fresnel"`, applied in the forward path): per-vertex Schlick Fresnel + reflect/refract directions sampling the entity's `environmentMap` cubemap, mixing refraction↔reflection by the Fresnel term and then with diffuse lighting. This is what makes each reconstructed droplet blob look like refractive water — it depends on the Mirrors pass having produced the env map, which is why droplets set `set_environment_map_required(true)`.
- **`ProjectilePass`** ([projectile_render_pass.cpp](../src/render/scene_passes/projectile_render_pass.cpp)): an additive droplet-decal accumulation pass that aliases the G-buffer's `albedoTexture`/`normalTexture` as its own color targets and blends Fresnel-weighted droplet splats back into them (`BlendState(true, One, One)`, depth test off, using each projectile's shadow/depth and `projectile.glsl`). Because it reads/writes the G-buffer, it only makes sense alongside the deferred path and is **disabled in the forward-only shipping build**.

> **Shipping configuration.** [main.cpp](../src/launcher/main.cpp) enables only `"Forward Lighting"` + `"Mirrors"`. The deferred and projectile passes are commented out there, deferred is additionally compiled out on web, and the LPP lighting half is `#if 0`. Forward + Mirrors + the `fresnel`/`sky` materials are the real web pipeline.

---

## 5. The physics & gameplay loop

The game logic lives in `World` ([world.cpp](../src/launcher/world.cpp), declared in [shared.h](../src/launcher/shared.h)), constructed with the scene root, the `SceneRenderer`, and the camera. It owns a Bullet world (`btDiscreteDynamicsWorld` with the standard collision-config / dispatcher / `btDbvtBroadphase` / `btSequentialImpulseConstraintSolver` stack), the loaded leaf/plant models, and the live entity vectors.

`World::update()` runs each frame in this order (high level — see [entities.md](entities.md) for the entity-level detail):

1. `stepSimulation(1/60, 10)`.
2. **Droplet spawning** — throttled spawning of sphere-particle clusters (capped at `MAX_PARTICLES_COUNT`).
3. **Leaf servo control** — each `Leaf` is driven toward a `target_transform` with central force + torque, and pinned by a `btPoint2PointConstraint` to a static anchor so it swings like a hinged flap the player can drag.
4. **Fallen-particle harvesting** — particles below a height threshold are flagged and removed.
5. **Clustering** — an iterative nearest-cluster pass groups particles into visible `Droplet` blobs, growing the cluster radius to keep the on-screen blob count low and stable.
6. **Hull reconstruction** — each droplet's filtered point cloud (statistical outlier rejection via per-axis variance) is fed to a `HullBuilder`: a Bullet `btConvexHullShape` → low-poly hull → **Loop-subdivision** smoothing (a half-edge mesh in [hull_loop_tesselation_smoother.cpp](../src/launcher/hull/hull_loop_tesselation_smoother.cpp)) → a smooth `scene::Mesh` with `set_environment_map_required(true)` for the Fresnel look.
7. **Fern growth** — accumulated fallen droplets spawn or scale up ferns at ground positions.
8. **Surface-tension forces** — per-particle forces toward the droplet center pull particles into coherent blobs.
9. **Transform sync** — each `PhysBodySync` copies its rigid body's motion-state transform into its scene mesh.
10. **Lights & sound** — droplet/zone lights are positioned; contact-sound gating plays leaf/ground droplet sounds via the global Bullet `gContactAddedCallback`.
11. **Water surface** — `WaterSurface::update()` runs a double-buffered discretized wave equation over a 128×128 grid, writing heights and finite-difference normals into a mesh and calling `mesh.touch()` to flag the GPU buffer dirty.

The central physics↔render binding object is **`PhysBodySync`**, a RAII component that owns the Bullet collision shape / motion state / rigid body and the bound `scene::Mesh::Pointer`: its constructor `addRigidBody`s into the world, its destructor `removeRigidBody`s. Bullet is a third-party dependency only of the launcher; nothing in the engine knows about it.

Entity types, constants, clustering math, hull subdivision, leaf shape construction, ray-picking, and the water wave step are documented in [entities.md](entities.md).

---

## 6. Cross-cutting idioms

These patterns recur in nearly every subsystem and are worth internalizing before reading the code.

### Reference-counted PIMPL components

Almost every public class hides its state behind `struct Impl;` and a smart pointer. Two flavors coexist:

- **`std::shared_ptr<Impl>`** (most of the engine: `Exception`, `PropertyMap`, `media::Mesh`/`Material`/`Image`, every `render` resource, `SceneRenderer`/`SceneViewport`/`FrameNode`, `Application`/`Window`, `World`/`SoundPlayer`/`HullBuilder`). This gives **value-semantic handles with shared ownership** — copying a handle is cheap and aliases the same backing data (and the same GPU object). GL/Bullet resources are released in the last `Impl` destructor.
- **`std::unique_ptr<Impl>`** (the scene graph: `Node` and each subclass). Note each level of a derived node owns its *own* `Impl` (e.g. a `Mesh` has `Mesh::Impl`, `Entity::Impl`, and `Node::Impl`).

Because copies share impls, passing a `media::geometry::Mesh` or `low_level::Material` by value is shallow — a deliberate, documented choice.

### Visitor (double dispatch)

`scene::ISceneVisitor` ([visitor.h](../include/scene/visitor.h)) declares one empty-default `visit(T&)` overload per concrete node type. `Node::traverse` recurses pre-order; each `visit` override chains its base then the most-derived overload, letting a visitor hook at any generality level. This is the sole bridge from the scene graph to the renderer, keeping the scene graph ignorant of render types.

### Self-registering Components (lazy plugin DI)

`common::Component` ([component.h](../include/common/component.h)) is an intrusive linked list of self-registering objects. Each concrete component is a `static` global (e.g. `static ForwardRenderingComponent component;` in [forward_render_passes.cpp](../src/render/scene_passes/forward_render_passes.cpp)) that links itself in at static-init time. `enable()`/`disable()` are reference-counted; the static `Component::enable(wildcard)` matches component type names against a glob. A `ComponentScope` RAII object enables matching components on construction and disables on destruction — `main()` uses `ComponentScope components("engine::render::scene::passes::*")` to load every render pass, each of which registers its creator with `ScenePassFactory` in `load()` and unregisters in `unload()`. This decouples pass availability from the renderer; the only coupling point is `add_pass("Name")`.

### Property maps & name-based binding

`common::PropertyMap` is a type-erased, ordered + name-indexed bag of typed values (`int`, `float`, `vecN`, `mat4`, and their array variants). It drives material parameters and is the backbone of name-based uniform binding: the low-level `Pass` reflects a program's active uniforms and resolves each from the `BindingContext` chain of `PropertyMap`s/`TextureList`s. Lookups go through `NamedDictionary`, a hashed `string → value` multimap that verifies full-string equality on collision.

### RAII guards and lazy caches

RAII is pervasive: `ComponentScope`, the `RenderingProcessGuard`/`RenderQueueGuard` in the renderer, `PhysBodySync` (Bullet body lifetime), and the macOS autorelease-pool guard. Per-node GPU caches (`RenderableMesh`, `Shadow`, `EnvironmentMap`, `RenderableProjectile`) are attached lazily via the scene graph's type-erased `Node::set_user_data<T>()`/`find_user_data<T>()`, so the render layer hangs state off scene nodes without the scene graph depending on it.

### Other recurring idioms

- **Transaction-ID dirty tracking** — `media::Mesh::update_transaction_id()`/`touch()` lets the render `Mesh` skip re-upload when geometry is unchanged (and the water/hull meshes call `touch()` to force re-upload).
- **Functor + RVO constructors** in the math library — every operation is a stateless `detail::` functor, enabling one code path to serve both the generic scalar loop and an SSE-specialized overload (the SSE path is MSVC-only and compiled out on web).
- **Factory** — `MeshFactory`, `Device`, `Node::create()`, `ScenePassFactory` are all factory entry points.

---

## 7. Directory layout

Top-level directories and their roles (verified against the working tree):

| Directory | Role |
|---|---|
| [include/](../include) | Public engine headers, mirroring the subsystem layers: `math/`, `common/`, `media/`, `render/`, `scene/`, `application/`. Each header `#include`s its `detail/*.inl` for inline/template implementations. |
| [src/](../src) | Engine + game implementation, one directory per subsystem: `common/`, `media/`, `scene/`, `render/low_level/`, `render/scene/`, `render/scene_passes/`, `application/` (+ `application/osx/`), and `launcher/` (+ `launcher/hull/`). |
| [media/](../media) | Build-time-embedded assets: `shaders/*.glsl` (combined `#shader vertex`/`#shader pixel` programs), `meshes/*.{obj,mtl}` (Wavefront geometry), `textures/*` (diffuse/normal/specular maps + a 6-face skybox cubemap). Embedded into the WASM MEMFS via `--embed-file`. |
| [third-party/](../third-party) | Vendored/submoduled dependencies. Currently `fast_obj/` (OBJ parser, a git submodule per [.gitmodules](../.gitmodules)). SDL2/SDL2_image, Bullet, and GLFW3 are pulled from Emscripten ports at build time, not vendored here. |
| [dist/](../dist) | Browser deliverables: `index.html` (canvas + `Module` shell), generated `index.{js,wasm,wasm.map,data}` (git-ignored), and `sounds/` (audio fetched over HTTP at runtime, **not** embedded in the WASM FS). |
| [docs/](../docs) | This documentation set: [README](README.md), [architecture](architecture.md), [entities](entities.md), [build](build.md), [asset-pipeline](asset-pipeline.md), [CHANGELOG](CHANGELOG.md), [plan](plan.md). |
| [tmp/](../tmp) | Build scratch — the parallel object-file tree (`tmp/src/.../*.o`) produced by the Makefile. |
| `Makefile` / `run-webserver.sh` / `deploy.sh` | The Emscripten build, the local Python dev server (`:8080`, matching the source-map base URL), and the `scp`-to-host deploy script. |

---

### Quick orientation for new contributors

- **Want to change how the scene looks?** Start in `src/render/scene_passes/` (passes) and `media/shaders/` (GLSL). Forward + Mirrors is the live path.
- **Want to change the gameplay?** It's all in [world.cpp](../src/launcher/world.cpp) and `src/launcher/hull/` — heavily tuning-driven (≈90 constants at the top of `world.cpp`). See [entities.md](entities.md).
- **Want to change the platform/loop?** `src/application/`.
- **Want to add an asset?** Drop it in `media/{shaders,meshes,textures}/` — the Makefile globs and embeds it, and runtime code opens it by literal relative path.
