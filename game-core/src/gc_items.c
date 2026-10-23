/* The player's items and the enemy's (upstream's modifier lists), and what game-core itself reads from
 * them. What each item does when taken or used is reward-core's; turning the lists into battle-core's
 * modifier entries (bt->mods) covers the classes battle-core knows, and an item it doesn't know goes in
 * as a plain held item or persistent modifier, which battle-core reports where it would matter. */
#include "internal.h"

int gc_item_stack(const gc_run *run, const char *key)
{
    int n = 0;
    for (int i = 0; i < run->n_items; i++)
        if (!strcmp(run->items[i].key, key)) n += run->items[i].stack;
    return n;
}

/* addModifier: a matching entry (same key, arg and Pokemon) stacks, else a new entry at the end */
void gc_item_add(gc_run *run, const char *key, int16_t arg, uint32_t pokemon_id, int stack)
{
    for (int i = 0; i < run->n_items; i++) {
        gc_item *it = &run->items[i];
        if (!strcmp(it->key, key) && it->arg == arg && it->pokemon_id == pokemon_id && !it->eviolite_base) { it->stack += stack; return; }
    }
    if (run->n_items >= PR_MAX_MODIFIERS) {pr_note_unported(&run->bt,PR_FEAT_ENGINE_LIMIT);return;}
    gc_item *it = &run->items[run->n_items++];
    it->key = key;
    it->arg = arg;
    it->pokemon_id = pokemon_id;
    it->detached = false;
    it->virtual_stack = 0;
    it->battle_count = 0;
    it->non_transferable = !strcmp(key, "EVOLUTION_TRACKER_GIMMIGHOUL");
    it->type_tier = 0;
    it->type_ref = 0;
    it->loot_ref = 0;
    it->eviolite_base = false;
    it->stack = stack;
}

void gc_item_remove_at(gc_run *run, int idx)
{
    if (idx < 0 || idx >= run->n_items) return;
    for (int i = idx; i < run->n_items - 1; i++) {
        run->items[i] = run->items[i + 1];
    }
    run->n_items--;
    memset(&run->items[run->n_items], 0, sizeof(gc_item));
}

int gc_item_remove_stacks(gc_run *run, const char *key, int count)
{
    int removed = 0;
    for (int i = 0; i < run->n_items && count > 0; ) {
        if (!strcmp(run->items[i].key, key) || (strstr(key, "BERRY") && strstr(run->items[i].key, "BERRY"))) {
            if (run->items[i].stack <= count) {
                int take = run->items[i].stack;
                count -= take;
                removed += take;
                gc_item_remove_at(run, i);
                continue;
            } else {
                run->items[i].stack -= count;
                removed += count;
                count = 0;
                break;
            }
        }
        i++;
    }
    return removed;
}

/* ExpBoosterModifier.apply for each Exp. Charm kind, in list order: floor(exp * (1 + stacks * boost)) */
double gc_apply_exp_boosters(const gc_run *run, double exp)
{
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        double b = 0;
        if (!strcmp(it->key, "EXP_CHARM")) b = 0.25;
        else if (!strcmp(it->key, "SUPER_EXP_CHARM")) b = 0.60;
        else if (!strcmp(it->key, "GOLDEN_EXP_CHARM")) b = 1.00;
        if (b > 0) exp = floor(exp * (1 + it->stack * b));
    }
    return exp;
}

/* One gc_item as battle-core's modifier. The held items and tokens battle-core reads on the damage path
 * get their class; everything else is a PokemonHeldItemModifier or PersistentModifier with
 * unknown_subclass set, so a query that would call its own apply reports itself unported. */
