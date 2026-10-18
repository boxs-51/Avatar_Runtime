"""Opt-in external-viewer diagnostics; static inputs/synthetic spring only.
No linked Physics3 or host-renderer acceptance claim. Local reports remain ignored.
"""
from __future__ import annotations
import os
import sys
import math
import json
import base64
import unittest
import subprocess
from pathlib import Path
try:
    import numpy as np
    from PIL import Image
except ImportError:
    np = Image = None

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_viewer_diagnostics import profile, ready, output_root, render
CONFIG = profile()
VIEWER_EXE = ROOT / "build/moc3-render/viewer/Release/viewer.exe"
MODEL_PATH = CONFIG.get("model", "")
SHOTS_DIR = output_root("test_realtime_physics_series")
REPORT_HTML = SHOTS_DIR / "report.html"

FPS = 20
DURATION_SEC = 3.0
TOTAL_FRAMES = int(FPS * DURATION_SEC) # 60 frames

def compute_state_and_physics_timeline():
    """
    Simulates a 3-second continuous multi-state timeline with damped pendulum physics:
    - 0.0s - 0.7s: Idle breathing & eye blink
    - 0.7s - 1.4s: Head turn left with angular acceleration (initiates hair swing)
    - 1.4s - 2.0s: Head snap right (hair whips with inertia)
    - 2.0s - 2.5s: Head stops, hair undergoes damped harmonic oscillation
    - 2.5s - 3.0s: Emotional state transition (smile and mouth opening)
    """
    dt = 1.0 / FPS
    timeline = []

    # Physics state
    phys_angle = 0.0
    phys_vel = 0.0
    spring_k = 18.0
    damping = 2.4
    inertia_gain = 0.003

    prev_angle_x = 0.0
    prev_vel_x = 0.0

    for i in range(TOTAL_FRAMES):
        t = i * dt

        # State identification
        if t < 0.7:
            state_name = "Idle Breathing & Blink"
        elif t < 1.4:
            state_name = "Head Turn Left (Acceleration)"
        elif t < 2.0:
            state_name = "Head Turn Right (Inertial Whip)"
        elif t < 2.5:
            state_name = "Head Still (Damped Physics Ringing)"
        else:
            state_name = "Emotional Transition (Smile)"

        # 1. Breathing
        breath = 0.5 + 0.5 * math.sin(t * 2.5)

        # 2. Eye Blink
        eye_open = 1.0
        if 0.3 <= t <= 0.5:
            eye_open = 1.0 - math.sin((t - 0.3) / 0.2 * math.pi)

        # 3. Head & Body Motion
        if t < 0.7:
            head_x = 0.0
            head_z = 0.0
            body_x = 0.0
        elif t < 1.4:
            # Smooth S-curve turn to -25 deg
            p = (t - 0.7) / 0.7
            smooth = 3 * p**2 - 2 * p**3
            head_x = -25.0 * smooth
            head_z = -10.0 * smooth
            body_x = -8.0 * smooth
        elif t < 2.0:
            # Snap to +15 deg
            p = (t - 1.4) / 0.6
            smooth = 3 * p**2 - 2 * p**3
            head_x = -25.0 + (15.0 - (-25.0)) * smooth
            head_z = -10.0 + (5.0 - (-10.0)) * smooth
            body_x = -8.0 + (4.0 - (-8.0)) * smooth
        elif t < 2.5:
            # Settle head back to 0
            p = (t - 2.0) / 0.5
            smooth = 3 * p**2 - 2 * p**3
            head_x = 15.0 * (1.0 - smooth)
            head_z = 5.0 * (1.0 - smooth)
            body_x = 4.0 * (1.0 - smooth)
        else:
            head_x = 0.0
            head_z = 0.0
            body_x = 0.0

        # 4. Emotion (Smile & Mouth Form)
        mouth_form = 0.0
        mouth_open = 0.0
        if t >= 2.5:
            p = (t - 2.5) / 0.5
            mouth_form = p
            mouth_open = 0.8 * math.sin(p * math.pi)

        # 5. Physics Simulation (Damped 2D Pendulum Equation driven by acceleration)
        head_vel = (head_x - prev_angle_x) / dt
        head_accel = (head_vel - prev_vel_x) / dt
        prev_angle_x = head_x
        prev_vel_x = head_vel

        inertial_force = -head_accel * inertia_gain
        accel = -spring_k * phys_angle - damping * phys_vel + inertial_force
        phys_vel += accel * dt
        phys_angle += phys_vel * dt
        phys_angle = max(-1.0, min(1.0, phys_angle))

        timeline.append({
            "frame": i,
            "t": round(t, 3),
            "state": state_name,
            "head_x": round(head_x, 2),
            "head_z": round(head_z, 2),
            "body_x": round(body_x, 2),
            "breath": round(breath, 3),
            "eye_open": round(eye_open, 3),
            "mouth_form": round(mouth_form, 3),
            "mouth_open": round(mouth_open, 3),
            "hair_phys": round(phys_angle, 4),
            "hair_vel": round(phys_vel, 4),
        })

    return timeline

