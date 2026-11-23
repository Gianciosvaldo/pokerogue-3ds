/* custom-core: the JoJo characters (data, generated from lorenzo's lineup files of 2026-10-18/19 (the first 17) and 2026-11-15 (the next 17, from
 * Caesar to Giorno) and edited by hand; see jojo.h).
 *
 * One row per character: id, display name, iconic line, biome (BiomeId; -1 = not in a biome pool: the Pillar Men), the background
 * passive (an AbilityId; -1 = none; CC_JOJO_PASSIVE_MADE_IN_HEAVEN for Pucci, CC_JOJO_PASSIVE_THE_WORLD for DIO, CC_JOJO_PASSIVE_PERFECT_BEING for Kars) and up to six members. A member is the species, the form index, the ability slot (0, 1 = the
 * form's abilities, 2 = hidden), shiny (0/1) and its variant tier (0..2, 0 unless the species has variant palettes), four move ids and
 * the held items {modifierTypes key, argument, stack}. The comments name the species, form and moves. Held items upstream does not have
 * were dropped (lorenzo, 2026-10-18: no substitutions); stacks are capped as upstream caps them. The ids are checked against
 * battle-core's data by game-core/tests/custom_trainer_host_test.c; a clean sweep through battle-core means no unported id was
 * consulted, it is not upstream parity. */
#include "cc/jojo.h"

