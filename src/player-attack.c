/**
 * \file player-attack.c
 * \brief Attacks (both throwing and melee) by the player
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "cmds.h"
#include "effects.h"
#include "game-event.h"
#include "game-input.h"
#include "generate.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-msg.h"
#include "mon-predicate.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "target.h"


/**
 * ------------------------------------------------------------------------
 * Hit and breakage calculations
 * ------------------------------------------------------------------------ */
/**
 * Returns percent chance of an object breaking after throwing or shooting.
 *
 * Artifacts will never break.
 *
 * Beyond that, each item kind has a percent chance to break (0-100). When the
 * object hits its target this chance is used.
 *
 * When an object misses it also has a chance to break. This is determined by
 * squaring the normaly breakage probability. So an item that breaks 100% of
 * the time on hit will also break 100% of the time on a miss, whereas a 50%
 * hit-breakage chance gives a 25% miss-breakage chance, and a 10% hit breakage
 * chance gives a 1% miss-breakage chance.
 */
int breakage_chance(const struct object *obj, bool hit_target) {
	int perc = obj->kind->base->break_perc;

	if (obj->artifact) return 0;
	if (of_has(obj->flags, OF_THROWING) &&
		!of_has(obj->flags, OF_EXPLODE) &&
		!tval_is_ammo(obj)) {
		perc = 1;
	}
	if (!hit_target) return (perc * perc) / 100;
	return perc;
}

/**
 * Calculate the player's base melee to-hit value without regard to a specific
 * monster.
 * See also: chance_of_missile_hit_base
 *
 * \param p The player
 * \param weapon The player's weapon
 */
int chance_of_melee_hit_base(const struct player *p, struct attack_roll *aroll)
{
	int bonus = aroll->to_hit;
	return p->state.skills[aroll->attack_skill] + bonus * BTH_PLUS_ADJ;
}

/**
 * Calculate the player's melee to-hit value against a specific monster.
 * See also: chance_of_missile_hit
 *
 * \param p The player
 * \param weapon The player's weapon
 * \param mon The monster
 */
static int chance_of_melee_hit(const struct player *p,
		struct attack_roll *aroll, const struct monster *mon)
{
	int chance = chance_of_melee_hit_base(p, aroll);
	/* Non-visible targets have a to-hit penalty of 50% */
	return monster_is_visible(mon) ? chance : chance / 2;
}

/**
 * Calculate the player's base missile to-hit value without regard to a specific
 * monster.
 * See also: chance_of_melee_hit_base
 *
 * \param p The player
 * \param missile The missile to launch
 * \param launcher The launcher to use (optional)
 */
int chance_of_missile_hit_base(const struct player *p,
								 const struct object *missile,
								 const struct object *launcher)
{
	int bonus = object_to_hit(missile);
	int chance;

	if (!launcher) {
		/* Other thrown objects are easier to use, but only throwing weapons 
		 * take advantage of bonuses to Skill and Deadliness from other 
		 * equipped items. */
		if (of_has(missile->flags, OF_THROWING)) {
			bonus += p->state.to_h;
			chance = p->state.skills[SKILL_TO_HIT_THROW] + bonus * BTH_PLUS_ADJ;
		} else {
			chance = 3 * p->state.skills[SKILL_TO_HIT_THROW] / 2
				+ bonus * BTH_PLUS_ADJ;
		}
	} else {
		bonus += p->state.to_h + object_to_hit(launcher);
		chance = p->state.skills[SKILL_TO_HIT_BOW] + bonus * BTH_PLUS_ADJ;
	}

	return chance;
}

/**
 * Calculate the player's missile to-hit value against a specific monster.
 * See also: chance_of_melee_hit
 *
 * \param p The player
 * \param missile The missile to launch
 * \param launcher Optional launcher to use (thrown weapons use no launcher)
 * \param mon The monster
 */
static int chance_of_missile_hit(const struct player *p,
	const struct object *missile, const struct object *launcher,
	const struct monster *mon)
{
	int chance = chance_of_missile_hit_base(p, missile, launcher);
	/* Penalize for distance */
	chance -= distance(p->grid, mon->grid);
	/* Non-visible targets have a to-hit penalty of 50% */
	return monster_is_obvious(mon) ? chance : chance / 2;
}

/**
 * Determine if a hit roll is successful against the target AC.
 * See also: hit_chance
 *
 * \param to_hit To total to-hit value to use
 * \param ac The AC to roll against
 */
bool test_hit(int to_hit, int ac)
{
	random_chance c;
	hit_chance(&c, to_hit, ac);
	return random_chance_check(c);
}

#if 0
/**
 * Return a random_chance by reference, which represents the likelihood of a
 * hit roll succeeding for the given to_hit and ac values. The hit calculation
 * will:
 *
 * Always hit 12% of the time
 * Always miss 5% of the time
 * Put a floor of 9 on the to-hit value
 * Roll between 0 and the to-hit value
 * The outcome must be >= AC*2/3 to be considered a hit
 *
 * \param chance The random_chance to return-by-reference
 * \param to_hit The to-hit value to use
 * \param ac The AC to roll against
 */
void hit_chance_old(random_chance *chance, int to_hit, int ac)
{
	/* Percentages scaled to 10,000 to avoid rounding error */
	const int HUNDRED_PCT = 10000;
	const int ALWAYS_HIT = 1200;
	const int ALWAYS_MISS = 500;

	/* Put a floor on the to_hit */
	to_hit = MAX(9, to_hit);

	/* Calculate the hit percentage */
	chance->numerator = MAX(0, to_hit - ac * 2 / 3);
	chance->denominator = to_hit;

	/* Convert the ratio to a scaled percentage */
	chance->numerator = HUNDRED_PCT * chance->numerator / chance->denominator;
	chance->denominator = HUNDRED_PCT;

	/* The calculated rate only applies when the guaranteed hit/miss don't */
	chance->numerator = chance->numerator *
			(HUNDRED_PCT - ALWAYS_MISS - ALWAYS_HIT) / HUNDRED_PCT;

	/* Add in the guaranteed hit */
	chance->numerator += ALWAYS_HIT;
}
#endif

void hit_chance(random_chance *chance, int to_hit, int ac)
{
	const int ALWAYS_HIT = 12;
	const int ALWAYS_MISS = 5;

	chance->denominator = 100;

    int scaleto = chance->denominator - ALWAYS_HIT - ALWAYS_MISS;

	chance->numerator = scaleto / 2 + (to_hit - ac) * 5 / 3;
	chance->numerator = MAX(chance->numerator, 0);
	chance->numerator = MIN(chance->numerator, scaleto);
	chance->numerator += ALWAYS_HIT;
}

/**
 * ------------------------------------------------------------------------
 * Damage calculations
 * ------------------------------------------------------------------------ */
/**
 * Conversion of plusses to Deadliness to a percentage added to damage.
 * Much of this table is not intended ever to be used, and is included
 * only to handle possible inflation elsewhere. -LM-
 */
