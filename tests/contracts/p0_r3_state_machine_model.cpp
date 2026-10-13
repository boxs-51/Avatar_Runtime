/*
 * DESIGN CONTRACT MODEL, NOT production Avatar Runtime code.
 * Exercises orderings required by the R3 security specification.
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
#include <thread>

enum class SourceState {
    active, rotating_old_revoked, rotate_failed, explicit_revoked, closed
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

struct ContractModel {
    // Required nesting: commit -> pose -> source. Source-only ingress is allowed.
    std::mutex commit_gate;
    std::mutex pose_gate;
    std::mutex router_gate;
    std::atomic<bool> running{true};
    std::atomic<uint64_t> security_now{100};
    Source source;
    uint64_t pose_generation = 0;
    uint64_t current_pose_epoch = 0;

    Rotation start_rotate() {
        std::scoped_lock commit(commit_gate);
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        if (!running.load() ||
            (source.state != SourceState::active &&
             source.state != SourceState::rotate_failed)) {
            return {};
        }
        if (source.epoch == UINT64_MAX ||
            source.control_epoch == UINT64_MAX ||
            source.ticket == UINT64_MAX) {
            return {};
        }
        ++source.epoch;
        ++source.control_epoch;
        ++source.ticket;
        source.active_cap = 0;
        source.state = SourceState::rotating_old_revoked;
        source.accepted_seq = 0;
        source.queue_count = 0;
        return {source.ticket, source.epoch, source.control_epoch, true};
    }

    bool finish_rotate(const Rotation &reservation, uint64_t cap, bool rng_ok = true) {
        // Model phase B token generation (no mutex held).
        std::scoped_lock commit(commit_gate);
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
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
            source.state == SourceState::explicit_revoked) return;
        assert(source.epoch != UINT64_MAX && source.control_epoch != UINT64_MAX);
        ++source.epoch;
        ++source.control_epoch;
        source.active_cap = 0;
        source.state = SourceState::explicit_revoked;
        source.queue_count = 0;
    }

    void admin_release() {
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        source.active_cap = 0;
        source.state = SourceState::closed;
        ++source.control_epoch;
        source.queue_count = 0;
    }

    void begin_close() {
        std::scoped_lock commit(commit_gate);
        {
            std::scoped_lock router(router_gate);
            running.store(false);
        }
        std::scoped_lock pose(pose_gate);
        std::scoped_lock ingress(source.ingress);
        source.active_cap = 0;
        source.state = SourceState::closed;
        source.queue_count = 0;
        ++source.control_epoch;
    }

    bool admit(uint64_t cap, uint64_t sequence) {
        std::scoped_lock ingress(source.ingress);
        const auto now = security_now.load();
        if (now < source.last_seen_security_ns) return false; // CLOCK_REGRESSION
        source.last_seen_security_ns = now;
        if (!running.load() || source.state != SourceState::active ||
            cap != source.active_cap || now >= source.expiry_ns ||
            sequence <= source.accepted_seq || source.queue_count >= 64) {
            return false;
        }
        ++source.queue_count;
        ++source.quota_spent;
        source.accepted_seq = sequence; // same critical section as append
        return true;
    }

    bool publish(uint64_t contributing_epoch,
                 const std::function<void()> &test_only_after_clock = {}) {
        // Test-only callback is used to demonstrate a scheduler pause after sample.
        // It is explicitly forbidden in the production publish critical section.
        std::scoped_lock commit(commit_gate);
        std::scoped_lock pose(pose_gate);
        const auto auth_sample = security_now.load();
        if (test_only_after_clock) test_only_after_clock();
        std::scoped_lock ingress(source.ingress);
        if (!running.load() || source.state != SourceState::active ||
            contributing_epoch != source.epoch || auth_sample >= source.expiry_ns) {
            return false;
        }
        current_pose_epoch = contributing_epoch;
        ++pose_generation;
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

int main() {
    race_l_revoke_wins();
    race_l_release_and_close();
    race_m_close_blocks_old_update();
    race_n_admission_clock_and_duplicate_sequence();
    race_o_expiry_before_final_check();
    race_p_sample_time_not_physical_erasure();
    provenance_revoke_cannot_republish();
    failclosed_rotate_rng();
    std::cout << "PASS ALL R3 CONTRACT MODEL TESTS (NOT production runtime)\n";
    return 0;
}
