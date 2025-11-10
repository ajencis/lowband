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
#include "mon-calcs.h"
#include "mon-msg.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-enum.h"
#include "player-properties.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "player.h"
#include "project.h"


struct mon_player_match elem_pp_matches[] = {
	{ ELEM_ACID, PP_EARTH_MAGIC },
	{ ELEM_COLD, PP_WATER_MAGIC },
	{ ELEM_DARK, PP_NECROMANCY_MAGIC },
	{ ELEM_ELEC, PP_AIR_MAGIC },
	{ ELEM_FIRE, PP_FIRE_MAGIC },
	{ ELEM_FORCE, PP_EARTH_MAGIC },
	{ ELEM_ICE, PP_WATER_MAGIC },
	{ ELEM_NETHER, PP_NECROMANCY_MAGIC },
	{ ELEM_NEXUS, PP_TELEPORTATION_MAGIC },
	{ ELEM_PLASMA, PP_FIRE_MAGIC },
	{ ELEM_POIS, PP_POISON_MAGIC },
	{ ELEM_SHARD, PP_EARTH_MAGIC },
	{ ELEM_WATER, PP_WATER_MAGIC },
	{ ELEM_HOLY_FIRE, PP_HOLY_MAGIC },
	{ ELEM_HELLFIRE, PP_FIRE_MAGIC },
	{ -1, -1 }
};

struct mon_player_match proj_pp_matches[] = {
	{ PROJ_ARROW, PP_BOW_SPECIALIZATION },
	{ PROJ_BANSHEE, PP_NECROMANCY_MAGIC },
	{ PROJ_DISP_EVIL, PP_HOLY_MAGIC },
	{ PROJ_DISP_UNDEAD, PP_HOLY_MAGIC },
	{ PROJ_MON_CONF, PP_ENCHANTMENT_MAGIC },
	{ PROJ_MON_DRAIN, PP_NECROMANCY_MAGIC },
	{ PROJ_MON_POIS, PP_POISON_MAGIC },
	{ PROJ_SLEEP_ALL, PP_ENCHANTMENT_MAGIC },
	{ PROJ_TURN_EVIL, PP_HOLY_MAGIC },
	{ PROJ_TURN_UNDEAD, PP_HOLY_MAGIC },
	{ PROJ_VAMPIRE, PP_NECROMANCY_MAGIC },
	{ -1, -1 }
};

struct skill_stat_info skill_stats[] = {
	#define SKILL(x, a, b, c, d, e) { SKILL_##x, c, d },
	#include "list-skills.h"
	#undef SKILL
	{ -1, STAT_NONE, STAT_NONE }
};



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
    int hsi = HI_STAT_IND;
	int asi = AVG_STAT_IND;
	int lsi = LOW_STAT_IND;
	assert(hsi >= asi);
	index = MAX(index, lsi);

	int negative = ((asi - lsi) * scaleto + 24) / 25;

	if (index >= asi) return (int)(scaleto * 
	        ((index - asi) * my_sqrt(index - asi)) /
			((hsi - asi) * my_sqrt(hsi - asi))) + 
			(minzero ? negative : 0);

	if (index <= asi) return ((index - asi) * scaleto - 24) / 25 + (minzero ? negative : 0);

	return minzero ? negative : 0;
}



int adj_dex_ta(int index) {
	return stat_scale(index, 15, false);
}

int adj_str_td(int index) {
	return (index - 7) * 15 / 8;
}

int adj_dex_th(int index) {
	return (index - 7) * 35 / 8;
}

static int adj_str_wgt(int index) {
	return stat_scale(index, 250, true) + 25;
}

int adj_str_hold(int index) {
	return stat_scale(index, 250, true) + 100;
}

int adj_str_blow(int index) {
	return stat_scale(index, 240, true);
}


int adj_stat_blow(int index) {
	if (index < AVG_STAT_IND) {
		index = (index + AVG_STAT_IND) / 2;
	}
	return index * 600 / 14;
}

int adj_dex_safe(int index) {
	return stat_scale(index, 80, true);
}

int adj_con_fix(int index) {
	return stat_scale(index, 10, true);
}

static int adj_mag_study(int index) {
	return (index + 5) * 10 / 20;
}

int adj_int_xp(int index) {
	return -stat_scale(index, 50, false);
}

int adj_int_lev(int index) {
	return 0;
	//return stat_scale(index, 6, false);
}

int adj_int_tome(int index) {
	return stat_scale(index, 10, false);
}

int adj_mag_stat(int index) {
	return index - 7;
}

int adj_str_web(int index) {
	return stat_scale(index, 50, true) + 5;
}

int adj_stat_skill_flat(int index, int skill) {
	int ret;
	if (skill == SKILL_MAGIC) {
		ret = index > 7 ? my_sqrt(index - 7) * 5 : index - 7;
	}
	else {
		ret = stat_scale(index, 20, false);
	}
	return MAX(0, ret);
}

int adj_stat_skill_percent(int index, int skill) {
	int ret;
	if (skill == SKILL_MAGIC) {
		ret = index > 7 ? my_sqrt(index - 7) * 10 : index - 7;
	}
	else {
		ret = stat_scale(index, 30, false);
	}
	return ret;
}

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
		if ((player->mon.state.ammo_tval == orig->tval) &&
			(player->mon.state.ammo_tval != new->tval))
			return false;
		if ((player->mon.state.ammo_tval != orig->tval) &&
			(player->mon.state.ammo_tval == new->tval))
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

	// L: tomes sort by display name
	if (orig->tval == TV_TOME) {
		const char *pnameo = get_obj_power_name(orig);
		const char *pnamen = get_obj_power_name(new);
		if (!pnamen || !pnameo) {
			return false;
		}
		int compared = my_stricmp(pnameo, pnamen);
		if (compared > 0) return true;
		if (compared < 0) return false;
	}

	/* No preference */
	return false;
}

