/**
 * \file mon-util.c
 * \brief Monster manipulation utilities.
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "cmd-core.h"
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-group.h"
#include "mon-list.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-msg.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-summon.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-properties.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"


void mark_mon_as_playable(struct monster_race *mr)
{
	if (!mr || !mr->evol || !mr->evol->race) return;

	struct evolution *me;

	for (me = mr->evol; me && me->race; me = me->next) {
		mark_mon_as_playable(me->race);
	}
}

struct object *monster_best_weapon(struct monster *m)
{
	struct object *weap, *best = NULL;
	int bestval, i;

	for (i = 0; i < m->body.count; ++i) {
		weap = m->body.slots[i].obj;
		if (!weap) continue;
		if (!tval_is_melee_weapon(weap)) continue;
		int curr = weap->dd * (weap->ds + 1) + weap->to_d * 2 + weap->to_h;
		if (!best || curr > bestval) {
			best = weap;
			bestval = curr;
		}
	}

	return best;
}

bool give_monster_powers(struct monster *mon)
{
	#if 0
	assert(mon);
	//bool isleader = true;
	//struct monster *leader = NULL;
	struct monster_race *mr = mon->race;
	struct monster_base *mb = mr->base;
	bool given = false;
	struct player_ability *abil;

	/*if (mon->group_info[PRIMARY_GROUP].role != MON_GROUP_LEADER) {
		plg("getting leader");
		leader = monster_group_leader(cave, mon);
		if (leader != mon) {
			isleader = false;
		}
	}*/
	
	/*for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->learn_index < 0) continue;
		if (abil->type != PY_ABIL_POWER) continue;
		int i = abil->index;

		if (mb->powers[i]) {
			mon->powers[i] = true;
			given = true;
		}
	}*/

	if (rf_has(mr->flags, RF_SAPIENT)) {
		while (one_in_(10)) {
			// randint0(30 - (-1) - 1) + (-1) + 1 = randint0()
			int choice = randint0(PP_MAX - PP_NONE - 1) + PP_NONE + 1;
			assert(choice > 0);
			pp_flag_on(mon->powers, choice);
		}
		/*for (i = PP_NONE + 1; i < PP_MAX; ++i) {
			if (one_in_(10)) {
				plg_fmt("turning on flag %i", i);
				pp_flag_on(mon->powers, i);
				given = true;
			}
		}*/
		/*else if (leader) {
			plg("has leader");
			for (i = 0; i < PP_MAX; ++i) {
				if (pp_flag_has(leader->powers, i) && one_in_(3)) {
					pp_flag_on(mon->powers, i);
					given = true;
				}
			}
		}*/
	}


	#endif

	return false;
}

bool mon_is_player(const struct monster *mon)
{
	return mon->player ? true : false;
}

bool player_can_learn_from_monster(struct player *p, struct monster *mon)
{
	struct player_ability *abil;

	int *max_target = mem_zalloc(sizeof *max_target * z_info->learn_max);
	tome_max_learnable(p, max_target);

	for (abil = player_abilities; abil; abil = abil->next) {
		if (abil->type != PY_ABIL_POWER) continue;
		if (mon->powers[abil->index]) continue;
		if (mon->race->level <= max_target[abil->learn_index]) continue;
		
		mem_free(max_target);
		return true;
	}

	mem_free(max_target);
	return false;
}

int monster_light(struct monster *mon)
{
	int base = mon->race->light;

	if (rf_has(mon->race->flags, RF_LIGHT_AURA)) {
		base += mon->race->level / 10;
	}
	if (rf_has(mon->race->flags, RF_DARK_AURA)) {
		base -= mon->race->level / 10;
	}

	return base;
}


/**
 * ------------------------------------------------------------------------
 * Lore utilities
 * ------------------------------------------------------------------------ */
static const struct monster_flag monster_flag_table[] =
{
	#define RF(a, b, c, d) { RF_##a, b, c, d },
	#include "list-mon-race-flags.h"
	#undef RF
	{ RF_MAX, 0, 0, NULL }
};

/**
 * Return a description for the given monster race flag.
 *
 * Returns an empty string for an out-of-range flag.
 *
 * \param flag is one of the RF_ flags.
 */
const char *describe_race_flag(int flag)
{
	const struct monster_flag *rf = &monster_flag_table[flag];

	if (flag <= RF_NONE || flag >= RF_MAX)
		return "";

	return rf->desc;
}

/**
 * Create a mask of monster flags of a specific type.
 *
 * \param f is the flag array we're filling
 * \param ... is the list of flags we're looking for
 *
 * N.B. RFT_MAX must be the last item in the ... list
 */
void create_mon_flag_mask(bitflag *f, ...)
{
	const struct monster_flag *rf;
	int i;
	va_list args;

	rf_wipe(f);

	va_start(args, f);

	/* Process each type in the va_args */
    for (i = va_arg(args, int); i != RFT_MAX; i = va_arg(args, int)) {
		for (rf = monster_flag_table; rf->index < RF_MAX; rf++) {
			if (rf->type == i) {
				rf_on(f, rf->index);
			}
		}
	}

	va_end(args);

	return;
}


/**
 * ------------------------------------------------------------------------
 * Lookup utilities
 * ------------------------------------------------------------------------ */
/**
 * Returns the monster with the given name. If no monster has the exact name
 * given, returns the first monster with the given name as a (case-insensitive)
 * substring.
 */
struct monster_race *lookup_monster(const char *name)
{
	int i;
	struct monster_race *closest = NULL;

	/* Look for it */
	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];
		if (!race->name)
			continue;

		/* Test for equality */
		if (my_stricmp(name, race->name) == 0)
			return race;

		/* Test for close matches */
		if (!closest && my_stristr(race->name, name))
			closest = race;
	}

	/* Return our best match */
	return closest;
}

/**
 * Return the monster base matching the given name.
 */
struct monster_base *lookup_monster_base(const char *name)
{
	struct monster_base *base;

	/* Look for it */
	for (base = rb_info; base; base = base->next) {
		if (streq(name, base->name))
			return base;
	}

	return NULL;
}

/**
 * Return whether the given base matches any of the names given.
 *
 * Accepts a variable-length list of name strings. The list must end with NULL.
 *
 * This function is currently unused, except in a test... -NRM-
 */
bool match_monster_bases(const struct monster_base *base, ...)
{
	bool ok = false;
	va_list vp;
	char *name;

	va_start(vp, base);
	while (!ok && ((name = va_arg(vp, char *)) != NULL))
		ok = base == lookup_monster_base(name);
	va_end(vp);

	return ok;
}

/**
 * Returns the monster currently commanded, or NULL
 */
struct monster *get_commanded_monster(void)
{
	int i;

	/* Look for it */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Skip dead monsters */
		if (!mon->race) continue;

		/* Test for control */
		//if (mon->m_timed[MON_TMD_COMMAND]) return mon;
	}

	return NULL;
}

/**
 * ------------------------------------------------------------------------
 * Monster updates
 * ------------------------------------------------------------------------ */
/**
 * Analyse the path from player to infravision-seen monster and forget any
 * grids which would have blocked line of sight
 */
static void path_analyse(struct chunk *c, struct loc grid)
{
	int path_n, i;
	struct loc path_g[256];

	if (c != cave) {
		return;
	}

	/* Plot the path. */
	path_n = project_path(c, path_g, z_info->max_range, player->mon.grid,
		grid, PROJECT_NONE);

	/* Project along the path */
	for (i = 0; i < path_n - 1; ++i) {
		/* Forget grids which would block los */
		if (!square_allowslos(player->cave, path_g[i])) {
			sqinfo_off(square(c, path_g[i])->info, SQUARE_SEEN);
			square_forget(c, path_g[i]);
			square_light_spot(c, path_g[i]);
		}
	}
}

