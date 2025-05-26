/**
 * \file player-spell.c
 * \brief Spell and prayer casting/praying
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
#include "cmd-core.h"
#include "effects.h"
#include "init.h"
#include "monster.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-calcs.h"
#include "player-properties.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "player.h"
#include "project.h"
#include "target.h"

#define NO_FAIL_LEVEL 25 // caster level for which spells get no fail
#define NO_MANA_LEVEL 60 // caster level for which spells are free

const struct player_spell *ref_spell = NULL;

/**
 * Used by get_spell_info() to pass information as it iterates through effects.
 */
struct spell_info_iteration_state {
	const struct effect *pre;
	char pre_special[40];
	random_value pre_rv;
	random_value shared_rv;
	bool have_shared;
};

/**
 * Stat Table (INT/WIS) -- Minimum failure rate (percentage)
 */
static const int adj_mag_fail[STAT_RANGE] =
{
	99	/* 3 */,
	99	/* 4 */,
	99	/* 5 */,
	99	/* 6 */,
	99	/* 7 */,
	50	/* 8 */,
	30	/* 9 */,
	20	/* 10 */,
	15	/* 11 */,
	12	/* 12 */,
	11	/* 13 */,
	10	/* 14 */,
	9	/* 15 */,
	8	/* 16 */,
	7	/* 17 */,
	6	/* 18/00-18/09 */,
	6	/* 18/10-18/19 */,
	5	/* 18/20-18/29 */,
	5	/* 18/30-18/39 */,
	5	/* 18/40-18/49 */,
	4	/* 18/50-18/59 */,
	4	/* 18/60-18/69 */,
	4	/* 18/70-18/79 */,
	4	/* 18/80-18/89 */,
	3	/* 18/90-18/99 */,
	3	/* 18/100-18/109 */,
	2	/* 18/110-18/119 */,
	2	/* 18/120-18/129 */,
	2	/* 18/130-18/139 */,
	2	/* 18/140-18/149 */,
	1	/* 18/150-18/159 */,
	1	/* 18/160-18/169 */,
	1	/* 18/170-18/179 */,
	1	/* 18/180-18/189 */,
	1	/* 18/190-18/199 */,
	0	/* 18/200-18/209 */,
	0	/* 18/210-18/219 */,
	0	/* 18/220+ */
};

#if 0
/**
 * Stat Table (INT/WIS) -- failure rate adjustment
 */
static const int adj_mag_stat[STAT_RANGE] =
{
	-5	/* 3 */,
	-4	/* 4 */,
	-3	/* 5 */,
	-3	/* 6 */,
	-2	/* 7 */,
	-1	/* 8 */,
	 0	/* 9 */,
	 0	/* 10 */,
	 0	/* 11 */,
	 0	/* 12 */,
	 0	/* 13 */,
	 1	/* 14 */,
	 2	/* 15 */,
	 3	/* 16 */,
	 4	/* 17 */,
	 5	/* 18/00-18/09 */,
	 6	/* 18/10-18/19 */,
	 7	/* 18/20-18/29 */,
	 8	/* 18/30-18/39 */,
	 9	/* 18/40-18/49 */,
	10	/* 18/50-18/59 */,
	11	/* 18/60-18/69 */,
	12	/* 18/70-18/79 */,
	15	/* 18/80-18/89 */,
	18	/* 18/90-18/99 */,
	21	/* 18/100-18/109 */,
	24	/* 18/110-18/119 */,
	27	/* 18/120-18/129 */,
	30	/* 18/130-18/139 */,
	33	/* 18/140-18/149 */,
	36	/* 18/150-18/159 */,
	39	/* 18/160-18/169 */,
	42	/* 18/170-18/179 */,
	45	/* 18/180-18/189 */,
	48	/* 18/190-18/199 */,
	51	/* 18/200-18/209 */,
	54	/* 18/210-18/219 */,
	57	/* 18/220+ */
};
#endif

/**
 * Initialise player spells
 */
void player_spells_init(struct player *p)
{
	p->player_spell_flags = mem_zalloc(z_info->spell_max * sizeof *p->player_spell_flags);
	p->player_spell_order = mem_zalloc(z_info->spell_max * sizeof *p->player_spell_order);
}

/**
 * Free player spells
 */
void player_spells_free(struct player *p)
{
	mem_free(p->player_spell_flags);
	mem_free(p->player_spell_order);
}

