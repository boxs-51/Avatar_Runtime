"""Opt-in external-viewer diagnostics; static inputs/synthetic spring only.
No linked Physics3 or host-renderer acceptance claim. Local reports remain ignored.
"""
from __future__ import annotations
import os
import sys
import json
import base64
import datetime
import unittest
import subprocess
from pathlib import Path

try:
    import numpy as np
    from PIL import Image, ImageDraw
except ImportError:
    np = Image = ImageDraw = None

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
ROOT        = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_viewer_diagnostics import profile, ready, output_root, render
CONFIG = profile()
VIEWER_EXE  = ROOT / "build/moc3-render/viewer/Release/viewer.exe"
MODEL_PATH = CONFIG.get("model", "")
SHOTS_DIR = output_root("test_facial_expressions")
REPORT_HTML = SHOTS_DIR / "report.html"

# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
EXPRESSION_TESTS = CONFIG.get("EXPRESSION_TESTS", [])

# ---------------------------------------------------------------------------
# CamTrack simulation tests — head pose + mouth frames
# These simulate the CamTrack mode (C key in viewer) that drives:
#   ParamAngleX, ParamAngleY, ParamAngleZ  -> nod / turn
#   ParamMouthOpenY                         -> speech
# ---------------------------------------------------------------------------
CAMTRACK_TESTS = CONFIG.get("CAMTRACK_TESTS", [])


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def render_shot(rel_out_name: str, params: dict) -> Path:
    """Run viewer --shot with given params. rel_out_name is relative to SHOTS_DIR."""
    out = SHOTS_DIR / rel_out_name
    render(out, MODEL_PATH, params)
    return out


def pixel_diff(img_a: Path, img_b: Path, mask_hud=True) -> tuple[int, list | None]:
    """Return (changed_pixel_count, bbox [x0,y0,x1,y1] or None)."""
    arr_a = np.asarray(Image.open(img_a).convert("RGB"), dtype=np.int16)
    arr_b = np.asarray(Image.open(img_b).convert("RGB"), dtype=np.int16)
    diff = np.abs(arr_a - arr_b)
    if mask_hud:
        diff[:50, :120] = 0   # mask top-left HUD / FPS counter
    active = np.any(diff > 4, axis=2)
    count = int(active.sum())
    ys, xs = np.nonzero(active)
    bbox = [int(xs.min()), int(ys.min()), int(xs.max() + 1), int(ys.max() + 1)] if count > 0 else None
    return count, bbox


def img_to_b64(path: Path) -> str:
    return base64.b64encode(path.read_bytes()).decode()


def draw_bbox_overlay(shot: Path, bbox: list | None) -> Path:
    overlay = Image.open(shot).convert("RGB")
    if bbox:
        draw = ImageDraw.Draw(overlay)
        draw.rectangle(bbox, outline="#ec4899", width=3)
    out = SHOTS_DIR / f"annotated_{shot.name}"
    overlay.save(out)
    return out


