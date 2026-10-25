# Shared Native Studio UI and Layer UV/Material/XPBD

Date: 2026-10-25. Beta workspace, local-only; no push/merge/public model assets.

## UI direction

The owner-approved viewport-first layout remains authoritative. The new
schematic docs/ui/AVATAR_STUDIO_SHARED_LAYOUT.svg is a source-editable
layout reference, not a screenshot of the real application.
Common Studio areas: global menu and Save/Undo/Preview, project/layer tree
at left, large central editable viewport, contextual Geometry / UV-Material
/ Rig-XPBD inspector at right, history/validation/timeline dock below.
Current Native implementation: left/right panels independently collapse,
contextual inspector tabs and enlarged viewport. Bottom dock remains DESIGN.
Previously generated concept PNGs exist as chat attachments, but binary
copies were not verified in the Windows repo; the SVG is additional design
documentation, not a replacement for the approved detailed art.

## Product changes

Editable sidecar V2 saves per-vertex UV, local layer RGBA material and
pinned Rig vertex; previous V1 projects load with safe default UV/material.
Direct edits use the sidecar source, preserve optimistic revision checks and
undo/redo. Compiled MOC3 buffers remain immutable.
Importing eligible compiled drawables copies their UVs into editable source.
The inspector edits UV/tint/opacity/pin. Viewport uses a generated UV checker
texture; native texture export or full material pipeline is NOT claimed.

Custom authored mesh binds into native_mesh_xpbd, the same native solver
core used for existing model cloth, through a separate Studio-owned scene.
A pinned vertex anchors the mesh. After candidate render, Studio flushes GL,
checks errors and accepts the candidate; status is scoped to local geometry
revision. Source edits invalidate the prior frame and require a new bind.
This is not full RigBank hierarchy/skin deformation and does not enable G5.

## Evidence and release gates

Native Release build and synthetic document migration tests PASS.
Real Studio local E2E PASS: UV/material edit and readback, save, anchor,
native solver bind, accepted GPU frames, vertex displacement, no anchor drift,
stale-frame invalidation and stop. Import/restart/Rig physical tests PASS.
Python and CTest regression totals are recorded after final run.

GUI input acceptance on the newest Host is BLOCKED: Win32 SendInput denied,
legacy synthetic click no longer activates Studio reliably. The GUI script
checks target PID, HWND and foreground before input and aborts rather than
touching other windows. No GUI PASS is claimed for this newest build.

Incomplete product scope: bottom timeline/history dock; direct source edits
to opaque compiled MOC3; native texture/material source export; multi-anchor
RigBank binding; multi-layer collisions; frame-image parity review;
independent security/code review and release gates.
