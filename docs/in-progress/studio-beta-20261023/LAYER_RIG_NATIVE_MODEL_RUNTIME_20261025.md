# Layer-Rig and Native Runtime Model Compilation Checkpoint

Date: 2026-10-25, Avatar_Runtime beta local workspace only. No push, merge,
or publication of private model assets.

## Functional implementation

Editable geometry document now supports per-vertex Rig influence weights
and optional hierarchical parent with stiffness. Validation blocks
invalid indices, cycles, self parent, nonfinite parameters and deletion
of referenced parent vertices. Same source revision and undo/redo applies.
Source sidecar upgraded to STUDIO_GEOMETRY_3 with V1/V2 load support.

Native custom layer XPBD solver applies these weights and parent links
in addition to pinned anchors. Native UI and Local Agent API provide
weight, parent, compile and compiled model reopen tools. Original
compiled MOC3 RigBank remains a separate legacy runtime subsystem.

Studio can compile edited source to a bounded integrity-checked native
binary named model_runtime.armesh. ARSMESH1 stores layer topology,
material, UV, weights, parent links, source identity and revision.
Compilation validates its temporary output and replaces only a fixed
local path, with previous copy backup. Loading rejects corrupt,
truncated, mismatched-model and trailing data without altering current
document. Cross-process reopen is covered by a real Native Studio test.

## Proprietary Cubism format boundary

The ARS binary is an actual runtime model asset readable by Native
Avatar Studio. It is NOT a Cubism .moc3. Live2D's editable model source
is .cmo3, and .moc3 is exported by Cubism Editor for application use.
The current repo has SDK runtime reader/renderer but no validated
.moc3 writer or accessible Cubism Editor authoring API.
See official documentation:
https://docs.live2d.com/en/cubism-editor-manual/file-type-and-extension/
https://docs.live2d.com/en/cubism-editor-manual/export-moc3-motion3-files/

Full editing/recompilation of original proprietary MOC3 requires an
approved source-authoring workflow and toolchain; never patch its
binary blindly, falsely rename ARS binaries as MOC3 or mutate private
original assets.

## Verification and open gates

Native Release binary build and C source/runtime tests PASS.
Native E2E PASS: Rig weights/parent readback, ARS compile and reopen,
altered binary rejection, recompile, local XPBD GPU accepted physical
frames, saved source/compiled asset reopen in another Native Studio
process, source MOC3 unchanged.
Broader regression recorded in the same work session.

Remaining: Cubism .cmo3 source workflow, true .moc3 export support,
cross-rig skinning between original MOC3 and authored layers, advanced
multi-layer XPBD/collision, visual image QA, GUI input rights and
independent code review. G5 writer remains HOLD.