/**
 * This function updates the monster record of the given monster
 *
 * This involves extracting the distance to the player (if requested),
 * and then checking for visibility (natural, infravision, see-invis,
 * telepathy), updating the monster visibility flag, redrawing (or
 * erasing) the monster when its visibility changes, and taking note
 * of any interesting monster flags (cold-blooded, invisible, etc).
 *
 * Note the new "mflag" field which encodes several monster state flags,
 * including "view" for when the monster is currently in line of sight,
 * and "mark" for when the monster is currently visible via detection.
 *
 * The only monster fields that are changed here are "cdis" (the
 * distance from the player), "ml" (visible to the player), and
 * "mflag" (to maintain the "MFLAG_VIEW" flag).
 *
 * Note the special "update_monsters()" function which can be used to
 * call this function once for every monster.
 *
 * Note the "full" flag which requests that the "cdis" field be updated;
 * this is only needed when the monster (or the player) has moved.
 *
 * Every time a monster moves, we must call this function for that
 * monster, and update the distance, and the visibility.  Every time
 * the player moves, we must call this function for every monster, and
 * update the distance, and the visibility.  Whenever the player "state"
 * changes in certain ways ("blindness", "infravision", "telepathy",
 * and "see invisible"), we must call this function for every monster,
 * and update the visibility.
 *
 * Routines that change the "illumination" of a grid must also call this
 * function for any monster in that grid, since the "visibility" of some
 * monsters may be based on the illumination of their grid.
 *
 * Note that this function is called once per monster every time the
 * player moves.  When the player is running, this function is one
 * of the primary bottlenecks, along with "update_view()" and the
 * "process_monsters()" code, so efficiency is important.
 *
 * Note the optimized "inline" version of the "distance()" function.
 *
 * A monster is "visible" to the player if (1) it has been detected
 * by the player, (2) it is close to the player and the player has
 * telepathy, or (3) it is close to the player, and in line of sight
 * of the player, and it is "illuminated" by some combination of
 * infravision, torch light, or permanent light (invisible monsters
 * are only affected by "light" if the player can see invisible).
 *
 * Monsters which are not on the current panel may be "visible" to
 * the player, and their descriptions will include an "offscreen"
 * reference.  Currently, offscreen monsters cannot be targeted
 * or viewed directly, but old targets will remain set.  XXX XXX
 *
 * The player can choose to be disturbed by several things, including
 * "OPT(player, disturb_near)" (monster which is "easily" viewable moves in some
 * way).  Note that "moves" includes "appears" and "disappears".
 */
void update_mon(struct monster *mon, struct chunk *c, bool full)
{
	struct monster_lore *lore;

	int d;

	/* If still generating the level, measure distances from the middle */
	struct loc pgrid = character_dungeon ? player->mon.grid :
		loc(c->width / 2, c->height / 2);

	/* Seen at all */
	bool flag = false;

	/* Seen by vision */
	bool easy = false;

	/* ESP permitted */
	bool telepathy_ok = player_of_has(player, OF_TELEPATHY);

	assert(mon != NULL);

	/* Return if this is not the current level */
	if (c != cave) {
		return;
	}

	lore = get_lore(mon->race);
	
	/* Compute distance, or just use the current one */
	if (full) {
		/* Distance components */
		int dy = ABS(pgrid.y - mon->grid.y);
		int dx = ABS(pgrid.x - mon->grid.x);

		/* Approximate distance */
		d = (dy > dx) ? (dy + (dx >>  1)) : (dx + (dy >> 1));

		/* Restrict distance */
		if (d > 255) d = 255;

		/* Save the distance */
		mon->cdis = d;
	} else {
		/* Extract the distance */
		d = mon->cdis;
	}

	/* Detected */
	if (mflag_has(mon->mflag, MFLAG_MARK)) flag = true;

	/* Check if telepathy works here */
	if (square_isno_esp(c, mon->grid) || square_isno_esp(c, pgrid)) {
		telepathy_ok = false;
	}

	/* Nearby */
	if (d <= z_info->max_sight) {
		/* Basic telepathy */
		if (telepathy_ok && monster_is_esp_detectable(mon)) {
			/* Detectable */
			flag = true;

			/* Check for LOS so that MFLAG_VIEW is set later */
			if (square_isview(c, mon->grid)) easy = true;
		}

		/* Normal line of sight and player is not blind */
		if (square_isview(c, mon->grid) && !player->mon.m_timed[TMD_BLIND]) {
			/* Use "infravision" */
			if (d <= player->mon.state.see_infra) {
				/* Learn about warm/cold blood */
				rf_on(lore->flags, RF_COLD_BLOOD);

				/* Handle "warm blooded" monsters */
				if (!rf_has(mon->race->flags, RF_COLD_BLOOD)) {
					/* Easy to see */
					easy = flag = true;
				}
			}

			/* Use illumination */
			if (square_isseen(c, mon->grid)) {
				/* Learn about invisibility */
				rf_on(lore->flags, RF_INVISIBLE);

				/* Handle invisibility */
				if (monster_is_invisible(mon)) {
					/* See invisible */
					if (player_of_has(player, OF_SEE_INVIS)) {
						/* Easy to see */
						easy = flag = true;
					}
				} else {
					/* Easy to see */
					easy = flag = true;
				}
			}

			/* Learn about intervening squares */
			path_analyse(c, mon->grid);
		}
	}

	/* If a mimic looks like an ignored item, it's not seen */
	if (monster_is_mimicking(mon)) {
		struct object *obj = mon->mimicked_obj;
		if (ignore_item_ok(player, obj))
			easy = flag = false;
	}

	/* Is the monster is now visible? */
	if (flag) {
		/* Learn about the monster's mind */
		if (telepathy_ok) {
			flags_set(lore->flags, RF_SIZE, RF_EMPTY_MIND, RF_WEIRD_MIND,
					  RF_SMART, RF_STUPID, FLAG_END);
		}

		/* It was previously unseen */
		if (!monster_is_visible(mon)) {
			/* Mark as visible */
			mflag_on(mon->mflag, MFLAG_VISIBLE);

			// L: mark as having ever been seen
			mflag_on(mon->mflag, MFLAG_KNOWN);

			/* Draw the monster */
			square_light_spot(c, mon->grid);

			/* Update health bar as needed */
			if (player->upkeep->health_who == mon) {
				player->upkeep->redraw |= (PR_HEALTH);
			}

			/* Hack -- Count "fresh" sightings */
			if (lore->sights < SHRT_MAX) {
				lore->sights++;
			}

			/* Window stuff */
			player->upkeep->redraw |= PR_MONLIST;
		}
	} else if (monster_is_visible(mon)) {
		/* Not visible but was previously seen - treat mimics differently */
		if (!mon->mimicked_obj
				|| ignore_item_ok(player, mon->mimicked_obj)) {
			/* Mark as not visible */
			mflag_off(mon->mflag, MFLAG_VISIBLE);

			/* Erase the monster */
			square_light_spot(c, mon->grid);

			/* Update health bar as needed */
			if (player->upkeep->health_who == mon) {
				player->upkeep->redraw |= (PR_HEALTH);
			}

			/* Window stuff */
			player->upkeep->redraw |= PR_MONLIST;
		}
	}


	/* Is the monster is now easily visible? */
	if (easy) {
		/* Change */
		if (!monster_is_in_view(mon)) {
			/* Mark as easily visible */
			mflag_on(mon->mflag, MFLAG_VIEW);

			/* Disturb on appearance */
			if (OPT(player, disturb_near))
				disturb(player);

			/* Re-draw monster window */
			player->upkeep->redraw |= PR_MONLIST;
		}
	} else {
		/* Change */
		if (monster_is_in_view(mon)) {
			/* Mark as not easily visible */
			mflag_off(mon->mflag, MFLAG_VIEW);

			// L: lose track of the monster
			mflag_off(mon->mflag, MFLAG_SPOTTED);

			/* Disturb on disappearance */
			if (OPT(player, disturb_near) && !monster_is_camouflaged(mon))
				disturb(player);

			/* Re-draw monster list window */
			player->upkeep->redraw |= PR_MONLIST;
		}
	}
}

/**
 * Updates all the (non-dead) monsters via update_mon().
 */
void update_monsters(bool full)
{
	int i;

	/* Update each (live) monster */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Update the monster if alive */
		if (mon->race) {
			update_mon(mon, cave, full);
		}
	}
}


/**
 * ------------------------------------------------------------------------
 * Monster (and player) actual movement
 * ------------------------------------------------------------------------ */
static void mon_leaving(struct loc grid1, struct loc grid2)
{
	//struct monster *mon = cave_monster(cave, square(cave, grid2)->mon);
	//bool reveal, destroy;
	//struct loc target;

	/*if (!mon_will_attack_player(mon, player)) {
		for (target.x = grid2.x - 1; target.x <= grid2.x + 1; ++target.x) {
			for (target.y = grid2.y - 1; target.y <= grid2.y + 1; ++target.y) {
				struct square *sq = square(cave, target);
				if (!square_in_bounds_fully(cave, target)) continue;
				if (sq->feat == FEAT_ILLUSORY_WALL) {
					square_true_memorize(cave, target);
				}
			}
		}
	}*/

	if (square(cave, grid2)->feat == FEAT_ILLUSORY_WALL) {
		square_force_floor(cave, grid2);
		player->upkeep->update |= PU_UPDATE_VIEW;
	}
}

/**
 * Called when the player has just left grid1 for grid2.
 */
static void player_leaving(struct loc grid1, struct loc grid2)
{
	struct loc decoy = cave_find_decoy(cave);

	/* Decoys get destroyed if player is too far away */
	if (!loc_is_zero(decoy) &&
		distance(decoy, grid2) > z_info->max_sight) {
		square_destroy_decoy(cave, decoy);
	}

	if (square(cave, grid2)->feat == FEAT_ILLUSORY_WALL) {
		square_force_floor(cave, grid2);
	}

	/* Delayed traps trigger when the player leaves. */
	hit_trap(grid1, 1);
}

/**
 * Is a helper function to move a mimicked object when the mimic (not known
 * to the player) is moved.  Assumes that the caller will be calling
 * square_light_spot() for the source grid.
 */