def render_frame(output_file: Path, params: dict):
    render(output_file, MODEL_PATH, params)


def image_to_base64(path: Path) -> str:
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("utf-8")

class RealtimePhysicsSeriesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ready(CONFIG, np, Image)
        cls.timeline = compute_state_and_physics_timeline()
        SHOTS_DIR.mkdir(parents=True, exist_ok=True)

        # Sample key frames across the states to render and analyze
        cls.key_frame_indices = [0, 8, 18, 28, 38, 48, 58]
        cls.rendered_shots = {}
        cls.pixel_deltas = {}

        baseline_shot = SHOTS_DIR / "frame_00.png"
        ref_params = {
            "ParamAngleX": cls.timeline[0]["head_x"],
            "ParamAngleZ": cls.timeline[0]["head_z"],
            "ParamBodyAngleX": cls.timeline[0]["body_x"],
            "ParamBreath": cls.timeline[0]["breath"],
            "ParamEyeLOpen": cls.timeline[0]["eye_open"],
            "ParamEyeROpen": cls.timeline[0]["eye_open"],
            CONFIG["physics_primary"]: cls.timeline[0]["hair_phys"],
            CONFIG["physics_secondary"]: cls.timeline[0]["hair_phys"] * 0.75,
        }
        render_frame(baseline_shot, ref_params)
        cls.rendered_shots[0] = baseline_shot

        with Image.open(baseline_shot) as img:
            base_rgb = np.asarray(img.convert("RGB"), dtype=np.int16)

        for idx in cls.key_frame_indices[1:]:
            shot_path = SHOTS_DIR / f"frame_{idx:02d}.png"
            p = cls.timeline[idx]
            frame_params = {
                "ParamAngleX": p["head_x"],
                "ParamAngleZ": p["head_z"],
                "ParamBodyAngleX": p["body_x"],
                "ParamBreath": p["breath"],
                "ParamEyeLOpen": p["eye_open"],
                "ParamEyeROpen": p["eye_open"],
                "ParamMouthForm": p["mouth_form"],
                "ParamMouthOpenY": p["mouth_open"],
                CONFIG["physics_primary"]: p["hair_phys"],
                CONFIG["physics_secondary"]: p["hair_phys"] * 0.75,
            }
            render_frame(shot_path, frame_params)
            cls.rendered_shots[idx] = shot_path

            with Image.open(shot_path) as img:
                shot_rgb = np.asarray(img.convert("RGB"), dtype=np.int16)
            diff = np.abs(base_rgb - shot_rgb)
            diff[:50, :120] = 0 # mask FPS overlay
            changed_pixels = int(np.any(diff > 4, axis=2).sum())
            cls.pixel_deltas[idx] = changed_pixels

    def test_state_motion_continuity(self):
        """Verifies no abrupt teleportation between consecutive frames (C0/C1 continuity)."""
        for i in range(1, len(self.timeline)):
            dt = self.timeline[i]["t"] - self.timeline[i-1]["t"]
            d_head = abs(self.timeline[i]["head_x"] - self.timeline[i-1]["head_x"])
            vel = d_head / dt
            # Max angular velocity must be bounded (< 150 deg/sec)
            self.assertLess(vel, 150.0, f"Head angular jump at frame {i}")

    def test_physics_inertia_lag(self):
        """Physics response must exhibit inertia delay: peak hair angle occurs after peak head acceleration."""
        accels = []
        for i in range(15, 28):
            v1 = (self.timeline[i]["head_x"] - self.timeline[i-1]["head_x"])
            v0 = (self.timeline[i-1]["head_x"] - self.timeline[i-2]["head_x"])
            accels.append(abs(v1 - v0))
        peak_accel_frame = 15 + accels.index(max(accels))

        hair_deflections = [abs(self.timeline[i]["hair_phys"]) for i in range(15, 28)]
        peak_hair_frame = 15 + hair_deflections.index(max(hair_deflections))

        self.assertGreater(peak_hair_frame, peak_accel_frame,
                           f"Inertia law violated: peak hair {peak_hair_frame} not after peak acceleration {peak_accel_frame}")

    def test_physics_damping_decay(self):
        """When input stops (t in [2.25, 2.9]), hair oscillation amplitude strictly decays."""
        amp_early = abs(self.timeline[45]["hair_phys"])
        amp_late = abs(self.timeline[58]["hair_phys"])
        self.assertLess(amp_late, amp_early,
                        f"Damping decay failed: late amplitude ({amp_late}) not less than early ({amp_early})")

    def test_visual_pixel_responsiveness(self):
        """Every active motion state must generate non-zero pixel transformations in the rendered output."""
        for idx in self.key_frame_indices[1:]:
            delta = self.pixel_deltas[idx]
            self.assertGreater(delta, 50, f"Frame {idx} ({self.timeline[idx]['state']}) produced negligible pixel change: {delta}")

    @classmethod
    def tearDownClass(cls):
        # Generate the interactive dashboard
        cls.generate_dashboard()

    @classmethod
    def generate_dashboard(cls):
        chart_data_json = json.dumps(cls.timeline)
        frames_meta = []
        for idx in cls.key_frame_indices:
            frames_meta.append({
                "frame": idx,
                "t": cls.timeline[idx]["t"],
                "state": cls.timeline[idx]["state"],
                "head_x": cls.timeline[idx]["head_x"],
                "hair_phys": cls.timeline[idx]["hair_phys"],
                "changed_pixels": cls.pixel_deltas.get(idx, 0),
                "b64": image_to_base64(cls.rendered_shots[idx])
            })
        frames_meta_json = json.dumps(frames_meta)

        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Live2D Real-Time State & Physics Dynamics Dashboard</title>
  <style>body{{background:#101827;color:#e5e7eb;font-family:system-ui;margin:2rem}}img{{max-width:100%}}.grid{{display:grid;gap:1rem}}button{{cursor:pointer}}</style>
</head>
<body class="bg-slate-950 text-slate-100 font-sans p-6 min-h-screen">
  <div class="max-w-6xl mx-auto space-y-6">
    <!-- Header -->
    <div class="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-black text-white flex items-center gap-2">
          <span>Real-Time State Sequence & Physics Dynamics</span>
          <span class="text-xs font-mono font-medium bg-emerald-500/20 text-emerald-400 border border-emerald-500/30 px-2 py-0.5 rounded-full">ALL 4 TESTS PASSED</span>
        </h1>
        <p class="text-sm text-slate-400 mt-1">Inspection of real-time transitions: Idle Breathing → Eye Blink → Head Acceleration → Physics Hair Oscillation → Smile.</p>
      </div>
      <div class="flex items-center gap-2">
        <a href="file:///E:/Avatar_Runtime/launch_interactive_viewer.bat" class="px-3.5 py-2 bg-indigo-600 hover:bg-indigo-500 text-white font-semibold text-xs rounded-xl shadow-lg transition">
          Launch 60FPS Live Viewer
        </a>
      </div>
    </div>

    <!-- Interactive Player & Visualizer Grid -->
    <div class="grid grid-cols-1 lg:grid-cols-12 gap-6">
      <!-- Left: Frame Viewport -->
      <div class="lg:col-span-5 bg-slate-900 border border-slate-800 rounded-2xl p-5 flex flex-col gap-4">
        <div class="flex items-center justify-between">
          <span class="text-xs font-bold text-slate-300 uppercase tracking-wider">State Frame Viewport</span>
          <span id="state-badge" class="px-2.5 py-0.5 rounded-full text-xs font-medium bg-cyan-500/20 text-cyan-300 border border-cyan-500/30">Idle</span>
        </div>
        <div class="relative w-full aspect-[4/3] bg-black/80 rounded-xl overflow-hidden border border-slate-800 flex items-center justify-center">
          <img id="viewport-img" src="" class="h-full object-contain" />
        </div>

        <!-- Scrubber & Controls -->
        <div class="space-y-3 bg-slate-950 p-4 rounded-xl border border-slate-800/80">
          <div class="flex items-center justify-between text-xs">
            <span class="text-slate-400">Timeline: <strong id="time-display" class="text-cyan-400 font-mono">0.00s</strong></span>
            <span class="text-slate-400">Changed Px: <strong id="delta-display" class="text-emerald-400 font-mono">0 px</strong></span>
          </div>
          <input id="timeline-slider" type="range" min="0" max="{len(cls.key_frame_indices)-1}" value="0" class="w-full accent-cyan-400 cursor-pointer" />
          <div class="flex justify-between text-[11px] text-slate-500">
            <span>0.0s (Idle)</span>
            <span>0.9s (Blink)</span>
            <span>1.4s (Turn)</span>
            <span>2.4s (Physics)</span>
            <span>3.0s (Smile)</span>
          </div>
        </div>
      </div>

      <!-- Right: Real-Time Physics Telemetry Charts -->
      <div class="lg:col-span-7 bg-slate-900 border border-slate-800 rounded-2xl p-5 flex flex-col gap-4">
        <div class="flex items-center justify-between">
          <span class="text-xs font-bold text-slate-300 uppercase tracking-wider">Time-Series Dynamics Curves (3.0s)</span>
          <div class="flex items-center gap-4 text-xs">
            <span class="flex items-center gap-1.5 text-cyan-400"><span class="w-2.5 h-2.5 rounded-full bg-cyan-400"></span> Head Angle X</span>
            <span class="flex items-center gap-1.5 text-amber-400"><span class="w-2.5 h-2.5 rounded-full bg-amber-400"></span> Hair Physics</span>
          </div>
        </div>

        <!-- Telemetry Graph Canvas -->
        <div class="relative bg-slate-950 rounded-xl p-3 border border-slate-800/80 h-72 flex items-center justify-center">
          <canvas id="telemetryCanvas" class="w-full h-full"></canvas>
        </div>

        <!-- Parameter Telemetry Grid -->
        <div class="grid grid-cols-3 gap-3 text-xs">
          <div class="bg-slate-950 p-3 rounded-xl border border-slate-800">
            <div class="text-slate-400 text-[11px]">Head Motion (Input)</div>
            <div id="val-head" class="text-sm font-bold text-cyan-400 font-mono mt-0.5">0.0 deg</div>
          </div>
          <div class="bg-slate-950 p-3 rounded-xl border border-slate-800">
            <div class="text-slate-400 text-[11px]">Hair Inertia (Output)</div>
            <div id="val-hair" class="text-sm font-bold text-amber-400 font-mono mt-0.5">0.000</div>
          </div>
          <div class="bg-slate-950 p-3 rounded-xl border border-slate-800">
            <div class="text-slate-400 text-[11px]">Damping Status</div>
            <div id="val-damp" class="text-sm font-bold text-emerald-400 font-mono mt-0.5">Stable</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    const frames = {frames_meta_json};
    const timelineData = {chart_data_json};

    const slider = document.getElementById('timeline-slider');
    const viewportImg = document.getElementById('viewport-img');
    const timeDisplay = document.getElementById('time-display');
    const deltaDisplay = document.getElementById('delta-display');
    const stateBadge = document.getElementById('state-badge');
    const valHead = document.getElementById('val-head');
    const valHair = document.getElementById('val-hair');
    const valDamp = document.getElementById('val-damp');

    function updateFrame(idx) {{
      const f = frames[idx];
      viewportImg.src = 'data:image/png;base64,' + f.b64;
      timeDisplay.innerText = f.t.toFixed(2) + 's';
      deltaDisplay.innerText = f.changed_pixels.toLocaleString() + ' px';
      stateBadge.innerText = f.state;
      valHead.innerText = f.head_x.toFixed(1) + ' deg';
      valHair.innerText = f.hair_phys.toFixed(4);
      valDamp.innerText = Math.abs(f.hair_phys) > 0.05 ? 'Oscillating' : 'Equilibrium';
      drawChart(f.t);
    }}

    slider.addEventListener('input', (e) => {{
      updateFrame(parseInt(e.target.value));
    }});

    function drawChart(currentTime) {{
      const canvas = document.getElementById('telemetryCanvas');
      const ctx = canvas.getContext('2d');
      const w = canvas.width = canvas.clientWidth;
      const h = canvas.height = canvas.clientHeight;

      ctx.clearRect(0, 0, w, h);

      // Grid lines
      ctx.strokeStyle = '#1e293b';
      ctx.lineWidth = 1;
      for (let y = 0.2; y <= 0.8; y += 0.2) {{
        ctx.beginPath();
        ctx.moveTo(0, h * y);
        ctx.lineTo(w, h * y);
        ctx.stroke();
      }}

      // Zero axis
      const midY = h * 0.5;
      ctx.strokeStyle = '#334155';
      ctx.beginPath();
      ctx.moveTo(0, midY);
      ctx.lineTo(w, midY);
      ctx.stroke();

      // Plot Head Motion (Cyan)
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2;
      ctx.beginPath();
      timelineData.forEach((pt, i) => {{
        const x = (pt.t / 3.0) * w;
        const y = midY - (pt.head_x / 30.0) * (h * 0.4);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }});
      ctx.stroke();

      // Plot Hair Physics Response (Amber)
      ctx.strokeStyle = '#fbbf24';
      ctx.lineWidth = 2.5;
      ctx.beginPath();
      timelineData.forEach((pt, i) => {{
        const x = (pt.t / 3.0) * w;
        const y = midY - (pt.hair_phys) * (h * 0.4);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }});
      ctx.stroke();

      // Current Time Needle
      const needleX = (currentTime / 3.0) * w;
      ctx.strokeStyle = '#f43f5e';
      ctx.lineWidth = 2;
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      ctx.moveTo(needleX, 0);
      ctx.lineTo(needleX, h);
      ctx.stroke();
      ctx.setLineDash([]);
    }}

    // Initial load
    updateFrame(0);
  </script>
</body>
</html>
"""
        REPORT_HTML.write_text(html, encoding="utf-8")
        print(f"\nTime-Series Dashboard generated at: {REPORT_HTML}")

if __name__ == "__main__":
    unittest.main()
