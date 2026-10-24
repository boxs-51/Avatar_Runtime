# Avatar Runtime SDK

Embeddable real-time 2D/3D character-rendering SDK under development.

**Status:** CPU runtime/validation components and optional private D3D11/MOC3 diagnostic paths are implemented. The production SDK and public model/character/render API are not released. Private test acceptance does not close the product gates.

**Principles:** host-owned integration, logical character with optional 2D/3D representations, normalized motion input, secure user-model loading, capability-based renderer, C ABI, testable acceptance gates.

For SDK packaging and the rule **never publish non-public model information**, see [ADR-005](docs/reference/ADR_005_MODEL_INFORMATION_NONPUBLIC_AND_SDK_PACKAGING.md). This governs GitHub/releases/logs, **not** authorized local model editing.

Start with the [documentation index and classification](docs/DOCUMENTATION_INDEX.md). It separates completed scopes, reference material, superseded plans and ongoing implementation, and identifies stale status text in historical documents.

Active integration branch: **alpha**. Follow the [linear issue backlog](docs/in-progress/ALPHA_LINEAR_BACKLOG.md)
and [integration/architecture audit](docs/completed/audits/ALPHA_INTEGRATION_AND_ARCHITECTURE_AUDIT_20261019.md).
Alpha combines the development branches, including the separate teardown receipt.
Known rendering and test-custody defects are tracked; integration is not release qualification.

Current product work is tracked in the [F4 release ledger](docs/in-progress/M2_MOC3_F4_RELEASE_LEDGER.md) and [public-input pipeline plan](docs/in-progress/M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md). The [static package admission checkpoint](docs/completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md) covers isolated PNG/Physics3 validation and drawable/atlas checks before GPU; physics is validated, not simulated in that slice. The subsequent [PUB-02-B bounded physics acceptance](docs/completed/audits/M2_MOC3_PUB02_B_ACCEPTANCE.md) adds opt-in isolated physics batches with local visible/API evidence. Persistent public control, full product publication and the shipping SDK remain HOLD.

[PUB-04-A](docs/completed/audits/M2_MOC3_PUB04_A_ACCEPTANCE.md) adds bounded
packet-only publication with actual core authorization after exact GPU query
completion. Rejected candidates retain the prior core/image/accepted physics
cursor. [PUB-04-B](docs/completed/audits/M2_MOC3_PUB04_B_ACCEPTANCE.md) extends
that transaction to actual particle/velocity/clock checkpoints: fresh isolated
workers restore the last accepted state and acknowledge its digest. This is a
bounded eight-step bridge; persistent control, full product publication and the
shipping SDK remain HOLD. These private diagnostics add no public capability.