uint8_t deadliness_conversion[151] =
  {
    0,
    5,  10,  14,  18,  22,  26,  30,  33,  36,  39,
    42,  45,  48,  51,  54,  57,  60,  63,  66,  69,
    72,  75,  78,  81,  84,  87,  90,  93,  96,  99,
    102, 104, 107, 109, 112, 114, 117, 119, 122, 124,
    127, 129, 132, 134, 137, 139, 142, 144, 147, 149,
    152, 154, 157, 159, 162, 164, 167, 169, 172, 174,
    176, 178, 180, 182, 184, 186, 188, 190, 192, 194,
    196, 198, 200, 202, 204, 206, 208, 210, 212, 214,
    216, 218, 220, 222, 224, 226, 228, 230, 232, 234,
    236, 238, 240, 242, 244, 246, 248, 250, 251, 253,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255
  };

/**
 * Deadliness multiplies the damage done by a percentage, which varies 
 * from 0% (no damage done at all) to at most 355% (damage is multiplied 
 * by more than three and a half times!).
 *
 * We use the table "deadliness_conversion" to translate internal plusses 
 * to deadliness to percentage values.
 *
 * This function multiplies damage by 100.
 */
void apply_deadliness(int *die_average, int deadliness)
{
	int i;

	/* Paranoia - ensure legal table access. */
	if (deadliness > 150)
		deadliness = 150;
	if (deadliness < -150)
		deadliness = -150;

	/* Deadliness is positive - damage is increased */
	if (deadliness >= 0) {
		i = deadliness_conversion[deadliness];

		*die_average *= (100 + i);
	}

	/* Deadliness is negative - damage is decreased */
	else {
		i = deadliness_conversion[ABS(deadliness)];

		if (i >= 100)
			*die_average = 0;
		else
			*die_average *= (100 - i);
	}
}

/**
 * Check if a monster is debuffed in such a way as to make a critical
 * hit more likely.
 */
static bool is_debuffed(const struct monster *monster)
{
	return monster->m_timed[MON_TMD_CONF] > 0 ||
			monster->m_timed[MON_TMD_HOLD] > 0 ||
			monster->m_timed[MON_TMD_FEAR] > 0 ||
			monster->m_timed[MON_TMD_STUN] > 0;
}

/**
 * Determine damage for critical hits from shooting.
 *
 * Factor in item weight, total plusses, and player level.
 */
static int critical_shot(const struct player *p,
		const struct monster *monster,
		int weight, int plus,
		int dam, bool launched, uint32_t *msg_type)
{
	int to_h = p->state.to_h + plus;
	int chance, new_dam;

	if (is_debuffed(monster)) {
		to_h += z_info->r_crit_debuff_toh;
	}
	chance = z_info->r_crit_chance_weight_scl * weight
		+ z_info->r_crit_chance_toh_scl * to_h
		+ z_info->r_crit_chance_level_scl * p->lev
		+ z_info->r_crit_chance_offset;
	if (launched) {
		chance += z_info->r_crit_chance_launched_toh_skill_scl
			* p->state.skills[SKILL_TO_HIT_BOW];
	} else {
		chance += z_info->r_crit_chance_thrown_toh_skill_scl
			* p->state.skills[SKILL_TO_HIT_THROW];
	}

	if (randint1(z_info->r_crit_chance_range) > chance
			|| !z_info->r_crit_level_head) {
		*msg_type = MSG_SHOOT_HIT;
		new_dam = dam;
	} else {
		int power = z_info->r_crit_power_weight_scl * weight
			+ randint1(z_info->r_crit_power_random);
		const struct critical_level *this_l = z_info->r_crit_level_head;

		while (power >= this_l->cutoff && this_l->next) {
			this_l = this_l->next;
		}
		*msg_type = this_l->msgt;
		new_dam = this_l->add + this_l->mult * dam;
	}

	return new_dam;
}

/**
 * Determine O-combat damage for critical hits from shooting.
 */
static int o_critical_shot(const struct player *p,
		const struct monster *monster,
		const struct object *missile,
		const struct object *launcher,
		uint32_t *msg_type)
{
	int power = chance_of_missile_hit_base(p, missile, launcher);
	int chance_num, chance_den, add_dice;

	if (is_debuffed(monster)) {
		power += z_info->o_r_crit_debuff_toh;
	}
	/* Apply a rational scale factor. */
	if (launcher) {
		power = (power * z_info->o_r_crit_power_launched_toh_scl_num)
			/ z_info->o_r_crit_power_launched_toh_scl_den;
	} else {
		power = (power * z_info->o_r_crit_power_thrown_toh_scl_num)
			/ z_info->o_r_crit_power_thrown_toh_scl_den;
	}

	/* Test for critical hit:  chance is a * power / (b * power + c) */
	chance_num = power * z_info->o_r_crit_chance_power_scl_num;
	chance_den = power * z_info->o_r_crit_chance_power_scl_den
		+ z_info->o_r_crit_chance_add_den;
	if (randint1(chance_den) <= chance_num && z_info->o_r_crit_level_head) {
		/* Determine level of critical hit. */
		const struct o_critical_level *this_l =
			z_info->o_r_crit_level_head;

		while (this_l->next && !one_in_(this_l->chance)) {
			this_l = this_l->next;
		}
		add_dice = this_l->added_dice;
		*msg_type = this_l->msgt;
	} else {
		add_dice = 0;
		*msg_type = MSG_SHOOT_HIT;
	}

	return add_dice;
}

/**
 * Determine damage for critical hits from melee.
 *
 * Factor in weapon weight, total plusses, player level.
 */
static int critical_melee(const struct player *p,
		const struct monster *monster,
		int weight, int plus,
		int dam, uint32_t *msg_type)
{
	int to_h = p->state.to_h + plus;
	int chance, new_dam;

	if (is_debuffed(monster)) {
		to_h += z_info->m_crit_debuff_toh;
	}
	chance = z_info->m_crit_chance_weight_scl * weight
		+ z_info->m_crit_chance_toh_scl * to_h
		+ z_info->m_crit_chance_level_scl * p->lev
		+ z_info->m_crit_chance_toh_skill_scl
			* p->state.skills[SKILL_TO_HIT_MELEE]
		+ z_info->m_crit_chance_offset;

	if (randint1(z_info->m_crit_chance_range) > chance
			|| !z_info->m_crit_level_head) {
		*msg_type = MSG_HIT;
		new_dam = dam;
	} else {
		int power = z_info->m_crit_power_weight_scl * weight
			+ randint1(z_info->m_crit_power_random);
		const struct critical_level *this_l = z_info->m_crit_level_head;

		while (power >= this_l->cutoff && this_l->next) {
			this_l = this_l->next;
		}
		*msg_type = this_l->msgt;
		new_dam = this_l->add + this_l->mult * dam;
	}

	return new_dam;
}

#if 0
/**
 * Determine O-combat damage for critical hits from melee.
 */
static int o_critical_melee(const struct player *p,
		const struct monster *monster,
		const struct object *obj, uint32_t *msg_type)
{
	int power = chance_of_melee_hit_base(p, obj);
	int chance_num, chance_den, add_dice;

	if (is_debuffed(monster)) {
		power += z_info->o_m_crit_debuff_toh;
	}
	/* Apply a rational scale factor. */
	power = (power * z_info->o_m_crit_power_toh_scl_num)
		/ z_info->o_m_crit_power_toh_scl_den;

	/* Test for critical hit:  chance is a * power / (b * power + c) */
	chance_num = power * z_info->o_m_crit_chance_power_scl_num;
	chance_den = power * z_info->o_m_crit_chance_power_scl_den
		+ z_info->o_m_crit_chance_add_den;
	if (randint1(chance_den) <= chance_num && z_info->o_m_crit_level_head) {
		/* Determine level of critical hit. */
		const struct o_critical_level *this_l =
			z_info->o_m_crit_level_head;

		while (this_l->next && !one_in_(this_l->chance)) {
			this_l = this_l->next;
		}
		add_dice = this_l->added_dice;
		*msg_type = this_l->msgt;
	} else {
		add_dice = 0;
		*msg_type = MSG_SHOOT_HIT;
	}

	return add_dice;
}
#endif

