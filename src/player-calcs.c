/**
 * \file player-calcs.c
 * \brief Player status calculation, signalling ui events based on 
 *	status changes.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2014 Nick McConnell
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
#include "effects.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "init.h"
#include "mon-msg.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "obj-curse.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"


/* L: matching monster resists to player_resists */
struct mon_player_match elem_matches[] = {
	{ RF_IM_ACID, ELEM_ACID },
	{ RF_IM_ELEC, ELEM_ELEC },
	{ RF_IM_FIRE, ELEM_FIRE },
	{ RF_IM_COLD, ELEM_COLD },
	{ RF_IM_POIS, ELEM_POIS },
	{ RF_IM_NETHER, ELEM_NETHER },
	{ RF_IM_WATER, ELEM_WATER },
	{ RF_IM_PLASMA, ELEM_PLASMA },
	{ RF_IM_NEXUS, ELEM_NEXUS },
	{ RF_IM_DISEN, ELEM_DISEN },
	{ RF_NONE, -1 }
};

/*struct mon_player_match flag_matches[] = {
	{ RF_NO_FEAR, OF_PROT_FEAR },
	{ RF_NO_STUN, OF_PROT_STUN },
	{ RF_NO_CONF, OF_PROT_CONF },
	{ -1, -1 }
};*/



/* L: rewriting the stat stuff entirely */

/**
 * L: scales a number to the stat value such that
 * stat_scale(10, x) = 0
 * and
 * stat_scale(17, x) = x
 * with numbers increasing quadratically
 * generally scaleto is the high value for that table
 */
static int stat_scale(int index, int scaleto, bool minzero) {
    int hsi = 14;
	int asi = 7;
	int lsi = 0;
	assert(hsi != asi);
	index = MAX(index, lsi);

	int negative = ((asi - lsi) * scaleto + 49) / 50;

	if (index >= asi) return (int)(scaleto * 
	                               ((index - asi) * (index - asi)) /
								   ((hsi - asi) * (hsi - asi))) + 
								   (minzero ? negative : 0);

	if (index <= asi) return ((index - asi) * scaleto - 24) / 25 + (minzero ? negative : 0);

	return minzero ? negative : 0;
}


static int adj_int_dev(int index) {
	return stat_scale(index, 15, false);
}

static int adj_wis_sav(int index) {
	return stat_scale(index, 50, false);
}

static int adj_dex_dis(int index) {
	return stat_scale(index, 20, false);
}

static int adj_int_dis(int index) {
	return stat_scale(index, 20, false);
}

static int adj_dex_ta(int index) {
	return stat_scale(index, 15, false);
}

int adj_str_td(int index) {
	return (index - 7) * 15 / 8;
}

int adj_dex_th(int index) {
	return (index - 7) * 35 / 8;
}

/*static int adj_str_th(int index) {
	return stat_scale(index, 15, false);
}*/

static int adj_str_wgt(int index) {
	int ret = stat_scale(index, 250, false) + 100;
	return MAX(ret, 25);
}

int adj_str_hold(int index) {
	return stat_scale(index, 200, true) + 50;
}

static int adj_str_dig(int index) {
	return stat_scale(index, 100, false);
}

int adj_str_blow(int index) {
	return stat_scale(index, 240, true);
}

/*static int adj_dex_blow(int index) {
	return stat_scale(index, 10, true);
}*/

static int adj_stat_blow(int index) {
	return (index + 1) * 600 / 16;
}

int adj_dex_safe(int index) {
	return stat_scale(index, 80, true);
}

int adj_con_fix(int index) {
	return stat_scale(index, 10, true);
}

static int adj_con_mhp(int index) {
	return stat_scale(index, 250, false);
}

static int adj_mag_study(int index) {
	return (index + 5) * 10 / 20;
}

static int adj_mag_mana(int index) {
	return (index + 5) * 500 / 20;
}

int adj_int_xp(int index) {
	return 14 - 2 * index;
}

int adj_int_lev(int index) {
	return index - 7;
}

int adj_mag_stat(int index) {
	return index - 7;
}


/**
 * L: monk agility bonuses
 */

static int unarmoured_speed_bonus(struct player_state *s, int wgt)
{
	int wpen = wgt / 5 - get_power_scale(player, PP_AGILITY, 10, PP_SCALE_SQUARE);
	wpen = MAX(0, wpen);
	int bonus = get_power_scale(player, PP_AGILITY, 10, PP_SCALE_SQUARE);
	bonus = MAX(0, bonus - wpen);

    s->speed += bonus;
	return bonus;
}

static int unarmoured_ac_bonus(struct player_state *s, int wgt)
{
	int wpen = wgt - get_power_scale(player, PP_AGILITY, 250, PP_SCALE_LINEAR);
	wpen = MAX(0, wpen);
    int bonus = get_power_scale(player, PP_AGILITY, 50, PP_SCALE_SQRT);
	bonus = MAX(bonus / 2, bonus - wpen);

    s->ac += bonus;
	return bonus;
}


#if 0
/**
 * This table is used to help calculate the number of blows the player can
 * make in a single round of attacks (one player turn) with a normal weapon.
 *
 * This number ranges from a single blow/round for weak players to up to six
 * blows/round for powerful warriors.
 *
 * Note that certain artifacts and ego-items give "bonus" blows/round.
 *
 * First, from the player class, we extract some values:
 *
 *    Warrior --> num = 6; mul = 5; div = MAX(30, weapon_weight);
 *    Mage    --> num = 4; mul = 2; div = MAX(40, weapon_weight);
 *    Priest  --> num = 4; mul = 3; div = MAX(35, weapon_weight);
 *    Rogue   --> num = 5; mul = 4; div = MAX(30, weapon_weight);
 *    Ranger  --> num = 5; mul = 4; div = MAX(35, weapon_weight);
 *    Paladin --> num = 5; mul = 5; div = MAX(30, weapon_weight);
 * (all specified in class.txt now)
 *
 * To get "P", we look up the relevant "adj_str_blow[]" (see above),
 * multiply it by "mul", and then divide it by "div", rounding down.
 *
 * To get "D", we look up the relevant "adj_dex_blow[]" (see above).
 *
 * Then we look up the energy cost of each blow using "blows_table[P][D]".
 * The player gets blows/round equal to 100/this number, up to a maximum of
 * "num" blows/round, plus any "bonus" blows/round.
 */
static const int blows_table[12][12] =
{
	/* P */
   /* D:   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11+ */
   /* DEX: 3,   10,  17,  /20, /40, /60, /80, /100,/120,/150,/180,/200 */

	/* 0  */
	{  100, 100, 95,  85,  75,  60,  50,  42,  35,  30,  25,  23 },

	/* 1  */
	{  100, 95,  85,  75,  60,  50,  42,  35,  30,  25,  23,  21 },

	/* 2  */
	{  95,  85,  75,  60,  50,  42,  35,  30,  26,  23,  21,  20 },

	/* 3  */
	{  85,  75,  60,  50,  42,  36,  32,  28,  25,  22,  20,  19 },

	/* 4  */
	{  75,  60,  50,  42,  36,  33,  28,  25,  23,  21,  19,  18 },

	/* 5  */
	{  60,  50,  42,  36,  33,  30,  27,  24,  22,  21,  19,  17 },

	/* 6  */
	{  50,  42,  36,  33,  30,  27,  25,  23,  21,  20,  18,  17 },

	/* 7  */
	{  42,  36,  33,  30,  28,  26,  24,  22,  20,  19,  18,  17 },

	/* 8  */
	{  36,  33,  30,  28,  26,  24,  22,  21,  20,  19,  17,  16 },

	/* 9  */
	{  35,  32,  29,  26,  24,  22,  21,  20,  19,  18,  17,  16 },

	/* 10 */
	{  34,  30,  27,  25,  23,  22,  21,  20,  19,  18,  17,  16 },

	/* 11+ */
	{  33,  29,  26,  24,  22,  21,  20,  19,  18,  17,  16,  15 },
   /* DEX: 3,   10,  17,  /20, /40, /60, /80, /100,/120,/150,/180,/200 */
};
#endif

/**
 * Decide which object comes earlier in the standard inventory listing,
 * defaulting to the first if nothing separates them.
 *
 * \return whether to replace the original object with the new one
 */
