# Avatar Runtime SDK

Embeddable real-time 2D/3D character-rendering SDK under development.

**Status:** CPU runtime/validation components and optional private D3D11/MOC3 diagnostic paths are implemented. The production SDK and public model/character/render API are not released. Private test acceptance does not close the product gates.

**Principles:** host-owned integration, logical character with optional 2D/3D representations, normalized motion input, secure user-model loading, capability-based renderer, C ABI, testable acceptance gates.

Start with the [documentation index and classification](docs/DOCUMENTATION_INDEX.md). It separates completed scopes, reference material, superseded plans and ongoing implementation, and identifies stale status text in historical documents.

Current product work is tracked in the [F4 release ledger](docs/in-progress/M2_MOC3_F4_RELEASE_LEDGER.md) and [public-input pipeline plan](docs/in-progress/M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md). The [static package admission checkpoint](docs/completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md) covers isolated PNG/Physics3 validation and drawable/atlas checks before GPU; physics is validated, not simulated in that slice. The subsequent [PUB-02-B bounded physics acceptance](docs/completed/audits/M2_MOC3_PUB02_B_ACCEPTANCE.md) adds opt-in isolated physics batches with local visible/API evidence. Persistent public control, final core publication and the shipping SDK remain HOLD.
