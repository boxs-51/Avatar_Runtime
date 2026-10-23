"""Isolated CLI regression, synthetic commands and no Native model."""
import json,subprocess,sys,unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CLI=ROOT/"scripts/avatar_toolkit_v2_cli.py"

class CliTests(unittest.TestCase):
 def command(self,*args,input=None):
  return subprocess.run([sys.executable,str(CLI),*args],input=input,
          cwd=ROOT,capture_output=True,timeout=5)
 def test_discovery_is_readonly(self):
  result=self.command("capabilities")
  self.assertEqual(result.returncode,0)
  obj=json.loads(result.stdout)
  self.assertEqual(obj["state"],"COMPLETED_READ")
  self.assertFalse(obj["result"]["native_v2_enabled"])
 def test_schema_and_unsupported(self):
  good=self.command("schema","avatar.capabilities")
  self.assertEqual(good.returncode,0)
  bad=self.command("schema","draft.commit")
  self.assertEqual(bad.returncode,2)
  self.assertEqual(json.loads(bad.stdout)["state"],"REJECTED")
 def test_private_payload_is_not_reflected(self):
  wire=json.dumps({"api_version":2,"request_id":"req-safe-1",
    "session_id":None,"expected_model_generation":None,
    "expected_edit_revision":None,"operation":"rig.weight.paint",
    "arguments":{"private_payload":"SECRET_SAMPLE_IGNORE"},"mode":"read"}).encode("utf-8")
  result=self.command("--stdin",input=wire)
  self.assertEqual(result.returncode,2)
  self.assertNotIn(b"SECRET_SAMPLE_IGNORE",result.stdout)
  self.assertEqual(json.loads(result.stdout)["state"],"REJECTED")
 def test_cli_invalid_usage(self):
  result=self.command("publish","private")
  self.assertEqual(result.returncode,2)
  self.assertEqual(json.loads(result.stdout)["error"]["code"],"INVALID_ARGUMENT")
if __name__=="__main__":unittest.main()
