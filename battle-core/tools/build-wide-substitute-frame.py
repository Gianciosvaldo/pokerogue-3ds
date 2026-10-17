#!/usr/bin/env python3
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text();old='p->level = integer(v[4], 1, 255);';assert s.count(old)==1
Path(sys.argv[2]).write_text(s.replace(old,'p->level = integer(v[4], 1, INT32_MAX);',1))