/**
 * Make a list of the spell realms the player's class has books from
 */
struct magic_realm *class_magic_realms(const struct player_class *c, int *count)
{
	int i;
	struct magic_realm *r = mem_zalloc(sizeof(struct magic_realm));

	*count = 0;

	if (!c->magic.total_spells) {
		mem_free(r);
		return NULL;
	}

	for (i = 0; i < c->magic.num_books; i++) {
		struct magic_realm *r_test = r;
		struct class_book *book = &c->magic.books[i];
		bool found = false;

		/* Test for first realm */
		if (r->name == NULL) {
			memcpy(r, book->realm, sizeof(struct magic_realm));
			r->next = NULL;
			(*count)++;
			continue;
		}

		/* Test for already recorded */
		while (r_test) {
			if (streq(r_test->name, book->realm->name)) {
				found = true;
			}
			r_test = r_test->next;
		}
		if (found) continue;

		/* Add it */
		r_test = mem_zalloc(sizeof(struct magic_realm));
		memcpy(r_test, book->realm, sizeof(struct magic_realm));
		r_test->next = r;
		r = r_test;
		(*count)++;
	}

	return r;
}


/**
 * Get the spellbook structure from any object which is a book
 */
const struct class_book *object_kind_to_book(const struct object_kind *kind)
{
	struct player_class *class = classes;
	while (class) {
		int i;

		for (i = 0; i < class->magic.num_books; i++)
		if ((kind->tval == class->magic.books[i].tval) &&
				(kind->sval == class->magic.books[i].sval)) {
			return &class->magic.books[i];
		}
		class = class->next;
	}

	return NULL;
}

/**
 * Get the spellbook structure from an object which is a book the player can
 * cast from
 */
const struct class_book *player_object_to_book(const struct player *p,
		const struct object *obj)
{
	int i;

	for (i = 0; i < p->class->magic.num_books; i++)
		if ((obj->tval == p->class->magic.books[i].tval) &&
				(obj->sval == p->class->magic.books[i].sval))
			return &p->class->magic.books[i];

	return NULL;
}

const struct class_spell *spell_by_index(const struct player *p, int index)
{
	int book = 0, count = 0;

	const struct class_magic *magic = &p->class->magic;

	/* Check index validity */
	if (index < 0 || index >= magic->total_spells)
		return NULL;

	/* Find the book, count the spells in previous books */
	while (count + magic->books[book].num_spells - 1 < index)
		count += magic->books[book++].num_spells;

	/* Find the spell */
	return &magic->books[book].spells[index - count];
}

/**
 * Collect spells from a book into the spells[] array, allocating
 * appropriate memory.
 */
int spell_collect_from_book(const struct player *p, const struct object *obj,
		int **splls)
{
	const struct class_book *book = player_object_to_book(p, obj);
	int i, n_spells = 0;

	if (!book) {
		return n_spells;
	}

	/* Count the spells */
	for (i = 0; i < book->num_spells; i++)
		n_spells++;

	/* Allocate the array */
	*splls = mem_zalloc(n_spells * sizeof(*splls));

	/* Write the spells */
	for (i = 0; i < book->num_spells; i++)
		(*splls)[i] = book->spells[i].sidx;

	return n_spells;
}


/**
 * Return the number of castable spells in the spellbook 'obj'.
 */
int spell_book_count_spells(const struct player *p, const struct object *obj,
		bool (*tester)(const struct player *p, int spell))
{
	const struct class_book *book = player_object_to_book(p, obj);
	int i, n_spells = 0;

	if (!book) {
		return n_spells;
	}

	for (i = 0; i < book->num_spells; i++)
		if (tester(p, book->spells[i].sidx))
			n_spells++;

	return n_spells;
}


/**
 * L: get effective caster level for a given spell
 */
int caster_level_bonus(const struct player *p, const struct class_spell *spell)
{
	int bonus = 0;
	struct monster_race *mon = lookup_player_monster(p);

	if (p->state.powers[spell->school] > 0) {
		bonus += (p->state.powers[spell->school] * 50 + 49) / 50;
	}

	if (mon) {
		int monspells = 0;
		int i;
		for (i = 0; i < RSF_MAX; i++) {
			if (mon_spell_is_innate(i)) continue;
			if (!rsf_has(mon->spell_flags, i)) continue;
			++monspells;
		}
		bonus += monspells;
	}

	return bonus;
}


