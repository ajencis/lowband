/**
 * \file effects.c
 * \brief Public effect and auxiliary functions for every effect in the game
 *
 * Copyright (c) 2007 Andi Sidwell
 * Copyright (c) 2016 Ben Semmler, Nick McConnell
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

#include "effects.h"
#include "cave.h"
#include "cmd-core.h"
#include "effect-handler.h"
#include "game-input.h"
#include "init.h"
#include "message.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "target.h"
#include "trap.h"
#include "z-type.h"


/**
 * ------------------------------------------------------------------------
 * Properties of effects
 * ------------------------------------------------------------------------ */
/**
 * Useful things about effects.
 */
static const struct effect_kind effects[] =
{
	{ EF_NONE, false, NULL, NULL, NULL, NULL },
	#define F(x) effect_handler_##x
	#define EFFECT(x, a, b, c, d, e, f)	{ EF_##x, a, b, F(x), e, f },
	#include "list-effects.h"
	#undef EFFECT
	#undef F
	{ EF_MAX, false, NULL, NULL, NULL, NULL }
};


static const char *effect_names[] = {
	NULL,
	#define EFFECT(x, a, b, c, d, e, f)	#x,
	#include "list-effects.h"
	#undef EFFECT
};

/*
 * Utility functions
 */

/**
 * Free all the effects in a structure
 *
 * \param source the effects being freed
 */
void free_effect(struct effect *source)
{
	struct effect *e = source, *e_next;
	while (e) {
		e_next = e->next;
		dice_free(e->dice);
		if (e->msg) {
			string_free(e->msg);
		}
		if (e->subtype_temp) {
			string_free(e->subtype_temp);
		}
		mem_free(e);
		e = e_next;
	}
}

bool effect_valid(const struct effect *effect)
{
	if (!effect) return false;
	return effect->index > EF_NONE && effect->index < EF_MAX;
}

bool effect_aim(const struct effect *effect)
{
	const struct effect *e = effect;

	if (!effect_valid(effect)) {
		return false;
	}

	while (e) {
		if (effects[e->index].aim) return true;
		e = e->next;
	}

	return false;
}

const char *effect_info(const struct effect *effect)
{
	if (!effect_valid(effect))
		return NULL;

	return effects[effect->index].info;
}

const char *effect_desc(const struct effect *effect)
{
	if (!effect_valid(effect))
		return NULL;

	return effects[effect->index].desc;
}

effect_index effect_lookup(const char *name)
{
	size_t i;

	for (i = 0; i < N_ELEMENTS(effect_names); i++) {
		const char *effect_name = effect_names[i];

		/* Test for equality */
		if (effect_name != NULL && streq(name, effect_name))
			return i;
	}

	return EF_MAX;
}

/**
 * Translate a string to an effect parameter subtype index
 */
