# Alpha #60 complete public CPU lifecycle: local acceptance

2026-10-21 Asia/Bangkok; Windows x64 / Visual Studio18. Baseline alpha
`5d90997e3f1e6df0ed5bef2b77318f815aa17769`, branch
`dev/alpha-60-cpu-lifecycle-complete`. This acceptance covers F1-F5 in the
[full design](../../in-progress/ALPHA_60_PUBLIC_CPU_COMPLETION_DESIGN.md).
Local runtime and final post-fix installed verification have passed. Final
receipt/source/binary reconciliation is recorded below. Committed custody
and integration are recorded below and on issue60/its integration PR. This
record does not itself assert an unperformed merge.

## Observable requirements

| ID | Direct local observation |
| --- | --- |
| F1 | Complete typed native2D/GLB3D/dual transfer with original hierarchy/default mode/mappings, dense numeric accessor data and RGBA;151 full-profile cases and150 original2D cases. Native reference compares all CPU fields and prepared2D/3D geometry/atlas. Recomputed-digest hostile tree/accessor/image/graph/domain inputs reject while preserving prior model/storage. GLB138 decode/52 encode OOM points and5879 truncations; full2D29/13 and754 truncations. Encoded ZIP/PNG/JSON/GLB functions absent from four host libraries. |
| F2 | Runtime Resource256MiB,64 model and16 character limits;65th model/17th character fail with zero outputs, slots reuse after retirement. Characters and historical snapshots retain released models; close waits for real pins. All140 observed model load allocations individually fail safely, plus character/publication OOM.96 destroy/admin-release cycles reclaim all64 source records. Allocator callbacks test reentry and exact allocation/free sizes/alignment. |
| F3 | ABI20 unchanged; default-off CPU profile supports model/character/control/source/ingress/update. One compact batch validates every affected character/source before no-fail atomic publication; later-character failure preserves all prior queues/caches/snapshots and does not bind S/time. Public16-character success and wrong-thread checks pass. Six value kinds, per-channel cache/confidence, priority, stable bool ties, quaternion hemisphere, FIFO16 saturation/retry/no-replay, revoke/rotate/expiry/clock/close/generation/races have direct API or native final-fence proof. Shared host-clock highwater and exclusive callback cover both ingress/update, including already-admitted ingress during actual S callback. |
| F4 | Actual AppContainer/Job/pipe loads with fixed module-adjacent helpers. Missing/hang/crash/flood/malformed/readonly/hostile cwd/PATH/env all pass expected statuses and observed cleanup. Owned quarantine and orphan slot retain actual process/job/profile/temp custody and runtime work pin; close is BUSY until independently observed cleanup. No TDR/device reset or fake physical evidence. |
| F5 | Installed+physically relocated C11/C++20 static/shared Release/Debug CPU consumers24 executions, hostile lookup ignored, source-before/after stable. Feature-off original four-config script unchanged, exact20 DLL exports, sentinel-header/configuration/platform/opt-in negatives. Full43 configured CTests PASS,155.07s, including112-case original verdict probe,90-bundle native oracle and original MOC3 harnesses. Two separately rebuilt original MOC3 negatives preserve MEMORY_DENIED/INVALID_ENVELOPE and emptyJob/deletedProfile. |

The public CPU profile advertises motion ingress only. Render/drain remain
UNSUPPORTED. Empty2D and GLB catalogs run through public C11/C++20
lifecycle; zero semantics do not silently increase authority limits. Mode/layer
controls preserve local values and reject invalid changes without mutation.

## Commands and provenance

```powershell
cmake --build build/alpha60-isolated-native --config Release --parallel 4
ctest --test-dir build/alpha60-isolated-native -C Release --output-on-failure
python scripts/verify_cpu_full_transfer.py --bin build/alpha60-isolated-native/bin/Release --corpus build/alpha60-isolated-native/asset-corpus --evidence build/alpha60-isolated-native/full-model-evidence
build/alpha60-public-cpu/bin/Release/ars_cpu_api_tests.exe build/alpha60-isolated-native/asset-corpus/positive-dual-full.arschar
# Pass the full-transfer receipt inputs/rich_fields.arschar and --typed for all value kinds.
# ars_cpu_custody_tests.exe <generated-dual> owned|orphan checks real runtime cleanup pins.
python scripts/verify_cpu_helper_faults.py --bin build/alpha60-isolated-native/bin/Release --asset build/alpha60-isolated-native/asset-corpus/positive-dual-full.arschar --evidence build/alpha60-helper-faults
python scripts/verify_public_cpu_sdk.py --corpus build/alpha60-isolated-native/asset-corpus --evidence build/alpha60-installed-cpu
python scripts/verify_installed_sdk.py --work-root <fresh-local-evidence-directory> --configs Release Debug
python -m unittest discover -s tests/contracts -p "test_*.py"
```

