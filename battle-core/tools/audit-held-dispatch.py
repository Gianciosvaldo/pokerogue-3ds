#!/usr/bin/env python3
"""Audit source-class mappings and evidence references, not semantic coverage."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'battle-core'
PIN='8555c08c823b856cbec4eb99ca84ea52a955836d'
def unique(pairs):
 out={}
 for k,v in pairs:
  assert k not in out,('duplicate JSON key',k)
  out[k]=v
 return out
def read(path):return json.loads(path.read_text(),object_pairs_hook=unique)
def reference(path):
 full=(ROOT/path).resolve();full.relative_to(ROOT);assert full.is_file(),path
 return full
def capture(upstream):
 def git(*args):return subprocess.check_output(['git','-C',str(upstream),*args],text=True).strip()
 assert git('rev-parse','HEAD')==PIN,'wrong upstream pin'
 assert not git('diff','--name-only','--','src'),'modified upstream source'
 path='src/modifier/modifier.ts';text=(upstream/path).read_text()
 classes={m.group(2):{'name':m.group(2),'parent':m.group(3),'abstract':bool(m.group(1)),'line':text.count('\n',0,m.start())+1}for m in re.finditer(r'export\s+(abstract\s+)?class\s+(\w+)\s+extends\s+(\w+)',text)}
 def held(name):return name=='PokemonHeldItemModifier' or name in classes and held(classes[name]['parent'])
 return {'version':1,'source_pin':PIN,'source_file':path,'sha256':hashlib.sha256(text.encode()).hexdigest(),'classes':[r for name,r in classes.items()if name!='PokemonHeldItemModifier'and held(name)]}
def relative(path):return os.path.relpath(ROOT/path,CORE)
def render(source,rows):
 text='''# Current pinned held-item dispatch inventory

Reconciled on 2026-10-26. Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`,
`src/modifier/modifier.ts`. The inventory has **33 transitive subclasses**:
**31 concrete classes and two abstract bases**. All 31 concrete names have C
registry mappings. This is a class/owner-reference inventory, **not a semantic
completion count**. It does not establish every input shape or interaction.

The [previous Card 104 snapshot](../docs/history/HELD-ITEM-INVENTORY-2026-10-26.md)
is historical. Its missing-class/helper-only rows have been superseded by the
proofs below. In particular, Baton, Grip Claw, Mini Black Hole, held EXP and
friendship classes are implemented. Form-item acquisition/party actions and the
Macho Brace stat/Victory/capture owners also have their own merged evidence.

The [source capture](inventory/held-classes.source.json) records class headers,
parents, abstract status and the pinned file hash. The [reviewed evidence
ledger](inventory/held-evidence.json) links actual owner references and bounded
proofs. The audit checks C registry names, inherited held-class relationships,
owner symbols, reference files and this rendered table. File/symbol existence
is not a substitute for the native comparisons described in each proof.

| Pinned class | C mapping and owner reference | Bounded evidence and remaining scope |
| --- | --- | --- |
'''
 for r in rows:
  owners='; '.join(f'[{o["symbol"]}]({relative(o["path"])})'for o in r['owners'])
  mapping=('`'+r['c_class']+'`'if r['c_class']else 'Abstract; no standalone C class')
  proofs='; '.join(f'[{Path(p).stem.replace("-VALIDATION", "")}]({relative(p)})'for p in r['evidence'])
  text+=f'| `{r["name"]}` | {mapping}'+(f'; {owners}'if owners else '')+' | '+(proofs+'. 'if proofs else '')+r['boundary']+' |\n'
 text+='''
## Cross-cutting owners and boundaries

- [Scene transfer](SCENE-TRANSFER-VALIDATION.md), [stat-item cloning](STAT-LOOT-VALIDATION.md),
  [Pickup](PICKUP-VALIDATION.md), [fused eligibility](FUSED-HELD-VALIDATION.md),
  [constructor locks](../game-core/HELD-LOCK-VALIDATION.md) and
  [inventory capacity](INVENTORY-CAPACITY-VALIDATION.md) supersede the corresponding
  older exclusions. Their own raw/transient/identity bounds remain explicit.
- The source has no concrete descendant of `LapsingPokemonHeldItemModifier` at
  this pin. The abstract transfer base's shared behavior is represented by the
  common C transfer implementation and its two concrete classes. Neither absent
  abstract registry name is counted as a missing enabled concrete mechanic.
- The ordinary item-turn producer's basic held-item evidence uses `TURN_ITEMS=1`.
  Named stock/transfer evidence does not automatically prove every battle effect,
  and a raw helper alone does not establish its owning phase or acquisition path.
- Source constructors permit inputs beyond canonical factory caps. The C
  admission checks and explicit unknown/unsupported paths remain necessary;
  registry membership does not waive those checks.
- Fusion evolution selection, arbitrary mixed cleanup/Transform histories,
  mystery event choices and whole-run/hardware acceptance are separate caller
  obligations. Mystery encounters/rivals have another owner. They are not closed
  by this index or by a large bounded turn sample.

Enemy tokens and side-wide persistent modifiers are not held subclasses and
remain outside this table. Move/ability attributes, constructor closures,
battler/arena tags, phases and their capacity paths still need their own source
inventory reconciliation in the [completion checklist](COMPLETION-CHECKLIST.md).

## Reproduction

```sh
# Check the retained source inventory against current C and evidence references:
python3 battle-core/tools/audit-held-dispatch.py
# Also reproduce source facts from a clean checkout of the exact pin:
python3 battle-core/tools/audit-held-dispatch.py --upstream /path/to/upstream
# After reviewing a ledger change, regenerate this table:
python3 battle-core/tools/audit-held-dispatch.py --write
```

This audit changes no battle dispatch, admission rule, capture, expected answer,
covered ID, save schema or test exclusion. Normal app/custom/save/football and
3DS/stack checks remain merge requirements; only the dedicated Task63 check is
skipped. It is not a battle-core completion claim.
'''
 return text
args=argparse.ArgumentParser();args.add_argument('--upstream',type=Path);args.add_argument('--write',action='store_true');a=args.parse_args()
source=read(CORE/'inventory/held-classes.source.json');assert set(source)=={'version','source_pin','source_file','sha256','classes'}and source['version']==1 and source['source_pin']==PIN
assert source['source_file']=='src/modifier/modifier.ts'and re.fullmatch('[0-9a-f]{64}',source['sha256'])
if a.upstream:assert capture(a.upstream)==source,'pinned source capture differs'
classes=source['classes'];assert len(classes)==33 and len({r['name']for r in classes})==33
assert all(set(r)=={'name','parent','abstract','line'}and type(r['abstract'])==bool and type(r['line'])==int and r['line']>0 for r in classes)
abstract={r['name']for r in classes if r['abstract']};assert abstract=={'LapsingPokemonHeldItemModifier','HeldItemTransferModifier'}
source_parents={r['name']:r['parent']for r in classes};assert not any(r['parent']=='LapsingPokemonHeldItemModifier'for r in classes)
ledger=read(CORE/'inventory/held-evidence.json');assert set(ledger)=={'version','meaning','rows'}and ledger['version']==1;rows=ledger['rows'];assert [r['name']for r in rows]==[r['name']for r in classes]
code=(CORE/'src/modifiers.c').read_text();names=code.split('pr_mod_class_name[PR_MOD_COUNT_] = {',1)[1].split('\n};',1)[0];pairs=re.findall(r'\[(PR_MOD_\w+)\]\s*=\s*"([^"]+)"',names);assert len({n for c,n in pairs})==len(pairs)
by_name={n:c for c,n in pairs};parents=dict(re.findall(r'\[(PR_MOD_\w+)\]\s*=\s*(PR_MOD_\w+)',code.split('pr_mod_parent[PR_MOD_COUNT_] = {',1)[1].split('\n};',1)[0]))
for r in rows:
 assert set(r)=={'name','c_class','owners','evidence','boundary'}and isinstance(r['boundary'],str)and r['boundary']
 name=r['name']
 if name in abstract:assert r['c_class']is None and name not in by_name
 else:
  assert by_name.get(name)==r['c_class'],('missing/wrong C registry mapping',name)
  parent=source_parents[name]
  while parent not in by_name:parent=source_parents[parent]
  assert parents[r['c_class']]==by_name[parent],('wrong inherited held class',name)
  assert r['owners']and r['evidence'],('missing owner/proof reference',name)
 for own in r['owners']:assert set(own)=={'path','symbol'}and own['symbol']in reference(own['path']).read_text(),('missing owner symbol',name,own)
 for path in r['evidence']:reference(path)
document=render(source,rows);target=CORE/'HELD-ITEM-INVENTORY.md'
if a.write:target.write_text(document)
else:assert target.read_text()==document,'inventory table differs: review ledger and run --write'
print('Held dispatch inventory: 33 source subclasses, 31 concrete C mappings, 2 abstract bases; owner/evidence references verified (not semantic completion)')