const cc_jojo_char cc_jojo_chars[] = {
    {"kars", "Kars", "I shall become the ultimate lifeform!", -1, CC_JOJO_PASSIVE_PERFECT_BEING, 6, {
        {151, 0, 0, 1, 1, {144, 94, 396, 105}, 3, {{"BERRY", 1, 1}, {"FOCUS_BAND", -1, 1}, {"GOLDEN_EGG", -1, 1}}}, /* Mew; Transform, Psychic, Aura Sphere, Recover */
        {132, 0, 2, 0, 0, {144, 0, 0, 0}, 3, {{"RARE_SPECIES_STAT_BOOSTER", 2, 1}, {"RARE_SPECIES_STAT_BOOSTER", 3, 1}, {"REVIVER_SEED", -1, 1}}}, /* Ditto; Transform */
        {773, 16, 0, 0, 0, {718, 14, 575, 242}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}}}, /* Silvally (dark); Multi-Attack, Swords Dance, Parting Shot, Crunch */
        {475, 0, 0, 1, 0, {533, 427, 14, 400}, 3, {{"SCOPE_LENS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}, {"BERRY", 1, 1}}}, /* Gallade; Sacred Sword, Psycho Cut, Swords Dance, Night Slash */
        {567, 0, 0, 0, 0, {512, 444, 369, 355}, 2, {{"BERRY", 0, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}}}, /* Archeops; Acrobatics, Stone Edge, U-turn, Roost */
        {774, 0, 0, 1, 1, {504, 512, 408, 153}, 2, {{"WHITE_HERB", -1, 1}, {"ATTACK_TYPE_BOOSTER", 5, 1}}}, /* Minior (red-meteor); Shell Smash, Acrobatics, Power Gem, Explosion */
    }, 0, {0}, 0},
    {"gyro", "Gyro Zeppeli", "Nyo-ho!", 14 /* Desert */, -1, 6, {
        {750, 0, 0, 0, 0, {667, 484, 334, 776}, 3, {{"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 4, 1}, {"QUICK_CLAW", -1, 1}}}, /* Mudsdale; High Horsepower, Heavy Slam, Iron Defense, Body Press */
        {205, 0, 0, 1, 2, {229, 360, 191, 153}, 2, {{"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Forretress; Rapid Spin, Gyro Ball, Spikes, Explosion */
        {237, 0, 0, 0, 0, {229, 370, 813, 183}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"SHELL_BELL", -1, 1}}}, /* Hitmontop; Rapid Spin, Close Combat, Triple Axel, Mach Punch */
        {121, 0, 0, 1, 2, {229, 56, 94, 105}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 13, 1}}}, /* Starmie; Rapid Spin, Hydro Pump, Psychic, Recover */
        {947, 0, 0, 0, 0, {229, 438, 809, 191}, 3, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"FOCUS_BAND", -1, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Brambleghast; Rapid Spin, Power Whip, Poltergeist, Spikes */
        {760, 0, 0, 1, 0, {38, 359, 14, 409}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"SHELL_BELL", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Bewear; Double-Edge, Hammer Arm, Swords Dance, Drain Punch */
    }, 0, {0}, 0},
    {"diavolo", "Diavolo", "No one can escape the fate that was chosen for them.", 4 /* Metropolis */, -1, 6, {
        {483, 0, 0, 0, 0, {459, 430, 414, 396}, 1, {{"ATTACK_TYPE_BOOSTER", 15, 1}}}, /* Dialga; Roar of Time, Flash Cannon, Earth Power, Aura Sphere */
        {484, 0, 0, 0, 0, {460, 56, 126, 414}, 1, {{"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Palkia; Spacial Rend, Hydro Pump, Fire Blast, Earth Power */
        {981, 0, 0, 1, 1, {888, 304, 94, 417}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"ATTACK_TYPE_BOOSTER", 13, 1}, {"MULTI_LENS", -1, 1}}}, /* Farigiraf; Twin Beam, Hyper Voice, Psychic, Nasty Plot */
        {178, 0, 0, 0, 0, {248, 94, 355, 100}, 3, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"LEFTOVERS", -1, 1}}}, /* Xatu; Future Sight, Psychic, Roost, Teleport */
        {877, 0, 0, 0, 0, {783, 182, 575, 269}, 3, {{"ATTACK_TYPE_BOOSTER", 12, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"FOCUS_BAND", -1, 1}}}, /* Morpeko (full-belly); Aura Wheel, Protect, Parting Shot, Taunt */
        {430, 0, 0, 1, 0, {389, 413, 355, 269}, 3, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"SCOPE_LENS", -1, 1}}}, /* Honchkrow; Sucker Punch, Brave Bird, Roost, Taunt */
    }, 0, {0}, 0},
    {"kira", "Yoshikage Kira", "My name is Yoshikage Kira. I'm 33 years old.", 9 /* Lake */, -1, 6, {
        {908, 0, 0, 1, 0, {870, 282, 369, 583}, 3, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"MULTI_LENS", -1, 1}}}, /* Meowscarada; Flower Trick, Knock Off, U-turn, Play Rough */
        {442, 0, 0, 1, 0, {261, 506, 389, 492}, 3, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Spiritomb; Will-O-Wisp, Hex, Sucker Punch, Foul Play */
        {426, 0, 0, 0, 0, {153, 506, 261, 247}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Drifblim; Explosion, Hex, Will-O-Wisp, Shadow Ball */
        {251, 0, 0, 1, 1, {105, 94, 437, 369}, 3, {{"BERRY", 1, 1}, {"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 11, 1}}}, /* Celebi; Recover, Psychic, Leaf Storm, U-turn */
        {424, 0, 0, 0, 0, {252, 458, 369, 282}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"KINGS_ROCK", -1, 1}, {"GRIP_CLAW", -1, 1}}}, /* Ambipom; Fake Out, Double Hit, U-turn, Knock Off */
        {289, 0, 0, 0, 0, {38, 89, 389, 303}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}}}, /* Slaking; Double-Edge, Earthquake, Sucker Punch, Slack Off */
    }, 0, {0}, 0},
    {"pucci", "Enrico Pucci", "Do you believe in 'gravity'?", 29 /* Temple */, CC_JOJO_PASSIVE_MADE_IN_HEAVEN, 6, {
        {687, 0, 0, 0, 0, {276, 282, 427, 576}, 2, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"LEFTOVERS", -1, 1}}}, /* Malamar; Superpower, Knock Off, Psycho Cut, Topsy-Turvy */
        {826, 1, 0, 1, 0, {356, 94, 405, 105}, 2, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 6, 1}}}, /* Orbeetle (gigantamax); Gravity, Psychic, Bug Buzz, Recover */
        {894, 0, 0, 0, 0, {486, 521, 229, 182}, 2, {{"ATTACK_TYPE_BOOSTER", 12, 1}, {"MULTI_LENS", -1, 1}}}, /* Regieleki; Electro Ball, Volt Switch, Rapid Spin, Protect */
        {625, 0, 0, 1, 0, {442, 389, 14, 282}, 2, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Bisharp; Iron Head, Sucker Punch, Swords Dance, Knock Off */
        {292, 0, 0, 0, 0, {809, 404, 261, 425}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"BERRY", 1, 1}}}, /* Shedinja; Poltergeist, X-Scissor, Will-O-Wisp, Shadow Sneak */
        {493, 17, 0, 1, 0, {449, 105, 347, 414}, 1, {{"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Arceus (fairy); Judgment, Recover, Calm Mind, Earth Power */
    }, 0, {0}, 0},
    {"jolyne", "Jolyne Cujoh", "Gimme a break...", 7 /* Swamp */, -1, 6, {
        {918, 0, 0, 1, 0, {852, 564, 369, 884}, 3, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"FOCUS_BAND", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Spidops; Silk Trap, Sticky Web, U-turn, Pounce */
        {465, 0, 0, 0, 0, {438, 282, 79, 73}, 2, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"LEFTOVERS", -1, 1}}}, /* Tangrowth; Power Whip, Knock Off, Sleep Powder, Leech Seed */
        {675, 0, 0, 0, 0, {370, 282, 418, 575}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Pangoro; Close Combat, Knock Off, Bullet Punch, Parting Shot */
        {707, 0, 0, 1, 0, {115, 113, 86, 583}, 2, {{"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 17, 1}}}, /* Klefki; Reflect, Light Screen, Thunder Wave, Play Rough */
        {186, 0, 0, 0, 0, {503, 92, 182, 156}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"LEFTOVERS", -1, 1}, {"TOXIC_ORB", -1, 1}}}, /* Politoed; Scald, Toxic, Protect, Rest */
        {267, 0, 0, 0, 0, {483, 405, 314, 355}, 2, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}}}, /* Beautifly; Quiver Dance, Bug Buzz, Air Cutter, Roost */
    }, 0, {0}, 0},
    {"joseph", "Joseph Joestar", "Your next line is...", 5 /* Forest */, -1, 6, {
        {768, 0, 0, 0, 0, {660, 710, 141, 191}, 2, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Golisopod; First Impression, Liquidation, Leech Life, Spikes */
        {202, 0, 0, 0, 0, {68, 243, 227, 194}, 3, {{"LEFTOVERS", -1, 1}, {"FOCUS_BAND", -1, 1}, {"BERRY", 1, 1}}}, /* Wobbuffet; Counter, Mirror Coat, Encore, Destiny Bond */
        {185, 0, 0, 0, 0, {452, 444, 389, 446}, 3, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"LEFTOVERS", -1, 1}, {"QUICK_CLAW", -1, 1}}}, /* Sudowoodo; Wood Hammer, Stone Edge, Sucker Punch, Stealth Rock */
        {781, 0, 0, 0, 0, {438, 809, 677, 282}, 3, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}, {"GRIP_CLAW", -1, 1}}}, /* Dhelmise; Power Whip, Poltergeist, Anchor Shot, Knock Off */
        {466, 0, 0, 0, 0, {9, 8, 238, 521}, 1, {{"ATTACK_TYPE_BOOSTER", 12, 1}}}, /* Electivire; Thunder Punch, Ice Punch, Cross Chop, Volt Switch */
        {468, 0, 1, 0, 0, {403, 605, 355, 86}, 3, {{"KINGS_ROCK", -1, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"MULTI_LENS", -1, 1}}}, /* Togekiss; Air Slash, Dazzling Gleam, Roost, Thunder Wave */
    }, 0, {0}, 0},
    {"jotaro", "Jotaro Kujo", "Good grief.", 10 /* Seabed */, -1, 6, {
        {68, 0, 0, 1, 0, {370, 418, 282, 8}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"FOCUS_BAND", -1, 1}}}, /* Machamp; Close Combat, Bullet Punch, Knock Off, Ice Punch */
        {964, 0, 0, 0, 0, {857, 834, 370, 812}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}, {"SHELL_BELL", -1, 1}}}, /* Palafin (zero); Jet Punch, Wave Crash, Close Combat, Flip Turn */
        {376, 1, 0, 0, 0, {309, 428, 418, 89}, 2, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"QUICK_CLAW", -1, 1}}}, /* Metagross (mega); Meteor Mash, Zen Headbutt, Bullet Punch, Earthquake */
        {892, 1, 0, 0, 0, {818, 370, 453, 369}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"MULTI_LENS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Urshifu (rapid-strike); Surging Strikes, Close Combat, Aqua Jet, U-turn */
        {212, 1, 0, 0, 0, {418, 14, 369, 370}, 1, {{"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Scizor (mega); Bullet Punch, Swords Dance, U-turn, Close Combat */
        {395, 0, 0, 0, 0, {56, 430, 58, 355}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}, {"LEFTOVERS", -1, 1}}}, /* Empoleon; Hydro Pump, Flash Cannon, Ice Beam, Roost */
    }, 0, {0}, 0},
    {"dio", "DIO", "You thought your first kiss would be JoJo, but it was I, DIO!", 19 /* Graveyard */, CC_JOJO_PASSIVE_THE_WORLD, 6, {
        {1000, 0, 0, 0, 0, {874, 247, 417, 105}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Gholdengo; Make It Rain, Shadow Ball, Nasty Plot, Recover */
        {534, 0, 0, 0, 0, {409, 183, 282, 263}, 2, {{"FLAME_ORB", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Conkeldurr; Drain Punch, Mach Punch, Knock Off, Facade */
        {169, 0, 0, 1, 1, {413, 440, 355, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Crobat; Brave Bird, Cross Poison, Roost, U-turn */
        {983, 0, 0, 1, 0, {869, 389, 442, 14}, 3, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}, {"LEFTOVERS", -1, 1}}}, /* Kingambit; Kowtow Cleave, Sucker Punch, Iron Head, Swords Dance */
        {478, 1, 0, 0, 0, {58, 247, 191, 194}, 2, {{"ATTACK_TYPE_BOOSTER", 14, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Froslass (mega); Ice Beam, Shadow Ball, Spikes, Destiny Bond */
        {491, 0, 0, 0, 0, {464, 399, 417, 188}, 3, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"WIDE_LENS", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Darkrai; Dark Void, Dark Pulse, Nasty Plot, Sludge Bomb */
    }, 0, {0}, 0},
    {"koichi", "Koichi Hirose", "OK, master! Let's kill da ho! Beeetch!", 1 /* Plains */, -1, 5, {   /* nerfed (lorenzo 2026-11-22): Arcanine gone, only Riolu's Eviolite left */
        {441, 0, 0, 0, 0, {448, 586, 417, 355}, 0, {{0}}}, /* Chatot; Chatter, Boomburst, Nasty Plot, Roost */
        {235, 0, 0, 0, 0, {147, 564, 446, 575}, 0, {{0}}}, /* Smeargle; Spore, Sticky Web, Stealth Rock, Parting Shot */
        {841, 0, 0, 0, 0, {788, 200, 389, 369}, 0, {{0}}}, /* Flapple; Grav Apple, Outrage, Sucker Punch, U-turn */
        {920, 0, 0, 0, 0, {660, 369, 282, 389}, 0, {{0}}}, /* Lokix; First Impression, U-turn, Knock Off, Sucker Punch */
        {447, 0, 0, 0, 0, {410, 418, 383, 179}, 1, {{"EVIOLITE", -1, 1}}}, /* Riolu; Vacuum Wave, Bullet Punch, Copycat, Reversal */
    }, 0, {0}, 0},
    {"mista", "Guido Mista", "Pass, pass, pass!", 30 /* Slum */, -1, 6, {
        {870, 0, 0, 1, 0, {748, 370, 224, 398}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"SHELL_BELL", -1, 1}}}, /* Falinks; No Retreat, Close Combat, Megahorn, Poison Jab */
        {818, 0, 0, 0, 0, {745, 58, 369, 403}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"SCOPE_LENS", -1, 1}}}, /* Inteleon; Snipe Shot, Ice Beam, U-turn, Air Slash */
        {224, 0, 0, 0, 0, {190, 331, 126, 58}, 2, {{"MULTI_LENS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Octillery; Octazooka, Bullet Seed, Fire Blast, Ice Beam */
        {693, 0, 0, 0, 0, {352, 396, 399, 406}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"MULTI_LENS", -1, 1}}}, /* Clawitzer; Water Pulse, Aura Sphere, Dark Pulse, Dragon Pulse */
        {9, 0, 0, 0, 0, {56, 396, 58, 504}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"WHITE_HERB", -1, 1}, {"SHELL_BELL", -1, 1}}}, /* Blastoise; Hydro Pump, Aura Sphere, Ice Beam, Shell Smash */
        {359, 2, 0, 0, 0, {389, 282, 583, 14}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Absol (mega-z); Sucker Punch, Knock Off, Play Rough, Swords Dance */
    }, 0, {0}, 0},
    {"wamuu", "Wamuu", "Awaken, my masters!", -1, -1, 6, {
        {641, 0, 0, 0, 0, {846, 542, 366, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"MULTI_LENS", -1, 1}}}, /* Tornadus (incarnate); Bleakwind Storm, Hurricane, Tailwind, U-turn */
        {530, 0, 0, 0, 0, {89, 442, 14, 229}, 3, {{"ATTACK_TYPE_BOOSTER", 4, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}, {"LEFTOVERS", -1, 1}}}, /* Excadrill; Earthquake, Iron Head, Swords Dance, Rapid Spin */
        {450, 0, 0, 0, 0, {89, 303, 446, 18}, 2, {{"ATTACK_TYPE_BOOSTER", 4, 1}, {"LEFTOVERS", -1, 1}}}, /* Hippowdon; Earthquake, Slack Off, Stealth Rock, Whirlwind */
        {589, 0, 0, 0, 0, {224, 442, 370, 14}, 2, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Escavalier; Megahorn, Iron Head, Close Combat, Swords Dance */
        {865, 0, 0, 1, 0, {794, 660, 282, 413}, 2, {{"LEEK", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Sirfetch'd; Meteor Assault, First Impression, Knock Off, Brave Bird */
        {142, 0, 0, 0, 0, {444, 814, 89, 355}, 3, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"KINGS_ROCK", -1, 1}}}, /* Aerodactyl; Stone Edge, Dual Wingbeat, Earthquake, Roost */
    }, 0, {0}, 0},
    {"esidisi", "Esidisi", "HEEEEYYYY! THEY KILLED LOGGINS!", -1, -1, 6, {
        {721, 0, 0, 0, 0, {592, 53, 188, 414}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 9, 1}}}, /* Volcanion; Steam Eruption, Flamethrower, Sludge Bomb, Earth Power */
        {555, 0, 2, 0, 0, {394, 89, 369, 157}, 2, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"SHELL_BELL", -1, 1}}}, /* Darmanitan; Flare Blitz, Earthquake, U-turn, Rock Slide */
        {851, 0, 0, 0, 0, {680, 141, 489, 282}, 3, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"ATTACK_TYPE_BOOSTER", 6, 1}, {"SHELL_BELL", -1, 1}}}, /* Centiskorch; Fire Lash, Leech Life, Coil, Knock Off */
        {73, 0, 1, 1, 0, {503, 188, 229, 390}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Tentacruel; Scald, Sludge Bomb, Rapid Spin, Toxic Spikes */
        {793, 0, 0, 0, 0, {408, 482, 447, 446}, 2, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Nihilego; Power Gem, Sludge Wave, Grass Knot, Stealth Rock */
        {467, 0, 0, 0, 0, {126, 85, 411, 269}, 2, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"MULTI_LENS", -1, 1}}}, /* Magmortar; Fire Blast, Thunderbolt, Focus Blast, Taunt */
    }, 0, {0}, 0},
    {"valentine", "Funny Valentine", "My heart and actions are utterly unclouded... They are all those of 'Justice'.", 24 /* Abyss */, -1, 6, {
        {720, 0, 0, 0, 0, {593, 247, 411, 417}, 2, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Hoopa; Hyperspace Hole, Shadow Ball, Focus Blast, Nasty Plot */
        {628, 0, 0, 0, 0, {413, 370, 369, 355}, 2, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Braviary; Brave Bird, Close Combat, U-turn, Roost */
        {778, 0, 0, 0, 0, {583, 425, 14, 452}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"ATTACK_TYPE_BOOSTER", 17, 1}}}, /* Mimikyu (disguised); Play Rough, Shadow Sneak, Swords Dance, Wood Hammer */
        {925, 0, 0, 0, 0, {860, 44, 331, 882}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"WIDE_LENS", -1, 1}, {"KINGS_ROCK", -1, 1}}}, /* Maushold (four); Population Bomb, Bite, Bullet Seed, Tidy Up */
        {571, 0, 0, 1, 1, {399, 53, 188, 417}, 2, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"MULTI_LENS", -1, 1}}}, /* Zoroark; Dark Pulse, Flamethrower, Sludge Bomb, Nasty Plot */
        {719, 1, 0, 1, 0, {591, 585, 414, 446}, 2, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"ATTACK_TYPE_BOOSTER", 17, 1}}}, /* Diancie (mega); Diamond Storm, Moonblast, Earth Power, Stealth Rock */
    }, 0, {0}, 0},
    {"polnareff", "Jean Pierre Polnareff", "I'm gonna turn you into a pincushion!", 20 /* Dojo */, -1, 5, {
        {589, 0, 0, 0, 0, {442, 224, 529, 14}, 2, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"ATTACK_TYPE_BOOSTER", 6, 1}}}, /* Escavalier; Iron Head, Megahorn, Drill Run, Swords Dance */
        {681, 0, 0, 1, 0, {442, 425, 14, 588}, 3, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}, {"LEFTOVERS", -1, 1}}}, /* Aegislash (shield); Iron Head, Shadow Sneak, Swords Dance, King’s Shield */
        {798, 0, 0, 0, 0, {348, 533, 684, 282}, 2, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Kartana; Leaf Blade, Sacred Sword, Smart Strike, Knock Off */
        {676, 0, 0, 0, 0, {216, 538, 389, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}}}, /* Furfrou; return.name, Cotton Guard, Sucker Punch, U-turn */
        {1024, 0, 0, 0, 0, {906, 414, 347, 229}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}, {"SHELL_BELL", -1, 1}}}, /* Terapagos; Tera Starstorm, Earth Power, Calm Mind, Rapid Spin */
    }, 0, {0}, 0},
    {"risotto", "Risotto Nero", "Metallica!", 13 /* Cave */, -1, 5, {
        {808, 0, 0, 0, 0, {430, 86, 151, 92}, 3, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"EVIOLITE", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Meltan; Flash Cannon, Thunder Wave, Acid Armor, Toxic */
        {598, 0, 0, 0, 0, {360, 438, 73, 191}, 2, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"ATTACK_TYPE_BOOSTER", 11, 1}}}, /* Ferrothorn; Gyro Ball, Power Whip, Leech Seed, Spikes */
        {352, 0, 0, 0, 0, {425, 282, 409, 252}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Kecleon; Shadow Sneak, Knock Off, Drain Punch, Fake Out */
        {461, 0, 0, 0, 0, {813, 282, 420, 14}, 3, {{"ATTACK_TYPE_BOOSTER", 14, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"MULTI_LENS", -1, 1}}}, /* Weavile; Triple Axel, Knock Off, Ice Shard, Swords Dance */
        {862, 0, 2, 1, 1, {792, 282, 263, 370}, 3, {{"FLAME_ORB", -1, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Obstagoon; Obstruct, Knock Off, Facade, Close Combat */
    }, 0, {0}, 0},
    {"stroheim", "Rudolf von Stroheim", "You fool! German science is the finest in the world!", 41 /* Laboratory */, -1, 5, {
        {649, 2, 0, 0, 0, {546, 442, 369, 153}, 4, {{"MULTI_LENS", -1, 2}, {"GRIP_CLAW", -1, 4}, {"REVIVER_SEED", -1, 1}, {"SOUL_DEW", -1, 3}}}, /* Genesect (burn); Techno Blast, Iron Head, U-turn, Explosion */
        {992, 0, 0, 0, 0, {409, 9, 370, 252}, 3, {{"LEFTOVERS", -1, 4}, {"BASE_STAT_BOOSTER", 1, 10}, {"BASE_STAT_BOOSTER", 2, 10}}}, /* Iron Hands; Drain Punch, Thunder Punch, Close Combat, Fake Out */
        {295, 0, 2, 0, 0, {586, 315, 304, 59}, 1, {{"ATTACK_TYPE_BOOSTER", 0, 5}}}, /* Exploud; Boomburst, Overheat, Hyper Voice, Blizzard */
        {966, 0, 2, 0, 0, {859, 441, 508, 442}, 4, {{"QUICK_CLAW", -1, 3}, {"ATTACK_TYPE_BOOSTER", 8, 4}, {"SHELL_BELL", -1, 3}, {"FOCUS_BAND", -1, 2}}}, /* Revavroom; Spin Out, Gunk Shot, Shift Gear, Iron Head */
        {376, 1, 0, 0, 0, {309, 418, 428, 153}, 1, {{"ATTACK_TYPE_BOOSTER", 8, 5}}}, /* Metagross (mega); Meteor Mash, Bullet Punch, Zen Headbutt, Explosion */
    }, 0, {0}, 0},
    {"caesar", "Caesar Zeppeli", "My spirit is unbending! Witness the ultimate Zeppeli technique!", 20 /* Dojo */, -1, 4, {
        {730, 0, 0, 1, 0, {664, 585, 453, 304}, 0, {{0}}}, /* Primarina; Sparkling Aria, Moonblast, Aqua Jet, Hyper Voice */
        {752, 0, 0, 0, 0, {710, 679, 564, 277}, 1, {{"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Araquanid; Liquidation, Lunge, Sticky Web, Magic Coat */
        {700, 0, 0, 0, 0, {304, 585, 595, 98}, 0, {{0}}}, /* Sylveon; Hyper Voice, Moonblast, Mystical Fire, Quick Attack */
        {448, 0, 0, 1, 2, {396, 370, 418, 245}, 1, {{"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Lucario; Aura Sphere, Close Combat, Bullet Punch, Extreme Speed */
    }, 0, {0}, 0},
    {"kakyoin", "Noriaki Kakyoin", "Rororerorerorerorerorero", 29 /* Temple */, -1, 4, {
        {254, 1, 0, 0, 0, {437, 406, 411, 202}, 1, {{"ATTACK_TYPE_BOOSTER", 11, 1}}}, /* Sceptile (mega); Leaf Storm, Dragon Pulse, Focus Blast, Giga Drain */
        {73, 0, 0, 0, 0, {503, 188, 282, 390}, 0, {{0}}}, /* Tentacruel; Scald, Sludge Bomb, Knock Off, Toxic Spikes */
        {724, 0, 0, 0, 0, {662, 348, 389, 369}, 0, {{0}}}, /* Decidueye; Spirit Shackle, Leaf Blade, Sucker Punch, U-turn */
        {429, 0, 0, 1, 1, {247, 595, 417, 220}, 1, {{"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Mismagius; Shadow Ball, Mystical Fire, Nasty Plot, Pain Split */
    }, 0, {0}, 0},
    {"avdol", "Muhammad Avdol", "I truly did plan to send you to the hospital", 18 /* Volcano */, -1, 4, {
        {257, 0, 0, 0, 0, {394, 370, 182, 14}, 2, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Blaziken; Flare Blitz, Close Combat, Protect, Swords Dance */
        {655, 0, 0, 0, 0, {53, 94, 595, 347}, 2, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"ATTACK_TYPE_BOOSTER", 13, 1}}}, /* Delphox; Flamethrower, Psychic, Mystical Fire, Calm Mind */
        {663, 0, 0, 0, 0, {413, 394, 355, 369}, 3, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"ATTACK_TYPE_BOOSTER", 9, 1}, {"SHELL_BELL", -1, 1}}}, /* Talonflame; Brave Bird, Flare Blitz, Roost, U-turn */
        {178, 0, 0, 0, 0, {94, 403, 355, 100}, 3, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"LEFTOVERS", -1, 1}}}, /* Xatu; Psychic, Air Slash, Roost, Teleport */
    }, 0, {0}, 0},
    {"vanillaice", "Vanilla Ice", "Into the void of absolute dark! Cream, devour them!", 24 /* Abyss */, -1, 4, {
        {799, 0, 0, 0, 0, {242, 407, 484, 282}, 2, {{"LEFTOVERS", -1, 1}, {"SHELL_BELL", -1, 1}}}, /* Guzzlord; Crunch, Dragon Rush, Heavy Slam, Knock Off */
        {362, 0, 0, 0, 0, {38, 89, 242, 153}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"ATTACK_TYPE_BOOSTER", 5, 1}}}, /* Glalie; Double-Edge, Earthquake, Crunch, Explosion */
        {442, 0, 0, 1, 0, {389, 506, 261, 492}, 0, {{0}}}, /* Spiritomb; Sucker Punch, Hex, Will-O-Wisp, Foul Play */
        {635, 0, 0, 0, 0, {399, 434, 126, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 15, 1}}}, /* Hydreigon; Dark Pulse, Draco Meteor, Fire Blast, U-turn */
    }, 0, {0}, 0},
    {"holhorse", "Hol Horse", "I'm strictly a 'Number Two' guy, but I never miss my mark!", 23 /* Wasteland */, -1, 3, {
        {818, 0, 0, 0, 0, {745, 58, 369, 403}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"SCOPE_LENS", -1, 1}}}, /* Inteleon; Snipe Shot, Ice Beam, U-turn, Air Slash */
        {750, 0, 0, 0, 0, {667, 334, 776, 484}, 0, {{0}}}, /* Mudsdale; High Horsepower, Iron Defense, Body Press, Heavy Slam */
        {128, 0, 0, 0, 0, {38, 89, 370, 37}, 1, {{"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Tauros; Double-Edge, Earthquake, Close Combat, Thrash */
    }, 0, {0}, 0},
    {"iggy", "Iggy", "*farts*", 14 /* Desert */, -1, 4, {
        {943, 0, 0, 0, 0, {242, 583, 746, 575}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Mabosstiff; Crunch, Play Rough, Jaw Lock, Parting Shot */
        {450, 0, 0, 0, 0, {89, 303, 446, 18}, 0, {{0}}}, /* Hippowdon; Earthquake, Slack Off, Stealth Rock, Whirlwind */
        {770, 0, 0, 0, 0, {414, 247, 659, 202}, 1, {{"ATTACK_TYPE_BOOSTER", 4, 1}}}, /* Palossand; Earth Power, Shadow Ball, Shore Up, Giga Drain */
        {508, 0, 0, 0, 0, {216, 242, 583, 276}, 1, {{"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Stoutland; Return, Crunch, Play Rough, Superpower */
    }, 0, {0}, 0},
    {"narancia", "Narancia Ghirga", "Aerosmith! Volare Via!", 21 /* Factory */, -1, 4, {
        {886, 0, 0, 0, 0, {751, 566, 369, 389}, 2, {{"ATTACK_TYPE_BOOSTER", 15, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Drakloak; Dragon Darts, Phantom Force, U-turn, Sucker Punch */
        {381, 1, 0, 0, 0, {94, 406, 396, 355}, 2, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 15, 1}}}, /* Latios (mega); Psychic, Dragon Pulse, Aura Sphere, Roost */
        {715, 0, 0, 0, 0, {586, 434, 542, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"ATTACK_TYPE_BOOSTER", 15, 1}}}, /* Noivern; Boomburst, Draco Meteor, Hurricane, U-turn */
        {227, 0, 0, 0, 0, {413, 355, 191, 18}, 3, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}, {"LEFTOVERS", -1, 1}}}, /* Skarmory; Brave Bird, Roost, Spikes, Whirlwind */
    }, 0, {0}, 0},
    {"bruno", "Bruno Bucciarati", "Arrivederci! Sticky Fingers!", 8 /* Beach */, -1, 4, {
        {354, 1, 0, 1, 0, {809, 425, 282, 194}, 0, {{0}}}, /* Banette (mega); Poltergeist, Shadow Sneak, Knock Off, Destiny Bond */
        {625, 0, 0, 1, 0, {442, 389, 282, 14}, 0, {{0}}}, /* Bisharp; Iron Head, Sucker Punch, Knock Off, Swords Dance */
        {707, 0, 0, 0, 0, {583, 191, 86, 115}, 0, {{0}}}, /* Klefki; Play Rough, Spikes, Thunder Wave, Reflect */
        {448, 0, 0, 0, 0, {396, 370, 309, 245}, 1, {{"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Lucario; Aura Sphere, Close Combat, Meteor Mash, Extreme Speed */
    }, 0, {0}, 0},
    {"abbacchio", "Leone Abbacchio", "Moody Blues!", 19 /* Graveyard */, -1, 3, {
        {233, 0, 0, 1, 0, {161, 58, 85, 105}, 1, {{"EVIOLITE", -1, 1}}}, /* Porygon2; Tri Attack, Ice Beam, Thunderbolt, Recover */
        {132, 0, 2, 1, 0, {144, 0, 0, 0}, 0, {{0}}}, /* Ditto; Transform */
        {571, 0, 0, 0, 0, {399, 53, 188, 417}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Zoroark; Dark Pulse, Flamethrower, Sludge Bomb, Nasty Plot */
    }, 0, {0}, 0},
    {"weather", "Weather Report", "You... The kind of evil that doesn't realize that it's evil... is the worst one there is...", 30 /* Slum */, -1, 5, {
        {351, 0, 0, 0, 0, {311, 126, 56, 59}, 0, {{0}}}, /* Castform; Weather Ball, Fire Blast, Hydro Pump, Blizzard */
        {186, 0, 2, 0, 0, {503, 92, 182, 156}, 1, {{"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Politoed; Scald, Toxic, Protect, Rest */
        {6706, 0, 0, 0, 0, {406, 188, 330, 151}, 1, {{"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Goodra; Dragon Pulse, Sludge Bomb, Muddy Water, Acid Armor */
        {641, 0, 0, 0, 0, {846, 542, 369, 282}, 0, {{0}}}, /* Tornadus (incarnate); Bleakwind Storm, Hurricane, U-turn, Knock Off */
        {642, 1, 0, 0, 0, {85, 521, 411, 447}, 1, {{"MULTI_LENS", -1, 1}}}, /* Thundurus (therian); Thunderbolt, Volt Switch, Focus Blast, Grass Knot */
    }, 0, {0}, 0},
    {"johnny", "Johnny Joestar", "Gyro! What are you trying to make her do?! She's only 14!", 14 /* Desert */, -1, 4, {
        {4080, 0, 0, 1, 1, {801, 188, 94, 303}, 0, {{0}}}, /* Slowbro; Shell Side Arm, Sludge Bomb, Psychic, Slack Off */
        {237, 0, 0, 0, 0, {229, 370, 813, 389}, 1, {{"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Hitmontop; Rapid Spin, Close Combat, Triple Axel, Sucker Punch */
        {896, 0, 0, 0, 0, {556, 667, 370, 14}, 0, {{0}}}, /* Glastrier; Icicle Crash, High Horsepower, Close Combat, Swords Dance */
        {724, 0, 0, 1, 0, {662, 348, 389, 369}, 1, {{"SCOPE_LENS", -1, 1}}}, /* Decidueye; Spirit Shackle, Leaf Blade, Sucker Punch, U-turn */
    }, 0, {0}, 0},
    {"sandman", "Sandman", "I will cross the continent with my feet alone.", 14 /* Desert */, -1, 4, {
        {291, 0, 0, 0, 0, {814, 404, 14, 182}, 0, {{0}}}, /* Ninjask; Dual Wingbeat, X-Scissor, Swords Dance, Protect */
        {849, 0, 0, 0, 0, {786, 586, 188, 521}, 1, {{"ATTACK_TYPE_BOOSTER", 12, 1}}}, /* Toxtricity (amped); Overdrive, Boomburst, Sludge Bomb, Volt Switch */
        {85, 0, 0, 0, 0, {413, 65, 26, 14}, 0, {{0}}}, /* Dodrio; Brave Bird, Drill Peck, Jump Kick, Swords Dance */
        {330, 0, 0, 0, 0, {89, 337, 369, 349}, 2, {{"ATTACK_TYPE_BOOSTER", 4, 1}, {"ATTACK_TYPE_BOOSTER", 15, 1}}}, /* Flygon; Earthquake, Dragon Claw, U-turn, Dragon Dance */
    }, 0, {0}, 0},
    {"diego", "Diego Brando", "Ohh...are you wondering why I was eating those rocks?", 23 /* Wasteland */, -1, 5, {
        {253, 0, 0, 0, 0, {348, 404, 98, 14}, 0, {{0}}}, /* Grovyle; Leaf Blade, X-Scissor, Quick Attack, Swords Dance */
        {880, 0, 0, 0, 0, {754, 200, 89, 332}, 1, {{"ATTACK_TYPE_BOOSTER", 12, 1}}}, /* Dracozolt; Bolt Beak, Outrage, Earthquake, Aerial Ace */
        {567, 0, 0, 0, 0, {512, 444, 89, 369}, 1, {{"ATTACK_TYPE_BOOSTER", 2, 1}}}, /* Archeops; Acrobatics, Stone Edge, Earthquake, U-turn */
        {445, 0, 0, 0, 0, {89, 200, 14, 444}, 1, {{"ATTACK_TYPE_BOOSTER", 4, 1}}}, /* Garchomp; Earthquake, Outrage, Swords Dance, Stone Edge */
        {483, 0, 0, 0, 0, {459, 430, 414, 396}, 0, {{0}}}, /* Dialga; Roar of Time, Flash Cannon, Earth Power, Aura Sphere */
    }, 0, {0}, 0},
    {"shigechi", "Shigechi", "You won't get nothing from me! Not a single yen!", 26 /* Construction Site */, -1, 3, {
        {925, 0, 0, 0, 0, {860, 44, 369, 882}, 1, {{"KINGS_ROCK", -1, 1}}}, /* Maushold (four); Population Bomb, Bite, U-turn, Tidy Up */
        {999, 1, 0, 0, 0, {310, 33, 168, 109}, 0, {{0}}}, /* Gimmighoul (roaming); Astonish, Tackle, Thief, Confuse Ray */
        {820, 0, 0, 0, 0, {34, 242, 89, 747}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"BERRY", 0, 1}}}, /* Greedent; Body Slam, Crunch, Earthquake, Stuff Cheeks */
    }, 0, {0}, 0},
    {"josuke", "Josuke Higashikata", "What did you just say about my hair?!", 1 /* Plains */, -1, 5, {
        {719, 0, 0, 1, 0, {591, 585, 215, 776}, 1, {{"LEFTOVERS", -1, 1}}}, /* Diancie; Diamond Storm, Moonblast, Heal Bell, Body Press */
        {626, 0, 0, 0, 0, {543, 89, 370, 14}, 0, {{0}}}, /* Bouffalant; Head Charge, Earthquake, Close Combat, Swords Dance */
        {675, 0, 0, 0, 0, {370, 282, 418, 575}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Pangoro; Close Combat, Knock Off, Bullet Punch, Parting Shot */
        {764, 0, 1, 0, 0, {666, 577, 347, 235}, 0, {{0}}}, /* Comfey; Floral Healing, Draining Kiss, Calm Mind, Synthesis */
        {936, 0, 0, 1, 0, {890, 797, 396, 347}, 1, {{"ATTACK_TYPE_BOOSTER", 9, 1}}}, /* Armarouge; Armor Cannon, Expanding Force, Aura Sphere, Calm Mind */
    }, 0, {0}, 0},
    {"rohan", "Rohan Kishibe", "It's great! I've found the perfect inspiration for my comics!!", 41 /* Laboratory */, -1, 4, {
        {235, 0, 0, 0, 0, {147, 564, 417, 226}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"BERRY", 1, 1}}}, /* Smeargle; Spore, Sticky Web, Nasty Plot, Baton Pass */
        {765, 0, 0, 1, 0, {689, 94, 492, 433}, 2, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"LEFTOVERS", -1, 1}}}, /* Oranguru; Instruct, Psychic, Foul Play, Trick Room */
        {168, 0, 0, 0, 0, {564, 672, 224, 389}, 2, {{"ATTACK_TYPE_BOOSTER", 3, 1}, {"ATTACK_TYPE_BOOSTER", 6, 1}}}, /* Ariados; Sticky Web, Toxic Thread, Megahorn, Sucker Punch */
        {201, 17, 0, 0, 0, {237, 0, 0, 0}, 1, {{"ATTACK_TYPE_BOOSTER", 13, 1}}}, /* Unown (r); Hidden Power */
    }, 0, {0}, 0},
    {"giorno", "Giorno Giovanna", "I, Giorno Giovanna, have a dream!", 2 /* Grass */, -1, 5, {
        {1000, 0, 0, 0, 0, {874, 247, 417, 105}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Gholdengo; Make It Rain, Shadow Ball, Nasty Plot, Recover */
        {3, 1, 0, 0, 0, {202, 188, 414, 235}, 2, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Venusaur (mega); Giga Drain, Sludge Bomb, Earth Power, Synthesis */
        {166, 0, 0, 1, 0, {409, 183, 405, 115}, 0, {{0}}}, /* Ledian; Drain Punch, Mach Punch, Bug Buzz, Reflect */
        {510, 0, 0, 0, 0, {282, 389, 369, 252}, 0, {{0}}}, /* Liepard; Knock Off, Sucker Punch, U-turn, Fake Out */
        {716, 0, 0, 1, 2, {601, 585, 411, 87}, 1, {{"ATTACK_TYPE_BOOSTER", 17, 1}}}, /* Xerneas (neutral); Geomancy, Moonblast, Focus Blast, Thunder */
    }, 0, {0}, 0},
    /* the third batch (lorenzo 2026-11-22) */
    {"ghiaccio", "Ghiaccio", "learn a little italian puttana! Its called Venezia", 15 /* Ice Cave */, -1, 5, {
        {615, 0, 0, 0, 0, {883, 115, 573, 105}, 0, {{0}}}, /* Cryogonal (Levitate); Snowscape, Reflect, Freeze-Dry, Recover; Light Clay: not in PokeRogue, ask */
        {2028, 0, 2, 0, 0, {813, 442, 229, 89}, 0, {{0}}}, /* Alolan Sandslash (Slush Rush); Triple Axel, Iron Head, Rapid Spin, Earthquake */
        {461, 0, 0, 0, 0, {400, 556, 420, 14}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Weavile (Pressure), Black Glasses; Night Slash, Icicle Crash, Ice Shard, Swords Dance */
        {362, 0, 0, 0, 0, {861, 242, 269, 153}, 0, {{0}}}, /* Glalie (Inner Focus); Ice Spinner, Crunch, Taunt, Explosion */
        {998, 0, 0, 0, 0, {862, 333, 89, 349}, 1, {{"ATTACK_TYPE_BOOSTER", 14, 1}}}, /* Baxcalibur (Thermal Exchange), Never-Melt Ice; Glaive Rush, Icicle Spear, Earthquake, Dragon Dance */
    }, 1, {31 /* Snowy Forest */}, 0},
    {"prosciutto", "Prosciutto & Pesci", "I'm angry at your spiritual weakness, Pesci.", 30 /* Slum */, -1, 4, {
        {4110, 0, 1, 0, 0, {790, 188, 261, 220}, 1, {{"LEFTOVERS", -1, 1}}}, /* Galarian Weezing (Neutralizing Gas), Black Sludge is no held item in PokeRogue -> Leftovers (asked); Strange Steam, Sludge Bomb, Will-O-Wisp, Pain Split */
        {781, 0, 0, 0, 0, {677, 438, 566, 14}, 1, {{"LEFTOVERS", -1, 1}}}, /* Dhelmise (Steelworker), Leftovers; Anchor Shot, Power Whip, Phantom Force, Swords Dance */
        {354, 1, 0, 0, 0, {421, 261, 194, 269}, 0, {{0}}}, /* Banette (mega) (Prankster); Shadow Claw, Will-O-Wisp, Destiny Bond, Taunt */
        {904, 0, 2, 0, 0, {839, 242, 710, 453}, 0, {{0}}}, /* Overqwil (Intimidate); Barb Barrage, Crunch, Liquidation, Aqua Jet */
    }, 0, {0}, 1 /* a double battle */},
    {"jodio", "Jodio Joestar", "you were jamming out to Dua Lipa too hard", 8 /* Beach */, -1, 5, {
        {279, 0, 1, 0, 0, {311, 542, 369, 355}, 1, {{"MYSTICAL_ROCK", -1, 1}}}, /* Pelipper (Drizzle), Damp Rock -> Mystical Rock (ask); Weather Ball, Hurricane, U-turn, Roost */
        {752, 0, 0, 0, 0, {710, 141, 564, 243}, 1, {{"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Araquanid (Water Bubble), Mystic Water; Liquidation, Leech Life, Sticky Web, Mirror Coat */
        {302, 0, 2, 0, 0, {261, 282, 492, 105}, 0, {{0}}}, /* Sableye (Prankster); Will-O-Wisp, Knock Off, Foul Play, Recover */
        {560, 1, 0, 0, 0, {409, 282, 339, 398}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Scrafty (mega) (Intimidate), Black Glasses; Drain Punch, Knock Off, Bulk Up, Poison Jab */
        {1000, 0, 0, 0, 0, {874, 247, 87, 417}, 0, {{0}}}, /* Gholdengo (Good as Gold); Make It Rain, Shadow Ball, Thunder, Nasty Plot */
    }, 0, {0}, 0},
    {"jonathan", "Jonathan Joestar", "I strive to be a true gentleman!", 22 /* Ruins */, -1, 4, {
        {508, 0, 0, 0, 0, {216, 583, 242, 528}, 1, {{"LEFTOVERS", -1, 1}}}, /* Stoutland (Intimidate), Leftovers; Return, Play Rough, Crunch, Wild Charge */
        {149, 0, 2, 0, 0, {349, 245, 7, 9}, 0, {{0}}}, /* Dragonite (Multiscale); Dragon Dance, Extreme Speed, Fire Punch, Thunder Punch; no item (lorenzo: Weakness Policy is not in PokeRogue) */
        {936, 0, 0, 0, 0, {890, 396, 94, 241}, 1, {{"ATTACK_TYPE_BOOSTER", 9, 1}}}, /* Armarouge (Flash Fire), Charcoal; Armor Cannon, Aura Sphere, Psychic, Sunny Day */
        {475, 1, 0, 0, 0, {533, 427, 370, 14}, 0, {{0}}}, /* Gallade (mega) (Inner Focus); Sacred Sword, Psycho Cut, Close Combat, Swords Dance */
    }, 0, {0}, 0},
    /* Tooru (lorenzo 2026-11-22: the species, Absol's Super Luck and Scope Lens, Mr. Rime's roles, Cofagrigus's Mummy and moves, Darkrai's Bad
     * Dreams and Dark Void; 2026-11-23 "generate the moveset yourself": the other moves, abilities and items are Claude's picks) */
    {"tooru", "Tooru", "Looks like we ran into each other at a good time.", 5 /* Forest */, -1, 6, {
        {465, 0, 2, 0, 0, {438, 282, 79, 73}, 1, {{"LEFTOVERS", -1, 1}}}, /* Tangrowth (Regenerator), Leftovers; Power Whip, Knock Off, Sleep Powder, Leech Seed */
        {359, 0, 1, 0, 0, {400, 427, 389, 14}, 1, {{"SCOPE_LENS", -1, 1}}}, /* Absol (Super Luck), Scope Lens; Night Slash, Psycho Cut, Sucker Punch, Swords Dance */
        {866, 0, 1, 0, 0, {115, 113, 229, 573}, 1, {{"LEFTOVERS", -1, 1}}}, /* Mr. Rime (Screen Cleaner), Leftovers; Reflect, Light Screen, Rapid Spin, Freeze-Dry */
        {934, 0, 0, 0, 0, {864, 105, 446, 776}, 1, {{"LEFTOVERS", -1, 1}}}, /* Garganacl (Purifying Salt), Leftovers; Salt Cure, Recover, Stealth Rock, Body Press */
        {563, 0, 0, 0, 0, {228, 220, 506, 261}, 1, {{"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Cofagrigus (Mummy), Spell Tag; Pursuit (P: no double damage on a switch-out), Pain Split, Hex, Will-O-Wisp */
        {491, 0, 0, 0, 0, {464, 399, 417, 188}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Darkrai (Bad Dreams), Black Glasses; Dark Void, Dark Pulse, Nasty Plot, Sludge Bomb */
    }, 0, {0}, 0},
    /* Okuyasu (lorenzo 2026-11-22/23: species, moves, items, default abilities; Pancham's moves were "any fitting moves", Claude's pick;
     * "white scarf" read as the Silk Scarf). Space, where trainers come one wave in 16 ("keep him in space"). */
    {"okuyasu", "Okuyasu Nijimura", "I'm just dying to erase your goddamn existence", 25 /* Space */, -1, 6, {
        {484, 0, 0, 0, 0, {460, 56, 414, 126}, 0, {{0}}}, /* Palkia; Spacial Rend, Hydro Pump, Earth Power, Fire Blast */
        {992, 0, 0, 0, 0, {409, 528, 8, 484}, 0, {{0}}}, /* Iron Hands; Drain Punch, Wild Charge, Ice Punch, Heavy Slam */
        {891, 0, 0, 0, 0, {370, 339, 8, 369}, 1, {{"EVIOLITE", -1, 1}}}, /* Kubfu, Eviolite; Close Combat, Bulk Up, Ice Punch, U-turn */
        {674, 0, 0, 0, 0, {509, 282, 242, 575}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Pancham, Black Glasses; Circle Throw, Knock Off, Crunch, Parting Shot */
        {720, 0, 0, 0, 0, {593, 428, 409, 441}, 0, {{0}}}, /* Hoopa; Hyperspace Hole, Zen Headbutt, Drain Punch, Gunk Shot */
        {143, 0, 0, 0, 0, {174, 34, 89, 242}, 2, {{"BERRY", 0, 1}, {"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Snorlax, Sitrus Berry, Silk Scarf; Curse, Body Slam, Earthquake, Crunch */
    }, 0, {0}, 0},
};

const unsigned cc_jojo_count = sizeof cc_jojo_chars / sizeof *cc_jojo_chars;
