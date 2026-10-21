"""Frozen alpha integration custody, not permission for descendant source changes."""
import json
import check_m0_rc1_pins as pins

MAIN = "8723ef174a7db98b05fd031aa39dbd910e5ba2db"
INTEGRATION = "31ce916a72a08af27b3d87daa793b5e43cb50394"
MOC3_BASE = "ec30b6c6b305f8d495864e3a33818d07106a1abd"
SOURCE_HEADS = (
    "ef28b2f7d12c3b993a4a2fbc80eca8426ff68fc3",
    "59f4a08006733c8e4fe61c4f899d77c150656b1c",
    "58425b2ff1684f2610fc7da146f2877760520cb7",
    "bfe1bd617863065ea4b591a658d65cac46b3dedc",
    "6b64a844d530eafb5ca27e7e6028f082ef2688d2",
    "ee0530386c91b6157e14b4739a798a4d8b475db8",
    "1bbdbd1d6fc2a93bb264f3dea1af3976f4b243c1",
    "366fe6abfe98e0b74b15b98f1c10db70611405f4",
    MOC3_BASE,
)
RECEIPT_DELTA = frozenset((
    "CMakeLists.txt", "docs/M2_E_HOST_HANDOFF_PLAN.md",
    "src/graphics/d3d11/close_coordinator.cpp", "src/graphics/d3d11/close_coordinator.hpp",
    "src/graphics/d3d11/teardown_receipt.cpp", "src/graphics/d3d11/teardown_receipt.hpp",
    "tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m2_d_impl_custody.py",
    "tests/contracts/test_m2_e_custody.py", "tests/gpu/d3d11_close_protocol.cpp",
))
HELD_PATHS = (
    "include/ars/ars.h", "src/abi/runtime_api.cpp", "src/abi/unsupported.cpp",
    "docs/in-progress/audits/M1_B_CASE_EXECUTION.json",
    "docs/in-progress/M1_D_GPU_CASE_EXECUTION.json",
)