static void move_mimicked_object(struct chunk *c, struct monster *mon,
	struct loc src, struct loc dest)
{
	struct object *mimicked = mon->mimicked_obj;
	/*
	 * Move a copy so, if necessary, the original can remain as a
	 * placeholder for the known version of the object in the player's
	 * view of the cave.
	 */
	struct object *moved = object_new();
	bool dummy = true;

	assert(mimicked);
	object_copy(moved, mimicked);
	moved->oidx = 0;
	mimicked->mimicking_m_idx = 0;
	if (mimicked->known) {
		moved->known = object_new();
		object_copy(moved->known, mimicked->known);
		moved->known->oidx = 0;
		moved->known->grid = loc(0,0);
	}
	if (floor_carry(c, dest, moved, &dummy)) {
		mon->mimicked_obj = moved;
	} else {
		/* Could not move the object so cancel mimicry. */
		moved->mimicking_m_idx = 0;
		mon->mimicked_obj = NULL;
		/* Give object to monster if appropriate; otherwise, delete. */
		if (!rf_has(mon->race->flags, RF_MIMIC_INV) ||
			!monster_carry(c, mon, moved)) {
			struct chunk *p_c = (c == cave) ? player->cave : NULL;
			if (moved->known) {
				object_delete(p_c, NULL, &moved->known);
			}
			object_delete(c, p_c, &moved);
		}
	}
	square_delete_object(c, src, mimicked, true, false);
}

/**
 * Swap the players/monsters (if any) at two locations.
 */
void monster_swap(struct loc grid1, struct loc grid2)
{
	int m1, m2;
	struct monster *mon;
	struct loc pgrid = player->mon.grid;

	/* Monsters */
	m1 = cave->squares[grid1.y][grid1.x].mon;
	m2 = cave->squares[grid2.y][grid2.x].mon;

	/* Update grids */
	square_set_mon(cave, grid1, m2);
	square_set_mon(cave, grid2, m1);

	/* Monster 1 */
	if (m1 > 0) {
		/* Monster */
		mon = cave_monster(cave, m1);

		/* Update monster */
		if (monster_is_camouflaged(mon)) {
			/*
			 * Become aware if the player can see the grid with
			 * the camouflaged monster before or after the swap.
			 */
			if (monster_is_in_view(mon) ||
					(m2 >= 0 && los(cave, pgrid, grid2)) ||
					(m2 < 0 && los(cave, grid1, grid2))) {
				become_aware(cave, mon);
			} else if (monster_is_mimicking(mon)) {
				move_mimicked_object(cave, mon, grid1, grid2);
				player->upkeep->redraw |= (PR_ITEMLIST);
			}
		}
		mon->grid = grid2;
		update_mon(mon, cave, true);
		mon_leaving(grid1, mon->grid);

		/* Affect light? */
		if (mon->race->light != 0) {
			player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;
		}

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);
	} else if (m1 < 0) {
		/* Player */
		player->mon.grid = grid2;
		player_leaving(pgrid, player->mon.grid);

		/* Update the trap detection status */
		player->upkeep->redraw |= (PR_DTRAP);

		/* Updates */
		player->upkeep->update |= (PU_PANEL | PU_UPDATE_VIEW | PU_DISTANCE);

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST | PR_MANA);

		// L: reset turns spent not moving
		player->search_turn = 0;

		/* Don't allow command repeat if moved away from item used. */
		cmd_disable_repeat_floor_item();
	}

	/* Monster 2 */
	if (m2 > 0) {
		/* Monster */
		mon = cave_monster(cave, m2);

		/* Update monster */
		if (monster_is_camouflaged(mon)) {
			/*
			 * Become aware if the player can see the grid with
			 * the camouflaged monster before or after the swap.
			 */
			if (monster_is_in_view(mon) ||
					(m1 >= 0 && los(cave, pgrid, grid1)) ||
					(m1 < 0 && los(cave, grid2, grid1))) {
				become_aware(cave, mon);
			} else if (monster_is_mimicking(mon)) {
				move_mimicked_object(cave, mon, grid2, grid1);
				player->upkeep->redraw |= (PR_ITEMLIST);
			}
		}
		mon->grid = grid1;
		update_mon(mon, cave, true);
		mon_leaving(grid2, mon->grid);

		/* Affect light? */
		if (mon->race->light != 0) {
			player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;
		}

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);
	} else if (m2 < 0) {
		/* Player */
		player->mon.grid = grid1;
		player_leaving(pgrid, player->mon.grid);

		/* Update the trap detection status */
		player->upkeep->redraw |= (PR_DTRAP);

		/* Updates */
		player->upkeep->update |= (PU_PANEL | PU_UPDATE_VIEW | PU_DISTANCE);

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST | PR_MANA);

		// L: reset turns spent not moving
		player->search_turn = 0;

		/* Don't allow command repeat if moved away from item used. */
		cmd_disable_repeat_floor_item();
	}

	/* Redraw */
	square_light_spot(cave, grid1);
	square_light_spot(cave, grid2);
}

/**
 * ------------------------------------------------------------------------
 * Awareness and learning
 * ------------------------------------------------------------------------ */
/**
 * Monster wakes up and possibly becomes aware of the player
 */
void monster_wake(struct monster *mon, bool notify, int aware_chance)
{
	int flag = notify ? MON_TMD_FLG_NOTIFY : MON_TMD_FLG_NOMESSAGE;
	mon_clear_timed(mon, TMD_ASLEEP, flag);
	if (randint0(100) < aware_chance) {
		mflag_on(mon->mflag, MFLAG_AWARE);
	}
	mon_check_target(cave, mon);
}

/**
 * Monster can see a grid
 */
bool monster_can_see(struct chunk *c, struct monster *mon, struct loc grid)
{
	return los(c, mon->grid, grid);
}

/**
 * Make player fully aware of the given mimic.
 *
 * \param c Is the chunk with the monster.
 * \param mon Is the monster.
 * When a player becomes aware of a mimic, we update the monster memory
 * and delete the "fake item" that the monster was mimicking.
 */
void become_aware(struct chunk *c, struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);

	if (mflag_has(mon->mflag, MFLAG_CAMOUFLAGE)) {
		mflag_off(mon->mflag, MFLAG_CAMOUFLAGE);

		/* Learn about mimicry */
		if (rf_has(mon->race->flags, RF_UNAWARE)) {
			rf_on(lore->flags, RF_UNAWARE);
		}

		/* Delete any false items */
		if (mon->mimicked_obj) {
			struct object *obj = mon->mimicked_obj;
			char o_name[80];
			object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, player);

			/* Print a message */
			if (square_isseen(c, obj->grid))
				msg("The %s was really a monster!", o_name);

			/* Clear the mimicry */
			obj->mimicking_m_idx = 0;
			mon->mimicked_obj = NULL;

			/*
			 * Give a copy of the object to the monster if
			 * appropriate.
			 */
			if (rf_has(mon->race->flags, RF_MIMIC_INV)) {
				struct object* given = object_new();

				object_copy(given, obj);
				given->oidx = 0;
				if (obj->known) {
					given->known = object_new();
					object_copy(given->known, obj->known);
					given->known->oidx = 0;
					given->known->grid = loc(0, 0);
				}
				if (!monster_carry(c, mon, given)) {
					struct chunk *p_c = (c == cave) ? player->cave : NULL;
					if (given->known) {
						object_delete(p_c, NULL, &given->known);
					}
					object_delete(c, p_c, &given);
				}
			}

			/*
			 * Delete the mimicked object; noting and lighting
			 * done below outside of the if block.
			 */
			square_delete_object(c, obj->grid, obj, false, false);

			/* Since mimicry affects visibility, update that. */
			update_mon(mon, c, false);
		}

		/* Update monster and item lists */
		if (mon->race->light != 0) {
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		}
		player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);
	}

	square_note_spot(c, mon->grid);
	square_light_spot(c, mon->grid);
}

/**
 * The given monster learns about an "observed" resistance or other player
 * state property, or lack of it.
 *
 * Note that this function is robust to being called with `element` as an
 * arbitrary PROJ_ type
 */
void update_smart_learn(struct monster *mon, struct player *p, int flag,
						int pflag, int element)
{
	bool element_ok = ((element >= 0) && (element < ELEM_MAX));

	/* Sanity check */
	if (!flag && !element_ok) return;

	/* Anything a monster might learn, the player should learn */
	if (flag) equip_learn_flag(p, flag);
	if (element_ok) equip_learn_element(p, element);

	/* Not allowed to learn */
	if (!OPT(p, birth_ai_learn)) return;

	/* Too stupid to learn anything */
	if (monster_is_stupid(mon)) return;

	/* Not intelligent, only learn sometimes */
	if (!monster_is_smart(mon) && one_in_(2)) return;

	/* Analyze the knowledge; fail very rarely */
	if (one_in_(100)) {
		return;
	}

	/* Learn the flag */
	if (flag) {
		if (player_of_has(p, flag)) {
			of_on(mon->known_pstate.flags, flag);
		} else {
			of_off(mon->known_pstate.flags, flag);
		}
	}

	/* Learn the pflag */
	if (pflag) {
		if (pf_has(p->mon.state.pflags, pflag)) {
			of_on(mon->known_pstate.pflags, pflag);
		} else {
			of_off(mon->known_pstate.pflags, pflag);
		}
	}

	/* Learn the element */
	if (element_ok) {
		mon->known_pstate.el_info[element].res_level
			= p->mon.state.el_info[element].res_level;
	}
}

