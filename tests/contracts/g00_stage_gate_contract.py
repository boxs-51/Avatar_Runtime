"""G00-P0-C staged-boundary regression; design contract only."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
g = (root / "docs/P0_SECURITY_REGRESSION_GATES_V0.1.md").read_text("utf-8")
a = (root / "docs/ACCEPTANCE_GATES_V0.1.md").read_text("utf-8")
assert "before production M1, minimal registry/mixer implementation passes" not in g
assert "G00-P0-C (DESIGN ONLY)" in g
assert "19 HDL + 45 SRC + 23 RACE = 87-case" in g
assert "M1-B / G07/G08" in g and "remain **HOLD" in g
assert "G00-P0-C stage boundary" in a
assert "Actual HDL/SRC/RACE production cases" in a
print("PASS G00-P0-C: design-only gate, real runtime/OS security holds at G07/G08")