static bool golden_bridge(const gc_run *run)
{
    for (int side = 0; side < 2; side++) {
        const gc_item *items = side ? run->enemy_items : run->items;
        int n = side ? run->n_enemy_items : run->n_items;
        if (n < 0 || n > GC_MAX_ITEMS) continue;
        if (n > PR_MAX_MODIFIERS) n = PR_MAX_MODIFIERS;
        for (int i = 0; i < n; i++) {
            const gc_item *it = &items[i];
            if (!strcmp(it->key, "GOLDEN_PUNCH") && it->stack >= 1 && it->virtual_stack >= 0
                && (int64_t)it->stack + it->virtual_stack <= 5 && (it->arg == -1 || it->arg == 0)) return true;
        }
    }
    return false;
}
static void to_modifier(const gc_item *it, pr_modifier *m, bool golden)
{
    memset(m, 0, sizeof *m);
    m->stack_count = it->stack;
    m->pokemon_id = it->detached ? 0 : it->pokemon_id ? (int64_t)it->pokemon_id : -1;
    m->is_transferable = !it->non_transferable;
    m->type_tier = it->type_tier;
    m->type_ref = it->type_ref;
    m->loot_ref = it->loot_ref;
    m->unknown_subclass = true;
    m->cls = it->pokemon_id ? PR_MOD_HELD_ITEM : PR_MOD_PERSISTENT;
    if (!strcmp(it->key, "EVOLUTION_TRACKER_GIMMIGHOUL")) {
        m->cls = PR_MOD_EVO_TRACKER; m->num = 10; m->n_species = 1;
        m->species[0] = SPECIES_ID_GIMMIGHOUL; m->is_transferable = false;
        m->virtual_stack_count = it->virtual_stack;
        m->unknown_subclass = !it->pokemon_id || (it->arg != -1 && it->arg != 0)
            || it->stack < 1 || it->virtual_stack < 0
            || (int64_t)it->stack + it->virtual_stack > 999;
        return;
    }
    if(!strcmp(it->key,"EVIOLITE")||!strcmp(it->key,"SPECIES_STAT_BOOSTER")||!strcmp(it->key,"RARE_SPECIES_STAT_BOOSTER")){
        m->virtual_stack_count=it->virtual_stack;
        if(!strcmp(it->key,"EVIOLITE")){
            m->cls=it->eviolite_base?PR_MOD_STAT_BOOSTER:PR_MOD_EVOLUTION_STAT_BOOSTER;
            m->mult=1.5;m->stats_mask=(1u<<STAT_DEF)|(1u<<STAT_SPDEF);
            m->unknown_subclass=(it->arg!=-1&&it->arg!=0);
        }else if(it->arg>=0&&it->arg<6&&!it->eviolite_base){
            static const uint8_t masks[]={ (1u<<STAT_ATK)|(1u<<STAT_SPATK),1u<<STAT_ATK,1u<<STAT_DEF,1u<<STAT_SPD,1u<<STAT_SPDEF,1u<<STAT_SPATK };
            static const uint16_t species[]={SPECIES_ID_PIKACHU,SPECIES_ID_CUBONE,SPECIES_ID_DITTO,SPECIES_ID_DITTO,SPECIES_ID_CLAMPERL,SPECIES_ID_CLAMPERL};
            m->cls=PR_MOD_SPECIES_STAT_BOOSTER;m->mult=2;m->stats_mask=masks[it->arg];m->species[0]=species[it->arg];m->n_species=1;
            if(it->arg==1){m->species[1]=SPECIES_ID_MAROWAK;m->species[2]=SPECIES_ID_ALOLA_MAROWAK;m->n_species=3;}
            m->unknown_subclass=(!strcmp(it->key,"RARE_SPECIES_STAT_BOOSTER")!=(it->arg<4));
        }
        m->unknown_subclass=m->unknown_subclass||!pr_mod_is_ported_item(m);return;
    }
    if(it->eviolite_base)return; /* saved class marker is meaningful only for Eviolite */
    if (!strcmp(it->key, "BERRY_POUCH")) {
        m->cls=PR_MOD_PRESERVE_BERRY; m->virtual_stack_count=it->virtual_stack;
        m->pokemon_id=0; m->is_transferable=false; m->unknown_subclass=false;
        m->unknown_subclass=it->pokemon_id || it->detached || !pr_mod_is_ported_item(m);
        return;
    }
    if (!strcmp(it->key,"BATON")) {
        m->cls=PR_MOD_SWITCH_EFFECT_TRANSFER;
        m->virtual_stack_count=it->virtual_stack;
        m->unknown_subclass=false;
        m->unknown_subclass=(it->arg!=-1&&it->arg!=0)||!pr_mod_is_ported_item(m);
        return;
    }
    if (!strcmp(it->key,"GRIP_CLAW")) {
        m->cls=PR_MOD_CONTACT_HELD_TRANSFER;
        m->mult=0.1;
        m->virtual_stack_count=it->virtual_stack;
        m->unknown_subclass=false;
        m->unknown_subclass=(it->arg!=-1&&it->arg!=0)||!pr_mod_is_ported_item(m);
        return;
    }
    if(!strcmp(it->key,"MYSTICAL_ROCK")||!strcmp(it->key,"LEEK")||!strcmp(it->key,"LUCKY_EGG")||!strcmp(it->key,"GOLDEN_EGG")||!strcmp(it->key,"SOOTHE_BELL")){
        m->virtual_stack_count=it->virtual_stack;m->unknown_subclass=false;
        if(!strcmp(it->key,"MYSTICAL_ROCK"))m->cls=PR_MOD_FIELD_EFFECT;
        else if(!strcmp(it->key,"LEEK")){
            m->cls=PR_MOD_SPECIES_CRIT_BOOSTER;m->num=2;m->n_species=3;
            m->species[0]=SPECIES_ID_FARFETCHD;m->species[1]=SPECIES_ID_GALAR_FARFETCHD;m->species[2]=SPECIES_ID_SIRFETCHD;
        }else if(!strcmp(it->key,"SOOTHE_BELL"))m->cls=PR_MOD_FRIENDSHIP_BOOSTER;
        else {m->cls=PR_MOD_EXP_BOOSTER;m->mult=!strcmp(it->key,"LUCKY_EGG")?0.4:1.0;}
        m->unknown_subclass=(it->arg!=-1&&it->arg!=0)||!pr_mod_is_ported_item(m);return;
    }
    /* Transient Amulet Coin is a bounded Golden Punch dependency. */
    if (it->virtual_stack && strcmp(it->key, "WIDE_LENS")
        && strcmp(it->key, "GOLDEN_PUNCH") && strcmp(it->key, "AMULET_COIN") && strcmp(it->key, "MULTI_LENS")) return;
    /* These classes now have turn-path implementations in battle-core.  Parameters and
     * transferability come from upstream modifierTypes, not an inferred item name. */
    static const struct { const char *key; uint8_t cls; } held[] = {
        {"LEFTOVERS", PR_MOD_TURN_HEAL}, {"SHELL_BELL", PR_MOD_HIT_HEAL},
        {"FOCUS_BAND", PR_MOD_SURVIVE_DAMAGE}, {"QUICK_CLAW", PR_MOD_BYPASS_SPEED_CHANCE},
        {"KINGS_ROCK", PR_MOD_FLINCH_CHANCE}, {"WHITE_HERB", PR_MOD_RESET_NEGATIVE_STAT_STAGE},
        {"BERRY", PR_MOD_BERRY}, {"MINI_BLACK_HOLE", PR_MOD_TURN_HELD_TRANSFER}
    };
    for (size_t i = 0; i < sizeof held / sizeof held[0]; i++) {
        if (strcmp(it->key, held[i].key)) continue;
        m->cls = held[i].cls;
        m->unknown_subclass = false;
        if (m->cls == PR_MOD_BERRY) m->berry_type = (int8_t)it->arg;
        if (m->cls == PR_MOD_TURN_HELD_TRANSFER && (it->stack!=1 || it->virtual_stack || (it->arg!=-1&&it->arg!=0))) m->unknown_subclass=true;
        return;
    }
    if (!strcmp(it->key, "GOLDEN_PUNCH") || !strcmp(it->key, "AMULET_COIN")) {
        bool coin = !strcmp(it->key, "AMULET_COIN");
        m->cls = coin ? PR_MOD_MONEY_MULTIPLIER : PR_MOD_DAMAGE_MONEY_REWARD;
        if (it->stack >= 1 && it->virtual_stack >= 0
            && (int64_t)it->stack + it->virtual_stack <= 5
            && (it->arg == -1 || it->arg == 0) && (!coin || (!it->pokemon_id && !it->detached))
            && (!coin || !it->virtual_stack || golden)) {
            if (coin) m->pokemon_id = 0; /* persistent source class has no holder */
            m->virtual_stack_count = it->virtual_stack;
            m->unknown_subclass = false;
        }
    } else if (!strcmp(it->key, "TEMP_STAT_STAGE_BOOSTER") || !strcmp(it->key, "DIRE_HIT")) {
        const bool crit = !strcmp(it->key, "DIRE_HIT");
        if (!it->pokemon_id && !it->detached && !it->virtual_stack
            && it->stack >= 1 && it->stack <= 2 && it->battle_count >= 1 && it->battle_count <= 5
            && (crit ? it->arg == 0 : it->arg >= STAT_ATK && it->arg <= STAT_ACC)) {
            m->cls = crit ? PR_MOD_TEMP_CRIT_BOOSTER : PR_MOD_TEMP_STAT_STAGE_BOOSTER;
            m->pokemon_id = 0; m->stat = it->arg;
            m->mult = it->arg == STAT_ACC ? 1.0 : 0.2;
            m->unknown_subclass = false; m->is_transferable = false;
        }
    } else if (!strcmp(it->key, "MULTI_LENS")) {
        /* PokemonMultiHitModifier: one or two effective loaded stacks. Invalid
         * records stay generic/opaque: the damage helper reads matching classes. */
        if (it->stack >= 1 && it->virtual_stack >= 0
            && (int64_t)it->stack + it->virtual_stack <= 2 && (it->arg == -1 || it->arg == 0)) {
            m->cls = PR_MOD_MULTI_HIT;
            m->virtual_stack_count = it->virtual_stack;
            m->unknown_subclass = false;
        }
    } else if (!strcmp(it->key, "WIDE_LENS")) {
        /* modifierTypes.WIDE_LENS: +5 for every real and transient virtual stack. */
        if (it->stack >= 1 && it->stack <= 3 && it->virtual_stack >= 0
            && (int64_t)it->stack + it->virtual_stack <= 3 && (it->arg == -1 || it->arg == 0)) {
            m->cls = PR_MOD_MOVE_ACCURACY_BOOSTER;
            m->num = 5;
            m->virtual_stack_count = it->virtual_stack;
            m->unknown_subclass = false;
        }
    } else if (!strcmp(it->key, "REVIVER_SEED")) {
        /* InstantRevive has one real stack. Generic acquisition may retain an
         * invalid stack; keep that opaque rather than crediting its effect. */
        if (it->stack == 1 && it->virtual_stack == 0 && (it->arg == -1 || it->arg == 0)) {
            m->cls = PR_MOD_INSTANT_REVIVE;
            m->unknown_subclass = false;
        }
    } else if (!strcmp(it->key, "TOXIC_ORB") || !strcmp(it->key, "FLAME_ORB")) {
        /* modifierTypes: PokemonTurnStatusEffectModifier(TOXIC/BURN). Both
         * share a class; the item identity selects its explicit status. */
        m->cls = PR_MOD_TURN_STATUS_EFFECT;
        m->status_effect = !strcmp(it->key, "TOXIC_ORB") ? STATUS_EFFECT_TOXIC : STATUS_EFFECT_BURN;
        m->unknown_subclass = false;
    } else if (!strcmp(it->key, "BASE_STAT_BOOSTER") && it->arg >= STAT_HP && it->arg <= STAT_SPD) {
        m->cls = PR_MOD_BASE_STAT; m->stat = (int8_t)it->arg; m->virtual_stack_count = it->virtual_stack;
        m->is_transferable = false; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "MYSTERY_ENCOUNTER_SHUCKLE_JUICE") && (it->arg == 10 || it->arg == -15)) {
        m->cls = PR_MOD_BASE_STAT_TOTAL; m->num = it->arg; m->virtual_stack_count = it->virtual_stack;
        m->is_transferable = false; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "MYSTERY_ENCOUNTER_OLD_GATEAU") && it->arg == 0) {
        m->cls = PR_MOD_BASE_STAT_FLAT; m->virtual_stack_count = it->virtual_stack;
        m->is_transferable = false; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "ATTACK_TYPE_BOOSTER")) { /* AttackTypeBoosterModifier(type, 20) */
        m->cls = PR_MOD_ATTACK_TYPE_BOOSTER; m->move_type = (int8_t)it->arg; m->mult = 0.2; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "SOUL_DEW")) {
        m->cls=PR_MOD_NATURE_WEIGHT; m->virtual_stack_count=it->virtual_stack; m->unknown_subclass=false;
        m->unknown_subclass=!pr_mod_is_ported_item(m);
    } else if (!strcmp(it->key, "SCOPE_LENS")) {          /* CritBoosterModifier(1) */
        m->cls = PR_MOD_CRIT_BOOSTER; m->num = 1; m->unknown_subclass = false;
    } else if (!strcmp(it->key, "ENEMY_DAMAGE_BOOSTER")) {
        m->cls = PR_MOD_ENEMY_DAMAGE_BOOSTER; m->mult = 1.05; m->unknown_subclass = false; m->is_transferable = false;
    } else if (!strcmp(it->key, "ENEMY_DAMAGE_REDUCTION")) {
        m->cls = PR_MOD_ENEMY_DAMAGE_REDUCER; m->mult = 0.975; m->unknown_subclass = false; m->is_transferable = false;
    }
}

