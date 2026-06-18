# Droplet Metaball Raymarch — Implementation Plan

## Context

Droplets are currently visualized as a **convex hull** built per frame from the cluster's
particle positions ([hull.cpp](../src/launcher/hull/hull.cpp)), Loop-subdivided, re-uploaded as a
mesh every frame, and shaded by [fresnel.glsl](../media/shaders/fresnel.glsl) against a **per-droplet
environment cubemap** rendered from the cluster center ([mirrors_render_pass.cpp](../src/render/scene_passes/mirrors_render_pass.cpp)).

A convex hull cannot represent concavity, necking, or merging — every droplet reads as a rounded
blob — and its normals are **faked radially** (`normalize(position - center)`, the `//???` at
[hull.cpp:146](../src/launcher/hull/hull.cpp#L146)). The goal is a true fluid surface: replace the
hull with a **metaball signed-distance field raymarched in a fragment shader**, which yields real
concavity/necking/merges and **exact analytic normals**.

Key decision: the per-droplet **cubemap is kept and reused** for reflection + refraction exactly as
today. The raymarch changes *only* how the surface point and normal are computed; the optical
shading (cubemap reflect/refract + Fresnel + lights) is reused from
[fresnel.glsl](../media/shaders/fresnel.glsl). The cubemap's render cost (plan item O3) is a
separate, later optimization. Both the new raymarch path and the old hull path are kept behind a
**compile-time toggle** so they can be A/B-compared in-engine while tuning; the hull is removed in a
later change once the raymarch is satisfactory.

This is feasible with **no new low-level engine plumbing**: uniform arrays, per-draw property maps,
per-entity user-data, and cubemap texture binding are all already-used patterns.

---

## Why it's cheap in this engine (verified facts)

- **Per-droplet dynamic uniforms** flow through the per-draw `PropertyMap` argument of
  `Pass::add_mesh(...)` ([device.h:666-673](../include/render/device.h#L666-L673)), stored per-primitive
  and merged into the binding context at draw time. The forward pass currently passes
  `Pass::default_primitive_properties()` at
  [forward_render_passes.cpp:173](../src/render/scene_passes/forward_render_passes.cpp#L173) — the
  single injection point.
- **Uniform arrays already work**: `properties.set("name", std::vector<vec4f>)` → `glUniform4fv`,
  identical to `pointLightPositions[32]`
  ([light_pre_pass.cpp:263](../src/render/scene_passes/light_pre_pass.cpp#L263),
  [pass.cpp:404-406](../src/render/low_level/pass.cpp#L404-L406)). Constant-bound `for` loops with
  `break` are the existing GLSL idiom ([fresnel.glsl:117](../media/shaders/fresnel.glsl#L117)).
- **Per-node data carrier**: scene nodes support `set_user_data<T>()/find_user_data<T>()`
  ([node.h:91-100](../include/scene/node.h#L91-L100)); `EnvironmentMap`/`WaterReflection` already use
  it. The droplet material is **shared by name**
  ([world.cpp:576-594](../src/launcher/world.cpp#L576-L594)), so per-droplet values must travel via
  the per-draw PropertyMap, not the material.
- **Cubemap is already bound** to the droplet draw: the forward pass picks `envmap->textures`
  (containing `"environmentMap"`) for env-map entities
  ([forward_render_passes.cpp:165-169](../src/render/scene_passes/forward_render_passes.cpp#L165-L169),
  cubemap created in [shared.h:104-125](../src/render/scene_passes/shared.h#L104-L125)).
- **Shader registration** is a fixed list: add a program-file constant + program + pass + state, then
  `pass_group.add_pass("droplet_fluid", ...)` mirroring `fresnel`
  ([forward_render_passes.cpp:17-21, 31-40, 67-72](../src/render/scene_passes/forward_render_passes.cpp#L17-L72)).
- **Shaders are embedded at build time** (`--embed-file`), so a shader edit needs `make` (re-link),
  not just a reload.

---

## Step 1 — New shader: `media/shaders/droplet_fluid.glsl`

A new GLSL ES file in the `#shader vertex` / `#shader pixel` convention. It is the **fresnel pixel
shading wrapped in a metaball raymarch**.

**Uniforms (new):**
- `uniform vec4 particles[MAX_DROPLET_PARTICLES];` — world-space center `.xyz` + per-particle radius `.w`
- `uniform int particleCount;`
- `uniform vec3 dropletCenter;`
- `uniform float influenceRadius;` — metaball blend radius (tuning)
- `uniform float isoThreshold;` — surface level (tuning)
- reuse existing: `modelMatrix`, `MVP`, `worldViewPosition`, `environmentMap` (samplerCube),
  the point/spot light arrays, `eta`, Fresnel `F`/`fresnelPower`, `envFactor`.

**Vertex shader:** transform the proxy-box vertex (`gl_Position = MVP * vec4(vPosition,1)`), pass the
world-space position so the fragment shader builds the ray `ro = worldViewPosition`,
`rd = normalize(worldPos - worldViewPosition)`.

**Fragment shader:**
1. **SDF** `float map(vec3 p)`: smooth-union of sphere SDFs over `particles[0..particleCount]` using
   polynomial `smin` with `k = influenceRadius`. Constant loop bound `MAX_DROPLET_PARTICLES` with
   `if (i >= particleCount) break;`.
2. **Sphere-trace** from the box entry: step by `map()` distance, ~48 step cap, hit when `d < eps`; on
   exit/over-distance → `discard` (lets the opaque scene show through).
3. **Normal** = normalized gradient of `map()` (central differences) → exact analytic normal.
4. **Shade** = port of [fresnel.glsl:100-159](../media/shaders/fresnel.glsl#L100-L159):
   `reflectDir`/`refractDir` from the hit normal, `textureCube(environmentMap, …)` for both, Fresnel
   mix, point/spot diffuse, mix by `envFactor`. Same cubemap, same look — just a real normal.
5. **Thickness (optional v1.5):** accumulate marched in-fluid distance for a Beer-Lambert tint knob.
6. **Depth (optional v1.5):** write `gl_FragDepth` at the hit for exact occlusion vs leaves/water.

`MAX_DROPLET_PARTICLES` = 64 (typical droplet ≈24 particles; `PREFERRED_MAX_DROPLETS_COUNT=3`).

---

## Step 2 — Register the shader/pass (render layer)

In [forward_render_passes.cpp](../src/render/scene_passes/forward_render_passes.cpp), mirroring `fresnel`:
- add `DROPLET_FLUID_PROGRAM_FILE = "media/shaders/droplet_fluid.glsl"`
- create `droplet_fluid_program` + `droplet_fluid_pass`
- copy `fresnel_pass` state: opaque `DepthStencilState(true,true,CompareMode_Less)`,
  `RasterizerState(false)`, `Clear_None`
- `pass_group.add_pass("droplet_fluid", droplet_fluid_pass, N)`; `remove_all_primitives()` +
  `set_frame_buffer(...)` it in `render()` like the others.

---

## Step 3 — Feed per-droplet uniforms (render layer, one-line injection)

In `render_mesh`
([forward_render_passes.cpp:157-175](../src/render/scene_passes/forward_render_passes.cpp#L157-L175)),
read a per-node PropertyMap instead of the default:

```cpp
common::PropertyMap* fluid_props = mesh.find_user_data<common::PropertyMap>();
const common::PropertyMap& props = fluid_props ? *fluid_props
                                               : Pass::default_primitive_properties();
pass_group.add_mesh(renderable_mesh->mesh, mesh.world_tm(),
  mesh.first_primitive(), mesh.primitives_count(), props, prim_textures);
```

`prim_textures` unchanged — droplets keep the `envmap->textures` branch, so `environmentMap` binds as
today.

---

## Step 4 — Gameplay wiring (launcher)

In [world.cpp](../src/launcher/world.cpp): add `const bool DROPLET_RAYMARCH = true;` near
`DROPLET_DEBUG_DRAW`, plus tuning constants.

**Material:** alongside the `"droplet"`/`"fresnel"` material, create a `"droplet_fluid"` material
with `set_shader_tags("droplet_fluid")` and the same cubemap-carrying texture set.

**Per-droplet mesh setup** (when `DROPLET_RAYMARCH`): in the hull-creation block set the mesh
geometry to a **proxy box** (`MeshFactory::create_box("droplet_fluid", 1,1,1)`) instead of the hull
builder's mesh; keep `set_environment_map_required(true)` and the `environment_map_local_point`
update so the cubemap still renders from the cluster center.

**Per-frame update** (configure/build loop), when raymarch is on, **skip `build_hull`** and instead:
- gather up to `MAX_DROPLET_PARTICLES` particle world positions from `droplet->bodies` into a
  `std::vector<vec4f>` (`xyz` = position, `w` = `DROPLET_PARTICLE_RADIUS`); subsample if more.
- bounding radius = `max|pos − center| + influenceRadius`; position the proxy mesh node at
  `droplet->center`, scale it to that radius (box must enclose the iso-surface).
- build a `common::PropertyMap` with `particles`, `particleCount`, `dropletCenter`,
  `influenceRadius`, `isoThreshold` (+ tuning scalars), attach via `hull_mesh->set_user_data(props)`
  each frame.

When `DROPLET_RAYMARCH` is false, the existing hull path runs unchanged.

---

## Step 5 — Tuning

Iterate constants in [world.cpp](../src/launcher/world.cpp) (and shader defaults), rebuilding between
changes: `influenceRadius`/`smin k` (blobbiness/merge), `isoThreshold` (inflation), `eta`/`F`/
`fresnelPower`/`envFactor` (refraction/reflection balance — start from fresnel.glsl values), raymarch
`eps`/step cap (quality vs fill), optional Beer-Lambert tint.

---

## Critical files

| File | Change |
|------|--------|
| `media/shaders/droplet_fluid.glsl` | **new** — metaball SDF raymarch + cubemap shading |
| `src/render/scene_passes/forward_render_passes.cpp` | register `droplet_fluid` pass; read per-node PropertyMap in `render_mesh` |
| `src/launcher/world.cpp` | `DROPLET_RAYMARCH` toggle + tuning consts; `"droplet_fluid"` material; proxy-box geometry; per-frame particle PropertyMap upload; skip `build_hull` when on |

No changes to the low-level render API, the mirrors/cubemap pass, or `hull.cpp` (hull stays as the
toggle-off path; removed later).

---

## Verification

```bash
make -j               # re-embeds the new .glsl and relinks
./run-webserver.sh    # serves dist/ on http://localhost:8080
```

**Functional:** builds/runs clean; raymarch droplets render as smooth refractive blobs sampling the
cubemap; two droplets merge with visible necking (vs hull); smooth curvature highlights; sub-min
droplets hide; console counts match. **A/B & perf:** `DROPLET_RAYMARCH=false` ≡ hull baseline;
frame time ≤ hull path; watch large on-screen droplet fill cost. **Edge cases:** `particleCount`
clamps at `MAX_DROPLET_PARTICLES`; camera-near-droplet ray start clamped.