#if 0
/**
 * Determine standard melee damage.
 *
 * Factor in damage dice, to-dam and any brand or slay.
 */
static int melee_damage(const struct monster *mon, struct object *obj, int b, int s)
{
	int dmg;
	if (obj) dmg = damroll(obj->dd, obj->ds);
	else dmg = damroll(unarmed_melee_dam_dice(), unarmed_melee_dam_sides());

	if (s) {
		dmg *= slays[s].multiplier;
	} else if (b) {
		dmg *= get_monster_brand_multiplier(mon, &brands[b], false);
	}

	if (obj) dmg += object_to_dam(obj);
	else dmg += unarmed_melee_to_dam();

	return dmg;
}
#endif

#if 0
/**
 * Determine O-combat melee damage.
 *
 * Deadliness and any brand or slay add extra sides to the damage dice,
 * criticals add extra dice.
 */
static int o_melee_damage(struct player *p, const struct monster *mon,
		struct object *obj, int b, int s, uint32_t *msg_type)
{
	int dice = (obj) ? obj->dd : 1;
	int sides, deadliness, dmg, add = 0;
	bool extra;

	/* Get the average value of a single damage die. (x10) */
	int die_average = (10 * (((obj) ? obj->ds : 1) + 1)) / 2;

	/* Adjust the average for slays and brands. (10x inflation) */
	if (s) {
		die_average *= slays[s].o_multiplier;
		add = slays[s].o_multiplier - 10;
	} else if (b) {
		int bmult = get_monster_brand_multiplier(mon, &brands[b], true);

		die_average *= bmult;
		add = bmult - 10;
	} else {
		die_average *= 10;
	}

	/* Apply deadliness to average. (100x inflation) */
	deadliness = p->state.to_d + ((obj) ? object_to_dam(obj) : 0);
	apply_deadliness(&die_average, MIN(deadliness, 150));

	/* Calculate the actual number of sides to each die. */
	sides = (2 * die_average) - 10000;
	extra = randint0(10000) < (sides % 10000);
	sides /= 10000;
	sides += (extra ? 1 : 0);

	/*
	 * Get number of critical dice; for now, excluding criticals for
	 * unarmed combat
	 */
	if (obj) dice += o_critical_melee(p, mon, obj, msg_type);

	/* Roll out the damage. */
	dmg = damroll(dice, sides);

	/* Apply any special additions to damage. */
	dmg += add;

	return dmg;
}
#endif

/**
 * Determine standard ranged damage.
 *
 * Factor in damage dice, to-dam, multiplier and any brand or slay.
 */
static int ranged_damage(struct player *p, const struct monster *mon,
						 struct object *missile, struct object *launcher,
						 int b, int s)
{
	int dmg;
	int mult = (launcher ? p->state.ammo_mult : 1);

	/* If we have a slay or brand, modify the multiplier appropriately */
	if (b) {
		mult += get_monster_brand_multiplier(mon, &brands[b], false);
	} else if (s) {
		mult += slays[s].multiplier;
	}

	/* Apply damage: multiplier, slays, bonuses */
	dmg = damroll(missile->dd, missile->ds);
	dmg += object_to_dam(missile);
	if (launcher) {
		dmg += object_to_dam(launcher);
	} else if (of_has(missile->flags, OF_THROWING)) {
		/* Adjust damage for throwing weapons.
		 * This is not the prettiest equation, but it does at least try to
		 * keep throwing weapons competitive. */
		dmg *= 2 + object_weight_one(missile) / 12;
	}
	dmg *= mult;

	return dmg;
}

/**
 * Determine O-combat ranged damage.
 *
 * Deadliness, launcher multiplier and any brand or slay add extra sides to the
 * damage dice, criticals add extra dice.
 */
static int o_ranged_damage(struct player *p, const struct monster *mon,
		struct object *missile, struct object *launcher,
		int b, int s, uint32_t *msg_type)
{
	int mult = (launcher ? p->state.ammo_mult : 1);
	int dice = missile->dd;
	int sides, deadliness, dmg, add = 0;
	bool extra;

	/* Get the average value of a single damage die. (x10) */
	int die_average = (10 * (missile->ds + 1)) / 2;

	/* Apply the launcher multiplier. */
	die_average *= mult;

	/* Adjust the average for slays and brands. (10x inflation) */
	if (b) {
		int bmult = get_monster_brand_multiplier(mon, &brands[b], true);

		die_average *= bmult;
		add = bmult - 10;
	} else if (s) {
		die_average *= slays[s].o_multiplier;
		add = slays[s].o_multiplier - 10;
	} else {
		die_average *= 10;
	}

	/* Apply deadliness to average. (100x inflation) */
	deadliness = object_to_dam(missile);
	if (launcher) {
		deadliness += object_to_dam(launcher) + p->state.to_d;
	} else if (of_has(missile->flags, OF_THROWING)) {
		deadliness += p->state.to_d;
	}
	apply_deadliness(&die_average, MIN(deadliness, 150));

	/* Calculate the actual number of sides to each die. */
	sides = (2 * die_average) - 10000;
	extra = randint0(10000) < (sides % 10000);
	sides /= 10000;
	sides += (extra ? 1 : 0);

	/* Get number of critical dice - only for suitable objects */
	if (launcher) {
		dice += o_critical_shot(p, mon, missile, launcher, msg_type);
	} else if (of_has(missile->flags, OF_THROWING)) {
		dice += o_critical_shot(p, mon, missile, NULL, msg_type);

		/* Multiply the number of damage dice by the throwing weapon
		 * multiplier.  This is not the prettiest equation,
		 * but it does at least try to keep throwing weapons competitive. */
		dice *= 2 + object_weight_one(missile) / 12;
	}

	/* Roll out the damage. */
	dmg = damroll(dice, sides);

	/* Apply any special additions to damage. */
	dmg += add;

	return dmg;
}

/**
 * Apply the player damage bonuses
 */
static int player_damage_bonus(struct player_state *state)
{
	return state->to_d;
}

/**
 * ------------------------------------------------------------------------
 * Non-damage melee blow effects
 * ------------------------------------------------------------------------ */
/**
 * Apply blow side effects
 */
static void blow_side_effects(struct player *p, struct monster *mon)
{
	/* Confusion attack */
	if (p->timed[TMD_ATT_CONF]) {
		player_clear_timed(p, TMD_ATT_CONF, true, false);

		mon_inc_timed(mon, MON_TMD_CONF, (10 + randint0(p->lev) / 10),
					  MON_TMD_FLG_NOTIFY);
	}
}

/**
 * Apply blow after effects
 */