/* A canonical held Seed is unique per owner. Duplicate generic entries have
 * no validated replacement policy, so all of them stay opaque. */
static bool duplicate_seed(const gc_item *items, int n, int index)
{
    if (strcmp(items[index].key, "REVIVER_SEED")) return false;
    if (n < 0 || n > GC_MAX_ITEMS) return true;
    for (int i = 0; i < n; i++)
        if (i != index && !strcmp(items[i].key, "REVIVER_SEED")
            && items[i].pokemon_id == items[index].pokemon_id) return true;
    return false;
}

void gc_items_to_battle(gc_run *run)
{
    pr_battle *bt = &run->bt;
    bt->n_mods[0] = bt->n_mods[1] = 0;
    /* Do not quietly run with the first part of an invalid inventory. */
    if(run->n_items<0||run->n_enemy_items<0||run->n_items>PR_MAX_MODIFIERS||run->n_enemy_items>PR_MAX_MODIFIERS){
        pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);return;
    }
    const bool golden = golden_bridge(run);
    for (int side=0;side<2;side++) {
        const gc_item *items=side?run->enemy_items:run->items;
        int n=side?run->n_enemy_items:run->n_items;
        for (int i=0;i<n && i<GC_MAX_ITEMS;i++)
            if (items[i].type_ref>bt->modifier_type_ref_max) bt->modifier_type_ref_max=items[i].type_ref;
    }
    for (int i = 0; i < run->n_items && bt->n_mods[0] < PR_MAX_MODIFIERS; i++)
        to_modifier(&run->items[i], &bt->mods[0][bt->n_mods[0]++], golden);
    for (int i = 0; i < run->n_enemy_items && bt->n_mods[1] < PR_MAX_MODIFIERS; i++)
        to_modifier(&run->enemy_items[i], &bt->mods[1][bt->n_mods[1]++], golden);
    for (int side = 0; side < 2; side++) {
        const gc_item *items = side ? run->enemy_items : run->items;
        int n = side ? run->n_enemy_items : run->n_items;
        for (int i = 0; i < bt->n_mods[side]; i++)
            if (duplicate_seed(items, n, i)) {
                bt->mods[side][i].cls = items[i].pokemon_id ? PR_MOD_HELD_ITEM : PR_MOD_PERSISTENT;
                bt->mods[side][i].unknown_subclass = true;
            }
    }
    /* The battle monetary primitive requires a representable scene wallet.
     * Keep malformed known dependencies typed/opaque so its preflight refuses
     * rather than silently omitting an Amulet bonus. */
    if (!isfinite(run->money) || run->money < 0 || run->money > 9007199254740991.0)
        for (int side = 0; side < 2; side++) for (int i = 0; i < bt->n_mods[side]; i++)
            if (bt->mods[side][i].cls == PR_MOD_DAMAGE_MONEY_REWARD || bt->mods[side][i].cls == PR_MOD_MONEY_MULTIPLIER)
                bt->mods[side][i].unknown_subclass = true;
    gc_int(run)->battle_item_count[0] = bt->n_mods[0];
    gc_int(run)->battle_item_count[1] = bt->n_mods[1];
}

