#!/usr/bin/env python3
"""Entry-point shim for optionsplus_extractor.cli.

The real implementation lives in scripts/optionsplus_extractor/. This
shim stays in place so existing docs and CI paths keep working.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from optionsplus_extractor.cli import main

if __name__ == "__main__":
    sys.exit(main())
