"""Windows OS proof for the default-off, verdict-only native admission probe.

Generated fixtures only. No model packet, registry handle, SDK activation or GPU.
Every OS trial is retained; containment/cleanup failure aborts without retry.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import time
import uuid

MAGIC = 0x31565341
CAP = 16 * 1024 * 1024
# ABI20 numeric status values; verify against the canonical header during review.
STATUS = {'OK': 0, 'BAD_ASSET': 3, 'UNSUPPORTED': 2, 'OUT_OF_BUDGET': 4, 'INTERNAL': 9}


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def request(data, nonce=1):
    if not 0 < len(data) <= CAP or not 0 < nonce < 2**64:
        raise ValueError('probe input/nonce exceeds diagnostic profile')
    return struct.pack('<IIIIQQ', MAGIC, 1, len(data), checksum(data), nonce, 0) + data


def reply(data, nonce):
    if len(data) != 32:
        raise ValueError('verdict length')
    magic, version, status, zero, echoed, reserved = struct.unpack('<IIIIQQ', data)
    if (magic, version, zero, echoed, reserved) != (MAGIC, 1, 0, nonce, 0) or status not in STATUS.values():
        raise ValueError('verdict header/status/nonce')
    return status


def check(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', type=Path, required=True)
    parser.add_argument('--fault-worker', type=Path, required=True)
    parser.add_argument('--broker', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    # Reuse the existing Windows registry/SID query; no parser or model pipeline.
    from moc3_public_preflight import profile_absent
    args.evidence = args.evidence.resolve() / uuid.uuid4().hex
    args.evidence.mkdir(parents=True, exist_ok=False)
    report = {'result': 'RUNNING', 'profile': 'ISOLATED_NATIVE_VERDICT_PROBE',
              'issue60_complete': False, 'public_activation': False, 'cases': [],
              'source_sha256': {name: hashlib.sha256((Path(__file__).resolve().parents[1] / name).read_bytes()).hexdigest()
                                for name in ('CMakeLists.txt', 'src/assets/cpu_admission_worker.cpp',
                                             'src/assets/validated_cpu_model.cpp', 'src/assets/validated_cpu_model.hpp',
                                             'src/moc3/import_sandbox.cpp', 'scripts/verify_cpu_admission_probe.py',
                                             'tests/assets/fixtures/manifest.json')},
              'binary_sha256': {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in (args.worker, args.broker, args.fault_worker)}}

    def save():
        (args.evidence / 'evidence.json').write_text(json.dumps(report, indent=2), encoding='utf-8')

    def passed(name, **proof):
        report['cases'].append({'case': name, 'result': 'PASS', **proof})
        save()
        print('PASS', name, flush=True)

    def trial(name, payload, *, mode='--cpu-admit', fault=False):
        folder = args.evidence / ('trial-' + uuid.uuid4().hex)
        folder.mkdir()
        worker = folder / 'worker.exe'
        shutil.copyfile(args.fault_worker if fault else args.worker, worker)
        (folder / 'worker-data.txt').write_bytes(b'generated admission worker sentinel')
        profile = 'AvatarMocImport-' + uuid.uuid4().hex
        (folder / 'request.bin').write_bytes(payload)
        (folder / 'trial.json').write_text(json.dumps({'case': name, 'profile': profile, 'mode': mode}), encoding='utf-8')
        started = time.monotonic()
        run = subprocess.run([str(args.broker.resolve()), str(worker), profile, mode],
                             input=payload, capture_output=True, timeout=15, cwd=folder)
        elapsed = time.monotonic() - started
        (folder / 'stdout.bin').write_bytes(run.stdout)
        (folder / 'stderr.bin').write_bytes(run.stderr)
        state_path = folder / 'launcher-state.json'
        state = json.loads(state_path.read_text()) if state_path.exists() else {}
        cleanup_path = folder / 'job-cleanup.json'
        cleanup = json.loads(cleanup_path.read_text()) if cleanup_path.exists() else {}
        # Read actual Job accounting as well as final broker exit/profile state.
        proof = {'launcher_exit': run.returncode, 'profile_removed': profile_absent(profile),
                 'job_empty': run.returncode == 0 and type(state.get('worker_exit')) is int and
                              cleanup.get('query_error') == 0 and cleanup.get('active_processes') == 0,
                 'cleanup': cleanup,
                 'state': state, 'elapsed_seconds': elapsed}
        (folder / 'proof.json').write_text(json.dumps(proof, indent=2), encoding='utf-8')
        check(proof['profile_removed'] and proof['job_empty'], name + ': containment/cleanup failure')
        return run, state, proof

    try:
        direct = subprocess.run([str(args.worker.resolve()), '--cpu-admit'], input=request(b'x'),
                                capture_output=True, timeout=5)
        check(direct.returncode == 21 and not direct.stdout, 'direct worker isolation refusal')
        passed('direct_execution_refused')
        # Independent fixed-header consumer negatives, no OS run needed.
        good = struct.pack('<IIIIQQ', MAGIC, 1, 0, 0, 42, 0)
        check(reply(good, 42) == 0, 'valid typed verdict')
        bad = [good[:-1], good+b'x', struct.pack('<IIIIQQ', MAGIC, 1, 999, 0, 42, 0)]
        for offset in (0, 4, 12, 16, 24):
            value = bytearray(good); value[offset] ^= 1; bad.append(value)
        for data in bad:
            try:
                reply(data, 42)
            except ValueError:
                continue
            raise RuntimeError('malformed verdict accepted')
        passed('independent_verdict_negatives', count=len(bad))
        # Complete existing generated native bundle oracle, not a curated success subset.
        rows = json.loads((args.corpus / 'manifest.json').read_text())
        for index, row in enumerate(rows):
            if not row['file'].endswith('.arschar'):
                continue
            data = (args.corpus / row['file']).read_bytes()
            check(hashlib.sha256(data).hexdigest() == row['sha256'], 'fixture digest ' + row['id'])
            nonce = index + 1
            run, state, proof = trial(row['id'], request(data, nonce))
            check(state['worker_exit'] == 0 and state['wait_result'] == 0 and not state['output_overflow'], row['id'] + ': worker failure')
            actual = reply(run.stdout, nonce)
            check(actual == STATUS[row['expected']], row['id'] + ': wrong status ' + str(actual))
            passed('corpus_' + row['id'], status=actual, **proof)
        data = (args.corpus / 'positive-2d.arschar').read_bytes()
        good = request(data, 42)
        bad = {'short_header': good[:12], 'truncated': good[:-1], 'trailing': good+b'x'}
        for name, offset in [('magic', 0), ('version', 4), ('checksum', 12), ('reserved', 24), ('reserved_high', 28)]:
            altered = bytearray(good); altered[offset] ^= 1; bad[name] = bytes(altered)
        for name, size in [('empty', 0), ('over_cap', CAP+1), ('u32_max', 2**32-1)]:
            altered = bytearray(good[:32]); struct.pack_into('<I', altered, 8, size); bad[name] = bytes(altered)
        altered = bytearray(good); struct.pack_into('<Q', altered, 16, 0); bad['zero_nonce'] = bytes(altered)
        for name, payload in bad.items():
            run, state, proof = trial('wire_' + name, payload)
            check(state['worker_exit'] == 23 and run.stdout == b'WORKER_REJECTED\r\n', 'bad framing ' + name)
            passed('wire_' + name, **proof)
        nonce = 1 << 63
        run, state, proof = trial('nonce_high64', request(data, nonce))
        check(state['worker_exit'] == 0 and reply(run.stdout, nonce) == STATUS['OK'], 'full64bit nonce correlation')
        passed('nonce_high64', **proof)
        # Production has no diagnostic fault commands. Separate test-only binary exercises same broker.
        run, state, proof = trial('production_fault_command', b'', mode='--fault-hang')
        check(state['worker_exit'] == 22 and run.stdout == b'WORKER_REJECTED\r\n', 'production fault mode exposed')
        passed('production_fault_mode_absent', **proof)
        run, state, proof = trial('fault_memory', b'', mode='--fault-memory', fault=True)
        check(state['worker_exit'] == 0 and reply(run.stdout, 1) == STATUS['OUT_OF_BUDGET'], 'memory cap')
        passed('contained_memory', **proof)
        missing_profile = 'AvatarMocImport-' + uuid.uuid4().hex
        missing = subprocess.run([str(args.broker.resolve()), str(args.evidence / 'missing-worker.exe'),
                                  missing_profile, '--cpu-admit'], input=b'', capture_output=True, timeout=5)
        check(missing.returncode == 2 and not missing.stdout and profile_absent(missing_profile), 'missing helper')
        passed('missing_helper_refused_before_profile')
        for name, expected in [('crash', b'WORKER_REJECTED\r\n'), ('output', b'OUTPUT_REJECTED\r\n'),
                               ('cpu', b'CPU_REJECTED\r\n'), ('hang', b'TIMEOUT_REJECTED\r\n')]:
            run, state, proof = trial('fault_' + name, b'', mode='--fault-' + name, fault=True)
            check(run.stdout == expected, 'fault not contained: ' + name)
            passed('contained_' + name, **proof)
        for name, digest in report['source_sha256'].items():
            check(hashlib.sha256((Path(__file__).resolve().parents[1] / name).read_bytes()).hexdigest() == digest,
                  'source changed during qualification: ' + name)
        for name, digest in report['binary_sha256'].items():
            check(hashlib.sha256(Path(name).read_bytes()).hexdigest() == digest, 'binary changed during qualification')
        report['result'] = 'PASS'
    except Exception as error:
        report['result'] = 'FAIL'
        report['error'] = str(error)
        raise
    finally:
        save()
    print('ISOLATED_NATIVE_VERDICT_PASS', len(report['cases']), str(args.evidence), flush=True)


if __name__ == '__main__':
    main()