Each receipt below stores original source/binary hashes and actual outcomes;
raw receipts/logs remain local ignored build artifacts. SHA256 is byte identity,
not proof of trust in a compromised parser. Host semantic validation is independent.
A cleanup test that inventories system temporary folders must run without other
CPU admission suites: one concurrent attempt saw another suite's fresh folder
and failed the global observation. The serialized rerun passed unchanged cleanup
assertions. This was not used to waive or rewrite the failed receipt.

## Independent review and assessment

Separate read-only reviewers f1_independent_review (runtime/authority/publication)
and f2_independent_review (typed transfer/custody/package) reviewed the actual
working-tree delta against the frozen scope. Resolved findings: full-profile
pixel/layer/ID/key parity, bind rollback ownership, multi-character atomicity,
admin/empty-catalog compatibility, orphan/read-only custody, last-batch-pin guard
lifetime and shared clock exclusion/highwater. Final focused reviews PASS, no
remaining supported P0-P2 in their scopes. Reviewers performed no OS trials;
primary OS evidence was executed locally by the implementation agent.

The clock defect also has a **reconstructed pre-fix** RED/GREEN receipt, explicitly
not a historical commit or a claim about chronological pre-implementation proof.
Replacing only the reviewed CPU sampling branch reproduces expected
CLOCK_REGRESSION14 versus actual OK0. A finally block restores byte-identical
final source, rebuilds and observes GREEN, including the preadmitted ingress test.
No frozen negative/status oracle was relaxed.

Final configuration inspection also reproduced an incorrectly registered test:
CPU-ON ran the frozen feature-off b1_public_c oracle (five expected feature-off
assertions failed). The oracle source/assertions remain unchanged, now registered
for OFF only. CPU-ON registers five real public API/C11/C++20/custody cases with
explicit generated-corpus and adjacent-helper build dependencies; common C++
lifecycle/security tests remain enabled. The five tests passed18.70s with
--no-tests=error, and the focused independent CMake/profile review PASS. An early
script assertion prevented the first intended CMake edit; its subsequent0-test
invocation was discarded, never counted as verification.

The enabled common suite then caught two actual regressions (21/23 PASS): eager
cpu_reap management pins changed the ordinary finalizer race outcome, and the
relay callback marker changed private close-during-allocation rollback. The
adapter now finalizes first, reaping only after BUSY, while relay markers arm
only during active public CPU operations. Unarmed guards never clear a marker.
Both unchanged legacy tests now pass alongside public reentry/custody: targeted
7/7 PASS15.81s, then all23 enabled core/public/custody tests PASS50.99s.
Independent runtime review closed both findings. These failures were fixed,
not excluded from the enabled suite.

