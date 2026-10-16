# Partner cures and healing

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.23.3.
No tracked upstream source, existing producer, comparator or coverage gate changed.
Run-core is unchanged. Dedicated custom-only Task63 checks remain skipped; normal
app/custom/save tests remain required.

## Source behavior

PartyStatusCureAttr checks its custom canApply, which deliberately omits target
HP. For each party member, off-field members and matching user IDs call
resetStatus(false). An active member with the exempt ability queues show then
hide; the passive display flag compares getPassiveAbility even if disabled.
Other active members call resetStatus() with its default revive=true. This
matters for FAINT-status controls. Heal Bell exempts Soundproof, Aromatherapy
Sap Sipper. The user exemption compares IDs, rather than object pointers.

HealOnAllyAttr refuses opponents then delegates to HealAttr; polymorphic canApply
requires the exact getAlly object and the parent HP check. The separate move
condition accepts opponents or applies the parent's full-HP condition. Raw apply
can still queue a full-HP heal, and a fainted user can heal a living ally. The
shared parent rounds half up and queues PokemonHealPhase; Heal Block is handled
when that phase executes. Typed child payload extraction avoids relying on
coincidental generated struct layout.

The existing parent calls MoveHealBoostAbAttr, which was missing. The source
callback checks its boostCondition then multiplies the heal ratio. Mega Launcher
uses PULSE_MOVE and 1.5, with no ability display. Its generated closure already
resolves through the existing source owner/hash registry. Both positive Heal
Pulse and negative Pollen Puff predicates are now observed.

## Native evidence

`tools/partner-cures-diff.sh UPSTREAM` runs the additive producer and C replay.
The producer captures native BattleScene.prototype.randBattleSeedInt before
GameManager construction and restores it after natural doubles startup.

There are 12,800 complete cases: the original 12,288 cases cycle Heal Bell,
Aromatherapy and Pollen Puff; 256 additional Heal Pulse and 256 additional
Aromatherapy controls exercise Mega Launcher and Sap Sipper. All match with
zero unsupported/deferred/overflow cases. No cases were removed after failure.
An initial incomplete capture hit the mock call-history memory limit; clearing
observer history between completed cases preserves mocks and all native calls.
Only the final complete capture is evidence.

The matrix uses twelve Rattata actors, both sides, party lengths 1..6,
singles/doubles, active and reserve calls, duplicate user IDs, switching,
primary/passive abilities, enabled/disabled passives, suppression and Neutralizing
Gas. It supplies HP boundaries and all statuses, including toxic/sleep/freeze
counters, FAINT, Protect, Heal Block, confusion and Nightmare. Some cases swap
party slots after construction to observe cached ResetStatus identity versus
field-resolved healing.

The replay compares the established 125-field actor projection plus status
counters and four tag-presence bits before apply, after apply and after queue
execution. It compares queue levels/dynamic order and constructor fields,
messages, ability-bar visibility, last involved battlers and both RNG streams.
Native queued phases execute; only asset/info/frame-rate/animation/UI boundaries
and phase advancement are mocked. The C queue runs through the production
cleanup controller to its TurnInit sentinel. Source booleans are recorded but
the C effect API is void, so return parity is not claimed.

This is an augmented API matrix, not every legal command arrangement, species,
form, modifier, fusion or phase interaction. The projection does not include
waveData.abilityRevealed; that AI/UI metadata remains an inventory boundary.
Jungle Healing's combined owning move and all other party-cure callers are not
closed by these three owning leaves.

The fault check compiles 17 distinct runtime faults, including ID/pointer
exemption, revive defaults, ability blocking/display, missing ally checks,
full-HP/user-HP gates, floor rounding, missing/incorrect boost and ignored pulse
predicate. All are caught. It also rejects 28 malformed captures, including
incomplete streams, invalid fields, nonfinite values and queue integer overflow.

## Retained and integration checks

All retained covered IDs remain. Three unchanged doubles captures now compare
1,955 / 1,942 / 1,958 turns exactly, up by 23 IDs, with 145 cases still explicitly
unsupported across the 6,000 inputs. Four mixed captures retain their prior
1,974 / 1,969 / 1,991 / 1,995 exact IDs. These counts describe bounded captures,
not completion percentages.

The game-core command/save test uses actual Heal Bell, Aromatherapy and Heal
Pulse commands from augmented status/HP starts, compares saved and unsaved runs,
checks both party cures and increased Mega Launcher healing, and checks all
three RNG states. It is singles command evidence, not a natural doubles save
proof. Normal full game-core and exact-head app/custom/save/3DS checks remain
required before merge. Full game-core passed, including 55 new command/save
checks; the paired 200-run digest remains `5e19bd8a`. Remaining doubles, wide-level progression/combat and the
complete source/interaction inventory remain open.