/* The supported canonical generated stocks each occupy one tier in their
 * native generation pool. Capture it at creation, before a later transfer can
 * change the holder's side or trainer context. Unknown classes stay opaque. */
void gc_cache_generated_item_tiers(gc_run *run)
{
    for (int i=0;i<run->n_enemy_items;i++) {
        gc_item *item=&run->enemy_items[i];
        pr_modifier m;to_modifier(item,&m,golden_bridge(run));
        /* Wave1000 Mini Black Holes are appended with getModifierType(),
         * rather than a tiered pool option. Their fresh type has no tier. */
        if(m.cls==PR_MOD_TURN_HELD_TRANSFER){item->type_tier=0;continue;}
        int tier=pr_held_item_tier_for_pool(&m,run->battle_type==BATTLE_TYPE_TRAINER);
        if(tier>=0)item->type_tier=(uint8_t)(tier+1);
    }
}

/* Canonical fields of the native transfer families. Opaque/unsupported stock
 * remains owned by its existing entry and is never guessed from a class name. */
static bool transfer_stock(const pr_modifier *m,gc_item *it)
{
    if(m->unknown_subclass)return false;
    const char *key=NULL;int arg=-1;
    switch(m->cls){
    case PR_MOD_STAT_BOOSTER: case PR_MOD_EVOLUTION_STAT_BOOSTER:
        if(m->mult==1.5&&m->stats_mask==((1u<<STAT_DEF)|(1u<<STAT_SPDEF)))key="EVIOLITE";
        break;
    case PR_MOD_SPECIES_STAT_BOOSTER:
        if(m->mult!=2)break;
        if(m->n_species==1&&m->species[0]==SPECIES_ID_PIKACHU&&m->stats_mask==((1u<<STAT_ATK)|(1u<<STAT_SPATK)))arg=0;
        else if(m->n_species==3&&m->species[0]==SPECIES_ID_CUBONE&&m->species[1]==SPECIES_ID_MAROWAK&&m->species[2]==SPECIES_ID_ALOLA_MAROWAK&&m->stats_mask==(1u<<STAT_ATK))arg=1;
        else if(m->n_species==1&&m->species[0]==SPECIES_ID_DITTO){if(m->stats_mask==(1u<<STAT_DEF))arg=2;else if(m->stats_mask==(1u<<STAT_SPD))arg=3;}
        else if(m->n_species==1&&m->species[0]==SPECIES_ID_CLAMPERL){if(m->stats_mask==(1u<<STAT_SPDEF))arg=4;else if(m->stats_mask==(1u<<STAT_SPATK))arg=5;}
        if(arg>=0)key=arg<4?"RARE_SPECIES_STAT_BOOSTER":"SPECIES_STAT_BOOSTER";
        break;
    case PR_MOD_TURN_HEAL:key="LEFTOVERS";break;
    case PR_MOD_HIT_HEAL:key="SHELL_BELL";break;
    case PR_MOD_SURVIVE_DAMAGE:key="FOCUS_BAND";break;
    case PR_MOD_BYPASS_SPEED_CHANCE:key="QUICK_CLAW";break;
    case PR_MOD_FLINCH_CHANCE:key="KINGS_ROCK";break;
    case PR_MOD_RESET_NEGATIVE_STAT_STAGE:key="WHITE_HERB";break;
    case PR_MOD_SWITCH_EFFECT_TRANSFER:key="BATON";break;
    case PR_MOD_CONTACT_HELD_TRANSFER:key="GRIP_CLAW";break;
    case PR_MOD_TURN_HELD_TRANSFER:key="MINI_BLACK_HOLE";break;
    case PR_MOD_BERRY:key="BERRY";arg=m->berry_type;break;
    case PR_MOD_MOVE_ACCURACY_BOOSTER:key="WIDE_LENS";break;
    case PR_MOD_MULTI_HIT:key="MULTI_LENS";break;
    case PR_MOD_CRIT_BOOSTER:key="SCOPE_LENS";break;
    case PR_MOD_SPECIES_CRIT_BOOSTER:key="LEEK";break;
    case PR_MOD_FIELD_EFFECT:key="MYSTICAL_ROCK";break;
    case PR_MOD_EXP_BOOSTER:if(m->mult==0.4)key="LUCKY_EGG";else if(m->mult==1.0)key="GOLDEN_EGG";break;
    case PR_MOD_FRIENDSHIP_BOOSTER:key="SOOTHE_BELL";break;
    case PR_MOD_NATURE_WEIGHT:key="SOUL_DEW";break;
    case PR_MOD_DAMAGE_MONEY_REWARD:key="GOLDEN_PUNCH";break;
    case PR_MOD_ATTACK_TYPE_BOOSTER:key="ATTACK_TYPE_BOOSTER";arg=m->move_type;break;
    case PR_MOD_INSTANT_REVIVE:key="REVIVER_SEED";break;
    case PR_MOD_TURN_STATUS_EFFECT:
        if(m->status_effect==STATUS_EFFECT_TOXIC)key="TOXIC_ORB";
        if(m->status_effect==STATUS_EFFECT_BURN)key="FLAME_ORB";
        break;
    default:break;
    }
    if(!key)return false;
    if(it)*it=(gc_item){.key=key,.arg=arg,.stack=m->stack_count,
        .pokemon_id=m->pokemon_id<0?0:(uint32_t)m->pokemon_id,
        .detached=m->pokemon_id==0,.virtual_stack=m->virtual_stack_count,
        .non_transferable=!m->is_transferable,.type_tier=m->type_tier,.type_ref=m->type_ref,.loot_ref=m->loot_ref,.eviolite_base=m->cls==PR_MOD_STAT_BOOSTER};
    return true;
}
static bool same_transfer_stock(const pr_modifier *a,const pr_modifier *b)
{
    gc_item x,y;
    return transfer_stock(a,&x)&&transfer_stock(b,&y)&&!strcmp(x.key,y.key)&&x.arg==y.arg&&x.eviolite_base==y.eviolite_base;
}

