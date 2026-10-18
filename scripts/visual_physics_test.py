"""Opt-in external-viewer diagnostics; static inputs/synthetic spring only.
No linked Physics3 or host-renderer acceptance claim. Local reports remain ignored.
"""
from __future__ import annotations
import os
import sys
import json
import base64
import subprocess
from pathlib import Path
try:
    import numpy as np
    from PIL import Image, ImageDraw
except ImportError:
    np = Image = ImageDraw = None

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_viewer_diagnostics import profile, ready, output_root, render
CONFIG = profile()
VIEWER_EXE = ROOT / "build/moc3-render/viewer/Release/viewer.exe"
MODEL_PATH = CONFIG.get("model", "")
SHOTS_DIR = output_root("visual_physics_test")
REPORT_HTML = SHOTS_DIR / "report.html"

TEST_CASES = CONFIG.get("TEST_CASES", [])

def render_shot(output_file: Path, params: list, flags: list):
    render(output_file, MODEL_PATH, params, flags)


def analyze_diff(ref_rgb, shot_rgb):
    diff = np.abs(ref_rgb - shot_rgb)
    # Mask out top-left corner where raylib draws dynamic FPS counter (x: 0..120, y: 0..50)
    diff[:50, :120] = 0
    active = np.any(diff > 4, axis=2)
    changed_count = int(active.sum())
    mean_delta = float(diff.mean())
    max_delta = int(diff.max())
    y_idx, x_idx = np.nonzero(active)
    if len(x_idx):
        bbox = [int(x_idx.min()), int(y_idx.min()), int(x_idx.max() + 1), int(y_idx.max() + 1)]
    else:
        bbox = None

    return {
        "changed_pixels": changed_count,
        "mean_delta": mean_delta,
        "max_delta": max_delta,
        "bbox": bbox,
    }

def image_to_base64(path: Path) -> str:
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("utf-8")

def main():
    ready(CONFIG, np, Image)
    if not VIEWER_EXE.is_file():
        print(f"Error: Viewer executable not found at {VIEWER_EXE}")
        sys.exit(1)
    if not Path(MODEL_PATH).is_file():
        print(f"Error: Model not found at {MODEL_PATH}")
        sys.exit(1)

    SHOTS_DIR.mkdir(parents=True, exist_ok=True)
    baseline_path = SHOTS_DIR / "baseline.png"
    print("[1/3] Rendering baseline frame...")
    render_shot(baseline_path, [], [])

    with Image.open(baseline_path) as img:
        ref_full = np.asarray(img.convert("RGB"), dtype=np.int16)

    print("[2/3] Executing visual & physics test cases...")
    test_results = []
    for tc in TEST_CASES:
        out_file = SHOTS_DIR / tc["filename"]
        render_shot(out_file, tc["params"], tc["flags"])
        with Image.open(out_file) as img:
            shot_full = np.asarray(img.convert("RGB"), dtype=np.int16)

        diff_info = analyze_diff(ref_full, shot_full)
        passed = tc["expected_min_pixels"] <= diff_info["changed_pixels"] <= tc["expected_max_pixels"]

        # Save diff overlay with red bounding box
        diff_img_path = SHOTS_DIR / f"diff_{tc['id']}.png"
        overlay = Image.fromarray(shot_full.astype(np.uint8))
        if diff_info["bbox"]:
            draw = ImageDraw.Draw(overlay)
            draw.rectangle(diff_info["bbox"], outline="#ff0044", width=4)
        overlay.save(diff_img_path)

        test_results.append({
            "tc": tc,
            "passed": passed,
            "diff": diff_info,
            "shot_path": str(out_file),
            "diff_path": str(diff_img_path),
            "shot_b64": image_to_base64(out_file),
            "diff_b64": image_to_base64(diff_img_path),
        })
        status_str = "PASS" if passed else "FAIL"
        print(f"  - [{status_str}] {tc['name']}: {diff_info['changed_pixels']:,} changed px, bbox={diff_info['bbox']}")

    print("[3/3] Generating HTML Interactive Test Bench...")
    baseline_b64 = image_to_base64(baseline_path)

    html_content = generate_html_report(baseline_b64, test_results)
    REPORT_HTML.write_text(html_content, encoding="utf-8")

    print(f"\nVisual Test Bench Complete! Report written to: {REPORT_HTML}")
    return 0 if all(t["passed"] for t in test_results) else 1