/**
 * True if at least one spell in spells[] is OK according to spell_test.
 */
bool spell_okay_list(const struct player *p,
		bool (*spell_test)(const struct player *p, int spell),
		const int splls[], int n_spells)
{
	int i;
	bool okay = false;

	for (i = 0; i < n_spells; i++)
		if (spell_test(p, splls[i]))
			okay = true;

	return okay;
}

/**
 * True if the spell is castable.
 */
bool spell_okay_to_cast(const struct player *p, int spell)
{
	return (p->spell_flags[spell] & PY_SPELL_LEARNED);
}

/**
 * True if the spell can be studied.
 */
bool spell_okay_to_study(const struct player *p, int spell_index)
{
	const struct class_spell *spell = spell_by_index(p, spell_index);
	return spell && spell->slevel <= (caster_level_bonus(p, spell) + p->lev)
		&& !(p->spell_flags[spell_index] & PY_SPELL_LEARNED);
}

/**
 * True if the spell is browsable.
 */
bool spell_okay_to_browse(const struct player *p, int spell_index)
{
	const struct class_spell *spell = spell_by_index(p, spell_index);
	return spell && spell->slevel < 99;
}

/**
 * Spell failure adjustment by casting stat level
 */
static int fail_adjust(struct player *p, const struct class_spell *spell)
{
	int stat = spell->realm->stat;
	return adj_mag_stat(p->state.stat_ind[stat]);
}

/**
 * Spell minimum failure by casting stat level
 */
static int min_fail(struct player *p, const struct class_spell *spell)
{
	int stat = spell->realm->stat;
	return adj_mag_fail[p->state.stat_ind[stat]];
}

/**
 * Returns chance of failure for a spell
 */
int16_t spell_chance(int spell_index)
{
	int chance = 100, minfail;

	int curr_unlight = player->state.powers[PP_UNLIGHT] ?
			unlight_power(player) - get_power_scale(player, PP_UNLIGHT, 5) :
			0;

	const struct class_spell *spell;

	/* Paranoia -- must be literate */
	if (!player->class->magic.total_spells) return chance;

	/* Get the spell */
	spell = spell_by_index(player, spell_index);
	if (!spell) return chance;

	/* Extract the base spell failure rate */
	chance = spell->sfail;

	/* Reduce failure rate by "effective" level adjustment */
	chance -= 3 * (caster_level_bonus(player, spell) + player->lev - spell->slevel);

	/* Reduce failure rate by casting stat level adjustment */
	chance -= fail_adjust(player, spell);

	/* Not enough mana to cast */
	if (spell->smana > player->csp) {
		chance += 5 * (spell->smana - player->csp);
	}

	/* Get the minimum failure rate for the casting stat level */
	minfail = min_fail(player, spell);

	/* Non zero-fail characters never get better than 5 percent */
	if (!player_has(player, PF_ZERO_FAIL) && minfail < 5) {
		minfail = 5;
	}

	/* Necromancers are punished by being on lit squares */
	chance -= curr_unlight;

	/* Fear makes spells harder (before minfail) */
	/* Note that spells that remove fear have a much lower fail rate than
	 * surrounding spells, to make sure this doesn't cause mega fail */
	if (player_of_has(player, OF_AFRAID)) chance += 20;

	/* Minimal and maximal failure rate */
	if (chance < minfail) chance = minfail;
	if (chance > 50) chance = 50;

	/* Stunning makes spells harder (after minfail) */
	if (player->mon.m_timed[TMD_STUN] > 50) {
		chance += 25;
	} else if (player->mon.m_timed[TMD_STUN]) {
		chance += 15;
	}

	/* Amnesia makes spells very difficult */
	if (player->mon.m_timed[TMD_AMNESIA]) {
		chance = 50 + chance / 2;
	}

	/* Always a 5 percent chance of working */
	if (chance > 95) {
		chance = 95;
	}

	/* Return the chance */
	return (chance);
}


/**
 * Learn the specified spell.
 */
