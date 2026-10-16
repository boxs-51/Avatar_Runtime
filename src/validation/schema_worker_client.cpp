#include "schema_worker_client.hpp"
namespace ars::validation {
ars_status SchemaClient::validate(Schema,std::string_view,assets::Resource&,WorkerResult*) noexcept {
    return ARS_E_UNSUPPORTED; // Native RED: bounded schema process not implemented yet.
}
}
