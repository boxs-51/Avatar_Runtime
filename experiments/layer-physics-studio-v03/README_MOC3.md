# Layer Physics Studio v0.3 — MOC3 native evaluator + experimental XPBD

Status: **EXPERIMENTAL / DEFAULT OFF**. This is a private MOC3-specific path for users whose model is a Live2D `*.model3.json`, not `.arschar`. The renderer uses the existing independent PurismCore-based MOC3 evaluator and D3D11 WARP offscreen renderer; the editor opens a Windows Tk native desktop window backed by genuine MOC3 rendered pixels. **This is not a DXGI swapchain inside the Tk window.** The separate `ars_lps_native_viewer.exe` remains ARSCHAR-only.

## Windows 10 / PowerShell build

Requires VS2022 C++ workload (MSVC), Windows SDK, CMake, Git, Python 3.12 with tkinter. Clone the independent evaluator separately; the repository does not redistribute MOC3 model files, images or PurismCore binaries. The exact evaluator source pin is required by the existing CMake contract.

```powershell
cd E:\Avatar_Runtime
git fetch origin
git switch exp/layer-physics-studio-v0.3-full-model
git pull --ff-only origin exp/layer-physics-studio-v0.3-full-model

git clone https://github.com/SakuraMotion/PurismCore.git build/lps-purism
git -C build/lps-purism checkout --detach 1069334965522df5d0b791e97f01e26b19c45456
$p = (Resolve-Path build/lps-purism).Path

cmake -S . -B build/m2-d-private -A x64 `
  -DARS_BUILD_D3D11_SPIKE=ON `
  -DARS_BUILD_ASSET_VALIDATOR=ON `
  -DARS_ASSET_FETCH_DEPS=ON `
  -DARS_BUILD_M2_2D_REFERENCE=ON `
  -DARS_BUILD_M2_D3D11_2D=ON `
  -DARS_BUILD_M2_3D_REFERENCE=ON `
  -DARS_BUILD_M2_D3D11_3D=ON `
  -DARS_BUILD_M2_D3D11_DUAL=ON `
  -DARS_BUILD_MOC3_INDEPENDENT=ON `
  -DARS_BUILD_MOC3_D3D11=ON `
  -DARS_BUILD_LPS_MOC3_BRIDGE=ON `
  "-DARS_PURISM_CORE_ROOT=$p"
if ($LASTEXITCODE -ne 0) { throw "MOC3 CMake configure failed" }

cmake --build build/m2-d-private --config Release --target ars_lps_moc3_bridge --parallel 2
if ($LASTEXITCODE -ne 0) { throw "MOC3 XPBD bridge build failed" }

py experiments/layer-physics-studio-v03/moc3_viewer.py "E:\your-model\my-character.model3.json"
```

Choose the `*.model3.json` file from your model folder. It must reference its own `*.moc3` and PNG atlases. A loose `.moc3` without manifest or textures is not a usable full model. In the editor, select one drawable (or Ctrl-click two), then **Apply selected XPBD meshes**. Use gravity/wind/stiffness/collision radius; enable two-way joint with two selected drawables and apply changes. Disable XPBD to inspect original MOC3 output. If the manifest references Physics3, the editor can enable or disable the existing Physics3 state separately. It does not claim Physics3 is the XPBD solver.

## Limits and evidence

- Existing `admit_package` handles bounded `model3.json` path containment, MOC bytes and RGB/RGBA PNG decoding. Advanced PNG variants, all Live2D/SDK extensions, unsupported MOC versions, motion expressions and model3 extras can be rejected; DO NOT report unsupported assets as an accepted full model.
- Overlay can deform a selected drawable or two with at most **2048 vertices total**, only if selected mesh topology is supported. Nonselected drawables remain exactly as evaluator produced. UV/indices/masks/opacity/blend/color/order are unmodified in the GPU snapshot.
- Optional two-way XPBD distance joint joins the last particle of each of two selected drawables. Pinned anchors cannot receive impulse. Sphere-style particle contact is not real mesh CCD/edge collision.
- Solver `prepare` creates a candidate with no mutation; `accept` occurs only after the D3D11 exact host query commits. Native evaluator `update_complete` still owns parameter updates; this is *not* a claim that all model+GPU+solver operations are one public transaction.
- The native bridge provides evaluated real MOC3 pixel output to an interactive Windows Tk window. The Tk UI is not a native DXGI swapchain or the full alpha public SDK Viewer.
- CI exercises generated snapshots and WARP renderer without redistributing any user MOC3. Testing a user's model on their own Windows system is **not evidenced by CI**. Public release, performance/resize/device-loss and native MOC3 editor renderer parity remain HOLD.
- For privacy: all model bytes and frame files stay local under `build/`; do not upload personal assets/logs to public GitHub.

## Troubleshooting

If build says `ARS_PURISM_CORE_ROOT` missing, supply the folder containing the pinned evaluator CMakeLists.txt. If admission rejects a PNG/manifest/MOC format, preserve the rejection and test a minimal locally stored copy rather than weakening bounds. If GUI opens but XPBD selection returns `LPS_ERROR selection_budget_or_mesh`, the selected meshes are over budget or have unsupported topology; all original MOC3 rendering remains available.
