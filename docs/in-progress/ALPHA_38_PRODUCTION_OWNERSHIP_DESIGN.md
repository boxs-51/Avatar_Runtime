# Alpha #38: production context ownership and teardown

Design/implementation contract at alpha64c129eb1c8be3a4a12ac8b7fb807bfdbb0276eb,
2026-10-21. User continuation: #38 after #60. This is proposed implementation
structure within the authorized issue; it is not a public activation or gate PASS.

## Scope and inspected evidence

Issue38 and M2_E_HOST_HANDOFF_PLAN E1 are the remaining requirement; E0 receipt
and ALPHA_58_SESSION_LIFECYCLE_DESIGN inventory are historical private proof.
The current CPU profile implements asset/authority lifecycle but no public GPU
context owner. Existing actual native context domains are dual-mode backend,
packet/lease publication adapters, their readback paths, frame/query resources,
restore scopes and optional atlas residency. Diagnostic executables that create
separate devices are separate domains, not hidden members of a production graph.

H38-1 inventory/shared admission; H38-2 ordered reference surrender and retained
unknown-completion CPU/runtime pins; H38-3 negative receipt authority checks;
H38-4 healthy/injected WARP/hardware and actual cleanup; H38-5 independent
review, source custody, docs and integration. No destructive loss, ABI change,
public graphics capability, private asset publication or full G00 promotion.

## Chosen architecture and alternative

A ContextOwnership domain is canonical for one device/immediate-context and
nonzero generation on R. It holds a real core RuntimeLease supplied by the
owning adapter. A bounded registry rejects duplicate live domains for the same
context. Move-only participant tokens track reference owners; move-only operation
leases cover submissions, callbacks/readback and nested restore scopes. New root
work is rejected after close intent; only explicit R drain and nested scopes of
already admitted work may proceed. No ARS mutex is held across D3D or callbacks.
Registered external native users use the same leased entry, never a Boolean
quiescence assertion. Arbitrary unregistered native pointer use cannot be made
safe by this library and is outside the owned graph; public interop remains OFF.

Alternative: bolt another receipt onto the private host. Rejected because it
cannot enumerate adapter/cache/restore/readback references or separate GPU frame
ownership from CPU snapshots. A domain integrated into existing owners is a
larger patch but preserves private defaults and supports actual audited teardown.

Query retirement currently stores mixed CPU+GPU frame objects in one type-erased
pin. Split owned GPU resources from the CPU snapshot. Healthy completion retires
both at the exact query. Loss first surrenders GPU/query/context/RTV/cache refs
on R while retaining CPU snapshots, final-fence data and the real runtime pin.
Only a single-use domain/device/generation/R-bound receipt releases those CPU
pins. Production detach cannot take the legacy private bypass.

Dual-mode rendering optionally joins the domain; standalone private diagnostic
constructors retain existing behavior and oracles. PacketPublication and
LeasePublication join it using a real CoreConsumer lease. Their direct readback,
callbacks, close and host references are enrolled too. A cache participant stays
live while any cached GPU references exist. Restore scopes finish before receipt
issuance. Close from within an admitted callback is deferred/BUSY until unwind;
new work remains denied. Device replacement creates a new domain, cannot reopen
the old admission epoch and cannot consume old authority receipts.

Receipt issuance requires closing intent, no admitted operations and no tracked
GPU owners. It clears state and surrenders domain-owned context/device refs.
Receipt consumption additionally validates authority identity, generation,
device identity and R ownership, then releases quarantined CPU/runtime pins.
No query completion is fabricated. Missing/failed cleanup remains pending.

## Implementation and verification order

1. Implement bounded canonical domain, operation/participant tokens and receipt
   authority; prove negative admission/identity/thread/capacity/close cases.
2. Split query frame GPU/CPU owners; enroll backend, renderer frame and restore
   scope lifetimes; preserve legacy query/receipt tests. Add two-phase lost
   surrender/receipt consumption to the domain-aware dual host.
3. Enroll packet/lease adapters, actual core lease, direct readback and caches;
   close admission before cancel/drain, surrender references before receipt.
4. Run generated/native2D/GLB and permitted generated MOC3 data on WARP and named
   hardware, actual queries for healthy completion and injected errors only for
   loss. Exercise active callbacks, resize/close, stale generations, pending
   query pins, forged/early/replayed receipts and debug-layer live objects.
5. Review exact diff, reconcile inventory and evidence; assess G00 method from
   integrated observations, leave other G00 dependencies/physical loss explicit.
   Commit/push/alpha integration only on scoped proof. Rollback is the feature
   branch; default CPU SDK/ABI20 and original negative oracles remain preserved.

No whole issue closure from a domain unit test or from renaming a private host
production. Actual adapter graph coverage and runtime/COM cleanup must be observed.