static bool blow_after_effects(struct loc grid, int dmg, int splash,
							   bool *fear, bool quake, struct attack_roll *aroll)
{
    struct monster *mon = square_monster(cave, grid);
	bool gone = false;

    if (mon) {
		int i;
		int flgs = MON_TMD_FLG_NOMESSAGE | MON_TMD_FLG_GETS_SAVE;
		for (i = 0; i < MON_TMD_MAX; i++) {
			if (!aroll->mtimed[i]) continue;
			int power = randint1(aroll->mtimed[i]);
			if (power > 25)
				mon_inc_timed(mon, i, power, flgs);
		}
	}

	/* Apply earthquake brand */
	if (quake) {
		effect_simple(EF_EARTHQUAKE, source_player(), "0", 0, 10, 0, 0, 0,
					  NULL);

		/* Monster may be dead or moved */
		if (!square_monster(cave, grid))
			gone = true;
	}

	return gone;
}



/**
 * L: functions to create the attack itself
 */

static void unarmed_mod_attack(struct attack_roll *aroll, struct object *obj)
{
	if (obj) return;
	int lev = get_power_scale(player, PP_UNARMED_STRIKE);
	if (!lev) return;
    aroll->dsides += (lev * 10 + 49) / 50;
	aroll->to_hit += (lev * 25 + 49) / 50;
	aroll->to_dam += (lev * 5 + 49) / 50;
	aroll->mtimed[MON_TMD_STUN] += (lev * 50 + 49) / 50;
}

static void unarmed_get_attack(struct attack_roll *aroll, struct object *obj)
{
	int lev = get_power_scale(player, PP_UNARMED_STRIKE);
	aroll->ddice = lev < 15 ? 1 : lev < 30 ? 2 : 3;
	aroll->dsides = 1 + (lev * 9 + 49) / 50;
	aroll->mtimed[MON_TMD_STUN] = (lev * 25 + 49) / 50;
	aroll->accuracy_stat = STAT_DEX;
	aroll->damage_stat = STAT_STR;
	aroll->message = "punch";
	aroll->obj = NULL;
	aroll->proj_type = PROJ_BLUDGEONING;
}

static void specialization_mod_attack(struct attack_roll *aroll, struct object *obj)
{
	if (!obj) return;
	int lev = 0;
    if (obj->tval == TV_HAFTED) lev = get_power_scale(player, PP_HAFTED_SPECIALIZATION);
	if (obj->tval == TV_POLEARM) lev = get_power_scale(player, PP_POLEARM_SPECIALIZATION);
	if (obj->tval == TV_SWORD) lev = get_power_scale(player, PP_SWORD_SPECIALIZATION);
	
	aroll->to_hit += (lev * 15 + 49) / 50;
	aroll->to_dam += (lev * 10 + 49) / 50;
	aroll->dsides += (lev * 10 + 49) / 50;
}

static bool backstab_mod_attack(struct attack_roll *aroll, int power)
{
	if (!power) return false;
	if (aroll->attack_skill != SKILL_TO_HIT_MELEE) return false;
	int lev = get_power_scale(player, PP_BACKSTAB);

	int dsm = lev + 25 * power;
	if (dsm <= 25) return false;

	aroll->dsides *= dsm;
	aroll->dsides += 24;
	aroll->dsides /= 25;
	aroll->to_hit += (lev * 10 + 49) / 50;
	aroll->to_dam += (lev * 10 + 49) / 50;
	return true;
}


/**
 * L: get a weapon attack
 */
struct attack_roll get_weapon_attack(struct player *p, struct player_state *ps, struct object *obj)
{
	struct attack_roll aroll = { 0 };
	if (obj) {
		int td = object_to_dam(obj);
		aroll.ddice = obj->dd;
		aroll.dsides = obj->ds + td / 2;
		aroll.to_dam = (td + 1) / 2;
		aroll.to_hit = object_to_hit(obj);
		aroll.message = "hit";
		aroll.accuracy_stat = STAT_DEX;
		aroll.damage_stat = STAT_STR;
		aroll.obj = obj;
		aroll.proj_type = obj->kind->base->proj_type;
	}
	else {
		unarmed_get_attack(&aroll, obj);
        unarmed_mod_attack(&aroll, obj);
	}

	aroll.attack_skill = SKILL_TO_HIT_MELEE;

	aroll.to_hit += ps->to_h;
	aroll.dsides += player_damage_bonus(&p->state);
	aroll.ddice = aroll.ddice * (ps->skills[aroll.attack_skill] + 33) / 33;

	aroll.to_hit += adj_dex_th(ps->stat_ind[aroll.accuracy_stat]);
	aroll.dsides += adj_str_td(ps->stat_ind[aroll.damage_stat]);

	specialization_mod_attack(&aroll, obj);

	aroll.dsides = MAX(aroll.dsides, 1);
	aroll.ddice = MAX(aroll.ddice, 1);

	return aroll;
}

static bool monster_attack_is_usable(struct player *p, struct monster_blow *blow)
{
	bool foundempty = false;
	bool foundfull = false;
	int j;
	struct object *obj;

	if (blow->method->skill == SKILL_SEARCH) {
		if (p->timed[TMD_BLIND]) return false;
	}

	for (j = 0; j < p->body.count; j++) {
		if (!slot_type_is(p, j, blow->method->equip_slot)) continue;

		obj = slot_object(p, j);

		if (obj) foundfull = true;
		else foundempty = true;
	}
	if (foundfull && !foundempty) return false;

	return true;
}

static int mon_blow_dam_stat(struct monster_blow *mb, struct player_state *ps)
{
	if (mb->method->skill == SKILL_SEARCH) {
		return ps->stat_ind[STAT_INT] > ps->stat_ind[STAT_WIS] ? STAT_WIS : STAT_INT;
	}
	return STAT_STR;
}

static bool get_monster_attack(struct player *p, struct player_state *ps,
							   struct monster_race *mr, struct attack_roll *aroll, int aind)
{
	int j;
	struct monster_blow *mb = &mr->blow[aind];
	if (!mb->method) return false;
	if (!monster_attack_is_usable(p, mb)) return false;

	int mindice = 1;
	int minsides = mb->dice.sides ? 1 : 0;

	aroll->ddice = mb->dice.dice;
	aroll->dsides = (mb->dice.sides + 1) / 2;
	aroll->blows = 100;
	aroll->message = (const char *)mb->method->fmessage;
	aroll->to_hit = 0;
	aroll->to_dam = 0;
	if (mb->effect->lash_type == -1) {
		aroll->proj_type = mb->method->lash_type;
	} else {
		aroll->proj_type = mb->effect->lash_type;
	}
	aroll->attack_skill = mb->method->skill;
	aroll->obj = NULL;
	aroll->blows = 100;
	for (j = 0; j < MON_TMD_MAX; j++) {
		if (mb->effect->mtimed == j)
			aroll->mtimed[j] = 50 + mr->level;
		else
			aroll->mtimed[j] = 0;
	}
	aroll->accuracy_stat = STAT_DEX;
	aroll->damage_stat = mon_blow_dam_stat(mb, ps);

	aroll->to_hit += ps->to_h;
	aroll->dsides += player_damage_bonus(ps);
	//aroll->ddice = aroll->ddice * (p->state.skills[aroll->attack_skill] + 33) / 33;
	
