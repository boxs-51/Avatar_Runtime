# M2-C private GLB implementation evidence — 2026-10-17

**Tested source commit:** `936b8ca4a8264147bac64e5cacac895fd2b5f4a8` on `m2/3d-render-implementation`, stacked on design PR #33 at `058e8e7159c5f262a452e69f8764e032e4776415`. The later evidence-only commit adds this report and raw logs; it does not alter tested C++/fixture/workflow blobs. The M1-D, M2-A, M2-B and design PRs remain unmerged. No public runtime export or advertised GPU capability changed.

## Direct local execution

Windows 10 x64; Visual Studio 18 2026, MSVC 19.51.36252.0; Windows SDK 10.0.26100.0; CMake `Release`; D3D feature level `0xb000`; D3D11 debug layer available. Configuration enabled `ARS_BUILD_D3D11_SPIKE`, `ARS_BUILD_ASSET_VALIDATOR`, `ARS_BUILD_M2_3D_REFERENCE` and `ARS_BUILD_M2_D3D11_3D`, with the verified local asset dependency cache. All commands below returned exit code 0. Raw command outputs are [build](M2_C_BUILD_RAW.txt), [CTest](M2_C_CTEST_RAW.txt), [WARP](M2_C_WARP_RAW.txt) and [hardware](M2_C_HARDWARE_RAW.txt).

| Probe | Direct observation |
| --- | --- |
| C2 reader + C0 CPU + WARP | `c2_glb_reader`, `m2_private_3d_scene`, `m2_warp_3d_pixels`: 3/3 passed. |
| Adapter identity | WARP: Microsoft Basic Render Driver, vendor `1414`, device `008c`; hardware: NVIDIA GeForce GTX 1060 3GB, vendor `10de`, device `1c02`. Both ran the same binary with debug layer enabled. No D3D error/corruption messages were accepted. |
| Geometry/depth | Two-joint, nonzero morph fixture rendered green pixel `(0,75,0,255)` and background `(0,0,0,0)` on both adapters. Direct D32 readback was `0.5` at the triangle and `1.0` outside. CPU golden rejects skin-before-morph under nonuniform scale and checks morph tangent, node weight precedence and joint palette order. |
| Base/MR/sampler | Base PNG sRGB plus linear MR channel pixel `(129,3,3,255)` on both. Repeat and clamp samples were `(129,10,10,255)` and `(10,129,10,255)`; point and linear magnification samples were `(10,129,10,255)` and `(99,99,10,255)`. Numeric oracles allow at most 3 byte values of adapter variance. |
| Mask/blend | MASK passes alpha exactly at cutoff `0.5` and discards at `0.5001`. BLEND over opaque green was `(64,38,1,255)` on WARP and `(64,39,1,255)` on GTX 1060. Depth remained `0.5` under the nearer transparent draw, proving depth-read/no-write behavior. |
| Camera/host contract | Selected glTF orthographic camera produced `(0,75,0,255)` and camera shift cleared that location. Perspective and infinite-far projection passed CPU selection; 16×16 resize produced the expected pixel. Wrong device, generation and R thread were rejected; host viewport and PS texture slot 1 were restored. |
| Resource rollback | C0 pose/GPU estimate limits returned `ARS_E_OUT_OF_BUDGET` without replacing the snapshot. D3D preflight returned `out_of_budget` and preserved the previous target pixel. The exact event query retired after submission. M2-B 2D WARP regression passed after state-scope extension. |

Generated fixture SHA-256 values: `positive-m2-3d-pose.glb` `cbbcef0f53b60493825c97dd38a82c33b5be5639b473320519f6bf7249a30b89`; `positive-m2-3d-flat.glb` `6966cf067a7022c508658e615a6a7d48fb90dab80a5175794586e1a894c4652e`; `positive-m2-3d-texture.glb` `ba015bf7d5f1900d500cddcc80864c854cdbd2dfda6d9a47dd137283a7d3fb91`; `positive-m2-3d-camera-orthographic.glb` `3c2c8bc4301fbc70b1ce3b6fb7db2e1495833ad4b3e43d470ab4d411f79c8813`. The complete 191-case deterministic fixture manifest is committed with the source.

## Gate result and remaining work

**G04: HOLD (private C0-C2 slice evidenced, integrated acceptance incomplete).** The host has not wired committed 3D pose/camera into its transaction. The CPU snapshot still uses bounded standard allocations rather than the host allocator, and an independently retained CPU snapshot pin has not been verified across query retirement. Full minification/mipmap sampler pixel coverage, injected GPU allocation failure, and before/after live-object baselines remain open. CI on the stacked PR is pending; a green run would add platform replication, not close these missing criteria. G00 technical-method, G01, G02 and G05 remain on their own HOLD evidence requirements.

**D06/D08: `HOLD_CARRY_TO_M2`.** WARP fault injection can prove control flow but cannot establish physical device reset, driver removal reason or post-loss live-object cleanup. The current workstation has no independent reset-capable GPU runner. No TDR or driver reset was attempted.
