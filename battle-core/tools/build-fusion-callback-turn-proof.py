#!/usr/bin/env python3
"""Natural fused ability ownership using the retained native-form turn codec."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp)/'base';subprocess.run([sys.executable,str(root/'tools/build-native-form-turn-proof.py'),sys.argv[1],str(base)],check=True);s=base.read_text()
if sys.argv[1]=='producer':
 s=s.replace('NATIVEFORMTURN','FUSIONCALLBACKTURN')
 a='        holder.customPokemonData.ability=native[1];holder.summonData.ability=undefined;'
 assert s.count(a)==1
 s=s.replace(a,'''        holder.customPokemonData.ability=-1;holder.summonData.ability=undefined;
        holder.fusionSpecies=holder.species;holder.fusionCustomPokemonData=null;
        const choices=holder.species.forms.flatMap((f:any,k:number)=>[f.ability1,f.ability2,f.abilityHidden].map((ab:any,index:number)=>({k,index,ab}))).filter((f:any)=>f.ab===native[1]);
        if(!choices.length)throw Error("native fusion ability unavailable");const chosen=choices[state%choices.length];holder.fusionFormIndex=chosen.k;holder.fusionAbilityIndex=chosen.index;
''')
 # The unfused scaffold requires native quiet changes. Here NoFusionAbility
 # intentionally cancels most of those callbacks; require actual ownership
 # and full source capture instead, without forcing a disabled callback.
 a='    if(N>=512&&[0,1,2,3,4,7].some(k=>nativeChanges[k]===0))throw Error(`unreached native changes ${nativeChanges}`);'
 assert s.count(a)==1
 s=s.replace(a,'    if(stateCounts.reduce((a:number,b:number)=>a+b,0)!==N)throw Error("incomplete native fusion ownership matrix");')
 s=s.replace('actual native form full move turns','actual natural fusion callback full move turns')
elif sys.argv[1]!='replay':raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