/* Persist the losses of mapped consumables before another encounter rebuilds bt->mods or
 * a save serializes the run inventory. Match owner AND berry kind: two party members' berries
 * are separate entries. Unsupported items and entries beyond battle-core's capacity stay intact.
 * The tracked prefix shrinks with removed entries, so repeated synchronization never mistakes
 * an unrepresented overflow item for a consumed one. Restored berries are inserted into that represented prefix; other additions remain separate. */
void gc_items_from_battle(gc_run *run)
{
    if(run->n_items<0||run->n_enemy_items<0||run->n_items>PR_MAX_MODIFIERS||run->n_enemy_items>PR_MAX_MODIFIERS){
        pr_note_unported(&run->bt,PR_FEAT_ENGINE_LIMIT);return;
    }
    const bool golden = golden_bridge(run);
    for (int side = 0; side < 2; side++) {
        gc_item *items = side ? run->enemy_items : run->items;
        int *n = side ? &run->n_enemy_items : &run->n_items;
        const int tracked = gc_int(run)->battle_item_count[side];
        bool used[PR_MAX_MODIFIERS] = {false};
        bool duplicate[GC_MAX_ITEMS];
        for (int i = 0; i < *n; i++) duplicate[i] = duplicate_seed(items, *n, i);
        int w = 0, removed = 0;
        for (int i = 0; i < *n; i++) {
            gc_item it = items[i];
            pr_modifier expected;
            to_modifier(&it, &expected, golden);
            if(i<tracked&&!duplicate[i]&&transfer_stock(&expected,NULL)){
                int match=-1;bool opaque=false;
                const pr_pokemon *holder=pr_pokemon_by_id(&run->bt,it.pokemon_id);
                for(int j=0;j<run->bt.n_mods[side];j++){
                    const pr_modifier *m=&run->bt.mods[side][j];
                    bool detached=side&&m->pokemon_id==0&&holder&&holder->hp<=0;
                    if(m->cls==expected.cls&&m->pokemon_id==expected.pokemon_id&&m->unknown_subclass)opaque=true;
                    if(!used[j]&&(!it.loot_ref||m->loot_ref==it.loot_ref)
                        &&(m->pokemon_id==expected.pokemon_id||detached)&&same_transfer_stock(m,&expected)){match=j;break;}
                }
                if(opaque){items[w++]=it;continue;}
                if(match<0||run->bt.mods[side][match].stack_count<=0){removed++;continue;}
                const pr_modifier *m=&run->bt.mods[side][match];used[match]=true;
                it.stack=m->stack_count;it.virtual_stack=m->virtual_stack_count;
                it.detached=m->pokemon_id==0;it.non_transferable=!m->is_transferable;it.type_tier=m->type_tier;
                it.type_ref=m->type_ref;it.loot_ref=m->loot_ref;
                items[w++]=it;continue;
            }
            if (i < tracked && !expected.unknown_subclass && (expected.cls == PR_MOD_INSTANT_REVIVE
                                || expected.cls == PR_MOD_MOVE_ACCURACY_BOOSTER
                                || expected.cls == PR_MOD_DAMAGE_MONEY_REWARD || expected.cls == PR_MOD_MULTI_HIT || expected.cls == PR_MOD_PRESERVE_BERRY)
                && !duplicate[i]) {
                bool opaque = false;
                for (int j = 0; j < run->bt.n_mods[side]; j++) {
                    const pr_modifier *m = &run->bt.mods[side][j];
                    if (m->cls == expected.cls && m->pokemon_id == expected.pokemon_id && m->unknown_subclass)
                        opaque = true;
                }
                if (opaque) { items[w++] = it; continue; }
                int remaining = 0;
                const pr_pokemon *holder = pr_pokemon_by_id(&run->bt, it.pokemon_id);
                for (int j = 0; j < run->bt.n_mods[side]; j++) {
                    const pr_modifier *m = &run->bt.mods[side][j];
                    if (used[j] || m->unknown_subclass || m->cls != expected.cls) continue;
                    bool detached = side && m->pokemon_id == 0 && holder && holder->hp <= 0;
                    if (m->pokemon_id != expected.pokemon_id && !detached) continue;
                    remaining = m->stack_count;
                    it.detached = expected.cls != PR_MOD_PRESERVE_BERRY && m->pokemon_id == 0;
                    if (expected.cls == PR_MOD_MOVE_ACCURACY_BOOSTER || expected.cls == PR_MOD_DAMAGE_MONEY_REWARD || expected.cls == PR_MOD_MULTI_HIT || expected.cls == PR_MOD_PRESERVE_BERRY)
                        it.virtual_stack = m->virtual_stack_count;
                    used[j] = true;
                    break;
                }
                if (remaining <= 0) { removed++; continue; }
                it.stack = remaining;
            }
            if (i < tracked && (expected.cls == PR_MOD_BERRY ||
                                expected.cls == PR_MOD_RESET_NEGATIVE_STAT_STAGE)) {
                int remaining = 0;
                for (int j = 0; j < run->bt.n_mods[side]; j++) {
                    const pr_modifier *m = &run->bt.mods[side][j];
                    if (m->unknown_subclass || m->cls != expected.cls ||
                        m->pokemon_id != expected.pokemon_id) continue;
                    if (m->cls == PR_MOD_BERRY && m->berry_type != expected.berry_type) continue;
                    remaining += m->stack_count;
                }
                if (remaining <= 0) { removed++; continue; }
                it.stack = remaining;
            }
            items[w++] = it;
        }
        memset(&items[w], 0, (size_t)(*n - w) * sizeof *items);
        *n = w;
        int prefix = tracked - removed;
        for(int j=0;j<run->bt.n_mods[side];j++){
            const pr_modifier *m=&run->bt.mods[side][j];gc_item added;
            if(used[j]||m->stack_count<=0||!transfer_stock(m,&added))continue;
            /* The legacy gc_item zero-ID sentinel cannot represent a live
             * zero-ID holder; keep that limitation explicit at this adapter. */
            if(m->pokemon_id==0&&pr_pokemon_by_id(&run->bt,0)){
                pr_note_unported(&run->bt,PR_FEAT_MODIFIERS);continue;
            }
            bool found=false;
            for(int i=0;i<*n;i++){
                pr_modifier present;to_modifier(&items[i],&present,golden);
                if(present.pokemon_id==m->pokemon_id&&same_transfer_stock(&present,m)){found=true;break;}
            }
            if(found)continue;
            if(*n>=GC_MAX_ITEMS){pr_note_unported(&run->bt,PR_FEAT_ENGINE_LIMIT);continue;}
            memmove(items+prefix+1,items+prefix,(size_t)(*n-prefix)*sizeof *items);
            items[prefix++]=added;(*n)++;
        }
        for (int j = 0; j < run->bt.n_mods[side]; j++) {
            const pr_modifier *m = &run->bt.mods[side][j];
            if (m->cls != PR_MOD_BERRY || m->unknown_subclass || m->stack_count <= 0) continue;
            bool found = false;
            for (int i = 0; i < *n; i++)
                if (!strcmp(items[i].key, "BERRY") && items[i].pokemon_id == m->pokemon_id && items[i].arg == m->berry_type) found = true;
            if (found) continue;
            if (*n >= GC_MAX_ITEMS) {pr_note_unported(&run->bt, PR_FEAT_ENGINE_LIMIT); continue;}
            memmove(items + prefix + 1, items + prefix, (size_t)(*n - prefix) * sizeof *items);
            items[prefix] = (gc_item){.key="BERRY", .arg=m->berry_type, .pokemon_id=(uint32_t)m->pokemon_id, .stack=m->stack_count};
            prefix++; (*n)++;
        }
        /* addModifier removes and re-appends a receiving stack. Preserve that
         * native order: subsequent theft RNG indexes the inventory list. */
        int ranks[GC_MAX_ITEMS];
        bool ranked[PR_MAX_MODIFIERS]={false};
        for(int i=0;i<prefix;i++){
            pr_modifier expected;to_modifier(&items[i],&expected,golden);
            ranks[i]=PR_MAX_MODIFIERS+i;
            for(int j=0;j<run->bt.n_mods[side];j++){
                const pr_modifier *m=&run->bt.mods[side][j];
                if(ranked[j]||m->cls!=expected.cls||m->pokemon_id!=expected.pokemon_id)continue;
                if(transfer_stock(&expected,NULL)?!same_transfer_stock(m,&expected):
                    (m->stat!=expected.stat||m->num!=expected.num||m->move_type!=expected.move_type))continue;
                ranked[j]=true;ranks[i]=j;break;
            }
        }
        for(int i=1;i<prefix;i++){
            gc_item it=items[i];int rank=ranks[i],j=i;
            while(j>0&&ranks[j-1]>rank){items[j]=items[j-1];ranks[j]=ranks[j-1];j--;}
            items[j]=it;ranks[j]=rank;
        }
        gc_int(run)->battle_item_count[side] = prefix;
    }
}

