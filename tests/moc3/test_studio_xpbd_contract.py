"""RED-first native Studio integration: existing XPBD solver must drive rendered mesh."""
import pathlib
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[2]
STUDIO=ROOT/"tools/avatar-studio"

class StudioXpbdWiring(unittest.TestCase):
    def test_native_xpbd_backend_exposes_bounded_admission(self):
        header=(STUDIO/"studio_xpbd.h").read_text(encoding="utf-8")
        source=(STUDIO/"studio_xpbd.cpp").read_text(encoding="utf-8")
        for token in ("studio_xpbd_bind", "studio_xpbd_prepare", "studio_xpbd_accept",
                      "studio_xpbd_positions", "studio_xpbd_disable"):
            self.assertIn(token,header)
            self.assertIn(token,source)
        self.assertIn("native_mesh_xpbd.hpp",source)
        self.assertIn("2048",source)
    def test_renderer_consumes_simulated_drawable_vertices(self):
        code=(STUDIO/"studio_render.c").read_text(encoding="utf-8")
        self.assertIn("#define csmGetDrawableVertexPositions",code)
        self.assertIn("studio_xpbd_positions",code)
        self.assertIn('#include "render.c"',code)
    def test_main_loop_has_bind_prepare_render_accept_order(self):
        code=(STUDIO/"studio.c").read_text(encoding="utf-8")
        for token in ("studio_xpbd_bind(", "studio_xpbd_prepare(", "studio_xpbd_accept(",
                      "studio_xpbd_disable(", "XPBD", "Drawable"):
            self.assertIn(token,code)
        self.assertLess(code.index("studio_xpbd_prepare("),code.index("RenderModel("))
        self.assertLess(code.index("RenderModel("),code.index("studio_xpbd_accept("))
    def test_downsampling_uses_supersampled_internal_viewport(self):
        code=(STUDIO/"studio.c").read_text(encoding="utf-8")
        self.assertIn("STUDIO_RENDER_SCALE",code)
        self.assertIn("s.viewport.width*STUDIO_RENDER_SCALE",code)
        self.assertIn("s.viewport.height*STUDIO_RENDER_SCALE",code)

    def test_motion_smoothing_and_deterministic_substeps(self):
        code=(STUDIO/"studio.c").read_text(encoding="utf-8")
        self.assertIn("s.xpbd_source_transfer=.03f",code)
        self.assertIn("s.xpbd_damping=.96f",code)
        self.assertIn("s.xpbd_step=1.f/60",code)
        self.assertIn("s.xpbd_substeps=4",code)
    def test_build_includes_cpp_and_retains_no_vendor_patch(self):
        code=(STUDIO/"CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("studio_xpbd.cpp",code)

if __name__=="__main__":unittest.main()
