# PUB-02-A independent review

2026-10-19, Asia/Bangkok. Read-only review by the separate
`/root/f2_independent_review` agent of the uncommitted PUB-02-A diff and new files,
against [RET-01..04](../../in-progress/M2_MOC3_PUB02_A_RETAINED_SESSION_DESIGN.md).
The [source closure](../../reference/audits/M2_MOC3_PUB02_A_SOURCE.json) pins the
reviewed/tested code; reports are subsequent public-safe handoff artifacts.

**Final scoped verdict: PASS.** No open supported P0/P1/P2 findings.

## Finding and correction

RET-04/P2: the initial verifier discarded geometry evidence and checked only
pixels; opacity/color-only changes could pass without proving geometry motion.
Initial verdict was REVISE. The corrected verifier extracts XY positions only
from independently validated retained records and requires default != driven,
default == released. Generated negative tests cover opacity, color, UV and atlas
changes, with a positive actual-XY control. The corrected oracle first failed
on the old fixture binding; final motion-binding selection still requires the
actual XY witness rather than trusting its name.

Reviewer independently reran the five frontend/oracle tests (PASS), checked the
XY witness on actual retained output and reconciled fresh
`ff54dbf09fda42c9b25a3c65` child/parent evidence against current exact pins.
Both backends had mapped/sample identity checks, at least three presentations,
API sequences1..3, and removed observer descriptor. Finding:
**FIXED_WITH_PROOF**.

## Other reviewed boundaries

RET-01..03: bounded command parsing, complete overrides, native catalog preflight,
version separation, all-record validation before GPU and unchanged sandbox limits
supported. Reviewer independently observed native sequence694, static packet379,
Physics3 admission18, broker5/5 and initial retained frontend3/3 PASS; final
frontend/oracle suite5/5 PASS. No additional supported protocol/security P0-P2.

Architecture assessment: **SUPPORTED**. Project structure assessment:
**SUPPORTED**. Optimization proposals: **NO_RECOMMENDATION**.

The reviewer did not rerun the later static proof; its final PASS and frozen
closure are directly checked by the delivery assessment in the
[acceptance report](M2_MOC3_PUB02_A_ACCEPTANCE.md). Full PUB-02/PUB-04 remain HOLD;
Physics3 is validated, not simulated. This review grants no merge or activation.