bool earlier_object(struct object *orig, struct object *new, bool store)
{
	/* Check we have actual objects */
	if (!new) return false;
	if (!orig) return true;

	if (!store) {
		/* Readable books always come first */
		if (obj_can_browse(orig) && !obj_can_browse(new)) return false;
		if (!obj_can_browse(orig) && obj_can_browse(new)) return true;
	}

	/* Usable ammo is before other ammo */
	if (tval_is_ammo(orig) && tval_is_ammo(new)) {
		/* First favour usable ammo */
		if ((player->state.ammo_tval == orig->tval) &&
			(player->state.ammo_tval != new->tval))
			return false;
		if ((player->state.ammo_tval != orig->tval) &&
			(player->state.ammo_tval == new->tval))
			return true;
	}

	/* Objects sort by decreasing type */
	if (orig->tval > new->tval) return false;
	if (orig->tval < new->tval) return true;

	if (!store) {
		/* Non-aware (flavored) items always come last (default to orig) */
		if (!object_flavor_is_aware(new)) return false;
		if (!object_flavor_is_aware(orig)) return true;
	}

	/* Objects sort by increasing sval */
	if (orig->sval < new->sval) return false;
	if (orig->sval > new->sval) return true;

	if (!store) {
		/* Unaware objects always come last (default to orig) */
		if (new->kind->flavor && !object_flavor_is_aware(new)) return false;
		if (orig->kind->flavor && !object_flavor_is_aware(orig)) return true;

		/* Lights sort by decreasing fuel */
		if (tval_is_light(orig)) {
			if (orig->pval > new->pval) return false;
			if (orig->pval < new->pval) return true;
		}
	}

	/* Objects sort by decreasing value, except ammo */
	if (tval_is_ammo(orig)) {
		if (object_value(orig, 1) < object_value(new, 1))
			return false;
		if (object_value(orig, 1) >	object_value(new, 1))
			return true;
	} else {
		if (object_value(orig, 1) >	object_value(new, 1))
			return false;
		if (object_value(orig, 1) <	object_value(new, 1))
			return true;
	}

	/* No preference */
	return false;
}

int equipped_item_slot(struct player_body body, struct object *item)
{
	int i;

	if (item == NULL) return body.count;

	/* Look for an equipment slot with this item */
	for (i = 0; i < body.count; i++)
		if (item == body.slots[i].obj) break;

	/* Correct slot, or body.count if not equipped */
	return i;
}

/**
 * Put the player's inventory and quiver into easily accessible arrays.  The
 * pack may be overfull by one item
 */
void calc_inventory(struct player *p)
{
	int old_inven_cnt = p->upkeep->inven_cnt;
	int n_stack_split = 0;
	int n_pack_remaining = z_info->pack_size - pack_slots_used(p);
	int n_max = 1 + z_info->pack_size + z_info->quiver_size
		+ p->body.count;
	struct object **old_quiver = mem_zalloc(z_info->quiver_size
		* sizeof(*old_quiver));
	struct object **old_pack = mem_zalloc(z_info->pack_size
		* sizeof(*old_pack));
	bool *assigned = mem_alloc(n_max * sizeof(*assigned));
	struct object *current;
	int i, j;

	/*
	 * Equipped items are already taken care of.  Only the others need
	 * to be tested for assignment to the quiver or pack.
	 */
	for (current = p->gear, j = 0; current; current = current->next, ++j) {
		assert(j < n_max);
		assigned[j] = object_is_equipped(p->body, current);
	}
	for (; j < n_max; ++j) {
		assigned[j] = false;
	}

	/* Prepare to fill the quiver */
	p->upkeep->quiver_cnt = 0;

	/* Copy the current quiver and then leave it empty. */
	for (i = 0; i < z_info->quiver_size; i++) {
		if (p->upkeep->quiver[i]) {
			old_quiver[i] = p->upkeep->quiver[i];
			p->upkeep->quiver[i] = NULL;
		} else {
			old_quiver[i] = NULL;
		}
	}

	/* Fill quiver.  First, allocate inscribed items. */
	for (current = p->gear, j = 0; current; current = current->next, ++j) {
		int prefslot;

		/* Skip already assigned (i.e. equipped) items. */
		if (assigned[j]) continue;

		prefslot  = preferred_quiver_slot(current);
		if (prefslot >= 0 && prefslot < z_info->quiver_size
				&& !p->upkeep->quiver[prefslot]) {
			/*
			 * The preferred slot is empty.  Split the stack if
			 * necessary.  Don't allow splitting if it could
			 * result in overfilling the pack by more than one slot.
			 */
			int mult = tval_is_ammo(current) ?
				1 : z_info->thrown_quiver_mult;
			struct object *to_quiver;

			if (current->number * mult
					<= z_info->quiver_slot_size) {
				to_quiver = current;
			} else {
				int nsplit = z_info->quiver_slot_size / mult;

				assert(nsplit < current->number);
				if (nsplit > 0 && n_stack_split
						<= n_pack_remaining) {
					/*
					 * Split off the portion that goes to
					 * the pack.  Since the stack in the
					 * quiver is earlier in the gear list it
					 * will prefer to remain in the quiver
					 * in future calls to calc_inventory()
					 * and will be the preferred target for
					 * combine_pack().
					 */
					to_quiver = current;
					gear_insert_end(p, object_split(current,
						current->number - nsplit));
					++n_stack_split;
				} else {
					to_quiver = NULL;
				}
			}

			if (to_quiver) {
				p->upkeep->quiver[prefslot] = to_quiver;
				p->upkeep->quiver_cnt += to_quiver->number * mult;

				/* That part of the gear has been dealt with. */
				assigned[j] = true;
			}
		}
	}

	/* Now fill the rest of the slots in order. */
	for (i = 0; i < z_info->quiver_size; ++i) {
		struct object *first = NULL;
		int jfirst = -1;

		/* If the slot is full, move on. */
		if (p->upkeep->quiver[i]) continue;

		/* Find the quiver object that should go there. */
		j = 0;
		current = p->gear;
		while (1) {
			if (!current) break;
			assert(j < n_max);

			/*
			 * Only try to assign if not assigned, ammo, and,
			 * if necessary to split, have room for the split
			 * stacks.
			 */
			if (!assigned[j] && tval_is_ammo(current)
					&& (current->number
					<= z_info->quiver_slot_size
					|| (z_info->quiver_slot_size > 0
					&& n_stack_split
					<= n_pack_remaining))) {
				/* Choose the first in order. */
				if (earlier_object(first, current, false)) {
					first = current;
					jfirst = j;
				}
			}

			current = current->next;
			++j;
		}

		/* Stop looking if there's nothing left in the gear. */
		if (!first) break;

		/* Put the item in the slot, splitting (if needed) to fit. */
		if (first->number > z_info->quiver_slot_size) {
			assert(z_info->quiver_slot_size > 0
				&& n_stack_split <= n_pack_remaining);
			/* As above, split off the portion going to the pack. */
			gear_insert_end(p, object_split(first,
				first->number - z_info->quiver_slot_size));
		}
		p->upkeep->quiver[i] = first;
		p->upkeep->quiver_cnt += first->number;

		/* That part of the gear has been dealt with. */
		assigned[jfirst] = true;
	}

	/* Note reordering */
	if (character_dungeon) {
		for (i = 0; i < z_info->quiver_size; i++) {
			if (old_quiver[i] && p->upkeep->quiver[i] != old_quiver[i]) {
				msg("You re-arrange your quiver.");
				break;
			}
		}
	}

	/* Copy the current pack */
	for (i = 0; i < z_info->pack_size; i++) {
		old_pack[i] = p->upkeep->inven[i];
	}

	/* Prepare to fill the inventory */
	p->upkeep->inven_cnt = 0;

	for (i = 0; i <= z_info->pack_size; i++) {
		struct object *first = NULL;
		int jfirst = -1;

		/* Find the object that should go there. */
		j = 0;
		current = p->gear;
		while (1) {
			if (!current) break;
			assert(j < n_max);

			/* Consider it if it hasn't already been handled. */
			if (!assigned[j]) {
				/* Choose the first in order. */
				if (earlier_object(first, current, false)) {
					first = current;
					jfirst = j;
				}
			}

			current = current->next;
			++j;
		}

		/* Allocate */
		p->upkeep->inven[i] = first;
		if (first) {
			++p->upkeep->inven_cnt;
			assigned[jfirst] = true;
		}
	}

	/* Note reordering */
	if (character_dungeon && p->upkeep->inven_cnt == old_inven_cnt) {
		for (i = 0; i < z_info->pack_size; i++) {
			if (old_pack[i] && p->upkeep->inven[i] != old_pack[i]
					 && !object_is_equipped(p->body, old_pack[i])) {
				msg("You re-arrange your pack.");
				break;
			}
		}
	}

	mem_free(assigned);
	mem_free(old_pack);
	mem_free(old_quiver);
}

#if 0
/**
 * Average of the player's spell stats across all the realms they can cast
 * from, rounded up
 *
 * If the player can only cast from a single realm, this is simple the stat
 * for that realm
 */
static int average_spell_stat(struct player *p, struct player_state *state)
{
	int i, count, sum = 0;
	struct magic_realm *realm = class_magic_realms(p->class, &count), *r_next;

	for (i = count; i > 0; i--) {
		sum += state->stat_ind[realm->stat];
		r_next = realm->next;
		mem_free(realm);
		realm = r_next;
	}
	return (sum + count - 1) / count;
}
#endif

/**
 * Calculate number of spells player should have, and forget,
 * or remember, spells until that number is properly reflected.
 *
 * Note that this function induces various "status" messages,
 * which must be bypasses until the character is created.
 */
