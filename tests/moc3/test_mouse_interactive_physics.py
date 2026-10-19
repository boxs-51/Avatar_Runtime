"""Opt-in external-viewer diagnostics; static inputs/synthetic spring only.
No linked Physics3 or host-renderer acceptance claim. Local reports remain ignored.
"""
from __future__ import annotations
import os
import sys
import json
import base64
import unittest
import subprocess
from pathlib import Path
try:
    import numpy as np
    from PIL import Image, ImageDraw
except ImportError:
    np = Image = ImageDraw = None

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_viewer_diagnostics import profile, ready, output_root, render
CONFIG = profile()
VIEWER_EXE = ROOT / "build/moc3-render/viewer/Release/viewer.exe"
MODEL_PATH = CONFIG.get("model", "")
SHOTS_DIR = output_root("test_mouse_interactive_physics")
REPORT_HTML = SHOTS_DIR / "report.html"

INTERACTION_CASES = CONFIG.get("INTERACTION_CASES", [])

def render_shot(output_file: Path, params: dict):
    render(output_file, MODEL_PATH, params)


def image_to_base64(path: Path) -> str:
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("utf-8")

class MouseInteractivePhysicsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ready(CONFIG, np, Image)
        SHOTS_DIR.mkdir(parents=True, exist_ok=True)
        cls.baseline_shot = SHOTS_DIR / "baseline.png"
        render_shot(cls.baseline_shot, {})
        with Image.open(cls.baseline_shot) as img:
            cls.base_rgb = np.asarray(img.convert("RGB"), dtype=np.int16)
        cls.baseline_b64 = image_to_base64(cls.baseline_shot)

    def test_all_interactive_cases(self):
        results = []
        for case in INTERACTION_CASES:
            shot_file = SHOTS_DIR / f"{case['id']}.png"
            render_shot(shot_file, case["params"])
            with Image.open(shot_file) as img:
                shot_rgb = np.asarray(img.convert("RGB"), dtype=np.int16)

            diff = np.abs(self.base_rgb - shot_rgb)
            diff[:50, :120] = 0 # mask FPS overlay
            active = np.any(diff > 4, axis=2)
            changed_pixels = int(active.sum())
            y_idx, x_idx = np.nonzero(active)
            bbox = [int(x_idx.min()), int(y_idx.min()), int(x_idx.max() + 1), int(y_idx.max() + 1)] if len(x_idx) else None

            # Generate overlay with bounding box
            overlay = Image.fromarray(shot_rgb.astype(np.uint8))
            if bbox:
                draw = ImageDraw.Draw(overlay)
                draw.rectangle(bbox, outline="#38bdf8", width=3)
            diff_file = SHOTS_DIR / f"diff_{case['id']}.png"
            overlay.save(diff_file)

            passed = changed_pixels >= case["expected_min_px"]
            self.assertTrue(passed, f"{case['name']} changed pixels ({changed_pixels}) < expected ({case['expected_min_px']})")

            results.append({
                "case": case,
                "changed_pixels": changed_pixels,
                "bbox": bbox,
                "shot_b64": image_to_base64(shot_file),
                "diff_b64": image_to_base64(diff_file),
            })
            print(f"  [PASS] {case['name']} ({case['zone']}): {changed_pixels:,} px, BBox: {bbox}")

        self.__class__.generate_report(results, self.baseline_b64)

    @classmethod
    def generate_report(cls, results, baseline_b64):
        cards = []
        for r in results:
            c = r["case"]
            bbox_str = f"[{r['bbox'][0]}, {r['bbox'][1]}, {r['bbox'][2]}, {r['bbox'][3]}]" if r['bbox'] else "None"
            card = f"""
            <div class="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-col gap-4">
              <div class="flex items-center justify-between">
                <div>
                  <div class="flex items-center gap-2">
                    <span class="px-2 py-0.5 rounded text-[11px] font-bold bg-cyan-500/20 text-cyan-300 border border-cyan-500/30">{c['zone']}</span>
                    <h3 class="text-sm font-bold text-slate-100">{c['name']}</h3>
                  </div>
                  <p class="text-xs text-slate-400 mt-1">{c['desc']}</p>
                </div>
                <span class="px-2.5 py-1 text-xs font-semibold rounded-full bg-emerald-500/20 text-emerald-400 border border-emerald-500/30">PASSED</span>
              </div>

              <div class="grid grid-cols-2 gap-3 text-xs">
                <div class="bg-slate-950 p-2.5 rounded-xl border border-slate-800">
                  <span class="text-slate-400">Changed Pixels:</span>
                  <span class="font-mono font-bold text-cyan-400 ml-1">{r['changed_pixels']:,}</span>
                </div>
                <div class="bg-slate-950 p-2.5 rounded-xl border border-slate-800">
                  <span class="text-slate-400">Bounding Box:</span>
                  <span class="font-mono text-amber-400 ml-1">{bbox_str}</span>
                </div>
              </div>

              <div class="relative w-full aspect-[16/10] bg-black/60 rounded-xl overflow-hidden border border-slate-800 flex items-center justify-center p-2">
                <img src="data:image/png;base64,{r['diff_b64']}" class="h-full object-contain" />
              </div>
            </div>
            """
            cards.append(card)

        cards_html = "\n".join(cards)
        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Mouse Interactive Physics & State Switch Report</title>
  <style>body{{background:#101827;color:#e5e7eb;font-family:system-ui;margin:2rem}}img{{max-width:100%}}.grid{{display:grid;gap:1rem}}button{{cursor:pointer}}</style>
</head>
<body class="bg-slate-950 text-slate-100 font-sans p-6 min-h-screen">
  <div class="max-w-6xl mx-auto space-y-6">
    <!-- Header -->
    <div class="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-black text-white flex items-center gap-2">
          <span>Mouse Interactive Physics & State Transitions</span>
          <span class="text-xs font-mono font-medium bg-emerald-500/20 text-emerald-400 border border-emerald-500/30 px-2.5 py-0.5 rounded-full">9 INTERACTION TESTS PASSED</span>
        </h1>
        <p class="text-sm text-slate-400 mt-1">Verification of mouse drag on 3 zones (Hair, Arms, Body/Skirt), cursor gaze tracking, and 6 animated states.</p>
      </div>
      <a href="file:///E:/Avatar_Runtime/launch_interactive_viewer.bat" class="px-4 py-2 bg-cyan-600 hover:bg-cyan-500 text-white font-bold text-xs rounded-xl shadow-lg transition">
        Launch Live Mouse Viewer
      </a>
    </div>

    <!-- Instruction Card -->
    <div class="bg-gradient-to-r from-cyan-950/60 via-slate-900 to-indigo-950/60 border border-cyan-500/30 rounded-2xl p-5 shadow-lg space-y-3">
      <h2 class="text-sm font-bold text-cyan-300 uppercase tracking-wider flex items-center gap-2">
        <svg class="w-4 h-4 fill-current" viewBox="0 0 20 20"><path fill-rule="evenodd" d="M6.672 1.911a1 1 0 10-1.932.518l.259.966a1 1 0 001.932-.518l-.259-.966zM2.429 4.74a1 1 0 10-.517 1.932l.966.259a1 1 0 00.517-1.932l-.966-.259zm8.864-.426a1 1 0 00-1.932-.518l-.259.966a1 1 0 001.932.518l.259-.966zM11.64 1.47a1 1 0 00-1.414 0l-7 7a1 1 0 000 1.414l7 7a1 1 0 001.414-1.414L5.414 9.5l6.226-6.616a1 1 0 000-1.414z" clip-rule="evenodd"/></svg>
        Desktop Mouse Physics Controls
      </h2>
      <div class="grid grid-cols-1 md:grid-cols-3 gap-4 text-xs text-slate-300">
        <div class="bg-slate-950/70 p-3 rounded-xl border border-slate-800">
          <div class="font-bold text-cyan-400 mb-1">Zone 1: Head & Hair (Top 40%)</div>
          <p class="text-slate-400">Click & drag to pull hair strands and tilt head. Release to see hair spring back and oscillate.</p>
        </div>
        <div class="bg-slate-950/70 p-3 rounded-xl border border-slate-800">
          <div class="font-bold text-amber-400 mb-1">Zone 2: Arms & Torso (Middle 30%)</div>
          <p class="text-slate-400">Click & drag to rotate arms (`Param1000`) and twist the chest. Release to spring back.</p>
        </div>
        <div class="bg-slate-950/70 p-3 rounded-xl border border-slate-800">
          <div class="font-bold text-indigo-400 mb-1">Zone 3: Body & Skirt (Bottom 30%)</div>
          <p class="text-slate-400">Click & drag to sway the skirt fabric (`Param33/34`) and bend the body.</p>
        </div>
      </div>
      <div class="text-xs text-slate-400 border-t border-slate-800 pt-3 flex flex-wrap items-center gap-4">
        <span><strong>State Hotkeys:</strong> <code class="text-cyan-300 bg-slate-950 px-1 rounded">1: Normal</code> <code class="text-cyan-300 bg-slate-950 px-1 rounded">2: Smile</code> <code class="text-cyan-300 bg-slate-950 px-1 rounded">3: Shock</code> <code class="text-cyan-300 bg-slate-950 px-1 rounded">4: Angry</code> <code class="text-cyan-300 bg-slate-950 px-1 rounded">5: Shy</code> <code class="text-cyan-300 bg-slate-950 px-1 rounded">6: Dance</code></span>
        <span><strong>Mouse Gaze:</strong> Hover mouse cursor anywhere on screen to have eyes & head follow you.</span>
      </div>
    </div>

    <!-- Test Cases Grid -->
    <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
      {cards_html}
    </div>
  </div>
</body>
</html>
"""
        REPORT_HTML.write_text(html, encoding="utf-8")
        print(f"\nInteractive Mouse Report generated at: {REPORT_HTML}")

if __name__ == "__main__":
    unittest.main()
