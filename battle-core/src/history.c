/* Turn history: summonData.moveHistory, turnData.attacksReceived and the last turn order
 * (src/field/pokemon.ts, src/queues/move-phase-priority-queue.ts). Each list keeps its true
 * tracked length and newest entries; a read that reaches past the kept entries reports itself
 * unported instead of guessing. Last Resort membership is recorded separately. */
#include "pr/battle.h"
#include <string.h>

static void remember_used_move(pr_pokemon *p,const pr_turn_move *tm)
{
    if(tm->move<PR_MOVE_COUNT&&tm->use_mode<MOVE_USE_MODE_INDIRECT)
        p->history_used_moves[tm->move/8]|=(uint8_t)(1u<<(tm->move%8));
}
void pr_rebuild_history_used_moves(pr_pokemon *p)
{
    memset(p->history_used_moves,0,sizeof p->history_used_moves);
    p->history_used_complete=p->n_move_history<=PR_MAX_MOVE_HISTORY;
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    for(int i=0;i<kept;i++)remember_used_move(p,&p->move_history[i]);
}
bool pr_history_used_move(const pr_pokemon *p,int move)
{
    if(move<0||move>=PR_MOVE_COUNT)return false;
    if(p->history_used_moves[move/8]&(1u<<(move%8)))return true;
    /* Raw callers can supply the existing tail without a summary. */
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    for(int i=0;i<kept;i++)if(p->move_history[i].move==move&&p->move_history[i].use_mode<MOVE_USE_MODE_INDIRECT)return true;
    return false;
}

static bool nonvirtual_matches(const pr_turn_move *tm,bool ignore_struggle,bool ignore_follow_up)
{
    return tm->move!=MOVE_ID_NONE && (!ignore_struggle || tm->move!=MOVE_ID_STRUGGLE)
        && (tm->use_mode<MOVE_USE_MODE_INDIRECT || (!ignore_follow_up && tm->use_mode==MOVE_USE_MODE_FOLLOW_UP));
}
static void remember_nonvirtual(pr_pokemon *p,const pr_turn_move *tm,uint32_t serial)
{
    for(int q=0;q<4;q++)if(nonvirtual_matches(tm,q/2,q%2)){
        p->history_last[q]=*tm;
        p->history_last_serial[q]=serial;
        p->history_last_known|=(uint8_t)(1u<<q);
        p->history_last_present|=(uint8_t)(1u<<q);
    }
}
void pr_rebuild_nonvirtual_history(pr_pokemon *p)
{
    memset(p->history_last,0,sizeof p->history_last);
    memset(p->history_last_serial,0,sizeof p->history_last_serial);
    p->history_serial=p->n_move_history;
    p->history_last_present=0;
    p->history_last_known=p->n_move_history<=PR_MAX_MOVE_HISTORY?15:0;
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    for(int i=0;i<kept;i++)remember_nonvirtual(p,&p->move_history[i],p->history_serial-kept+i+1);
}

