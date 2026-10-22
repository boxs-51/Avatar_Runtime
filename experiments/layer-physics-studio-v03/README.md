# Layer Physics Studio v0.3 — experimental, offline

**Status:** Non-production proof of concept. Adds two-sided XPBD distance joints and cross-layer particle-circle contacts to the v0.2 hair/cloth graph. NOT part of the native Avatar_Runtime render pipeline, not a replacement for the original MOC3/native2D/GLB alpha viewer, and not an authoritative .arschar validator.

## Windows 10 app quick start

1. Unzip the release, double-click `index.html` in Edge/Chrome to test the included 6-layer demo scene. No npm/CDN/network needed.
2. For a native2D `.arschar` **whole-part preview**, install Python 3.12+ and Pillow (`py -m pip install pillow`). In this folder run:
   ```powershell
   py run_studio.py --model "E:\models\example.arschar" --out "E:\model-preview"
   ```
   The generated `E:\model-preview\index.html` is a self-contained local app. It preloads each declared native2D rig part as a visual layer. Choose a part, change its type to `hair`/`cloth`, then set physical parameters and connect cross-layer joints. Or run `py model_to_studio.py path.arschar --out imported.json` and import the JSON into the main app.
3. During simulation: click **＋ Nối 2 particle**, select two particles on two different dynamic layers to create a **two-sided XPBD joint**. `Xóa` removes the joint. Check/uncheck **Collision** globally or for each layer; adjust collider radius in the inspector.
4. Use pause / one tick / reset, drag particle, switch hair/cloth, set anchors and textures. Export JSON for portable authoring; imports of v0.1/v0.2 documents are migrated to v0.3.

## Honest native model coverage

- `.arschar` **native 2D**: development-only import of declared parts, atlas rectangles, z ordering into separate preview sprites, capped at 63 parts, 2048×2048 atlas, ZIP32 MiB. Static parts are shown in a single canvas and may be converted manually to hair/cloth for XPBD. This is NOT a faithful native mesh renderer: MOC evaluators, clipping, material/rig semantics, multi-target deforms and precise inherited transforms are absent. The original geometry/UV may not fit the approximated sprite transform. All user assets remain on local disk.
- `.arschar` GLB-only / `.model3.json` MOC3: NOT imported into the XPBD canvas. Use the existing alpha/native Viewer via repository's `launch_interactive_viewer.bat` for original model runtime. The v0.3 app **does not** inject XPBD output into the alpha renderer; parity remains HOLD.
- Production/public admission, untrusted asset hardening, collision shape geometry, cloth self-collision, capsule/edge contact, restitution/CCD, and bidirectional *hierarchy anchor* reactions remain future work. Explicit interlayer XPBD distance joints between free particles produce real two-way impulses. Kinematic pinned endpoints necessarily do not receive feedback.

## Verification

```powershell
node --test tests/test_xpbd.cjs tests/test_interlayer_red.cjs
python -m unittest discover -s tests -p 'test_model_adapter.py' -v
python tests/ui_smoke.py  # needs playwright + Chromium
```

Current local checks: 42/42 JS tests, 3/3 bundle adapter tests, Chromium UI smoke PASS (see tests/ logs). RED baseline of v0.2 recorded separately before adding v0.3 implementation. Windows native actual-model and D3D11 parity have NOT run.

No ABI20, native runtime sources, production activation, alpha/main merges, public MOC3 assets or security oracles are changed.
