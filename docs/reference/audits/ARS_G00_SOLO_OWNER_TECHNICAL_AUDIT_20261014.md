# ARS G00 Solo Owner technical-method audit — 2026-10-14

**Authority:** owner approved ARS-SOLO-OWNER-001; governance Draft PR #11. The same GitHub account is the sole owner/implementer: `INDEPENDENCE_NOT_AVAILABLE`. No actor-distinct FINAL or GitHub APPROVE is asserted.

**Baseline:** main `637bdae7373aecbf50b18834f3118421d921fe3a`, PR10 `d90393d7ec8556b7c69bf1e1cd468150f8728d93` → Draft child PR12. Source/branch HEAD shown by CI is authoritative; PR10/main are unchanged.

## RED-first findings / remedies

| ID | Type | Concrete negative proof | Resolution |
| --- | --- | --- | --- |
| ARS-G00-FMT-P1-01 | P1 semantic guard | Mask unknown ID, self-loop, two-mask cycle and mixed parent/mask cycle incorrectly fall through to UNSUPPORTED. Test-only PR12 commit `b13c77af96f4e98fe4826945911055b6885b8245` [Linux/Windows RED #38567043771](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38567043771). | Validate combined directed mask+parent graph before capability dispatch in RC1 checker and separately written Profile B oracle; malformed graph returns BAD_ASSET; well-formed disabled mask still UNSUPPORTED. |
| ARS-G00-PINS-P1-02 | P1 CI provenance | Child PR12 correctly uses PR10 parent, while PR10-only strict pin checker demanded PR9: [RED #38567043871](https://github.com/boxs-51/Avatar_Runtime/actions/runs/38567043871). | Pin **one exact audit-child branch and PR10 base SHA**, retain PR8→PR9 ancestry plus nine blob SHA checks; no generic audit-child exemption. This audit-only pin is not directly main-mergeable. |
| ARS-G00-87-P2-03 | P2 audit coverage | 19 HDL + 45 SRC + 23 RACE inventory was prose only, and expected oracle/owner/OS/stage could drift without CI detection. | `docs/G00_SECURITY_87_CASE_INVENTORY.json` enumerates exactly 87 expected outcomes verbatim from normative source, owner custody, Linux/Windows platform plan and `HOLD_RUNTIME`. Six fail-closed tests reject deletion, duplication, oracle weakening, fabricated PASS and lost OS coverage; G07/G08 actual runtime remain HOLD. |
| ARS-G00-DOC-P2-04 | P2 documentation drift | Historical RC1/ABI/ADR records mentioned pre-approval 21-symbol and distinct-review constraints. | Mark historical language explicitly superseded by owner YES ABI20, HOST_DEVICE and Solo Owner method-review exception. No independent sign-off is fabricated. |

## Gate boundary

- DESIGN-ONLY contract checks are not C++ SDK runtime implementation.
- ABI20 declaration and stub link ≠ G01 real SDK or G10 functioning embedded C consumer.
- 87 enumerated negative oracles ≠ 87 executed production security cases; Linux/Windows OS stress and sanitizer G07/G08 HOLD.
- G02 actual ZIP/PNG/GLB parser, graphics G03-G05, motion G06, native GPU D3D11 M1-D, performance G09, SDK G10 and release G11 are not established.
- Required: final exact SHA GCC/Clang/MSVC prep CI, Linux/Windows R5, F03 and Profile B, 87 inventory, zero source P0/P1 regressions; verify GitHub rule compatibility before any eventual merge.
- **G00 status:** design source checks can earn TECHNICAL_METHOD_PASS on their tested subset; full gate remains HOLD unless separately explicitly evaluated and accepted under ARS-SOLO-OWNER-001. No second actor exists.
- **Merge:** NOT AUTHORIZED. PR12 is a draft audit child based on PR10; source pins are deliberately child-scoped. Fresh main-targeted composition, diff, pins, CI and owner MERGE YES required for any future merge.

**Rollback:** abandon Draft PR12 and PR11. Existing PR1–10/main remain untouched.