static void calc_spells(struct player *p)
{
	int j, k;
	int num_allowed, num_known;
	struct magic_realm *realm = get_player_realm(p);

	const struct player_spell *spell;

	int16_t old_spells;

	/* Hack -- wait for creation */
	if (!character_generated) return;

	/* Hack -- handle partial mode */
	if (p->upkeep->only_partial) return;

	/* Save the new_spells value */
	old_spells = p->upkeep->new_spells;

	/* Determine the number of spells allowed */
	//levels = p->lev - p->class->magic.spell_first + 1;

	/* Hack -- no negative spells */
	//if (levels < 0) levels = 0;

	/* Number of 1/100 spells per level (or something - needs clarifying) */
	num_allowed = adj_mag_study(realm->stat);

	/* Extract total allowed spells (rounded up) */
	//num_allowed = (((percent_spells * levels) + 50) / 100);

	/* Assume none known */
	num_known = 0;

	/* Count num we know */
	for (j = 0; j < z_info->spell_max; j++) {
		if (p->player_spell_flags[j] & PY_SPELL_LEARNED)
			++num_known;
	}

	/* See how many spells we must forget or may learn */
	p->upkeep->new_spells = num_allowed - num_known;

	/*
	// Forget spells which are too hard 
	for (i = num_total - 1; i >= 0; i--) {
		// Get the spell
		j = p->spell_order[i];

		// Skip non-spells
		if (j >= 99) continue;

		// Get the spell
		spell = spell_by_index(p, j);

		// Skip spells we are allowed to know
		if (spell->slevel <= (p->lev + caster_level_bonus(p, spell))) continue;

		// Is it known?
		if (p->spell_flags[j] & PY_SPELL_LEARNED) {
			// Mark as forgotten
			p->spell_flags[j] |= PY_SPELL_FORGOTTEN;

			// No longer known
			p->spell_flags[j] &= ~PY_SPELL_LEARNED;

			// Message
			msg("You have forgotten the %s of %s.", spell->realm->spell_noun,
				spell->name);

			// One more can be learned
			p->upkeep->new_spells++;
		}
	}
	

	// Forget spells if we know too many spells
	for (i = num_total - 1; i >= 0; i--) {
		// Stop when possible
		if (p->upkeep->new_spells >= 0) break;

		// Get the (i+1)th spell learned
		j = p->spell_order[i];

		// Skip unknown spells
		if (j >= 99) continue;

		// Get the spell
		spell = spell_by_index(p, j);

		// Forget it (if learned)
		if (p->spell_flags[j] & PY_SPELL_LEARNED) {
			// Mark as forgotten
			p->spell_flags[j] |= PY_SPELL_FORGOTTEN;

			// No longer known
			p->spell_flags[j] &= ~PY_SPELL_LEARNED;

			// Message 
			msg("You have forgotten the %s of %s.", spell->realm->spell_noun,
				spell->name);

			// One more can be learned
			p->upkeep->new_spells++;
		}
	}

	// Check for spells to remember
	for (i = 0; i < num_total; i++) {
		// None left to remember
		if (p->upkeep->new_spells <= 0) break;

		// Get the next spell we learned
		j = p->spell_order[i];

		// Skip unknown spells
		if (j >= 99) break;

		// Get the spell
		spell = spell_by_index(p, j);

		// Skip spells we cannot remember
		if (spell->slevel > p->lev) continue;

		// First set of spells
		if (p->spell_flags[j] & PY_SPELL_FORGOTTEN) {
			// No longer forgotten
			p->spell_flags[j] &= ~PY_SPELL_FORGOTTEN;

			// Known once more
			p->spell_flags[j] |= PY_SPELL_LEARNED;

			// Message
			msg("You have remembered the %s of %s.", spell->realm->spell_noun,
				spell->name);

			// One less can be learned
			p->upkeep->new_spells--;
		}
	}
	*/

	/* Assume no spells available */
	k = 0;

	/* Count spells that can be learned */
	for (j = 0; j < z_info->spell_max; j++) {
		/* Get the spell */
		spell = player_spell_lookup(j);

		/* Skip spells we cannot remember or don't exist */
		if (!spell) continue;
		if (gener_spell_power(p, spell) <= 0 || spell->slevel == 0) continue;

		/* Skip spells we already know */
		if (p->player_spell_flags[j] & PY_SPELL_LEARNED)
			continue;

		/* Count it */
		k++;
	}

	/* Cannot learn more spells than exist */
	if (p->upkeep->new_spells > k) p->upkeep->new_spells = k;

	/* Spell count changed */
	if (old_spells != p->upkeep->new_spells) {
		/* Message if needed */
		/*
		if (p->upkeep->new_spells) {
			int count;
			struct magic_realm *r = class_magic_realms(p->class, &count), *r1;
			char buf[120];

			my_strcpy(buf, r->spell_noun, sizeof(buf));
			if (p->upkeep->new_spells > 1) {
				my_strcat(buf, "s", sizeof(buf));
			}
			r1 = r->next;
			mem_free(r);
			r = r1;
			if (count > 1) {
				while (r) {
					count--;
					if (count) {
						my_strcat(buf, ", ", sizeof(buf));
					} else {
						my_strcat(buf, " or ", sizeof(buf));
					}
					my_strcat(buf, r->spell_noun, sizeof(buf));
					if (p->upkeep->new_spells > 1) {
						my_strcat(buf, "s", sizeof(buf));
					}
					r1 = r->next;
					mem_free(r);
					r = r1;
				}
			}
			

			// Message
			msg("You can learn %d more %s.", p->upkeep->new_spells, buf);
		}
		*/

		if (p->upkeep->new_spells) {
			msg("You can learn %d new %s%s.",
					p->upkeep->new_spells,
					realm->spell_noun,
					PLURAL(p->upkeep->new_spells));
		}

		/* Redraw Study Status */
		p->upkeep->redraw |= (PR_STUDY | PR_OBJECT);
	}
}


/**
 * Calculate maximum mana.  You do not need to know any spells.
 * Note that mana is lowered by heavy (or inappropriate) armor.
 *
 * This function induces status messages.
 */
static void calc_mana(struct player *p, struct player_state *state, bool update)
{
	int i, msp, levels, cur_wgt, max_wgt, ass;
	struct monster_race *monr = lookup_player_monster(p);

	/* Must be literate */
	if (/*!p->class->magic.total_spells && */state->skills[SKILL_MAGIC] <= 0) {
		p->msp = 0;
		p->csp = 0;
		p->csp_frac = 0;
		return;
	}

	
	levels = state->skills[SKILL_MAGIC];
	ass = state->stat_ind[STAT_INT];

	/* Extract "effective" player level */
	if (levels > 0) {
		msp = 1;
		msp += adj_mag_mana(ass) * levels * p->lev / 5000;
	} else {
		levels = 0;
		msp = 0;
	}

	/* Assume player not encumbered by armor */
	state->cumber_armor = false;

	/* Weigh the armor */
	cur_wgt = 0;
	for (i = 0; i < p->body.count; i++) {
		struct object *obj_local = slot_object(p, i);

		/* Ignore non-armor */
		if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
		if (slot_type_is(p, i, EQUIP_BOW)) continue;
		if (slot_type_is(p, i, EQUIP_RING)) continue;
		if (slot_type_is(p, i, EQUIP_AMULET)) continue;
		if (slot_type_is(p, i, EQUIP_LIGHT)) continue;

		/* Add weight */
		if (obj_local)
			cur_wgt += object_weight_one(obj_local);
	}

	/* Determine the weight allowance */
	max_wgt = 1000;

	/* Heavy armor penalizes mana */
	if (((cur_wgt - max_wgt) / 10) > 0) {
		/* Encumbered */
		state->cumber_armor = true;

		/* Reduce mana */
		msp -= ((cur_wgt - max_wgt) / 10);
	}

	if (monr && msp > 0 && monr->freq_spell) {
		msp += MIN(msp, monr->freq_spell);
	}

	/* Mana can never be negative */
	if (msp < 0) msp = 0;

	msp = MAX(msp / 2, msp - p->sp_burn);

	/* Return if no updates */
	if (!update) return;

	/* Maximum mana has changed */
	if (p->msp != msp) {
		/* Save new limit */
		p->msp = msp;

		/* Enforce new limit */
		if (p->csp >= msp) {
			p->csp = msp;
			p->csp_frac = 0;
		}

		/* Display mana later */
		p->upkeep->redraw |= (PR_MANA);
	}
}


/**
 * Calculate the players (maximal) hit points
 *
 * Adjust current hitpoints if necessary
 */
static void calc_hitpoints(struct player *p)
{
	long bonus;
	int mhp;
	int resil = get_power_scale(p, PP_RESILIENCE, 50, PP_SCALE_LINEAR);
	struct monster_race *mon = lookup_player_monster(p);

	/* Get "1/100th hitpoint bonus per level" value */
	bonus = adj_con_mhp(p->state.stat_ind[STAT_CON]);

	/* Calculate hitpoints */
	mhp = p->player_hp[p->lev - 1] + (bonus * p->lev / 100);

	/* L: bonus from resilience power */
	mhp += (mhp + p->lev * 2) * resil / 100;

	/* L: bonus from being a monster */
	if (mon) {
		mhp += (int)my_cbrt((double)mon->avg_hp * mon->avg_hp);
	}

	/* Always have at least one hitpoint per level */
	if (mhp < p->lev + 1) mhp = p->lev + 1;

	mhp = MAX(mhp / 2, mhp - p->hp_burn);

	/* New maximum hitpoints */
	if (p->mhp != mhp) {
		/* Save new limit */
		p->mhp = mhp;

		/* Enforce new limit */
		if (p->chp >= mhp) {
			p->chp = mhp;
			p->chp_frac = 0;
		}

		/* Display hitpoints (later) */
		p->upkeep->redraw |= (PR_HP);
	}
}