# Reviewed issue scopes only. This is not an alpha/* wildcard or approval source.
PR_SCOPES = {
    "dev/alpha-60-cpu-lifecycle-complete": (
        "5d90997e3f1e6df0ed5bef2b77318f815aa17769",
        frozenset((
            'CMakeLists.txt',
            'cmake/AvatarRuntimeConfig.cmake.in',
            'cmake/AvatarRuntimeCpuHelpers.cmake',
            'cmake/ars_runtime_sdk.cmake',
            'docs/DOCUMENTATION_CATALOG.json',
            'docs/DOCUMENTATION_INDEX.md',
            'docs/completed/audits/ALPHA_60_PUBLIC_CPU_ACCEPTANCE.md',
            'docs/in-progress/ALPHA_60_COMPLETION_PROGRESS.md',
            'docs/in-progress/ALPHA_60_PUBLIC_CPU_COMPLETION_DESIGN.md',
            'docs/in-progress/ALPHA_LINEAR_BACKLOG.md',
            'docs/reference/SDK_DEVELOPMENT_PACKAGE.md',
            'scripts/verify_cpu_full_transfer.py',
            'scripts/verify_cpu_helper_faults.py',
            'scripts/verify_public_cpu_sdk.py',
            'src/abi/cpu_api.cpp',
            'src/assets/cpu_admission_worker.cpp',
            'src/assets/cpu_model_packet.cpp',
            'src/assets/cpu_model_packet.hpp',
            'src/assets/glb_reader.hpp',
            'src/assets/isolated_cpu_load.hpp',
            'src/assets/isolated_cpu_load_windows.cpp',
            'src/assets/typed_glb_packet.cpp',
            'src/assets/typed_glb_packet.hpp',
            'src/assets/validated_cpu_model.cpp',
            'src/assets/validated_cpu_model.hpp',
            'src/core/character_authority.hpp',
            'src/core/cpu_context.hpp',
            'src/core/runtime_cpu.cpp',
            'src/core/runtime_lifecycle.cpp',
            'src/core/runtime_lifecycle.hpp',
            'src/core/runtime_state.hpp',
            'src/moc3/import_sandbox.cpp',
            'src/motion/provenance.hpp',
            'src/motion/public_resolution.cpp',
            'src/motion/public_resolution.hpp',
            'src/motion/publish_gate.cpp',
            'src/motion/publish_gate.hpp',
            'src/motion/semantic_candidate.cpp',
            'src/motion/semantic_candidate.hpp',
            'src/security/authority_types.hpp',
            'src/security/source_authority.cpp',
            'src/security/source_authority.hpp',
            'src/validation/glb_validator.cpp',
            'src/validation/glb_validator.hpp',
            'src/validation/strict_json.cpp',
            'src/validation/strict_value.cpp',
            'tests/abi/cpu_profile_consumer.c',
            'tests/assets/cpu_custody_tests.cpp',
            'tests/assets/cpu_fault_worker.cpp',
            'tests/assets/cpu_model_packet_guard.cpp',
            'tests/assets/cpu_model_packet_reference.cpp',
            'tests/assets/isolated_cpu_load_tests.cpp',
            'tests/contracts/check_alpha_custody.py',
            'tests/security/public_cpu_api_tests.cpp',
            'tests/security/public_resolution_tests.cpp',
        ))),
    "dev/alpha-60-typed-2d-transfer": (
        "fa9a1a02afc8cf75c1867f58aff9f6e70fd0bdbb",
        frozenset((
            'CMakeLists.txt',
            'docs/DOCUMENTATION_CATALOG.json',
            'docs/DOCUMENTATION_INDEX.md',
            'docs/completed/audits/ALPHA_60_TYPED_2D_TRANSFER_ACCEPTANCE.md',
            'docs/in-progress/ALPHA_60_ISOLATED_ADMISSION_DESIGN.md',
            'docs/in-progress/ALPHA_60_TYPED_2D_TRANSFER_DESIGN.md',
            'docs/in-progress/ALPHA_LINEAR_BACKLOG.md',
            'scripts/verify_cpu_model_transfer.py',
            'src/assets/cpu_admission_worker.cpp',
            'src/assets/cpu_model_packet.cpp',
            'src/assets/cpu_model_packet.hpp',
            'src/assets/validated_cpu_model.hpp',
            'src/moc3/import_sandbox.cpp',
            'tests/assets/cpu_model_packet_guard.cpp',
            'tests/assets/cpu_model_packet_reference.cpp',
            'tests/contracts/check_alpha_custody.py',
        ))),
    "dev/alpha-60-isolated-admission": (
        "49eb54a88cfc30e1104bf12750852827be36aa87",
        frozenset((
            'CMakeLists.txt',
            'docs/DOCUMENTATION_CATALOG.json',
            'docs/DOCUMENTATION_INDEX.md',
            'docs/completed/audits/ALPHA_60_ISOLATED_ADMISSION_ACCEPTANCE.md',
            'docs/in-progress/ALPHA_60_ISOLATED_ADMISSION_DESIGN.md',
            'docs/in-progress/ALPHA_LINEAR_BACKLOG.md',
            'scripts/verify_cpu_admission_probe.py',
            'src/assets/cpu_admission_worker.cpp',
            'src/assets/validated_cpu_model.cpp',
            'src/assets/validated_cpu_model.hpp',
            'src/moc3/import_sandbox.cpp',
            'tests/contracts/check_alpha_custody.py',
        ))),
    "dev/alpha-59-installed-abi-contract": (
        "52f3c7366021988d6d035d70c43bf13d22f4e337",
        frozenset((
            '.github/workflows/sdk-development-package.yml',
            'CMakeLists.txt',
            'cmake/AvatarRuntimeConfig.cmake.in',
            'cmake/ars_runtime_sdk.cmake',
            'docs/DOCUMENTATION_CATALOG.json',
            'docs/DOCUMENTATION_INDEX.md',
            'docs/completed/audits/ALPHA_59_INSTALLED_ABI_ACCEPTANCE.md',
            'docs/in-progress/ALPHA_59_PUBLIC_ABI_CONTRACT.md',
            'docs/in-progress/ALPHA_LINEAR_BACKLOG.md',
            'docs/reference/SDK_DEVELOPMENT_PACKAGE.md',
            'scripts/verify_installed_sdk.py',
            'tests/contracts/check_alpha_custody.py',
            'tests/sdk/CMakeLists.txt',
            'tests/sdk/consumer.c',
            'tests/sdk/consumer.cpp',
        ))),
    "dev/alpha-58-r1-lease-codec": (
        "3846f1f7ae19e7d163b316c2d192252b199c6be7",
        frozenset((
            'CMakeLists.txt',
            'docs/DOCUMENTATION_CATALOG.json',
            'docs/DOCUMENTATION_INDEX.md',
            'docs/completed/audits/ALPHA_58_SESSION_ACCEPTANCE.md',
            'docs/in-progress/ALPHA_58_LEASE_PROTOCOL.md',
            'docs/in-progress/ALPHA_58_SESSION_LIFECYCLE_DESIGN.md',
            'docs/in-progress/ALPHA_LINEAR_BACKLOG.md',
            'scripts/moc3_admission_provenance.py',
            'scripts/moc3_isolated_session.py',
            'scripts/moc3_lease.py',
            'scripts/moc3_lease_publication.py',
            'scripts/moc3_private_lease_fixture.py',
            'scripts/moc3_session_viewer.py',
            'scripts/moc3_verify_lease_caps.py',
            'scripts/moc3_verify_session_acceptance.py',
            'scripts/moc3_verify_session_lifecycle.py',
            'scripts/moc3_verify_session_viewer.py',
            'src/moc3/core_consumer.cpp',
            'src/moc3/core_consumer.hpp',
            'src/moc3/drawable_packet.cpp',
            'src/moc3/import_sandbox.cpp',
            'src/moc3/import_worker.cpp',
            'src/moc3/lease_protocol.cpp',
            'src/moc3/lease_protocol.hpp',
            'src/moc3/lease_publication.cpp',
            'src/moc3/lease_publication.hpp',
            'src/moc3/lease_worker.cpp',
            'src/moc3/lease_worker.hpp',
            'src/moc3/package_admission.cpp',
            'src/moc3/package_admission.hpp',
            'src/moc3/packet_publication_host.cpp',
            'src/moc3/retained_physics.cpp',
            'src/moc3/retained_physics.hpp',
            'tests/contracts/check_alpha_custody.py',
            'tests/moc3/core_consumer_tests.cpp',
            'tests/moc3/lease_fault_worker.cpp',
            'tests/moc3/lease_protocol_tests.cpp',
            'tests/moc3/test_lease_frontend.py',
        ))),
    "fix/alpha-52-native-test-targets": (
        "a17d4f92d246ad189b569527b05b471f076eb8b2",
        frozenset(("CMakeLists.txt", "scripts/moc3_native_test_target.py",
            "scripts/moc3_publication_probe.py", "scripts/moc3_admission_provenance.py",
            "tests/moc3/test_native_physics_reference.py", "tests/moc3/test_packet_publication.py",
            "tests/moc3/test_step_publication.py", "tests/moc3/test_native_test_targets.py",
            "docs/completed/audits/ALPHA_52_NATIVE_TEST_TARGET_ACCEPTANCE.md",
            "docs/in-progress/ALPHA_LINEAR_BACKLOG.md", "docs/DOCUMENTATION_CATALOG.json",
            "docs/DOCUMENTATION_INDEX.md"))),
    "fix/alpha-53-historical-custody": (
        "4b05828d402003d560602718eec7d8f9f7d63d48",
        frozenset(("tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m2_e_custody.py",
            "tests/contracts/test_g00_final_guards.py", "tests/contracts/check_alpha_custody.py",
            "tests/contracts/test_alpha_integration_custody.py",
            "docs/completed/audits/ALPHA_53_CUSTODY_ACCEPTANCE.md",
            "docs/in-progress/ALPHA_LINEAR_BACKLOG.md", "docs/DOCUMENTATION_CATALOG.json",
            "docs/DOCUMENTATION_INDEX.md"))),
    "fix/alpha-54-packed-gltf-accessors": (
        "579a04cab90f73d021d88996f0af1d6571d86f94",
        frozenset(("src/rendering/scene3d.cpp", "tests/rendering/scene3d_tests.cpp",
            "tests/rendering/d3d11_scene3d_pixels.cpp", "tests/assets/fixtures/generate_fixtures.py",
            "tests/assets/fixtures/manifest.json", "tests/contracts/check_alpha_custody.py",
            "docs/completed/audits/ALPHA_54_PACKED_ACCESSORS_ACCEPTANCE.md",
            "docs/in-progress/ALPHA_LINEAR_BACKLOG.md", "docs/DOCUMENTATION_CATALOG.json",
            "docs/DOCUMENTATION_INDEX.md"))),
    "fix/alpha-55-vertex-colors": (
        "2c335e8120e3946e5ee931143d68359e0f8a6b4b",
        frozenset(("src/rendering/scene3d.hpp", "src/rendering/scene3d.cpp",
            "src/rendering/d3d11_scene3d.cpp", "tests/rendering/scene3d_tests.cpp",
            "tests/rendering/d3d11_scene3d_pixels.cpp", "tests/assets/fixtures/generate_fixtures.py",
            "tests/assets/fixtures/manifest.json", "tests/contracts/check_alpha_custody.py",
            "docs/completed/audits/ALPHA_55_VERTEX_COLOR_ACCEPTANCE.md",
            "docs/in-progress/ALPHA_LINEAR_BACKLOG.md", "docs/DOCUMENTATION_CATALOG.json",
            "docs/DOCUMENTATION_INDEX.md"))),
    "fix/alpha-56-d3d-failure-status": (
        "76dc6282b2b52b015bd38bfdccb8400539a710f6",
        frozenset(("src/graphics/d3d11/creation_status.hpp", "src/graphics/d3d11/host_device.hpp",
            "src/graphics/d3d11/host_device.cpp", "src/graphics/d3d11/query_retirement.hpp",
            "src/graphics/d3d11/query_retirement.cpp", "src/rendering/d3d11_scene3d.hpp",
            "src/rendering/d3d11_scene3d.cpp", "tests/rendering/d3d11_scene3d_pixels.cpp",
            "tests/contracts/check_alpha_custody.py", "docs/completed/audits/ALPHA_56_D3D_FAILURE_ACCEPTANCE.md",
            "docs/in-progress/ALPHA_LINEAR_BACKLOG.md", "docs/DOCUMENTATION_CATALOG.json",
            "docs/DOCUMENTATION_INDEX.md"))),
    "fix/alpha-57-reflected-orientation": (
        "da805b4a361fca11dda766d3943c4b8369c34691",
        frozenset(("src/rendering/scene3d.cpp", "tests/rendering/scene3d_tests.cpp",
            "tests/rendering/d3d11_scene3d_pixels.cpp", "tests/assets/fixtures/generate_fixtures.py",
            "tests/assets/fixtures/manifest.json", "tests/contracts/check_alpha_custody.py",
            "docs/completed/audits/ALPHA_57_REFLECTED_TRANSFORM_ACCEPTANCE.md",
            "docs/in-progress/ALPHA_LINEAR_BACKLOG.md", "docs/DOCUMENTATION_CATALOG.json",
            "docs/DOCUMENTATION_INDEX.md"))),
    "dev/alpha-58-session-policy": (
        "37ba1355f987a1d9fe14c97ab5654700e6c93b32",
        frozenset(("docs/in-progress/ALPHA_58_SESSION_LIFECYCLE_DESIGN.md",
            "docs/in-progress/ALPHA_LINEAR_BACKLOG.md", "docs/DOCUMENTATION_CATALOG.json",
            "docs/DOCUMENTATION_INDEX.md", "tests/contracts/check_alpha_custody.py"))),
}

