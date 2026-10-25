"""Synthetic tests for the no-publication model artifact gate.

No private model assets or identifying model metadata are used.
"""
from scripts.check_model_publication import classify, scan_package, main


def test_authored_studio_source_and_compiled_runtime_are_private_even_renamed():
    samples = (
        ("assets/safe-looking.txt", b"STUDIO_GEOMETRY_5 7 4 1 1\\n"),
        ("assets/source.dat", b"ARSMESH3" + b"\\0"*32),
        ("assets/mesh.armesh", b"some-data"),
        ("assets/layer_geometry.txt", b"V 0.1 0.2"),
        ("assets/model_runtime.armesh.bak", b"binary backup"),
    )
    for name,data in samples:
        assert classify(name,data), "private authored model format slipped guard"


def test_authored_source_prefix_whitespace_and_renamed_json_are_private():
    assert classify("tmp/safe.txt", b"\xef\xbb\xbf\n STUDIO_GEOMETRY_5 1 2 3")
    assert classify("tmp/config.dat", b'{"Vertices":[[0,0],[1,0],[0,1]]}')


def test_studio_utf16_source_and_uninspectable_structured_blobs_fail():
    for data in (
        ("STUDIO_GEOMETRY_5 5 3 2").encode("utf-16"),
        ("\ufeff  STUDIO_GEOMETRY_4 5 3 2").encode("utf-8"),
    ):
        assert classify("rename/any.dat", data)
    from scripts.check_model_publication import MAX_JSON
    assert classify("assets/safe.dat", b"{" + b" "*MAX_JSON + b"}")


def test_nested_geometry_json_and_plaintext_receipts_fail():
    assert classify("safe.dat",
                    b'{"payload":{"mesh":{"vertices":[[0,0],[1,1],[2,2]]}}}')
    assert classify("synthetic.txt",
                    b"layer=0 index=1 vertex_id=2 x=1.25 y=0.5 u=0.25 v=0.75 revision=5")
    assert classify("note.txt",
                    b"layer=0 index=2 a=0 b=1 c=2 revision=7")
    assert classify("log.txt",
                    b"id=5 vertex_id=3 x=1.5 y=-1 revision=9 accepted=5 gpu=1")


def test_lfs_pointer_and_unapproved_git_modes_fail():
    from scripts.check_model_publication import validate_git_mode
    fake=b"version https://git-lfs.github.com/spec/v1\noid sha256:"+b"1"*64+b"\nsize 32\n"
    assert classify("src/a.txt",fake)
    for mode in ("120000","160000","100664"):
        try:
            validate_git_mode(mode)
        except RuntimeError:pass
        else:assert False,"unsafe Git mode not rejected"
    assert validate_git_mode("100644") is None
    assert validate_git_mode("100755") is None


def test_push_range_scans_deleted_private_blob_in_new_commits(tmp_path):
    import subprocess,sys
    from pathlib import Path
    repo=tmp_path/"isolated"
    repo.mkdir()
    def git(*args):
        return subprocess.run(["git",*args],cwd=repo,capture_output=True,
                              text=True,check=True,encoding="utf-8").stdout.strip()
    git("init","-q")
    git("config","user.email","synthetic@localhost")
    git("config","user.name","Fixture")
    (repo/"source.py").write_text("print(1)",encoding="utf-8")
    git("add","source.py")
    git("commit","-q","-m","base")
    base=git("rev-parse","HEAD")
    (repo/"sample.moc3").write_bytes(b"MOC3fake synthetic not a real model")
    git("add","sample.moc3")
    git("commit","-q","-m","bad")
    (repo/"sample.moc3").unlink()
    git("add","-u")
    git("commit","-q","-m","deleted")
    head=git("rev-parse","HEAD")
    approved=tmp_path/"approved.json"
    approved.write_text('{"format":"ARS_PUBLIC_TREE_REVIEW_V1","entries":[]}',
                         encoding="utf8")
    result=subprocess.run([sys.executable,"-B",
          str(Path(__file__).resolve().parents[1]/"scripts"/"check_model_publication.py"),
          "--push-range",base+".."+head,"--manifest",str(approved)],
          cwd=repo,capture_output=True,text=True)
    assert result.returncode==2
    assert "BLOCKED" in result.stderr and "sample.moc3" not in result.stderr


def test_exact_single_commit_tree_and_message_approval(tmp_path):
    import subprocess,sys,hashlib,json
    from pathlib import Path
    repo=tmp_path/"safe"
    repo.mkdir()
    def git(*args):
        return subprocess.run(["git",*args],cwd=repo,check=True,
              capture_output=True,text=True,encoding="utf8").stdout.strip()
    git("init","-q")
    git("config","user.name","Studio Review")
    git("config","user.email","review@users.noreply.github.com")
    (repo/"code.py").write_text("print(1)",encoding="utf8")
    git("add","code.py")
    git("commit","-q","-m","baseline")
    base=git("rev-parse","HEAD")
    (repo/"code.py").write_text("print(2)",encoding="utf8")
    git("add","code.py")
    subject="review(studio): synthetic source gate test"
    git("commit","-q","-m",subject)
    head=git("rev-parse","HEAD")
    raw=subprocess.run(["git","ls-tree","-r","-z","HEAD"],cwd=repo,
                       capture_output=True,check=True).stdout
    entries=[]
    for item in raw.split(b"\0"):
        if not item:continue
        mode,kind,oid,path=item.split(None,3)
        entries.append({"path":path.decode("utf8"),"oid":oid.decode("ascii")})
    manifest=tmp_path/"frozen.json"
    manifest.write_text(json.dumps({
       "format":"ARS_PUBLIC_TREE_REVIEW_V1","entries":entries,
       "base_oid":base,"commit_oid":head,
        "commit_message_sha256":hashlib.sha256((subject+"\n").encode()).hexdigest()
    }),encoding="utf8")
    cmd=[sys.executable,"-B",
         str(Path(__file__).resolve().parents[1]/"scripts"/"check_model_publication.py"),
         "--push-range",base+".."+head,"--manifest",str(manifest)]
    accepted=subprocess.run(cmd,cwd=repo,capture_output=True,text=True)
    assert accepted.returncode==0,accepted.stderr
    assert "PASS" in accepted.stdout
    payload=json.loads(manifest.read_text(encoding="utf8"))
    payload["entries"][0]["oid"]="a"*40
    manifest.write_text(json.dumps(payload),encoding="utf8")
    rejected=subprocess.run(cmd,cwd=repo,capture_output=True,text=True)
    assert rejected.returncode==2
    assert "BLOCKED" in rejected.stderr


