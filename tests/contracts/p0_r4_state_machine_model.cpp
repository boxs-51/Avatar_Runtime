/*
 * R5 CONTRACT MODEL, NOT production Avatar Runtime code.
 * Exercises owner R5 close-fairness, source-expiry provenance and overflow design.
 * This model deliberately does not implement public C ABI, GPU or native registry.
 */
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <latch>
#include <limits>
#include <mutex>
#include <condition_variable>
#include <array>
#include <vector>
#include <optional>
#include <thread>

enum class SourceState {
    active, rotating_old_revoked, rotate_failed,
    explicit_revoked, expired, exhausted, closed
};

struct Rotation {
    uint64_t ticket = 0;
    uint64_t epoch = 0;
    uint64_t control_epoch = 0;
    bool valid = false;
};

struct Source {
    std::mutex ingress;
    SourceState state = SourceState::active;
    uint64_t epoch = 1;
    uint64_t control_epoch = 1;
    uint64_t ticket = 0;
    uint64_t active_cap = 111;
    uint64_t accepted_seq = 0;
    uint64_t last_seen_security_ns = 0;
    uint64_t expiry_ns = 200;
    uint32_t quota_spent = 0;
    uint32_t queue_count = 0;
};


/*
 * Writer-priority admission barrier. A closing writer changes OPEN->CLOSING
 * under this mutex BEFORE waiting. No newly-arriving ingress can increase
 * reader count after that transition, even on reader-favoring platforms.
 * No source, clock, pose or router lock is taken while gate mutex is held.
 */
class AdmissionGate {
    std::mutex state_mutex_;
    std::condition_variable drained_;
    unsigned readers_ = 0;
    bool closing_ = false;
public:
    bool enter() {
        std::lock_guard lock(state_mutex_);
        if (closing_) return false;
        ++readers_;
        return true;
    }
    void leave() {
        std::lock_guard lock(state_mutex_);
        assert(readers_ > 0);
        --readers_;
        if (readers_ == 0) drained_.notify_all();
    }
    void close_and_wait(std::atomic<bool>& visible_closing_intent) {
        std::unique_lock lock(state_mutex_);
        closing_ = true;
        visible_closing_intent.store(true, std::memory_order_release);
        visible_closing_intent.notify_all();  // deterministic test-only observer
        drained_.wait(lock, [&]{ return readers_ == 0; }); // releases mutex
    }
    bool is_closing() {
        std::lock_guard lock(state_mutex_);
        return closing_;
    }
    unsigned active_readers() {
        std::lock_guard lock(state_mutex_);
        return readers_;
    }
};
struct AdmissionLease {
    AdmissionGate& gate;
    const bool acquired;
    explicit AdmissionLease(AdmissionGate& g): gate(g), acquired(gate.enter()) {}
    ~AdmissionLease() { if (acquired) gate.leave(); }
    AdmissionLease(const AdmissionLease&) = delete;
    AdmissionLease& operator=(const AdmissionLease&) = delete;
};

struct ContractModel {
    // Lock paths:
    //   publish: commit -> pose -> source -> security_clock
    //   admit: bounded admission reader lease -> source -> security_clock
    //   begin_close: commit -> close_and_drain admission readers -> router -> pose -> source
    //   revoke: pose -> source
    std::mutex commit_gate;
    AdmissionGate admission_gate;
    std::mutex pose_gate;
    std::mutex router_gate;
    std::mutex security_clock_gate;
    std::atomic<bool> running{true};
    std::atomic<bool> closing_intent{false};
    std::atomic<uint64_t> security_now{100};
    // Protected by security_clock_gate, not by a caller-provided clock callback:
    uint64_t security_highwater_ns = 0;
    bool security_clock_poisoned = false;
    Source source;
    uint64_t pose_generation = 0;
    uint64_t current_pose_epoch = 0;