static void remember_all_history(pr_pokemon *p,const pr_turn_move *tm,uint32_t serial)
{
    if(tm->move!=MOVE_ID_NONE){p->history_all_known|=1;p->history_all_present|=1;}
    if(tm->move==MOVE_ID_LOCK_ON||tm->move==MOVE_ID_MIND_READER){
        p->history_all_known|=2;p->history_all_present|=2;
        p->history_aim=*tm;p->history_aim_serial=serial;
    }
}
void pr_rebuild_all_history(pr_pokemon *p)
{
    p->history_all_known=p->n_move_history<=PR_MAX_MOVE_HISTORY?3:0;
    p->history_all_present=0;p->history_aim_serial=0;
    memset(&p->history_aim,0,sizeof p->history_aim);
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    uint32_t serial=p->history_serial<p->n_move_history?p->n_move_history:p->history_serial;
    for(int i=0;i<kept;i++)remember_all_history(p,&p->move_history[i],serial-kept+i+1);
}
bool pr_history_any_move(pr_battle *bt,const pr_pokemon *p,bool *unknown)
{
    *unknown=false;
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    for(int i=0;i<kept;i++)if(p->move_history[i].move!=MOVE_ID_NONE)return true;
    if(p->n_move_history<=PR_MAX_MOVE_HISTORY)return false;
    if(p->history_serial>=p->n_move_history&&(p->history_all_known&1))return (p->history_all_present&1)!=0;
    pr_note_unported(bt,PR_FEAT_MOVE_HISTORY);*unknown=true;return false;
}
const pr_turn_move *pr_last_aim_move(pr_battle *bt,const pr_pokemon *p,bool *unknown)
{
    *unknown=false;
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    for(int i=kept-1;i>=0;i--)if(p->move_history[i].move==MOVE_ID_LOCK_ON||p->move_history[i].move==MOVE_ID_MIND_READER)return &p->move_history[i];
    if(p->n_move_history<=PR_MAX_MOVE_HISTORY)return NULL;
    if(p->history_serial>=p->n_move_history&&(p->history_all_known&2))return p->history_all_present&2?&p->history_aim:NULL;
    pr_note_unported(bt,PR_FEAT_MOVE_HISTORY);*unknown=true;return NULL;
}

static bool protection_entry(const pr_turn_move *tm)
{
    return tm->result==MOVE_RESULT_SUCCESS && (pr_move_has_attr(tm->move,PR_AC_ProtectAttr)
        || tm->move==MOVE_ID_QUICK_GUARD || tm->move==MOVE_ID_WIDE_GUARD);
}
static uint32_t protect_tail(const pr_pokemon *p,bool *complete)
{
    *complete=p->n_move_history<=PR_MAX_MOVE_HISTORY;
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    uint32_t count=0;
    for(int i=kept-1;i>=0;i--){
        if(!protection_entry(&p->move_history[i])){*complete=true;break;}
        count++;
    }
    return count;
}
void pr_rebuild_protect_history(pr_pokemon *p)
{
    p->history_protect_count=protect_tail(p,&p->history_protect_complete);
}
uint32_t pr_protect_streak(pr_battle *bt,const pr_pokemon *p,bool *unknown)
{
    bool complete=p->history_protect_complete;
    uint32_t count=p->history_protect_count;
    if(p->n_move_history<=PR_MAX_MOVE_HISTORY||!complete||p->history_serial<p->n_move_history)
        count=protect_tail(p,&complete);
    *unknown=!complete;
    if(!complete)pr_note_unported(bt,PR_FEAT_MOVE_HISTORY);
    return count;
}

void pr_push_move_history(pr_pokemon *p, pr_turn_move tm)
{
    if(p->n_move_history<=PR_MAX_MOVE_HISTORY||p->history_serial<p->n_move_history)pr_rebuild_all_history(p);
    else if(p->history_all_known!=3)
        for(int i=0;i<PR_MAX_MOVE_HISTORY;i++)remember_all_history(p,&p->move_history[i],p->history_serial-PR_MAX_MOVE_HISTORY+i+1);
    if(p->n_move_history<=PR_MAX_MOVE_HISTORY||!p->history_protect_complete||p->history_serial<p->n_move_history)
        pr_rebuild_protect_history(p);
    if(protection_entry(&tm)){
        if(p->history_protect_count<UINT32_MAX)p->history_protect_count++;
    }else{p->history_protect_count=0;p->history_protect_complete=true;}
    if(p->history_serial<p->n_move_history)pr_rebuild_nonvirtual_history(p);
    if(p->n_move_history<=PR_MAX_MOVE_HISTORY)pr_rebuild_nonvirtual_history(p);
    else if(p->history_last_known!=15)
        for(int i=0;i<PR_MAX_MOVE_HISTORY;i++)remember_nonvirtual(p,&p->move_history[i],p->history_serial-PR_MAX_MOVE_HISTORY+i+1);
    remember_nonvirtual(p,&tm,++p->history_serial);
    remember_all_history(p,&tm,p->history_serial);
    if(!p->n_move_history||(!p->history_used_complete&&p->n_move_history<=PR_MAX_MOVE_HISTORY))
        pr_rebuild_history_used_moves(p);
    else if(!p->history_used_complete)
        for(int i=0;i<PR_MAX_MOVE_HISTORY;i++)remember_used_move(p,&p->move_history[i]);
    remember_used_move(p,&tm);
    if (p->n_move_history >= PR_MAX_MOVE_HISTORY)
        memmove(p->move_history, p->move_history + 1, (PR_MAX_MOVE_HISTORY - 1) * sizeof *p->move_history);
    int kept = p->n_move_history < PR_MAX_MOVE_HISTORY ? p->n_move_history : PR_MAX_MOVE_HISTORY - 1;
    p->move_history[kept] = tm;
    if (p->n_move_history < UINT16_MAX) p->n_move_history++;
}