/**
 * Calculate and set the current light radius.
 *
 * The light radius will be the total of all lights carried.
 */
static void calc_light(struct player *p, struct player_state *state,
					   bool update)
{
	int i;

	/* Assume no light */
	state->cur_light = 0;

	/* Ascertain lightness if in the town */
	if (!p->depth && is_daytime() && update) {
		/* Update the visuals if necessary*/
		if (p->state.cur_light != state->cur_light)
			p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		return;
	}

	/* Examine all wielded objects, use the brightest */
	for (i = 0; i < p->body.count; i++) {
		int amt = 0;
		struct object *obj = slot_object(p, i);

		/* Skip empty slots */
		if (!obj) continue;

		/* Light radius - innate plus modifier */
		if (of_has(obj->flags, OF_LIGHT_2)) {
			amt = 2;
		} else if (of_has(obj->flags, OF_LIGHT_3)) {
			amt = 3;
		}
		amt += obj->modifiers[OBJ_MOD_LIGHT];

		/* Adjustment to allow UNLIGHT players to use +1 LIGHT gear */
		if ((obj->modifiers[OBJ_MOD_LIGHT] > 0) && pf_has(state->pflags, PF_UNLIGHT)) {
			amt--;
		}

		/* Examine actual lights */
		if (tval_is_light(obj) && !of_has(obj->flags, OF_NO_FUEL) &&
				obj->timeout == 0)
			/* Lights without fuel provide no light */
			amt = 0;

		/* Alter p->state.cur_light if reasonable */
	    state->cur_light += amt;
	}
}

/**
 * Populates `chances` with the player's chance of digging through
 * the diggable terrain types in one turn out of 1600.
 */
void calc_digging_chances(struct player_state *state, int chances[DIGGING_MAX])
{
	int i;

	chances[DIGGING_RUBBLE] = state->skills[SKILL_DIGGING] * 8;
	chances[DIGGING_MAGMA] = (state->skills[SKILL_DIGGING] - 10) * 4;
	chances[DIGGING_QUARTZ] = (state->skills[SKILL_DIGGING] - 20) * 2;
	chances[DIGGING_GRANITE] = (state->skills[SKILL_DIGGING] - 40) * 1;
	/* Approximate a 1/1200 chance per skill point over 30 */
	chances[DIGGING_DOORS] = (state->skills[SKILL_DIGGING] * 4 - 119) / 3;

	/* Don't let any negative chances through */
	for (i = 0; i < DIGGING_MAX; i++)
		chances[i] = MAX(0, chances[i]);
}

/*
 * Return the chance, out of 100, for unlocking a locked door with the given
 * lock power.
 *
 * \param p is the player trying to unlock the door.
 * \param lock_power is the power of the lock.
 * \param lock_unseen, if true, assumes the player does not have sufficient
 * light to work with the lock.
 */
int calc_unlocking_chance(const struct player *p, int lock_power,
		bool lock_unseen)
{
	int skill = p->state.skills[SKILL_DISARM_PHYS];

	if (lock_unseen || p->timed[TMD_BLIND]) {
		skill /= 10;
	}
	if (p->timed[TMD_CONFUSED] || p->timed[TMD_IMAGE]) {
		skill /= 10;
	}

	/* Always allow some chance of unlocking. */
	return MAX(2, skill - 4 * lock_power);
}

/**
 * L: new calc_blows 
 * relies on object weight, melee skill, and both str and dex, with
 * the higher of str and dex weighted more heavily
 */
void calc_blows(struct player *p, int wgt, struct attack_roll *aroll,
               struct player_state *state, int extra_blows)
{
	int div = MAX(wgt * 2, 25) + 100;

    int sind1 = state->stat_ind[aroll->damage_stat];
	int sind2 = state->stat_ind[aroll->accuracy_stat];

	// max 18
	int statind = (sind1 + sind2 + MAX(sind1, sind2)) / 3;

	// max 600
	int baseblows = adj_stat_blow(statind);

	// max 100
	int skill = aroll->attack_skill;

	// max 600 * 100 / 100 + 100
	int blows = MAX(0, baseblows) * state->skills[skill] / div;

	aroll->blows = blows + 100 * extra_blows;
}

#if 0
/**
 * Calculate the blows a player would get.
 *
 * \param obj is the object for which we are calculating blows
 * \param state is the player state for which we are calculating blows
 * \param extra_blows is the number of +blows available from this object and
 * this state
 *
 * N.B. state->num_blows is now 100x the number of blows.
 */
int calc_blows_old(struct player *p, const struct object *obj,
			   struct player_state *state, int extra_blows)
{
	int blows;
	int str_index, dex_index;
	int div;
	int blow_energy;

	int weight = (obj == NULL) ? 0 : object_weight_one(obj);
	int min_weight = p->class->min_weight;

	/* Enforce a minimum "weight" (tenth pounds) */
	div = (weight < min_weight) ? min_weight : weight;

	/* Get the strength vs weight */
	str_index = adj_str_blow(state->stat_ind[STAT_STR]) *
			p->class->att_multiply / div;

	/* Maximal value */
	if (str_index > 11) str_index = 11;

	/* Index by dexterity */
	dex_index = MIN(adj_dex_blow(state->stat_ind[STAT_DEX]), 11);

	/* Use the blows table to get energy per blow */
	blow_energy = blows_table[str_index][dex_index];

	blows = MIN((10000 / blow_energy), (100 * p->class->max_attacks));

	/* Require at least one blow, two for O-combat */
	return MAX(blows + (100 * extra_blows),
			   OPT(p, birth_percent_damage) ? 200 : 100);
}
#endif


/**
 * Computes current weight limit.
 */
static int weight_limit(struct player_state *state)
{
	int i;

	/* Weight limit based only on strength */
	i = adj_str_wgt(state->stat_ind[STAT_STR]) * 10;

	/* Return the result */
	return (i);
}


/**
 * Computes weight remaining before burdened.
 */
int weight_remaining(struct player *p)
{
	int i;

	/* Weight limit based only on strength */
	i = 6 * adj_str_wgt(p->state.stat_ind[STAT_STR])
		- p->upkeep->total_weight - 1;

	/* Return the result */
	return (i);
}


/**
 * Adjust a value by a relative factor of the absolute value.  Mimics the
 * inline calculations of value = (value * (den + num)) / num when value is
 * positive.
 * \param v Is a pointer to the value to adjust.
 * \param num Is the numerator of the relative factor.  Use a negative value
 * for a decrease in the value, and a positive value for an increase.
 * \param den Is the denominator for the relative factor.  Must be positive.
 * \param minv Is the minimum absolute value of v to use when computing the
 * adjustment; use zero for this to get a pure relative adjustment.  Must be
 * be non-negative.
 */
static void adjust_skill_scale(int *v, int num, int den, int minv)
{
	if (num >= 0) {
		*v += (MAX(minv, ABS(*v)) * num) / den;
	} else {
		/*
		 * To mimic what (value * (den + num)) / den would give for
		 * positive value, need to round up the adjustment.
		 */
		*v -= (MAX(minv, ABS(*v)) * -num + den - 1) / den;
	}
}


/**
 * Calculate the effect of a shapechange on player state
 */
static void calc_shapechange(struct player_state *state, bool vuln[ELEM_MAX],
							 struct player_shape *shape,
							 int *blows, int *shots, int *might, int *moves)
{
	int i;

	/* Combat stats */
	state->to_a += shape->to_a;
	state->to_h += shape->to_h;
	state->to_d += shape->to_d;

	/* Skills */
	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] += shape->skills[i];
	}

	/* Object flags */
	of_union(state->flags, shape->flags);

	/* Player flags */
	pf_union(state->pflags, shape->pflags);

	/* Stats */
	for (i = 0; i < STAT_MAX; i++) {
		state->stat_add[i] += shape->modifiers[i];
	}

	/* Other modifiers */
	state->skills[SKILL_STEALTH] += shape->modifiers[OBJ_MOD_STEALTH];
	state->skills[SKILL_SEARCH] += (shape->modifiers[OBJ_MOD_SEARCH] * 5);
	state->see_infra += shape->modifiers[OBJ_MOD_INFRA];
	state->skills[SKILL_DIGGING] += (shape->modifiers[OBJ_MOD_TUNNEL] * 20);
	state->speed += shape->modifiers[OBJ_MOD_SPEED];
	state->dam_red += shape->modifiers[OBJ_MOD_DAM_RED];
	*blows += shape->modifiers[OBJ_MOD_BLOWS];
	*shots += shape->modifiers[OBJ_MOD_SHOTS];
	*might += shape->modifiers[OBJ_MOD_MIGHT];
	*moves += shape->modifiers[OBJ_MOD_MOVES];

	/* Resists and vulnerabilities */
	for (i = 0; i < ELEM_MAX; i++) {
		if (shape->el_info[i].res_level == -1) {
			/* Remember vulnerabilities for application later. */
			vuln[i] = true;
		} else if (shape->el_info[i].res_level
				> state->el_info[i].res_level) {
			/*
			 * Otherwise apply the shape's resistance level if it
			 * is better; this is okay because any vulnerabilities
			 * have not been included in the state's res_level yet.
			 */
			state->el_info[i].res_level =
				shape->el_info[i].res_level;
		}
	}
}

