# Procedural flower — chosen technical references

The droplet demo is getting a **real procedural growing flower** to replace the fake "scale a
static `fern.obj`" growth (see `generate_plant()` / `Plant` in
[src/launcher/world.cpp](../src/launcher/world.cpp)). Approach **A — parametric developmental
flower** driven by a water "growth budget" `g ∈ [0,1]`, with golden‑angle phyllotaxis for the head.

This file records the sources we are actually building from, why each was chosen, and which part of
the flower it informs. The first prototype lives in
[src/launcher/flower.h](../src/launcher/flower.h) / [flower.cpp](../src/launcher/flower.cpp).

## Primary sources (driving the implementation)

### 1. Ijiri, Owada, Okabe, Igarashi — *Floral diagrams and inflorescences* (SIGGRAPH 2005 / ACM TOG 24(3))
- **Link:** <https://dl.acm.org/doi/10.1145/1073204.1073253> · project page (figures + video):
  <https://takashiijiri.com/projects/ProjTakaFlower/index.html>
- **Why chosen:** the canonical CG treatment of *botanically structured* flowers. It separates
  **structural editing** (the *floral diagram* — what components sit where on a receptacle, and the
  *inflorescence* — how multiple flowers are arranged on an axis) from **geometric editing** (the
  shape of each component). That separation is exactly our `g`‑driven parametric model: a structural
  layout we can grow, plus per‑component geometry.
- **Informs:** overall flower topology — receptacle → whorls of petals → florets; the data model for
  "what to place and where" before we worry about each petal's surface.

### 2. Prusinkiewicz & Lindenmayer — *The Algorithmic Beauty of Plants* (ABOP), esp. Ch. 4 "Phyllotaxis"
- **Link (free PDF):** <http://algorithmicbotany.org/papers/abop/abop.pdf>
- **Why chosen:** the authoritative derivation of **phyllotaxis** and the **Vogel model** for the
  sunflower head: `θ(n) = n · 137.5°`, `r(n) = c · √n`. This is the spiral packing of florets in the
  flower head and the angular placement of organs around the stem.
- **Informs:** the **flower‑head floret layout** (golden‑angle spiral) and the divergence angle used
  to place petals/leaves around the axis.

### 3. Runions, Lane, Prusinkiewicz — *Modeling Trees with a Space Colonization Algorithm* (and the explanatory video)
- **Link:** paper <http://algorithmicbotany.org/papers/colonization.egwnp2007.html> · video walk‑through
  <https://vimeo.com/201449534>
- **Why chosen:** reference for the **stem/branch** side — growth that *chases* attractor points
  (where water lands). We are **not** adopting full space colonization for v1 (overkill for a single
  flower), but it is the model we will reach for when the stem needs to bend toward water or branch
  into an inflorescence. Kept as a forward‑looking reference, not a v1 dependency.
- **Informs:** future water‑seeking stem curvature / branching; v1 uses a simple parametric swept
  tube instead.

## Supporting / background

- **Vogel, H. (1979)** *A better way to construct the sunflower head*, Math. Biosciences 44 — the
  original `r = c√n`, `θ = n·137.5°` formulation that ABOP Ch.4 builds on.
- **Deussen & Lintermann** *Digital Design of Nature* — book‑length survey of procedural plant
  modeling; useful for petal surface and organ‑shape ideas.
- Practical write‑ups consulted for the math and a working feel of the Vogel disk:
  - Sunflower florets / golden ratio: <https://thatsmaths.com/2014/06/05/sunflowers-and-fibonacci-models-of-efficiency/>
  - Interactive phyllotaxis lab: <https://iris.joshua-becker.com/lab/phyllotaxis/>
  - Phyllotaxis growth sim in C++: <https://markus-x-buchholz.medium.com/phyllotaxis-growth-simulations-in-c-imggui-76aff14e0121>

## Ruled out (recorded so we don't revisit)

- **SpeedTree** — no WebGL/WASM/Emscripten target; runtime only plays pre‑baked static assets;
  "growth" is authoring‑time Alembic with no resource drivers; free tier can't export.
- **Drop‑in OSS growth libs** (`proctree`, `klingerj/Trees`) — reference only; CUDA/desktop‑GL or
  static one‑shot, not a fit for in‑engine `g`‑driven growth.

## How each source maps to the v1 prototype

| Flower part            | Source                | Technique in `flower.cpp`                          |
|------------------------|-----------------------|----------------------------------------------------|
| Stem                   | (parametric; Runions for future) | swept tube along a gently bent curve, tapered radius, parallel‑transport frame |
| Flower‑head florets    | ABOP Ch.4 / Vogel     | golden‑angle spiral, `θ=n·137.5°`, `r=c·√n`, count ∝ `g` |
| Petal whorl            | Ijiri 2005            | `K` petals on the receptacle, opening angle ∝ `g`  |
| Growth driver `g`      | (our integration)     | fed by `fallen_droplet_particles_count` at the leaf contact |
