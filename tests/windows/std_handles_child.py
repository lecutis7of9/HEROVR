"""The session's first program start, as 0.0.70's xr_runtime.probe made it (stdin not given). argv: tools folder,
'1' to run std_handles.repair() first. Prints STARTED [repaired] or FAILED <error>."""
import subprocess
import sys
sys.path.insert(0, sys.argv[1])
import std_handles  # noqa: E402
fixed = std_handles.repair() if sys.argv[2] == '1' else []
try:
    subprocess.run(['cmd', '/c', 'echo', 'hi'], capture_output=True, timeout=30)
    print('STARTED', fixed)
except OSError as error:
    print('FAILED', error)