int effect_subtype(int index, const char *type)
{
	char *pe;
	long lv;

	lv = strtol(type, &pe, 10);
	if (pe != type) {
		/*
		 * Got a plain numeric value.  Verify that there isn't garbage
		 * after it and that it doesn't overflow.  Also reject INT_MIN
		 * and INT_MAX so don't have to check errno to detect overflow
		 * when sizeof(long) == sizeof(int).
		 */
		return (contains_only_spaces(pe) && lv < INT_MAX
			&& lv > INT_MIN) ? (int)lv : -1;
	}
	/* If not a numerical value, assign according to effect index */
	switch (index) {
		/* Projection name */
		case EF_PROJECT_LOS:
		case EF_PROJECT_LOS_AWARE:
		case EF_DESTRUCTION:
		case EF_SPOT:
		case EF_SPHERE:
		case EF_BALL:
		case EF_BALL_NO_DAM_RED:
		case EF_BREATH:
		case EF_ARC:
		case EF_SHORT_BEAM:
		case EF_LASH:
		case EF_SWARM:
		case EF_STRIKE:
		case EF_STAR:
		case EF_STAR_BALL:
		case EF_BOLT:
		case EF_BEAM:
		case EF_BOLT_OR_BEAM:
		case EF_LINE:
		case EF_ALTER:
		case EF_BOLT_STATUS:
		case EF_BOLT_STATUS_DAM:
		case EF_BOLT_AWARE:
		case EF_MELEE_BLOWS:
		case EF_TOUCH:
		case EF_TOUCH_AWARE:
			return proj_name_to_idx(type);

		/* Timed effect name */
		case EF_CURE:
		case EF_TIMED_SET:
		case EF_TIMED_INC:
		case EF_TIMED_INC_NO_RES:
		case EF_TIMED_DEC:
		case EF_OTHER_TIMED_INC:
		case EF_SELF_TIMED_INC:
			return timed_name_to_idx(type);

		/* Nourishment types */
		case EF_NOURISH:
			if (streq(type, "INC_BY")) {
				return 0;
			} else if (streq(type, "DEC_BY")) {
				return 1;
			} else if (streq(type, "SET_TO")) {
				return 2;
			} else if (streq(type, "INC_TO")) {
				return 3;
			}
			break;

		/* Monster timed effect name */
		case EF_MON_TIMED_INC:
			return mon_timed_name_to_idx(type);

		/* Summon name */
		case EF_SUMMON:
			return summon_name_to_idx(type);

		/* Stat name */
		case EF_RESTORE_STAT:
		case EF_DRAIN_STAT:
		case EF_LOSE_RANDOM_STAT:
		case EF_GAIN_STAT:
			return stat_name_to_idx(type);

		/* Enchant type name - not worth a separate function */
		case EF_ENCHANT:
			if (streq(type, "TOBOTH")) {
				return ENCH_TOBOTH;
			} else if (streq(type, "TOHIT")) {
				return ENCH_TOHIT;
			} else if (streq(type, "TODAM")) {
				return ENCH_TODAM;
			} else if (streq(type, "TOAC")) {
				return ENCH_TOAC;
			}
			break;

		/* Player shape name */
		case EF_SHAPECHANGE:
			return shape_name_to_idx(type);

		/* Targeted earthquake */
		case EF_EARTHQUAKE:
			if (streq(type, "TARGETED")) {
				return 1;
			} else if (streq(type, "NONE")) {
				return 0;
			}
			break;

		/* Inscribe a glyph */
		case EF_GLYPH:
			if (streq(type, "WARDING")) {
				return GLYPH_WARDING;
			} else if (streq(type, "DECOY")) {
				return GLYPH_DECOY;
			}
			break;

		/* Allow teleport away */
		case EF_TELEPORT:
			if (streq(type, "AWAY")) {
				return 1;
			}
			break;

		/* Allow monster teleport toward */
		case EF_TELEPORT_TO:
			if (streq(type, "SELF")) {
				return 1;
			}
			break;

		// L: terrain feature id
		case EF_TERRAIN_FEAT:
		case EF_FEAT_GROW:
			return lookup_feat_code(type);

		case EF_POLY_SELF:
		case EF_TRANSFORM: {
			struct monster_race *mr = lookup_monster(type);
			if (!mr) return -1;
			return mr->ridx;
		}

		/* Some effects only want a radius, so this is a dummy */
		default:
			if (streq(type, "NONE")) {
				return 0;
			}
			break;
	}

	return -1;
}

static int32_t effect_value_base_spell_power(void)
{
	int power = 0;

	/* L: check if player is casting */
	if (ref_spell) {
		power = gener_spell_power(player, ref_spell);
	}
	/* Check the reference race first */
	else if (ref_race) {
		power = ref_race->spell_power;
	}
	/* Otherwise the current monster if there is one */
	else if (cave->mon_current > 0) {
		power = cave_monster(cave, cave->mon_current)->race->spell_power;
	}

	return power;
}

static int32_t effect_value_base_player_level(void)
{
	return player->lev;
}

static int32_t effect_value_base_dungeon_level(void)
{
	return cave->depth;
}

static int32_t effect_value_base_max_sight(void)
{
	return z_info->max_sight;
}

static int32_t effect_value_base_weapon_damage(void)
{
	struct object *obj = slot_object(&player->mon, slot_by_type(&player->mon, EQUIP_WEAPON, true));
	if (!obj) {
		return 0;
	}
	return (damroll(obj->dd, obj->ds) + obj->to_d);
}

static int32_t effect_value_base_player_hp(void)
{
	return player->mon.hp;
}

static int32_t effect_value_base_monster_percent_hp_gone(void)
{
	/* Get the targeted monster, fail horribly if none */
	struct monster *mon = target_get_monster();

	return mon ? (((mon->maxhp - mon->hp) * 100) / mon->maxhp) : 0;
}

static int32_t effect_value_base_caster_hp(void)
{
	int power = 0;

	// Use the current monster if there is one
	if (cave->mon_current > 0) {
		power = cave_monster(cave, cave->mon_current)->hp;
	}
	// Else assume the player is casting
	else {
		power = player->mon.hp;
	}

	return power;
}