static bool calc_monster_spell(int *counts, const struct monster_spell *mspell)
{
	if (!mspell) return false;

	int skill = -1;

	switch (mspell->effect->index)
	{
		case EF_BALL:
		case EF_BALL_NO_DAM_RED:
		case EF_BEAM:
		case EF_BOLT:
		case EF_BREATH:
		{
			switch (mspell->effect->subtype)
			{
				case ELEM_PLASMA:
				case ELEM_FIRE:
					skill = PP_FIRE_MAGIC;
					break;
				case ELEM_FORCE:
				case ELEM_ACID:
				case ELEM_SHARD:
					skill = PP_EARTH_MAGIC;
					break;
				case ELEM_ICE:
				case ELEM_COLD:
				case ELEM_WATER:
					skill = PP_WATER_MAGIC;
					break;
				case ELEM_ELEC:
					skill = PP_AIR_MAGIC;
					break;
				case ELEM_NETHER:
					skill = PP_NECROMANCY_MAGIC;
					break;
				case ELEM_POIS:
					skill = PP_POISON_MAGIC;
					break;
				default:
					skill = -1;
					break;
			}
			break;
		}
		case EF_MON_HEAL_HP:
		case EF_MON_HEAL_KIN:
			skill = PP_HEALING_MAGIC;
			break;
		default:
			skill = -1;
			break;
	}
	if (skill > -1) {
		counts[skill] += 1;
		return true;
	}
	return false;
}

/**
 * L: calculate the effects of being a monster on player state
 */
static void calc_monster(struct player_state *state, bool vuln[ELEM_MAX],
						 struct monster_race *mrace, int *moves)
{
	if (!mrace) return;
	int i;
	const struct monster_spell *mspell;
	int *spell_counts = mem_zalloc((size_t)PP_MAX * sizeof(int));
	int numcounts = 0, totalbonus;

	i = 0;
	while (elem_matches[i].mval != RF_NONE) {
		if (rf_has(mrace->flags, elem_matches[i].mval)) {
			state->el_info[elem_matches[i].pval].res_level = 3;
		}
		i++;
	}

	state->speed += mrace->speed / 2 - 55;
	state->ac = MAX(state->ac, mrace->ac);

	if (rf_has(mrace->flags, RF_NEVER_MOVE)) *moves -= 2;

	for (i = 0; i < RSF_MAX; i++)
	{
		if (!rsf_has(mrace->spell_flags, i)) continue;
		mspell = monster_spell_by_index(i);
		if (calc_monster_spell(spell_counts, mspell))
			numcounts++;
	}

	if (numcounts > 0) {
		totalbonus = mrace->spell_power * (2 + numcounts) / (5 + numcounts);
		for (i = 0; i < PP_MAX; i++)
		{
			state->powers[i] += spell_counts[i] * totalbonus / numcounts;
		}
	}

	mem_free(spell_counts);
}

/**
 * Calculate the players current "state", taking into account
 * not only race/class intrinsics, but also objects being worn
 * and temporary spell effects.
 *
 * See also calc_mana() and calc_hitpoints().
 *
 * Take note of the new "speed code", in particular, a very strong
 * player will start slowing down as soon as he reaches 150 pounds,
 * but not until he reaches 450 pounds will he be half as fast as
 * a normal kobold.  This both hurts and helps the player, hurts
 * because in the old days a player could just avoid 300 pounds,
 * and helps because now carrying 300 pounds is not very painful.
 *
 * The "weapon" and "bow" do *not* add to the bonuses to hit or to
 * damage, since that would affect non-combat things.  These values
 * are actually added in later, at the appropriate place.
 *
 * If known_only is true, calc_bonuses() will only use the known
 * information of objects; thus it returns what the player _knows_
 * the character state to be.
 */
