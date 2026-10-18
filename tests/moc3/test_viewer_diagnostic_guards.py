"""Generated negative tests for optional user-owned viewer diagnostics."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import moc3_viewer_diagnostics as diagnostics

class DiagnosticGuards(unittest.TestCase):
    def test_failed_viewer_cannot_reuse_old_image(self):
        out = diagnostics.output_root('guard') / 'old.png'
        out.parent.mkdir(parents=True)
        out.write_bytes(b'old output')
        with patch.object(diagnostics.subprocess, 'run', return_value=subprocess.CompletedProcess([], 7)):
            with self.assertRaisesRegex(RuntimeError, 'no fresh evidence'):
                diagnostics.render(out, ROOT/'local-assets'/'synthetic.model3.json', {})
        self.assertFalse(out.exists())
        out.parent.rmdir()

    def test_missing_output_is_failure_even_with_exit_zero(self):
        out = diagnostics.output_root('guard') / 'missing.png'
        with patch.object(diagnostics.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0)):
            with self.assertRaises(RuntimeError):
                diagnostics.render(out, ROOT/'local-assets'/'synthetic.model3.json', {})
        out.parent.rmdir()

    def test_no_opt_in_skips_without_starting_viewer(self):
        with patch.object(diagnostics.subprocess, 'run') as run:
            with self.assertRaises(unittest.SkipTest):
                diagnostics.ready({}, None, None)
        run.assert_not_called()

    def test_report_cannot_advertise_success_for_failure(self):
        spec = importlib.util.spec_from_file_location('visual', ROOT/'scripts/visual_physics_test.py')
        module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        case = {'name':'generated','desc':'generated','expected_min_pixels':1,'expected_max_pixels':2}
        record = {'tc':case,'passed':False,'diff':{'changed_pixels':0,'bbox':None},'shot_b64':'','diff_b64':''}
        html = module.generate_html_report('', [record])
        self.assertIn('TEST FAILURES', html)
        self.assertNotIn('ALL TESTS PASSED', html)

if __name__ == '__main__':
    unittest.main()
