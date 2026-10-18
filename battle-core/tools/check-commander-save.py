#!/usr/bin/env python3
"""Catch loss of Commander cache persistence and malformed-index validation."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2];game=root/'game-core';out=root/'battle-core/build/commander-save-guards';out.mkdir(parents=True,exist_ok=True)
s=(game/'src/gc_save.c').read_text()
mutations={
 'missing-cache-field':('    SF(pr_battler_tag, commanded_form_key),\n',''),
 'unchecked-cache-index':('        for (int j = 0; j < b->n_tags; j++)\n            if (b->tags[j].commanded_form_key > PR_FORM_COUNT) return false;\n',''),
}
flags=['-O2','-std=c99','-ffp-contract=off']
flags+=['-I'+str(root/p) for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]
libs=[game/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
for name,(old,new) in mutations.items():
 assert s.count(old)==1,name
 src,obj,exe=(out/(name+suffix) for suffix in ['.c','.o','']);src.write_text(s.replace(old,new))
 subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc',*flags,str(game/'tests/commander_host_test.c'),str(obj),*map(str,libs),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe)],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
print('2 save faults caught')