static int32_t effect_value_base_feat_size(void)
{
	if (ref_feat) {
		return ref_feat->size;
	}
	return 0;
}

static int32_t effect_value_base_monster_level(void)
{
	const struct monster_race *monr = NULL;

	if (cave && cave->mon_current > 0) {
		struct monster *mon = cave_monster(cave, cave->mon_current);
		if (mon && mon->race) {
			monr = mon->race;
		}
	}

	if (!monr && ref_race) {
		monr = ref_race;
	}

	return monr ? monr->level : 0;
}

expression_base_value_f effect_value_base_by_name(const char *name)
{
	static const struct value_base_s {
		const char *name;
		expression_base_value_f function;
	} value_bases[] = {
		{ "SPELL_POWER", effect_value_base_spell_power },
		{ "PLAYER_LEVEL", effect_value_base_player_level },
		{ "DUNGEON_LEVEL", effect_value_base_dungeon_level },
		{ "MAX_SIGHT", effect_value_base_max_sight },
		{ "WEAPON_DAMAGE", effect_value_base_weapon_damage },
		{ "PLAYER_HP", effect_value_base_player_hp },
		{ "MONSTER_PERCENT_HP_GONE",
		  effect_value_base_monster_percent_hp_gone },
		{ "CASTER_HP", effect_value_base_caster_hp },
		{ "FEAT_SIZE", effect_value_base_feat_size },
		{ "MONSTER_LEVEL", effect_value_base_monster_level },
		{ NULL, NULL },
	};
	const struct value_base_s *current = value_bases;

	while (current->name != NULL && current->function != NULL) {
		if (my_stricmp(name, current->name) == 0)
			return current->function;

		current++;
	}

	return NULL;
}

/**
 * ------------------------------------------------------------------------
 * Execution of effects
 * ------------------------------------------------------------------------ */
/**
 * Execute an effect chain.
 *
 * \param effect is the effect chain
 * \param origin is the origin of the effect (player, monster etc.)
 * \param obj    is the object making the effect happen (or NULL)
 * \param ident  will be updated if the effect is identifiable
 *               (NB: no effect ever sets *ident to false)
 * \param aware  indicates whether the player is aware of the effect already
 * \param dir    is the direction the effect will go in
 * \param beam   is the base chance out of 100 that a BOLT_OR_BEAM effect will beam
 * \param boost  is the extent to which skill surpasses difficulty, used as % boost. It
 *               ranges from 0 to 138.
 * \param cmd    If the effect is invoked as part of a command, this is the
 *               the command structure - used primarily so repeating the
 *               command can use the same information without prompting the
 *               player again.  Use NULL for this if not invoked as part of
 *               a command.
 */
bool effect_do(struct effect *effect,
		struct source origin,
		struct source target,
		struct object *obj,
		bool *ident,
		bool aware,
		int dir,
		int beam,
		int boost,
		struct command *cmd)
{
	struct loc targ_grid = { 0, 0 };
	bool completed = false;
	effect_handler_f handler;
	random_value value = { 0, 0, 0, 0 };

	if (target.what == SRC_MONSTER) {
		struct monster *mon = cave_monster(cave, target.which.monster);
		assert(mon && mon->race);
		targ_grid = mon->grid;
	}

