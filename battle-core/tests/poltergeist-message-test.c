/* Method-level source expectations; whole-turn ITEMS also validates the real caller. */
#define main established_turn_main
#include "turntest.c"
#undef main
int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) return 2;
    FILE *f = fopen(argv[1], "r"); if (!f) return 2;
    char line[4096], name[128], extra;
    if (!fgets(line, sizeof line, f) || strcmp(line, "POLTERGEIST_MESSAGES 1 25\n")) return 2;
    static pr_battle bt, before;
    int failures = 0;
    for (int ci = 0; ci < 25; ci++) {
        int id, side, null_target; unsigned int player_id, enemy_id;
        if (!fgets(line, sizeof line, f) || sscanf(line, "CASE %d %d %d %u %u %c", &id, &side, &null_target, &player_id, &enemy_id, &extra) != 5 || id != ci || side < 0 || side > 1 || null_target < 0 || null_target > 1) return 2;
        memset(&bt, 0, sizeof bt);
        pr_pokemon player = {.id=player_id, .species=SPECIES_ID_BULBASAUR, .is_player=true}, enemy = {.id=enemy_id, .species=SPECIES_ID_MAGIKARP};
        while (fgets(line, sizeof line, f) && !strncmp(line, "MOD ", 4)) {
            int s, berry, transferable, stacks; long long owner;
            if (sscanf(line, "MOD %d %127s %d %lld %d %d %c", &s, name, &berry, &owner, &transferable, &stacks, &extra) != 6 || s < 0 || s > 1 || bt.n_mods[s] >= PR_MAX_MODIFIERS || transferable < 0 || transferable > 1) return 2;
            int cls = pr_mod_class_by_name(name); if (cls < 0) return 2;
            bt.mods[s][bt.n_mods[s]++] = (pr_modifier){.cls=cls, .berry_type=berry, .pokemon_id=owner, .is_transferable=transferable, .stack_count=stacks};
        }
        int expected; if (sscanf(line, "RESULT %d %c", &expected, &extra) != 1 || expected < 0 || expected > 1) return 2;
        before = bt; pr_pokemon player_before = player, enemy_before = enemy; pr_msg msg = {0};
        bool result = pr_poltergeist_message(&bt, null_target ? NULL : side ? &enemy : &player, &msg);
        sbuf text = {{0},0}; sb(&text,"TEXT "); msg_json(&text, result ? &msg : &(pr_msg){0}); sb(&text,"\n");
        if (!fgets(line,sizeof line,f) || strncmp(line,"TEXT ",5)) return 2;
        if (result != expected || strcmp(line,text.s) || (result && !msg.key) || memcmp(&bt,&before,sizeof bt) || memcmp(&player,&player_before,sizeof player) || memcmp(&enemy,&enemy_before,sizeof enemy)) { printf("FAIL Poltergeist fixture %d\n",ci); failures++; }
        int end; if (!fgets(line,sizeof line,f) || sscanf(line,"END %d %c",&end,&extra)!=1 || end!=ci) return 2;
    }
    if (!fgets(line,sizeof line,f) || strcmp(line,"COMPLETE 25\n") || fgetc(f)!=EOF) return 2;
    fclose(f); printf("Poltergeist messages25 identical%d failures%d\n",25-failures,failures); return failures ? 1 : 0;
}
