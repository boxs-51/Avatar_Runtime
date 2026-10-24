"""Synthetic tests for the no-publication model artifact gate.

No private model assets or identifying model metadata are used.
"""
from scripts.check_model_publication import classify, scan_package, main


def test_public_source_does_not_trigger_model_guard():
    assert classify("src/runtime_math.cpp", b"int add(int x,int y){return x+y;}") == set()
    assert classify("docs/reference/ADR.md",
                    b"Do not publish any private model texture.") == set()


def test_cubism_model_formats_are_non_public_by_default():
    for extension in (".moc3", ".model3.json", ".physics3.json",
                      ".motion3.json", ".exp3.json", ".pose3.json"):
        assert "model-or-media-extension" in classify(
            "fixtures/synthetic" + extension, b"dummy-not-real-model")


def test_private_directories_block_even_plain_text():
    for path in ("local-assets/result.txt", "private-models/input.tmp",
                 "build/trace.txt", "captures-private/frame.txt"):
        assert "restricted-directory" in classify(path, b"fake")


def test_renamed_binary_model_magic_is_rejected():
    assert "renamed-model-or-image" in classify("assets/placeholder.dat",
                                               b"MOC3synthetic-test")
    assert "renamed-model-or-image" in classify("sample/fake.blob",
                                               b"\x89PNG\r\n\x1a\n" + b"mock")


def test_model_manifest_json_disallowed_even_with_generic_filename():
    text = b'{"FileReferences":{"Moc":"synthetic-placeholder"}}'
    assert "model-manifest-json" in classify("fixtures/ordinary.json", text)


def test_raw_model_geometry_json_disallowed():
    text = b'{"Vertices":[[0,0],[1,0],[0,1]],"Example":true}'
    assert "model-geometry-json" in classify("fixtures/ordinary.json", text)


def test_non_model_json_control_passes():
    assert classify("contracts/response.json",
                    b'{"state":"COMMITTED","revision":3}') == set()


def test_guard_categories_do_not_include_sensitive_path_or_payload():
    path = "private-models/example.model3.json"
    result = classify(path, b"MOC3fake")
    assert result
    assert all("/" not in k and ".json" not in k and "example" not in k
               for k in result)

def test_sdk_package_public_source_passes(tmp_path):
    package = tmp_path / "sdk"
    (package / "include").mkdir(parents=True)
    (package / "include" / "api.h").write_text("int api(void);", encoding="utf-8")
    count, findings = scan_package(package)
    assert count == 1 and not findings


def test_sdk_package_private_model_and_renamed_image_fail_closed(tmp_path):
    package = tmp_path / "sdk"
    (package / "lib").mkdir(parents=True)
    (package / "private-models").mkdir()
    (package / "lib" / "renamed.dat").write_bytes(b"MOC3fake")
    (package / "private-models" / "data.txt").write_text("fake", encoding="utf-8")
    count, findings = scan_package(package)
    assert count == 2
    assert findings["renamed-model-or-image"] == 1
    assert findings["restricted-directory"] == 1


def test_sdk_package_symlink_is_blocked_without_dereference(tmp_path):
    package = tmp_path / "sdk"
    package.mkdir()
    target = tmp_path / "different"
    target.write_text("not a model", encoding="utf-8")
    link = package / "indirect.txt"
    try:
        link.symlink_to(target)
    except (OSError, NotImplementedError):
        import pytest
        pytest.skip("symlink creation not permitted by test OS")
    _, findings = scan_package(package)
    assert findings["symbolic-link-in-package"] == 1


def test_legacy_utf16_document_is_not_a_model_binary():
    ordinary = "Public release notes only".encode("utf-16")
    assert classify("docs/release_notes.txt", ordinary) == set()

def test_publication_denial_hides_name_and_private_file_counts(tmp_path, capsys):
    package = tmp_path / "pkg"
    package.mkdir()
    (package / "synthetic-private-model.moc3").write_bytes(b"MOC3fake")
    result = main(["--package", str(package)])
    output = capsys.readouterr().out
    assert result == 1
    assert "BLOCKED" in output
    assert "model-or-media-extension" in output
    assert "synthetic-private-model" not in output
    assert "1" not in output


def test_publication_cli_accepts_public_safe_installed_sdk(tmp_path, capsys):
    package = tmp_path / "sdk"
    package.mkdir()
    (package / "include.h").write_text("int public_api(void);", encoding="utf-8")
    assert main(["--package", str(package)]) == 0
    assert "PASS" in capsys.readouterr().out