# ---------------------------------------------------------------------------
# Test classes
# ---------------------------------------------------------------------------
class FacialExpressionTests(unittest.TestCase):
    """Validates 7 facial expression renders against baseline pixel-diff thresholds."""

    baseline_path: Path
    base_arr: np.ndarray

    @classmethod
    def setUpClass(cls):
        ready(CONFIG, np, Image)
        cls.baseline_path = render_shot("baseline.png", {})
        cls.base_arr = np.asarray(Image.open(cls.baseline_path).convert("RGB"), dtype=np.int16)
        cls.results: list[dict] = []

    def _run_expression(self, tc: dict):
        out = render_shot(f"{tc['id']}.png", tc["params"])
        count, bbox = pixel_diff(self.baseline_path, out)
        annotated = draw_bbox_overlay(out, bbox)
        self.results.append({
            "tc": tc, "count": count, "bbox": bbox,
            "shot_b64": img_to_b64(out),
            "ann_b64": img_to_b64(annotated),
            "status": "PASS" if tc["min_px"] <= count <= tc["max_px"] else "FAIL",
        })
        return count, bbox

    def test_01_smile(self):
        tc = EXPRESSION_TESTS[0]
        count, bbox = self._run_expression(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"], f"{tc['name']}: {count} > {tc['max_px']}")
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_02_surprised(self):
        tc = EXPRESSION_TESTS[1]
        count, bbox = self._run_expression(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_03_angry(self):
        tc = EXPRESSION_TESTS[2]
        count, bbox = self._run_expression(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_04_blush(self):
        tc = EXPRESSION_TESTS[3]
        count, bbox = self._run_expression(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_05_wink_left(self):
        tc = EXPRESSION_TESTS[4]
        count, bbox = self._run_expression(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_06_wink_right(self):
        tc = EXPRESSION_TESTS[5]
        count, bbox = self._run_expression(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_07_cry_sad(self):
        tc = EXPRESSION_TESTS[6]
        count, bbox = self._run_expression(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    @classmethod
    def tearDownClass(cls):
        if cls.results:
            _generate_report(cls.results, [], cls.baseline_path)


class CamTrackSimulationTests(unittest.TestCase):
    """
    Simulates CamTrack mode data:
      - Head nod (AngleX sweep)
      - Head turn (AngleY)
      - Speech (MouthOpenY)
      - Combined nod + speech combo
    """

    baseline_path: Path

    @classmethod
    def setUpClass(cls):
        ready(CONFIG, np, Image)
        cls.baseline_path = render_shot("camtrack_baseline.png", {})
        cls.results: list[dict] = []

    def _run_camtrack(self, tc: dict):
        out = render_shot(f"{tc['id']}.png", tc["params"])
        count, bbox = pixel_diff(self.baseline_path, out)
        annotated = draw_bbox_overlay(out, bbox)
        self.results.append({
            "tc": tc, "count": count, "bbox": bbox,
            "shot_b64": img_to_b64(out),
            "ann_b64": img_to_b64(annotated),
            "status": "PASS" if tc["min_px"] <= count <= tc["max_px"] else "FAIL",
        })
        return count, bbox

    def test_cam_01_nod_down(self):
        tc = CAMTRACK_TESTS[0]
        count, bbox = self._run_camtrack(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_cam_02_nod_up(self):
        tc = CAMTRACK_TESTS[1]
        count, bbox = self._run_camtrack(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_cam_03_turn_right(self):
        tc = CAMTRACK_TESTS[2]
        count, bbox = self._run_camtrack(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_cam_04_speak_open(self):
        tc = CAMTRACK_TESTS[3]
        count, bbox = self._run_camtrack(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    def test_cam_05_speak_combo(self):
        tc = CAMTRACK_TESTS[4]
        count, bbox = self._run_camtrack(tc)
        self.assertGreaterEqual(count, tc["min_px"], f"{tc['name']}: {count} < {tc['min_px']}")
        self.assertLessEqual(count, tc["max_px"])
        print(f"  PASS {tc['name']}: {count:,} px  bbox={bbox}")

    @classmethod
    def tearDownClass(cls):
        _generate_report([], cls.results, cls.baseline_path)


# ---------------------------------------------------------------------------
# Unified HTML Report generator
# ---------------------------------------------------------------------------
def _generate_report(expr_results: list, cam_results: list, baseline_path: Path):
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    baseline_b64 = img_to_b64(baseline_path)

    def card(r: dict, section_color: str) -> str:
        tc = r["tc"]
        bbox_s = str(r["bbox"]) if r["bbox"] else "—"
        status_cls = "bg-emerald-500/20 text-emerald-400 border-emerald-500/30" if r["status"] == "PASS" else "bg-red-500/20 text-red-400 border-red-500/30"
        return f"""
        <div class="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-col gap-3">
          <div class="flex items-center justify-between">
            <div>
              <div class="flex items-center gap-2 flex-wrap">
                <span class="px-2 py-0.5 rounded text-[11px] font-bold bg-{section_color}-500/20 text-{section_color}-300 border border-{section_color}-500/30">{tc.get('region', tc.get('id',''))}</span>
                <h3 class="text-sm font-bold text-slate-100">{tc['name']}</h3>
              </div>
              <p class="text-xs text-slate-400 mt-1">{tc['desc']}</p>
            </div>
            <span class="px-2.5 py-1 text-xs font-semibold rounded-full {status_cls} border">{r['status']}</span>
          </div>
          <div class="grid grid-cols-2 gap-2 text-xs">
            <div class="bg-slate-950 p-2 rounded-xl border border-slate-800">
              <span class="text-slate-400">Δ Pixels:</span>
              <span class="font-mono font-bold text-pink-400 ml-1">{r['count']:,}</span>
            </div>
            <div class="bg-slate-950 p-2 rounded-xl border border-slate-800">
              <span class="text-slate-400">BBox:</span>
              <span class="font-mono text-amber-400 ml-1 text-[10px]">{bbox_s}</span>
            </div>
          </div>
          <div class="relative w-full bg-black/60 rounded-xl overflow-hidden border border-slate-800 flex justify-center p-1">
            <img src="data:image/png;base64,{r['ann_b64']}" class="max-h-48 object-contain" />
          </div>
        </div>"""

    expr_cards = "\n".join(card(r, "pink") for r in expr_results)
    cam_cards  = "\n".join(card(r, "cyan") for r in cam_results)

    total = len(expr_results) + len(cam_results)
    passed = sum(1 for r in expr_results + cam_results if r["status"] == "PASS")

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Live2D Facial Expression & CamTrack Report</title>
  <style>body{{background:#101827;color:#e5e7eb;font-family:system-ui;margin:2rem}}img{{max-width:100%}}.grid{{display:grid;gap:1rem}}button{{cursor:pointer}}</style>
</head>
<body class="bg-slate-950 text-slate-100 font-sans p-6 min-h-screen">
  <div class="max-w-6xl mx-auto space-y-8">

    <!-- Header -->
    <div class="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl">
      <div class="flex items-start justify-between flex-wrap gap-4">
        <div>
          <h1 class="text-2xl font-black text-white">Live2D MOC3 — Facial Expression & CamTrack Report</h1>
          <p class="text-sm text-slate-400 mt-1">Pixel-diff verification against baseline render. Generated: {ts}</p>
        </div>
        <div class="flex items-center gap-3">
          <span class="px-3 py-1.5 text-sm font-bold rounded-full bg-emerald-500/20 text-emerald-400 border border-emerald-500/30">{passed}/{total} PASSED</span>
        </div>
      </div>
      <div class="mt-4 flex items-center gap-4">
        <div class="text-xs text-slate-500">Baseline render:</div>
        <img src="data:image/png;base64,{baseline_b64}" class="h-24 rounded-lg border border-slate-700 object-contain bg-black" />
      </div>
    </div>

    <!-- Facial Expressions -->
    {"" if not expr_results else f'''
    <div>
      <h2 class="text-lg font-bold text-pink-400 mb-3">🎭 Facial Expressions ({len(expr_results)} tests)</h2>
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-5">
        {expr_cards}
      </div>
    </div>'''}

    <!-- CamTrack Simulation -->
    {"" if not cam_results else f'''
    <div>
      <h2 class="text-lg font-bold text-cyan-400 mb-3">📷 CamTrack Simulation ({len(cam_results)} tests)</h2>
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-5">
        {cam_cards}
      </div>
    </div>'''}

  </div>
</body>
</html>"""

    REPORT_HTML.write_text(html, encoding="utf-8")
    print(f"\n  Report Report saved -> {REPORT_HTML}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
