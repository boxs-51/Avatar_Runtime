"""F2 actual local host pixels. Private images/metrics remain under build/."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import platform
from pathlib import Path
import queue
import re
import struct
import subprocess
import threading
import time
import uuid

from moc3_local_replay import _private_manifest
from moc3_package_admission import AdmissionError, admit_package
from moc3_memory import process_memory
from moc3_session import MAX_TOTAL_PEAK_COMMIT
from moc3_process_tree import wait_launch_gate

ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/"build"/"m2-d-private"/"bin"/"Release"


class PixelError(Exception):
    pass


def require(value: bool, reason: str) -> None:
    if not value:
        raise PixelError(reason)


def eligible() -> bool:
    try:
        return (_private_manifest() is not None and
                (BIN/"ars_moc3_pixel_bridge.exe").is_file() and
                (BIN/"ars_moc3_pixel_tests.exe").is_file())
    except OSError:
        return False


def render(package, output: Path, hardware: bool, notify) -> dict:
    args=[str(BIN/"ars_moc3_pixel_bridge.exe"), str(output)]
    if hardware:
        args.append("--hardware")
    process=subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL)
    responses: queue.Queue[bytes | None]=queue.Queue(maxsize=16)
    def reader():
        while True:
            line=process.stdout.readline(256)
            responses.put(line if line else None)
            if not line:
                return
    worker=threading.Thread(target=reader, daemon=True)
    worker.start()
    def response():
        try:
            line=responses.get(timeout=30)
        except queue.Empty as error:
            raise PixelError("NATIVE_FRAME_TIMEOUT") from error
        require(line is not None, "NATIVE_RENDERER_CLOSED")
        return line.decode("ascii").split()
    frames=[]
    try:
        process.stdin.write(b"ARSF2PK1"+struct.pack("<II",len(package.moc_bytes),len(package.textures)))
        process.stdin.write(package.moc_bytes)
        for image in package.textures:
            process.stdin.write(struct.pack("<II",image.width,image.height))
            process.stdin.write(image.rgba)
        process.stdin.flush()
        require(response()==["READY"],"NATIVE_READY")
        for number,command in enumerate((b"FRAME 0\n", b"FRAME 1\nParamAngleX 10\n",
                                         b"FRAME 0\n", b"FRAME 0\n"),1):
            process.stdin.write(command); process.stdin.flush()
            result=response()
            require(result==["FRAME",str(number),str(number+1),str(number)],"FRAME_IDENTITY")
            metadata=json.loads((output/"latest.json").read_text(encoding="utf-8"))
            require(metadata["frame_generation"]==number and metadata["target_epoch"]==number
                    and metadata["snapshot_generation"]==number+1,"READBACK_METADATA_IDENTITY")
            rgba=(output/metadata["rgba"]).read_bytes()
            require(len(rgba)==512*512*4,"READBACK_SIZE")
            require(any(rgba[3::4]) and rgba[3]==0,"NONBLANK_TRANSPARENT_HOST_OUTPUT")
            frames.append(rgba)
            if notify:
                public_meta={key:metadata[key] for key in ("width","height","frame_generation",
                            "snapshot_generation","target_epoch","device_generation","backend")}
                public_meta.update(image=f"warp/{metadata['image']}",rgba=f"warp/{metadata['rgba']}")
                (output.parent/f"frame-{number}.json").write_text(json.dumps(public_meta),encoding="utf-8")
                (output.parent/"latest.json").write_text(json.dumps(public_meta),encoding="utf-8")
                notify(number)
                time.sleep(.3)
        require(frames[0]!=frames[1],"EVALUATOR_CHANGED_HOST_PIXELS")
        require(frames[0]==frames[2]==frames[3],"RESET_REPEAT_HOST_PIXELS")
        memory=process_memory(process.pid)
        parent_memory=process_memory()
        (output/"memory.json").write_text(json.dumps({"native":memory,"parent":parent_memory}),encoding="utf-8")
        require(parent_memory["peak_commit_bytes"]+memory["peak_commit_bytes"]<=MAX_TOTAL_PEAK_COMMIT,
                "COMBINED_PROCESS_COMMIT_BUDGET")
        process.stdin.write(b"QUIT\n");process.stdin.flush()
        require(response()==["BYE"],"NATIVE_CLOSE_AND_DEBUG_QUEUE")
        require(process.wait(timeout=5)==0,"NATIVE_RENDER_EXIT")
        return {"verdict":"PASS","native_memory":memory,
                "adapter":json.loads((output/"adapter.json").read_text(encoding="utf-8")),
                "frame_sha256":[hashlib.sha256(frame).hexdigest() for frame in frames]}
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        worker.join(timeout=1)
        process.stdin.close();process.stdout.close()


def run(run_id: str, packet: dict) -> None:
    require(eligible(),"LOCAL_FIXTURE_OR_F2_BUILD_UNAVAILABLE")
    output=ROOT/"build"/"moc3-f2-local"/run_id
    output.mkdir(parents=True,exist_ok=False)
    packet["output_ref"]=output.relative_to(ROOT).as_posix()
    for driver,args in (("warp",[]),("hardware",["--hardware"])):
        print(f"F2_STAGE {driver.upper()}",flush=True)
        test=subprocess.run([str(BIN/"ars_moc3_pixel_tests.exe"),*args],capture_output=True,
                            text=True,timeout=120,check=False)
        packet[f"generated_{driver}"]={"exit_code":test.returncode,"stdout":test.stdout,"stderr":test.stderr}
        require(test.returncode==0 and "F2_GENERATED_PIXELS_AND_LIFECYCLE_PASS" in test.stdout,
                f"GENERATED_{driver.upper()}_FAILED_OR_UNAVAILABLE")
    print("F2_STAGE PACKAGE",flush=True)
    manifest=_private_manifest()
    require(manifest is not None,"LOCAL_FIXTURE_UNAVAILABLE")
    package=admit_package(manifest.parent,manifest.name)
    packet["private_input"]={"moc_sha256":hashlib.sha256(package.moc_bytes).hexdigest(),
                             "atlas_sha256":[hashlib.sha256(t.rgba).hexdigest() for t in package.textures]}
    print("F2_STAGE RENDER",flush=True)
    packet["private_warp"]=render(package,output/"warp",False,
                                    lambda number:print(f"F2_FRAME {number}",flush=True))
    packet["private_hardware"]=render(package,output/"hardware",True,None)
    packet["python_memory"]=process_memory()
    print("F2_STAGE COMPLETE",flush=True)


if __name__=="__main__":
    wait_launch_gate()
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id",default=uuid.uuid4().hex[:24])
    args=parser.parse_args()
    if re.fullmatch(r"[0-9a-f]{24}",args.run_id) is None:
        parser.error("invalid run ID")
    packet={"schema_version":1,"phase":"F2","proof_tier":"host_integrated_local",
            "started_utc":datetime.now(timezone.utc).isoformat()}
    sources=("src/moc3/d3d11_renderer.hpp","src/moc3/d3d11_renderer.cpp",
             "src/moc3/pixel_bridge.cpp","tests/moc3/d3d11_pixels.cpp",
             "src/rendering/d3d11_dual_mode_host.hpp","src/rendering/d3d11_dual_mode_host.cpp",
             "src/graphics/d3d11/host_device.cpp","src/graphics/d3d11/state_scope.cpp",
             "src/graphics/d3d11/query_retirement.cpp","src/moc3/independent_snapshot.cpp",
             "scripts/moc3_local_pixels.py","scripts/moc3_package_admission.py",
             "scripts/moc3_test_harness.py","scripts/moc3_process_tree.py",
             "scripts/moc3_memory.py","src/moc3/independent_snapshot.hpp",
             "tests/moc3/test_harness_api.py","tests/moc3/test_process_tree.py","CMakeLists.txt")
    packet["source_sha256"]={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in sources}
    packet["parent_head"]=subprocess.run(["git","rev-parse","HEAD"],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
    packet["environment"]={"platform":platform.platform(),"python":platform.python_version(),"build":"Release"}
    packet["binary_sha256"]={name:hashlib.sha256((BIN/name).read_bytes()).hexdigest()
                             for name in ("ars_moc3_pixel_bridge.exe","ars_moc3_pixel_tests.exe")
                             if (BIN/name).is_file()}
    try:
        run(args.run_id,packet)
        packet["verdict"]="PASS"
    except (PixelError,AdmissionError,OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        packet["verdict"]="FAIL"
        packet["reason"]=str(error) if isinstance(error,PixelError) else type(error).__name__
    packet["completed_utc"]=datetime.now(timezone.utc).isoformat()
    folder=ROOT/"build"/"moc3-f2-local"/args.run_id
    folder.mkdir(parents=True,exist_ok=True)
    (folder/"evidence.json").write_text(json.dumps(packet,indent=2),encoding="utf-8")
    print("F2_LOCAL_HOST_PIXELS_PASS" if packet["verdict"]=="PASS" else "F2_LOCAL_HOST_PIXELS_FAIL",flush=True)
    raise SystemExit(0 if packet["verdict"]=="PASS" else 1)
