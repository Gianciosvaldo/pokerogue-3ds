/* Test transport only. Source SwitchSummon->Summon->PartyMemberPokemonPhase
 * addresses party/field members, not PokemonPhase.battlerIndex. C engine.c
 * phase_pokemon explicitly routes PH_SWITCH_SUMMON through sw_player/field/party.
 * MoveHeader/Tera use their Pokemon reference, Dancer its params.pokemon.
 * None declares/inherits battlerIndex. Dynamic membership alone is not presence.
 * No expected source token participates in this independent classification. */
static bool optional_phase_absent(const pr_phase *p){
 switch(p->type){case PH_SWITCH_SUMMON:case PH_MOVE_HEADER:case PH_DANCER:case PH_TERA:return true;default:return false;}
}