    // Call ONLY with security_clock_gate held; guarantee global ordering
    // across both ingress and publish in this runtime.
    std::optional<uint64_t> sample_security_locked() {
        if (security_clock_poisoned) return std::nullopt;
        const auto now = security_now.load(std::memory_order_relaxed);
        if (now < security_highwater_ns) {
            security_clock_poisoned = true; // sticky until runtime destruction
            return std::nullopt;
        }
        security_highwater_ns = now;
        return now;
    }

    // Requires source ingress lock, and (for dirty handoff) release ordering.
    // Exhaustion is terminal; it never wraps and never needs a successful ++
    // to invalidate an existing token.
    void exhaust_source_locked() {
        source.active_cap = 0;
        source.queue_count = 0;
        source.state = SourceState::exhausted;
    }
    bool advance_epoch_locked() {
        if (source.epoch == UINT64_MAX ||
            source.control_epoch == UINT64_MAX) {
            exhaust_source_locked();
            return false;
        }
        ++source.epoch;
        ++source.control_epoch;
        return true;
    }

    // Call with source ingress mutex; never increments epoch modulo 2^64.
    void expire_locked() {
        if (source.state == SourceState::expired ||
            source.state == SourceState::explicit_revoked ||
            source.state == SourceState::exhausted ||
            source.state == SourceState::closed) return;
        if (!advance_epoch_locked()) return; // typed HANDLE_EXHAUSTED in real ABI
        source.active_cap = 0;
        source.queue_count = 0;
        source.state = SourceState::expired; // terminal for this registration
    }

    Rotation start_rotate() {
        std::scoped_lock commit(commit_gate);
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        std::scoped_lock clock(security_clock_gate);
        const auto now = sample_security_locked();
        if (!now) return {};
        if (*now >= source.expiry_ns) {
            expire_locked(); // may expire ACTIVE or ROTATE_FAILED
            return {};
        }
        if (!running.load() ||
            (source.state != SourceState::active &&
             source.state != SourceState::rotate_failed)) {
            return {};
        }
        if (source.ticket == UINT64_MAX) {
            exhaust_source_locked();
            return {};
        }
        if (!advance_epoch_locked()) return {};
        ++source.ticket;
        source.active_cap = 0;
        source.state = SourceState::rotating_old_revoked;
        source.accepted_seq = 0;
        source.queue_count = 0;
        return {source.ticket, source.epoch, source.control_epoch, true};
    }

    bool finish_rotate(const Rotation &reservation, uint64_t cap, bool rng_ok = true) {
        // Model phase B token mint, not actual CSPRNG or nonreuse ledger.
        std::scoped_lock commit(commit_gate);
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        std::scoped_lock clock(security_clock_gate);
        const auto now = sample_security_locked();
        if (!now) return false;
        if (*now >= source.expiry_ns) {
            expire_locked(); // phase-B expiry defeats phase-C reactivation
            return false;
        }
        const bool matches = reservation.valid && running.load() &&
            source.state == SourceState::rotating_old_revoked &&
            reservation.ticket == source.ticket &&
            reservation.epoch == source.epoch &&
            reservation.control_epoch == source.control_epoch;
        if (!matches) return false;
        if (!rng_ok || cap == 0 || cap == 111) {
            source.state = SourceState::rotate_failed;
            source.active_cap = 0;
            return false;
        }
        source.active_cap = cap;
        source.state = SourceState::active;
        return true;
    }

    void revoke() {
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        if (source.state == SourceState::closed ||
            source.state == SourceState::explicit_revoked ||
            source.state == SourceState::expired) return;
        if (!advance_epoch_locked()) return;
        source.active_cap = 0;
        source.state = SourceState::explicit_revoked;
        source.queue_count = 0;
    }

    void admin_release() {
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        source.active_cap = 0;
        source.state = SourceState::closed;
        if (source.control_epoch != UINT64_MAX) ++source.control_epoch;
        source.queue_count = 0;
    }

    void begin_close() {
        std::scoped_lock commit(commit_gate);
        // Writer-intent prevents a continuing stream of new entrants from
        // starving close while it waits for prior admission readers.
        admission_gate.close_and_wait(closing_intent); // writer preference + bounded readers
        {
            std::scoped_lock router(router_gate);
            running.store(false); // begin_close linearization
        }
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        source.active_cap = 0;
        source.state = SourceState::closed;
        source.queue_count = 0;
        if (source.control_epoch != UINT64_MAX) ++source.control_epoch;
    }