/**
 * ------------------------------------------------------------------------
 * Monster healing
 * ------------------------------------------------------------------------ */
#define MAX_KIN_RADIUS			5
#define MAX_KIN_DISTANCE		5

/**
 * Given a dungeon chunk, a monster, and a location, see if there is
 * an injured monster with the same base kind in LOS and less than
 * MAX_KIN_DISTANCE away.
 */
static struct monster *get_injured_kin(struct chunk *c,
									   const struct monster *mon,
									   struct loc grid)
{
	/* Ignore the monster itself */
	if (loc_eq(grid, mon->grid))
		return NULL;

	/* Check kin */
	struct monster *kin = square_monster(c, grid);
	if (!kin)
		return NULL;

	if (kin->race->base != mon->race->base)
		return NULL;

	/* Check line of sight */
	if (los(c, mon->grid, grid) == false)
		return NULL;

	/* Check injury */
	if (kin->hp == kin->maxhp)
		return NULL;

	/* Check distance */
	if (distance(mon->grid, grid) > MAX_KIN_DISTANCE)
		return NULL;

	return kin;
}

/**
 * Find out if there are any injured monsters nearby.
 *
 * See get_injured_kin() above for more details on what monsters qualify.
 */
bool find_any_nearby_injured_kin(struct chunk *c, const struct monster *mon)
{
	struct loc grid;
	for (grid.y = mon->grid.y - MAX_KIN_RADIUS;
		 grid.y <= mon->grid.y + MAX_KIN_RADIUS; grid.y++) {
		for (grid.x = mon->grid.x - MAX_KIN_RADIUS;
			 grid.x <= mon->grid.x + MAX_KIN_RADIUS; grid.x++) {
			if (get_injured_kin(c, mon, grid) != NULL) {
				return true;
			}
		}
	}

	return false;
}

/**
 * Choose one injured monster of the same base in LOS of the provided monster.
 *
 * Scan MAX_KIN_RADIUS grids around the monster to find potential grids,
 * using reservoir sampling with k = 1 to find a random one.
 */
struct monster *choose_nearby_injured_kin(struct chunk *c,
                                          const struct monster *mon)
{
	struct loc grid;
	int nseen = 0;
	struct monster *found = NULL;

	for (grid.y = mon->grid.y - MAX_KIN_RADIUS;
		 grid.y <= mon->grid.y + MAX_KIN_RADIUS; grid.y++) {
		for (grid.x = mon->grid.x - MAX_KIN_RADIUS;
			 grid.x <= mon->grid.x + MAX_KIN_RADIUS; grid.x++) {
			struct monster *kin = get_injured_kin(c, mon, grid);
			if (kin) {
				nseen++;
				if (!randint0(nseen))
					found = kin;
			}
		}
	}

	return found;
}


/**
 * ------------------------------------------------------------------------
 * Monster damage and death utilities
 * ------------------------------------------------------------------------ */

static int first_slot_with_object_equipped(struct player_body *body)
{
	uint16_t i;

	for (i = 0; i < body->count; ++i) {
		struct object *obj = body->slots[i].obj;

		if (obj) return i;
	}

	return 0;
}

/**
 * Handles the "death" of a monster.
 *
 * Disperses treasures carried by the monster centered at the monster location.
 * Note that objects dropped may disappear in crowded rooms.
 *
 * Checks for "Quest" completion when a quest monster is killed.
 *
 * Note that only the player can induce "monster_death()" on Uniques.
 * Thus (for now) all Quest monsters should be Uniques.
 *
 * If `stats` is true, then we skip updating the monster memory. This is
 * used by stats-generation code, for efficiency.
 */
void monster_death(struct monster *mon, struct player *p, bool stats)
{
	int dump_item = 0;
	int dump_gold = 0;
	bool visible = monster_is_visible(mon) || monster_is_unique(mon);

	/* Delete any mimicked objects */
	if (mon->mimicked_obj) {
		square_delete_object(cave, mon->grid, mon->mimicked_obj, true, true);
		mon->mimicked_obj = NULL;
	}

	/* Drop objects being carried */
	while (true) {
		struct object *obj;
		int slot = first_slot_with_object_equipped(&mon->body);
		if (mon->held_obj) {
			obj = mon->held_obj;
			pile_excise(&mon->held_obj, obj);
		}
		else if (slot > 0) {
			obj = mon->body.slots[slot].obj;
			mon->body.slots[slot].obj = NULL;
		}
		else {
			break;
		}

		/* Object no longer held */
		obj->held_m_idx = 0;

		/* Count it and drop it - refactor once origin is a bitflag */
		if (!stats) {
			if (tval_is_money(obj) && (obj->origin != ORIGIN_STOLEN)) {
				dump_gold++;
			} else if (!tval_is_money(obj) && ((obj->origin == ORIGIN_DROP)
					|| (obj->origin == ORIGIN_DROP_PIT)
					|| (obj->origin == ORIGIN_DROP_VAULT)
					|| (obj->origin == ORIGIN_DROP_SUMMON)
					|| (obj->origin == ORIGIN_DROP_SPECIAL)
					|| (obj->origin == ORIGIN_DROP_BREED)
					|| (obj->origin == ORIGIN_DROP_POLY)
					|| (obj->origin == ORIGIN_DROP_WIZARD))) {
				dump_item++;
			}
		}

		/* Change origin if monster is invisible, unless we're in stats mode */
		if (!visible && !stats) {
			obj->origin = ORIGIN_DROP_UNKNOWN;
		}

		drop_near(cave, &obj, 0, mon->grid, true, false);
	}

	/* Forget objects */
	mon->held_obj = NULL;

	/* Take note of any dropped treasure */
	if (visible && (dump_item || dump_gold)) {
		lore_treasure(mon, dump_item, dump_gold);
	}

	/* Update monster list window */
	p->upkeep->redraw |= PR_MONLIST;

	/* Check if we finished a quest */
	quest_check(p, mon);
}

/**
 * Handle the consequences of the killing of a monster by the player
 */
static void player_kill_monster(struct monster *mon, struct player *p,
		const char *note)
{
	int32_t div, new_exp, new_exp_frac;
	struct monster_lore *lore = get_lore(mon->race);
	char m_name[80];
	char buf[80];
	int desc_mode = MDESC_DEFAULT | ((note) ? MDESC_COMMA : 0);

	/* Assume normal death sound */
	int soundfx = MSG_KILL;

	/* Extract monster name */
	monster_desc(m_name, sizeof(m_name), mon, desc_mode);

	/* Shapechanged monsters revert on death */
	if (mon->original_race) {
		monster_revert_shape(mon);
		lore = get_lore(mon->race);
		monster_desc(m_name, sizeof(m_name), mon, desc_mode);
	}

	/* Play a special sound if the monster was unique */
	if (monster_is_unique(mon)) {
		if (mon->race->base == lookup_monster_base("Morgoth")) {
			soundfx = MSG_KILL_KING;
		} else {
			soundfx = MSG_KILL_UNIQUE;
		}
	}

	/* Death message */
	if (note) {
		if (strlen(note) <= 1) {
			/* Death by Spell attack - messages handled by project_m() */
		} else {
			/* Make sure to flush any monster messages first */
			notice_stuff(p);

			/* Death by Missile attack */
			my_strcap(m_name);
			msgt(soundfx, "%s%s", m_name, note);
		}
	} else {
		/* Make sure to flush any monster messages first */
		notice_stuff(p);

		if (!monster_is_visible(mon)) {
			/* Death by physical attack -- invisible monster */
			msgt(soundfx, "You have killed %s.", m_name);
		} else if (monster_is_destroyed(mon)) {
			/* Death by Physical attack -- non-living monster */
			msgt(soundfx, "You have destroyed %s.", m_name);
		} else {
			/* Death by Physical attack -- living monster */
			msgt(soundfx, "You have slain %s.", m_name);
		}
	}

	/* Player level */
	div = p->lev;

	/* Give some experience for the kill */
	new_exp = ((long)mon->race->mexp * mon->race->level) / div;

	/* Handle fractional experience */
	new_exp_frac = ((((long)mon->race->mexp * mon->race->level) % div)
					* 0x10000L / div);

	/* When the player kills a Unique, it stays dead */
	if (monster_is_unique(mon)) {
		char unique_name[80];
		assert(mon->original_race == NULL);
		mon->race->max_num = 0;

		/*
		 * This gets the correct name if we slay an invisible
		 * unique and don't have See Invisible.
		 */
		monster_desc(unique_name, sizeof(unique_name), mon,
					 MDESC_DIED_FROM);

		/* Log the slaying of a unique */
		strnfmt(buf, sizeof(buf), "Killed %s", unique_name);
		history_add(p, buf, HIST_SLAY_UNIQUE);
	}

	/* Gain experience */
	player_exp_gain(p, new_exp, new_exp_frac);

	/* Generate treasure */
	monster_death(mon, p, false);

	/* Bloodlust bonus */
	if (p->mon.m_timed[TMD_BLOODLUST]) {
		check_berserk(p, mon);
		//player_inc_timed(p, TMD_BLOODLUST, 10, false, false, true);
		player_over_exert(p, PY_EXERT_CONF, 5, 2);
		//player_over_exert(p, PY_EXERT_HALLU, 10, 15);
	}

	/* Recall even invisible uniques or winners */
	if (monster_is_visible(mon) || monster_is_unique(mon)) {
		/* Count kills this life */
		if (lore->pkills < SHRT_MAX) lore->pkills++;

		/* Count kills in all lives */
		if (lore->tkills < SHRT_MAX) lore->tkills++;

		/* Update lore and tracking */
		lore_update(mon->race, lore);
		monster_race_track(p->upkeep, mon->race);
	}

	/* Delete the monster */
	delete_monster_idx(cave, mon->midx);
}