void spell_learn(int spell_index)
{
	int i;
	const struct class_spell *spell = spell_by_index(player, spell_index);
	int maxi = player->class->magic.total_spells;

	/* Learn the spell */
	player->spell_flags[spell_index] |= PY_SPELL_LEARNED;

	/* Find the next open entry in "spell_order[]" */
	for (i = 0; i < maxi; i++) {
		if (player->spell_order[i] == 99) break;
	}

	/* Add the spell to the known list */
	player->spell_order[i] = spell_index;

	/* Mention the result */
	msgt(MSG_STUDY, "You have learned the %s of %s.", spell->realm->spell_noun,
		 spell->name);

	/* One less spell available */
	player->upkeep->new_spells--;

	/* Message if needed */
	if (player->upkeep->new_spells) {
		msg("You can learn %d more %s%s.", player->upkeep->new_spells,
			spell->realm->spell_noun, PLURAL(player->upkeep->new_spells));
	}

	/* Redraw Study Status */
	player->upkeep->redraw |= (PR_STUDY | PR_OBJECT);
}

static int beam_chance(void)
{
	int plev = player->lev;
	return (player_has(player, PF_BEAM) ? plev : (plev / 2));
}

/**
 * Cast the specified spell
 */
bool spell_cast(int spell_index, int dir, struct command *cmd)
{
	int chance;
	bool ident = false;
	int beam  = beam_chance();

	/* Get the spell */
	const struct class_spell *spell = spell_by_index(player, spell_index);

	/* Spell failure chance */
	chance = spell_chance(spell_index);

	/* Fail or succeed */
	if (randint0(100) < chance) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to concentrate hard enough!");
	} else {
		/* Cast the spell */
		if (!effect_do(spell->effect, source_player(), NULL, &ident, true, dir,
					   beam, caster_level_bonus(player, spell), cmd)) {
			return false;
		}

		/* Reward COMBAT_REGEN with small HP recovery */
		if (player_has(player, PF_COMBAT_REGEN)) {
			convert_mana_to_hp(player, spell->smana << 16);
		}

		/* A spell was cast */
		sound(MSG_SPELL);

		if (!(player->spell_flags[spell_index] & PY_SPELL_WORKED)) {
			int e = spell->sexp;

			/* The spell worked */
			player->spell_flags[spell_index] |= PY_SPELL_WORKED;

			/* Gain experience */
			player_exp_gain(player, e * spell->slevel, 0);

			/* Redraw object recall */
			player->upkeep->redraw |= (PR_OBJECT);
		}
	}

	/* Sufficient mana? */
	if (spell->smana <= player->csp) {
		/* Use some mana */
		player->csp -= spell->smana;

		if (one_in_(10)) take_max_sp_dam(player, spell->smana);
	} else {
		int oops = spell->smana - player->csp;

		take_max_sp_dam(player, one_in_(10) ? spell->smana : oops);

		/* No mana left */
		player->csp = 0;
		player->csp_frac = 0;

		/* Over-exert the player */
		player_over_exert(player, PY_EXERT_FAINT, 100, 5 * oops + 1);
		player_over_exert(player, PY_EXERT_CON, 50, 0);
	}

	/* Redraw mana */
	player->upkeep->redraw |= (PR_MANA);

	return true;
}


bool gener_spell_cast(int spell_index, int dir, struct command *cmd)
{
	bool ident = false;
	int beam  = beam_chance();

	/* Get the spell */
	const struct player_spell *spell = player_spell_lookup(spell_index);
	assert(spell);
	const struct magic_realm *realm = get_player_realm(player);
	int mana = player_spell_mana(spell);
	int availmana = available_mana(cave, player->grid);
	int chance = player_spell_fail(spell);

	assert(realm);

	/* L: save the spell for spellpower calc purposes */
	ref_spell = spell;

	/* Fail or succeed */
	if (randint0(100) < chance) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to concentrate hard enough!");
	} else {
		/* Cast the spell */
		if (!effect_do(spell->effect, source_player(), NULL, &ident, true, dir,
					beam, gener_spell_power(player, spell), cmd)) {
			ref_spell = NULL;
			return false;
		}

		/* Reward COMBAT_REGEN with small HP recovery */
		if (player_has(player, PF_COMBAT_REGEN)) {
			convert_mana_to_hp(player, mana << 16);
		}

		if (!(player->player_spell_flags[spell_index] & PY_SPELL_WORKED)) {
			int i;
			bool found_order = false;
			for (i = 0; i < z_info->spell_max && !found_order; i++) {
				if (player->player_spell_order[i] == 99) {
					player->player_spell_order[i] = spell->sidx;
					found_order = true;
				}
			}
			player->player_spell_flags[spell_index] |= PY_SPELL_WORKED;
		}

		/* A spell was cast */
		sound(MSG_SPELL);
	}

	ref_spell = NULL;

	// make a sound if we're a bard
	if (realm->realm_special[RLM_SPCL_LOUD]) {
		player->curr_noise = MAX(player->curr_noise, spell->slevel / 3 + 1);
	}

	/* Sufficient mana? */
	if (realm && realm->realm_special[RLM_SPCL_HP_CAST]) {
		// Use hp
		take_hit(player, mana, "the strain of casting a spell");
	} else if (mana <= availmana) {
		/* Use some mana */
		cave->squares[player->grid.y][player->grid.x].mana -= mana;
		square_average_mana(cave, player->grid);
	} else {
		int oops = mana - availmana;
		cave->squares[player->grid.y][player->grid.x].mana -= availmana;
		square_average_mana(cave, player->grid);

		/* Over-exert the player */
		player_over_exert(player, PY_EXERT_FAINT, 100, 5 * oops + 1);
		player_over_exert(player, PY_EXERT_CON, 50, 0);
	}

	/* Redraw mana */
	player->upkeep->redraw |= (PR_MANA);

	return true;
}



