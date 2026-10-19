"""Owned Windows job lifetime for the local F2 runner and native descendants.

The child waits at a launch gate before spawning anything. Assignment failure
therefore cannot leak an unowned native renderer. Kill-on-close covers an
unexpected harness exit; normal cancel waits for ActiveProcesses == 0.
https://learn.microsoft.com/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject
"""
import ctypes
from ctypes import wintypes
import os
import secrets
import time


class BasicLimits(ctypes.Structure):
    _fields_=[("process_time",ctypes.c_int64),("job_time",ctypes.c_int64),
              ("flags",wintypes.DWORD),("minimum",ctypes.c_size_t),("maximum",ctypes.c_size_t),
              ("active_limit",wintypes.DWORD),("affinity",ctypes.c_size_t),
              ("priority",wintypes.DWORD),("scheduling",wintypes.DWORD)]


class IoCounters(ctypes.Structure):
    _fields_=[(f"value{i}",ctypes.c_uint64) for i in range(6)]


class ExtendedLimits(ctypes.Structure):
    _fields_=[("basic",BasicLimits),("io",IoCounters),("process_memory",ctypes.c_size_t),
              ("job_memory",ctypes.c_size_t),("peak_process",ctypes.c_size_t),("peak_job",ctypes.c_size_t)]


class Accounting(ctypes.Structure):
    _fields_=[(f"time{i}",ctypes.c_int64) for i in range(4)]+[
        ("faults",wintypes.DWORD),("total",wintypes.DWORD),("active",wintypes.DWORD),
        ("terminated",wintypes.DWORD)]


def kernel_api():
    k=ctypes.WinDLL("kernel32",use_last_error=True)
    def bind(name,args,result):
        fn=getattr(k,name);fn.argtypes=args;fn.restype=result
    H,D,B,P=wintypes.HANDLE,wintypes.DWORD,wintypes.BOOL,ctypes.c_void_p
    bind("CreateJobObjectW",[P,wintypes.LPCWSTR],H)
    bind("CreateEventW",[P,B,B,wintypes.LPCWSTR],H)
    bind("OpenEventW",[D,B,wintypes.LPCWSTR],H)
    bind("SetInformationJobObject",[H,ctypes.c_int,P,D],B)
    bind("QueryInformationJobObject",[H,ctypes.c_int,P,D,P],B)
    bind("AssignProcessToJobObject",[H,H],B)
    bind("TerminateJobObject",[H,wintypes.UINT],B)
    bind("OpenProcess",[D,B,D],H)
    bind("SetEvent",[H],B)
    bind("WaitForSingleObject",[H,D],D)
    bind("CloseHandle",[H],B)
    return k


def wait_launch_gate():
    name=os.environ.get("MOC3_F2_LAUNCH_GATE")
    if not name:
        return
    k=kernel_api();event=k.OpenEventW(0x100000,False,name)
    if not event:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        if k.WaitForSingleObject(event,5000)!=0:
            raise OSError("F2 launch gate timeout")
    finally:
        k.CloseHandle(event)


class ProcessTree:
    def __init__(self):
        if os.name!="nt":
            raise OSError("Windows job required for F2")
        self.k=kernel_api();self.job=None;self.event=None
        self.gate_name="Local\\AvatarMocF2-"+secrets.token_hex(16)
        try:
            self.job=self.k.CreateJobObjectW(None,None)
            if not self.job:
                raise ctypes.WinError(ctypes.get_last_error())
            limits=ExtendedLimits();limits.basic.flags=0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            if not self.k.SetInformationJobObject(self.job,9,ctypes.byref(limits),ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
            self.event=self.k.CreateEventW(None,True,False,self.gate_name)
            if not self.event:
                raise ctypes.WinError(ctypes.get_last_error())
        except BaseException:
            self.close()
            raise

    def environment(self):
        return {**os.environ,"MOC3_F2_LAUNCH_GATE":self.gate_name}

    def attach_and_release(self,pid: int):
        process=self.k.OpenProcess(0x100|0x1,False,pid)
        if not process:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            if not self.k.AssignProcessToJobObject(self.job,process):
                raise ctypes.WinError(ctypes.get_last_error())
            if not self.k.SetEvent(self.event):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            self.k.CloseHandle(process)

    def active(self):
        info=Accounting()
        if not self.k.QueryInformationJobObject(self.job,1,ctypes.byref(info),ctypes.sizeof(info),None):
            raise ctypes.WinError(ctypes.get_last_error())
        return info.active

    def terminate(self):
        if not self.k.TerminateJobObject(self.job,1):
            raise ctypes.WinError(ctypes.get_last_error())

    def wait_empty(self,seconds=5):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            if self.active()==0:
                return True
            time.sleep(.01)
        return self.active()==0

    def close(self):
        if self.event:
            self.k.CloseHandle(self.event);self.event=None
        if self.job:
            self.k.CloseHandle(self.job);self.job=None
