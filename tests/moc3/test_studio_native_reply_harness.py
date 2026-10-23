"""Pure synthetic Windows-sharing and strict ACK-frame regression."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from scripts.test_studio_native_grammar import _poll_reply

TOKEN="0123456789abcdef01234567"

class ResponseHarnessTests(unittest.TestCase):
    def test_transient_permission_error_is_not_false_failure(self):
        with tempfile.TemporaryDirectory() as dirname:
            api=Path(dirname)
            response=api/"response.txt"
            response.write_text(TOKEN+" ERR rejected\n",encoding="ascii")
            real=Path.read_text
            failures=[0]
            def flaky(path,*args,**kwargs):
                if path==response and failures[0]==0:
                    failures[0]+=1
                    raise PermissionError("synthetic sharing denial")
                return real(path,*args,**kwargs)
            with patch.object(Path,"read_text",flaky):
                self.assertEqual(_poll_reply(api,TOKEN,.3),"ERR")
            self.assertEqual(failures[0],1)
            self.assertFalse(response.exists())
    def test_partial_ok_is_not_success(self):
        with tempfile.TemporaryDirectory() as dirname:
            api=Path(dirname)
            response=api/"response.txt"
            response.write_text(TOKEN+" OK",encoding="ascii")
            self.assertEqual(_poll_reply(api,TOKEN,.04),"TIMEOUT")
            self.assertTrue(response.exists())
    def test_wrong_token_not_accepted_or_deleted(self):
        with tempfile.TemporaryDirectory() as dirname:
            api=Path(dirname)
            response=api/"response.txt"
            response.write_text("000000000000000000000000 OK valid\n",encoding="ascii")
            self.assertEqual(_poll_reply(api,TOKEN,.04),"TIMEOUT")
            self.assertTrue(response.exists())
    def test_complete_frame_ok(self):
        with tempfile.TemporaryDirectory() as dirname:
            api=Path(dirname)
            response=api/"response.txt"
            response.write_text(TOKEN+" OK done\n",encoding="ascii")
            self.assertEqual(_poll_reply(api,TOKEN,.2),"OK")
            self.assertFalse(response.exists())

if __name__=="__main__":
    unittest.main()
