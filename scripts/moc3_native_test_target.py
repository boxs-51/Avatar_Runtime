"""Native test custody: explicit configured targets are required, never optional."""
import os
from pathlib import Path

def configured_target(variable, fallback):
    value=os.environ.get(variable)
    if value is None:
        return Path(fallback)  # Standalone diagnostic may have no optional build.
    candidate=Path(value)
    if not value or not candidate.is_absolute():
        raise ValueError(variable+' must name an absolute configured executable')
    if not candidate.is_file():
        raise FileNotFoundError(variable+' configured executable is missing: '+value)
    return candidate