	aroll->to_hit += adj_dex_th(ps->stat_ind[aroll->accuracy_stat]);
	aroll->dsides += adj_str_td(ps->stat_ind[aroll->damage_stat]);

	aroll->dsides = MAX(minsides, aroll->dsides);
	aroll->ddice = MAX(mindice, aroll->ddice);

	if (aroll->attack_skill == SKILL_TO_HIT_MELEE)
		unarmed_mod_attack(aroll, NULL);

	return true;
}

int get_monster_attacks(struct player *p, struct player_state *ps,
						struct monster_race *mr, struct attack_roll *aroll, int maxnum)
{
	if (!mr) return 0;
	if (!mr->blow[0].method) return 0;
	//struct monster_blow *mb;
	int mbi, pai = 0;

	/*for (mbi = 0; mbi < z_info->mon_blows_max && mr->blow[mbi].method; mbi++) {
		mb = &mr->blow[mbi];
		if (!monster_attack_is_usable(p, mb)) continue;
		currdam = (mb->dice.sides + 1) * mb->dice.dice;
		currdam += adj_str_td(p->state.stat_ind[mon_blow_dam_stat(mb, p)]) * 2;
		if (currdam > bestblowdam) {
			bestblow = mbi;
			bestblowdam = currdam;
		}
	}

	if (bestblow >= 0) {
		if (get_monster_attack(p, mr, aroll, bestblow))	++pai;
	}*/

	for (mbi = 0; mbi < z_info->mon_blows_max && pai < maxnum && mr->blow[mbi].method; mbi++) {
		if (get_monster_attack(p, ps, mr, &aroll[pai], mbi)) ++pai;
	}

	return pai;
}

static int get_attack_dam(struct attack_roll *aroll, struct monster *mon, int b, int s) {
    int dmg = damroll(aroll->ddice, aroll->dsides) + aroll->to_dam;
	if (s) {
		dmg *= slays[s].multiplier;
	} else if (b) {
		dmg *= get_monster_brand_multiplier(mon, &brands[b], false);
	}
	return dmg;
}





/**
 * ------------------------------------------------------------------------
 * Melee attack
 * ------------------------------------------------------------------------ */
/* Melee and throwing hit types */
static const struct hit_types melee_hit_types[] = {
	{ MSG_MISS, NULL },
	{ MSG_HIT, NULL },
	{ MSG_HIT_GOOD, "It was a good hit!" },
	{ MSG_HIT_GREAT, "It was a great hit!" },
	{ MSG_HIT_SUPERB, "It was a superb hit!" },
	{ MSG_HIT_HI_GREAT, "It was a *GREAT* hit!" },
	{ MSG_HIT_HI_SUPERB, "It was a *SUPERB* hit!" },
};

/**
 * Attack the monster at the given location with a single blow.
 */
bool py_attack_real(struct player *p, struct loc grid, bool *fear, struct attack_roll aroll)
{
	size_t i;

	/* Information about the target of the attack */
	struct monster *mon = square_monster(cave, grid);
	char m_name[80];
	bool stop = false;

	/* The weapon used */
	struct object *obj = slot_object(p, slot_by_type(p, EQUIP_WEAPON, true));

	/* Information about the attack */
	int drain = 0;
	int splash = 0;
	bool do_quake = false;
	bool success = false;

	char verb[20];
	uint32_t msg_type = MSG_HIT;
	int j, b, s, weight, dmg;

	/* Default to punching / kicking */
	if (one_in_(3))
	    my_strcpy(verb, "kick", sizeof(verb));
	else
    	my_strcpy(verb, "punch", sizeof(verb));

	/* Extract monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);

	/* Auto-Recall and track if possible and visible */
	if (monster_is_visible(mon)) {
		monster_race_track(p->upkeep, mon->race);
		health_track(p->upkeep, mon);
	}

	/* Handle player fear (only for invisible monsters) */
	if (player_of_has(p, OF_AFRAID)) {
		equip_learn_flag(p, OF_AFRAID);
		msgt(MSG_AFRAID, "You are too afraid to attack %s!", m_name);
		return false;
	}

	/* Disturb the monster */
	monster_wake(mon, false, 100);
	mon_clear_timed(mon, MON_TMD_HOLD, MON_TMD_FLG_NOMESSAGE);

	/* See if the player hit */
	success = test_hit(chance_of_melee_hit(p, &aroll, mon), mon->race->ac);

	/* If a miss, skip this hit */
	if (!success) {
		msgt(MSG_MISS, "You miss %s.", m_name);

		/* Small chance of bloodlust side-effects */
		if (p->timed[TMD_BLOODLUST] && one_in_(50)) {
			msg("You feel strange...");
			player_over_exert(p, PY_EXERT_SCRAMBLE, 20, 20);
		}

		return false;
	}

	if (obj) {
		/* Handle normal weapon */
		weight = object_weight_one(obj);
	} else {
		weight = 0;
	}

	if (aroll.message) {
		my_strcpy(verb, aroll.message, sizeof(verb));
	} else if (obj) {
		my_strcpy(verb, "hit", sizeof(verb));
	}

	/* Best attack from all slays or brands on all non-launcher equipment */
	b = 0;
	s = 0;
	for (j = 0; j < p->body.count; j++) {
		if (slot_type_is(p, j, EQUIP_BOW) || slot_type_is(p, j, EQUIP_WEAPON))
			continue;
		struct object *obj_local = slot_object(p, j);
		if (obj_local)
			improve_attack_modifier(p, obj_local, mon, &b, &s,
				verb, false);
	}

	/* Get the best attack from all slays or brands - weapon or temporary */
	if (obj) {
		improve_attack_modifier(p, obj, mon, &b, &s, verb, false);
	}
	improve_attack_modifier(p, NULL, mon, &b, &s, verb, false);

	/* Get the damage */
	if (true || !OPT(p, birth_percent_damage)) {
		dmg = get_attack_dam(&aroll, mon, b, s);
		/* For now, exclude criticals on unarmed combat */
		if (obj) {
			dmg = critical_melee(p, mon, weight, object_to_hit(obj),
				dmg, &msg_type);
		}
	} else {
		//dmg = o_melee_damage(p, mon, obj, b, s, &msg_type);
	}

	/* Splash damage and earthquakes */
	splash = (weight * dmg) / 100;
	if (player_of_has(p, OF_IMPACT) && dmg > 50) {
		do_quake = true;
		equip_learn_flag(p, OF_IMPACT);
	}

	/* Learn by use */
	equip_learn_on_melee_attack(p);
	learn_brand_slay_from_melee(p, obj, mon);

	/* Substitute shape-specific blows for shapechanged players */
	if (player_is_shapechanged(p)) {
		int choice = randint0(p->shape->num_blows);
		struct player_blow *blow = p->shape->blows;
		while (choice--) {
			blow = blow->next;
		}
		my_strcpy(verb, blow->name, sizeof(verb));
	}

	/* No negative damage; change verb if no damage done */
	if (dmg <= 0) {
		dmg = 0;
		msg_type = MSG_MISS;
		my_strcpy(verb, "fail to harm", sizeof(verb));
	}

	for (i = 0; i < N_ELEMENTS(melee_hit_types); i++) {
		const char *dmg_text = "";
		const char *proj_text = "";
		const struct projection *proj = &projections[aroll.proj_type];

		if (msg_type != melee_hit_types[i].msg_type)
			continue;

		if (OPT(p, show_damage))
			dmg_text = format(" (%d)", dmg);

		if (proj->player_message) {
			char proj_name[64];
			monster_desc(proj_name, sizeof(proj_name), mon, MDESC_PRO_VIS | MDESC_OBJE);
			proj_text = format("; you %s %s", proj->player_message, proj_name);
		}

		if (melee_hit_types[i].text)
			msgt(msg_type, "You %s %s%s. %s", verb, m_name, proj_text, dmg_text,
					melee_hit_types[i].text);
		else
			msgt(msg_type, "You %s %s%s.", verb, m_name, proj_text, dmg_text);

		break;
	}

	/* Pre-damage side effects */
	blow_side_effects(p, mon);

	drain = MIN(mon->hp, dmg);

	/* Damage, check for hp drain, fear and death */
	stop = proj_melee_attack_mon(mon, p, dmg, aroll.proj_type, fear, NULL);
	

	/* Small chance of bloodlust side-effects */
	if (p->timed[TMD_BLOODLUST] && one_in_(50)) {
		msg("You feel something give way!");
		player_over_exert(p, PY_EXERT_CON, 20, 0);
	}

	if (!stop) {
		if (p->timed[TMD_ATT_VAMP] && monster_is_living(mon)) {
			effect_simple(EF_HEAL_HP, source_player(), format("%d", drain),
						  0, 0, 0, 0, 0, NULL);
		}
	}

	if (stop)
		(*fear) = false;

	/* Post-damage effects */
	if (blow_after_effects(grid, dmg, splash, fear, do_quake, &aroll))
		stop = true;

	return stop;
}