const pr_turn_move *pr_last_move(pr_battle *bt, const pr_pokemon *p, int k)
{
    if (k < 0 || k >= p->n_move_history) return NULL;
    int kept = p->n_move_history < PR_MAX_MOVE_HISTORY ? p->n_move_history : PR_MAX_MOVE_HISTORY;
    if (k >= kept) {
        pr_note_unported(bt, PR_FEAT_MOVE_HISTORY);
        return NULL;
    }
    return &p->move_history[kept - 1 - k];
}

/* getLastNonVirtualMove(ignoreStruggle, ignoreFollowUp): moveHistory.findLast(m => m.move !== NONE
 * && (!ignoreStruggle || m.move !== STRUGGLE) && (!isVirtual(m.useMode) || (!ignoreFollowUp &&
 * m.useMode === FOLLOW_UP))). *unknown: the search ran past the kept entries (reported). */
const pr_turn_move *pr_last_non_virtual_move(pr_battle *bt, const pr_pokemon *p, bool ignore_struggle,
                                             bool ignore_follow_up, bool *unknown)
{
    *unknown = false;
    int kept=p->n_move_history<PR_MAX_MOVE_HISTORY?p->n_move_history:PR_MAX_MOVE_HISTORY;
    for (int k = 0; k < kept; k++) {
        const pr_turn_move *tm = pr_last_move(bt, p, k);
        if (!tm) { *unknown = true; return NULL; }
        if (nonvirtual_matches(tm,ignore_struggle,ignore_follow_up)) return tm;
    }
    if(p->n_move_history<=PR_MAX_MOVE_HISTORY)return NULL;
    int q=2*ignore_struggle+ignore_follow_up;
    if(p->history_serial>=p->n_move_history && (p->history_last_known&(1u<<q)))
        return p->history_last_present&(1u<<q)?&p->history_last[q]:NULL;
    pr_note_unported(bt,PR_FEAT_MOVE_HISTORY);
    *unknown=true;
    return NULL;
}

void pr_unshift_attack_received(pr_pokemon *p, const pr_attack_record *r)
{
    int kept = pr_attacks_received_kept(p);
    if (kept == PR_MAX_ATTACKS_RECEIVED) kept--;
    memmove(p->attacks_received + 1, p->attacks_received, (size_t)kept * sizeof *p->attacks_received);
    p->attacks_received[0] = *r;
    if (p->n_attacks_received < UINT16_MAX) p->n_attacks_received++;
}

int pr_attacks_received_kept(const pr_pokemon *p)
{
    return p->n_attacks_received < PR_MAX_ATTACKS_RECEIVED ? p->n_attacks_received : PR_MAX_ATTACKS_RECEIVED;
}

void pr_attacks_received_exhausted(pr_battle *bt, const pr_pokemon *p)
{
    if (p->n_attacks_received > PR_MAX_ATTACKS_RECEIVED) pr_note_unported(bt, PR_FEAT_MOVE_HISTORY);
}

pr_pokemon *pr_field_pokemon_by_id(const pr_battle *bt, int64_t id)
{
    for (int i = 0; i < 4; i++)
        if (bt->field[i] && bt->field[i]->id == id) return bt->field[i];
    return NULL;
}

