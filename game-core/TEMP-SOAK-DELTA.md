# Temporary caller: all changed normal-soak output rows

Before main `6b65575`; command `game-core/build/gcplay --auto 1000 --jobs 8`.
Saved path has the same outcomes, turns, digest and counters; round trips
14,291→14,679, largest save11,400→11,472 bytes. See [scope](TEMP-VALIDATION.md).

```diff
--- before 6b65575
+++ temporary caller
@@ -2,3 +2,3 @@
-1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 49206 turns (49.2 per run)
-final wave:  0-9:125 10-19:708 20-29:156 30-39:10 40-49:1
-runs' digest: 15d6d01d
+1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 50394 turns (50.4 per run)
+final wave:  0-9:111 10-19:698 20-29:171 30-39:18 40-49:2
+runs' digest: 9e0f75df
@@ -7,2 +7,2 @@
-  enemy moveset (last 4 level moves)                   1000 runs     14701
-  double battle played as single                        762 runs      1544
+  enemy moveset (last 4 level moves)                   1000 runs     15094
+  double battle played as single                        769 runs      1588
@@ -10,5 +10,5 @@
-  biome weather roll                                   1000 runs      1851
-  enemy run-core can't generate (stand-in)             1000 runs      2258
-  post-battle abilities, Pay Day, lapsing items           3 runs         4
-  trainer's name without its title in a message         307 runs       402
-  reward taken without its own effect                   139 runs       169
+  biome weather roll                                   1000 runs      1902
+  enemy run-core can't generate (stand-in)             1000 runs      2301
+  post-battle abilities, Pay Day, lapsing items         167 runs      1380
+  trainer's name without its title in a message         314 runs       424
+  reward taken without its own effect                   145 runs       178
@@ -17,17 +17,17 @@
-  POKEBALL                                              850 runs      2021 taken         0 refused
-  GREAT_BALL                                            247 runs       285 taken         0 refused
-  ULTRA_BALL                                             64 runs        66 taken         0 refused
-  ROGUE_BALL                                             31 runs        31 taken         0 refused
-  MASTER_BALL                                             1 runs         1 taken         0 refused
-  RARE_CANDY                                            756 runs      1478 taken         0 refused
-  RARER_CANDY                                             9 runs         9 taken         0 refused
-  EVOLUTION_ITEM                                         17 runs        17 taken         0 refused
-  MEGA_BRACELET                                          17 runs        17 taken         0 refused
-  DYNAMAX_BAND                                           10 runs        10 taken         0 refused
-  MAP                                                   115 runs       115 taken         0 refused
-  POTION                                                981 runs      4769 taken       309 refused
-  SUPER_POTION                                           50 runs        59 taken         2 refused
-  REVIVE                                                482 runs       714 taken       155 refused
-  MAX_REVIVE                                            209 runs       235 taken        22 refused
-  FULL_HEAL                                               7 runs         7 taken        36 refused
-  SACRED_ASH                                              3 runs         3 taken         0 refused
+  POKEBALL                                              851 runs      2068 taken         0 refused
+  GREAT_BALL                                            260 runs       300 taken         0 refused
+  ULTRA_BALL                                             67 runs        69 taken         0 refused
+  ROGUE_BALL                                             28 runs        28 taken         0 refused
+  MASTER_BALL                                             2 runs         2 taken         0 refused
+  RARE_CANDY                                            760 runs      1497 taken         0 refused
+  RARER_CANDY                                            11 runs        11 taken         0 refused
+  EVOLUTION_ITEM                                         20 runs        20 taken         0 refused
+  MEGA_BRACELET                                          16 runs        16 taken         0 refused
+  DYNAMAX_BAND                                           11 runs        11 taken         0 refused
+  MAP                                                   117 runs       117 taken         0 refused
+  POTION                                                987 runs      4899 taken       311 refused
+  SUPER_POTION                                           55 runs        73 taken         5 refused
+  REVIVE                                                495 runs       734 taken       164 refused
+  MAX_REVIVE                                            217 runs       245 taken        16 refused
+  FULL_HEAL                                               9 runs         9 taken        38 refused
+  SACRED_ASH                                              4 runs         4 taken         0 refused
@@ -35,2 +35,2 @@
-  ETHER                                                  22 runs        22 taken        59 refused
-  MAX_ETHER                                               1 runs         1 taken         1 refused
+  ETHER                                                  21 runs        21 taken        61 refused
+  MAX_ETHER                                               0 runs         0 taken         2 refused
@@ -38,3 +38,3 @@
-  PP_UP                                                 110 runs       117 taken        16 refused
-  PP_MAX                                                 17 runs        18 taken         1 refused
-  LURE                                                  515 runs       691 taken         0 refused
+  PP_UP                                                 115 runs       122 taken        18 refused
+  PP_MAX                                                 16 runs        17 taken         0 refused
+  LURE                                                  528 runs       718 taken         0 refused
@@ -44,3 +44,3 @@
-  TEMP_STAT_STAGE_BOOSTER                               779 runs      1461 taken         0 refused
-  DIRE_HIT                                              173 runs       192 taken         0 refused
-  BASE_STAT_BOOSTER                                     157 runs       177 taken         2 refused
+  TEMP_STAT_STAGE_BOOSTER                               779 runs      1544 taken         0 refused
+  DIRE_HIT                                              175 runs       194 taken         0 refused
+  BASE_STAT_BOOSTER                                     160 runs       176 taken         2 refused
@@ -48,11 +48,11 @@
-  MINT                                                   15 runs        15 taken         1 refused
-  MYSTICAL_ROCK                                           4 runs         4 taken         0 refused
-  BERRY                                                 556 runs       790 taken         0 refused
-  TM_COMMON                                             490 runs      4005 taken         0 refused
-  TM_GREAT                                              149 runs       867 taken         0 refused
-  TM_ULTRA                                               54 runs       336 taken         0 refused
-  MEMORY_MUSHROOM                                        11 runs        40 taken        53 refused
-  EXP_SHARE                                              47 runs        48 taken         0 refused
-  EXP_CHARM                                              29 runs        29 taken         0 refused
-  SUPER_EXP_CHARM                                        12 runs        12 taken         0 refused
-  SOOTHE_BELL                                           107 runs       115 taken         0 refused
+  MINT                                                   16 runs        16 taken         1 refused
+  MYSTICAL_ROCK                                           3 runs         3 taken         0 refused
+  BERRY                                                 556 runs       798 taken         0 refused
+  TM_COMMON                                             486 runs      4053 taken         0 refused
+  TM_GREAT                                              151 runs       930 taken         0 refused
+  TM_ULTRA                                               53 runs       330 taken         0 refused
+  MEMORY_MUSHROOM                                        17 runs        50 taken        60 refused
+  EXP_SHARE                                              48 runs        49 taken         0 refused
+  EXP_CHARM                                              34 runs        34 taken         0 refused
+  SUPER_EXP_CHARM                                        14 runs        14 taken         0 refused
+  SOOTHE_BELL                                           110 runs       118 taken         0 refused
@@ -60,5 +60,5 @@
-  SOUL_DEW                                                8 runs         9 taken         0 refused
-  NUGGET                                                267 runs       294 taken         0 refused
-  BIG_NUGGET                                             64 runs        66 taken         0 refused
-  RELIC_GOLD                                              2 runs         2 taken         0 refused
-  AMULET_COIN                                            11 runs        11 taken         0 refused
+  SOUL_DEW                                                7 runs         7 taken         0 refused
+  NUGGET                                                276 runs       304 taken         0 refused
+  BIG_NUGGET                                             67 runs        70 taken         0 refused
+  RELIC_GOLD                                              3 runs         3 taken         0 refused
+  AMULET_COIN                                            13 runs        13 taken         0 refused
@@ -67,2 +67,2 @@
-  WIDE_LENS                                              25 runs        26 taken         0 refused
-  MULTI_LENS                                              3 runs         3 taken         0 refused
+  WIDE_LENS                                              27 runs        28 taken         0 refused
+  MULTI_LENS                                              2 runs         2 taken         0 refused
@@ -71,4 +71,4 @@
-  BERRY_POUCH                                             4 runs         4 taken         0 refused
-  FOCUS_BAND                                              3 runs         3 taken         0 refused
-  QUICK_CLAW                                             14 runs        14 taken         0 refused
-  KINGS_ROCK                                              4 runs         4 taken         0 refused
+  BERRY_POUCH                                             3 runs         3 taken         0 refused
+  FOCUS_BAND                                              2 runs         2 taken         0 refused
+  QUICK_CLAW                                             15 runs        15 taken         0 refused
+  KINGS_ROCK                                              5 runs         5 taken         0 refused
@@ -77,2 +77,2 @@
-  TOXIC_ORB                                              10 runs        10 taken         0 refused
-  FLAME_ORB                                               4 runs         4 taken         0 refused
+  TOXIC_ORB                                              11 runs        11 taken         0 refused
+  FLAME_ORB                                               6 runs         6 taken         0 refused
@@ -80,3 +80,3 @@
-  SHINY_CHARM                                             3 runs         3 taken         0 refused
-  ABILITY_CHARM                                           7 runs         7 taken         0 refused
-  IV_SCANNER                                             19 runs        19 taken         0 refused
+  SHINY_CHARM                                             2 runs         2 taken         0 refused
+  ABILITY_CHARM                                           9 runs         9 taken         0 refused
+  IV_SCANNER                                             21 runs        21 taken         0 refused
@@ -84,2 +84,2 @@
-  VOUCHER                                                71 runs        71 taken         0 refused
-  VOUCHER_PLUS                                            3 runs         3 taken         0 refused
+  VOUCHER                                                73 runs        73 taken         0 refused
+  VOUCHER_PLUS                                            4 runs         4 taken         0 refused
@@ -88,2 +88,2 @@
-  fixed classic battle                                 1000 runs      2002
-  trainer config                                        215 runs       256
+  fixed classic battle                                 1000 runs      2023
+  trainer config                                        231 runs       278
@@ -93,20 +93,21 @@
-battle-core and ai-core unported, by runs that hit it (49 kinds):
-  (held items/modifiers on the turn)              983 runs     52018
-  (boss segments)                                 707 runs       770
-  (phase CheckInterludePhase)                     701 runs       764
-  GrowthStatStageChangeAttr                       108 runs       151
-  MultiHitAttr                                     57 runs       120
-  (move condition)                                 42 runs       162
-  SpeedBoostAbAttr                                 36 runs       184
-  StealHeldItemChanceAttr                          29 runs        34
-  PostVictoryAbAttr                                26 runs        99
-  TransformAttr                                    26 runs       129
-  PostTurnRestoreBerryAbAttr                       14 runs        43
-  FormBlockDamageAbAttr                             9 runs        45
-  HealFromBerryUseAbAttr                            9 runs         9
-  StealEatBerryAttr                                 9 runs        12
-  AI: Illusion                                      9 runs        52
-  AddSecondStrikeAbAttr                             7 runs        32
-  FetchBallAbAttr                                   6 runs         6
-  PostSummonAllyHealAbAttr                          6 runs         9
-  (form change)                                     6 runs        41
+battle-core and ai-core unported, by runs that hit it (48 kinds):
+  (held items/modifiers on the turn)              983 runs     52586
+  (boss segments)                                 721 runs       809
+  (phase CheckInterludePhase)                     716 runs       804
+  GrowthStatStageChangeAttr                       112 runs       177
+  MultiHitAttr                                     57 runs       112
+  (move condition)                                 42 runs       159
+  StealHeldItemChanceAttr                          37 runs        44
+  SpeedBoostAbAttr                                 36 runs       190
+  TransformAttr                                    26 runs       131
+  PostVictoryAbAttr                                25 runs       112
+  PostTurnRestoreBerryAbAttr                       16 runs        49
+  StealEatBerryAttr                                12 runs        16
+  FormBlockDamageAbAttr                             9 runs        44
+  HealFromBerryUseAbAttr                            8 runs         8
+  PostSummonAllyHealAbAttr                          8 runs        11
+  AI: Illusion                                      8 runs        52
+  AddSecondStrikeAbAttr                             7 runs        20
+  (arena tags on the turn)                          7 runs        33
+  (form change)                                     7 runs        45
+  FetchBallAbAttr                                   6 runs         8
@@ -114,2 +115 @@
-  SketchAttr                                        5 runs        10
-  (arena tags on the turn)                          5 runs        28
+  PreLeaveFieldAbAttr                               5 runs        10
@@ -119,2 +119,4 @@
-  AI: move restriction                              4 runs        21
-  PostFaintFormChangeAbAttr                         3 runs         3
+  SketchAttr                                        4 runs         8
+  AI: move restriction                              4 runs         9
+  MoodyAbAttr                                       3 runs        11
+  PartyStatusCureAttr                               3 runs         3
@@ -123 +124,0 @@
-  PreLeaveFieldAbAttr                               3 runs         5
@@ -127,3 +128,2 @@
-  FieldPriorityMoveImmunityAbAttr                   2 runs        14
-  MoodyAbAttr                                       2 runs         2
-  PartyStatusCureAttr                               2 runs         2
+  FieldPriorityMoveImmunityAbAttr                   2 runs        16
+  MoveImmunityStatStageChangeAbAttr                 2 runs         5
@@ -130,0 +131,2 @@
+  PostFaintContactDamageAbAttr                      2 runs         2
+  PostFaintFormChangeAbAttr                         2 runs         2
@@ -131,0 +134,2 @@
+  PostKnockOutAbAttr                                2 runs         5
+  SecretPowerAttr                                   2 runs         3
@@ -134 +137,0 @@
-  MoveImmunityStatStageChangeAbAttr                 1 runs         1
@@ -136,2 +138,0 @@
-  PostFaintContactDamageAbAttr                      1 runs         1
-  PostKnockOutAbAttr                                1 runs         1
@@ -140,2 +140,0 @@
-  RandomMoveAttr                                    1 runs         1
-  SecretPowerAttr                                   1 runs         1
```
