# Keldeo learned forms and Mimicry

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Node 22 producers capture
native `BattleScene.prototype.randBattleSeedInt` before GameManager construction
and restore it after natural doubles startup. Gameplay RNG is not mocked.

## Source behavior

The only enabled MoveLearned form registrations are Keldeo's two transitions.
They inspect the original moveset for Secret Sword and exclude Daily mode.
The player receives a nonquiet FormChangePhase; the enemy receives the quiet
phase. The target and Pokemon reference are cached when queued. The nonquiet
body changes form without the quiet ability wrapper, emits the native form
message, removes Autotomized and queues EndEvolution unless modal.
The game-core LearnMove callback queues the cached form behind pending learning
phases and retains the existing profile seen/caught updates.

Mimicry's overridden canApply only checks Terastallization. The generic ability
dispatcher still owns HP, suppression and passive eligibility. Its apply mutates
types even when simulated: four terrains select their corresponding type;
otherwise it reads raw types while bypassing summon data and the third type.
A truthy added type matching the result is cleared. This also exposed two
shared raw-identity query bugs: bypassed type queries must bypass Transform's
copied form, and getWeight always reads original species/form. Transform itself
continues copying the target's current species/form.

An explicit challenge-context bit distinguishes known empty challenge rules
from missing rules. Unknown rules and unsupported Passives/Flip Stat rules
remain flagged. A missing game mode also refuses a potentially eligible
learned form instead of silently treating it as Classic.

## Bounded evidence

Seeds 777 and 2034:

- 33,792 raw Keldeo cases per seed compare full 125-field snapshots of eight
  real actors, queue levels/order, cached ability/name metadata, translated text,
  Autotomized counts, weight and both RNG streams. The original 32,768 cases
  retain their prefix; 1,024 additional direct registered constructors cover
  cached/same targets and modal/nonmodal execution. Each seed executes 256
  modal bodies. Actual Evolution/FormChange start, clone, cycle, changeForm
  and end methods run; only asset/animation/audio/UI presentation is replaced.
- 16,384 actual Mimicry dispatcher cases per seed cover active/passive abilities,
  suppression, fainting, simulation, terrain fallback, Transform, Tera, bench
  actors, added types and levels through 1,000,000.
- 32,768 actual LearnMove caller cases per seed compare cached form identity,
  relative queue order, all actor frames, messages, tags, profile seen/caught
  bits and both RNG streams against actual game-core phase execution. Half
  retain another pending LearnMove ahead of the form. This is an explicit
  caller projection; raw phase proofs separately compare Evolution queues.
- The host fixture passes 53 checks through actual learn/form callers, caught
  profile records, cached targets, delayed ordering, legal saves and missing
  challenge/mode context. Compiled faults catch 21 production regressions;
  malformed transport checks reject 19 altered captures.

Both complete Mimicry terrain-turn matrices match all 8,192 turns per seed,
including 4,096 retained wide-combat controls each. There are no exclusions or
source exceptions. Each seed executes 4,620 native Mimicry callbacks; the wide
controls retain damage above Int32, ally/spread targets, multiple faints and
switches. The comparison includes exact state, queue, translated text and RNG.
Normal app/custom/save, game-core and 3DS integration gates remain required
on the final PR head.

## Reproduction and limits

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/keldeo-mimicry-diff.sh /path/to/pinned-upstream 777
sh battle-core/tools/keldeo-mimicry-diff.sh /path/to/pinned-upstream 2034
```

Initial failures and captures remain saved in the worktree: blanket Challenge
refusal, copied weight, copied fallback types, incomplete ability queue metadata
and invalid fixture setup. Full original matrix sizes were rerun after fixes;
expected source output was not normalized and required cases were not dropped.

The LearnMove proof covers default learning into an empty slot, not TM/Memory
purchase or replacement UI. Modal constructor execution does not implement or
prove the scene's external modal override protocol. Profile bits do not prove
the entire profile catalog. These bounded proofs do not close the source
inventory, remaining doubles callbacks or wide-level Endless acceptance; see
[the completion checklist](COMPLETION-CHECKLIST.md).
