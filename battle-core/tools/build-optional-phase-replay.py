from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
anchor='        for (int k = 0; k < e->n_dyn[t]; k++) sb(&q, k ? "/%d" : "%d", e->pool[e->dyn[t][k]].battler);'
assert s.count(anchor)==1
s=s.replace(anchor,'''        for (int k = 0; k < e->n_dyn[t]; k++) {
            const pr_phase *p = &e->pool[e->dyn[t][k]];
            if(k) sb(&q,"/");
            if(optional_phase_absent(p)) sb(&q,"A");
            else sb(&q,"I%d",p->battler);
        }''')
Path(sys.argv[2]).write_text(s)
