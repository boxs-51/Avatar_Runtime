// Deliberately separate, unsanitized shipping helper: ASAN's huge shadow address
// space conflicts with the Linux hard address-space cap. Parent/decoders are
// sanitized; the real worker always retains its OS cap, even in ASAN test runs.
#include "schema_literals.hpp"
#include "schema_worker_protocol.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#endif
using namespace ars::validation;
int main() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    // Parent assigns a suspended child to the memory/active-process Job before
    // resuming any code. Refuse direct execution without an enclosing Job.
    BOOL assigned = FALSE;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &assigned) || !assigned)
        return 2;
#else
    const auto parent = getppid();
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != parent || parent == 1)
        return 2;
    rlimit memory{worker_memory, worker_memory}, cpu{5, 5}, core{0, 0};
    if (setrlimit(RLIMIT_AS, &memory) || setrlimit(RLIMIT_CPU, &cpu) || setrlimit(RLIMIT_CORE, &core))
        return 2;
#endif
    Message request{};
    if (std::fread(request.data(), 1, request.size(), stdin) != request.size() || !valid_header(request))
        return 2;
    const auto kind = get(request, 8), length = get(request, 12);
    if ((kind != 1 && kind != 2) || length == 0 || length > max_request)
        return 2;
    auto status = ARS_E_INTERNAL;
    try {
        std::string bytes(length, '\0');
        if (std::fread(bytes.data(), 1, length, stdin) != length || std::fgetc(stdin) != EOF ||
            checksum(bytes) != get(request, 24))
            return 2;
#ifdef ARS_SCHEMA_RESOURCE_PROBE
        // Dedicated native test executable only; absent from shipping objects.
        // The production OS limit setup above remains identical in this build.
        if (bytes == "\"resource_probe\"") {
            auto *p = std::malloc(worker_memory * 2);
            status = p ? ARS_E_INTERNAL : ARS_E_OUT_OF_BUDGET;
            if (p) {
                std::memset(p, 1, worker_memory * 2);
                std::free(p);
            }
        } else
#endif
        {
            const auto options = jsoncons::json_options{}.max_nesting_depth(64);
            const auto instance = jsoncons::json::parse(bytes, options);
            // Schemas are generated from frozen local repo files at build time.
            // No resolver/network/file callback is installed and no asset schema is accepted.
            auto schema = jsoncons::jsonschema::make_json_schema(
                jsoncons::json::parse(kind == 1 ? manifest_schema : rig_schema));
            status = schema.is_valid(instance) ? ARS_OK : ARS_E_BAD_ASSET;
        }
    } catch (const std::bad_alloc &) {
        status = ARS_E_OUT_OF_BUDGET;
    } catch (const jsoncons::ser_error &) {
        status = ARS_E_BAD_ASSET;
    } catch (...) {
        status = ARS_E_INTERNAL;
    }
    Message reply = request;
    put(reply, 8, static_cast<std::uint32_t>(status));
    put(reply, 24, 0);
    return std::fwrite(reply.data(), 1, reply.size(), stdout) == reply.size() && std::fflush(stdout) == 0 ? 0
                                                                                                          : 2;
}