bool spell_needs_aim(int spell_index)
{
	const struct class_spell *spell = spell_by_index(player, spell_index);
	assert(spell);
	return effect_aim(spell->effect);
}

bool innate_needs_aim(int innate_index)
{
	const struct monster_spell *ms = monster_spell_by_index(innate_index);
	assert(ms);
	return effect_aim(ms->effect);
}

bool gener_spell_needs_aim(const struct player_spell *spell)
{
	assert(spell);
	return effect_aim(spell->effect);
}

static size_t append_random_value_string(char *buffer, size_t size,
										 random_value *rv)
{
	size_t offset = 0;

	if (rv->base > 0) {
		offset += strnfmt(buffer + offset, size - offset, "%d", rv->base);

		if (rv->dice > 0 && rv->sides > 0) {
			offset += strnfmt(buffer + offset, size - offset, "+");
		}
	}

	if (rv->dice == 1 && rv->sides > 0) {
		offset += strnfmt(buffer + offset, size - offset, "d%d", rv->sides);
	} else if (rv->dice > 1 && rv->sides > 0) {
		offset += strnfmt(buffer + offset, size - offset, "%dd%d", rv->dice,
						  rv->sides);
	}

	return offset;
}

static void spell_effect_append_value_info(const struct effect *effect,
		char *p, size_t len, struct spell_info_iteration_state *ist)
{
	random_value rv = { 0, 0, 0, 0 };
	const char *type = NULL;
	char special[40] = "";
	size_t offset = strlen(p);

	if (effect->index == EF_CLEAR_VALUE) {
		ist->have_shared = false;
	} else if (effect->index == EF_SET_VALUE && effect->dice) {
		ist->have_shared = true;
		dice_roll(effect->dice, &ist->shared_rv);
	}

	type = effect_info(effect);
	if (type == NULL) return;

	if (effect->dice != NULL) {
		dice_roll(effect->dice, &rv);
	} else if (ist->have_shared) {
		rv = ist->shared_rv;
	}

	/* Handle some special cases where we want to append some additional info */
	switch (effect->index) {
		case EF_HEAL_HP:
			/* Append percentage only, as the fixed value is always displayed */
			if (rv.m_bonus) {
				strnfmt(special, sizeof(special), "/%d%%",
					rv.m_bonus);
			}
			break;
		case EF_TELEPORT:
			/* m_bonus means it's a weird random thing */
			if (rv.m_bonus) {
				my_strcpy(special, "random", sizeof(special));
			}
			break;
		case EF_SPHERE:
			/* Append radius */
			if (effect->radius) {
				int rad = effect->radius;
				strnfmt(special, sizeof(special), ", rad %d",
					rad);
			} else {
				my_strcpy(special, ", rad 2", sizeof(special));
			}
			break;
		case EF_BALL:
		case EF_BALL_NO_DAM_RED:
			/* Append radius */
			if (effect->radius) {
				int rad = effect->radius;
				if (effect->other) {
					rad += player->lev / effect->other;
				}
				strnfmt(special, sizeof(special), ", rad %d",
					rad);
			} else {
				my_strcpy(special, "rad 2", sizeof(special));
			}
			break;
		case EF_STRIKE:
			/* Append radius */
			if (effect->radius) {
				strnfmt(special, sizeof(special), ", rad %d",
					effect->radius);
			}
			break;
		case EF_SHORT_BEAM: {
			/* Append length of beam */
			int beam_len = effect->radius;
			if (effect->other) {
				beam_len += player->lev / effect->other;
				beam_len = MIN(beam_len, z_info->max_range);
			}
			strnfmt(special, sizeof(special), ", len %d", beam_len);
			break;
		}
		case EF_SWARM:
			/* Append number of projectiles. */
			strnfmt(special, sizeof(special), "x%d", rv.m_bonus);
			break;
	}

	/*
	 * Only display if have dice and it isn't redundant with the
	 * previous one that was displayed.
	 */
	if ((rv.base > 0 || (rv.dice > 0 && rv.sides > 0))
			&& (!ist->pre
			|| ist->pre->index != effect->index
			|| !streq(special, ist->pre_special)
			|| ist->pre_rv.base != rv.base
			|| (((ist->pre_rv.dice > 0 && ist->pre_rv.sides > 0)
			|| (rv.dice > 0 && rv.sides > 0))
			&& (ist->pre_rv.dice != rv.dice
			|| ist->pre_rv.sides != rv.sides)))) {
		if (offset) {
			offset += strnfmt(p + offset, len - offset, ";");
		}

		offset += strnfmt(p + offset, len - offset, " %s ", type);
		offset += append_random_value_string(p + offset, len - offset, &rv);

		if (strlen(special) > 1) {
			strnfmt(p + offset, len - offset, "%s", special);
		}

		ist->pre = effect;
		my_strcpy(ist->pre_special, special, sizeof(ist->pre_special));
		ist->pre_rv = rv;
	}
}

