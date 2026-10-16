#include "schema_worker_client.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace ars::validation {
WorkerResult run_schema_process(const WorkerConfig &config, const Message &header,
                                std::string_view input) noexcept {
    WorkerResult result;
    int sockets[2]{-1, -1};
    pid_t child = -1;
    struct Cleanup {
        int *sockets;
        pid_t *child;
        WorkerResult *result;
        ~Cleanup() {
            for (int i = 0; i < 2; ++i)
                if (sockets[i] >= 0)
                    close(sockets[i]);
            if (*child > 0) {
                kill(*child, SIGKILL);
                while (waitpid(*child, nullptr, 0) < 0 && errno == EINTR) {
                }
                result->reaped = true;
            }
        }
    } cleanup{sockets, &child, &result};
    // Return-value copy occurs before a local destructor: update reaped explicitly
    // before every return after spawn rather than relying on NRVO for evidence.
    auto reap = [&](bool terminate) {
        if (child > 0) {
            if (terminate)
                kill(child, SIGKILL);
            while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
            }
            child = -1;
            result.reaped = true;
        }
    };
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(config.deadline_ms);
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0)
        return result;
    // Normalize descriptors so dup2/close-from cannot accidentally close stdio.
    for (auto &fd : sockets)
        if (fd < 3) {
            int next = fcntl(fd, F_DUPFD_CLOEXEC, 3);
            if (next < 0)
                return result;
            close(fd);
            fd = next;
        }
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0)
        return result;
    int error = posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
    error |= posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO);
    error |= posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    error |= posix_spawn_file_actions_addclosefrom_np(&actions, 3);
    char *args[]{const_cast<char *>(config.executable.c_str()), nullptr};
    if (!error)
        error = posix_spawn(&child, config.executable.c_str(), &actions, nullptr, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error) {
        child = -1;
        return result;
    }
    result.process_id = static_cast<std::uint64_t>(child);
    close(sockets[1]);
    sockets[1] = -1;
    if (fcntl(sockets[0], F_SETFL, O_NONBLOCK) < 0) {
        reap(true);
        return result;
    }
    std::array<std::uint8_t, 33> reply{};
    std::size_t sent = 0, received = 0;
    bool shut = false, eof = false;
    const auto total = header.size() + input.size();
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= end) {
            result.status = ARS_E_OUT_OF_BUDGET;
            reap(true);
            return result;
        }
        if (sent == total && !shut) {
            shutdown(sockets[0], SHUT_WR);
            shut = true;
        }
        pollfd p{eof ? -1 : sockets[0], static_cast<short>((sent < total ? POLLOUT : 0) | POLLIN), 0};
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count();
        const int ready = poll(&p, 1, static_cast<int>(ms > 10 ? 10 : ms + 1));
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0) {
            reap(true);
            return result;
        }
        if (p.revents & POLLOUT) {
            const bool first = sent < header.size();
            const void *data = first ? static_cast<const void *>(header.data() + sent)
                                     : static_cast<const void *>(input.data() + sent - header.size());
            const auto size = first ? header.size() - sent : total - sent;
            const auto n = send(sockets[0], data, size, MSG_NOSIGNAL);
            if (n > 0)
                sent += static_cast<std::size_t>(n);
            else if (n < 0 && errno != EAGAIN && errno != EINTR) {
                reap(true);
                return result;
            }
        }
        if (p.revents & (POLLIN | POLLHUP | POLLERR)) {
            const auto n = recv(sockets[0], reply.data() + received, reply.size() - received, 0);
            if (n > 0) {
                received += static_cast<std::size_t>(n);
                if (received > 32) {
                    reap(true);
                    return result;
                }
            } else if (n == 0)
                eof = true;
            else if (errno != EAGAIN && errno != EINTR) {
                reap(true);
                return result;
            }
        }
        int status = 0;
        const auto done = waitpid(child, &status, WNOHANG);
        if (done == child) {
            child = -1;
            result.reaped = true;
            // A child may exit before the socket's final buffered reply was read.
            if (!eof) {
                while (received < reply.size()) {
                    const auto n = recv(sockets[0], reply.data() + received, reply.size() - received, 0);
                    if (n < 0 && errno == EINTR)
                        continue;
                    if (n <= 0) {
                        eof = n == 0;
                        break;
                    }
                    received += static_cast<std::size_t>(n);
                }
            }
            if (sent == total && received == 32 && eof && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                Message m{};
                std::copy_n(reply.begin(), 32, m.begin());
                if (valid_reply(m, header))
                    result.status = static_cast<ars_status>(get(m, 8));
            }
            return result;
        }
        if (done < 0 && errno != EINTR) {
            reap(true);
            return result;
        }
    }
}
} // namespace ars::validation
