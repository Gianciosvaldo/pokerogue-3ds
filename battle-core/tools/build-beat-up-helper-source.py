#!/usr/bin/env python3
"""Add actual count and per-strike power probes to the generated owning source fixture."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
anchor='    const out:string[]=[];'
assert s.count(anchor)==1
s=s.replace(anchor,'''    if(process.env.BEATUP_HELPERS==="1"){
        const rows:string[]=[];let cases=0,powerReads=0;
        const species=[SpeciesId.SLAKING,SpeciesId.ONIX,SpeciesId.SHUCKLE,SpeciesId.ALAKAZAM,SpeciesId.RATTATA,SpeciesId.DEOXYS];
        for(const side of [0,1])for(let size=1;size<=6;size++)for(let pattern=0;pattern<6;pattern++){
            const party=side?scene.getEnemyParty():scene.getPlayerParty(),pool=side?[D,...enemyBench]:[A,...playerBench];
            party.length=0;party.push(...pool.slice(0,size));
            const userIndex=pattern%size,user=party[userIndex],target=side?A:D;
            party.forEach((p:any,k:number)=>{
                p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(species[k]);p.formIndex=0;p.fusionSpecies=null;p.hp=100;p.status=null;
                p.summonData.ability=AbilityId.BALL_FETCH;p.passive=false;
                if(pattern===1 && k!==userIndex)p.status=new Status(StatusEffect.POISON);
                if(pattern===2 && k%2===0)p.status=new Status(StatusEffect.PARALYSIS);
                if(pattern===3 && k!==userIndex)p.hp=0,p.status=new Status(StatusEffect.FAINT);
                if(pattern===4)p.hp=0;
                if(pattern===5){if(k===5)p.formIndex=1;p.fusionSpecies=speciesDataRegistry.getSpecies(SpeciesId.ONIX);p.fusionFormIndex=0;p.fusionAbilityIndex=0;}
            });
            const rngBefore=[Phaser.Math.RND.state(),battle.battleSeedState].join("/");
            const count=(allMoves[MoveId.BEAT_UP].getAttrs("MultiHitAttr")[0] as any).getHitCount(user,target);
            rows.push(`B ${cases} ${side} ${size} ${userIndex} ${count} ${count+2}`);
            party.forEach((p:any,k:number)=>rows.push(`M ${k} ${p.id} ${p.species.speciesId} ${p.formIndex} ${p.fusionSpecies?.speciesId??0} ${p.hp} ${p.status?.effect??0}`));
            for(let hit=0;hit<count+2;hit++){
                user.turnData.hitCount=count;user.turnData.hitsLeft=count-hit;
                const power=new NumberHolder(-1);
                (allMoves[MoveId.BEAT_UP].getAttrs("BeatUpAttr")[0] as any).apply(user,target,allMoves[MoveId.BEAT_UP],[power]);
                rows.push(`P ${hit} ${power.value}`);powerReads++;
            }
            if(rngBefore!==[Phaser.Math.RND.state(),battle.battleSeedState].join("/"))throw new Error("Beat Up probes advanced RNG");
            rows.push("E");cases++;
        }
        fs.writeFileSync(OUT,`BEATUP_HELPERS 1 ${cases} ${powerReads}\\n`+rows.join("\\n")+`\\nBEATUP_HELPERS_COMPLETE ${cases} ${powerReads}\\n`);
        console.log(`BEATUP actual count helpers=${cases} power probes=${powerReads}`);return;
    }
    const out:string[]=[];''',1)
Path(sys.argv[2]).write_text(s)
