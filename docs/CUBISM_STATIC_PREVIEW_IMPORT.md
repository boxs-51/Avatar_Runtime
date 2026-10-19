# Cubism sample: local static-preview import

The `ulvm2_0002` files are available to the owner for free use but **may not be redistributed in this public repository**. Keep them under ignored `local-assets/ulvm2_0002/`, or use another local path. Do not commit the `.moc3`, texture atlases, portrait, expressions, VTube Studio metadata, or generated `.arschar` derived from them. The local copy intentionally excludes `key.txt`, which is unrelated to the renderer.

The adapter at `scripts/cubism_static_preview.py` checks a Cubism 3 `model3.json`, its MOC3 magic, declared texture paths and optional linked files, then packages a **separately rendered portrait PNG** into one static ARS2D quad. For this sample, `icon_ulvm2_0002.png` is a 256×256 portrait. This path exercises ARSCHAR import and static 2D scene preparation, but it does not decode `.moc3`, recreate the full character from texture atlases, map parameters, play expressions, simulate physics, or support dynamic Live2D rendering. A texture atlas is a collection of parts and is not a rendered portrait.

From the repo root, run:

```powershell
python scripts/cubism_static_preview.py local-assets/ulvm2_0002/ulvm2_0002.model3.json local-assets/ulvm2_0002/icon_ulvm2_0002.png build/ulvm2_0002-preview.arschar
python -m unittest discover -s tests/assets -p test_cubism_static_preview.py -v
cmake --build build/cubism-preview --config Release --target ars_cubism_preview_validation
build/cubism-preview/bin/Release/ars_cubism_preview_validation.exe build/cubism-preview/bin/Release/ars_schema_worker.exe build/ulvm2_0002-preview.arschar
build/cubism-preview/bin/Release/ars_m2_2d_pixels.exe --sample build/cubism-preview/bin/Release/ars_schema_worker.exe build/ulvm2_0002-preview.arschar
```

The native validator command checks the generated archive with the schema/PNG/profile-B pipeline and prepares one static 2D draw when `ARS_BUILD_M2_2D_REFERENCE=ON`. The last command reads three actual pixels from a WARP offscreen render and compares them with the decoded portrait; it requires `ARS_BUILD_M2_D3D11_2D=ON`. The converter tests use synthetic, redistributable inputs so CI can run without the owner's model. Successful local validation is evidence for this static portrait path only. Full Cubism support remains a separate adapter/runtime project requiring the official Cubism Core and its license, a drawable/parameter/physics mapping, and direct pixel and animation tests.

The proposed private-to-public sequence and HOLD exit criteria for dynamic Cubism are in [the dynamic adapter plan](M2_LIVE2D_DYNAMIC_ADAPTER_PLAN.md).