/**
 * See how a monster reacts to damage
 */
static bool monster_scared_by_damage(struct monster *mon, int dam)
{
	int current_fear = mon->m_timed[TMD_AFRAID];

	/* Pain can reduce or cancel existing fear, or cause fear */
	if (current_fear) {
		int tmp = randint1(dam);

		/* Cure a little or all fear */
		if (tmp < current_fear) {
			/* Reduce fear */
			mon_dec_timed(mon, TMD_AFRAID, tmp, MON_TMD_FLG_NOMESSAGE);
		} else {
			/* Cure fear */
			mon_clear_timed(mon, TMD_AFRAID, MON_TMD_FLG_NOMESSAGE);
			return false;
		}
	} else if (monster_can_be_scared(mon)) {
		/* Percentage of fully healthy */
		int percentage = (100L * mon->hp) / mon->maxhp;

		/* Run (sometimes) if at 10% or less of max hit points... */
		bool low_hp = randint1(10) >= percentage;

		/* ...or (usually) when hit for half its current hit points */
		bool big_hit = (dam >= mon->hp) && (randint0(100) < 80);

		if (low_hp || big_hit) {
			int time = randint1(10);
			if ((dam >= mon->hp) && (percentage > 7)) {
				time += 20;
			} else {
				time += (11 - percentage) * 5;
			}

			/* Note fear */
			mon_inc_timed(mon, TMD_AFRAID, time,
						  MON_TMD_FLG_NOMESSAGE | MON_TMD_FLG_NOFAIL);
			return true;
		}
	}
	return false;
}

/**
 * Deal damage to a monster from another monster (or at least not the player).
 *
 * This is a helper for melee handlers. It is very similar to mon_take_hit(),
 * but eliminates the player-oriented stuff of that function.
 *
 * \param context is the project_m context.
 * \param hurt_msg is the message if the monster is hurt (if any).
 * \return true if the monster died, false if it is still alive.
 */
bool mon_take_nonplayer_hit(int dam, struct monster *t_mon,
							enum mon_messages hurt_msg,
							enum mon_messages die_msg,
							bool skipmsg)
{
	assert(t_mon);

	/* "Unique" or arena monsters can only be "killed" by the player */
	/*if (monster_is_unique(t_mon) || player->upkeep->arena_level) {
		// Reduce monster hp to zero, but don't kill it.
		if (dam > t_mon->hp) dam = t_mon->hp;
	}*/

	/* Redraw (later) if needed */
	if (player->upkeep->health_who == t_mon)
		player->upkeep->redraw |= (PR_HEALTH);

	/* Wake the monster up, doesn't become aware of the player */
	monster_wake(t_mon, false, 0);

	/* Hurt the monster */
	t_mon->hp -= dam;

	if (t_mon->hp < 0 && rf_has(t_mon->race->flags, RF_PHOENIX_RESURRECT)) {
		bool id;
		effect_simple(EF_REBIRTH, source_monster(t_mon->midx), "1d1", 0, 0, 0, 0, 0, &id);
	}

	/* Dead or damaged monster */
	if (t_mon->hp < 0) {
		/* Death message */
		add_monster_message(t_mon, die_msg, false);

		/* Generate treasure, etc */
		monster_death(t_mon, player, false);

		/* Delete the monster */
		delete_monster_idx(cave, t_mon->midx);

		return true;
	} else if (!monster_is_camouflaged(t_mon)) {
		/* Give detailed messages if visible */
		if (hurt_msg != MON_MSG_NONE) {
			add_monster_message(t_mon, hurt_msg, false);
		} else if (dam > 0 && !skipmsg) {
			message_pain(t_mon, dam);
		}
	}

	/* Sometimes a monster gets scared by damage */
	if (!t_mon->m_timed[TMD_AFRAID] && dam > 0) {
		(void) monster_scared_by_damage(t_mon, dam);
	}

	return false;
}

/**
 * Decreases a monster's hit points by `dam` and handle monster death.
 *
 * Hack -- we "delay" fear messages by passing around a "fear" flag.
 *
 * We announce monster death (using an optional "death message" (`note`)
 * if given, and a otherwise a generic killed/destroyed message).
 *
 * Returns true if the monster has been killed (and deleted).
 *
 * TODO: Consider decreasing monster experience over time, say, by using
 * "(m_exp * m_lev * (m_lev)) / (p_lev * (m_lev + n_killed))" instead
 * of simply "(m_exp * m_lev) / (p_lev)", to make the first monster
 * worth more than subsequent monsters.  This would also need to
 * induce changes in the monster recall code.  XXX XXX XXX
 **/
bool mon_take_hit(struct monster *mon, struct player *p, int dam, bool *fear,
		const char *note)
{
	/* Redraw (later) if needed */
	if (p->upkeep->health_who == mon) {
		p->upkeep->redraw |= (PR_HEALTH);
	}

	/* If the hit doesn't kill, wake it up, make it aware of the player */
	if (dam <= mon->hp) {
		monster_wake(mon, false, 100);
		mon_clear_timed(mon, TMD_PARALYZED, MON_TMD_FLG_NOTIFY);
	}

	// L: monster usually becomes aware of the player
	if (monster_can_see_player(mon) || monster_can_smell(mon)) {
		monster_become_aware(mon);
	}

	// L: make it angry
	monster_attacked_get_angry(mon, p, dam);

	/* Become aware of its presence */
	if (monster_is_camouflaged(mon)) {
		become_aware(cave, mon);
	}

	/* No damage, we're done */
	if (dam == 0) return false;

	/* Covering tracks is no longer possible */
	p->mon.m_timed[TMD_COVERTRACKS] = 0;

	/* Hurt it */
	mon->hp -= dam;

	if (mon->hp < 0 && rf_has(mon->race->flags, RF_PHOENIX_RESURRECT)) {
		bool id;
		effect_simple(EF_REBIRTH, source_monster(mon->midx), "1d1", 0, 0, 0, 0, 0, &id);
	}

	if (mon->hp < 0) {
		/* Deal with arena monsters */
		if (p->upkeep->arena_level) {
			p->upkeep->generate_level = true;
			p->upkeep->health_who = mon;
			(*fear) = false;
			return true;
		}

		/* It is dead now */
		player_kill_monster(mon, p, note);

		/* Not afraid */
		(*fear) = false;

		/* Monster is dead */
		return true;
	} else {
		/* Did it get frightened? */
		(*fear) = monster_scared_by_damage(mon, dam);

		/* Not dead yet */
		return false;
	}
}

void kill_arena_monster(struct monster *mon)
{
	struct monster *old_mon = cave_monster(cave, mon->midx);
	assert(old_mon);
	update_mon(old_mon, cave, true);
	old_mon->hp = -1;
	player_kill_monster(old_mon, player, " is defeated!");
}

/**
 * Terrain damages monster
 */
void monster_take_terrain_damage(struct monster *mon)
{
	/* Damage the monster */
	if (square_isfiery(cave, mon->grid)) {
		bool fear = false;
		int res_level = mon->race->el_info[ELEM_FIRE].res_level;

		if (res_level < 3) {//!rf_has(mon->race->flags, RF_IM_FIRE)) {
			mon_take_nonplayer_hit(100 + randint1(100), mon, MON_MSG_CATCH_FIRE,
								   MON_MSG_DISINTEGRATES, false);
		}

		if (fear && monster_is_visible(mon)) {
			add_monster_message(mon, MON_MSG_FLEE_IN_TERROR, true);
		}
	}
}

