/**
 * Calculations for monster state
 */

#include "angband.h"
#include "mon-calcs.h"
#include "object.h"
#include "obj-util.h"
#include "monster.h"
#include "player-calcs.h"
#include "player-util.h"


static int mon_ac(struct monster *mon)
{
	int base = mon->race->ac, ac = 0, to_a = 0;
	struct object *obj;

	for (obj = mon->equipped_obj; obj; obj = obj->next) {
		ac += obj->ac;
		to_a = object_to_ac(obj);
	}

	return MAX(base, ac) + MIN(base, ac) / 2 + to_a;
}


void calc_mon_bonuses(struct monster *mon, struct player_state *state)
{
    int i, extra_blows = 0;

    memset(state, 0, sizeof *state);

    state->ac = mon_ac(mon);
    state->speed = mon->race->speed;

    for (i = 0; i < PP_MAX; ++i) {
        state->powers[i] = mon->abilities[i];
    }
    
    
	if (mon->m_timed[TMD_INVULN]) {
		state->to_a += 100;
	}
	if (mon->m_timed[TMD_BLESSED]) {
		state->to_a += 5;
		state->to_h += 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (mon->m_timed[TMD_SHIELD]) {
		state->to_a += 50;
	}
	if (mon->m_timed[TMD_STONESKIN]) {
		state->to_a += 40;
		state->speed -= 5;
	}
	if (mon->m_timed[TMD_HERO]) {
		state->to_h += 12;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (mon->m_timed[TMD_SHERO]) {
		state->skills[SKILL_TO_HIT_MELEE] += 75;
		state->to_a -= 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
	}
	if (mon->m_timed[TMD_FAST] || mon->m_timed[TMD_SPRINT]) {
		state->speed += 10;
	}
	if (mon->m_timed[TMD_SLOW]) {
		state->speed -= 10;
	}
	if (mon->m_timed[TMD_SINFRA]) {
		state->see_infra += 5;
	}
	if (mon->m_timed[TMD_TERROR]) {
		state->speed += 10;
	}
	for (i = 0; i < TMD_MAX; ++i) {
		if (mon->m_timed[i] && timed_effects[i].temp_resist != -1
				&& state->el_info[timed_effects[i].temp_resist].res_level
				< 2) {
			state->el_info[timed_effects[i].temp_resist].res_level++;
		}
	}
	if (mon->m_timed[TMD_CONFUSED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 4, 0);
	}
	if (mon->m_timed[TMD_AMNESIA]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (mon->m_timed[TMD_POISONED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}
	if (mon->m_timed[TMD_IMAGE]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (mon->m_timed[TMD_BLOODLUST]) {
		int p_berserk = get_power_scale_state(state, PP_BERSERK, 150, mon->race->level);
		int bonus = mon->m_timed[TMD_BLOODLUST] * (100 + p_berserk) / 100;

		state->to_d += bonus / 5 + 1;
		state->to_h += bonus * 2 / 3;
		extra_blows += bonus * 4;
		state->speed += bonus / 5 - 3;
		state->dam_red += bonus * (mon->maxhp + 100) / 1000;
		adjust_skill_scale(&state->skills[SKILL_STEALTH], -bonus, 5, 10);
		adjust_skill_scale(&state->skills[SKILL_SAVE], bonus, 20, 10);
	}
	if (mon->m_timed[TMD_STEALTH]) {
		state->skills[SKILL_STEALTH] += 10;
	}
}


void update_mon_state(struct monster *mon)
{
    if (mflag_has(mon->mflag, MFLAG_UPDATE)) {
        calc_mon_bonuses(mon, &mon->state);
        mflag_off(mon->mflag, MFLAG_UPDATE);
    }
}


