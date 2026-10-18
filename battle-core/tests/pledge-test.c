/* Strict replay of native Pledge waiting callback observations. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
#include "../src/phases.c"

static void message_read(char *out, size_t cap)
{
    char *s = line_read("MSG "); s[strcspn(s, "\n")] = 0;
    size_t n = strlen(s);
    if (!n || n % 2 || strspn(s, "0123456789abcdef") != n || n / 2 >= cap) bad();
    for (size_t i = 0; i < n / 2; i++) {
        unsigned b; if (sscanf(s + 2*i, "%2x", &b) != 1 || !b) bad(); out[i] = b;
    }
    out[n/2] = 0;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) return 2;
    input = fopen(argv[1], "r"); if (!input) return 2;
    pr_closures_init();
    double h[3]; read_vector("PLEDGE ", h, 3); if(h[0]!=1&&h[0]!=2)bad(); integer(h[2],0,INT32_MAX);
    int n=integer(h[1],h[0]==2?12544:12288,100000),failures=0,operations=0,waited=0,declined=0,already=0,fainted_wait=0,separate_wait=0,modes[3]={0};
    for (int i = 0; i < n; i++) {
        static pr_battle bt; static pr_engine e; memset(&bt, 0, sizeof bt); memset(&e, 0, sizeof e);
        bt.engine = &e; bt.double_battle = true; e.current = -1; e.n_levels = 1;
        double c[6]; read_vector("C ", c, 6); if (c[0] != i) bad();
        int user=integer(c[1],0,3),move=integer(c[2],1,PR_MOVE_COUNT-1),count=integer(c[3],0,8),ops=integer(c[4],1,3);
        const int pledges[]={MOVE_ID_FIRE_PLEDGE,MOVE_ID_WATER_PLEDGE,MOVE_ID_GRASS_PLEDGE};int mode=-1;for(int j=0;j<3;j++)if(move==pledges[j])mode=j;if(mode<0)bad();modes[mode]++;
        int swap = integer(c[5], 0, 3);
        pr_pokemon actors[4] = {0};
        for (int k = 0; k < 4; k++) {
            double m[5]; read_vector("MON ", m, 5);
            actors[k].species = integer(m[0], 1, 65535); actors[k].form = integer(m[1], 0, 255);
            const pr_species *sp = pr_pokemon_species(&actors[k]); if (!sp || actors[k].form >= sp->nforms) bad();
            actors[k].hp = integer(m[2], 0, INT_MAX); actors[k].is_player = k < 2; actors[k].on_field = true;
            actors[k].acted = integer(m[3], 0, 1);
            actors[k].combining_pledge=integer(m[4],0,PR_MOVE_COUNT-1);
            actors[k].id = k + 1; bt.field[k] = &actors[k]; bt.party[k/2][k%2] = &actors[k];
        }
        bt.n_party[0] = bt.n_party[1] = 2;
        for (int j = 0; j < count; j++) {
            double m[3]; read_vector("MOVE ", m, 3); int who=integer(m[0],0,3),timing=integer(m[1],0,2),mm=integer(m[2],1,PR_MOVE_COUNT-1);
            int ph = pr_phase_new(&e, PH_MOVE, who); e.pool[ph].pokemon = &actors[who];
            e.pool[ph].timing = timing; e.pool[ph].move.id = mm; pr_phase_push(&bt, &e, ph);
        }
        for (int side = 0; side < 2; side++) if (swap & (1 << side)) {
            bt.field[side*2] = bt.party[side][0] = &actors[side*2+1];
            bt.field[side*2+1] = bt.party[side][1] = &actors[side*2];
        }
        rng_read("R ", &bt.rng); rng_read("B ", &bt.battle_rng); bt.rng_live = bt.battle_rng_live = true;
        bool exact = true;
        for (int op = 0; op < ops; op++) {
            double o[3]; read_vector("OP ",o,3);bool flag=integer(o[0],0,1),want=integer(o[1],0,1),override=integer(o[2],0,1);
            bool had=actors[user].combining_pledge!=0;const pr_phase *next=pr_find_move_phase(&e,-1,actors[user].is_player?1:0);bool distinct=next&&next->pokemon!=&actors[user];operations++;bool result=await_combined_pledge(&bt,&e,&actors[user],move);
            separate_wait+=want&&distinct;waited+=want;declined+=!want;already+=had;fainted_wait+=want&&actors[user].hp==0;
            exact&=result==want&&(flag||result)==override&&!bt.n_unported&&!e.overflow;
            double v[1]; read_vector("LEVELS ", v, 1); int levels = integer(v[0], 1, PR_MAX_LEVELS); exact &= levels == e.n_levels;
            for (int l = 0; l < levels; l++) {
                double q[2]; read_vector("L ", q, 2); if (q[0] != l) bad(); int len = integer(q[1], 0, PR_LEVEL_CAP);
                exact &= l < e.n_levels && len == e.level_len[l];
                for (int j = 0; j < len; j++) {
                    int ch = fgetc(input); if (ch == EOF || ungetc(ch, input) == EOF) bad();
                    const pr_phase *p = l < e.n_levels && j < e.level_len[l] ? &e.pool[e.levels[l][j]] : NULL;
                    if (ch == 'M' && count && j < count && l == 0) {
                        if (strcmp(line_read("MARKER "), "MovePhase\n")) bad();
                        exact &= p && p->type == PH_MARKER && p->marker_of == PH_MOVE;
                    } else {
                        char expected[2048], actual[2048]; message_read(expected, sizeof expected);
                        if (!p || p->type != PH_MESSAGE) { exact = false; continue; }
                        pr_text_source text = {locale_get, NULL, "en"}; pr_msg_format(&p->msg, &text, actual, sizeof actual);
                        exact &= !strcmp(expected, actual);
                    }
                }
            }
            read_vector("DYNAMIC ", v, 1); int nd = integer(v[0], 0, 8); if (nd != count) bad();
            for (int k = 0; k < PH_COUNT_; k++) exact &= e.n_dyn[k] == (k == PH_MOVE ? nd : 0);
            for (int j = 0; j < nd; j++) {
                double d[3]; read_vector("D ", d, 3); int who=integer(d[0],0,3),timing=integer(d[1],0,2),mm=integer(d[2],1,PR_MOVE_COUNT-1);
                if (j >= e.n_dyn[PH_MOVE]) { exact = false; continue; }
                const pr_phase *p = &e.pool[e.dyn[PH_MOVE][j]];
                exact &= p->type == PH_MOVE && p->pokemon == &actors[who] && p->timing == timing&&p->move.id==mm;
            }
            for(int k=0;k<4;k++){double v[3];read_vector("AFTER ",v,3);int hp=integer(v[0],0,INT_MAX),acted=integer(v[1],0,1),pledge=integer(v[2],0,PR_MOVE_COUNT-1);exact&=actors[k].hp==hp&&actors[k].acted==acted&&actors[k].combining_pledge==pledge;}
        }
        pr_rng r;rng_read("RO ",&r);exact&=rng_equal(bt.rng,r);rng_read("BO ",&r);exact&=rng_equal(bt.battle_rng,r);
        if (!exact) { if (failures < 12) fprintf(stderr, "Pledge case%d failed\n", i); failures++; }
    }
    double footer[1]; read_vector("PLEDGE_COMPLETE ", footer, 1); if (footer[0] != n || fgetc(input) != EOF) bad();
    if(!failures){if(h[0]>=2&&separate_wait<100)bad();if(waited<100||declined<100||already<100||fainted_wait<100)bad();for(int j=0;j<3;j++)if(modes[j]<100)bad();}
    printf("Pledge: %d complete API cases, %d operations, %d failures\n", n, operations, failures);
    return failures ? 1 : 0;
}
