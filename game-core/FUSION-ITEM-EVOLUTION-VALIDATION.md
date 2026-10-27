# Evolution items on fused party members

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`, with pinned English locales and Node 22.

`EvolutionItemModifier.apply` tries primary evolutions first, then validates secondary candidates using their form and gender. A secondary selection queues `FusionSpeciesFormEvolution` with `lastLevel = level - 1`. Explicit item application does not obey the pause flag used when generating offers.

Reward-core already emitted the selected half. Game-core returned its counted fallback for every fusion before reading that result. The caller now checks primary-first item targets and passes the selected half through the existing evolution phase. Its existing confirmation handler mutates that half and updates stats, HP, ability and fused name. Football's separate primary-only lookup keeps its existing contract. No state layout, save version or generated tables change.

An older save can lack secondary gender. If an earlier matching secondary candidate requires it, selection reports the existing unsupported form feature and refuses the item before mutation. It does not assume the primary gender or read an untrusted secondary value.

The producer observes actual `EvolutionItemModifierType.selectFilter`, `EvolutionItemModifier.apply`, its queued evolution arguments, and `PlayerPokemon.evolve`. Native battle RNG is captured before `GameManager` and restored after startup; method identities are checked. Asset loading, status display and icon refresh are presentation boundaries. The phase-manager interception records the native queued target; it does not choose one.

There are 2,016 prepared compatible fusion snapshots: twelve species pairs, seven ordinary/rare items, two genders, levels 1/50/70,000, two ability slots, and both pause states. They contain 144 primary selections, 288 secondary selections and 1,584 refusals. The raw JSON and C fixture reproduce byte-for-byte. These are prepared actor snapshots, not DNA Splicers acquisition observations.

The host compares direct queries, reward lifecycle application and actual `gc_run_choose` reward selection for every input. Valid choices reach the real evolution screen; confirmation uses the real phase handler. All ordered moves, PP metadata, species/forms, gender, nature/IVs, stats/HP, effective ability, pause state, rendered fused name and global/battle RNG match native frames. The comparison stops before executing subsequently queued learn-move phases, matching the native direct `evolve` boundary. It does not claim complete EvolutionScene animation, dex/candy side effects, every form/condition, or a whole-run queue comparison.

```sh
make -C game-core fusion-item-evolution-host-test
python3 game-core/tools/check-fusion-item-evolution-proof.py
FUSION_ITEM_EVOLUTION_SANITIZE=1 python3 game-core/tools/check-fusion-item-evolution-proof.py
```

Fourteen compiled faults, twelve malformed captures and five forged native observations must fail. Scoped ASan/UBSan/LSan covers mon evolution, reward lifecycle and phase execution; supporting libraries are unsanitized. The retained level-up and save proof keeps all 1,440 native cases and fifteen compiled faults; two text anchors now uniquely target its existing level-up function after the item function was added. No assertion or old case was removed. Existing Gimmighoul acquisition/cleanup and unfused reward lifecycle proofs remain enabled.

General fusion acquisition, special extra-party evolutions, broader conditional interactions and full battle-core completion remain separate. Normal core/app/custom/save/football tests and ARM build remain required; only dedicated custom Task63 checks are skipped. No new Windows builder is released.
