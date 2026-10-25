"""The shared owner authority stays isolated until production gates pass."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/"tools/avatar-studio"

def test_native_ingress_authority_is_test_only_for_now():
    cm=(SRC/"CMakeLists.txt").read_text(encoding="utf-8")
    app=cm.split("add_executable(avatar_studio ",1)[1].split(")",1)[0]
    assert "studio_rig_authority.c" not in app
    assert "add_executable(studio_rig_authority_tests" in cm
    assert "studio_rig_authority.c" in cm
    assert "g5_rig_shared_authority" in cm

def test_source_has_functional_contract_without_roadmap_labels():
    for name in ("studio_g5_bus.h","studio_g5_txn.h",
                 "studio_rig_dispatch.h","studio_rig_authority.h",
                 "studio_rig_authority.c"):
        text=(SRC/name).read_text(encoding="utf-8")
        assert not re.search(r"(?i)roadmap|milestone|G5 T[0-9]|phase [0-9]",text),name

def test_product_g5_applied_is_not_fabricated():
    studio=(SRC/"studio.c").read_text(encoding="utf-8")
    assert "g5_txn_apply(" not in studio
    assert "g5_bus_submit_trusted(" not in studio