void calc_bonuses(struct player *p, struct player_state *state, bool known_only,
				  bool update)
{
	int i, j, hold;
	int extra_blows = 0;
	int extra_shots = 0;
	int extra_might = 0;
	int extra_moves = 0;
	int armwgt = 0;
	int attacknum;
	struct object *launcher = NULL;
	struct object *weapon[PY_MAX_ATTACKS] = { 0 };
	bitflag f[OF_SIZE];
	bitflag collect_f[OF_SIZE];
	bool vuln[ELEM_MAX];
	struct monster_race *mrace = lookup_player_monster(p);

	/* Hack to allow calculating hypothetical blows for extra STR, DEX - NRM */
	int str_ind = state->stat_ind[STAT_STR];
	int dex_ind = state->stat_ind[STAT_DEX];

	/* Reset */
	memset(state, 0, sizeof *state);

	/* Set various defaults */
	state->speed = 110;
	state->num_blows = 100;

	/* Extract race/class info */
	state->see_infra = p->race->infra;
	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] = p->race->r_skills[i]	+ player_class_c_skill(p, i);
	}
	for (i = 0; i < ELEM_MAX; i++) {
		vuln[i] = false;
		if (p->race->el_info[i].res_level == -1) {
			vuln[i] = true;
		} else {
			state->el_info[i].res_level = p->race->el_info[i].res_level;
		}
	}

	/* Base pflags */
	pf_wipe(state->pflags);
	pf_copy(state->pflags, p->race->pflags);
	pf_union(state->pflags, p->class->pflags);

	/* Extract the player flags */
	player_flags(p, collect_f);

	/* L: get powers */
	for (i = 0; i < PP_MAX; i++) {
		int scale = player_class_power(p, i) + p->race->r_powers[i];
		int minlev = 5 - (scale + 5) / 7;
		int efflev = minlev < 0 ? MAX((p->lev + 1) / 2 - minlev    , p->lev) :
								  MIN((p->lev + 1) * 2 - minlev * 2, p->lev);

		if ((scale <= 0) || (efflev <= 0))
			state->powers[i] = 0;

		else if (p->lev > 50)
			state->powers[i] = (p->lev * scale + 99) / 100;

		else
			state->powers[i] = (efflev * scale + 99) / 100;

		state->powers[i] += MIN(p->extra_powers[i] / 2, p->lev * 3);
	}

	calc_extra_points(p, state);

	/* Analyze equipment */
	for (i = 0; i < p->body.count; i++) {
		int index = 0;
		struct object *obj = slot_object(p, i);
		struct curse_data *curse = obj ? obj->curses : NULL;
		bool obj_is_curse = false;

		while (obj) {
			int dig = 0;
			int owgt = object_weight_one(obj);

			/* L: track armour weight */
			if (slot_type_is(p, i, EQUIP_BODY_ARMOR))
			    armwgt = MAX(armwgt, owgt);

			if (slot_type_is(p, i, EQUIP_WEAPON) && obj->tval != TV_SHIELD && !obj_is_curse) {
				for (j = 0; j < PY_MAX_ATTACKS; j++) {
					if (!weapon[j]) {
						weapon[j] = obj;
						break;
					}
				}
			}

			if (!launcher && slot_type_is(p, i, EQUIP_BOW))
				launcher = obj;

			/* Extract the item flags */
			if (known_only) {
				object_flags_known(obj, f);
			} else {
				object_flags(obj, f);
			}
			of_union(collect_f, f);

			/* Apply modifiers */
			state->stat_add[STAT_STR] += obj->modifiers[OBJ_MOD_STR]
				* p->obj_k->modifiers[OBJ_MOD_STR];
			state->stat_add[STAT_INT] += obj->modifiers[OBJ_MOD_INT]
				* p->obj_k->modifiers[OBJ_MOD_INT];
			state->stat_add[STAT_WIS] += obj->modifiers[OBJ_MOD_WIS]
				* p->obj_k->modifiers[OBJ_MOD_WIS];
			state->stat_add[STAT_DEX] += obj->modifiers[OBJ_MOD_DEX]
				* p->obj_k->modifiers[OBJ_MOD_DEX];
			state->stat_add[STAT_CON] += obj->modifiers[OBJ_MOD_CON]
				* p->obj_k->modifiers[OBJ_MOD_CON];
			state->skills[SKILL_STEALTH] += obj->modifiers[OBJ_MOD_STEALTH]
				* p->obj_k->modifiers[OBJ_MOD_STEALTH];
			state->skills[SKILL_SEARCH] += (obj->modifiers[OBJ_MOD_SEARCH] * 5)
				* p->obj_k->modifiers[OBJ_MOD_SEARCH];

			state->see_infra += obj->modifiers[OBJ_MOD_INFRA]
				* p->obj_k->modifiers[OBJ_MOD_INFRA];
			if (tval_is_digger(obj)) {
				if (of_has(obj->flags, OF_DIG_1))
					dig = 1;
				else if (of_has(obj->flags, OF_DIG_2))
					dig = 2;
				else if (of_has(obj->flags, OF_DIG_3))
					dig = 3;
			}
			dig += obj->modifiers[OBJ_MOD_TUNNEL]
				* p->obj_k->modifiers[OBJ_MOD_TUNNEL];
			state->skills[SKILL_DIGGING] += (dig * 20);
			state->speed += obj->modifiers[OBJ_MOD_SPEED]
				* p->obj_k->modifiers[OBJ_MOD_SPEED];
			state->dam_red += obj->modifiers[OBJ_MOD_DAM_RED]
				* p->obj_k->modifiers[OBJ_MOD_DAM_RED];
			extra_blows += obj->modifiers[OBJ_MOD_BLOWS]
				* p->obj_k->modifiers[OBJ_MOD_BLOWS];
			extra_shots += obj->modifiers[OBJ_MOD_SHOTS]
				* p->obj_k->modifiers[OBJ_MOD_SHOTS];
			extra_might += obj->modifiers[OBJ_MOD_MIGHT]
				* p->obj_k->modifiers[OBJ_MOD_MIGHT];
			extra_moves += obj->modifiers[OBJ_MOD_MOVES]
				* p->obj_k->modifiers[OBJ_MOD_MOVES];

			/* Apply element info, noting vulnerabilites for later processing */
			for (j = 0; j < ELEM_MAX; j++) {
				if (!known_only || obj->known->el_info[j].res_level) {
					if (obj->el_info[j].res_level == -1)
						vuln[j] = true;

					/* OK because res_level hasn't included vulnerability yet */
					if (obj->el_info[j].res_level > state->el_info[j].res_level)
						state->el_info[j].res_level = obj->el_info[j].res_level;
				}
			}

			/* Apply combat bonuses */
			state->ac += obj->ac;
			if (!known_only || obj->known->to_a)
				state->to_a += obj->to_a;
			if (!slot_type_is(p, i, EQUIP_WEAPON)
					&& !slot_type_is(p, i, EQUIP_BOW)) {
				if (!known_only || obj->known->to_h) {
					state->to_h += obj->to_h;
				}
				if (!known_only || obj->known->to_d) {
					state->to_d += obj->to_d;
				}
			}

			/* Move to any unprocessed curse object */
			if (curse) {
				index++;
				obj = NULL;
				obj_is_curse = true;
				while (index < z_info->curse_max) {
					if (curse[index].power) {
						obj = curses[index].obj;
						break;
					} else {
						index++;
					}
				}
			} else {
				obj = NULL;
			}
		}
	}

	/* Apply the collected flags */
	of_union(state->flags, collect_f);

	/* Add shapechange info */
	calc_shapechange(state, vuln, p->shape, &extra_blows, &extra_shots,
		&extra_might, &extra_moves);

	/* L: add monster info */
	if (mrace) {
		calc_monster(state, vuln, mrace, &extra_moves);
	}

	/* Now deal with vulnerabilities */
	for (i = 0; i < ELEM_MAX; i++) {
		if (vuln[i] && (state->el_info[i].res_level < 3))
			state->el_info[i].res_level--;
	}

	/* Calculate light */
	calc_light(p, state, update);

	/* Unlight - needs change if anything but resist is introduced for dark */
	if (pf_has(state->pflags, PF_UNLIGHT) && character_dungeon) {
		state->el_info[ELEM_DARK].res_level = 1;
	}

	/* Evil */
	if (pf_has(state->pflags, PF_EVIL) && character_dungeon) {
		state->el_info[ELEM_NETHER].res_level = 1;
		state->el_info[ELEM_HOLY_ORB].res_level = -1;
	}

	/* Calculate the various stat values */
	for (i = 0; i < STAT_MAX; i++) {
		int add, use, ind;

        /* L: Class doesn't affect stats any more, race affects them elsewhere */
		add = state->stat_add[i];
		state->stat_top[i] = modify_stat_value(p->stat_max[i], add);
		use = modify_stat_value(p->stat_cur[i], add);

		state->stat_use[i] = use;

		if (use <= 3) {/* Values: n/a */
			ind = 0;
		} else if (use <= 18) {/* Values: 3, 4, ..., 18 */
			ind = (use - 3);
		} else if (use <= 18+219) {/* Ranges: 18/00-18/09, ..., 18/210-18/219 */
			ind = (15 + (use - 18) / 10);
		} else {/* Range: 18/220+ */
			ind = (37);
		}

		assert((0 <= ind) && (ind < STAT_RANGE));

		/* Hack for hypothetical blows - NRM */
		if (!update) {
			if (i == STAT_STR) {
				ind += str_ind;
				ind = MIN(ind, 37);
				ind = MAX(ind, 3);
			} else if (i == STAT_DEX) {
				ind += dex_ind;
				ind = MIN(ind, 37);
				ind = MAX(ind, 3);
			}
		}

		/* Save the new index */
		state->stat_ind[i] = ind;
	}

	/* Effects of food outside the "Fed" range */
	if (!player_timed_grade_eq(p, TMD_FOOD, "Fed")) {
		int excess = p->timed[TMD_FOOD] - PY_FOOD_FULL;
		int lack = PY_FOOD_HUNGRY - p->timed[TMD_FOOD];
		if ((excess > 0) && !p->timed[TMD_ATT_VAMP]) {
			/* Scale to units 1/10 of the range and subtract from speed */
			excess = (excess * 10) / (PY_FOOD_MAX - PY_FOOD_FULL);
			state->speed -= excess;
		} else if (lack > 0) {
			/* Scale to units 1/20 of the range */
			lack = (lack * 20) / PY_FOOD_HUNGRY;

			/* Apply effects progressively */
			state->to_h -= lack;
			state->to_d -= lack;
			if ((lack > 10) && (lack <= 15)) {
				adjust_skill_scale(&state->skills[SKILL_DEVICE],
					-1, 10, 0);
			} else if ((lack > 15) && (lack <= 18)) {
				adjust_skill_scale(&state->skills[SKILL_DEVICE],
					-1, 5, 0);
				state->skills[SKILL_DISARM_PHYS] *= 9;
				state->skills[SKILL_DISARM_PHYS] /= 10;
				state->skills[SKILL_DISARM_MAGIC] *= 9;
				state->skills[SKILL_DISARM_MAGIC] /= 10;
			} else if (lack > 18) {
				adjust_skill_scale(&state->skills[SKILL_DEVICE],
					-3, 10, 0);
				state->skills[SKILL_DISARM_PHYS] *= 8;
				state->skills[SKILL_DISARM_PHYS] /= 10;
				state->skills[SKILL_DISARM_MAGIC] *= 8;
				state->skills[SKILL_DISARM_MAGIC] /= 10;
				state->skills[SKILL_SAVE] *= 9;
				state->skills[SKILL_SAVE] /= 10;
				state->skills[SKILL_SEARCH] *= 9;
				state->skills[SKILL_SEARCH] /= 10;
			}
		}
	}

	/* L: monk bonuses */
	unarmoured_speed_bonus(state, armwgt);
	unarmoured_ac_bonus(state, armwgt);

	/* Other timed effects */
	player_flags_timed(p, state->flags);

	if (player_timed_grade_eq(p, TMD_STUN, "Heavy Stun")) {
		state->to_h -= 20;
		state->to_d -= 20;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
		if (update) {
			p->timed[TMD_FASTCAST] = 0;
		}
	} else if (player_timed_grade_eq(p, TMD_STUN, "Stun")) {
		state->to_h -= 5;
		state->to_d -= 5;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
		if (update) {
			p->timed[TMD_FASTCAST] = 0;
		}
	}
	if (p->timed[TMD_INVULN]) {
		state->to_a += 100;
	}
	if (p->timed[TMD_BLESSED]) {
		state->to_a += 5;
		state->to_h += 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (p->timed[TMD_SHIELD]) {
		state->to_a += 50;
	}
	if (p->timed[TMD_STONESKIN]) {
		state->to_a += 40;
		state->speed -= 5;
	}
	if (p->timed[TMD_HERO]) {
		state->to_h += 12;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (p->timed[TMD_SHERO]) {
		state->skills[SKILL_TO_HIT_MELEE] += 75;
		state->to_a -= 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
	}
	if (p->timed[TMD_FAST] || p->timed[TMD_SPRINT]) {
		state->speed += 10;
	}
	if (p->timed[TMD_SLOW]) {
		state->speed -= 10;
	}
	if (p->timed[TMD_SINFRA]) {
		state->see_infra += 5;
	}
	if (p->timed[TMD_TERROR]) {
		state->speed += 10;
	}
	for (i = 0; i < TMD_MAX; ++i) {
		if (p->timed[i] && timed_effects[i].temp_resist != -1
				&& state->el_info[timed_effects[i].temp_resist].res_level
				< 2) {
			state->el_info[timed_effects[i].temp_resist].res_level++;
		}
	}
	if (p->timed[TMD_CONFUSED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 4, 0);
	}
	if (p->timed[TMD_AMNESIA]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (p->timed[TMD_POISONED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}
	if (p->timed[TMD_IMAGE]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (p->timed[TMD_BLOODLUST]) {
		state->to_d += p->timed[TMD_BLOODLUST] / 2;
		extra_blows += p->timed[TMD_BLOODLUST] / 20;
	}
	if (p->timed[TMD_STEALTH]) {
		state->skills[SKILL_STEALTH] += 10;
	}

	/* Analyze flags - check for fear */
	if (of_has(state->flags, OF_AFRAID)) {
		state->to_h -= 20;
		state->to_a += 8;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}

	/* Analyze weight */
	j = p->upkeep->total_weight;
	i = weight_limit(state);
	if (j > i / 2)
		state->speed -= ((j - (i / 2)) / (i / 10));
	if (state->speed < 0)
		state->speed = 0;
	if (state->speed > 199)
		state->speed = 199;

	/* Apply modifier bonuses (Un-inflate stat bonuses) */
	state->to_a += adj_dex_ta(state->stat_ind[STAT_DEX]);

	/* L: change expfact based on int */
	state->expfact = p->race->r_exp + p->class->c_exp + adj_int_xp(state->stat_ind[STAT_INT]);

	/* Modify skills */
	state->skills[SKILL_DISARM_PHYS] += adj_dex_dis(state->stat_ind[STAT_DEX]);
	state->skills[SKILL_DISARM_MAGIC] += adj_int_dis(state->stat_ind[STAT_INT]);
	state->skills[SKILL_DEVICE] += adj_int_dev(state->stat_ind[STAT_INT]);
	state->skills[SKILL_SAVE] += adj_wis_sav(state->stat_ind[STAT_WIS]);
	state->skills[SKILL_DIGGING] += adj_str_dig(state->stat_ind[STAT_STR]);
	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] += (player_class_x_skill(p, i) * p->lev / 10);
	}

	/* L: add extra skills */
	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] += MIN(p->extra_skills[i], p->lev * 3);
	}

	if (state->skills[SKILL_DIGGING] < 1) state->skills[SKILL_DIGGING] = 1;
	if (state->skills[SKILL_STEALTH] > 150) state->skills[SKILL_STEALTH] = 150;
	if (state->skills[SKILL_STEALTH] < 0) state->skills[SKILL_STEALTH] = 0;
	hold = adj_str_hold(state->stat_ind[STAT_STR]);

	/* L: magic gets a special bonus from its ability score */
	if (state->skills[SKILL_MAGIC] > 0) {
		int stat = get_player_realm(p)->stat;
		int adj = adj_mag_stat(state->stat_ind[stat]);
		state->skills[SKILL_MAGIC] += MIN(adj, state->skills[SKILL_MAGIC]);
	} else {
		state->skills[SKILL_MAGIC] = 0;
	}

	/* Analyze launcher */
	state->heavy_shoot = false;
	if (launcher) {
		int16_t launcher_weight = object_weight_one(launcher);

		if (hold < launcher_weight / 10) {
			state->to_h += 2 * (hold - launcher_weight / 10);
			state->heavy_shoot = true;
		}

		state->num_shots = 10;

		/* Type of ammo */
		if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_SHOTS))
			state->ammo_tval = TV_SHOT;
		else if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_ARROWS))
			state->ammo_tval = TV_ARROW;
		else if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_BOLTS))
			state->ammo_tval = TV_BOLT;

		/* Multiplier */
		state->ammo_mult = launcher->pval;

		/* Apply special flags */
		if (!state->heavy_shoot) {
			state->num_shots += extra_shots;
			state->ammo_mult += extra_might;
			if (pf_has(state->pflags, PF_FAST_SHOT)) {
				state->num_shots += p->lev / 3;
			}
		}

		/* Require at least one shot */
		if (state->num_shots < 10) state->num_shots = 10;
	}

	/* Analyze weapon */
	state->heavy_wield = false;
	state->bless_wield = false;
	if (weapon[0]) {
		int16_t weapon_weight = 0;
		bool all_hafted = true;
		bool any_hafted = false;
		for (i = 0; weapon[i]; i++) {
			int currwgt = object_weight_one(weapon[i]);
			if (weapon[i]->tval == TV_HAFTED) {
				any_hafted = true;
			}
			else {
				all_hafted = false;
			}
			weapon_weight = MAX(weapon_weight, currwgt);
		}

		/* It is hard to hold a heavy weapon */
		if (hold < weapon_weight / 10) {
			state->to_h += 2 * (hold - weapon_weight / 10);
			state->heavy_wield = true;
		}

		/* Normal weapons */
		if (!state->heavy_wield) {
			//state->num_blows = calc_blows(p, weapon_weight, state, extra_blows);
			state->skills[SKILL_DIGGING] += weapon_weight / 10;
		}

		/* Divine weapon bonus for blessed weapons */
		if (pf_has(state->pflags, PF_BLESS_WEAPON)
				&& ((any_hafted && all_hafted)
				|| of_has(state->flags, OF_BLESSED))) {
			state->to_h += 2;
			state->to_d += 2;
			state->bless_wield = true;
		}
	}

	/* L: get melee attacks */
	for (attacknum = 0; (attacknum < PY_MAX_ATTACKS - 1) && (weapon[attacknum]); ++attacknum) {
		state->attacks[attacknum] = get_melee_weapon_attack(p, state, weapon[attacknum]);
	}
	if (mrace) {
		attacknum += get_monster_attacks(p, state, mrace,
										 &state->attacks[attacknum],
										 PY_MAX_ATTACKS - attacknum);
	}
	if (!attacknum) {
		state->attacks[attacknum] = get_melee_weapon_attack(p, state, NULL);
		++attacknum;
	}
	state->num_attacks = attacknum;

	if (launcher) {
		state->ranged_attack = get_shooter_weapon_attack(p, state, launcher);
		calc_blows(p, launcher->weight, &state->ranged_attack, state, extra_shots);
	}

	/* L: give attacks blow numbers */
	for (i = 0; i < attacknum; i++) {
		if (state->heavy_wield) state->attacks[i].blows = 100;
		else {
			const struct object *obj = state->attacks[i].obj;
			int wgt = obj ? object_weight_one(obj) : 0;
			calc_blows(p, wgt, &state->attacks[i], state, extra_blows + attacknum);
		}
	}

	/* Mana */
	calc_mana(p, state, update);
	if (!p->msp) {
		pf_on(state->pflags, PF_NO_MANA);
	}

	/* Movement speed */
	state->num_moves = extra_moves;

	return;
}

