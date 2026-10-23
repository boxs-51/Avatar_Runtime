import json,unittest
from scripts.studio_public_report import safe_case,safe_json

class PublicReportTests(unittest.TestCase):
    def test_known_safe_classification(self):
        report=safe_json([
            {"case_id":"F01-01","status":"PASS","error_class":"none"},
            {"case_id":"F02-02","status":"REVISE","error_class":"race"}])
        obj=json.loads(report)
        self.assertEqual(obj["schema"],"studio-public-safe-v1")
        self.assertEqual(len(obj["cases"]),2)
    def test_private_or_model_derived_fields_rejected(self):
        good={"case_id":"F01-01","status":"PASS","error_class":"none"}
        for key,value in (("raw_receipt","private"),("model_path","sensitive"),
                          ("vertex_count",123),("image_b64","abcd"),
                          ("elapsed_ms",23.4),("hash","abc")):
            with self.subTest(key=key),self.assertRaises(ValueError):
                safe_case({**good,key:value})
    def test_disguised_payload_and_status_rejected(self):
        for case in ("F01 path","MODEL/USER","F01\nPRIVATE","F01-01:9"):
            with self.subTest(case=case),self.assertRaises(ValueError):
                safe_case({"case_id":case,"status":"PASS","error_class":"none"})
        with self.assertRaises(ValueError):
            safe_json([{"case_id":"F01-01","status":"PASS","error_class":"none"}]*2)
        with self.assertRaises(ValueError):
            safe_case({"case_id":"F01-01","status":"PASS","error_class":"privacy"})
if __name__=="__main__":unittest.main()
