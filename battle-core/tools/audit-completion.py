#!/usr/bin/env python3
"""Locate explicit remaining-path evidence; this is not a semantic coverage count."""
from pathlib import Path
import argparse,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument('upstream',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2]
def git(path,*args):return subprocess.check_output(['git','-C',str(path),*args],text=True).strip()
assert git(a.upstream,'rev-parse','HEAD')=='8555c08c823b856cbec4eb99ca84ea52a955836d'
assert not git(a.upstream,'diff','--name-only')
report={'source_pin':git(a.upstream,'rev-parse','HEAD'),'port_head':git(root,'rev-parse','HEAD'),'port_tracked_changes':git(root,'status','--porcelain','--untracked-files=no').splitlines(),'meaning':'Static candidate locations, not distinct missing mechanics or completion percentages. Each requires source/caller review.','candidates':[]}
for path in sorted((root/'battle-core/src').glob('*.c')):
 for i,line in enumerate(path.read_text().splitlines(),1):
  if re.search(r'pr_note_unported\s*\(|pr_defer_unported\s*\(|PR_FEAT_',line):
   report['candidates'].append({'file':str(path.relative_to(root)),'line':i,'code':line.strip()})
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(f"Recorded {len(report['candidates'])} candidate source lines; semantic classification remains required")