void monster_take_timed_damage(struct monster *mon, int energy)
{
	if (mon->m_timed[TMD_POISONED] > 0) {
		int pois1 = (mon->m_timed[TMD_POISONED] + 3) / 4;
		int pois2 = (mon->m_timed[TMD_POISONED] + 5) / 4;
		int pdam = (pois1 * pois2 + energy - 1) / energy;
		if (pdam > 0) {
			mon_take_nonplayer_hit(pdam, mon, MON_MSG_NONE, MON_MSG_COLLAPSE, true);
		}
	}
	if (mon->m_timed[TMD_SUFFOCATE] > 0) {
		int suff = mon->m_timed[TMD_SUFFOCATE] * 2;
		int sdam = (suff + 50 + energy - 1) / energy;
		if (sdam > 0) {
			mon_take_nonplayer_hit(sdam, mon, MON_MSG_NONE, MON_MSG_COLLAPSE, true);
		}
	}
}

/**
 * Terrain is currently damaging monster
 */
bool monster_taking_terrain_damage(struct chunk *c, struct monster *mon)
{
	if (square_isdamaging(c, mon->grid) &&
		!rf_has(mon->race->flags, square_feat(c, mon->grid)->resist_flag)) {
		return true;
	}

	return false;
}


struct object *monster_best_takeable_item(struct chunk *c, struct monster *mon, int danger) {
    if (!rf_has(mon->race->flags, RF_TAKE_ITEM)) return NULL;
	int i, score, dist, bestscore;
	struct object *obj;
	struct object *best = NULL;

	for (i = 0; i < c->obj_max; i++) {
		obj = c->objects[i];
        if (!obj) continue;
		//if (tval_is_money(obj)) continue;
		if (obj->mimicking_m_idx) continue;
		if (react_to_slay(obj, mon)) continue;
        if (!los(cave, mon->grid, obj->grid)) continue;

		dist = distance(mon->grid, obj->grid) + 1;
		dist = MAX(dist, 1);
		score = object_value_real(obj, obj->number) * 2 / (dist * dist + dist) - danger - mon->race->level;

		if (score > bestscore) {
			bestscore = score;
			best = obj;
		}
	}
	return best;
}



/**
 * ------------------------------------------------------------------------
 * Monster inventory utilities
 * ------------------------------------------------------------------------ */
/**
 * Add the given object to the given monster's inventory.
 *
 * Currently always returns true - it is left as a bool rather than
 * void in case a limit on monster inventory size is proposed in future.
 */
bool monster_carry(struct chunk *c, struct monster *mon, struct object *obj)
{
	struct object *held_obj;

	if (player->cave && player->cave->objects) {
		assert(!player->cave->objects[obj->oidx] || player->cave->objects[obj->oidx] == obj->known);
	}

	// L: flag the monster as wanting to recheck its equipment
	mflag_on(mon->mflag, MFLAG_CHECK_EQ);

	/* Scan objects already being held for combination */
	for (held_obj = mon->held_obj; held_obj; held_obj = held_obj->next) {
		/* Check for combination */
		if (object_mergeable(held_obj, obj, OSTACK_MONSTER)) {
			/* Combine the items */
			object_absorb(held_obj, obj);
			
			if (player->cave && player->cave->objects) {
				assert(!player->cave->objects[obj->oidx] || player->cave->objects[obj->oidx] == obj->known);
			}

			/* Result */
			return true;
		}
	}

	/* Forget location */
	obj->grid = loc(0, 0);

	/* Link the object to the monster */
	obj->held_m_idx = mon->midx;

	/* Add the object to the monster's inventory */
	list_object(c, obj);

	if (obj->known) {
		obj->known->oidx = obj->oidx;
	}

	if (player && player->cave && player->cave->objects) {
		player->cave->objects[obj->oidx] = obj->known;
	}

	pile_insert(&mon->held_obj, obj);

	/* Result */
	return true;
}

bool monster_equip(struct chunk *c, struct monster *mon, struct object *obj)
{
	uint16_t i;
	int slot = -1;

	// L: flag the monster as wanting to recheck its equipment
	mflag_on(mon->mflag, MFLAG_CHECK_EQ);

	for (i = 0; i < mon->body.count; ++i) {
		if (mon->body.slots[i].obj) continue;
		if (wield_slot_type(obj) != mon->body.slots[i].type) continue;

		slot = i;
		break;
	}

	if (slot == -1) return false;

	/* Forget location */
	obj->grid = loc(0, 0);

	/* Link the object to the monster */
	obj->held_m_idx = mon->midx;

	/* Add the object to the monster's inventory */
	list_object(c, obj);
	if (obj->known) {
		obj->known->oidx = obj->oidx;
		player->cave->objects[obj->oidx] = obj->known;
	}

	mon->body.slots[slot].obj = obj;

	//pile_insert(&mon->equipped_obj, obj);

	mflag_on(mon->mflag, MFLAG_UPDATE_STATE);
	mflag_on(mon->mflag, MFLAG_UPDATE_ATTACKS);

	/* Result */
	return true;
}

bool monster_unequip(struct chunk *c, struct monster *mon, struct object *obj)
{
	uint16_t i;

	// L: flag the monster as wanting to recheck its equipment
	mflag_on(mon->mflag, MFLAG_CHECK_EQ);

	for (i = 0; i < mon->body.count; ++i) {
		if (mon->body.slots[i].obj == obj) {
			mon->body.slots[i].obj = NULL;
			monster_carry(c, mon, obj);
			return true;
		}
	}

	return false;
}

/**
 * Get a random object from a monster's inventory
 */
struct object *get_random_monster_object(struct monster *mon)
{
    struct object *obj, *pick = NULL;
    int i = 1;

    /* Pick a random object */
    for (obj = mon->held_obj; obj; obj = obj->next)
    {
        /* Check it isn't a quest artifact */
        if (obj->artifact && kf_has(obj->kind->kind_flags, KF_QUEST_ART))
            continue;

        if (one_in_(i)) pick = obj;
        i++;
    }

    return pick;
}

/**
 * Player or monster midx steals an item from a monster
 *
 * \param mon Monster stolen from
 * \param midx Index of the thief
 */
void steal_monster_item(struct monster *mon, int midx)
{
	struct object *obj = get_random_monster_object(mon);
	struct monster_lore *lore = get_lore(mon->race);
	struct monster *thief = NULL;
	char m_name[80];

	/* Get the target monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);

	if (midx < 0) {
		/* Base monster protection and player stealing skill */
		bool unique = monster_is_unique(mon);
		int guard = (mon->race->level * (unique ? 4 : 3)) / 4 +
			mon->state.speed - player->mon.state.speed;
		int steal_skill = player->mon.state.skills[SKILL_STEALTH] / 5 +
			adj_dex_th(player->mon.state.stat_ind[STAT_DEX]);
		int monster_reaction;

		/* No object */
		if (!obj) {
			msg("You can find nothing to steal from %s.", m_name);
			if (one_in_(3)) {
				/* Monster notices */
				monster_wake(mon, false, 100);
			}
			return;
		}

		/* Penalize some status conditions */
		if (player->mon.m_timed[TMD_BLIND] || player->mon.m_timed[TMD_CONFUSED] ||
			player->mon.m_timed[TMD_IMAGE]) {
			steal_skill /= 4;
		}
		if (mon->m_timed[TMD_ASLEEP]) {
			guard /= 2;
		}

		/* Monster base reaction, plus allowance for item weight */
		monster_reaction = guard / 2 + randint1(MAX(guard, 1));
		monster_reaction += (obj->number * object_weight_one(obj)) / 20;

		/* Try and steal */
		if (monster_reaction < steal_skill) {
			int wake = 35 - player->mon.state.skills[SKILL_STEALTH] / 5;

			/* Success! */
			obj->held_m_idx = 0;
			pile_excise(&mon->held_obj, obj);
			if (tval_is_money(obj)) {
				msg("You steal %d gold pieces worth of treasure.", obj->pval);
				player->au += obj->pval;
				player->upkeep->redraw |= (PR_GOLD);
				delist_object(cave, obj);
				object_delete(cave, player->cave, &obj);
			} else {
				object_grab(player, obj);
				delist_object(player->cave, obj->known);
				delist_object(cave, obj);
				/* Drop immediately if ignored,
				   or if inventory already full to prevent pack overflow */
				if (ignore_item_ok(player, obj) || !inven_carry_okay(obj)) {
					char o_name[80];
					object_desc(o_name, sizeof(o_name), obj,
						ODESC_PREFIX | ODESC_FULL,
						player);
					drop_near(cave, &obj, 0, player->mon.grid, true, true);
					msg("You drop %s.", o_name);
				} else {
					inven_carry(player, obj, true, true);
				}
			}

			/* Track thefts */
			lore->thefts++;

			/* Monster wakes a little */
			mon_dec_timed(mon, TMD_ASLEEP, wake, MON_TMD_FLG_NOTIFY);
		} else if (monster_reaction / 2 < steal_skill) {
			/* Decent attempt, at least */
			char o_name[80];

			object_see(player, obj);
			if (tval_is_money(obj)) {
				(void)strnfmt(o_name, sizeof(o_name), "treasure");
			} else {
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_PREFIX | ODESC_FULL, player);
			}
			msg("You fail to steal %s from %s.", o_name, m_name);
			/* Monster wakes, may notice */
			monster_wake(mon, true, 50);
		} else {
			/* Bungled it */
			monster_wake(mon, true, 100);
			monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
			msg("%s cries out in anger!", m_name);
			effect_simple(EF_WAKE, source_monster(mon->midx), "", 0, 0, 0, 0, 0,
						  NULL);
		}

		/* Player hit and run */
		if (player->mon.m_timed[TMD_ATT_RUN]) {
			const char *near = "20";
			msg("You vanish into the shadows!");
			effect_simple(EF_TELEPORT, source_player(), near, 0, 0, 0, 0, 0,
						  NULL);
			(void) player_clear_timed(player, TMD_ATT_RUN, false,
				false);
		}
	} else {
		/* Get the thief details */
		char t_name[80];
		thief = cave_monster(cave, midx);
		assert(thief);
		monster_desc(t_name, sizeof(t_name), thief, MDESC_STANDARD);

		/* Try to steal */
		if (!obj || react_to_slay(obj, thief)) {
			/* Fail to steal */
			msg("%s tries to steal something from %s, but fails.", t_name,
				m_name);
		} else {
			msg("%s steals something from %s!", t_name, m_name);

			/* Steal and carry */
			obj->held_m_idx = 0;
			pile_excise(&mon->held_obj, obj);
			(void)monster_carry(cave, thief, obj);
		}
	}
}


