# Studio Rig Agent → XPBD physical frame checkpoint (2026-10-25)

Scope: local Native Studio executable, separate private test session, current beta branch. No remote model or public assets. Product G5 writer remains HOLD.

## Physical test

The local Agent performs four real Rig operations on the selected drawable:
anchor, support beam, parent and weight. The owner-thread Studio accepts each
revision and the Agent reads the updated weight and bounded Rig history.

The Agent calls `rig.apply`, waits for a GPU-accepted legacy Rig frame, then
`rig.undo`, verifies the old frame is NOT current and the weight reverted,
re-applies XPBD and obtains another accepted frame. `rig.redo` does the same.
Four distinct physical acceptance samples are confirmed, with advancing frame
serials and model-generation/revision parity.

The same test performs a generic `xpbd.apply` and then `xpbd.disable`.
Both operations MUST invalidate the prior Rig frame witness. A synthetic G5
document is not considered APPLIED by any of these operations.

## RED-first finding and fix

RED: After a real GPU-accepted `rig.apply`, local `xpbd.disable` returned
success while `studio.g5.legacy_frame.status` still reported
`legacy_frame_current=True`. The owner had disabled the solver but had not
unbound the prior Rig frame witness. This could mislead an Agent.

Fix: on successful generic XPBD replacement and on XPBD disable, invalidate
the legacy Rig frame fence in the Native owner-thread API handler. Do not
reset on a failed XPBD replacement. Existing UI XPBD paths already unbind.

GREEN: The updated physical Studio test PASS with all four accepted-frame
samples; `xpbd.apply` and `xpbd.disable` both show a noncurrent Rig frame.
A static source regression test prevents removing these invalidations.

## Verification

- Release build of real `avatar_studio.exe`: PASS (existing C compiler
  warnings were not addressed).
- `python -B -m scripts.test_studio_rig_physics_workflow_live`: PASS.
- `python -B -m scripts.test_studio_g5_legacy_frame_live`: PASS before fix.
- Python moc3 and model-publication guard: 299 PASS, 19 SKIP, 208 subtests PASS.
- Native G5/Rig selected CTest: 15 PASS, heavy multi-process fixtures excluded.

The private local fixture is deleted by the test; no captured model data,
mesh geometry, local path, identity or rendered texture is placed in reports.

## Follow-up product direction

Rig is not declared complete. Before starting direct layer/model write tooling,
test physical UI interactions as well as Agent calls, full vertex-weight brush,
cross-drawable constraints, RigBank switching while XPBD is bound, and local
backup/recovery. Keep all design phases and roadmaps here in documentation,
not in production source. Avoid additional standalone G5 infrastructure.