/* ---- catching: the held items change hands (gc_catch.c) ---- */

/* The entries of one side that are held items (they carry a Pokemon's id): all of them, or one Pokemon's */
static void drop_held(gc_run *run, int side, uint32_t id, bool all)
{
    gc_item *items = side ? run->enemy_items : run->items;
    int *n = side ? &run->n_enemy_items : &run->n_items;
    int w = 0;
    for (int i = 0; i < *n; i++) {
        if (items[i].pokemon_id && (all || items[i].pokemon_id == id)) continue;
        items[w++] = items[i];
    }
    memset(&items[w], 0, (size_t)(*n - w) * sizeof *items);
    *n = w;
}

/* addToParty: every enemy held item is added to the player's modifiers (a Pokemon takes its id with it, so they are
 * the new member's), stacking where the same item is already held */
void gc_items_take_enemy_held(gc_run *run)
{
    for (int i = 0; i < run->n_enemy_items; i++) {
        const gc_item *it = &run->enemy_items[i];
        if (it->pokemon_id) gc_item_add(run, it->key, it->arg, it->pokemon_id, it->stack);
    }
}

void gc_items_drop_enemy_held(gc_run *run)
{
    drop_held(run, 1, 0, true);
    gc_items_to_battle(run);
}

void gc_items_drop_held_of(gc_run *run, uint32_t pokemon_id)
{
    drop_held(run, 0, pokemon_id, false);
    gc_items_to_battle(run);
}

