# Model privacy and public GitHub Codex review, 2026-10-25

Repository: boxs-51/Avatar_Runtime, GitHub visibility PUBLIC.
Local base: beta experimental HEAD 0f8602f.
Purpose: review SOURCE, TEST and DESIGN files only. Keep MOC3, UV,
authored ARSMESH runtime, sidecars, textures, screenshots, data captures,
IPC logs, credentials and user-specific private files off GitHub.
User approved a branch push and GitHub review, NOT merge/deployment.

## Trust boundary

Native Studio handles private Cubism runtime model buffers, local
STUDIO_GEOMETRY_5 source and compiled ARSMESH3 assets. GitHub public PRs
cannot be used to store these private user data. Local API is not
equivalent to OS sandbox or publicly safe remote API.

## RED -> GREEN publication prevention

Synthetic test reproduced bypass for renamed Studio authoring sidecar
and ARSMESH output. Publication gate extended to identify:
- STUDIO_GEOMETRY header, even when BOM/leading whitespace present.
- ARSMESH1/2/3 binary signatures under arbitrary filenames.
- Authored sidecar/runtime output and backup filename patterns.
- Model manifest and geometry JSON even renamed as generic .dat files.
- Existing MOC3, images, archives, unsafe binaries/private directories.

Gitignore now covers authored source/output/backup as well as existing
Cubism formats. No real user model artifacts are included in tests.

## Evidence and limits

Model publication tests: 22 PASS, 1 SKIP.
HEAD and Git index full tracked blob scan: PASS, 662 blobs.
101 changed source/text files screened locally: no flagged media/model
binary or authored source payloads; no hard-coded credentials or direct
outbound HTTP upload calls found in reviewed candidate text.
Tracked observed history: no .moc3 paths. This is not full forensic
history proof. The exact staged index MUST be rescanned before push.

Code/security audit is SELF_REVIEW_ONLY. Independent Codex review
is NOT YET CONCLUDED. Cannot guarantee absence of unknown encoded data,
all host leaks, supply-chain risks or GUI input authorization failures.

## Review contract for GitHub

Review exact PR diff: privacy exfil/export, risky logs, source sidecar
writing, identity/CAS/ABA, Rig graph cycle/deletion, undo/recovery,
binary V1-V3 backward reads, Native GPU frame acceptance and tests.
Classify findings P0/P1/P2/P3 with line evidence. Do not auto-merge,
deploy, activate G5 writer or publish model files.


## Codex CLI secondary security review (2026-10-25)

Codex CLI 0.164.0 authenticated on Windows Host. Direct uncommitted
review was BLOCKED by PowerShell current-directory behavior and is NOT
counted as successful review. Source-only Codex review succeeded
against sanitized, numbered source through stdin without model assets.
Findings: P1 encoded/renamed source, history deletion, LFS/gitlinks,
IPC receipts, mandatory manifest and Git replacement; P2 backup,
parser, GPU witness and physical test gaps. Repeated review runs led
to tightened publication gate and new negative tests.

Only --push-range is authorized to report PASS. It requires exactly
one direct child commit of the known public base; all tree blob OIDs
and paths equal the external frozen manifest, and base/commit SHAs and
raw commit message hash also match. Git object replacement is disabled
for inspection; symlinks/gitlinks, LFS, encoded sidecars, nested geometry,
renamed payloads and IPC receipts are denied. Diagnostic --ref,
--staged and --package produce CLEAN_NOT_APPROVED, never a push permit.

Python regression: 319 passed, 19 skipped, 229 subtests passed.
Native selected CTest: 17/17 PASS. Some security and GUI acceptance
gates remain unverified. Independent GitHub PR Codex review is still
required and this report is not a release approval.