/**
 * Attempt a shield bash; return true if the monster dies
 */
static bool attempt_shield_bash(struct player *p, struct monster *mon, bool *fear)
{
	struct object *weapon = slot_object(p, slot_by_type(p, EQUIP_WEAPON, true));
	struct object *shield = NULL;
	int i;
	int nblows = p->state.num_blows / 100;
	int bash_quality, bash_dam, energy_lost;

	for (i = 0; i < p->body.count; i++) {
		if (p->body.slots[i].obj == weapon) continue;
		if (p->body.slots[i].obj && p->body.slots[i].obj->kind->tval == TV_SHIELD)
			shield = p->body.slots[i].obj;
	}

	/* Bashing chance depends on melee skill, DEX, and a level bonus. */
	int bash_chance = p->state.skills[SKILL_TO_HIT_MELEE] / 8 +
		adj_dex_th(p->state.stat_ind[STAT_DEX]) / 2;

	/* No shield, no bash */
	if (!shield) return false;

	/* Monster is too pathetic, don't bother */
	if (mon->race->level < p->lev / 2) return false;

	/* Players bash more often when they see a real need: */
	if (!weapon) {
		/* Unarmed... */
		bash_chance *= 4;
	} else if (weapon->dd * weapon->ds * nblows < shield->dd * shield->ds * 3) {
		/* ... or armed with a puny weapon */
		bash_chance *= 2;
	}

	/* Try to get in a shield bash. */
	if (bash_chance <= randint0(200 + mon->race->level)) {
		return false;
	}

	/* Calculate attack quality, a mix of momentum and accuracy. */
	bash_quality = p->state.skills[SKILL_TO_HIT_MELEE] / 4 + p->wt / 8 +
		p->upkeep->total_weight / 80 + object_weight_one(shield) / 2;

	/* Calculate damage.  Big shields are deadly. */
	bash_dam = damroll(shield->dd, shield->ds);

	/* Multiply by quality and experience factors */
	bash_dam *= bash_quality / 40 + p->lev / 14;

	/* Strength bonus. */
	bash_dam += adj_str_td(p->state.stat_ind[STAT_STR]);

	/* Paranoia. */
	if (bash_dam <= 0) return false;
	bash_dam = MIN(bash_dam, 125);

	if (OPT(p, show_damage)) {
		msgt(MSG_HIT, "You get in a shield bash! (%d)", bash_dam);
	} else {
		msgt(MSG_HIT, "You get in a shield bash!");
	}

	/* Encourage the player to keep wearing that heavy shield. */
	if (randint1(bash_dam) > 30 + randint1(bash_dam / 2)) {
		msgt(MSG_HIT_HI_SUPERB, "WHAMM!");
	}

	/* Damage, check for fear and death. */
	if (mon_take_hit(mon, p, bash_dam, fear, NULL)) return true;

	/* Stunning. */
	if (bash_quality + p->lev > randint1(200 + mon->race->level * 8)) {
		mon_inc_timed(mon, MON_TMD_STUN, randint0(p->lev / 5) + 4, 0);
	}

	/* Confusion. */
	if (bash_quality + p->lev > randint1(300 + mon->race->level * 12)) {
		mon_inc_timed(mon, MON_TMD_CONF, randint0(p->lev / 5) + 4, 0);
	}

	/* The player will sometimes stumble. */
	if (35 + adj_dex_th(p->state.stat_ind[STAT_DEX]) < randint1(60)) {
		energy_lost = randint1(50) + 25;
		/* Lose 26-75% of a turn due to stumbling after shield bash. */
		msgt(MSG_GENERIC, "You stumble!");
		p->upkeep->energy_use += energy_lost * z_info->move_energy / 100;
	}

	return false;
}

/**
 * Attack the monster at the given location
 *
 * We get blows until energy drops below that required for another blow, or
 * until the target monster dies. Each blow is handled by py_attack_real().
 * We don't allow @ to spend more than 1 turn's worth of energy,
 * to avoid slower monsters getting double moves.
 */