	do {
		int choice_count = 0, leftover = 1;

		if (!effect_valid(effect)) {
			msg("Bad effect passed to effect_do(). Please report this bug.");
			return false;
		}

		if (effect->dice != NULL) {
			choice_count = dice_roll(effect->dice, &value);
		}

		/* Deal with special random and select effects */
		if (effect->index == EF_RANDOM || effect->index == EF_SELECT) {
			int choice;

			/*
			 * If it has no subeffects, act as if it completed
			 * successfully and go to the next effect.
			 */
			if (choice_count <= 0) {
				completed = true;
				effect = effect->next;
				continue;
			}

			/*
			 * Treat select effects like random ones if they
			 * aren't from a player or if there's really no choice
			 * to be made.
			 */
			if (effect->index == EF_RANDOM ||
					origin.what != SRC_PLAYER ||
					choice_count < 2) {
				choice = randint0(choice_count);
			} else {
				assert(effect->index == EF_SELECT &&
					origin.what == SRC_PLAYER);
				/*
				 * Since a choice is presented, allow
				 * identification, even if no choice is made.
				 */
				if (ident) {
					*ident = true;
				}
				if (cmd) {
					if (cmd_get_effect_from_list(cmd,
							"list_index",
							&choice, NULL,
							effect->next,
							choice_count,
							true) != CMD_OK) {
						return false;
					}
				} else {
					choice = get_effect_from_list(NULL,
						effect->next, choice_count,
						true);
					if (choice == -1) return false;
				}

				/*
				 * If the player chose to use a random effect,
				 * roll for it.
				 */
				if (choice == -2) {
					choice = randint0(choice_count);
				}
				assert(choice >= 0 && choice < choice_count);
			}

			leftover = choice_count - choice;

			/* Skip to the chosen effect */
			effect = effect->next;
			while (choice-- && effect)
				effect = effect->next;
			if (!effect) {
				/*
				 * There's fewer subeffects than expected.  Act
				 * as if it ran successfully.
				 */
				completed = true;
				break;
			}

			/* Roll the damage, if needed */
			if (effect->dice != NULL) {
				(void) dice_roll(effect->dice, &value);
			}
		}

		/* Handle the effect */
		handler = effects[effect->index].handler;
		if (handler != NULL) {
			effect_handler_context_t context = {
				effect->index,
				origin,
				obj,
				aware,
				dir,
				beam,
				boost,
				value,
				effect->subtype,
				effect->radius,
				effect->other,
				effect->y,
				effect->x,
				effect->msg,
				ident ? *ident : true,
				cmd
			};

			completed = handler(&context) || completed;
			if (ident) {
				*ident = context.ident;
			}
		}

		if (target.what == SRC_MONSTER) {
			struct monster *tgt = cave_monster(cave, target.which.monster);

			if (!tgt || !tgt->race || !loc_eq(tgt->grid, targ_grid)) {
				break;
			}
		}

		/* Get the next effect, if there is one */
		while (leftover-- && effect) {
			effect = effect->next;
		}
	} while (effect);

	return completed;
}

/**
 * Perform a single effect with a simple dice string and parameters
 * Calling with ident a valid pointer will (depending on effect) give success
 * information; ident = NULL will ignore this
 */
void effect_simple(int index,
				   struct source origin,
				   const char *dice_string,
				   int subtype,
				   int radius,
				   int other,
				   int y,
				   int x,
				   bool *ident)
{
	struct effect effect;
	int dir = DIR_TARGET;
	bool dummy_ident = false;

	/* Set all the values */
	memset(&effect, 0, sizeof(effect));
	effect.index = index;
	effect.dice = dice_new();
	dice_parse_string(effect.dice, dice_string);
	effect.subtype = subtype;
	effect.radius = radius;
	effect.other = other;
	effect.y = y;
	effect.x = x;

	/* Direction if needed */
	if (effect_aim(&effect)) {
		get_aim_dir(&dir);
	}

	/* Do the effect */
	if (!ident) {
		ident = &dummy_ident;
	}

	effect_do(&effect, origin, source_none(), NULL, ident, true, dir, 0, 0, NULL);
	dice_free(effect.dice);
}

/**
 * Returns N which is the 1 in N chance for recharging to fail.
 */
int recharge_failure_chance(const struct object *obj, int strength) {
	/* Ease of recharge ranges from 9 down to 4 (wands) or 3 (staffs) */
	int ease_of_recharge = (100 - obj->kind->level) / 10;
	int raw_chance = strength + ease_of_recharge
		- 2 * (obj->pval / obj->number);
	return raw_chance > 1 ? raw_chance : 1;
}

/**
 * L: returns a smite-targeted monster; if there is a monster already selected
 * it will return that if it still exists or NULL otherwise
 */
struct monster *smite_target_get(int dir, struct command *cmd)
{
	struct monster *mon = NULL;
	int midx;

	if (cmd && !cmd_get_arg_number(cmd, "midx", &midx)) {
		mon = cave_monster(cave, midx);
		mon = mon && mon->race ? mon : NULL;
	}
	else if (dir == DIR_TARGET) {
		if (target_okay()) {
			mon = target_get_monster();
		}
	}
	else if (dir != DIR_UNKNOWN) {
		int i, range = z_info->max_sight;
		struct loc direction = loc_sum(player->mon.grid, loc(range * ddx[dir], range * ddy[dir]));
		int path_n;
		struct loc path_g[256], target;

		path_n = project_path(cave, path_g, range, player->mon.grid, direction, 0);

		for (i = 0; i < path_n && !mon; i++) {
			target = path_g[i];
			mon = square_monster(cave, target);
			if (mon) {
				break;
			}
		}
	}

	if (!target_able(mon)) {
		return NULL;
	}

	if (cmd) {
		cmd_set_arg_number(cmd, "target", mon->midx);
	}

	return mon;
}