/**
 * ------------------------------------------------------------------------
 * Monster shapechange utilities
 * ------------------------------------------------------------------------ */
/**
 * The shape base for shapechanges
 */
struct monster_base *shape_base;

/**
 * Predicate function for get_mon_num_prep
 * Check to see if the monster race has the same base as the desired shape
 */
static bool monster_base_shape_okay(struct monster_race *race)
{
	assert(race);

	/* Check if it matches */
	if (race->base != shape_base) return false;

	return true;
}

/**
 * Monster shapechange
 */
bool monster_change_shape(struct monster *mon)
{
	struct monster_shape *shape = mon->race->shapes;
	struct monster_race *race = NULL;

	/* Use the monster's preferred shapes if any */
	if (shape) {
		/* Pick one */
		int choice = randint0(mon->race->num_shapes);
		while (choice--) {
			shape = shape->next;
		}

		/* Race or base? */
		if (shape->race) {
			/* Simple */
			race = shape->race;
		} else {
			/* Set the shape base */
			shape_base = shape->base;

			/* Choose a race of the given base */
			get_mon_num_prep(monster_base_shape_okay);

			/* Pick a random race */
			race = get_mon_num(player->depth + 5, player->depth);

			/* Reset allocation table */
			get_mon_num_prep(NULL);
		}
	} else {
		/* Choose something the monster can summon */
		bitflag summon_spells[RSF_SIZE];
		int i, poss = 0, which, index, summon_type;
		const struct monster_spell *spell;

		/* Extract the summon spells */
		create_mon_spell_mask(summon_spells, RST_SUMMON, RST_NONE);
		rsf_inter(summon_spells, mon->race->spell_flags);

		/* Count possibilities */
		for (i = rsf_next(summon_spells, FLAG_START); i != FLAG_END;
			 i = rsf_next(summon_spells, i + 1)) {
			poss++;
		}

		/* Pick one */
		which = randint0(poss);
		index = rsf_next(summon_spells, FLAG_START);
		for (i = 0; i < which; i++) {
			index = rsf_next(summon_spells, index);
		}
		spell = monster_spell_by_index(index);

		/* Set the summon type, and the kin_base if necessary */
		summon_type = spell->effect->subtype;
		if (summon_type == summon_name_to_idx("KIN")) {
			kin_base = mon->race->base;
		}

		/* Choose a race */
		race = select_shape(mon, summon_type);
	}

	/* Print a message immediately, update visuals */
	if (monster_is_obvious(mon)) {
		char m_name[80];
		monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
		msgt(MSG_GENERIC, "%s %s", m_name, "shimmers and changes!");
		if (player->upkeep->health_who == mon)
			player->upkeep->redraw |= (PR_HEALTH);

		player->upkeep->redraw |= (PR_MONLIST);
		square_light_spot(cave, mon->grid);
	}

	/* Set the race */
	if (race) {
		if (!mon->original_race) mon->original_race = mon->race;
		mon->race = race;
		mon->mspeed += mon->race->speed - mon->original_race->speed;
	}

	/* Emergency teleport if needed */
	if (!monster_passes_walls(mon) &&
		!square_is_monster_walkable(cave, mon->grid)) {
		effect_simple(EF_TELEPORT, source_monster(mon->midx), "1", 0, 0, 0,
					  mon->grid.y, mon->grid.x, NULL);
	}

	mflag_on(mon->mflag, MFLAG_UPDATE_STATE);
	mflag_on(mon->mflag, MFLAG_UPDATE_ATTACKS);

	return mon->original_race != NULL;
}

/**
 * Monster reverse shapechange
 */
bool monster_revert_shape(struct monster *mon)
{
	if (mon->original_race) {
		if (monster_is_obvious(mon)) {
			char m_name[80];
			monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
			msgt(MSG_GENERIC, "%s %s", m_name, "shimmers and changes!");
			if (player->upkeep->health_who == mon)
				player->upkeep->redraw |= (PR_HEALTH);

			player->upkeep->redraw |= (PR_MONLIST);
			square_light_spot(cave, mon->grid);
		}
		mon->mspeed += mon->original_race->speed - mon->race->speed;
		mon->race = mon->original_race;
		mon->original_race = NULL;

		/* Emergency teleport if needed */
		if (!monster_passes_walls(mon) &&
			!square_is_monster_walkable(cave, mon->grid)) {
			effect_simple(EF_TELEPORT, source_monster(mon->midx), "1", 0, 0, 0,
						  mon->grid.y, mon->grid.x, NULL);
		}

		return true;
	}

	return false;
}

void reaction_roll(struct monster *mon, struct player *p)
{
	int min = MON_REACT_HOSTILE, max = MON_REACT_FRIENDLY;
	struct monster_race *pmr = lookup_player_monster(p);
	int roll1, roll2;

	if (mon->reaction != MON_REACT_NONE) {
		return;
	}

	if (pmr && pmr->d_char == mon->race->d_char) {
		min = MON_REACT_NEUTRAL;
	}
	if (p->lev > mon->race->level) {
		min += (p->lev - mon->race->level) * 5;
	}
	if (cave->depth <= 0) {
		min = MAX(min, MON_REACT_NEUTRAL);
	}

	if (rf_has(mon->race->flags, RF_SAPIENT)) {
		max = MON_REACT_ALLY;
	}

	roll1 = randint0(MAX(0, max - min)) + min;
	roll2 = randint0(MAX(0, max - min)) + min;

	mon->reaction = MAX(0, MIN(roll1, roll2));

	mon_check_target(cave, mon);
}

void reaction_change(struct monster *mon, int amt)
{
	struct monster *leader = monster_group_leader(cave, mon);

	if (!monster_can_see_player(mon) && !monster_can_smell(mon)) {
		return;
	}
	if (mon->reaction == MON_REACT_NONE) {
		return;
	}

	mon->reaction += amt;
	mon->reaction = MAX(0, MIN(MON_REACT_MAX, mon->reaction));

	if (leader && leader != mon && leader->reaction != MON_REACT_NONE) {
		leader->reaction += amt / 2;
		leader->reaction = MAX(0, MIN(MON_REACT_MAX, leader->reaction));
	}

	mon_check_target(cave, mon);
}

void monster_attacked_get_angry(struct monster *mon, struct player *p, int dam)
{
	int pen = 250 + randint0(250);

	if (mon->reaction == MON_REACT_NONE) {
		return;
	}

	if (p->mon.m_timed[TMD_CONFUSED]) {
		pen = MIN(pen, MON_REACT_MAX - mon->reaction);
	}

	pen = pen * dam / MAX(mon->hp, 1) + 10;

	reaction_change(mon, -pen);
}

void monster_become_aware(struct monster *mon)
{
	mflag_on(mon->mflag, MFLAG_AWARE);
	if (mon->reaction == MON_REACT_NONE) {
		reaction_roll(mon, player);
	}
}

int mon_ac(struct monster *mon)
{
	return mon->state.ac + mon->state.to_a;
	/*int base = mon->race->ac, ac = 0, to_a = 0;
	struct object *obj;

	for (obj = mon->equipped_obj; obj; obj = obj->next) {
		ac += obj->ac;
		to_a = object_to_ac(obj);
	}

	return MAX(base, ac) + MIN(base, ac) / 2 + to_a;*/
}