void py_attack(struct player *p, struct loc grid)
{
	int avail_energy = MIN(p->energy, z_info->move_energy);
	int blow_energy;// = 100 * z_info->move_energy / p->state.num_blows;
	bool slain = false, fear = false;
	struct monster *mon = square_monster(cave, grid);
	struct attack_roll aroll;
	int i;
	int pretimed[MON_TMD_MAX];
	int backstab = 0;
	bool backstab_msg = false;
	//int sumblows = 0;

	if (mon->m_timed[MON_TMD_SLEEP] || mon->m_timed[MON_TMD_HOLD]) backstab = 2;
	else if (mon->m_timed[MON_TMD_SLOW] || mon->m_timed[MON_TMD_FEAR] || mon->m_timed[MON_TMD_STUN]) backstab = 1;

	for (i = 0; i < MON_TMD_MAX; i++)
		pretimed[i] = (int)mon->m_timed[i];

	/*for (i = 0; i < p->state.num_attacks; i++) {
		sumblows += p->state.attacks[i].blows;
	}*/

	/* Disturb the player */
	disturb(p);

	/* Initialize the energy used */
	p->upkeep->energy_use = 0;

	/* Reward BGs with 5% of max SPs, min 1/2 point */
	if (player_has(p, PF_COMBAT_REGEN)) {
		int32_t sp_gain = (((int32_t)MAX(p->msp, 10)) * 16384) / 5;
		player_adjust_mana_precise(p, sp_gain);
	}

	/* Player attempts a shield bash if they can, and if monster is visible
	 * and not too pathetic */
	if (player_has(p, PF_SHIELD_BASH) && monster_is_visible(mon)) {
		/* Monster may die */
		if (attempt_shield_bash(p, mon, &fear)) return;
	}

	/* Attack until the next attack would exceed energy available or
	 * a full turn or until the enemy dies. We limit energy use
	 * to avoid giving monsters a possible double move. */
	while (!slain) {
		int which = randint0(p->state.num_attacks);
		aroll = p->state.attacks[which];

		blow_energy = 100 * z_info->move_energy / aroll.blows;
		if (blow_energy + p->upkeep->energy_use >= avail_energy)
			break;

		if (backstab && backstab_mod_attack(&aroll, backstab) && !backstab_msg) {
			backstab_msg = true;
			char m_name[80];
			monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);
			msg("You backstab %s.", m_name);
		}

		slain = py_attack_real(p, grid, &fear, aroll);
		p->upkeep->energy_use += blow_energy;
	}

	/*for (i = 1; (i < PY_MAX_ATTACKS) && !slain && (p->state.attacks[i].proj_type >= 0); i++) {
		if (randint0(z_info->move_energy) < p->upkeep->energy_use) {
			aroll = p->state.attacks[i];
			if (backstab)
				backstab_mod_attack(&aroll, backstab);
			slain = py_attack_real(p, grid, &fear, aroll);
		}
	}*/

	if (!slain) {
		mon->target.midx = MON_TARGET_PLAYER;
	}

	/* Hack - delay fear messages */
	/*if (fear && monster_is_visible(mon)) {
		add_monster_message(mon, MON_MSG_FLEE_IN_TERROR, true);
	}*/
	for (i = 0; i < MON_TMD_MAX; i++) {
		if (monster_is_visible(mon)) {
			add_mon_timed_message(mon, i, true, pretimed[i], (int)mon->m_timed[i]);
		}
	}
}

/**
 * ------------------------------------------------------------------------
 * Ranged attacks
 * ------------------------------------------------------------------------ */
/* Shooting hit types */
static const struct hit_types ranged_hit_types[] = {
	{ MSG_MISS, NULL },
	{ MSG_SHOOT_HIT, NULL },
	{ MSG_HIT_GOOD, "It was a good hit!" },
	{ MSG_HIT_GREAT, "It was a great hit!" },
	{ MSG_HIT_SUPERB, "It was a superb hit!" }
};

/**
 * This is a helper function used by do_cmd_throw and do_cmd_fire.
 *
 * It abstracts out the projectile path, display code, identify and clean up
 * logic, while using the 'attack' parameter to do work particular to each
 * kind of attack.
 */
static void ranged_helper(struct player *p,	struct object *obj, int dir,
						  int range, int shots, ranged_attack attack,
						  const struct hit_types *hit_types, int num_types)
{
	int i, j;

	int path_n;
	struct loc path_g[256];

	/* Start at the player */
	struct loc grid = p->grid;

	/* Predict the "target" location */
	struct loc target = loc_sum(grid, loc(99 * ddx[dir], 99 * ddy[dir]));

	bool hit_target = false;
	bool none_left = false;

	struct object *missile;
	int pierce = 1;

	/* Check for target validity */
	if ((dir == DIR_TARGET) && target_okay()) {
		int taim;
		target_get(&target);
		taim = distance(grid, target);
		if (taim > range) {
			char msg[80];
			strnfmt(msg, sizeof(msg),
					"Target out of range by %d squares. Fire anyway? ",
				taim - range);
			if (!get_check(msg)) return;
		}
	}

	/* Sound */
	sound(MSG_SHOOT);

	/* Actually "fire" the object -- Take a partial turn */
	p->upkeep->energy_use = (z_info->move_energy * 10 / shots);

	/* Calculate the path */
	path_n = project_path(cave, path_g, range, grid, target, 0);

	/* Calculate potenital piercing */
	if (p->timed[TMD_POWERSHOT] && tval_is_sharp_missile(obj)) {
		pierce = p->state.ammo_mult;
	}

	/* Hack -- Handle stuff */
	handle_stuff(p);

	/* Project along the path */
	for (i = 0; i < path_n; ++i) {
		struct monster *mon = NULL;
		bool see = square_isseen(cave, path_g[i]);

		/* Stop before hitting walls */
		if (!(square_ispassable(cave, path_g[i])) &&
			!(square_isprojectable(cave, path_g[i])))
			break;

		/* Advance */
		grid = path_g[i];

		/* Tell the UI to display the missile */
		event_signal_missile(EVENT_MISSILE, obj, see, grid.y, grid.x);

		/* Try the attack on the monster at (x, y) if any */
		mon = square_monster(cave, path_g[i]);
		if (mon) {
			int visible = monster_is_obvious(mon);

			bool fear = false;
			const char *note_dies = monster_is_destroyed(mon) ? 
				" is destroyed." : " dies.";

			struct attack_result result = attack(p, obj, grid);
			int dmg = result.dmg;
			uint32_t msg_type = result.msg_type;
			char hit_verb[20];
			my_strcpy(hit_verb, result.hit_verb, sizeof(hit_verb));
			mem_free(result.hit_verb);

			if (result.success) {
				char o_name[80];

				hit_target = true;

				missile_learn_on_ranged_attack(p, obj);

				/* Learn by use for other equipped items */
				equip_learn_on_ranged_attack(p);

				/*
				 * Describe the object (have most up-to-date
				 * knowledge now).
				 */
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_FULL | ODESC_SINGULAR, p);

				/* No negative damage; change verb if no damage done */
				if (dmg <= 0) {
					dmg = 0;
					msg_type = MSG_MISS;
					my_strcpy(hit_verb, "fails to harm", sizeof(hit_verb));
				}

				if (!visible) {
					/* Invisible monster */
					msgt(MSG_SHOOT_HIT, "The %s finds a mark.", o_name);
				} else {
					for (j = 0; j < num_types; j++) {
						char m_name[80];
						const char *dmg_text = "";

						if (msg_type != hit_types[j].msg_type) {
							continue;
						}

						if (OPT(p, show_damage)) {
							dmg_text = format(" (%d)", dmg);
						}

						monster_desc(m_name, sizeof(m_name), mon, MDESC_OBJE);

						if (hit_types[j].text) {
							msgt(msg_type, "Your %s %s %s%s. %s", o_name, 
								 hit_verb, m_name, dmg_text, hit_types[j].text);
						} else {
							msgt(msg_type, "Your %s %s %s%s.", o_name, hit_verb,
								 m_name, dmg_text);
						}
					}

					/* Track this monster */
					if (monster_is_obvious(mon)) {
						monster_race_track(p->upkeep, mon->race);
						health_track(p->upkeep, mon);
					}
				}

				/* Hit the monster, check for death */
				if (!mon_take_hit(mon, p, dmg, &fear, note_dies)) {
					message_pain(mon, dmg);
					if (fear && monster_is_obvious(mon)) {
						add_monster_message(mon, MON_MSG_FLEE_IN_TERROR, true);
					}
				}
			}
			/* Stop the missile, or reduce its piercing effect */
			pierce--;
			if (pierce) continue;
			else break;
		}

		/* Stop if non-projectable but passable */
		if (!(square_isprojectable(cave, path_g[i]))) 
			break;
	}

	/* Get the missile */
	if (object_is_carried(p, obj)) {
		missile = gear_object_for_use(p, obj, 1, true, &none_left);
	} else {
		missile = floor_object_for_use(p, obj, 1, true, &none_left);
	}

	/* Terminate piercing */
	if (p->timed[TMD_POWERSHOT]) {
		player_clear_timed(p, TMD_POWERSHOT, true, false);
	}

	/* Drop (or break) near that location */
	drop_near(cave, &missile, breakage_chance(missile, hit_target), grid, true, false);
}


