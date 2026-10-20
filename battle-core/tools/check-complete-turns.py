#!/usr/bin/env python3
"""Require complete native turn parity, including coverage and text, from a runner.

The general doubles runner also supports historical partially implemented corpora.
A complete owning-caller proof must separately refuse any uncovered native case.
"""
import argparse,re,subprocess,sys
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('runner');p.add_argument('trace');p.add_argument('--count',type=int,default=8192)
a=p.parse_args()
if a.count<1:p.error('--count must be positive')
n=a.count
with open(a.trace,encoding='utf-8') as f:
 header=f.readline().rstrip('\r\n');seen=0;last=header
 if header!=f'# DOUBLES_CONTRACT 1 {n}':
  print('Incomplete native turn proof: wrong source contract/count',file=sys.stderr);sys.exit(1)
 for line in f:
  last=line.rstrip('\r\n')
  if line.startswith('CASE '):
   if last!=f'CASE {seen}':
    print('Incomplete native turn proof: missing, duplicate or reordered native CASE ID',file=sys.stderr);sys.exit(1)
   seen+=1
 if seen!=n or last!=f'# DOUBLES_COMPLETE {n} 0':
  print('Incomplete native turn proof: missing native cases or nonzero source exceptions',file=sys.stderr);sys.exit(1)
r=subprocess.run([a.runner,a.trace],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
s=r.stdout;sys.stdout.write(s)
if r.returncode:sys.exit(r.returncode if r.returncode>0 else 1)
n=a.count
checks={
 'empty native exception inventory':r'^# upstream threw \(case left out\): \{\}$',
 'native footer without exceptions':rf'^# DOUBLES_COMPLETE {n} 0$',
 'all cases parsed':rf'^cases {n} \(skipped 0: records not read yet\), parse mismatches 0, stats mismatches 0$',
 'all cases covered and exact':rf'^covered {n}: pass {n}, fail 0; not covered 0$',
 'all text exact':rf'^texts of covered turns {n}: identical {n}, differ 0, a message with no text yet 0$',
 'all requested native turns present':rf'^doubles cases {n} of {n} requested \(upstream threw on 0\)$',
}
missing=[label for label,pattern in checks.items() if not re.search(pattern,s,re.M)]
if missing:
 print('Incomplete native turn proof: '+', '.join(missing),file=sys.stderr)
 sys.exit(1)
print(f'Complete native turn proof: all {n} cases exact, covered and present')