static bool race_has_drops(struct monster_race *mr)
{
	assert(mr);

	if (rf_has(mr->flags, RF_DROP_20)) return true;
	if (rf_has(mr->flags, RF_DROP_40)) return true;
	if (rf_has(mr->flags, RF_DROP_60)) return true;
	if (rf_has(mr->flags, RF_DROP_1)) return true;
	if (rf_has(mr->flags, RF_DROP_2)) return true;
	if (rf_has(mr->flags, RF_DROP_3)) return true;
	if (rf_has(mr->flags, RF_DROP_4)) return true;
	if (mr->drops) return true;
	
	return false;
}

/**
 * L: decide which spells to give a monster
 * should be done after spellpower is calculated
 */
static void rearrange_monster_spells(struct monster_race *mr, bool is_player)
{
	int i, j;
	int level_mod = my_int_sqrt(mr->level);
	int magic, magic_mod;

	magic = mr->base->skills[SKILL_MAGIC] + mr->spell_power;

	magic_mod = my_int_sqrt(magic);

	for (i = RSF_NONE + 1; i < RSF_MAX; ++i) {
		bool on = false;
		int chance = level_mod;
		const struct monster_spell *ms = monster_spell_by_index(i);
		if (!mon_spell_is_innate(i)) {
			if (magic <= 0) continue;
			else chance = magic_mod;
		}
		if (!ms) continue;
		if (!ms->knowable) continue;

		for (j = 0; j < PP_MAX; ++j) {
			int min = ms->powers[j];
			int race_power = mon_power(mr, j);// mr->powers[j] * mr->level / 100;
			int mod;
			if (min <= 0) continue;

			mod = race_power - min;
			if (race_power <= 0) mod *= 2; // mages without any specialty at all in the subject are unlikely to know
			if (mod > 0) mod += magic_mod; // good mages in their specialty are likely to know a spell

			chance += mod;
		}

		if (is_player) {
			if (chance >= 50) {
				on = true;
			}
		}

		else if (randint0(100) < chance) {
			on = true;
		}

		if (on) {
			rsf_on(mr->spell_flags, i);
		}
		else {
			rsf_off(mr->spell_flags, i);
		}
	}
}

static int level_to_hp(int level)
{
	return (int)(MAX(level + 25.0, level * 2.5) * (my_sqrt(level) + 1.0) / 11.0);
}

static void normal_monster(struct monster_race *mr)
{
	mr->ac = 0;
	mr->speed = 110;
	mr->avg_hp = level_to_hp(mr->level);
}

void rearrange_monster(struct monster_race *mr, bool is_player)
{
	if (is_player && rf_has(mr->flags, RF_PLAYABLE)) {
		normal_monster(mr);
		return;
	}

	int power = mr->level;
	if (!is_player) power += randint0(mr->level / 5 + 1) - randint0(mr->level / 5 + 1);
	int blows = 0;
	bool mspells = false, breaths = false, gear = rf_has(mr->flags, RF_GEAR);
	int ttdam, tdice, quo; // twice total dam
	struct monster_blow *cblow;
	int mintotal, maxtotal;
	int i;
	int dam, hp, ac, spe, mag;
	int powermod = 0;
	struct monster_base *rb = mr->base;
	bool spellcaster = rf_has(mr->flags, RF_SPELLCASTER);

	// change monster's power based on its flags
	for (i = 0; i < RF_MAX; i++) {
		if (rf_has(mr->flags, i)) {
			const struct monster_flag *f = &monster_flag_table[i];
			powermod += f->power_mod;
		}
	}

	power += (power + 25) * powermod / 100;

	power = MAX(power, 0);
	mintotal = power * 5 - 2;
	maxtotal = power * 5 + 2;

	// randomize different abilities
	if (is_player) {
		dam = hp = ac = spe = mag = power;
	} else {
		dam = randint0(power * 3 / 2 + 2) + randint0(power / 2);
		hp  = randint0(power * 3 / 2 + 2) + randint0(power / 2);
		ac  = randint0(power * 3 / 2 + 2) + randint0(power / 2);
		spe = randint0(power * 3 / 2 + 2) + randint0(power / 2);
		mag = randint0(power * 3 / 2 + 2) + randint0(power / 2);
	}

	// check if it has no attacks or no spells
	i = 0;
	for (i = 0; i < z_info->mon_blows_max && mr->blow[i].method; i++) {
		if (mr->blow[i].dice.dice > 0) {
			blows++;
		}
	}
	if (!blows) dam /= 2;

	if (spellcaster) mspells = true;
	if (!mspells) mag /= 2;

	// if it breathes give it better hp at expense of magic
	if (test_spells(mr->spell_flags, RST_BREATH)) breaths = true;
	if (breaths && mag > hp) {
		int temp = hp;
		hp = mag;
		mag = temp;
	}

	// make sure the monster has averageish total power
	while (dam + hp + ac + spe + mag < mintotal) {
		int inc = (mintotal - dam - hp - ac - spe - mag + 4) / 5;
		if (blows) dam += inc;
		hp += inc;
		ac += inc;
		spe += inc;
		if (mspells) mag += inc;
	}
	while (dam + hp + ac + spe + mag > maxtotal) {
		int dec = (dam + hp + ac + spe + mag - maxtotal + 4) / 5;
		dam = MAX(dam - dec, 0);
		hp  = MAX(hp  - dec, 0);
		ac  = MAX(ac  - dec, 0);
		spe = MAX(spe - dec, 0);
		mag = MAX(mag - dec, 0);
	}

	// give it bonuses for its base's strengths and weaknesses
	// +10 doubles and -10 reduces to 0 (+ n * 10 %)
	dam += rb->attributes[MA_DAMAGE] * (dam + power) / 10 / 2;
	hp  += rb->attributes[MA_HP]     * (hp  + power) / 10 / 2;
	ac  += rb->attributes[MA_AC]     * (ac  + power) / 10 / 2;
	spe += rb->attributes[MA_SPEED]  * (spe + power) / 10 / 2;
	mag += rb->attributes[MA_MAGIC]  * (mag + power) / 10 / 2;

	dam = MAX(0, dam);
	hp  = MAX(0, hp );
	ac  = MAX(0, ac );
	spe = MAX(0, spe);
	mag = MAX(0, mag);

	// calculate its stats based on power
	//mr->avg_hp = MAX(hp / 2 + 5, hp) * MAX((hp + 1) / 2 + 10, hp) / 10; // 1000ish for level 100
	mr->avg_hp = level_to_hp(hp); // 250ish for level 100
	mr->ac = ac; // 100ish for level 100
	mr->speed = 105 + (spe * 30 + 49) / 100; // 135ish for level 100
	mr->spell_power = spellcaster ? mag : 0; // 100ish for level 100
	ttdam = MAX(dam / 2 + 4, dam); // 100ish for level 100
	if (dam > 0) mr->freq_spell = 40 * mag / dam;
	else mr->freq_spell = 100;
	mr->freq_spell = MIN(75, mr->freq_spell);
	mr->mexp = rf_has(mr->flags, RF_UNIQUE) ? power * power * 10 : 0;

	// spread damage over its damaging blows
	quo = 0;
	for (i = 0; i < z_info->mon_blows_max && mr->blow[i].method; i++) {
		quo += 5 + mr->blow[i].method->power;
	}
	// gets a total number of dice for its attacks based on its level and number of attacks
	// 5 + blows dice total at level 100
	tdice = dam / 20 + blows;

	for (i = 0; i < z_info->mon_blows_max && mr->blow[i].method; i++) {
		cblow = &mr->blow[i];
		int fact = 5 + cblow->method->power;

		if (cblow->dice.dice < 1) {
			continue;
		}

		int ddice = fact * tdice / quo;
		ddice = MAX(1, MIN(4, ddice));

		int dsides = (ttdam * fact + quo * ddice - 1) / quo / ddice - 1;
		dsides = MAX(1, dsides);

		if (gear) {
			cblow->dice.dice = 1;
			cblow->dice.sides = 1;
			ddice = 2;
			dsides = mr->level / 5 + 1;
		}
		else {
			cblow->dice.dice = ddice;
			cblow->dice.sides = dsides;
		}

		ttdam -= ddice * (dsides + 1);
		tdice = MAX(tdice - ddice, 1);

		quo = MAX(1, quo - fact);
	}

	rearrange_monster_spells(mr, is_player);
}

void rearrange_monsters(struct monster_race *mraces, uint32_t seed)
{
	Rand_quick = true;
	Rand_value = seed;
	
	struct monster_base *pbase = lookup_monster_base("player");
	struct monster_race *curr;
	for (curr = mraces; curr; curr = curr->next) {
		if (curr->base == pbase) continue;
		if (curr->level <= 0) continue;
		rearrange_monster(curr, false);

		// most monsters will pick up items
		if (race_has_drops(curr)) {
			rf_on(curr->flags, RF_TAKE_ITEM);
		}
	}
	Rand_quick = false;
}