/**
 * Helper function used with ranged_helper by do_cmd_fire.
 */
struct attack_result make_ranged_shot(struct player *p,
		struct object *ammo, struct loc grid)
{
	char *hit_verb = mem_alloc(20 * sizeof(char));
	struct attack_result result = {false, 0, 0, hit_verb};
	struct object *bow = slot_object(p, slot_by_type(p, EQUIP_BOW, true));
	struct monster *mon = square_monster(cave, grid);
	int b = 0, s = 0;

	my_strcpy(hit_verb, "hits", 20);

	/* Did we hit it */
	if (!test_hit(chance_of_missile_hit(p, ammo, bow, mon), mon->race->ac))
		return result;

	result.success = true;

	improve_attack_modifier(p, ammo, mon, &b, &s, result.hit_verb, true);
	improve_attack_modifier(p, bow, mon, &b, &s, result.hit_verb, true);

	if (!OPT(p, birth_percent_damage)) {
		result.dmg = ranged_damage(p, mon, ammo, bow, b, s);
		result.dmg = critical_shot(p, mon, object_weight_one(ammo),
			object_to_hit(ammo), result.dmg, true,
			&result.msg_type);
	} else {
		result.dmg = o_ranged_damage(p, mon, ammo, bow, b, s, &result.msg_type);
	}

	missile_learn_on_ranged_attack(p, bow);
	learn_brand_slay_from_launch(p, ammo, bow, mon);

	return result;
}


/**
 * Helper function used with ranged_helper by do_cmd_throw.
 */
struct attack_result make_ranged_throw(struct player *p,
	struct object *obj, struct loc grid)
{
	char *hit_verb = mem_alloc(20 * sizeof(char));
	struct attack_result result = {false, 0, 0, hit_verb};
	struct monster *mon = square_monster(cave, grid);
	int b = 0, s = 0;

	my_strcpy(hit_verb, "hits", 20);

	/* If we missed then we're done */
	if (!test_hit(chance_of_missile_hit(p, obj, NULL, mon), mon->race->ac))
		return result;

	result.success = true;

	improve_attack_modifier(p, obj, mon, &b, &s, result.hit_verb, true);

	if (!OPT(p, birth_percent_damage)) {
		result.dmg = ranged_damage(p, mon, obj, NULL, b, s);
		result.dmg = critical_shot(p, mon, object_weight_one(obj),
			object_to_hit(obj), result.dmg, false,
			&result.msg_type);
	} else {
		result.dmg = o_ranged_damage(p, mon, obj, NULL, b, s, &result.msg_type);
	}

	/* Direct adjustment for exploding things (flasks of oil) */
	if (of_has(obj->flags, OF_EXPLODE))
		result.dmg *= 3;

	learn_brand_slay_from_throw(p, obj, mon);

	return result;
}


/**
 * Fire an object from the quiver, pack or floor at a target.
 */
void do_cmd_fire(struct command *cmd) {
	int dir;
	int range = MIN(6 + 2 * player->state.ammo_mult, z_info->max_range);
	int shots = player->state.num_shots;

	ranged_attack attack = make_ranged_shot;

	struct object *bow = slot_object(player, slot_by_type(player, EQUIP_BOW, true));
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Fire which ammunition?",
			/* Error  */ "You have no suitable ammunition to fire.",
			/* Filter */ obj_can_fire,
			/* Choice */ USE_INVEN | USE_QUIVER | USE_FLOOR | QUIVER_TAGS)
		!= CMD_OK)
		return;

	/* Require a usable launcher */
	if (!bow || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Check the item being fired is usable by the player. */
	if (!item_is_available(obj)) {
		msg("That item is not within your reach.");
		return;
	}

	/* Check the ammo can be used with the launcher */
	if (obj->tval != player->state.ammo_tval) {
		msg("That ammo cannot be fired by your current weapon.");
		return;
	}

	if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
		player_confuse_dir(player, &dir, false);
	else
		return;

	ranged_helper(player, obj, dir, range, shots, attack, ranged_hit_types,
				  (int) N_ELEMENTS(ranged_hit_types));
}


/**
 * Throw an object from the quiver, pack, floor, or, in limited circumstances,
 * the equipment.
 */
void do_cmd_throw(struct command *cmd) {
	int dir;
	int shots = 10;
	int str = adj_str_blow(player->state.stat_ind[STAT_STR]);
	ranged_attack attack = make_ranged_throw;

	int weight;
	int range;
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/*
	 * Get arguments.  Never default to showing the equipment as the first
	 * list (since throwing the equipped weapon leaves that slot empty will
	 * have to choose another source anyways).
	 */
	if (player->upkeep->command_wrk == USE_EQUIP)
		player->upkeep->command_wrk = USE_INVEN;
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Throw which item?",
			/* Error  */ "You have nothing to throw.",
			/* Filter */ obj_can_throw,
			/* Choice */ USE_EQUIP | USE_QUIVER | USE_INVEN | USE_FLOOR | SHOW_THROWING)
		!= CMD_OK)
		return;

	if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
		player_confuse_dir(player, &dir, false);
	else
		return;

	if (object_is_equipped(player->body, obj)) {
		assert(obj_can_takeoff(obj) && tval_is_melee_weapon(obj));
		inven_takeoff(obj);
	}

	weight = MAX(object_weight_one(obj), 10);
	range = MIN(((str + 20) * 10) / weight, 10);

	ranged_helper(player, obj, dir, range, shots, attack, ranged_hit_types,
				  (int) N_ELEMENTS(ranged_hit_types));
}

/**
 * Front-end command which fires at the nearest target with default ammo.
 */
void do_cmd_fire_at_nearest(void) {
	int i, dir = DIR_TARGET;
	struct object *ammo = NULL;
	struct object *bow = slot_object(player, slot_by_type(player, EQUIP_BOW, true));

	/* Require a usable launcher */
	if (!bow || !player->state.ammo_tval) {
		msg("You have nothing to fire with.");
		return;
	}

	/* Find first eligible ammo in the quiver */
	for (i = 0; i < z_info->quiver_size; i++) {
		if (!player->upkeep->quiver[i])
			continue;
		if (player->upkeep->quiver[i]->tval != player->state.ammo_tval)
			continue;
		ammo = player->upkeep->quiver[i];
		break;
	}

	/* Require usable ammo */
	if (!ammo) {
		msg("You have no ammunition in the quiver to fire.");
		return;
	}

	/* Require foe */
	if (!target_set_closest((TARGET_KILL | TARGET_QUIET), NULL)) return;

	/* Fire! */
	cmdq_push(CMD_FIRE);
	cmd_set_arg_item(cmdq_peek(), "item", ammo);
	cmd_set_arg_target(cmdq_peek(), "target", dir);
}