void get_spell_info(int spell_index, char *p, size_t len)
{
	struct effect *effect = spell_by_index(player, spell_index)->effect;
	struct spell_info_iteration_state ist = {
		NULL, "", { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, false };

	p[0] = '\0';

	while (effect) {
		spell_effect_append_value_info(effect, p, len, &ist);
		effect = effect->next;
	}
}

/**
 * L: functions for magic schools
 */
const char *school_idx_to_name(int idx)
{
	assert(idx > MS_NONE && idx < MS_MAX);

	struct player_ability *abil = lookup_player_ability(idx, PY_ABIL_POWER);

	return abil->name;
}

int innate_spell_mana(const struct monster_race *mon)
{
	int freq = MAX(0, 50 - mon->freq_innate);
	freq = MAX(freq, 0);
	int cost = mon->level * freq / 150;

	return cost;
}

int innate_spell_power(struct player *p, int spell)
{
	struct monster_race *mr = lookup_player_monster(p);
	const struct monster_spell *ms = monster_spell_by_index(spell);
	int base = mr ? mr->spell_power : p->lev;
	int powerind = skill_by_effect(ms->effect->index, ms->effect->subtype);
	int powerlevel = powerind > PP_NONE ? 0 : p->state.powers[powerind];
	int powerbonus = MIN(powerlevel, base);

	return base + powerbonus;
}

void get_innate_info(int innate_index, char *p, size_t len)
{
	struct effect *effect = monster_spell_by_index(innate_index)->effect;
	struct spell_info_iteration_state ist = {
		NULL, "", { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, false };

	p[0] = '\0';

	while (effect) {
		spell_effect_append_value_info(effect, p, len, &ist);
		effect = effect->next;
	}
}


static bool spell_is_continuous(const struct player_spell *s)
{
	const struct effect *ef;

	for (ef = s->effect; ef; ef = ef->next) {
		switch (ef->index) {
			case EF_COMMAND:
			case EF_MON_TIMED_INC:
			case EF_SHAPECHANGE:
			case EF_SUMMON:
			case EF_TIMED_INC:
			case EF_TIMED_INC_NO_RES:
			case EF_TIMED_SET:
			case EF_TRANSFORM:
				return true;
		}
	}

	return false;
}

static int realm_school_modifier(const struct player *p, const struct magic_realm *r, int school)
{
	int base = r->school_modifiers[school];
	struct monster_race *mon;

	if (!r->realm_special[RLM_SPCL_MON_APT]) return base;

	mon = lookup_player_monster(p);

	if (mon) {
		int power = mon_power(mon, school);

		base += power;
	}

	return base - 5;
}

int gener_spell_power(const struct player *p, const struct player_spell *s)
{
	int numschools = 0, sumschools = 0;
	int schoolbonus = 0, realmbonus = 0;
	int skill = p->state.skills[SKILL_MAGIC];
	int antim = get_power_scale(p, PP_ANTIMAGIC, 25);
	int power = get_power_scale(p, PP_SPELL_POWER, 50);
	int ease = get_power_scale(p, PP_SPELL_EASE, 25);
	int i;
	int result, stepdown;
	const struct magic_realm *r = get_player_realm(p);
	bool is_continuous = spell_is_continuous(s);
	int level = s->slevel;

	if (!r) return -s->slevel;

	for (i = 0; i < MAX_SPELL_SCHOOLS; i++) {
		if (s->school[i] > MS_NONE) {
			++numschools;
			sumschools += p->state.powers[s->school[i]];
			if (r) {
				realmbonus += realm_school_modifier(p, r, s->school[i]);
			}
		}
	}

	if (numschools > 0) {
		schoolbonus = 3 * sumschools / (2 + numschools);
	}

	schoolbonus = MIN(schoolbonus, skill * 2);

	if (is_continuous) {
		level -= level * r->realm_special[RLM_SPCL_CONTINUOUS] * level / 100;
	}
	else {
		level -= level * r->realm_special[RLM_SPCL_INSTANT] * level / 100;
	}

	result = skill + schoolbonus + realmbonus - level - antim + 1;

	if (result > 10 && power > 0) {
		result = (result - 10) * (100 + power) / 100 + 10;
	}
	if (result < 10 && ease > 0) {
		result = MAX(result, MIN(result / 2, result) + ease - 10);
	}

	for (stepdown = 20; result > stepdown; stepdown += 10) {
		result = (result - stepdown) / 2 + stepdown;
	}

	return result;
}

void gener_spell_learn(struct player *p, const struct player_spell *s, bool verbose)
{
	//int i;
	//bool found_order;

	p->player_spell_flags[s->sidx] |= PY_SPELL_LEARNED;

	if (verbose) {
		msg("You have learned the spell of %s.", s->name);
	}

	p->upkeep->update |= PU_SPELLS;

	return;
}

struct player_spell *player_spell_lookup(int index) {
	struct player_spell *ps;
	for (ps = spells; ps; ps = ps->next) {
		if (ps->sidx == index) return ps;
	}
	return NULL;
}

int player_spell_mana(const struct player_spell *ps) {
	int base = ps->smana;
	int power = gener_spell_power(player, ps);
	int result;
	assert(NO_MANA_LEVEL > 0);

	result = ((NO_MANA_LEVEL - power) * base + NO_MANA_LEVEL - 1) / NO_MANA_LEVEL;

	return MAX(0, MIN(base, result));
}

int player_spell_fail(const struct player_spell *ps) {
	int base = ps->sfail;
	int power = gener_spell_power(player, ps);
	int ease = get_power_scale(player, PP_SPELL_EASE, 25);
	int result;

	int sqrt_pwr = my_int_sqrt(25 * (power + ease));
	result = base - sqrt_pwr;

	return MAX(0, MIN(base, result));

	assert(NO_FAIL_LEVEL > 0);

	result = ((NO_FAIL_LEVEL - power - ease) * base + NO_FAIL_LEVEL - 1) / NO_FAIL_LEVEL;

	return MAX(0, MIN(base, result));
}

void get_player_spell_info(int spell_index, char *p, size_t len)
{
	struct player_spell *spell = player_spell_lookup(spell_index);
	struct effect *effect = spell->effect;
	struct spell_info_iteration_state ist = {
		NULL, "", { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, false };

	p[0] = '\0';

	ref_spell = spell;

	while (effect) {
		spell_effect_append_value_info(effect, p, len, &ist);
		effect = effect->next;
	}

	ref_spell = NULL;
}

struct magic_realm *realm_by_index(int index)
{
	// return the first realm in the file if they don't currently have one
	struct magic_realm *realm = realms;
	for (realm = realms; realm; realm = realm->next) {
		if (realm->index == index) {
			return realm;
		}
	}
	return NULL;
}

const struct magic_realm *get_player_realm(const struct player *p)
{
	struct player_ability *abil = lookup_player_ability(SKILL_MAGIC, PY_ABIL_SKILL);
	int which = p->extra_choice[abil->learn_index];

	return realm_by_index(which);

	if (p->realm) return p->realm;
	// return the first realm in the file if they don't currently have one
	struct magic_realm *realm = realms;
	while (realm->next) {
		realm = realm->next;
	}
	return realm;
}

bool can_autocast(const struct player_spell *ps)
{
	if (!(player->player_spell_flags[ps->sidx] & PY_SPELL_WORKED)) {
		return false;
	}

	struct effect *ef;

	for (ef = ps->effect; ef; ef = ef->next)
	{
		switch (ps->effect->index)
		{
			case EF_TIMED_INC:
			case EF_NOURISH:
			case EF_HEAL_HP:
			case EF_RESTORE_STAT:
			case EF_RESTORE_EXP:
			case EF_CURE:
			case EF_LIGHT_AREA:
				return true;
		}
	}
	return false;
}

static bool will_autocast(struct player_spell *ps, const struct player *p)
{
	struct effect *ef;

	if (player_spell_mana(ps) > p->csp) {
		return false;
	}
	if (player_spell_fail(ps) >= 100) {
		return false;
	}
	if (!player_can_cast(p, false)) {
		return false;
	}

	for (ef = ps->effect; ef; ef = ef->next) {

		if (ef->index == EF_TIMED_INC) {
			if (p->mon.m_timed[ef->subtype] < 5) {
				return true;
			}
		}

		else if (ef->index == EF_NOURISH) {
			random_value rv = { 0, 0, 0, 0 };
			int amt, min = -1;

			dice_roll(ef->dice, &rv);
			amt = randcalc(rv, 0, AVERAGE);
			if (ef->subtype == 3) {
				min = (PY_FOOD_HUNGRY + amt) / 2;
				min = MIN(min, amt - 10);
			}
			else if (ef->subtype == 0) {
				min = PY_FOOD_FULL - amt - 1;
				min = MAX(min, PY_FOOD_HUNGRY);
			}

			if (min >= p->mon.m_timed[TMD_FOOD]) {
				return true;
			}
		}

		else if (ef->index == EF_HEAL_HP) {
			random_value rv = { 0, 0, 0, 0 };
			int amt;
			int warning = (p->mon.maxhp * p->opts.hitpoint_warn / 10);

			dice_roll(ef->dice, &rv);
			amt = randcalc(rv, 0, AVERAGE);

			if (p->mon.hp + amt < p->mon.maxhp) {
				return true;
			}
			if (p->mon.hp < warning) {
				return true;
			}
		}

		else if (ef->index == EF_CURE) {
			if (p->mon.m_timed[ef->subtype]) {
				return true;
			}
		}

		else if (ef->index == EF_RESTORE_STAT) {
			if (p->stat_max[ef->subtype] > p->stat_cur[ef->subtype]) {
				return true;
			}
		}

		else if (ef->index == EF_RESTORE_EXP) {
			if (p->exp < p->max_exp) {
				return true;
			}
		}

		else if (ef->index == EF_LIGHT_AREA) {
			if (!square_isglow(cave, p->grid)) {
				return true;
			}
		}
	}

	return false;
}

bool autocast(const struct player *p)
{
	int i;
	for (i = 0; i < z_info->spell_max; i++) {
		struct player_spell *ps = player_spell_lookup(i);
		if (p->player_spell_flags[i] & PY_SPELL_AUTOCAST) {
			if (will_autocast(ps, p)) {
				cmdq_push(CMD_CAST);
				cmd_set_arg_choice(cmdq_peek(), "spell", i);
				return true;
			}
		}
	}

	return false;
}


bool spell_is_castable_innately(const struct monster_race *mr, int spell_index)
{
	if (mon_spell_is_innate(spell_index)) return true;
	if (rf_has(mr->flags, RF_INNATE_MAGIC)) return true;
	return false;
}


static int random_spell_weight(struct player_spell *spell, int level)
{
	int levmod = level ? 2 * level + 25 : 10;

	return MAX(0, levmod - spell->slevel);
}

const struct player_spell *random_spell_at_level(int level)
{
	int total_count = 0, choice;
	struct player_spell *curr;

	for (curr = spells; curr; curr = curr->next) {
		total_count += random_spell_weight(curr, level);
	}

	choice = randint0(total_count);

	for (curr = spells; curr; curr = curr->next) {
		choice -= random_spell_weight(curr, level);

		if (choice < 0) return curr;
	}

	assert(!"got here");

	return NULL;
}

