"""Layer Physics Studio: use the existing native raylib Avatar Studio as the primary UI.

This adapter delegates to the proven local Studio launcher and never loads
private model bytes itself. No Git, remote network or upload operations.
"""
from __future__ import annotations

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from avatar_studio import main as run_native_studio


def main(argv: list[str] | None = None) -> int:
    return run_native_studio(sys.argv[1:] if argv is None else argv)


if __name__ == "__main__":
    raise SystemExit(main())
