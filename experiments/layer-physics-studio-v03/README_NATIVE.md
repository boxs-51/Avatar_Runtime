# LPS v0.3 — native committed-mesh integration, experimental

This slice is the first **native CPU** bridge between the v0.3 XPBD design and
`alpha`'s real `PreparedScene2D` geometry. It does not modify the immutable
`ValidatedCpuModel`, physics3 adapter, ABI20, source authorization, renderer,
GPU ownership, or the default-off configurations.

## Current verifiable path

1. Native model -> existing isolated admission -> actual committed Runtime pose.
2. `prepare_committed_2d` generates **all** native 2D draw parts, including
   parameter-based deformation, hierarchy visibility, opacity and z order.
3. `native_mesh_xpbd.hpp::Session` explicitly maps selected `part_index` and
   `vertex_index` bindings onto that already-validated mesh. It cannot create or
   remove parts or change UV/triangles. Nonselected parts remain present.
4. `prepare` produces a separate XPBD candidate with triangle-edge constraints,
   user-specified cross-part distance joints and two-way circle contacts.
   Committed source movement updates the pinned vertex and target pose offset.
5. Only an external **successful GPU publication transaction** may call
   `accept`. This test harness deliberately calls it manually; no D3D11 host
   connection or interactive Win32 Viewer is claimed.

## Native test programs

```
cmake -S experiments/layer-physics-studio-v03/native -B build/lps-native
cmake --build build/lps-native
ctest --test-dir build/lps-native --output-on-failure --no-tests=error
```

With `ARS_BUILD_LPS_NATIVE_TESTS=ON` and the opt-in public CPU/D3D11 profiles on
Windows, `lps_v03_committed_native_cpu` uses the existing generated dual-mode
`.arschar` fixture, actual source authority/update/capture, 2D and GLB scene
projection, checks XPBD overlays retain every 2D drawable, checks animation
motion/replay and confirms 3D draw identity is untouched. See CI workflow.

## Explicit unfinished gates

- Native XPBD state is not yet held across `PublicD3D11Host` candidate/query/
  copy-ready/accepted publication. GPU query failure must never commit solver
  state. `source model id + pose generation + configuration + target generation`
  must be checked before admitting any candidate.
- A real interactive native Viewer/editor (and whole-model PNG readback to a
  visible window) has not been built. The browser v0.3 editor is NOT that Viewer.
- Native GLB3D and private MOC3 drawables are not deformed by the XPBD bridge.
  Their original render paths remain untouched; full native parity is HOLD.
- No physical Windows host/device evidence has been collected; Windows GitHub
  runner's acceptance is CI evidence only, not user-host native proof.
- Physical shape contacts, edges, time-of-impact, renderer-target-recreation,
  device loss/recovery, and performance qualification remain open.

The native side is intentionally nonshipping and must stay in this experiment
branch until separately approved and independently reviewed.

## Windows-native pixel probe (new experimental application)
`ars_lps_native_pixels accepted-model.arschar [output-prefix]` loads the
entire **supported native2D** model through real runtime admission/pose,
projects its full original drawable set and displays no fake sprites.
It renders a neutral frame and 24 XPBD-affected native frames via D3D11 WARP,
checks exact query completion before calling solver `accept`, and requires a
nonzero before/after pixel difference. With an optional output prefix, it
writes two PPM images for visual inspection; CI retains these as artifacts.

This is a **headless native WARP pixel-probe**, not yet an interactive desktop
window. Unsupported models are rejected rather than approximated. GLB and
MOC3 are still not deformed. The public D3D11 publication fence, lifecycle,
host resize, failure injection, image authenticity and full parity remain HOLD.
