/*
 * G5-ING-01 / G5-ACK-02 — opt-in RED-first contract, not runtime-approved.
 *
 * Expected to fail at COMPILE against G5@f41f9a3: the trusted-actor entry
 * points and operation/payload-bound receipt fields do not exist yet.
 *
 * Build deliberately opt-in, outside default CMake/CTest until contract review.
 * No model assets, secrets, GUI actions, IPC transport or production writer.
 */
#include "../studio_g5_bus.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define REQUIRE(expr) do { \
 if (!(expr)) { fprintf(stderr,"G5_TRUST_RED line=%d\n",__LINE__);return 1; } \
} while (0)

static G5BusCommand command(G5Ingress ingress,G5BusOp op,uint64_t rev){
 G5BusCommand c={0};
 c.ingress=ingress;c.operation=op;c.session=99;c.generation=55;
 c.expected_revision=rev;
 return c;
}

static int nonzero_digest(const unsigned char *p,size_t n){
 unsigned accumulator=0;
 for(size_t i=0;i<n;i++)accumulator|=p[i];
 return accumulator!=0;
}

int main(void){
 StudioRig rig={0};G5Bus bus={0};G5BusResult answer={0};G5Receipt receipt={0};
 REQUIRE(studio_rig_init(&rig,0,42,10));
 REQUIRE(g5_bus_init(&bus,&rig,99,55,7)==G5_OK);

 /* The Host, not JSON, binds these independent actors to the same ingress. */
 const G5TrustedActor alice={G5_AGENT,UINT64_C(101)};
 const G5TrustedActor bob={G5_AGENT,UINT64_C(202)};

 G5BusCommand a=command(G5_AGENT,G5_BEGIN,7);
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&a,&answer)==G5_OK);
 const uint64_t alice_draft=answer.draft_id;
 REQUIRE(alice_draft!=0);

 /* The second Agent may share ingress, but never owns Alice's draft. */
 G5BusCommand b=command(G5_AGENT,G5_ANCHOR,7);
 b.draft_id=alice_draft;b.vertex=3;b.enabled=1;
 REQUIRE(g5_bus_submit_trusted(&bus,&bob,&b,&answer)==G5_BUSY);
 REQUIRE(bus.txn.draft.anchor_count==0);

 b=command(G5_AGENT,G5_BEGIN,7);
 REQUIRE(g5_bus_submit_trusted(&bus,&bob,&b,&answer)==G5_BUSY);

 /* Caller-controlled wire provenance must not promote Agent to UI. */
 b=command(G5_UI,G5_ABORT,7);b.draft_id=alice_draft;
 REQUIRE(g5_bus_submit_trusted(&bus,&bob,&b,&answer)==G5_STALE_SESSION);
 REQUIRE(bus.txn.has_draft==1);

 a=command(G5_AGENT,G5_ANCHOR,7);
 a.draft_id=alice_draft;a.vertex=3;a.enabled=1;
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&a,&answer)==G5_OK);
 a.operation=G5_VALIDATE;
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&a,&answer)==G5_OK);
 a.operation=G5_COMMIT;a.request_id=700;
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&a,&answer)==G5_OK);
 REQUIRE(answer.document_revision==8);
 REQUIRE(bus.txn.live.anchor_count==1);

 /* Timeout is post-effect: look up immutable owner + operation + payload. */
 REQUIRE(g5_bus_mark_unknown_trusted(&bus,&alice,700)==G5_OK);
 REQUIRE(g5_bus_recover_trusted(&bus,&bob,99,700,&receipt)==G5_STALE_SESSION);
 REQUIRE(g5_bus_recover_trusted(&bus,&alice,99,700,&receipt)==G5_OK);
 REQUIRE(receipt.outcome==G5_COMMITTED);
 REQUIRE(receipt.revision==8);
 REQUIRE(receipt.operation==G5_COMMIT);
 REQUIRE(nonzero_digest(receipt.payload_sha256,sizeof receipt.payload_sha256));
 /* Independent SHA-256 vector: canonical v1 COMMIT {ingress=3, actor=101,
    session=99, generation=55, base=7, draft=1, request=700}; ignores stale
    vertex/enabled fields left from earlier staged anchor operation. */
 static const unsigned char expected_digest[32]={
  0x37,0x7c,0x15,0xb6,0x9c,0x76,0xf4,0x8c,
  0xed,0xb2,0x7a,0xd0,0x38,0xeb,0x33,0xb8,
  0xbf,0x46,0x70,0x28,0xad,0x42,0x79,0x28,
  0xa9,0x6a,0x2d,0x75,0x77,0xce,0x83,0x3d
 };
 REQUIRE(memcmp(receipt.payload_sha256,expected_digest,32)==0);


 /* Recovery must not be a mutation or unquarantine from a bare read. */
 a=command(G5_AGENT,G5_BEGIN,8);
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&a,&answer)==G5_BUSY);
 REQUIRE(g5_bus_reconcile_trusted(&bus,&bob,99,700)==G5_STALE_SESSION);
 /* A's quarantine must not block B's distinct session identity.
    B may obtain a draft before A reconciles, but cannot clear A's marker. */
 b=command(G5_AGENT,G5_BEGIN,8);
 REQUIRE(g5_bus_submit_trusted(&bus,&bob,&b,&answer)==G5_OK);
 const uint64_t temporary_bob_draft=answer.draft_id;
 REQUIRE(temporary_bob_draft!=0);
 b=command(G5_AGENT,G5_ABORT,8);b.draft_id=temporary_bob_draft;
 REQUIRE(g5_bus_submit_trusted(&bus,&bob,&b,&answer)==G5_OK);
 REQUIRE(g5_bus_reconcile_trusted(&bus,&alice,99,700)==G5_OK);

 /* The second Agent has no quarantine; can acquire a free lease independently. */
 b=command(G5_AGENT,G5_BEGIN,8);
 REQUIRE(g5_bus_submit_trusted(&bus,&bob,&b,&answer)==G5_OK);
 const uint64_t bob_draft=answer.draft_id;
 REQUIRE(bob_draft!=0);
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&a,&answer)==G5_BUSY);
 b=command(G5_AGENT,G5_ABORT,8);b.draft_id=bob_draft;
 REQUIRE(g5_bus_submit_trusted(&bus,&bob,&b,&answer)==G5_OK);

 /* Unknown request never grants replay or clears its own quarantine. */
 REQUIRE(g5_bus_mark_unknown_trusted(&bus,&alice,999)==G5_OK);
 REQUIRE(g5_bus_recover_trusted(&bus,&alice,99,999,&receipt)==G5_UNKNOWN);
 REQUIRE(g5_bus_reconcile_trusted(&bus,&alice,99,999)==G5_UNKNOWN);
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&a,&answer)==G5_BUSY);
 REQUIRE(bus.txn.revision==8);
 puts("G5_ACTOR_RECEIPT_PASS distinct_owner=1 no_replay=1");
 return 0;
}
