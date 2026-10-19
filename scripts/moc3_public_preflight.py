"""Default-off public-input parser preflight prototype; never renders/activates a model."""
from dataclasses import dataclass
import ctypes
import json
from ctypes import wintypes
from pathlib import Path
import shutil
import struct
import subprocess
import uuid
import winreg

ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/moc3-public-isolation/bin/Release'
STATUSES={'PREFLIGHT_OK','MODEL_REJECTED','INVALID_ENVELOPE','ISOLATION_DENIED','ISOLATION_NO_CONNECTION_WINDOW','MEMORY_DENIED',
          'TIMEOUT_REJECTED','CPU_REJECTED','WORKER_REJECTED','OUTPUT_REJECTED'}


@dataclass(frozen=True)
class Preflight:
    status:str
    profile_removed:bool
    job_empty:bool


def profile_absent(name):
    env=ctypes.WinDLL('userenv',use_last_error=True);adv=ctypes.WinDLL('advapi32',use_last_error=True)
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    env.DeriveAppContainerSidFromAppContainerName.argtypes=[wintypes.LPCWSTR,ctypes.POINTER(ctypes.c_void_p)]
    env.DeriveAppContainerSidFromAppContainerName.restype=ctypes.c_long
    adv.ConvertSidToStringSidW.argtypes=[ctypes.c_void_p,ctypes.POINTER(wintypes.LPWSTR)]
    adv.ConvertSidToStringSidW.restype=wintypes.BOOL
    adv.FreeSid.argtypes=[ctypes.c_void_p];adv.FreeSid.restype=ctypes.c_void_p
    kernel.LocalFree.argtypes=[ctypes.c_void_p];kernel.LocalFree.restype=ctypes.c_void_p
    sid=ctypes.c_void_p();text=wintypes.LPWSTR()
    if env.DeriveAppContainerSidFromAppContainerName(name,ctypes.byref(sid))<0:raise OSError('profile identity query failed')
    try:
        if not adv.ConvertSidToStringSidW(sid,ctypes.byref(text)):raise ctypes.WinError(ctypes.get_last_error())
        key=r'Software\Classes\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppContainer\Mappings'+'\\'+text.value
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,key):return False
        except FileNotFoundError:return True
    finally:
        if text:kernel.LocalFree(ctypes.cast(text,ctypes.c_void_p))
        if sid:adv.FreeSid(sid)


def _run(mode,payload=b'',arguments=()):
    if mode not in ('--moc','--probe','--memory','--hang','--cpu','--crash','--flood'):raise ValueError('invalid worker mode')
    folder=ROOT/'build/moc3-import-trials'/uuid.uuid4().hex[:24];folder.mkdir(parents=True,exist_ok=False)
    worker=folder/'worker.exe';shutil.copyfile(BIN/'ars_moc3_import_worker.exe',worker)
    (folder/'worker-data.txt').write_bytes(b'generated read-only worker sentinel')
    profile='AvatarMocImport-'+uuid.uuid4().hex
    try:
        completed=subprocess.run([str(BIN/'ars_moc3_import_sandbox.exe'),str(worker),profile,mode,*map(str,arguments)],
                                 input=payload,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,timeout=15,cwd=folder)
    except subprocess.TimeoutExpired:
        env=ctypes.WinDLL('userenv',use_last_error=True)
        env.DeleteAppContainerProfile.argtypes=[wintypes.LPCWSTR];env.DeleteAppContainerProfile.restype=ctypes.c_long
        env.DeleteAppContainerProfile(profile)
        return Preflight('BROKER_TIMEOUT_REJECTED',profile_absent(profile),False)
    removed=profile_absent(profile)
    (folder/'broker-state.json').write_text(json.dumps({'launcher_exit':completed.returncode,
         'profile_removed':removed,'output_bytes':len(completed.stdout)}),encoding='utf-8')
    if completed.returncode or not removed:return Preflight('ISOLATION_SETUP_REJECTED',removed,False)
    if len(completed.stdout)>64:return Preflight('BROKER_PROTOCOL_REJECTED',removed,True)
    try:status=completed.stdout.decode('ascii').strip()
    except UnicodeError:return Preflight('BROKER_PROTOCOL_REJECTED',removed,True)
    return Preflight(status if status in STATUSES else 'BROKER_PROTOCOL_REJECTED',removed,True)


def preflight_moc(moc,texture_count):
    if type(moc) is not bytes or not 4<=len(moc)<=16*1024*1024:
        raise ValueError('INVALID_MOC_ENVELOPE')
    if type(texture_count) is not int or not 1<=texture_count<=16:
        raise ValueError('INVALID_TEXTURE_COUNT')
    return _run('--moc',struct.pack('<II',len(moc),texture_count)+moc)
