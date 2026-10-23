"""RED-first contracts for actual-model interaction diagnostics."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
STUDIO=ROOT/"tools/avatar-studio"
class ModelInteractionDiagnostics(unittest.TestCase):
    def test_native_metrics_from_accepted_frames(self):
        header=(STUDIO/"studio_xpbd.h").read_text(encoding="utf-8-sig")
        code=(STUDIO/"studio_xpbd.cpp").read_text(encoding="utf-8-sig")
        for name in ("StudioXpbdMetrics","studio_xpbd_metrics","max_step","max_acceleration","pin_drift"):
            self.assertIn(name,header)
            self.assertIn(name,code)
        self.assertIn("last_render_positions",code)
    def test_native_state_json_exposes_only_aggregate_metrics(self):
        code=(STUDIO/"studio.c").read_text(encoding="utf-8-sig")
        self.assertIn("studio_xpbd_metrics(s.xpbd",code)
        self.assertIn('xpbd_max_step',code)
        self.assertIn('xpbd_pin_drift',code)
        self.assertIn('interaction_pull_x',code)
    def test_fixed_vertices_do_not_lag_source_on_presentation(self):
        f=(STUDIO/"studio_motion_filter.hpp").read_text(encoding="utf-8-sig")
        self.assertIn("pinned_mask",f)
        self.assertIn("(*pinned_mask)[i]",f)
        backend=(STUDIO/"studio_xpbd.cpp").read_text(encoding="utf-8-sig")
        self.assertIn("r->pinned",backend)
if __name__=="__main__":unittest.main()