/* PokemonHeldItemModifier.getMaxStackCount() for `mon` holding `key`/`arg`: getMaxHeldItemCount(), which is the holder's IV for a
 * vitamin (BaseStatBoosterModifier) and per generated type for a generator's items (berries) */
int gc_item_max_held(const gc_run *run, const char *key, int16_t arg, const gc_mon *mon)
{
    (void)run;
    const int idx = gc_reward_item_index(key);
    if (idx < 0) return 1;
    int max = pr_items[idx].max_held;
    if (max == -3)
        for (int i = 0; i < pr_gen_type_count; i++)
            if (pr_gen_types[i].item == idx && pr_gen_types[i].arg == arg) { max = pr_gen_types[i].max_held; break; }
    if (max == -2) return mon && arg >= 0 && arg < 6 ? mon->b.ivs[arg] : 0;
    return max < 0 ? 0 : max;
}

/* addModifier / removeModifier end in updateModifiers: virtual stacks reset, every party member's stats calculated again */
static void held_items_changed(gc_run *run)
{
    for (int i = 0; i < run->n_items; i++) run->items[i].virtual_stack = 0;
    gc_items_to_battle(run);
    for (int i = 0; i < run->n_party; i++) {
        pr_pokemon *b = &run->party[i].b;
        const int max = b->stats[STAT_HP], hp = b->hp;
        pr_recalculate_stats(&run->bt, b);
        /* a vitamin that moved changes the maximum HP: the screen hears it as an HP event, as an item used on the Pokemon */
        if (b->stats[STAT_HP] != max || b->hp != hp) {
            gc_event *ev = gc_ev_new(run, GC_EV_HP);
            if (ev) { ev->a = 0; ev->b = i; ev->c = b->hp; }
        }
    }
}