/**
 * Calculate bonuses, and print various things on changes.
 */
static void update_bonuses(struct player *p)
{
	int i;

	struct player_state state = p->state;
	struct player_state known_state = p->known_state;


	/* ------------------------------------
	 * Calculate bonuses
	 * ------------------------------------ */

	calc_bonuses(p, &state, false, true);
	calc_bonuses(p, &known_state, true, true);


	/* ------------------------------------
	 * Notice changes
	 * ------------------------------------ */

	/* Analyze stats */
	for (i = 0; i < STAT_MAX; i++) {
		/* Notice changes */
		if (state.stat_top[i] != p->state.stat_top[i])
			/* Redisplay the stats later */
			p->upkeep->redraw |= (PR_STATS);

		/* Notice changes */
		if (state.stat_use[i] != p->state.stat_use[i])
			/* Redisplay the stats later */
			p->upkeep->redraw |= (PR_STATS);

		/* Notice changes */
		if (state.stat_ind[i] != p->state.stat_ind[i]) {
			/* Change in CON affects Hitpoints */
			if (i == STAT_CON)
				p->upkeep->update |= (PU_HP);

			/* Change in stats may affect Mana/Spells */
			p->upkeep->update |= (PU_MANA | PU_SPELLS);
		}
	}

	/* L: update exp if needed */
	if (state.expfact != p->state.expfact)
		p->upkeep->redraw |= PR_EXP;

	// L: redraw status if learning ability changed
	if (state.extra_points_max != p->state.extra_points_max ||
			state.extra_points_used != p->state.extra_points_used) {
		p->upkeep->redraw |= PR_STATUS;
	}


	/* Hack -- Telepathy Change */
	if (of_has(state.flags, OF_TELEPATHY) !=
		of_has(p->state.flags, OF_TELEPATHY))
		/* Update monster visibility */
		p->upkeep->update |= (PU_MONSTERS);
	/* Hack -- See Invis Change */
	if (of_has(state.flags, OF_SEE_INVIS) !=
		of_has(p->state.flags, OF_SEE_INVIS))
		/* Update monster visibility */
		p->upkeep->update |= (PU_MONSTERS);

	/* Redraw speed (if needed) */
	if (state.speed != p->state.speed)
		p->upkeep->redraw |= (PR_SPEED);

	/* Redraw armor (if needed) */
	if ((known_state.ac != p->known_state.ac) || 
		(known_state.to_a != p->known_state.to_a))
		p->upkeep->redraw |= (PR_ARMOR);

	/* Notice changes in the "light radius" */
	if (p->state.cur_light != state.cur_light) {
		/* Update the visuals */
		p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}

	/* Notice changes to the weight limit. */
	if (weight_limit(&p->state) != weight_limit(&state)) {
		p->upkeep->redraw |= (PR_INVEN);
	}

	/* Hack -- handle partial mode */
	if (!p->upkeep->only_partial) {
		/* Take note when "heavy bow" changes */
		if (p->state.heavy_shoot != state.heavy_shoot) {
			/* Message */
			if (state.heavy_shoot)
				msg("You have trouble wielding such a heavy bow.");
			else if (slot_object(p, slot_by_type(p, EQUIP_BOW, true)))
				msg("You have no trouble wielding your bow.");
			else
				msg("You feel relieved to put down your heavy bow.");
		}

		/* Take note when "heavy weapon" changes */
		if (p->state.heavy_wield != state.heavy_wield) {
			/* Message */
			if (state.heavy_wield)
				msg("You have trouble wielding such a heavy weapon.");
			else if (slot_object(p, slot_by_type(p, EQUIP_WEAPON, true)))
				msg("You have no trouble wielding your weapon.");
			else
				msg("You feel relieved to put down your heavy weapon.");	
		}

		/* Take note when "illegal weapon" changes */
		if (p->state.bless_wield != state.bless_wield) {
			/* Message */
			if (state.bless_wield) {
				msg("You feel attuned to your weapon.");
			} else if (slot_object(p, slot_by_type(p, EQUIP_WEAPON, true))) {
				msg("You feel less attuned to your weapon.");
			}
		}

		/* Take note when "armor state" changes */
		if (p->state.cumber_armor != state.cumber_armor) {
			/* Message */
			if (state.cumber_armor)
				msg("The weight of your armor encumbers your movement.");
			else
				msg("You feel able to move more freely.");
		}
	}

	memcpy(&p->state, &state, sizeof(state));
	memcpy(&p->known_state, &known_state, sizeof(known_state));
}