    bool admit(uint64_t cap, uint64_t sequence,
               const std::function<void()> &test_only_before_append = {}) {
        // Test-only callback is forbidden in production admission critical path.
        AdmissionLease lease(admission_gate);
        if (!lease.acquired || !running.load()) return false;
        std::scoped_lock ingress(source.ingress);
        std::scoped_lock clock(security_clock_gate);
        const auto now = sample_security_locked();
        if (!now) return false; // runtime-wide sticky clock fault
        source.last_seen_security_ns = *now;
        if (source.state == SourceState::active && *now >= source.expiry_ns)
            expire_locked(); // irrevocable epoch/cap invalidation
        if (!running.load() || source.state != SourceState::active ||
            cap != source.active_cap || sequence <= source.accepted_seq ||
            source.queue_count >= 64) {
            return false;
        }
        if (test_only_before_append) test_only_before_append();
        // Admission gate cannot be exclusively acquired until this append
        // finishes, so this OK necessarily precedes begin_close linearization.
        ++source.queue_count;
        ++source.quota_spent;
        source.accepted_seq = sequence;
        return true;
    }

    bool publish(uint64_t contributing_epoch,
                 const std::function<void()> &test_only_after_clock = {}) {
        // Test-only callback documents authorization at sample time, NOT
        // physically instantaneous TTL erasure. Prohibited in production.
        std::scoped_lock commit(commit_gate);
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        std::scoped_lock clock(security_clock_gate);
        const auto sampled = sample_security_locked();
        if (!sampled || !running.load()) return false;
        if (test_only_after_clock) test_only_after_clock();
        if (source.state == SourceState::active && *sampled >= source.expiry_ns)
            expire_locked(); // no later rollback can revive this epoch
        if (source.state != SourceState::active ||
            contributing_epoch != source.epoch) return false;
        current_pose_epoch = contributing_epoch;
        ++pose_generation; // modelled immutable PoseSnapshot swap
        return true;
    }
};

static void race_l_revoke_wins() {
    ContractModel m;
    std::latch phase_a(1), phase_c(1);
    bool activated = true;
    std::thread rotator([&] {
        const auto reservation = m.start_rotate();
        assert(reservation.valid);
        phase_a.count_down();
        phase_c.wait();
        activated = m.finish_rotate(reservation, 222);
    });
    phase_a.wait();
    m.revoke();
    phase_c.count_down();
    rotator.join();
    assert(!activated && !m.admit(222, 1));
    assert(m.source.state == SourceState::explicit_revoked);
    assert(!m.start_rotate().valid); // explicit revoke is terminal
    std::cout << "PASS RACE-L: pending rotate cannot undo explicit revoke\n";
}

static void race_l_release_and_close() {
    {
        ContractModel m;
        auto ticket = m.start_rotate();
        m.admin_release();
        assert(!m.finish_rotate(ticket, 222));
        assert(m.source.state == SourceState::closed);
    }
    {
        ContractModel m;
        auto ticket = m.start_rotate();
        m.begin_close();
        assert(!m.finish_rotate(ticket, 222));
        assert(!m.admit(222, 1));
    }
    {
        ContractModel m;
        auto ticket = m.start_rotate();
        assert(m.finish_rotate(ticket, 222)); // rotate completes first
        m.revoke(); // later revoke invalidates new token
        assert(!m.admit(222, 1));
    }
    std::cout << "PASS RACE-L variants: admin release / begin_close / revoke after rotate\n";
}

static void race_m_close_blocks_old_update() {
    ContractModel m;
    std::latch update_ready(1), release_update(1);
    bool published = true;
    std::thread simulation([&] {
        const auto staged_epoch = m.source.epoch; // precomputed and pinned outside commit gate
        update_ready.count_down();
        release_update.wait();
        published = m.publish(staged_epoch);
    });
    update_ready.wait();
    m.begin_close();
    release_update.count_down();
    simulation.join();
    assert(!published && m.pose_generation == 0);
    std::cout << "PASS RACE-M: begin_close fences precomputed pose\n";
}