F1-F5 outcome proof is direct for this Windows CPU scope. Physical D06/D08,
production GPU/context integration (#38), render acceptance (#39), whole security
and public gate matrices (#41/#42), fuzz/platform/performance (#61), shipping
release (#62), MOC3/physics and public activation remain outside this acceptance.
Main, public header/original ABI implementation blobs, physical execution ledgers
and historical DOCUMENTATION_PATHS are unchanged. Private model/derived files
are excluded. Local evidence is primary; CI is a separate cross-check.

## Final receipt inventory

| Evidence | Local receipt/log | SHA256 |
| --- | --- | --- |
| Full typed transfer151 | `build/alpha60-isolated-native/full-model-evidence/b50913520b2a44949f68e09fbdb68317/evidence.json` | `34f7873353fa02a026387dfcbaa00725ce2a6aaff9f727869c6f02190c77344f` |
| Original typed2D150 | `build/alpha60-isolated-native/cpu-model-evidence/1e9ada5ed8194a93943f2e29e8daddc8/evidence.json` | `51f7af3a4192de48038ae253a68c6efd5b1f8ef58f52679fcf1efb6a67119394` |
| Original verdict probe112 | `build/alpha60-isolated-native/cpu-admission-evidence/175f9a0df97f41ce81ce7ee5d67dc426/evidence.json` | `d7a8d74e791d9457ee926791a4ad6b2aa78543c17c3a709f3ff52c2d39e86a96` |
| Final API/custody/catalog11 | `build/alpha60-final-api/b164c605c4314777b9c5e65488caf0e6/evidence.json` | `ca7218deb41d65ca0f431d0d2ea127951f3f255d6bfb318bc82e304588738dad` |
| Final helper faults7 | `build/alpha60-helper-faults/9a26162dca174c47bcc37bfaa33f568c/evidence.json` | `6381784e25fa437813e9920f8798c612fa24a5253cc6d88b57459e484dcdd35b` |
| Installed CPU4configs/24runs | `build/alpha60-installed-cpu/088c5a71874e448689108fd30373bce8/evidence.json` | `e88216a30cad6273408b123d5bc341729f72bc780a9800665d95ee53cacccefd` |
| Installed feature-off4configs | `build/alpha60-featureoff-qualified/evidence.json` | `a769d64251ee713b5241e357ca80a283f4be0c908a045ee68df87274198eddad` |
| Parser-free host4libraries | `build/alpha60-link-closure/66bd6eefe1c14545bc6b150a60d1839f/evidence.json` | `1fea06798f69b8922a39b01f1c3fb4ababf1b070c7574cf14899e3175b8865dc` |
| Original MOC3 two negatives | `build/alpha60-original-moc3-proof.json` | `aa8b2c807f42e0aa089baa345beba2f526535afa799205f9d1eece70ec31262a` |
| CPU-enabled23/23,50.99s | `build/alpha60-enabled-core-qualified.log` | `9fa1cf70bcfc075c4dbe1d52a85ed4e384fae101956da2477851285ab71415a3` |
| Final bounded custody2/2,1.30s | `build/alpha60-custody-observation-qualified.log` | `e9a6dbab9f4575a711adb511f43fe5e712d75f5623f8d9d77bcb41fe61983a94` |
| Native43/43,155.07s | `build/alpha60-native-final-ctest.log` | `2dbf4d6fb0319c09074e818bc039ae7218000220e04718de1e7d9cdfd3130f4a` |
| Precommit contracts178,146.929s | `build/alpha60-final-contracts.log` | `b0169f1ea2c356b111c21f08d5f3e6e0f3bc66df4fb7b15cd1bfb283f9c6520f` |
| Scoped reconstructed clock RED/GREEN | `build/alpha60-clock-reconstructed-red/evidence.json` | `1b41c2d06a6cdbb53ca6bf73469f83c6b6ef71df11f340cd3ca0ccc22cac45e4` |
| Final source/binary reconciliation | `build/alpha60-final-source-custody.json` | `6977cd9d74b1825adcac56ac526b9b406d63b6ec6395d8a0bede0bf29497f4fe` |

Every final source/binary receipt above was checked against current bytes. The
43-test native run preceded only CPU-profile CTest routing and CPU-only adapter
corrections; all its compiled feature-off runtime/asset sources remain unchanged.
Final112/150/151 receipts and enabled23 cover the relevant later configuration.
One final immediate-completion custody assertion observed still-pending OS
cleanup. Its exact owned profile/temp were manually retired in a separately
labelled nongate cleanup record. The fixture now observes for at most5s, requiring
real retained work pins for every pending query and actual COMPLETE before
finish. It was independently reviewed and both cases rerun; installed-header empty-catalog executions are in the final11 receipt.
Rolling development logs/early non-UUID API diagnostics are not immutable final
receipts. The UUID final receipts above are retained without later rewriting.

F1-F5 are VERIFIED for local Windows CPU implementation, constraints preserved,
required independent review satisfied, no open supported P0-P2. Delivery and
whole issue closure remain contingent on exact committed55-path custody and
alpha integration. The PR/issue will record immutable source/merge SHAs and
committed contract results. This does not confer release/activation approval.
