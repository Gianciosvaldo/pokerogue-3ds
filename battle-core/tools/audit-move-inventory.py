#!/usr/bin/env python3
# Source registration/inheritance and bounded switch audit, not semantic completeness.
from pathlib import Path
import argparse,collections,gzip,json,re,subprocess
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'battle-core'
PIN='8555c08c823b856cbec4eb99ca84ea52a955836d'
p=argparse.ArgumentParser();p.add_argument('--dump',type=Path);p.add_argument('--capture',type=Path);p.add_argument('--write',action='store_true');a=p.parse_args()
def unique(pairs):
 d={}
 for k,v in pairs:assert k not in d,('duplicate key',k);d[k]=v
 return d
path=a.capture or CORE/'inventory/move-attributes.source.json.gz'
x=json.loads(gzip.decompress(path.read_bytes()),object_pairs_hook=unique)
assert set(x)=={'version','sourcePin','moves'}and x['version']==1 and x['sourcePin']==PIN
moves=x['moves'];assert len(moves)==921
classes={};active=collections.Counter();disabled=0
for i,m in enumerate(moves):
 assert set(m)=={'id','unimplemented','attrs','chargeAttrs'}and type(m['id'])is int and m['id']==i and type(m['unimplemented'])is bool
 disabled+=m['unimplemented']
 for key in ['attrs','chargeAttrs']:
  assert type(m[key])is list
  for chain in m[key]:
   assert type(chain)is list and chain and len(set(chain))==len(chain)and all(type(n)is str and re.fullmatch(r'\w+',n)for n in chain)
   assert chain[-1]=='MoveAttr'
   if chain[0]in classes:assert classes[chain[0]]==chain
   classes[chain[0]]=chain
   if not m['unimplemented']:active[chain[0]]+=1
assert disabled==86 and len(active)==196 and sum(active.values())==982
binary=(a.dump or CORE/'build/move-inventory-dump').resolve()
if not a.dump:subprocess.run(['make','-s','-C',str(CORE),'build/move-inventory-dump'],check=True)
lines=subprocess.check_output([str(binary)],text=True).splitlines();seen={};current=-1
for line in lines:
 row=line.split();tag=row.pop(0)
 if tag=='M':
  assert len(row)==2;index,flag=map(int,row);assert index==current+1 and flag in(0,1);current=index
  assert index<921 and bool(flag)==moves[index]['unimplemented'];seen[index]=[[],[]]
 else:
  assert tag=='A'and len(row)>=6
  index,charge,ordinal=map(int,row[:3]);name=row[3];parents=row[4:]
  assert index==current and charge in(0,1)and ordinal==len(seen[index][charge])
  native=moves[index]['chargeAttrs'if charge else'attrs'][ordinal]
  assert name==native[0]and len(parents)==len(set(parents))and set(parents)==set(native),(index,charge,ordinal,'inheritance/order differs')
  seen[index][charge].append(name)
assert current==920
for m in moves:
 for ch,key in enumerate(['attrs','chargeAttrs']):assert seen[m['id']][ch]==[c[0]for c in m[key]],(m['id'],key,'lost attribute')
owner_files=sorted(list((CORE/'src').glob('*.c'))+list((ROOT/'ai-core/src').glob('*.c'))+list((ROOT/'game-core/src').glob('*.c')))
owners={n:[str(f.relative_to(ROOT))for f in owner_files if re.search(r'\bPR_AC_'+n+r'\b',f.read_text())]for n in active}
assert {n for n,v in owners.items()if not v}=={'HitsTagForDoubleDamageAttr'}
assert classes['HitsTagForDoubleDamageAttr']==['HitsTagForDoubleDamageAttr','HitsTagAttr','MoveAttr']
assert 'pr_attr_is(a->cls, PR_AC_HitsTagAttr)'in(CORE/'src/move_attrs.c').read_text()
contracts=[('move_attrs.c',n,b)for n,b in[
 ('pr_apply_variable_power','VariablePowerAttr'),('pr_apply_variable_move_type','VariableMoveTypeAttr'),
 ('pr_apply_type_chart_override','MoveTypeChartOverrideAttr'),('pr_apply_variable_move_category','VariableMoveCategoryAttr'),
 ('pr_apply_variable_atk','VariableAtkAttr'),('pr_apply_variable_def','VariableDefAttr'),
 ('pr_apply_fixed_damage','FixedDamageAttr'),('pr_apply_modified_damage','ModifiedDamageAttr')]]
