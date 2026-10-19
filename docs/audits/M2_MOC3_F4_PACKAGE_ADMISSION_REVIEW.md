# Independent static admission review

Reviewer: separate agent `/root/f2_independent_review`; read-only source review and independent test/evidence inspection. This file transcribes the returned review conclusions; it is not a self-review or approval to merge/activate.

**Subject verdict: PASS — static PUB-01 / PUB-03 only.** No remaining supported P0–P2 findings.

* Architecture assessment: **SUPPORTED**. Untrusted decoding stays isolated; a typed immutable scene is minted only after complete independent host validation, before GPU creation.
* Project structure assessment: **SUPPORTED**. Default-OFF targets and separated libraries preserve the runtime/public ABI boundary. The trusted packet host does not link the independent MOC evaluator target.
* Optimization proposals: **NO_RECOMMENDATION**.

The reviewer independently ran the drawable corpus, native Physics3 corpus and five broker custody tests. It inspected the final package, mapped UI and separate API evidence under ignored run `f8b4b0a8ad2841cea2191a5b`: all PASS, descriptor removed, parent and child before/after pins agree, and every current pinned source/binary/dependency matches. The reviewer inspected recorded private results rather than claiming a second independent model execution.

One P2 finding was identified and fixed with direct proof: unwinding a started incomplete GPU query on timeout/loss could terminate through its destructor. The dedicated diagnostic now records quarantine and exits without unwinding query pins. Injected post-submission timeout/loss tests confirm nonpublication and the controlled process exit. This is not physical-loss evidence.

The reviewer explicitly withdrew source/execution closure for the earlier changing-source run `16ed17a8b4c847a988ae7845`; equality with final on-disk files alone did not prove which broker revision was imported. The final parent pins before child creation, then checks after exit. This closes that evidence gap.

Original full-resolution atlas admission exceeds the unchanged profile budget; private parity is for the explicit resized test derivative. Physics is validated, not simulated. Whole F4, public activation, SDK, integrated sanitizer and physical device-loss gates remain outstanding.
