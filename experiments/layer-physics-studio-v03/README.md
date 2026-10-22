# Layer Physics Studio v0.3 — experimental branch

Status: **experimental branch / non-production**. Base: `alpha@006f36689d1c432659925ea2f9abe865a7825831`.

## Purpose

Evaluate standalone cross-layer 2D XPBD hair/cloth physics, particle-circle collision, and bidirectional distance joints without modifying the native Avatar_Runtime SDK or any existing security gate.

The runnable offline app, standalone conversion scripts and RED-first test evidence were produced as the separately downloadable `Layer_Physics_Studio_v0.3.zip` conversation artifact. **This branch presently contains the experiment contract only; app sources have not yet been committed.** Do not count this branch as a runnable checked-in app until those exact sources are added and verified.

## Implemented in the separately produced app

- Versioned authoring v0.3.0; v0.1/v0.2 import migration
- Per-layer particle-circle contacts gated by collision flags and radius
- Interlayer free/free distance joints with inverse-mass weighted two-sided XPBD corrections; pinned endpoints are kinematic
- Fixed-step 120Hz preview, immutable accepted state, bounded collision work, scene export, offline HTML app and joint UI
- Experimental native2D `.arschar` -> all declared parts as static preview sprites, then per-part editing via hair/cloth mode. This **is not** full model native rendering fidelity.

Local non-production evidence for downloadable app: 42/42 JavaScript tests, 3/3 model adapter tests, and Chromium UI smoke PASS.

## Explicit HOLDs

1. Wire v0.3 solver candidate into actual alpha validated CPU pose/render ownership and D3D11; match exact accepted frame identity.
2. Real-model Windows physical testing and pixel comparison; private MOC3/GLB rendering remains alpha-owned, not supported in standalone preview.
3. Add source files to this branch, CI, hostile ZIP/PNG/JSON/GLB guards, crash/rollback, lifecycle, load/fuzz and independent security review.
4. Do not merge to `alpha` or `main`, advertise new public capabilities, or weaken existing runtime test/security oracles.

## Local test commands (once app files are checked out)

```sh
node --test tests/test_xpbd.cjs tests/test_interlayer_red.cjs
python -m unittest discover -s tests -p 'test_model_adapter.py' -v
python tests/ui_smoke.py
```

Developer design decision: keep the editor separate and create an explicit bridge only after RED-first contracts for full native-model validation/pose transfer.
