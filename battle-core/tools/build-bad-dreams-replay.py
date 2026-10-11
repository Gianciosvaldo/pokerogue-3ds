from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text();anchor='    if (ss) {'
if s.count(anchor)!=1:raise SystemExit('Bad Dreams comparator anchor changed')
Path(sys.argv[2]).write_text(s.replace(anchor,'    if (true) { /* observe zero overrides too */',1))
