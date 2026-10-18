"""Windows process lifetime peaks; measurements are private local evidence.

Layout/API: Microsoft PROCESS_MEMORY_COUNTERS_EX / GetProcessMemoryInfo.
https://learn.microsoft.com/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex
"""

import ctypes
from ctypes import wintypes
import os


class Counters(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
            "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage",
            "PagefileUsage", "PeakPagefileUsage", "PrivateUsage")]


def process_memory(pid: int | None = None) -> dict[str, int]:
    if os.name != "nt":
        raise OSError("Windows memory proof required")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
    psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
    handle = kernel.OpenProcess(0x1000, False, pid or os.getpid())
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        value = Counters()
        value.cb = ctypes.sizeof(value)
        if not psapi.GetProcessMemoryInfo(handle, ctypes.byref(value), value.cb):
            raise ctypes.WinError(ctypes.get_last_error())
        return {"peak_commit_bytes": value.PeakPagefileUsage,
                "commit_bytes": value.PrivateUsage,
                "peak_working_set_bytes": value.PeakWorkingSetSize}
    finally:
        kernel.CloseHandle(handle)