/* globalScene.getPokemonById: the player's party, then the enemy's (a side with no party given:
 * its Pokemon on the field) */
pr_pokemon *pr_pokemon_by_id(const pr_battle *bt, int64_t id)
{
    if (id < 0) return NULL;
    for (int side = 0; side < 2; side++) {
        if (!bt->n_party[side]) {
            for (int i = side * 2; i < side * 2 + 2; i++)
                if (bt->field[i] && bt->field[i]->id == id) return bt->field[i];
            continue;
        }
        for (int k = 0; k < bt->n_party[side]; k++)
            if (bt->party[side][k]->id == id) return bt->party[side][k];
    }
    return NULL;
}

/* areAllies: a === ATTACKER || b === ATTACKER ? false : (a is a player slot) === (b is a player slot) */
bool pr_are_allies(int a, int b)
{
    if (a == -1 || b == -1) return false;
    return (a == 0 || a == 1) == (b == 0 || b == 1);
}

/* FaintPhase's ordered logs have two consumers: newest turn (Retaliate),
 * and encounter membership (trainer instant Tera). Repeated revivals do not
 * consume new identities within the current six-member party contract. */
static void remember_fainted_id(pr_battle *bt,int side,uint32_t id)
{
    for(int i=0;i<bt->n_fainted_ids[side]&&i<PR_MAX_PARTY;i++)if(bt->fainted_ids[side][i]==id)return;
    if(bt->n_fainted_ids[side]>=PR_MAX_PARTY){
        bt->faint_ids_complete[side]=false;pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);return;
    }
    bt->fainted_ids[side][bt->n_fainted_ids[side]++]=id;
}
void pr_record_faint(pr_battle *bt,int side,uint32_t id,int turn)
{
    if(side<0||side>1){pr_note_unported(bt,PR_FEAT_FAINT_HISTORY);return;}
    int kept=bt->n_faint_history[side];
    if(kept>12){pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);kept=12;}
    if(!bt->faint_history_total[side]){
        bt->faint_history_total[side]=(uint32_t)kept;
        bt->faint_ids_complete[side]=true;
    }
    // Raw callers may supply a full tail with no membership summary.
    for(int i=0;i<kept;i++)remember_fainted_id(bt,side,bt->faint_history[side][i].id);
    remember_fainted_id(bt,side,id);
    if(kept==12){memmove(bt->faint_history[side],bt->faint_history[side]+1,11*sizeof bt->faint_history[side][0]);kept--;}
    bt->faint_history[side][kept].id=id;bt->faint_history[side][kept].turn=turn;
    bt->n_faint_history[side]=(uint8_t)(kept+1);
    if(bt->faint_history_total[side]<UINT32_MAX)bt->faint_history_total[side]++;
    else pr_note_unported(bt,PR_FEAT_ENGINE_LIMIT);
}
bool pr_faint_history_contains(pr_battle *bt,int side,uint32_t id,bool *unknown)
{
    *unknown=false;
    if(side<0||side>1){*unknown=true;pr_note_unported(bt,PR_FEAT_FAINT_HISTORY);return false;}
    int kept=bt->n_faint_history[side];
    if(kept>12){*unknown=true;pr_note_unported(bt,PR_FEAT_FAINT_HISTORY);return false;}
    for(int i=0;i<kept;i++)if(bt->faint_history[side][i].id==id)return true;
    if(bt->n_fainted_ids[side]>PR_MAX_PARTY){*unknown=true;pr_note_unported(bt,PR_FEAT_FAINT_HISTORY);return false;}
    for(int i=0;i<bt->n_fainted_ids[side]&&i<PR_MAX_PARTY;i++)if(bt->fainted_ids[side][i]==id)return true;
    if(bt->faint_history_total[side]<=(uint32_t)kept||bt->faint_ids_complete[side])return false;
    *unknown=true;pr_note_unported(bt,PR_FEAT_FAINT_HISTORY);return false;
}