def generate_html_report(baseline_b64: str, test_results: list) -> str:
    cards = []
    for idx, tr in enumerate(test_results):
        tc = tr["tc"]
        diff = tr["diff"]
        status_badge = '<span class="px-2.5 py-1 text-xs font-semibold rounded-full bg-emerald-500/20 text-emerald-400 border border-emerald-500/30">PASSED</span>' if tr["passed"] else '<span class="px-2.5 py-1 text-xs font-semibold rounded-full bg-rose-500/20 text-rose-400 border border-rose-500/30">FAILED</span>'

        bbox_str = f"[{diff['bbox'][0]}, {diff['bbox'][1]}, {diff['bbox'][2]}, {diff['bbox'][3]}]" if diff['bbox'] else "None"

        card = f"""
        <div class="bg-slate-900 border border-slate-800 rounded-xl p-5 shadow-lg flex flex-col gap-4">
          <div class="flex items-center justify-between">
            <div>
              <h3 class="text-base font-bold text-slate-100">{tc['name']}</h3>
              <p class="text-xs text-slate-400 mt-0.5">{tc['desc']}</p>
            </div>
            {status_badge}
          </div>

          <div class="grid grid-cols-2 gap-3 text-xs">
            <div class="bg-slate-950 p-2.5 rounded-lg border border-slate-800/80">
              <span class="text-slate-400">Changed Pixels:</span>
              <span class="font-mono font-bold text-slate-200 ml-1">{diff['changed_pixels']:,}</span>
            </div>
            <div class="bg-slate-950 p-2.5 rounded-lg border border-slate-800/80">
              <span class="text-slate-400">Bounding Box:</span>
              <span class="font-mono text-amber-400 ml-1">{bbox_str}</span>
            </div>
          </div>

          <div class="relative bg-slate-950 rounded-lg overflow-hidden border border-slate-800 p-3">
            <div class="flex items-center justify-between mb-2">
              <span class="text-xs font-medium text-slate-400">Interactive A/B Comparison</span>
              <button onclick="toggleCardImage('card-img-{idx}', 'data:image/png;base64,{baseline_b64}', 'data:image/png;base64,{tr['diff_b64']}')"
                      class="px-2.5 py-1 text-xs bg-slate-800 hover:bg-slate-700 text-cyan-300 rounded border border-slate-700 transition">
                Toggle Effect
              </button>
            </div>
            <div class="relative w-full aspect-[16/9] max-h-[320px] rounded overflow-hidden flex items-center justify-center bg-black/60 border border-slate-800/80">
              <img id="card-img-{idx}" src="data:image/png;base64,{tr['diff_b64']}" class="h-full object-contain" />
            </div>
          </div>
        </div>
        """
        cards.append(card)

    cards_joined = "\n".join(cards)

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Live2D MOC3 Visual & Physics Test Bench</title>
  <style>body{{background:#101827;color:#e5e7eb;font-family:system-ui;margin:2rem}}img{{max-width:100%}}.grid{{display:grid;gap:1rem}}button{{cursor:pointer}}</style>
  <script>
    function toggleCardImage(imgId, baseSrc, diffSrc) {{
      const el = document.getElementById(imgId);
      if (el.src === baseSrc) {{
        el.src = diffSrc;
      }} else {{
        el.src = baseSrc;
      }}
    }}
  </script>
</head>
<body class="bg-slate-950 text-slate-100 font-sans p-6 min-h-screen">
  <div class="max-w-6xl mx-auto space-y-6">
    <!-- Header -->
    <div class="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-black tracking-tight text-white flex items-center gap-2">
          <span>Live2D MOC3 Visual & Physics Test Bench</span>
          <span class="text-xs font-mono font-medium bg-cyan-500/20 text-cyan-400 border border-cyan-500/30 px-2 py-0.5 rounded-full">v1.1.0 ABI</span>
        </h1>
        <p class="text-sm text-slate-400 mt-1">Verification of render correctness, masking fidelity, deformation, and physics parameter dynamics.</p>
      </div>
      <div class="text-right">
        <span class="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-emerald-500/10 border border-emerald-500/30 text-emerald-400 font-semibold text-xs">
          <svg class="w-4 h-4 fill-current" viewBox="0 0 20 20"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd"/></svg>
          {"ALL TESTS PASSED" if all(t["passed"] for t in test_results) else "TEST FAILURES"}
        </span>
      </div>
    </div>

    <!-- Interactive Launch Tip -->
    <div class="bg-gradient-to-r from-indigo-900/40 via-purple-900/30 to-slate-900 border border-indigo-500/30 rounded-xl p-4 flex items-center justify-between">
      <div class="flex items-center gap-3">
        <div class="p-2 bg-indigo-500/20 rounded-lg text-indigo-400">
          <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 15l-2 5L9 9l11 4-5 2zm0 0l5 5M7.188 2.239l.777 2.897M5.136 7.965l-2.898-.777M13.95 4.05l-2.122 2.122m-5.657 5.656l-2.12 2.122"/></svg>
        </div>
        <div>
          <div class="text-sm font-semibold text-indigo-200">Interactive Desktop Live Mode Available</div>
          <div class="text-xs text-indigo-300/80">Launch <code class="bg-slate-950 px-1.5 py-0.5 rounded text-indigo-300">launch_interactive_viewer.bat</code> to control all sliders with GUI, pan/zoom, and live 60fps rendering.</div>
        </div>
      </div>
    </div>

    <!-- Test Cards Grid -->
    <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
      {cards_joined}
    </div>
  </div>
</body>
</html>
"""

if __name__ == "__main__":
    raise SystemExit(main())