static void race_n_admission_clock_and_duplicate_sequence() {
    ContractModel m;
    std::latch ready(2), start(1);
    std::atomic<int> success{0};
    auto worker = [&] {
        ready.count_down();
        start.wait();
        if (m.admit(111, 100)) ++success;
    };
    std::thread a(worker), b(worker);
    ready.wait();
    start.count_down();
    a.join();
    b.join();
    assert(success == 1 && m.source.accepted_seq == 100);
    assert(m.source.queue_count == 1 && m.source.quota_spent == 1);
    m.security_now.store(110);
    assert(m.admit(111, 101));
    m.security_now.store(105); // simulated broken OS clock
    assert(!m.admit(111, 102)); // must not backdate
    assert(m.source.accepted_seq == 101);
    std::cout << "PASS RACE-N/G: seq atomicity and clock regression denied\n";
}

static void race_o_expiry_before_final_check() {
    ContractModel m;
    std::latch staged(1), proceed(1);
    bool published = true;
    std::thread simulation([&] {
        const auto epoch = m.source.epoch;
        staged.count_down();
        proceed.wait();
        published = m.publish(epoch);
    });
    staged.wait();
    m.security_now.store(200); // exclusive end of authorization lease
    proceed.count_down();
    simulation.join();
    assert(!published && m.pose_generation == 0);
    std::cout << "PASS RACE-O: expired at final authorization sample\n";
}

static void race_p_sample_time_not_physical_erasure() {
    ContractModel m;
    const bool published = m.publish(m.source.epoch, [&] {
        m.security_now.store(250); // test-only simulated preemption after sample
    });
    assert(published && m.pose_generation == 1);
    assert(!m.publish(m.source.epoch)); // next sample sees expiry
    std::cout << "PASS RACE-P: deliberately scoped sample-time TTL semantics\n";
}

static void provenance_revoke_cannot_republish() {
    ContractModel m;
    const auto epoch = m.source.epoch;
    assert(m.publish(epoch));
    m.revoke();
    assert(!m.publish(epoch));
    assert(m.pose_generation == 1);
    std::cout << "PASS contract provenance epoch invalidation\n";
}

static void failclosed_rotate_rng() {
    ContractModel m;
    auto ticket = m.start_rotate();
    assert(!m.finish_rotate(ticket, 0, false));
    assert(m.source.state == SourceState::rotate_failed);
    assert(!m.admit(111, 1));
    auto retry = m.start_rotate();
    assert(retry.valid);
    assert(m.finish_rotate(retry, 333));
    assert(m.admit(333, 1));
    std::cout << "PASS failed rotation cannot re-enable old cap\n";
}

static void race_q_irrevocable_expiry_and_clock_fault() {
    {
        ContractModel m;
        m.security_now.store(150);
        const auto old_epoch = m.source.epoch;
        assert(m.publish(old_epoch));
        m.security_now.store(200);
        assert(!m.publish(old_epoch)); // causes terminal source expiration
        assert(m.source.state == SourceState::expired);
        assert(m.source.epoch != old_epoch && m.source.active_cap == 0);
        const auto after_expiry_generation = m.pose_generation;
        m.security_now.store(120); // injected clock rollback
        assert(!m.publish(old_epoch) && !m.admit(111, 2));
        assert(m.security_clock_poisoned);
        assert(m.pose_generation == after_expiry_generation);
        assert(m.source.state == SourceState::expired);
        assert(!m.start_rotate().valid);
    }
    {
        ContractModel m;
        m.security_now.store(150);
        assert(m.publish(m.source.epoch));
        m.security_now.store(120); // regression even BEFORE expiry
        assert(!m.publish(m.source.epoch));
        assert(m.security_clock_poisoned);
        m.security_now.store(160);
        assert(!m.publish(m.source.epoch));
        assert(!m.admit(111, 1)); // no automatic recovery
        assert(m.pose_generation == 1);
    }
    std::cout << "PASS RACE-Q: terminal expiry and sticky clock-regression denial\n";
}