def test_git_replace_cannot_hide_private_model_blob(tmp_path):
    import subprocess,sys
    from pathlib import Path
    repo=tmp_path/"replace"
    repo.mkdir()
    def git(*args):
        return subprocess.run(["git",*args],cwd=repo,check=True,
                    capture_output=True,text=True,encoding="utf8").stdout.strip()
    git("init","-q")
    git("config","user.name","Synthetic")
    git("config","user.email","synthetic@localhost")
    (repo/"safe.py").write_text("print(1)",encoding="utf8")
    git("add","safe.py")
    git("commit","-q","-m","baseline")
    (repo/"model.moc3").write_bytes(b"MOC3fake synthetic test")
    git("add","model.moc3")
    git("commit","-q","-m","bad")
    original=git("rev-parse","HEAD")
    (repo/"model.moc3").unlink()
    git("add","-u")
    git("commit","-q","-m","clean-looking")
    benign=git("rev-parse","HEAD")
    git("replace",original,benign)
    script=str(Path(__file__).resolve().parents[1]/
               "scripts"/"check_model_publication.py")
    response=subprocess.run([sys.executable,"-B",script,"--ref",original],
                            cwd=repo,capture_output=True,text=True)
    assert response.returncode==1
    assert "BLOCKED" in response.stdout


def test_raw_parent_rejects_local_git_graft(tmp_path):
    import subprocess,sys,json,hashlib
    from pathlib import Path
    repo=tmp_path/"graft"
    repo.mkdir()
    def git(*args):
        return subprocess.run(["git",*args],cwd=repo,check=True,
               capture_output=True,text=True,encoding="utf8").stdout.strip()
    git("init","-q")
    git("config","user.name","Studio Review")
    git("config","user.email","review@users.noreply.github.com")
    (repo/"code.py").write_text("print(1)",encoding="utf8")
    git("add","code.py")
    git("commit","-q","-m","base")
    base=git("rev-parse","HEAD")
    (repo/"code.py").write_text("print(2)",encoding="utf8")
    git("add","code.py")
    git("commit","-q","-m","intermediate")
    (repo/"code.py").write_text("print(3)",encoding="utf8")
    git("add","code.py")
    message="review(studio): synthetic graft detection"
    git("commit","-q","-m",message)
    head=git("rev-parse","HEAD")
    # Rewrite the visible ancestry only; raw commit still has a
    # private intermediate parent. No real assets are used here.
    info=repo/".git"/"info"
    info.mkdir(exist_ok=True)
    (info/"grafts").write_text(head+" "+base+"\n",encoding="utf8")
    raw=subprocess.run(["git","ls-tree","-r","-z",head],cwd=repo,
          capture_output=True,check=True).stdout
    entries=[]
    for item in raw.split(b"\0"):
        if not item:continue
        info,path=item.split(b"\t",1)
        mode,kind,oid=info.split()
        entries.append({"path":path.decode("utf8"),"oid":oid.decode("ascii")})
    approval=tmp_path/"manifest.json"
    approval.write_text(json.dumps({
        "format":"ARS_PUBLIC_TREE_REVIEW_V1",
        "entries":entries,
        "base_oid":base,"commit_oid":head,
        "commit_message_sha256":hashlib.sha256((message+"\n").encode()).hexdigest()
    }),encoding="utf8")
    tool=str(Path(__file__).resolve().parents[1]/
             "scripts"/"check_model_publication.py")
    r=subprocess.run([sys.executable,"-B",tool,"--push-range",
        base+".."+head,"--manifest",str(approval)],cwd=repo,
        capture_output=True,text=True)
    assert r.returncode==2
    assert "BLOCKED" in r.stderr


def test_manifest_requires_exact_frozen_blob_list(tmp_path):
    import json
    from scripts.check_model_publication import compare_manifest
    manifest=tmp_path/"approved.json"
    manifest.write_text(json.dumps({
         "format":"ARS_PUBLIC_TREE_REVIEW_V1",
         "entries":[{"oid":"a"*40,"path":"src/example.c"}]}),encoding="utf8")
    compare_manifest([(("a"*40).encode("ascii"),"src/example.c")],str(manifest))
    try:
        compare_manifest([(("b"*40).encode("ascii"),"src/example.c")],str(manifest))
    except RuntimeError:pass
    else:assert False,"modified blob unexpectedly accepted"


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
    assert "CLEAN_NOT_APPROVED" in capsys.readouterr().out
