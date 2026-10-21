"""Relocated ABI20 D3D11 SDK consumers; run explicitly on Windows GPU hosts."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--static-build', type=Path, default=ROOT / 'build/a39-sdk-s')
    parser.add_argument('--shared-build', type=Path, default=ROOT / 'build/a39-sdk-d')
    args = parser.parse_args()
    root = args.evidence.resolve() / uuid.uuid4().hex
    root.mkdir(parents=True)
    paths = [ROOT / 'CMakeLists.txt', Path(__file__), ROOT / 'include/ars/ars.h']
    for folder in ('src', 'cmake'):
        paths += [p for p in (ROOT / folder).rglob('*') if p.is_file()]
    paths += [ROOT / 'tests/rendering/public_d3d11_tests.cpp', ROOT / 'tests/abi/public_gpu_consumer.c']
    paths += [ROOT / 'tests/rendering/generate_committed_fixtures.py']

    def hashes():
        return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}

    receipt = {'result': 'RUNNING', 'source_before': hashes(), 'cases': [], 'logs': []}

    def save():
        (root / 'evidence.json').write_text(json.dumps(receipt, indent=2))

    def run(command, cwd=ROOT):
        log = root / f'{len(receipt["logs"]):03d}.log'
        with log.open('wb') as stream:
            result = subprocess.run(list(map(str, command)), cwd=cwd, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=900)
        receipt['logs'].append({'command': list(map(str, command)), 'exit': result.returncode,
                                'log': log.name, 'sha256': hashlib.sha256(log.read_bytes()).hexdigest()})
        save()
        assert result.returncode == 0, (result.returncode, str(log))

    try:
        committed = root / 'committed-fixtures'
        run([sys.executable, ROOT / 'tests/rendering/generate_committed_fixtures.py', args.corpus.resolve(), committed])
        for shared in (False, True):
            # Keep dependency include paths below legacy Windows path limits;
            # installed/deployed artifacts still live in the isolated receipt.
            build = (args.shared_build if shared else args.static_build).resolve()
            run(['cmake', '-S', ROOT, '-B', build, '-G', 'Visual Studio 18 2026', '-A', 'x64',
                 '-DARS_BUILD_RUNTIME_CORE=ON', '-DARS_BUILD_ASSET_VALIDATOR=ON',
                 '-DARS_ENABLE_PUBLIC_CPU_ASSETS=ON', '-DARS_ENABLE_PUBLIC_D3D11_HOST=ON',
                 '-DARS_INSTALL_RUNTIME_CORE_SDK=ON', '-DARS_ENABLE_PREP_TESTS=OFF',
                 '-DARS_ASSET_FETCH_DEPS=OFF', '-DARS_ASSET_DEP_CACHE=' + str(ROOT / 'build/c-deps'),
                 '-DBUILD_SHARED_LIBS=' + ('ON' if shared else 'OFF')])
            for config in ('Release', 'Debug'):
                case = root / (('shared-' if shared else 'static-') + config.lower())
                case.mkdir()
                run(['cmake', '--build', build, '--config', config, '--target', 'ars_runtime_core',
                     'ars_cpu_admission_worker', 'ars_cpu_model_broker', '--parallel', '4'])
                if shared:
                    run(['ctest', '--test-dir', build, '-C', config, '-R', '^b1_exact_exports$',
                         '--no-tests=error', '--output-on-failure'])
                original, installed = case / 'original-sdk', case / 'relocated-sdk'
                run(['cmake', '--install', build, '--config', config, '--prefix', original])
                assert original.resolve().is_relative_to(root) and not installed.exists()
                shutil.move(str(original), str(installed))
                assert not original.exists()
                assert (installed / 'include/ars/ars.h').read_bytes() == (ROOT / 'include/ars/ars.h').read_bytes()
                for path in installed.rglob('*.cmake'):
                    text = path.read_text().replace('\\', '/').lower()
                    assert str(ROOT).replace('\\', '/').lower() not in text
                source = case / 'consumer-src'
                source.mkdir()
                shutil.copyfile(ROOT / 'tests/rendering/public_d3d11_tests.cpp', source / 'gpu.cpp')
                shutil.copyfile(ROOT / 'tests/abi/public_gpu_consumer.c', source / 'consumer.c')
                (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.22)
project(PublicGpuConsumer LANGUAGES C CXX)
find_package(AvatarRuntime 0.1 CONFIG REQUIRED COMPONENTS RuntimeCore)
if(NOT AvatarRuntime_PROFILE STREQUAL "OPT_IN_PUBLIC_D3D11_HOST")
 message(FATAL_ERROR "Wrong profile")
endif()
add_executable(gpu gpu.cpp)
target_compile_features(gpu PRIVATE cxx_std_20)
target_compile_definitions(gpu PRIVATE NOMINMAX)
target_link_libraries(gpu PRIVATE ARS::RuntimeCore d3d11 dxgi)
add_executable(consumer_c consumer.c)
set_target_properties(consumer_c PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
target_link_libraries(consumer_c PRIVATE ARS::RuntimeCore d3d11 dxgi)
foreach(target IN ITEMS gpu consumer_c)
 ars_copy_cpu_helpers(${target})
 target_compile_options(${target} PRIVATE /W4 /WX)
endforeach()
''')
                consumer = case / 'consumer-build'
                run(['cmake', '-S', source, '-B', consumer, '-G', 'Visual Studio 18 2026', '-A', 'x64',
                     '-DCMAKE_PREFIX_PATH=' + str(installed)])
                run(['cmake', '--build', consumer, '--config', config, '--parallel', '4'])
                deployed = case / 'deployed'
                shutil.copytree(consumer / config, deployed)
                hostile = case / 'hostile-cwd'
                hostile.mkdir()
                fixture = args.corpus.resolve() / 'positive-dual-full.arschar'
                run([deployed / 'consumer_c.exe', fixture], cwd=hostile)
                run([deployed / 'gpu.exe', fixture], cwd=hostile)
                run([deployed / 'gpu.exe', fixture, '--hardware'], cwd=hostile)
                for suffix in ([], ['--hardware']):
                    run([deployed / 'gpu.exe', committed / 'committed-euler.arschar', '--dynamic', *suffix], cwd=hostile)
                    run([deployed / 'gpu.exe', committed, '--mip', *suffix], cwd=hostile)
                receipt['cases'].append({'case': case.name, 'result': 'PASS',
                    'installed_sha256': {p.relative_to(installed).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                         for p in installed.rglob('*') if p.is_file()}})
        receipt['source_after'] = hashes()
        assert receipt['source_before'] == receipt['source_after'], 'SOURCE_CHANGED'
        receipt['result'] = 'PASS'
    except BaseException as error:
        receipt['result'] = 'FAIL'
        receipt['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        save()
    print('PUBLIC_D3D11_INSTALLED_MATRIX_PASS', root)


if __name__ == '__main__':
    main()
