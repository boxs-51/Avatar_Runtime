# Native Studio Local Agent API (experimental)

This API is public to trusted local agents inside the enrolled Avatar_Runtime
Windows workspace. It does not extend SDK ABI20 or register new Computer Bridge
MCP tools. The interface is JSON -> validated ASCII request -> native receipt.

## Invoke

Start launch_layer_physics_studio.bat, then use:

    py scripts/studio_agent_cli.py '{"op":"studio.status"}'
    py scripts/studio_agent_cli.py '{"op":"rig.select","drawable":0}'
    py scripts/studio_agent_cli.py '{"op":"layer.topology.scan"}'

Computer Bridge can invoke the CLI with terminal_run:
workspace_alias="beta", program="python", cwd=".",
arguments=["scripts/studio_agent_cli.py", "<JSON command>"].
The enrolled default alpha workspace is not changed.

## Public command groups

- Scene status: studio.status; studio.metrics.
- Playback: studio.play (enabled bool); studio.step;
  studio.expression (index 0..8).
- Scene toggles: studio.toggle (feature, enabled bool). Features:
  physics, blink, breath, gaze, speech, cycle, grid, particles.
- Motion controls: studio.setting (name, value). Names:
  motion_speed, motion_amount, wind_drive, blink_interval, cycle_interval.
- Camera and UI: studio.camera (mode fit/face), studio.zoom (value .3..3),
  studio.pan (x,y), studio.tab (index 0..3),
  studio.background (index 0..3), studio.reset.
- Local output: studio.preset (action save/load),
  studio.capture (kind viewport only).
- Parameters: param.set (index,value), param.clear (index),
  param.inspect (index), param.reset.
- Parts: part.opacity (index,value 0..1), part.inspect (index).
- Rig: rig.select, rig.vertex, rig.inspect, rig.anchor, rig.beam,
  rig.store (action save/load), rig.apply.
- Semantic hints: layer.scan, layer.suggest, layer.adopt,
  layer.label (kind unknown/hair/skirt/strap).
- Topology: layer.topology (selected drawable), layer.topology.scan
  (aggregates of the entire model).
- Physics: xpbd.set, xpbd.read (name,value for set), xpbd.select
  (first,second,joint), xpbd.inspect, xpbd.apply, xpbd.disable;
  physics.set (legacy gravity,wind,damping).
- Input: action.trigger (kind pull/release/shake/settle, strength 0..1).

XPBD field names: gravity, wind, damping, edge_compliance, collision,
mass, contact_compliance, iterations, step, pin, substeps, pose, transfer,
bending, surface, anchors. Integer: iterations, pin, substeps, anchors.

Changing parameters does not automatically rebind XPBD: call xpbd.apply
or rig.apply separately. No semantic unknown is automatically adopted.

## Receipts, security and limitations

- A call succeeds only when Native Studio acknowledges it. Invalid
  commands and failed native validation return ok=false. Missing
  acknowledgment raises TimeoutError; do not infer success.
- Native parameter values must be within actual model min/max bounds.
  Native XPBD and scene controls also validate allowed ranges.
- The file-backed IPC allows one in-flight command only. There is
  no per-agent OS ACL, transaction journal, cancellation protocol or
  multi-agent serialization guarantee. Computer Bridge executes using
  inherited Windows user permissions, not an OS sandbox.
- No model path or arbitrary output path is accepted in API requests;
  no command execution, arbitrary file reads or network endpoints.
  The executable must be launched through the trusted local launcher.
- Viewport capture saves a PNG only in the ignored local Studio evidence
  folder; the API returns a success receipt, not image content.
- Preset and rig save/load validate local outcomes before success.
- Layer/topology inspection exports aggregate numeric counts only, not
  raw model identifiers, coordinates, textures or rendered images.
  A topological island is not a semantic hair/skirt identification.
- For H3 GUI interaction, use Computer Bridge's separately gated
  window-specific actions. Do not bypass desktop input denial through
  terminal access.

## Regression gates

    py -m unittest discover -s tests/moc3 -p test_studio_agent_api.py
    py scripts/test_studio_agent_api_live.py
    py scripts/test_studio_layer_scan_live.py
    py scripts/test_studio_interaction_metrics.py

Native tests: studio_rig_tests, studio_layer_analysis_tests,
studio_xpbd_parameter_tests. Test logs remain ignored under
build/studio-user.

Known missing platform features: remote/HTTP serving, a native MCP
Studio tool registered with Computer Bridge, atomic multi-command
transactions, a graphical paint-weight editor, skeletal parenting
and automatic semantic identification for all model components.