int equipped_item_slot(struct player_body body, struct object *item)
{
	int i;

	if (item == NULL) return body.count;

	/* Look for an equipment slot with this item */
	for (i = 0; i < body.count; i++) {
		if (item == body.slots[i].obj) break;
	}

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
	int n_pack_remaining = z_info->pack_size - pack_slots_used(&p->mon);
	int n_max = 1 + z_info->pack_size + z_info->quiver_size
		+ p->mon.body.count;
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
	for (current = p->mon.gear, j = 0; current; current = current->next, ++j) {
		assert(j < n_max);
		assigned[j] = object_is_equipped(p->mon.body, current);
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
	for (current = p->mon.gear, j = 0; current; current = current->next, ++j) {
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
					gear_insert_end(&p->mon, object_split(current,
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
		current = p->mon.gear;
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
			gear_insert_end(&p->mon, object_split(first,
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
		current = p->mon.gear;
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
					 && !object_is_equipped(p->mon.body, old_pack[i])) {
				msg("You re-arrange your pack.");
				break;
			}
		}
	}

	mem_free(assigned);
	mem_free(old_pack);
	mem_free(old_quiver);
}

/**
 * Calculate number of spells player should have, and forget,
 * or remember, spells until that number is properly reflected.
 *
 * Note that this function induces various "status" messages,
 * which must be bypasses until the character is created.
 */
static void calc_spells(struct player *p)
{
	int i, j, k;
	int num_allowed, num_known;
	int lev = p->mon.state.skills[SKILL_MAGIC];
	const struct magic_realm *realm = get_player_realm(p);
	const struct player_spell *spell;
	int16_t old_spells;

	/* Hack -- wait for creation */
	if (!character_generated) return;

	/* Hack -- handle partial mode */
	if (p->upkeep->only_partial) return;

	// L: no magic, no spells
	if (lev <= 0) return;

	// L: no realm, no spells
	if (!realm) return;

	/* Save the new_spells value */
	old_spells = p->upkeep->new_spells;

	/* Number of 1/100 spells per level (or something - needs clarifying) */
	num_allowed = adj_mag_study(p->mon.state.stat_ind[realm->stat]) * lev / 100 + 3;

	if (realm->realm_special[RLM_SPCL_SPELLS_KNOWN]) {
		int mod = realm->realm_special[RLM_SPCL_SPELLS_KNOWN] + 100;
		num_allowed = (num_allowed * mod + 99) / 100;
	}

	/* Assume none known */
	num_known = 0;

	/* Count num we know */
	for (j = 0; j < z_info->spell_max; j++) {
		if (p->player_spell_flags[j] & PY_SPELL_LEARNED) {
			++num_known;
		}
	}

	/* See how many spells we must forget or may learn */
	p->upkeep->new_spells = num_allowed - num_known;


	// L: if we're an innate caster and never got any spells known give them now
	if (realm && realm->realm_special[RLM_SPCL_INNATE] && lev >= 3 && num_known == 0) {
		player_learn_spell_xp(p, true, 0);
	}


	// Forget spells which are too hard 
	for (i = z_info->spell_max - 1; i >= 0; i--) {
		// Get the spell
		j = p->player_spell_order[i];

		// Skip non-spells
		if (j >= 99) continue;

		// Get the spell
		spell = player_spell_lookup(j);

		// Skip spells we are allowed to know
		if (gener_spell_power(p, spell) > 0) continue;

		// Is it known?
		if (p->player_spell_flags[j] & PY_SPELL_LEARNED) {
			// Mark as forgotten
			p->player_spell_flags[j] |= PY_SPELL_FORGOTTEN;

			// No longer known
			p->player_spell_flags[j] &= ~PY_SPELL_LEARNED;

			// Message
			msg("You have forgotten the spell of %s.", spell->name);

			// One more can be learned
			p->upkeep->new_spells++;
		}
	}
	

	// Forget spells if we know too many spells
	for (i = z_info->spell_max - 1; i >= 0; i--) {
		// Stop when possible
		if (p->upkeep->new_spells >= 0) break;

		// Get the (i+1)th spell learned
		j = p->player_spell_order[i];

		// Skip unknown spells
		if (j >= 99) continue;

		// Get the spell
		spell = player_spell_lookup(j);

		// Forget it (if learned)
		if (p->player_spell_flags[j] & PY_SPELL_LEARNED) {
			// Mark as forgotten
			p->player_spell_flags[j] |= PY_SPELL_FORGOTTEN;

			// No longer known
			p->player_spell_flags[j] &= ~PY_SPELL_LEARNED;

			// Message 
			msg("You have forgotten the spell of %s.", spell->name);

			// One more can be learned
			p->upkeep->new_spells++;
		}
	}

	// Check for spells to remember
	for (i = 0; i < z_info->spell_max; i++) {
		// None left to remember
		if (p->upkeep->new_spells <= 0) break;

		// Get the next spell we learned
		j = p->player_spell_order[i];

		// Skip unknown spells
		if (j >= 99) break;

		// Get the spell
		spell = player_spell_lookup(j);

		// Skip spells we cannot remember
		if (spell->slevel > p->lev) continue;

		// First set of spells
		if (p->player_spell_flags[j] & PY_SPELL_FORGOTTEN) {
			// No longer forgotten
			p->player_spell_flags[j] &= ~PY_SPELL_FORGOTTEN;

			// Known once more
			p->player_spell_flags[j] |= PY_SPELL_LEARNED;

			// Message
			msg("You have remembered the spell of %s.", spell->name);

			// One less can be learned
			p->upkeep->new_spells--;
		}
	}

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

		if (p->upkeep->new_spells && realm && !realm->realm_special[RLM_SPCL_INNATE]) {
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
	int i, msp, levels, cur_wgt, max_wgt;
	struct monster_race *monr = lookup_player_monster(p);
	const struct magic_realm *realm = get_player_realm(p);

	levels = state->skills[SKILL_MAGIC];

	/* Extract "effective" player level */
	if (!realm || levels <= 0 || realm->realm_special[RLM_SPCL_HP_CAST]) {
		p->msp = 0;
		p->csp = 0;
		p->csp_frac = 0;
		return;
	}
	
	msp = levels;

	/* Assume player not encumbered by armor */
	state->cumber_armor = false;

	/* Weigh the armor */
	cur_wgt = 0;
	for (i = 0; i < p->mon.body.count; i++) {
		struct object *obj_local = slot_object(&p->mon, i);

		/* Ignore non-armor */
		if (slot_type_is(&p->mon, i, EQUIP_WEAPON)) continue;
		if (slot_type_is(&p->mon, i, EQUIP_BOW)) continue;
		if (slot_type_is(&p->mon, i, EQUIP_RING)) continue;
		if (slot_type_is(&p->mon, i, EQUIP_AMULET)) continue;
		if (slot_type_is(&p->mon, i, EQUIP_LIGHT)) continue;

		/* Add weight */
		if (obj_local) {
			cur_wgt += object_weight_one(obj_local);
		}
	}

	/* Determine the weight allowance */
	max_wgt = realm->weight;

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
	int mhp;

	/* Calculate hitpoints */
	// L: basically all handled elsewhere now
	mhp = p->mon.state.skills[SKILL_HEALTH];

	/* New maximum hitpoints */
	if (p->mon.maxhp != mhp) {
		/* Save new limit */
		p->mon.maxhp = mhp;

		/* Enforce new limit */
		if (p->mon.hp >= mhp) {
			p->mon.hp = mhp;
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
	int glow = get_power_scale_state(state, PP_GLOW, UNLIGHT_MAX_POWER * 2);
	int unlight = get_power_scale_state(state, PP_UNLIGHT, UNLIGHT_MAX_POWER * 2);
	//int unlight = get_power_scale_state(state, PP_UNLIGHT, UNLIGHT_MAX_POWER * 2, p->lev);

	/* Assume no light */
	state->cur_light = glow - unlight;

	/* Ascertain lightness if in the town */
	if (!p->depth && is_daytime() && update) {
		/* Update the visuals if necessary*/
		if (p->mon.state.cur_light != state->cur_light) {
			p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		}

		return;
	}

	/* Examine all wielded objects, use the brightest */
	for (i = 0; i < p->mon.body.count; i++) {
		int amt = 0;
		struct object *obj = slot_object(&p->mon, i);

		/* Skip empty slots */
		if (!obj) continue;

		/* Light radius - innate plus modifier */
		if (of_has(obj->flags, OF_LIGHT_2)) {
			amt = 2;
		} else if (of_has(obj->flags, OF_LIGHT_3)) {
			amt = 3;
		}
		amt += obj->modifiers[OBJ_MOD_LIGHT];

		/* Examine actual lights */
		if (tval_is_light(obj) && !of_has(obj->flags, OF_NO_FUEL) &&
				obj->timeout == 0) {
			/* Lights without fuel provide no light */
			amt = 0;
		}

		/* Alter p->mon.state.cur_light if reasonable */
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
	int skill = p->mon.state.skills[SKILL_DISARM_PHYS];

	if (lock_unseen || p->mon.m_timed[TMD_BLIND]) {
		skill /= 10;
	}
	if (p->mon.m_timed[TMD_CONFUSED] || p->mon.m_timed[TMD_IMAGE]) {
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
void calc_blows(struct player *p, int wgt, struct py_attack_roll *aroll,
               struct player_state *state, int extra_blows)
{
	int div = wgt * 2 + 100;

    int sind1 = state->stat_ind[aroll->damage_stat];
	int sind2 = aroll->accuracy_stat >= 0 ?
			state->stat_ind[aroll->accuracy_stat] :
			0;
	int sind3 = player_skill_stat_ind(p, state, aroll->attack_skill);

	// max 18
	int statind = sind2 != 0 ? 
			(sind1 + sind2 + sind3 + MAX(sind1, MAX(sind2, sind3))) / 4 :
			(sind1 + sind3 + MAX(sind1, sind3)) / 3;

	// max 600
	int baseblows = adj_stat_blow(statind);

	// max 100
	int skill = state->skills[aroll->attack_skill];

	// max 600 * 100 / 100
	int blows = MAX(0, baseblows) * skill / div;

	aroll->blows = blows + extra_blows + 100;
}

/**
 * Computes current weight limit.
 */
static int weight_limit(struct player_state *state)
{
	int i;

	/* Weight limit based only on strength */
	i = adj_str_wgt(state->stat_ind[STAT_STR]) * 10 + 100;

	/* Return the result */
	return MAX(i, 10);
}


/**
 * Computes weight remaining before burdened.
 */
int weight_remaining(struct player *p)
{
	int i;

	/* Weight limit based only on strength */
	i = 5 * adj_str_wgt(p->mon.state.stat_ind[STAT_STR]) + 50
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
void adjust_skill_scale(int *v, int num, int den, int minv)
{
	if (den < 0) {
		den *= -1;
		num *= -1;
	}

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


#if 0
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
	*blows += shape->modifiers[OBJ_MOD_BLOWS] * 100;
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
#endif

static int power_by_element(int elem)
{
	int i;

	for (i = 0; elem_pp_matches[i].mval >= 0; ++i) {
		if (elem_pp_matches[i].mval == elem) {
			return elem_pp_matches[i].pval;
		}
	}

	return PP_NONE;
}

static int power_by_projection(int proj)
{
	int i;
	
	for (i = 0; proj_pp_matches[i].mval >= 0; ++i) {
		if (proj_pp_matches[i].mval == proj) {
			return proj_pp_matches[i].pval;
		}
	}

	return power_by_element(proj);
}

int skill_by_effect(int effect_ind, int effect_subtype)
{
	
	switch (effect_ind)
	{
		case EF_BALL:
		case EF_BALL_NO_DAM_RED:
		case EF_BEAM:
		case EF_BOLT:
		case EF_BREATH:
		case EF_BOLT_AWARE:
		case EF_BOLT_STATUS:
		case EF_BOLT_STATUS_DAM:
		case EF_SPOT:
		case EF_SPHERE:
			return power_by_element(effect_subtype);
		case EF_PROJECT_LOS:
		case EF_PROJECT_LOS_AWARE:
			return power_by_projection(effect_subtype);
		case EF_MON_HEAL_HP:
		case EF_MON_HEAL_KIN:
			return PP_HOLY_MAGIC;
		case EF_LASH:
			return PP_HAFTED_SPECIALIZATION;
	}

	return PP_NONE;
}

#if 0
static int calc_monster_stats(const struct player *p, int which)
{
	if (which >= STAT_MAX || which <= STAT_NONE) return 0;

	const struct monster_race *mr = lookup_player_monster(p);
	int min, max, result, currlev, maxlev;

	assert(mr);

	min = -1;
	max = mr->stat_mod[which];
	currlev = mr->level;
	maxlev = expected_max_evol_level(p);

	if (maxlev <= 0) return max;

	result = ((max - min) * currlev + maxlev / 2) / maxlev + min;
	
	return result;
}
#endif

static bool calc_monster_blow(int counts[PP_MAX], const struct monster_blow *mb)
{
	bool effect = false;
	int lash_type = mb->effect->lash_type == -1 ? mb->method->lash_type : mb->effect->lash_type;
	int lash_skill;

	if (!mb->method->player_usable && mb->method->unarmed) {
		counts[PP_UNARMED_STRIKE]++;
		effect = true;
	}

	lash_skill = power_by_element(lash_type);
	if (lash_skill > PP_NONE) {
		counts[lash_skill]++;
		effect = true;
	}

	return effect;
}

void calc_monster_powers(struct monster_race *mrace, int powers[PP_MAX], int curr_powers[PP_MAX])
{
	int i, totalbonus, numcounts = 0, numblows = 0;
	const struct monster_blow *mblow;
	struct player_ability *abil;
	int spell_counts[PP_MAX] = { 0 };
	int blow_counts[PP_MAX] = { 0 };

	if (numcounts > 0) {
		totalbonus = my_cbrt(mrace->spell_power * mrace->spell_power) * (4.0 + numcounts) / (9.0 + numcounts);
		for (i = 0; i < PP_MAX; i++) {
			powers[i] += spell_counts[i] * totalbonus / numcounts;
		}
	}

	
	for (i = 0; i < z_info->mon_blows_max && mrace->blow[i].method; i++) {
		mblow = &mrace->blow[i];
		if (calc_monster_blow(blow_counts, mblow)) {
			++numblows;
		}
	}

	if (numblows > 0) {
		totalbonus = my_sqrt(mrace->level * mrace->level) * (4.0 + numblows) / (9.0 + numblows);
		for (i = 0; i < PP_MAX; i++) {
			powers[i] += blow_counts[i] * totalbonus / numblows;
		}
	}

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index < 0) continue;
		if (abil->type != PY_ABIL_POWER) continue;
		int base = mrace->powers[abil->index];
		int add = base < 0 ? base / 2 : base / 5;
		if (base < 0 && curr_powers[abil->index] > 0) {
			powers[abil->index] += curr_powers[abil->index] * base / 100;
		}
		powers[abil->index] += add;
	}

	for (i = 0; i < MS_MAX; ++i) {
		// monsters are specialized, take penalty to magic skills they don't get
		if (powers[i] > 0) continue;
		int penalty = my_int_sqrt(mrace->level);
		penalty = MIN(curr_powers[i] / 2, penalty);
		penalty = MAX(0, penalty);
		powers[i] -= penalty;
	}
}

void calc_monster_skills(struct monster_race *mrace, int skills[SKILL_MAX])
{
	int i, norm_hp, mod;

	memset(skills, 0, sizeof *skills * SKILL_MAX);

	for (i = 0; i < SKILL_MAX; i++) {
		skills[i] += mrace->skills[i] * (mrace->level + 66) / 66;
	}

	// assume the monster gets hp equal to half its level from its class
	norm_hp = (int)(mrace->level * my_sqrt((double)mrace->level) / 10.0);
	mod = mrace->avg_hp - norm_hp;
	if (mod > 0) {
		mod = my_int_cbrt(mod * mod);
	}
	else {
		mod = -my_int_sqrt(-mod);
	}
	skills[SKILL_HEALTH] += mod;
}

#if 0
/**
 * L: calculate the effects of being a monster on player state
 */
static void calc_monster(struct player *p, struct player_state *state,
						 bool vuln[ELEM_MAX], int *moves)
{
	struct monster_race *mrace = lookup_player_monster(p);

	if (rf_has(mrace->flags, RF_NEVER_MOVE)) *moves -= 25;
	return;

	int i;
	int powers[PP_MAX] = { 0 };
	int skills[SKILL_MAX] = { 0 };

	if (!mrace) {
		return;
	}

	/*for (i = 0; i < ELEM_MAX; ++i) {
		int mon_res = mrace->el_info[i].res_level;
		int new_res = state->el_info[i].res_level + mon_res;
		state->el_info[i].res_level = MAX(MIN(new_res, 3), -1);
	}*/

	state->speed += mrace->speed / 2 - 55;
	state->to_a = MAX(state->to_a, mrace->ac) + MIN(state->to_a, mrace->ac) / 2;

	if (rf_has(mrace->flags, RF_NEVER_MOVE)) *moves -= 25;

	calc_monster_powers(mrace, powers, state->powers);

	for (i = 0; i < PP_MAX; ++i) {
		state->powers[i] += powers[i];
	}

	calc_monster_skills(mrace, skills);

	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] += skills[i];
	}

	pf_union(state->pflags, mrace->base->pflags);
	of_union(state->flags, mrace->base->oflags);
}
#endif

#if 0
/**
 * L: bonuses from the UNLIGHT power
 */
static void calc_unlight(struct player_state *ps, struct player *p)
{
	if (ps->powers[PP_UNLIGHT] < 0) return;

	int power = unlight_power_state(ps, p);

	if (power > 5) {
		ps->el_info[ELEM_DARK].res_level++;
	}

	adjust_skill_scale(&ps->skills[SKILL_STEALTH], power, 25, 25);
	adjust_skill_scale(&ps->skills[SKILL_SAVE], power, 25, 25);

	ps->ac += power * ABS(power);
}

/** 
 * L: bonuses from the GLOW power
 */
static void calc_glow(struct player_state *ps, struct player *p)
{
	if (ps->powers[PP_GLOW] < 0) return;

	int power = glow_power_state(ps, p);

	if (power > 5) {
		ps->el_info[ELEM_LIGHT].res_level++;
	}

	adjust_skill_scale(&ps->skills[SKILL_SAVE], power, 30, 10);
	ps->to_a += SGN(power) * my_int_sqrt(ABS(power) * power * power);
}
#endif




void mon_class_skill(const struct monster *mon, int skill, int *base, int *xtra)
{
	struct player *p = mon->player;
	int tome, b_amt, x_amt;

	if (!p) return;
	if (skill < 0 || skill >= SKILL_MAX) return;

	tome = p->extra_skills[skill];
	b_amt = p->class->c_skills[skill];
	x_amt = p->class->x_skills[skill];

	if (pf_has(p->class->pflags, PF_EXTRA_LEARNING)) {
		b_amt = MAX(b_amt, tome * 1 / 4);
		x_amt = MAX(x_amt, tome * 3 / 4);
	}

	*base += b_amt;
	*xtra += x_amt;
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
void calc_bonuses(struct player *p, struct monster *mon, struct player_state *state, bool known_only,
				  bool update)
{
	int i, j, hold;
	int base, xtra;
	//int extra_blows = 0;
	int extra_shots = 0;
	int extra_might = 0;
	int extra_moves = 0;
	//int attacknum;
	struct object *launcher = NULL;
	struct object *weapons[PY_MAX_ATTACKS] = { 0 };
	int num_weapons = 0;
	bitflag collect_f[OF_SIZE];
	struct monster_race *mrace = mon->race;
	//int avail_hands, attack_div;
	//int race_skills[SKILL_MAX] = { 0 }, race_x_skills[SKILL_MAX] = { 0 };
	//bool has_feet = false;
	//bool vuln[ELEM_MAX] = { false };

	/* Hack to allow calculating hypothetical blows for extra STR, DEX - NRM */
	int str_ind = state->stat_ind[STAT_STR];
	int dex_ind = state->stat_ind[STAT_DEX];

	/* Reset */
	memset(state, 0, sizeof *state);

	// L: base monster calcs
	assert(mon == &p->mon);

	calc_mon_bonuses(mon, state);

	/* Extract race/class info */
	state->see_infra = p->race->infra;

	/* Base pflags */
	pf_union(state->pflags, p->race->pflags);
	pf_union(state->pflags, p->class->pflags);

	/* Extract the player flags */
	player_flags(p, collect_f);

	for (i = 0; i < SKILL_MAX; ++i) {
		base = 0;
		xtra = 0;

		mon_class_skill(mon, i, &base, &xtra);

		state->skills[i] += base;
		state->skills[i] += xtra * p->lev / 50;
	}

	/* L: get powers */
	/*for (i = PP_NONE + 1; i < PP_MAX; ++i) {
		state->powers[i] /= 2;

		struct player_ability *abil = lookup_player_ability(i, PY_ABIL_POWER);
		assert(abil);

		int scale = player_class_power(p, i);// + player_race_power(p, i);
		int minlev = 5 - (scale + 5) / 7;
		int efflev = minlev < 0 ? MAX((p->lev + 1) / 2 - minlev    , p->lev) :
								  MIN((p->lev + 1) * 2 - minlev * 2, p->lev);

		double fact = 1.0, div = 1.0;
		int scaling = abil->scale;

		while (scaling >= 2) {
			fact *= (float)p->lev;
			div *= 50.0;
			scaling -= 2;
		}
		while (scaling >= 1) {
			fact *= my_sqrt((double)p->lev);
			div *= my_sqrt(50.0);
			--scaling;
		}
		while (scaling <= -2) {
			fact *= 50.0;
			div *= (double)p->lev;
			scaling += 2;
		}
		while (scaling <= -1) {
			fact *= my_sqrt(50.0);
			div *= my_sqrt((double)p->lev);
			++scaling;
		}

		if ((scale <= 0) || (efflev <= 0)) {
			state->powers[i] += 0;
		}

		else if (p->lev >= PY_MAX_LEVEL) {
			state->powers[i] += (p->lev * scale + 99) / 100;
		}

		else {
			state->powers[i] += (int)((efflev * scale * fact + div * 100 - 1) / (div * 100));
		}

		state->powers[i] += MIN((p->extra_powers[i] + 1) / 2, p->lev * 3);
	}*/

#if 0
	/* Analyze equipment */
	for (i = 0; i < p->mon.body.count; i++) {
		int index = 0;
		struct object *obj = slot_object(&p->mon, i);
		struct curse_data *curse = obj ? obj->curses : NULL;
		
		if (slot_type_is(&p->mon, i, EQUIP_WEAPON) && num_weapons < PY_MAX_ATTACKS) {
			weapons[num_weapons] = obj;
			++num_weapons;
		}

		if (slot_type_is(&p->mon, i, EQUIP_BOOTS)) {
			has_feet = true;
		}

		while (obj) {
			int dig = 0;
			int owgt = object_weight_one(obj);

			/* L: track armour weight */
			if (slot_type_is(&p->mon, i, EQUIP_BODY_ARMOR)) {
			    armwgt = MAX(armwgt, owgt);
			}

			if (!launcher && slot_type_is(&p->mon, i, EQUIP_BOW)) {
				launcher = obj;
			}

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
			extra_blows += obj->modifiers[OBJ_MOD_BLOWS] * 100
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
					if (obj->el_info[j].res_level == -1) {
						--state->el_info[j].res_level;
						//vuln[j] = true;
					}

					/* OK because res_level hasn't included vulnerability yet */
					if (obj->el_info[j].res_level > state->el_info[j].res_level) {
						state->el_info[j].res_level = obj->el_info[j].res_level;
					}
				}
			}

			/* Apply combat bonuses */
			/*state->ac += obj->ac;
			if (!known_only || obj->known->to_a) {
				state->to_a += obj->to_a;
			}*/
			if (!slot_type_is(&p->mon, i, EQUIP_WEAPON)
					&& !slot_type_is(&p->mon, i, EQUIP_BOW)) {
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
#endif

	/* Apply the collected flags */
	//of_union(state->flags, collect_f);

	/* Add shapechange info */
	/*calc_shapechange(state, vuln, p->shape, &extra_blows, &extra_shots,
		&extra_might, &extra_moves);*/

	/* L: add monster info */
	/*if (mrace) {
		calc_monster(p, state, vuln, &extra_moves);
	}*/

	/* Calculate light */
	//calc_light(p, state, update);

	/* Evil */
	/*if (pf_has(state->pflags, PF_EVIL) && character_dungeon) {
		state->el_info[ELEM_NETHER].res_level = 1;
		vuln[ELEM_HOLY_ORB] = true;
	}*/

	/* Now deal with vulnerabilities */
	/*for (i = 0; i < ELEM_MAX; i++) {
		if (vuln[i] && (state->el_info[i].res_level < 3)) {
			state->el_info[i].res_level--;
		}
	}*/

	/* Calculate the various stat values */
	for (i = 0; i < STAT_MAX; i++) {
		int add, use, ind;

        /* L: Class doesn't affect stats any more, race affects them elsewhere */
		add = state->stat_add[i];
		if (mrace) {
			//add += calc_monster_stats(p, i);
			//add += modify_stat_value(use, calc_monster_stats(p, i));
		}
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
		if (!update && character_generated) {
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

	// L: calc extra points
	calc_extra_points(p, state);

	// L: calculate skills
	//player_race_r_skill(p->race, mrace ? true : false, race_skills);
	//player_race_x_skill(p->race, mrace ? true : false, race_x_skills);
	/*for (i = 0; i < SKILL_MAX; i++) {
		int stat_ind = player_skill_stat_ind(p, state, i);
		int base = player_class_c_skill(p, i);
		int xtra = player_class_x_skill(p, i) * p->lev / PY_MAX_LEVEL;
		int tome = p->extra_skills[i];
		// += because monster skills have already been calcd
		state->skills[i] += base + xtra + tome;
		if (stat_ind > -1) {
			state->skills[i] += MAX(state->skills[i], 0) * adj_stat_skill_percent(stat_ind, i) / 100;
			state->skills[i] += adj_stat_skill_flat(stat_ind, i);
		}

		state->skills[i] = MAX(state->skills[i], 0);
	}*/

	//calc_unlight(state, p);
	//calc_glow(state, p);

	/* Effects of food outside the "Fed" range */
	if (!player_timed_grade_eq(p, TMD_FOOD, "Fed")) {
		int excess = p->mon.m_timed[TMD_FOOD] - PY_FOOD_FULL;
		int lack = PY_FOOD_HUNGRY - p->mon.m_timed[TMD_FOOD];
		if ((excess > 0) && !p->mon.m_timed[TMD_ATT_VAMP]) {
			/* Scale to units 1/10 of the range and subtract from speed */
			excess = excess * 25 / (PY_FOOD_MAX - PY_FOOD_FULL);
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
	//unarmoured_speed_bonus(state, armwgt);
	//unarmoured_ac_bonus(state, armwgt);

	/* Other timed effects */
	player_flags_timed(p, state->flags);

	/*if (player_timed_grade_eq(p, TMD_STUN, "Heavy Stun")) {
		state->to_h -= 20;
		state->to_d -= 20;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
		if (update) {
			p->mon.m_timed[TMD_FASTCAST] = 0;
		}
	} else if (player_timed_grade_eq(p, TMD_STUN, "Stun")) {
		state->to_h -= 5;
		state->to_d -= 5;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
		if (update) {
			p->mon.m_timed[TMD_FASTCAST] = 0;
		}
	}
	if (p->mon.m_timed[TMD_INVULN]) {
		state->to_a += 100;
	}
	if (p->mon.m_timed[TMD_BLESSED]) {
		state->to_a += 5;
		state->to_h += 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (p->mon.m_timed[TMD_SHIELD]) {
		state->to_a += 50;
	}
	if (p->mon.m_timed[TMD_STONESKIN]) {
		state->to_a += 40;
		state->speed -= 5;
	}
	if (p->mon.m_timed[TMD_HERO]) {
		state->to_h += 12;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (p->mon.m_timed[TMD_SHERO]) {
		state->skills[SKILL_TO_HIT_MELEE] += 75;
		state->to_a -= 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
	}
	if (p->mon.m_timed[TMD_FAST] || p->mon.m_timed[TMD_SPRINT]) {
		state->speed += 10;
	}
	if (p->mon.m_timed[TMD_SLOW]) {
		state->speed -= 10;
	}
	if (p->mon.m_timed[TMD_SINFRA]) {
		state->see_infra += 5;
	}
	if (p->mon.m_timed[TMD_TERROR]) {
		state->speed += 10;
	}
	for (i = 0; i < TMD_MAX; ++i) {
		if (p->mon.m_timed[i] && timed_effects[i].temp_resist != -1
				&& state->el_info[timed_effects[i].temp_resist].res_level
				< 2) {
			state->el_info[timed_effects[i].temp_resist].res_level++;
		}
	}
	if (p->mon.m_timed[TMD_CONFUSED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 4, 0);
	}
	if (p->mon.m_timed[TMD_AMNESIA]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (p->mon.m_timed[TMD_POISONED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}
	if (p->mon.m_timed[TMD_IMAGE]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (p->mon.m_timed[TMD_BLOODLUST]) {
		int p_berserk = get_power_scale_state(state, PP_BERSERK, 150, p->lev);
		int bonus = p->mon.m_timed[TMD_BLOODLUST] * (100 + p_berserk) / 100;

		state->to_d += bonus / 5 + 1;
		state->to_h += bonus * 2 / 3;
		extra_blows += bonus * 4;
		state->speed += bonus / 5 - 3;
		state->dam_red += bonus * (p->mon.maxhp + 100) / 1000;
		adjust_skill_scale(&state->skills[SKILL_STEALTH], -bonus, 5, 10);
		adjust_skill_scale(&state->skills[SKILL_SAVE], bonus, 20, 10);
	}
	if (p->mon.m_timed[TMD_STEALTH]) {
		state->skills[SKILL_STEALTH] += 10;
	}*/

	/* Analyze flags - check for fear */
	if (of_has(state->flags, OF_AFRAID)) {
		state->to_h -= 20;
		state->to_a += 8;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}

	/* Analyze weight */
	j = p->upkeep->total_weight;
	i = weight_limit(state);
	if (j > i / 2) {
		state->speed -= ((j - (i / 2)) / (i / 10));
	}
	if (state->speed < 0) {
		state->speed = 0;
	}
	if (state->speed > 199) {
		state->speed = 199;
	}

	/* Apply modifier bonuses (Un-inflate stat bonuses) */
	//state->to_a += adj_dex_ta(state->stat_ind[STAT_DEX]);

	/* L: change expfact based on int */
	state->expfact = p->race->r_exp + p->class->c_exp + adj_int_xp(state->stat_ind[STAT_INT]);
	state->expfact = MAX(50, state->expfact);

	/* Modify skills */
	/*if (state->skills[SKILL_DIGGING] < 1) state->skills[SKILL_DIGGING] = 1;
	if (state->skills[SKILL_STEALTH] > 150) state->skills[SKILL_STEALTH] = 150;
	if (state->skills[SKILL_HEALTH] < 3) state->skills[SKILL_HEALTH] = 3;*/
	hold = adj_str_hold(state->stat_ind[STAT_STR]);

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
		if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_SHOTS)) {
			state->ammo_tval = TV_SHOT;
		} else if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_ARROWS)) {
			state->ammo_tval = TV_ARROW;
		} else if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_BOLTS)) {
			state->ammo_tval = TV_BOLT;
		}

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
	if (num_weapons > 0) {
		int16_t weapon_weight = 0;
		bool all_hafted = true;
		bool any_hafted = false;
		for (i = 0; i < num_weapons; i++) {
			if (!weapons[i]) continue;
			int currwgt = object_weight_one(weapons[i]);
			if (weapons[i]->tval == TV_HAFTED) {
				any_hafted = true;
			}
			else if (weapons[i]->tval != TV_SHIELD) {
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
	/*avail_hands = 0;
	attacknum = 0;
	attack_div = 0;
	bool hand_in_use = false;
	for (i = 0; i < num_weapons; ++i) {
		if (weapons[i]) {
			attack_div += 2;
		}
		else {
			attack_div += 1;
		}
	}

	for (i = 0; i < num_weapons; ++i) {
		assert(i < PY_MAX_ATTACKS && attacknum < PY_MAX_ATTACKS);
		if (weapons[i]) {
			if (get_melee_weapon_attack(p, state, weapons[i], &state->attacks[attacknum], attack_div)) {
				++attacknum;
				hand_in_use = true;
			}
		}
		else {
			++avail_hands;
		}
	}
	if (mrace) {
		avail_hands -= get_monster_attacks(p, state, mrace,
				state->attacks,
				PY_MAX_ATTACKS,
				&attacknum,
				false);
	}
	if (state->powers[PP_UNARMED_STRIKE] > 0 || attacknum == 0) {
		if (!state->powers[PP_UNARMED_STRIKE]) {
			avail_hands = MIN(avail_hands, 1);
		}
		if (hand_in_use) --avail_hands;
		while (avail_hands > 0 && attacknum < PY_MAX_ATTACKS) {
			if (get_unarmed_punch(p, state, &state->attacks[attacknum], attack_div)) {
				++attacknum;
			}
			--avail_hands;
		}
	}
	if (has_feet) {
		int numkicks = get_power_scale_state(state, PP_UNARMED_STRIKE, 3, p->lev) - 1;
		for (i = 0; i < numkicks && attacknum < PY_MAX_ATTACKS; i++) {
			if (get_unarmed_kick(p, state, &state->attacks[attacknum], attack_div)) {
				++attacknum;
			}
		}
	}

	state->num_attacks = attacknum;

	assert(attacknum <= PY_MAX_ATTACKS);

	state->has_ranged_attack = false;
	if (launcher) {
		state->ranged_attack = get_shooter_weapon_attack(p, state, launcher);
		calc_blows(p, launcher->weight, &state->ranged_attack, state, extra_shots);
		state->has_ranged_attack = true;
	}
	else {
		state->ranged_attack.obj = NULL;
	}

	// L: give attacks blows
	for (i = 0; i < attacknum; i++) {
		if (state->heavy_wield) {
			state->attacks[i].blows = 100;
		}
		else {
			const struct object *obj = state->attacks[i].obj;
			int wgt = obj ? object_weight_one(obj) : 0;
			calc_blows(p, wgt, &state->attacks[i], state, extra_blows);
		}
	}*/

	/* Mana */
	/*calc_mana(p, state, update);
	if (!p->msp) {
		pf_on(state->pflags, PF_NO_MANA);
	}*/

	//extra_moves += get_power_scale_state(state, PP_RUNNING, 10, p->lev);

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

	struct player_state state = p->mon.state;
	struct player_state known_state = p->known_state;

	/* ------------------------------------
	 * Calculate bonuses
	 * ------------------------------------ */

	calc_bonuses(p, &p->mon, &state, false, true);
	calc_bonuses(p, &p->mon, &known_state, true, true);


	/* ------------------------------------
	 * Notice changes
	 * ------------------------------------ */

	/* Analyze stats */
	for (i = 0; i < STAT_MAX; i++) {
		/* Notice changes */
		if (state.stat_top[i] != p->mon.state.stat_top[i])
			/* Redisplay the stats later */
			p->upkeep->redraw |= (PR_STATS);

		/* Notice changes */
		if (state.stat_use[i] != p->mon.state.stat_use[i])
			/* Redisplay the stats later */
			p->upkeep->redraw |= (PR_STATS);

		/* Notice changes */
		if (state.stat_ind[i] != p->mon.state.stat_ind[i]) {
			/* Change in CON affects Hitpoints */
			if (i == STAT_CON)
				p->upkeep->update |= (PU_HP);

			/* Change in stats may affect Mana/Spells */
			p->upkeep->update |= (PU_MANA | PU_SPELLS);
		}
	}

	// L: update exp if needed
	if (state.expfact != p->mon.state.expfact) {
		p->upkeep->redraw |= PR_EXP;
	}

	// L: redraw status if learning ability changed
	if (state.extra_points_max != p->mon.state.extra_points_max ||
			state.extra_points_used != p->mon.state.extra_points_used) {
		p->upkeep->redraw |= PR_STATUS;
	}

	// L: update spells if magic skill changed
	if (state.skills[SKILL_MAGIC] != p->mon.state.skills[SKILL_MAGIC]) {
		p->upkeep->update |= PU_SPELLS;
	}

	if (state.skills[SKILL_HEALTH] != p->mon.state.skills[SKILL_HEALTH]) {
		p->upkeep->update |= PU_HP;
	}


	/* Hack -- Telepathy Change */
	if (of_has(state.flags, OF_TELEPATHY) !=
			of_has(p->mon.state.flags, OF_TELEPATHY)) {
		/* Update monster visibility */
		p->upkeep->update |= (PU_MONSTERS);
	}
	/* Hack -- See Invis Change */
	if (of_has(state.flags, OF_SEE_INVIS) !=
			of_has(p->mon.state.flags, OF_SEE_INVIS)) {
		/* Update monster visibility */
		p->upkeep->update |= (PU_MONSTERS);
	}

	/* Redraw speed (if needed) */
	if (state.speed != p->mon.state.speed) {
		p->upkeep->redraw |= (PR_SPEED);
	}

	/* Redraw armor (if needed) */
	if ((known_state.ac != p->known_state.ac) || 
			(known_state.to_a != p->known_state.to_a)) {
		p->upkeep->redraw |= (PR_ARMOR);
	}

	/* Notice changes in the "light radius" */
	if (p->mon.state.cur_light != state.cur_light) {
		/* Update the visuals */
		p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}

	/* Notice changes to the weight limit. */
	if (weight_limit(&p->mon.state) != weight_limit(&state)) {
		p->upkeep->redraw |= (PR_INVEN);
	}

	/* Hack -- handle partial mode */
	if (!p->upkeep->only_partial) {
		/* Take note when "heavy bow" changes */
		if (p->mon.state.heavy_shoot != state.heavy_shoot) {
			/* Message */
			if (state.heavy_shoot) {
				msg("You have trouble wielding such a heavy bow.");
			} else if (slot_object(&p->mon, slot_by_type(&p->mon, EQUIP_BOW, true))) {
				msg("You have no trouble wielding your bow.");
			} else {
				msg("You feel relieved to put down your heavy bow.");
			}
		}

		/* Take note when "heavy weapon" changes */
		if (p->mon.state.heavy_wield != state.heavy_wield) {
			/* Message */
			if (state.heavy_wield) {
				msg("You have trouble wielding such a heavy weapon.");
			} else if (slot_object(&p->mon, slot_by_type(&p->mon, EQUIP_WEAPON, true))) {
				msg("You have no trouble wielding your weapon.");
			} else {
				msg("You feel relieved to put down your heavy weapon.");
			}
		}

		/* Take note when "illegal weapon" changes */
		if (p->mon.state.bless_wield != state.bless_wield) {
			/* Message */
			if (state.bless_wield) {
				msg("You feel attuned to your weapon.");
			} else if (slot_object(&p->mon, slot_by_type(&p->mon, EQUIP_WEAPON, true))) {
				msg("You feel less attuned to your weapon.");
			}
		}

		/* Take note when "armor state" changes */
		if (p->mon.state.cumber_armor != state.cumber_armor) {
			/* Message */
			if (state.cumber_armor) {
				msg("The weight of your armor encumbers your movement.");
			} else {
				msg("You feel able to move more freely.");
			}
		}
	}

	memcpy(&p->mon.state, &state, sizeof(state));
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
		combine_pack(&p->mon);
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
		calc_light(p, &p->mon.state, true);
	}

	if (p->upkeep->update & (PU_HP)) {
		p->upkeep->update &= ~(PU_HP);
		calc_hitpoints(p);
	}

	if (p->upkeep->update & (PU_MANA)) {
		p->upkeep->update &= ~(PU_MANA);
		calc_mana(p, &p->mon.state, true);
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
	if (!map_is_visible()) {
		redraw &= PR_SUBWINDOW;
	}

	/* Hack - rarely update while resting or running, makes it over quicker */
	if (((player_resting_count(p) % 100) || (p->upkeep->running % 100))
			&& !(redraw & (PR_MESSAGE | PR_MAP))) {
		return;
	}

	/* For each listed flag, send the appropriate signal to the UI */
	for (i = 0; i < N_ELEMENTS(redraw_events); i++) {
		const struct flag_event_trigger *hnd = &redraw_events[i];

		if (redraw & hnd->flag) {
			event_signal(hnd->event);
		}
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

