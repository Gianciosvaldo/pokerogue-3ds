/*
 * Helper functions for trainer rewards & genModifiersFunc
 */
#include "pr/reward.h"
#include "pr_enums.h"
#include "reward_internal.h"

#define PR_SPECIES_PIKACHU 25
#define PR_SPECIES_SWELLOW 277
#define PR_SPECIES_GLISCOR 472
#define PR_SPECIES_MILOTIC 350
#define PR_SPECIES_PELIPPER 279
#define PR_SPECIES_GIGALITH 526

#define PR_ABILITY_GUTS 62
#define PR_ABILITY_POISON_HEAL 90
#define PR_ABILITY_MARVEL_SCALE 63

int pr_reward_trainer_gen_modifiers(int trainer_type, const pr_reward_mon party[PR_REWARD_MAX_PARTY], int n_party, pr_item_type out[8])
{
    int n = 0;
    if (n_party <= 0) return 0;

    if (trainer_type == TRAINER_TYPE_RED) {
        /* Red: Pikachu slot 0 -> RARE_SPECIES_STAT_BOOSTER (arg 0 LIGHT_BALL) */
        if (party[0].species == PR_SPECIES_PIKACHU) {
            out[n].item = PR_ITEM_RARE_SPECIES_STAT_BOOSTER;
            out[n].arg = PR_SSB_LIGHT_BALL;
            n++;
        }
    } else if (trainer_type == TRAINER_TYPE_STEVEN) {
        /* Steven: Gigalith slot 0 -> MYSTICAL_ROCK */
        if (party[0].species == PR_SPECIES_GIGALITH) {
            out[n].item = PR_ITEM_MYSTICAL_ROCK;
            out[n].arg = -1;
            n++;
        }
    } else if (trainer_type == TRAINER_TYPE_WALLACE) {
        /* Wallace: Pelipper slot 0 -> MYSTICAL_ROCK, Milotic slot 5 w/ Marvel Scale -> TOXIC_ORB */
        if (party[0].species == PR_SPECIES_PELIPPER) {
            out[n].item = PR_ITEM_MYSTICAL_ROCK;
            out[n].arg = -1;
            n++;
        }
        if (n_party >= 6 && party[5].species == PR_SPECIES_MILOTIC && party[5].ability == PR_ABILITY_MARVEL_SCALE) {
            out[n].item = PR_ITEM_TOXIC_ORB;
            out[n].arg = -1;
            n++;
        }
    } else if (trainer_type == TRAINER_TYPE_CYNTHIA || trainer_type == TRAINER_TYPE_KIERAN) {
        /* Cynthia / Kieran: Gliscor slot 2 w/ Poison Heal -> TOXIC_ORB */
        if (n_party >= 3 && party[2].species == PR_SPECIES_GLISCOR && party[2].ability == PR_ABILITY_POISON_HEAL) {
            out[n].item = PR_ITEM_TOXIC_ORB;
            out[n].arg = -1;
            n++;
        }
    } else if (trainer_type == TRAINER_TYPE_RIVAL_5 || trainer_type == TRAINER_TYPE_RIVAL_6) {
        /* Rival 5/6: Bird slot 1 (Swellow w/ Guts) -> FLAME_ORB */
        if (n_party >= 2 && party[1].species == PR_SPECIES_SWELLOW && party[1].ability == PR_ABILITY_GUTS) {
            out[n].item = PR_ITEM_FLAME_ORB;
            out[n].arg = -1;
            n++;
        }
    } else if (trainer_type == TRAINER_TYPE_GUZMA || trainer_type == TRAINER_TYPE_GUZMA_2 || trainer_type == TRAINER_TYPE_SKULL_GRUNT) {
        /* Guzma / Guzma 2 / Skull Grunt (if Gliscor slot 4 w/ Poison Heal) -> TOXIC_ORB */
        if (n_party >= 5 && party[4].species == PR_SPECIES_GLISCOR && party[4].ability == PR_ABILITY_POISON_HEAL) {
            out[n].item = PR_ITEM_TOXIC_ORB;
            out[n].arg = -1;
            n++;
        }
    }

    return n;
}
