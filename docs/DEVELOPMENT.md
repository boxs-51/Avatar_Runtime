# Working on the contract preparation tree

This tree contains design fixtures, nonfunctional ABI stubs and a deterministic
security contract model. It does not contain a production rendering SDK.

Use Python 3.12, Git with complete history, CMake 3.22 or newer, and a C11/C++20
compiler. The Windows CI uses Visual Studio 2022 with the C++ workload and Windows
SDK. CMake alone does not supply a compiler. A newer local Visual Studio needs a
matching CMake generator and a separate build directory; it does not reproduce
the exact CI toolchain.

From the repository root, install the same schema dependency as CI in an isolated
environment:

```text
python -m venv .venv
```

Activate `.venv` using the command appropriate to your shell, then run:

```text
python -m pip install jsonschema==4.23.0
python -m unittest discover -s tests/contracts -p "test_*.py" -v
python tests/contracts/check_m0_rc1_pins.py
python tests/contracts/g00_stage_gate_contract.py
```

The current suite has 77 Python tests. One test writes a temporary JSON file, so
the process needs a writable temporary directory. A permission failure there is
an environment failure and must not be reported as a successful test run.

For Windows native preparation checks:

```text
cmake -S . -B build/prep -G "Visual Studio 17 2022" -A x64 -DARS_PREP_ONLY=ON -DARS_ENABLE_PREP_TESTS=ON
cmake --build build/prep --config Debug --parallel
ctest --test-dir build/prep -C Debug --output-on-failure
```

This workstation was also verified with the installed Visual Studio 2026:

```text
cmake -S . -B build/prep-vs2026 -G "Visual Studio 18 2026" -A x64 -DARS_PREP_ONLY=ON -DARS_ENABLE_PREP_TESTS=ON
cmake --build build/prep-vs2026 --config Debug --parallel
ctest --test-dir build/prep-vs2026 -C Debug --output-on-failure
```

For Linux with GCC or Clang configured in the environment:

```text
cmake -S . -B build/prep -DCMAKE_BUILD_TYPE=Debug -DARS_PREP_ONLY=ON -DARS_ENABLE_PREP_TESTS=ON
cmake --build build/prep --parallel
ctest --test-dir build/prep --output-on-failure
```

Run each command only after the preceding command succeeds. The five native
CTest cases prove stub linkage and model behavior, not runtime security or GPU
behavior. Do not disable `ARS_PREP_ONLY` to try to obtain a production library.

The provenance checker uses committed Git blobs. Its local success does not
validate uncommitted edits or prove a future PR synthetic merge will pass. CI
also checks exact parent custody: a new branch/base pair requires a separately
reviewed transition. Do not rename a branch to impersonate an approved candidate,
change frozen blob pins merely to make checks green, or suppress the checker.

The PREP workflow covers `include/**`, `tests/abi/**` and the 87-case inventory
on PRs and on pushes to its configured preparation branch. Pushes to other
branches are not covered by that workflow's push event. Verify GitHub Actions
on the exact new PR SHA before making any integration readiness claim.