def check_alpha(candidate="HEAD", base=MAIN):
    pins.require(base == MAIN, "Frozen alpha main parent moved")
    pins.git("merge-base", "--is-ancestor", INTEGRATION, candidate)
    commits = pins.git("rev-list", "--first-parent", "--reverse", MAIN + ".." + INTEGRATION).splitlines()
    pins.require(len(commits) == len(SOURCE_HEADS), "Alpha merge count drift")
    parent = MAIN
    for commit, source in zip(commits, SOURCE_HEADS):
        pins.require(pins.git("rev-parse", commit + "^1") == parent, "Alpha merge parent drift")
        pins.require(pins.git("rev-parse", commit + "^2") == source, "Alpha source/order drift")
        pins.git("merge-base", "--is-ancestor", source, INTEGRATION)
        parent = commit
    changed = set(pins.git("diff", "--name-only", MOC3_BASE, INTEGRATION).splitlines())
    pins.require(changed == RECEIPT_DELTA, "Alpha changed more than the separate PR44 receipt")
    for path in HELD_PATHS:
        pins.require(pins.git("rev-parse", candidate + ":" + path) ==
                     pins.git("rev-parse", INTEGRATION + ":" + path), "Alpha held/public blob drift: " + path)
    original = json.loads(pins.git("show", INTEGRATION + ":docs/reference/M0_CONTRACT_PARENT_PINS.json"))
    pins.validate_blob_manifest(original, candidate)
    return True

def check_alpha_pull_request(base, candidate, branch):
    pins.require(branch in PR_SCOPES, "Unregistered alpha issue branch")
    expected, paths = PR_SCOPES[branch]
    pins.require(base == expected, "Alpha issue base moved: fresh reconciliation required")
    pins.git("merge-base", "--is-ancestor", base, candidate)
    changed = set(pins.git("diff", "--name-only", base, candidate).splitlines())
    pins.require(changed == paths, "Alpha issue path scope drift")
    check_alpha(candidate)
    return expected
