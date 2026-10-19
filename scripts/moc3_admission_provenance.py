"""Exact public-code/binary provenance; never inventories model assets or pixels."""
from pathlib import Path
import hashlib
ROOT=Path(__file__).resolve().parents[1]
SOURCE_FILES=(
 'CMakeLists.txt','cmake/ars_asset_dependencies.cmake','include/ars/ars.h',
 'src/moc3/atlas.hpp','src/moc3/drawable_packet.hpp','src/moc3/drawable_packet.cpp',
 'src/moc3/package_admission.hpp','src/moc3/package_admission.cpp','src/moc3/physics_admission.cpp',
 'src/moc3/import_worker.cpp','src/moc3/import_sandbox.cpp','src/moc3/packet_guard.cpp','src/moc3/packet_render_host.cpp',
 'src/moc3/d3d11_renderer.hpp','src/moc3/d3d11_renderer.cpp','src/moc3/independent_snapshot.hpp','src/moc3/independent_snapshot.cpp','src/moc3/pixel_bridge.cpp',
 'src/assets/asset_limits.hpp','src/assets/owned_bundle.hpp','src/assets/owned_bundle.cpp','src/assets/zip_reader.hpp','src/assets/zip_reader.cpp',
 'src/assets/png_decoder.hpp','src/assets/png_decoder.cpp','src/validation/strict_json.hpp','src/validation/strict_json.cpp',
 'src/rendering/d3d11_dual_mode_host.hpp','src/rendering/d3d11_dual_mode_host.cpp',
 'src/graphics/d3d11/query_retirement.hpp','src/graphics/d3d11/query_retirement.cpp',
 'scripts/moc3_public_package.py','scripts/moc3_public_preflight.py','scripts/moc3_admission_fixtures.py','scripts/moc3_admission_provenance.py',
 'scripts/moc3_admission_observer.py','scripts/moc3_verify_admission_observer.py','scripts/moc3_verify_package_admission.py',
 'scripts/moc3_local_replay.py','scripts/moc3_package_admission.py','scripts/moc3_local_physics.py','scripts/moc3_memory.py',
 'scripts/moc3_process_tree.py','scripts/moc3_local_pixels.py','scripts/moc3_session.py','scripts/moc3_test_harness.py',
 'tests/moc3/drawable_packet_tests.cpp','tests/moc3/physics_admission_tests.cpp','tests/moc3/test_public_package.py',
 'docs/completed/M2_MOC3_F4_PACKAGE_ADMISSION_DESIGN.md',
 'docs/in-progress/M2_MOC3_PUB02_A_RETAINED_SESSION_DESIGN.md',
 'scripts/moc3_verify_retained_session.py','tests/moc3/sequence_packet_tests.cpp','tests/moc3/test_retained_session.py',
 'src/moc3/retained_physics.hpp','src/moc3/retained_physics.cpp',
 'tests/moc3/retained_physics_tests.cpp','tests/moc3/generated_physics3.json','tests/moc3/generated_physics3_multichain.json','tests/moc3/test_native_physics_reference.py',
 'scripts/moc3_verify_isolated_physics.py','scripts/moc3_physics.py',
 'docs/in-progress/M2_MOC3_PUB02_B_ISOLATED_PHYSICS_DESIGN.md',
 'src/moc3/core_consumer.hpp','src/moc3/core_consumer.cpp','tests/moc3/core_consumer_tests.cpp',
 'src/moc3/packet_publication.hpp','src/moc3/packet_publication.cpp','src/moc3/packet_publication_host.cpp',
 'scripts/moc3_publication_probe.py','scripts/moc3_verify_packet_publication.py','tests/moc3/test_packet_publication.py','tests/moc3/publication_fence_tests.cpp',
 'docs/in-progress/M2_MOC3_PUB04_A_PUBLICATION_DESIGN.md',
 'src/moc3/physics_checkpoint.hpp','src/moc3/physics_checkpoint.cpp','src/moc3/step_packet.hpp','src/moc3/step_packet.cpp',
 'tests/moc3/checkpoint_tests.cpp','tests/moc3/test_step_frontend.py','tests/moc3/test_step_publication.py',
 'scripts/moc3_verify_checkpoint.py','docs/in-progress/M2_MOC3_PUB04_B_CHECKPOINT_DESIGN.md')
BINARY_FILES=tuple('build/moc3-public-isolation/bin/Release/'+name for name in (
 'ars_moc3_import_worker.exe','ars_moc3_import_sandbox.exe','ars_moc3_packet_guard.exe','ars_moc3_packet_tests.exe','ars_moc3_physics_admission_tests.exe','ars_moc3_sequence_tests.exe','ars_moc3_retained_physics_tests.exe','ars_moc3_checkpoint_tests.exe'))+tuple(
 'build/m2-d-private/bin/Release/'+name for name in ('ars_moc3_packet_render_host.exe','ars_moc3_pixel_bridge.exe','ars_moc3_packet_publication_host.exe','ars_moc3_publication_fence_tests.exe','ars_moc3_core_consumer_tests.exe','ars_m2_dual_pixels.exe','ars_moc3_pixel_tests.exe'))

def capture():
    dependency=list((ROOT/'build/c-deps').glob('*.zip'))
    purism=ROOT/'build/moc3-i0/purismcore'
    dependency.extend(purism.glob('src/*.c'));dependency.extend(purism.glob('src/*.h'));dependency.extend(purism.glob('include/**/*.h'))
    dependency.extend([purism/'CMakeLists.txt',purism/'LICENSE'])
    def digest(paths):return {str(path.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    core_sources=[p for folder in ('src/core','src/security','src/motion') for p in (ROOT/folder).glob('*') if p.suffix in ('.hpp','.cpp')]
    return {'sources':digest([ROOT/name for name in SOURCE_FILES]+sorted(core_sources)),'binaries':digest([ROOT/name for name in BINARY_FILES]),'dependencies':digest(sorted(dependency))}
