/* custom-core: the JoJo characters (data, generated once from lorenzo's lineup file of 2026-10-18/19 and edited by hand; see jojo.h).
 *
 * One row per character: id, display name, iconic line, biome (BiomeId; -1 = not in a biome pool: the Pillar Men), the background
 * passive (an ability key; NULL = none) and up to six members. A member is the species, the form index, the ability slot (0, 1 = the
 * form's abilities, 2 = hidden), shiny (0/1) and its variant tier (0..2, 0 unless the species has variant palettes), four move ids and
 * the held items {modifierTypes key, argument, stack}. The comments name the species, form and moves. Held items upstream does not have
 * were dropped (lorenzo, 2026-10-18: no substitutions); stacks are capped as upstream caps them. The ids are checked against
 * battle-core's data by game-core/tests/custom_trainer_host_test.c; a clean sweep through battle-core means no unported id was
 * consulted, it is not upstream parity. */
#include "cc/jojo.h"

const cc_jojo_char cc_jojo_chars[] = {
    {"kars", "Kars", "I shall become the ultimate lifeform!", -1, NULL, 6, {
        {151, 0, 0, 1, 1, {144, 94, 396, 105}, 3, {{"BERRY", 1, 1}, {"FOCUS_BAND", -1, 1}, {"GOLDEN_EGG", -1, 1}}}, /* Mew; Transform, Psychic, Aura Sphere, Recover */
        {132, 0, 2, 0, 0, {144, 0, 0, 0}, 3, {{"RARE_SPECIES_STAT_BOOSTER", 2, 1}, {"RARE_SPECIES_STAT_BOOSTER", 3, 1}, {"REVIVER_SEED", -1, 1}}}, /* Ditto; Transform */
        {773, 16, 0, 0, 0, {718, 14, 575, 242}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}}}, /* Silvally (dark); Multi-Attack, Swords Dance, Parting Shot, Crunch */
        {475, 0, 0, 1, 0, {533, 427, 14, 400}, 3, {{"SCOPE_LENS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}, {"BERRY", 1, 1}}}, /* Gallade; Sacred Sword, Psycho Cut, Swords Dance, Night Slash */
        {567, 0, 0, 0, 0, {512, 444, 369, 355}, 2, {{"BERRY", 0, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}}}, /* Archeops; Acrobatics, Stone Edge, U-turn, Roost */
        {774, 0, 0, 1, 1, {504, 512, 408, 153}, 2, {{"WHITE_HERB", -1, 1}, {"ATTACK_TYPE_BOOSTER", 5, 1}}}, /* Minior (red-meteor); Shell Smash, Acrobatics, Power Gem, Explosion */
    }},
    {"gyro", "Gyro Zeppeli", "Nyo-ho!", 14 /* Desert */, NULL, 6, {
        {750, 0, 0, 0, 0, {667, 484, 334, 776}, 3, {{"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 4, 1}, {"QUICK_CLAW", -1, 1}}}, /* Mudsdale; High Horsepower, Heavy Slam, Iron Defense, Body Press */
        {205, 0, 0, 1, 2, {229, 360, 191, 153}, 2, {{"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Forretress; Rapid Spin, Gyro Ball, Spikes, Explosion */
        {237, 0, 0, 0, 0, {229, 370, 813, 183}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"SHELL_BELL", -1, 1}}}, /* Hitmontop; Rapid Spin, Close Combat, Triple Axel, Mach Punch */
        {121, 0, 0, 1, 2, {229, 56, 94, 105}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 13, 1}}}, /* Starmie; Rapid Spin, Hydro Pump, Psychic, Recover */
        {947, 0, 0, 0, 0, {229, 438, 809, 191}, 3, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"FOCUS_BAND", -1, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Brambleghast; Rapid Spin, Power Whip, Poltergeist, Spikes */
        {760, 0, 0, 1, 0, {38, 359, 14, 409}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"SHELL_BELL", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Bewear; Double-Edge, Hammer Arm, Swords Dance, Drain Punch */
    }},
    {"diavolo", "Diavolo", "No one can escape the fate that was chosen for them.", 4 /* Metropolis */, NULL, 6, {
        {483, 0, 0, 0, 0, {459, 430, 414, 396}, 1, {{"ATTACK_TYPE_BOOSTER", 15, 1}}}, /* Dialga; Roar of Time, Flash Cannon, Earth Power, Aura Sphere */
        {484, 0, 0, 0, 0, {460, 56, 126, 414}, 1, {{"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Palkia; Spacial Rend, Hydro Pump, Fire Blast, Earth Power */
        {981, 0, 0, 1, 1, {888, 304, 94, 417}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"ATTACK_TYPE_BOOSTER", 13, 1}, {"MULTI_LENS", -1, 1}}}, /* Farigiraf; Twin Beam, Hyper Voice, Psychic, Nasty Plot */
        {178, 0, 0, 0, 0, {248, 94, 355, 100}, 3, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"LEFTOVERS", -1, 1}}}, /* Xatu; Future Sight, Psychic, Roost, Teleport */
        {877, 0, 0, 0, 0, {783, 182, 575, 269}, 3, {{"ATTACK_TYPE_BOOSTER", 12, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"FOCUS_BAND", -1, 1}}}, /* Morpeko (full-belly); Aura Wheel, Protect, Parting Shot, Taunt */
        {430, 0, 0, 1, 0, {389, 413, 355, 269}, 3, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"SCOPE_LENS", -1, 1}}}, /* Honchkrow; Sucker Punch, Brave Bird, Roost, Taunt */
    }},
    {"kira", "Yoshikage Kira", "My name is Yoshikage Kira. I'm 33 years old.", 9 /* Lake */, NULL, 6, {
        {908, 0, 0, 1, 0, {870, 282, 369, 583}, 3, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"MULTI_LENS", -1, 1}}}, /* Meowscarada; Flower Trick, Knock Off, U-turn, Play Rough */
        {442, 0, 0, 1, 0, {261, 506, 389, 492}, 3, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Spiritomb; Will-O-Wisp, Hex, Sucker Punch, Foul Play */
        {426, 0, 0, 0, 0, {153, 506, 261, 247}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Drifblim; Explosion, Hex, Will-O-Wisp, Shadow Ball */
        {251, 0, 0, 1, 1, {105, 94, 437, 369}, 3, {{"BERRY", 1, 1}, {"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 11, 1}}}, /* Celebi; Recover, Psychic, Leaf Storm, U-turn */
        {424, 0, 0, 0, 0, {252, 458, 369, 282}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"KINGS_ROCK", -1, 1}, {"GRIP_CLAW", -1, 1}}}, /* Ambipom; Fake Out, Double Hit, U-turn, Knock Off */
        {289, 0, 0, 0, 0, {38, 89, 389, 303}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}}}, /* Slaking; Double-Edge, Earthquake, Sucker Punch, Slack Off */
    }},
    {"pucci", "Enrico Pucci", "Do you believe in 'gravity'?", 29 /* Temple */, "speedBoost", 6, {
        {687, 0, 0, 0, 0, {276, 282, 427, 576}, 2, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"LEFTOVERS", -1, 1}}}, /* Malamar; Superpower, Knock Off, Psycho Cut, Topsy-Turvy */
        {826, 1, 0, 1, 0, {356, 94, 405, 105}, 2, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 6, 1}}}, /* Orbeetle (gigantamax); Gravity, Psychic, Bug Buzz, Recover */
        {894, 0, 0, 0, 0, {486, 521, 229, 182}, 2, {{"ATTACK_TYPE_BOOSTER", 12, 1}, {"MULTI_LENS", -1, 1}}}, /* Regieleki; Electro Ball, Volt Switch, Rapid Spin, Protect */
        {625, 0, 0, 1, 0, {442, 389, 14, 282}, 2, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Bisharp; Iron Head, Sucker Punch, Swords Dance, Knock Off */
        {292, 0, 0, 0, 0, {809, 404, 261, 425}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"BERRY", 1, 1}}}, /* Shedinja; Poltergeist, X-Scissor, Will-O-Wisp, Shadow Sneak */
        {493, 17, 0, 1, 0, {449, 105, 347, 414}, 1, {{"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Arceus (fairy); Judgment, Recover, Calm Mind, Earth Power */
    }},
    {"jolyne", "Jolyne Cujoh", "Gimme a break...", 7 /* Swamp */, NULL, 6, {
        {918, 0, 0, 1, 0, {852, 564, 369, 884}, 3, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"FOCUS_BAND", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Spidops; Silk Trap, Sticky Web, U-turn, Pounce */
        {465, 0, 0, 0, 0, {438, 282, 79, 73}, 2, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"LEFTOVERS", -1, 1}}}, /* Tangrowth; Power Whip, Knock Off, Sleep Powder, Leech Seed */
        {675, 0, 0, 0, 0, {370, 282, 418, 575}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Pangoro; Close Combat, Knock Off, Bullet Punch, Parting Shot */
        {707, 0, 0, 1, 0, {115, 113, 86, 583}, 2, {{"LEFTOVERS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 17, 1}}}, /* Klefki; Reflect, Light Screen, Thunder Wave, Play Rough */
        {186, 0, 0, 0, 0, {503, 92, 182, 156}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"LEFTOVERS", -1, 1}, {"TOXIC_ORB", -1, 1}}}, /* Politoed; Scald, Toxic, Protect, Rest */
        {267, 0, 0, 0, 0, {483, 405, 314, 355}, 2, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}}}, /* Beautifly; Quiver Dance, Bug Buzz, Air Cutter, Roost */
    }},
    {"joseph", "Joseph Joestar", "Your next line is...", 5 /* Forest */, NULL, 6, {
        {768, 0, 0, 0, 0, {660, 710, 141, 191}, 2, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Golisopod; First Impression, Liquidation, Leech Life, Spikes */
        {202, 0, 0, 0, 0, {68, 243, 227, 194}, 3, {{"LEFTOVERS", -1, 1}, {"FOCUS_BAND", -1, 1}, {"BERRY", 1, 1}}}, /* Wobbuffet; Counter, Mirror Coat, Encore, Destiny Bond */
        {185, 0, 0, 0, 0, {452, 444, 389, 446}, 3, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"LEFTOVERS", -1, 1}, {"QUICK_CLAW", -1, 1}}}, /* Sudowoodo; Wood Hammer, Stone Edge, Sucker Punch, Stealth Rock */
        {781, 0, 0, 0, 0, {438, 809, 677, 282}, 3, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}, {"GRIP_CLAW", -1, 1}}}, /* Dhelmise; Power Whip, Poltergeist, Anchor Shot, Knock Off */
        {466, 0, 0, 0, 0, {9, 8, 238, 521}, 1, {{"ATTACK_TYPE_BOOSTER", 12, 1}}}, /* Electivire; Thunder Punch, Ice Punch, Cross Chop, Volt Switch */
        {468, 0, 1, 0, 0, {403, 605, 355, 86}, 3, {{"KINGS_ROCK", -1, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"MULTI_LENS", -1, 1}}}, /* Togekiss; Air Slash, Dazzling Gleam, Roost, Thunder Wave */
    }},
    {"jotaro", "Jotaro Kujo", "Good grief.", 10 /* Seabed */, NULL, 6, {
        {68, 0, 0, 1, 0, {370, 418, 282, 8}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"FOCUS_BAND", -1, 1}}}, /* Machamp; Close Combat, Bullet Punch, Knock Off, Ice Punch */
        {964, 0, 0, 0, 0, {857, 834, 370, 812}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}, {"SHELL_BELL", -1, 1}}}, /* Palafin (zero); Jet Punch, Wave Crash, Close Combat, Flip Turn */
        {376, 1, 0, 0, 0, {309, 428, 418, 89}, 2, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"QUICK_CLAW", -1, 1}}}, /* Metagross (mega); Meteor Mash, Zen Headbutt, Bullet Punch, Earthquake */
        {892, 1, 0, 0, 0, {818, 370, 453, 369}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"MULTI_LENS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Urshifu (rapid-strike); Surging Strikes, Close Combat, Aqua Jet, U-turn */
        {212, 1, 0, 0, 0, {418, 14, 369, 370}, 1, {{"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Scizor (mega); Bullet Punch, Swords Dance, U-turn, Close Combat */
        {395, 0, 0, 0, 0, {56, 430, 58, 355}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}, {"LEFTOVERS", -1, 1}}}, /* Empoleon; Hydro Pump, Flash Cannon, Ice Beam, Roost */
    }},
    {"dio", "DIO", "You thought your first kiss would be JoJo, but it was I, DIO!", 19 /* Graveyard */, NULL, 6, {
        {1000, 0, 0, 0, 0, {874, 247, 417, 105}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Gholdengo; Make It Rain, Shadow Ball, Nasty Plot, Recover */
        {534, 0, 0, 0, 0, {409, 183, 282, 263}, 2, {{"FLAME_ORB", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Conkeldurr; Drain Punch, Mach Punch, Knock Off, Facade */
        {169, 0, 0, 1, 1, {413, 440, 355, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Crobat; Brave Bird, Cross Poison, Roost, U-turn */
        {983, 0, 0, 1, 0, {869, 389, 442, 14}, 3, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}, {"LEFTOVERS", -1, 1}}}, /* Kingambit; Kowtow Cleave, Sucker Punch, Iron Head, Swords Dance */
        {478, 1, 0, 0, 0, {58, 247, 191, 194}, 2, {{"ATTACK_TYPE_BOOSTER", 14, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Froslass (mega); Ice Beam, Shadow Ball, Spikes, Destiny Bond */
        {491, 0, 0, 0, 0, {464, 399, 417, 188}, 3, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"WIDE_LENS", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Darkrai; Dark Void, Dark Pulse, Nasty Plot, Sludge Bomb */
    }},
    {"koichi", "Koichi Hirose", "OK, master! Let's kill da ho! Beeetch!", 1 /* Plains */, NULL, 6, {
        {441, 0, 0, 0, 0, {448, 586, 417, 355}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"MULTI_LENS", -1, 1}}}, /* Chatot; Chatter, Boomburst, Nasty Plot, Roost */
        {235, 0, 0, 0, 0, {147, 564, 446, 575}, 2, {{"BERRY", 1, 1}, {"LEFTOVERS", -1, 1}}}, /* Smeargle; Spore, Sticky Web, Stealth Rock, Parting Shot */
        {841, 0, 0, 0, 0, {788, 200, 389, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 15, 1}}}, /* Flapple; Grav Apple, Outrage, Sucker Punch, U-turn */
        {920, 0, 0, 0, 0, {660, 369, 282, 389}, 2, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Lokix; First Impression, U-turn, Knock Off, Sucker Punch */
        {447, 0, 0, 0, 0, {410, 418, 383, 179}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"EVIOLITE", -1, 1}}}, /* Riolu; Vacuum Wave, Bullet Punch, Copycat, Reversal */
        {59, 0, 0, 0, 0, {394, 245, 234, 261}, 3, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}}}, /* Arcanine; Flare Blitz, Extreme Speed, Morning Sun, Will-O-Wisp */
    }},
    {"mista", "Guido Mista", "Pass, pass, pass!", 30 /* Slum */, NULL, 6, {
        {870, 0, 0, 1, 0, {748, 370, 224, 398}, 2, {{"ATTACK_TYPE_BOOSTER", 1, 1}, {"SHELL_BELL", -1, 1}}}, /* Falinks; No Retreat, Close Combat, Megahorn, Poison Jab */
        {818, 0, 0, 0, 0, {745, 58, 369, 403}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"SCOPE_LENS", -1, 1}}}, /* Inteleon; Snipe Shot, Ice Beam, U-turn, Air Slash */
        {224, 0, 0, 0, 0, {190, 331, 126, 58}, 2, {{"MULTI_LENS", -1, 1}, {"ATTACK_TYPE_BOOSTER", 10, 1}}}, /* Octillery; Octazooka, Bullet Seed, Fire Blast, Ice Beam */
        {693, 0, 0, 0, 0, {352, 396, 399, 406}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"MULTI_LENS", -1, 1}}}, /* Clawitzer; Water Pulse, Aura Sphere, Dark Pulse, Dragon Pulse */
        {9, 0, 0, 0, 0, {56, 396, 58, 504}, 3, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"WHITE_HERB", -1, 1}, {"SHELL_BELL", -1, 1}}}, /* Blastoise; Hydro Pump, Aura Sphere, Ice Beam, Shell Smash */
        {359, 2, 0, 0, 0, {389, 282, 583, 14}, 1, {{"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Absol (mega-z); Sucker Punch, Knock Off, Play Rough, Swords Dance */
    }},
    {"wamuu", "Wamuu", "Awaken, my masters!", -1, NULL, 6, {
        {641, 0, 0, 0, 0, {846, 542, 366, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"MULTI_LENS", -1, 1}}}, /* Tornadus (incarnate); Bleakwind Storm, Hurricane, Tailwind, U-turn */
        {530, 0, 0, 0, 0, {89, 442, 14, 229}, 3, {{"ATTACK_TYPE_BOOSTER", 4, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}, {"LEFTOVERS", -1, 1}}}, /* Excadrill; Earthquake, Iron Head, Swords Dance, Rapid Spin */
        {450, 0, 0, 0, 0, {89, 303, 446, 18}, 2, {{"ATTACK_TYPE_BOOSTER", 4, 1}, {"LEFTOVERS", -1, 1}}}, /* Hippowdon; Earthquake, Slack Off, Stealth Rock, Whirlwind */
        {589, 0, 0, 0, 0, {224, 442, 370, 14}, 2, {{"ATTACK_TYPE_BOOSTER", 6, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Escavalier; Megahorn, Iron Head, Close Combat, Swords Dance */
        {865, 0, 0, 1, 0, {794, 660, 282, 413}, 2, {{"LEEK", -1, 1}, {"ATTACK_TYPE_BOOSTER", 1, 1}}}, /* Sirfetch'd; Meteor Assault, First Impression, Knock Off, Brave Bird */
        {142, 0, 0, 0, 0, {444, 814, 89, 355}, 3, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"ATTACK_TYPE_BOOSTER", 2, 1}, {"KINGS_ROCK", -1, 1}}}, /* Aerodactyl; Stone Edge, Dual Wingbeat, Earthquake, Roost */
    }},
    {"esidisi", "Esidisi", "HEEEEYYYY! THEY KILLED LOGGINS!", -1, NULL, 6, {
        {721, 0, 0, 0, 0, {592, 53, 188, 414}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 9, 1}}}, /* Volcanion; Steam Eruption, Flamethrower, Sludge Bomb, Earth Power */
        {555, 0, 2, 0, 0, {394, 89, 369, 157}, 2, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"SHELL_BELL", -1, 1}}}, /* Darmanitan; Flare Blitz, Earthquake, U-turn, Rock Slide */
        {851, 0, 0, 0, 0, {680, 141, 489, 282}, 3, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"ATTACK_TYPE_BOOSTER", 6, 1}, {"SHELL_BELL", -1, 1}}}, /* Centiskorch; Fire Lash, Leech Life, Coil, Knock Off */
        {73, 0, 1, 1, 0, {503, 188, 229, 390}, 2, {{"ATTACK_TYPE_BOOSTER", 10, 1}, {"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Tentacruel; Scald, Sludge Bomb, Rapid Spin, Toxic Spikes */
        {793, 0, 0, 0, 0, {408, 482, 447, 446}, 2, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"ATTACK_TYPE_BOOSTER", 3, 1}}}, /* Nihilego; Power Gem, Sludge Wave, Grass Knot, Stealth Rock */
        {467, 0, 0, 0, 0, {126, 85, 411, 269}, 2, {{"ATTACK_TYPE_BOOSTER", 9, 1}, {"MULTI_LENS", -1, 1}}}, /* Magmortar; Fire Blast, Thunderbolt, Focus Blast, Taunt */
    }},
    {"valentine", "Funny Valentine", "My heart and actions are utterly unclouded... They are all those of 'Justice'.", 24 /* Abyss */, NULL, 6, {
        {720, 0, 0, 0, 0, {593, 247, 411, 417}, 2, {{"ATTACK_TYPE_BOOSTER", 13, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}}}, /* Hoopa; Hyperspace Hole, Shadow Ball, Focus Blast, Nasty Plot */
        {628, 0, 0, 0, 0, {413, 370, 369, 355}, 2, {{"ATTACK_TYPE_BOOSTER", 2, 1}, {"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Braviary; Brave Bird, Close Combat, U-turn, Roost */
        {778, 0, 0, 0, 0, {583, 425, 14, 452}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"ATTACK_TYPE_BOOSTER", 17, 1}}}, /* Mimikyu (disguised); Play Rough, Shadow Sneak, Swords Dance, Wood Hammer */
        {925, 0, 0, 0, 0, {860, 44, 331, 882}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"WIDE_LENS", -1, 1}, {"KINGS_ROCK", -1, 1}}}, /* Maushold (four); Population Bomb, Bite, Bullet Seed, Tidy Up */
        {571, 0, 0, 1, 1, {399, 53, 188, 417}, 2, {{"ATTACK_TYPE_BOOSTER", 16, 1}, {"MULTI_LENS", -1, 1}}}, /* Zoroark; Dark Pulse, Flamethrower, Sludge Bomb, Nasty Plot */
        {719, 1, 0, 1, 0, {591, 585, 414, 446}, 2, {{"ATTACK_TYPE_BOOSTER", 5, 1}, {"ATTACK_TYPE_BOOSTER", 17, 1}}}, /* Diancie (mega); Diamond Storm, Moonblast, Earth Power, Stealth Rock */
    }},
    {"polnareff", "Jean Pierre Polnareff", "I'm gonna turn you into a pincushion!", 20 /* Dojo */, NULL, 5, {
        {589, 0, 0, 0, 0, {442, 224, 529, 14}, 2, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"ATTACK_TYPE_BOOSTER", 6, 1}}}, /* Escavalier; Iron Head, Megahorn, Drill Run, Swords Dance */
        {681, 0, 0, 1, 0, {442, 425, 14, 588}, 3, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"ATTACK_TYPE_BOOSTER", 7, 1}, {"LEFTOVERS", -1, 1}}}, /* Aegislash (shield); Iron Head, Shadow Sneak, Swords Dance, King’s Shield */
        {798, 0, 0, 0, 0, {348, 533, 684, 282}, 2, {{"ATTACK_TYPE_BOOSTER", 11, 1}, {"ATTACK_TYPE_BOOSTER", 8, 1}}}, /* Kartana; Leaf Blade, Sacred Sword, Smart Strike, Knock Off */
        {676, 0, 0, 0, 0, {216, 538, 389, 369}, 2, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}}}, /* Furfrou; return.name, Cotton Guard, Sucker Punch, U-turn */
        {1024, 0, 0, 0, 0, {906, 414, 347, 229}, 3, {{"ATTACK_TYPE_BOOSTER", 0, 1}, {"LEFTOVERS", -1, 1}, {"SHELL_BELL", -1, 1}}}, /* Terapagos; Tera Starstorm, Earth Power, Calm Mind, Rapid Spin */
    }},
    {"risotto", "Risotto Nero", "Metallica!", 13 /* Cave */, NULL, 5, {
        {808, 0, 0, 0, 0, {430, 86, 151, 92}, 3, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"EVIOLITE", -1, 1}, {"LEFTOVERS", -1, 1}}}, /* Meltan; Flash Cannon, Thunder Wave, Acid Armor, Toxic */
        {598, 0, 0, 0, 0, {360, 438, 73, 191}, 2, {{"ATTACK_TYPE_BOOSTER", 8, 1}, {"ATTACK_TYPE_BOOSTER", 11, 1}}}, /* Ferrothorn; Gyro Ball, Power Whip, Leech Seed, Spikes */
        {352, 0, 0, 0, 0, {425, 282, 409, 252}, 2, {{"ATTACK_TYPE_BOOSTER", 7, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}}}, /* Kecleon; Shadow Sneak, Knock Off, Drain Punch, Fake Out */
        {461, 0, 0, 0, 0, {813, 282, 420, 14}, 3, {{"ATTACK_TYPE_BOOSTER", 14, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"MULTI_LENS", -1, 1}}}, /* Weavile; Triple Axel, Knock Off, Ice Shard, Swords Dance */
        {862, 0, 2, 1, 1, {792, 282, 263, 370}, 3, {{"FLAME_ORB", -1, 1}, {"ATTACK_TYPE_BOOSTER", 16, 1}, {"ATTACK_TYPE_BOOSTER", 0, 1}}}, /* Obstagoon; Obstruct, Knock Off, Facade, Close Combat */
    }},
    {"stroheim", "Rudolf von Stroheim", "You fool! German science is the finest in the world!", 41 /* Laboratory */, NULL, 5, {
        {649, 2, 0, 0, 0, {546, 442, 369, 153}, 4, {{"MULTI_LENS", -1, 2}, {"GRIP_CLAW", -1, 4}, {"REVIVER_SEED", -1, 1}, {"SOUL_DEW", -1, 3}}}, /* Genesect (burn); Techno Blast, Iron Head, U-turn, Explosion */
        {992, 0, 0, 0, 0, {409, 9, 370, 252}, 3, {{"LEFTOVERS", -1, 4}, {"BASE_STAT_BOOSTER", 1, 10}, {"BASE_STAT_BOOSTER", 2, 10}}}, /* Iron Hands; Drain Punch, Thunder Punch, Close Combat, Fake Out */
        {295, 0, 2, 0, 0, {586, 315, 304, 59}, 1, {{"ATTACK_TYPE_BOOSTER", 0, 5}}}, /* Exploud; Boomburst, Overheat, Hyper Voice, Blizzard */
        {966, 0, 2, 0, 0, {859, 441, 508, 442}, 4, {{"QUICK_CLAW", -1, 3}, {"ATTACK_TYPE_BOOSTER", 8, 4}, {"SHELL_BELL", -1, 3}, {"FOCUS_BAND", -1, 2}}}, /* Revavroom; Spin Out, Gunk Shot, Shift Gear, Iron Head */
        {376, 1, 0, 0, 0, {309, 418, 428, 153}, 1, {{"ATTACK_TYPE_BOOSTER", 8, 5}}}, /* Metagross (mega); Meteor Mash, Bullet Punch, Zen Headbutt, Explosion */
    }},
};

const unsigned cc_jojo_count = sizeof cc_jojo_chars / sizeof *cc_jojo_chars;