/**
 * ------------------------------------------------------------------------
 * Monster and object tracking functions
 * ------------------------------------------------------------------------ */

/**
 * Track the given monster
 */
void health_track(struct player_upkeep *upkeep, struct monster *mon)
{
	upkeep->health_who = mon;
	upkeep->redraw |= PR_HEALTH;
}

/**
 * Track the given monster race
 */
void monster_race_track(struct player_upkeep *upkeep, struct monster_race *race)
{
	/* Save this monster ID */
	upkeep->monster_race = race;

	/* Window stuff */
	upkeep->redraw |= (PR_MONSTER);
}

/**
 * Track the given object
 */
void track_object(struct player_upkeep *upkeep, struct object *obj)
{
	upkeep->object = obj;
	upkeep->object_kind = NULL;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Track the given object kind
 */
void track_object_kind(struct player_upkeep *upkeep, struct object_kind *kind)
{
	upkeep->object = NULL;
	upkeep->object_kind = kind;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Cancel all object tracking
 */
void track_object_cancel(struct player_upkeep *upkeep)
{
	upkeep->object = NULL;
	upkeep->object_kind = NULL;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Is the given item tracked?
 */
bool tracked_object_is(struct player_upkeep *upkeep, struct object *obj)
{
	return (upkeep->object == obj);
}



/**
 * ------------------------------------------------------------------------
 * Generic "deal with" functions
 * ------------------------------------------------------------------------ */

/**
 * Handle "player->upkeep->notice"
 */
void notice_stuff(struct player *p)
{
	/* Notice stuff */
	if (!p->upkeep->notice) return;

	/* Deal with ignore stuff */
	if (p->upkeep->notice & PN_IGNORE) {
		p->upkeep->notice &= ~(PN_IGNORE);
		ignore_drop(p);
	}

	/* Combine the pack */
	if (p->upkeep->notice & PN_COMBINE) {
		p->upkeep->notice &= ~(PN_COMBINE);
		combine_pack(p);
	}

	/* Dump the monster messages */
	if (p->upkeep->notice & PN_MON_MESSAGE) {
		p->upkeep->notice &= ~(PN_MON_MESSAGE);

		/* Make sure this comes after all of the monster messages */
		show_monster_messages();
	}
}

/**
 * Handle "player->upkeep->update"
 */
void update_stuff(struct player *p)
{
	/* Update stuff */
	if (!p->upkeep->update) return;


	if (p->upkeep->update & (PU_INVEN)) {
		p->upkeep->update &= ~(PU_INVEN);
		calc_inventory(p);
	}

	if (p->upkeep->update & (PU_BONUS)) {
		p->upkeep->update &= ~(PU_BONUS);
		update_bonuses(p);
	}

	if (p->upkeep->update & (PU_TORCH)) {
		p->upkeep->update &= ~(PU_TORCH);
		calc_light(p, &p->state, true);
	}

	if (p->upkeep->update & (PU_HP)) {
		p->upkeep->update &= ~(PU_HP);
		calc_hitpoints(p);
	}

	if (p->upkeep->update & (PU_MANA)) {
		p->upkeep->update &= ~(PU_MANA);
		calc_mana(p, &p->state, true);
	}

	if (p->upkeep->update & (PU_SPELLS)) {
		p->upkeep->update &= ~(PU_SPELLS);
		calc_spells(p);
	}

	/* Character is not ready yet, no map updates */
	if (!character_generated) return;

	/* Map is not shown, no map updates */
	if (!map_is_visible()) return;

	if (p->upkeep->update & (PU_UPDATE_VIEW)) {
		p->upkeep->update &= ~(PU_UPDATE_VIEW);
		update_view(cave, p);
	}

	if (p->upkeep->update & (PU_DISTANCE)) {
		p->upkeep->update &= ~(PU_DISTANCE);
		p->upkeep->update &= ~(PU_MONSTERS);
		update_monsters(true);
	}

	if (p->upkeep->update & (PU_MONSTERS)) {
		p->upkeep->update &= ~(PU_MONSTERS);
		update_monsters(false);
	}


	if (p->upkeep->update & (PU_PANEL)) {
		p->upkeep->update &= ~(PU_PANEL);
		event_signal(EVENT_PLAYERMOVED);
	}
}



struct flag_event_trigger
{
	uint32_t flag;
	game_event_type event;
};



/**
 * Events triggered by the various flags.
 */
static const struct flag_event_trigger redraw_events[] =
{
	{ PR_MISC,    EVENT_RACE_CLASS },
	{ PR_TITLE,   EVENT_PLAYERTITLE },
	{ PR_LEV,     EVENT_PLAYERLEVEL },
	{ PR_EXP,     EVENT_EXPERIENCE },
	{ PR_STATS,   EVENT_STATS },
	{ PR_ARMOR,   EVENT_AC },
	{ PR_HP,      EVENT_HP },
	{ PR_MANA,    EVENT_MANA },
	{ PR_GOLD,    EVENT_GOLD },
	{ PR_HEALTH,  EVENT_MONSTERHEALTH },
	{ PR_DEPTH,   EVENT_DUNGEONLEVEL },
	{ PR_SPEED,   EVENT_PLAYERSPEED },
	{ PR_STATE,   EVENT_STATE },
	{ PR_STATUS,  EVENT_STATUS },
	{ PR_STUDY,   EVENT_STUDYSTATUS },
	{ PR_DTRAP,   EVENT_DETECTIONSTATUS },
	{ PR_FEELING, EVENT_FEELING },
	{ PR_LIGHT,   EVENT_LIGHT },

	{ PR_INVEN,   EVENT_INVENTORY },
	{ PR_EQUIP,   EVENT_EQUIPMENT },
	{ PR_MONLIST, EVENT_MONSTERLIST },
	{ PR_ITEMLIST, EVENT_ITEMLIST },
	{ PR_MONSTER, EVENT_MONSTERTARGET },
	{ PR_OBJECT, EVENT_OBJECTTARGET },
	{ PR_MESSAGE, EVENT_MESSAGE },
};

/**
 * Handle "player->upkeep->redraw"
 */
void redraw_stuff(struct player *p)
{
	size_t i;
	uint32_t redraw = p->upkeep->redraw;

	/* Redraw stuff */
	if (!redraw) return;

	/* Character is not ready yet, no screen updates */
	if (!character_generated) return;

	/* Map is not shown, subwindow updates only */
	if (!map_is_visible()) 
		redraw &= PR_SUBWINDOW;

	/* Hack - rarely update while resting or running, makes it over quicker */
	if (((player_resting_count(p) % 100) || (p->upkeep->running % 100))
		&& !(redraw & (PR_MESSAGE | PR_MAP)))
		return;

	/* For each listed flag, send the appropriate signal to the UI */
	for (i = 0; i < N_ELEMENTS(redraw_events); i++) {
		const struct flag_event_trigger *hnd = &redraw_events[i];

		if (redraw & hnd->flag)
			event_signal(hnd->event);
	}

	/* Then the ones that require parameters to be supplied. */
	if (redraw & PR_MAP) {
		/* Mark the whole map to be redrawn */
		event_signal_point(EVENT_MAP, -1, -1);
	}

	p->upkeep->redraw &= ~redraw;

	/* Map is not shown, subwindow updates only */
	if (!map_is_visible()) return;

	/*
	 * Do any plotting, etc. delayed from earlier - this set of updates
	 * is over.
	 */
	event_signal(EVENT_END);
}


/**
 * Handle "player->upkeep->update" and "player->upkeep->redraw"
 */
void handle_stuff(struct player *p)
{
	if (p->upkeep->update) update_stuff(p);
	if (p->upkeep->redraw) redraw_stuff(p);
}