/* The party screen's MODIFIER_TRANSFER callback from the reward screen (SelectModifierPhase.openModifierTransferScreen):
 * tryTransferHeldItemModifier(item, party[to], true, qty, undefined, undefined, itemLost false). The target's matching stack
 * is capped at its getMaxStackCount(); with no matching stack nothing caps it. The taken copies go in a new entry at the end
 * of the list (the old matching entry is removed and re-added, as removeModifier + addModifier do), and an emptied source is
 * removed. Returns how many copies moved; 0 = refused (upstream returns false and nothing changes). */
int gc_item_transfer(gc_run *run, int item_idx, int to, int qty)
{
    if (!run || item_idx < 0 || item_idx >= run->n_items || to < 0 || to >= run->n_party || qty <= 0) return 0;
    gc_item src = run->items[item_idx];
    const uint32_t to_id = run->party[to].b.id;
    if (!src.pokemon_id || src.pokemon_id == to_id || src.non_transferable
        || !strcmp(src.key, "EVOLUTION_TRACKER_GIMMIGHOUL") || src.stack <= 0) return 0;
    int match = -1;
    for (int i = 0; i < run->n_items; i++)
        if (i != item_idx && run->items[i].pokemon_id == to_id && !strcmp(run->items[i].key, src.key) && run->items[i].arg == src.arg) {
            match = i;
            break;
        }
    int taken, total;
    if (match >= 0) {
        const int max = gc_item_max_held(run, src.key, src.arg, &run->party[to]);
        const int have = run->items[match].stack;
        if (have >= max) return 0;
        taken = qty < src.stack ? qty : src.stack;
        if (taken > max - have) taken = max - have;
        total = have + taken;
    } else {
        taken = qty < src.stack ? qty : src.stack;
        total = taken;
    }
    /* newItemModifier = itemModifier.clone(): the source's fields, on the target */
    gc_item moved = src;
    moved.pokemon_id = to_id;
    moved.stack = total;
    if (match < 0 && src.stack > taken && run->n_items >= GC_MAX_ITEMS) return 0; /* no room for the new entry */
    run->items[item_idx].stack -= taken;
    /* remove the higher index first so the lower one stays valid */
    const int empty = run->items[item_idx].stack == 0 ? item_idx : -1;
    const int hi = empty > match ? empty : match, lo = empty > match ? match : empty;
    if (hi >= 0) gc_item_remove_at(run, hi);
    if (lo >= 0) gc_item_remove_at(run, lo);
    run->items[run->n_items++] = moved;
    held_items_changed(run);
    return taken;
}

/* The party screen's DISCARD (tryDiscardHeldItemModifier): `count` copies of a transferable held item go, the entry with
 * them when none is left. Returns the copies thrown away. */
int gc_item_discard(gc_run *run, int item_idx, int count)
{
    if (!run || item_idx < 0 || item_idx >= run->n_items || count <= 0) return 0;
    gc_item *it = &run->items[item_idx];
    if (!it->pokemon_id || it->non_transferable || !strcmp(it->key, "EVOLUTION_TRACKER_GIMMIGHOUL") || it->stack <= 0) return 0;
    const int taken = count < it->stack ? count : it->stack;
    it->stack -= taken;
    if (!it->stack) gc_item_remove_at(run, item_idx);
    /* upstream calls removeModifier alone and the stats wait for the next updateModifiers (the next encounter's). Here they
     * follow at once: a discarded vitamin otherwise left a Pokemon above its own maximum HP until then */
    held_items_changed(run);
    return taken;
}