static void race_q_pending_rotate_denied_after_clock_fault() {
    ContractModel m;
    auto pending = m.start_rotate();
    assert(pending.valid);
    m.security_now.store(170);
    // A publish sample from any source is sufficient to register high-water.
    assert(!m.publish(pending.epoch)); // rotating state; records highwater
    m.security_now.store(120);
    assert(!m.publish(pending.epoch)); // poison during in-flight mint
    assert(m.security_clock_poisoned);
    assert(!m.finish_rotate(pending, 222)); // cannot grant fresh token
    assert(!m.start_rotate().valid);
    std::cout << "PASS RACE-Q clock fault denies pending token activation\n";
}

static void race_u_rotation_mint_crosses_expiry() {
    ContractModel m;
    m.security_now.store(150);
    auto pending = m.start_rotate();
    assert(pending.valid);
    m.security_now.store(200); // source expires while Phase B mints cap
    assert(!m.finish_rotate(pending, 222));
    assert(m.source.state == SourceState::expired);
    assert(m.source.active_cap == 0);
    assert(!m.start_rotate().valid); // terminal expired registration
    assert(!m.admit(222, 1));
    std::cout << "PASS RACE-U: rotation phase C cannot reactivate expired lease\n";
}

static void race_r_admission_vs_close_commit_barrier() {
    ContractModel m;
    std::latch admission_inside(1), resume_admission(1), close_invoked(1);
    bool accepted = false;
    std::thread submitter([&] {
        accepted = m.admit(111, 17, [&] {
            admission_inside.count_down();
            resume_admission.wait(); // holds admission shared, source, clock
        });
    });
    admission_inside.wait();
    std::thread closer([&] {
        close_invoked.count_down();
        m.begin_close(); // must acquire exclusive admission_gate
    });
    close_invoked.wait();
    m.closing_intent.wait(false); // ensures closer announced intent, not just scheduled
    assert(m.closing_intent.load());
    assert(m.running.load()); // H cannot linearize close while shared is held
    resume_admission.count_down();
    submitter.join();
    closer.join();
    assert(accepted && !m.running.load()); // accepted BEFORE close
    assert(m.source.accepted_seq == 17 && m.source.queue_count == 0);
    assert(!m.admit(111, 18)); // NO append after close
    ContractModel close_first;
    close_first.begin_close();
    assert(!close_first.admit(111, 1)); // reverse order
    std::cout << "PASS RACE-R: admission commits before close or is denied\n";
}

static void race_s_global_clock_watermark_shared_by_ingress_publish() {
    ContractModel m;
    m.security_now.store(180);
    assert(m.admit(111, 1)); // ingress samples 180
    assert(m.security_highwater_ns == 180);
    m.security_now.store(170);
    assert(!m.publish(m.source.epoch)); // publish observes regression
    assert(m.security_clock_poisoned);
    m.security_now.store(190);
    assert(!m.admit(111, 2) && !m.publish(m.source.epoch));
    std::cout << "PASS RACE-S: ingress/publish share sticky clock watermark\n";
}

int main() {
    race_l_revoke_wins();
    race_l_release_and_close();
    race_m_close_blocks_old_update();
    race_n_admission_clock_and_duplicate_sequence();
    race_o_expiry_before_final_check();
    race_p_sample_time_not_physical_erasure();
    provenance_revoke_cannot_republish();
    failclosed_rotate_rng();
    race_q_irrevocable_expiry_and_clock_fault();
    race_q_pending_rotate_denied_after_clock_fault();
    race_u_rotation_mint_crosses_expiry();
    race_r_admission_vs_close_commit_barrier();
    race_s_global_clock_watermark_shared_by_ingress_publish();
    std::cout << "PASS ALL R4 CONTRACT MODEL TESTS (NOT production runtime)\n";
    return 0;
}
