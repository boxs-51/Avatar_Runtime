"""RED-first regression for exact native FRAME reply and user selection UX."""
import importlib.util
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

BASE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(BASE.parents[1]/"scripts"))
spec=importlib.util.spec_from_file_location("lps_moc3_viewer", BASE/"moc3_viewer.py")
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
from moc3_local_physics import Bridge
from moc3_local_pixels import PixelError

class Value:
    def __init__(self):self.items=[]
    def set(self,x):self.items.append(x)
class Frames(unittest.TestCase):
    def mock_bridge(self,reply):
        b=Bridge.__new__(Bridge)
        b.frames=3
        b.process=Mock();b.process.poll.return_value=None
        b.send=Mock()
        b.response=Mock(return_value=reply)
        return b
    def test_native_error_provides_actual_response_without_advancing_frame(self):
        b=self.mock_bridge(["LPS_ERROR","stale_or_unbound"])
        with self.assertRaises(PixelError) as failure:b.frame({})
        message=str(failure.exception)
        self.assertIn("F3_FRAME_IDENTITY",message)
        self.assertIn("LPS_ERROR",message)
        self.assertIn("accepted_frames=3",message)
        self.assertEqual(b.frames,3)
    def test_native_parameter_rejection_reports_reason_and_does_not_advance(self):
        b=self.mock_bridge(["ERR","parameter","invalid_parameter"])
        with self.assertRaises(PixelError) as failure:b.frame({})
        self.assertIn("invalid_parameter",str(failure.exception))
        self.assertEqual(b.frames,3)
        b=self.mock_bridge(["ERR","parameter","unsupported_feature"])
        with self.assertRaises(PixelError) as failure:b.frame({})
        self.assertIn("unsupported_feature",str(failure.exception))
        self.assertEqual(b.frames,3)
    def test_protocol_error_preserves_wrong_epoch(self):
        b=self.mock_bridge(["FRAME","4","99","4"])
        with self.assertRaises(PixelError) as failure:b.frame({})
        self.assertIn("'99'",str(failure.exception))
        self.assertEqual(b.frames,3)
    def test_bad_selection_is_inline_and_no_bridge_command(self):
        editor=mod.Editor.__new__(mod.Editor)
        editor.listbox=Mock();editor.listbox.curselection.return_value=()
        editor.status=Value();editor.bridge=Mock()
        editor.index=[(0,12,"Hair")]
        editor.select()
        self.assertIn("Select ONE",editor.status.items[-1])
        editor.bridge.send.assert_not_called()
        editor.listbox.curselection.return_value=(0,1,2)
        editor.select()
        self.assertIn("Select ONE",editor.status.items[-1])
    def test_oversize_selection_is_inline(self):
        editor=mod.Editor.__new__(mod.Editor)
        editor.listbox=Mock();editor.listbox.curselection.return_value=(0,)
        editor.status=Value();editor.bridge=Mock()
        editor.index=[(0,4096,"Hair")]
        editor.select()
        self.assertIn("2048",editor.status.items[-1])
        editor.bridge.send.assert_not_called()
if __name__=="__main__":unittest.main()