contracts.append(('phases.c','move_header_phase','MoveHeaderAttr'));counts=[]
for file,symbol,base in contracts:
 code=(CORE/'src'/file).read_text()
 masked=re.sub(r'/\*.*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"',lambda m:' '*len(m[0]),code,flags=re.S)
 matches=list(re.finditer(r'\b'+symbol+r'\([^;{}]*\)\s*\{',masked));assert len(matches)==1,symbol
 start=matches[0].end();depth=1;end=start
 while depth:depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
 body=code[start:end-1]
 assert '!pr_attr_is(a->cls, PR_AC_'+base+')) continue'in body and 'switch (a->cls)'in body
 cases=set(re.findall(r'case PR_AC_(\w+)',body));registered={n for n in active if base in classes[n]}
 assert registered<=cases,(symbol,sorted(registered-cases))
 counts.append((symbol,base,len(registered)))
rows='''# Registered move attribute source inventory

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`. All **921** move registrations,
**86** source-disabled flags and ordered normal/charge attribute instances match
the C tables, including complete class inheritance bitmaps. The **835 enabled**
entries contain **982 instances across 196 most-derived classes** (NONE is included
in the registry count). This proves registration transport, not semantic parity.

Nine reviewed family switches account for every registered enabled subclass
admitted by their filters:

| C owner | Source family | Enabled concrete classes |
| --- | --- | --- |
'''
for symbol,base,count in counts:rows+=f'| `{symbol}` | `{base}` | {count} |\n'
rows+='''
The switch check proves class exhaustiveness only. It does not establish arithmetic,
conditions/chance, target lookup, queues, text, RNG, owning callers or interactions.
Those still require their native proofs and the unsupported-path ledger.

`HitsTagForDoubleDamageAttr` uses inherited `HitsTagAttr` dispatch in
`pr_hits_tag_multiplier`; its missing most-derived token is not an absent mechanic.
Other enabled names have source references below. These are candidate owner
references, not semantic sign-offs, and may include AI or constructor callers.

| Enabled class | Instances | Candidate source references |
| --- | --- | --- |
'''
for name,count in sorted(active.items()):
 refs=owners[name]or['battle-core/src/move_attrs.c (inherited HitsTagAttr)']
 rows+=f'| `{name}` | {count} | '+', '.join('`'+s+'`'for s in refs)+' |\n'
rows+='''
Five compiled table faults and eight malformed/forged source captures are
rejected; a pristine control passes. Fresh native reproduction is byte-identical.
Run `python3 battle-core/tools/check-move-inventory-guards.py` for those controls.

Reproduce: `python3 battle-core/tools/audit-move-inventory.py` and
`battle-core/tools/reproduce-move-inventory.sh PINNED_UPSTREAM OUTPUT_DIR`.
The native producer observes initialized registrations and retains disabled
entries; it never enables a source-disabled move. Closure identity, held-class and
capacity audits remain separate. This record does not close the broader source
review or certify battle-core completion. Only dedicated Task63 is excluded;
normal core/app/custom/save/football and actual 3DS/ARM checks remain merge gates.
'''
target=CORE/'MOVE-ATTRIBUTE-INVENTORY.md'
if not a.capture:
 if a.write:target.write_text(rows)
 else:assert target.read_text()==rows,'review and regenerate source inventory table'
print('Move inventory: 921 registrations, 86 disabled, 982 enabled instances/196 classes; 9 family switches exhaustive over registered inputs (not semantic completion)')
