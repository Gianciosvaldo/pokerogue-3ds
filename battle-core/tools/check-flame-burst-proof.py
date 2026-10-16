#!/usr/bin/env python3
"""Compile Flame Burst/downstream faults and reject malformed native observations."""
from pathlib import Path
import argparse,os,re,subprocess
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);p.add_argument('turns',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];core=root/'battle-core';out=core/'build/flame-burst-guards';out.mkdir(parents=True,exist_ok=True)
assert os.environ.get('TURNTEST_LOCALES')
code={name:(core/'src'/f'{name}.c').read_text() for name in ['move_effects','phases','switching']}
s=code['move_effects'];start=s.index('    case PR_AC_FlameBurstAttr:');end=s.index('    case PR_AC_MoneyAttr:',start);leaf=s[start:end]
def change(old,new):
 assert old in leaf;return ('move_effects',s[:start]+leaf.replace(old,new)+s[end:])
faults={
 'missing-handler':('move_effects',s[:start]+s[end:]),
 'parent-hp-gate':change('        pr_pokemon *ally', '        if (!effect_can_apply(ef, user, target)) return;\n        pr_pokemon *ally'),
 'skip-fainted-ally':change('if (cancelled || !ally || ally->switch_out_status)', 'if (cancelled || !ally || ally->hp <= 0 || ally->switch_out_status)'),
 'damage-target':change('(pr_pokemon *)pr_get_ally(bt, target)', 'target'),
 'half-denominator':change('/ 16.0','/ 8.0'),
 'round-up':change('floor(pr_get_stat','ceil(pr_get_stat'),
 'zero-minimum':change('damage < 1 ? 1 : damage', 'damage'),
 'direct-damage':change('HIT_RESULT_INDIRECT', 'HIT_RESULT_EFFECTIVE'),
 'ignore-faint':change('HIT_RESULT_INDIRECT, false, NULL', 'HIT_RESULT_INDIRECT, true, NULL'),
 'wrong-source':change('HIT_RESULT_INDIRECT, false, NULL', 'HIT_RESULT_INDIRECT, false, user'),
 'skip-blocking-ability':change('            pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);', '            (void)ap;'),
 'switch-check-before-ability':change('        if (ally) {','        if (ally && ally->switch_out_status) return;\n        if (ally) {'),
 'skip-singles':change('        pr_pokemon *ally', '        if (!bt->double_battle) return;\n        pr_pokemon *ally'),
 'missing-damage':change('        pr_damage_and_update(bt, e, ally, damage < 1 ? 1 : damage, HIT_RESULT_INDIRECT, false, NULL);','        (void)damage;'),
}
s=code['phases'];old='int roll = pr_rng_seed_int(&bt->rng, total, 0);';assert s.count(old)==1
faults['boss-battle-rng']=('phases',s.replace(old,'int roll = pr_battle_rand_seed_int(bt, total, 0);'))
old='''    /* The source still queues an empty phase when every stat is capped. */
    int ph = pr_unshift_stat_stage_change_ex(bt, e, pr_battler_index(bt, p), stats, stages, n, p, 0);
    if (ph >= 0) PH(ph)->ssc_ignore_abilities = true;''';assert s.count(old)==1
faults['skip-empty-boss-phase']=('phases',s.replace(old,'    if (!n) return;\n'+old))
s=code['switching'];refresh='        for (int k = 0; k < bt->n_party[1]; k++) pr_recalculate_stats(bt, bt->party[1][k]);';clear='        pr_clear_enemy_held_item_modifiers(bt, NULL);';assert s.count(refresh)==1 and s.count(clear)==1
faults['missing-flee-stat-refresh']=('switching',s.replace(refresh,''))
faults['missing-flee-item-clear']=('switching',s.replace(clear,''))
faults['only-clear-fleeing-owner']=('switching',s.replace(clear,'        pr_clear_enemy_held_item_modifiers(bt, p);'))
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(core/'include'),'-I'+str(core/'gen/out')]
for name,(file,source) in faults.items():
 assert source!=code[file];src,obj,exe=(out/(name+suffix) for suffix in ['.c','.o','']);src.write_text(source)
 subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc',*flags,str(core/'tests/flame-burst-test.c'),str(obj),str(core/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe),str(a.trace.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
 if name=='missing-damage':
  exe=out/'missing-damage-turns';subprocess.run(['gcc',*flags,str(core/'tests/doublestest.c'),str(obj),str(core/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),str(a.turns.resolve())],capture_output=True,text=True);(out/'missing-damage-turns.log').write_text(r.stdout+r.stderr);m=re.search(r'^covered \d+: pass \d+, fail (\d+)',r.stdout,re.M);assert r.returncode==1 and m and int(m[1])>0;print('owning-turn failures',m[1],flush=True)
rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':['FLAMEBURST 2 16384 777']+rows[1:],'wrong-footer':rows[:-1]+['FLAMEBURST_COMPLETE 1']}
for name,prefix,col,value in [('fractional-id','C ',1,'0.5'),('invalid-holder','C ',2,'4'),('invalid-target','C ',3,'-1'),('invalid-double','C ',4,'2'),('invalid-gas-count','G ',1,'4'),('overflow-owner','G ',2,'4294967296'),('invalid-mod-count','MODS ',1,'4'),('invalid-mod-side','M ',1,'2'),('invalid-mod-stack','M ',3,'-1'),('invalid-actor-slot','I ',1,'4'),('invalid-level','I ',6,'256'),('invalid-hp','I ',8,'-1'),('nonfinite-hp','I ',8,'nan'),('invalid-boss-index','IX ',3,'5'),('invalid-endure','IX ',4,'2'),('invalid-result','RESULT ',1,'2'),('invalid-queue-count','QCOUNT ',1,'129'),('invalid-level-count','QCOUNT ',2,'0'),('invalid-deferred','QCOUNT ',3,'2'),('fractional-phase-data','P ',1,'0.5')]:
 j=next(i for i,row in enumerate(rows) if row.startswith(prefix));parts=rows[j].split();parts[col]=value;lines=rows.copy();lines[j]=' '.join(parts);bad[name]=lines
for name,lines in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(core/'build/flame-burst-test'),str(path)],capture_output=True,text=True);assert r.returncode==2,(name,r.returncode);print(name,'rejected',flush=True)
print(f'{len(faults)} compiled faults caught; {len(bad)} malformed captures rejected')
