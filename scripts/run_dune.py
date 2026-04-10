#!/usr/bin/env python3
"""Wrapper script for running dune commands reliably.

Usage:
    python scripts/run_dune.py build @all-apps
    python scripts/run_dune.py exec ./packages/.../test.exe
    python scripts/run_dune.py clean

The timeout defaults to 300 seconds and can be configured via the
DUNE_TIMEOUT environment variable (in seconds).

This script was created specifically for running Dune inside an AI orchestration agent.
"""

import os
import subprocess
import sys


def main() -> int:
    timeout = int(os.environ.get("DUNE_TIMEOUT", "300"))
    args = sys.argv[1:]

    if not args:
        print("Usage: python scripts/run_dune.py <dune args...>", file=sys.stderr)
        return 1

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cmd = ["dune"] + args

    try:
        result = subprocess.run(
            cmd,
            cwd=repo_root,
            timeout=timeout,
        )
        return result.returncode
    except subprocess.TimeoutExpired:
        print(
            f"dune timed out after {timeout}s (set DUNE_TIMEOUT to adjust)",
            file=sys.stderr,
        )
        return 124
    except FileNotFoundError:
        print("dune not found in PATH", file=sys.stderr)
        return 127


if __name__ == "__main__":
    sys.exit(main())
