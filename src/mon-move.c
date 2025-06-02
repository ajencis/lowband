/**
 * \file mon-move.c
 * \brief Monster movement
 *
 * Monster AI affecting movement and spells, process a monster 
 * (with spells and actions of all kinds, reproduction, effects of any 
 * terrain on monster movement, picking up and destroying objects), 
 * process all monsters.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
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
#include "game-world.h"
#include "init.h"
#include "monster.h"
#include "mon-attack.h"
#include "mon-calcs.h"
#include "mon-desc.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "mon-timed.h"
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
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"


/**
 * L: Routines to handle monsters targeting each other
 */

struct opposed_monster_char opposed_chars[] =
{
	{'C', 'f'},
	{'s', 'A'},
	{'G', 'A'},
	{'L', 'A'},
	{'V', 'A'},
	{'W', 'A'},
	{'z', 'A'},
	{'U', 'A'},
	{'u', 'A'},
	{-1, -1}
};


/**
 * ------------------------------------------------------------------------
 * Routines to enable decisions on monster behaviour
 * ------------------------------------------------------------------------ */
/**
 * From Will Asher in DJA:
 * Find whether a monster is near a permanent wall
 *
 * this decides whether PASS_WALL & KILL_WALL monsters use the monster flow code
 */
static bool monster_near_permwall(const struct monster *mon)
{
	struct loc gp[512];
	int path_grids, j;

	/* If player is in LOS, there's no need to go around walls */
    if (projectable(cave, mon->grid, player->mon.grid, PROJECT_SHORT)) return false;

    /* PASS_WALL & KILL_WALL monsters occasionally flow for a turn anyway */
    if (randint0(99) < 5) return true;

	/* Find the shortest path */
	path_grids = project_path(cave, gp, z_info->max_sight, mon->grid,
		player->mon.grid, PROJECT_ROCK);

	/* See if we can "see" the player without hitting permanent wall */
	for (j = 0; j < path_grids; j++) {
		if (square_isperm(cave, gp[j])) return true;
		if (square_isplayer(cave, gp[j])) return false;
	}

	return false;
}

/**
 * Check if the monster can see the player
 */
bool monster_can_see_player(struct monster *mon)
{
	int p_sq_light = square_light(cave, player->mon.grid);

	if (mon->m_timed[TMD_ASLEEP]) {
		return false;
	}
	if (!los(cave, mon->grid, player->mon.grid)) {
		return false;
	}
	if (player->mon.m_timed[TMD_COVERTRACKS] && (mon->cdis > z_info->max_sight / 4)) {
		return false;
	}
	if (player_is_invisible(player) && 
			(!rf_has(mon->race->flags, RF_SMART) || !mflag_has(mon->mflag, MFLAG_AWARE))) {
		return false;
	}
	if (-p_sq_light > mon->race->level / 20 && !rf_has(mon->race->flags, RF_DARK_AURA)) {
		return false;
	}
	return true;
}

static bool monster_cannot_target_player(struct monster *mon)
{
	if (player->mon.m_timed[TMD_PHOENIX]) return true;

	return false;
}

/**
 * Check if the monster can hear anything
 */
bool monster_can_hear(struct monster *mon)
{
	int base_hearing = mon->race->hearing
		- player->mon.state.skills[SKILL_STEALTH] / 15;
	if (cave->noise.grids[mon->grid.y][mon->grid.x] == 0) {
		return false;
	}
	return base_hearing > cave->noise.grids[mon->grid.y][mon->grid.x];
}

/**
 * Check if the monster can smell anything
 */
bool monster_can_smell(struct monster *mon)
{
	if (cave->scent.grids[mon->grid.y][mon->grid.x] == 0) {
		return false;
	}
	return mon->race->smell > cave->scent.grids[mon->grid.y][mon->grid.x];
}

/**
 * Compare the "strength" of two monsters XXX XXX XXX
 */
static int compare_monsters(const struct monster *mon1,
							const struct monster *mon2)
{
	// Get levels
	uint32_t mlev1 = (mon1->original_race) ?
		mon1->original_race->level : mon1->race->level;
	uint32_t mlev2 = (mon2->original_race) ?
		mon2->original_race->level : mon2->race->level;

	// Compare
	return mlev1 - mlev2;
}

/**
 * Check if the monster can kill any monster on the relevant grid
 */
static bool monster_can_kill(struct monster *mon, struct loc grid)
{
	struct monster *mon1 = square_monster(cave, grid);

	/* No monster */
	if (!mon1) return true;

	/* No trampling uniques */
	if (monster_is_unique(mon1)) {
		return false;
	}

	if (rf_has(mon->race->flags, RF_KILL_BODY) &&
			compare_monsters(mon, mon1) > 10) {
		return true;
	}

	return false;
}

/**
 * Check if the monster can move any monster on the relevant grid
 */
static bool monster_can_move(struct monster *mon, struct loc grid)
{
	struct monster *mon1 = square_monster(cave, grid);

	/* No monster */
	if (!mon1) return true;

	if (rf_has(mon->race->flags, RF_MOVE_BODY) &&
			compare_monsters(mon, mon1) > 0) {
		return true;
	}

	return false;
}

/**
 * Check if the monster can occupy a grid safely
 */
static bool monster_hates_grid(struct monster *mon, struct loc grid)
{
	/* Only some creatures can handle damaging terrain */
	if (square_isdamaging(cave, grid) &&
			!rf_has(mon->race->flags, square_feat(cave, grid)->resist_flag)) {
		return true;
	}
	return false;
}


bool mon_will_follow_player(const struct monster *mon, const struct player *p)
{
	const struct monster *leader = monster_group_leader(cave, mon);
	leader = leader->reaction != MON_REACT_NONE ? leader : mon;

	if (leader->faction == '@') return true;
	if (leader->m_timed[TMD_CHARMED]) return true;

	return false;
}

bool mon_will_attack_player(const struct monster *mon, const struct player *p)
{
	const struct monster *leader = monster_group_leader(cave, mon);
	leader = leader->reaction != MON_REACT_NONE ? leader : mon;

	if (mon_will_follow_player(leader, p)) return false;
	if (leader->reaction >= MON_REACT_NEUTRAL) return false;
	if (p->mon.m_timed[TMD_PHOENIX]) return false;

	// assume enemy for now
	return true;
}

bool mon_will_attack_mon(const struct monster *mon, const struct monster *other)
{
	int i = 0;
	const struct monster *mlead = monster_group_leader(cave, mon);
	const struct monster *olead = monster_group_leader(cave, other);
	
	mlead = mlead->reaction != MON_REACT_NONE ? mlead : mon;
	olead = olead->reaction != MON_REACT_NONE ? olead : mon;

	bool mpally = mon_will_follow_player(mlead, player) || mlead->reaction >= MON_REACT_ALLY;
	bool opally = mon_will_follow_player(olead, player) || olead->reaction >= MON_REACT_ALLY;

	// if one is an enemy of the player and the other is a pet they'll fight
	if (mpally && mon_will_attack_player(olead, player)) return true;
	if (opally && mon_will_attack_player(mlead, player)) return true;

	while (opposed_chars[i].char1 >= 0) {

		if (mlead->faction == opposed_chars[i].char1 &&
		    	olead->faction == opposed_chars[i].char2) {
			return true;
		}

		if (olead->faction == opposed_chars[i].char1 &&
		    	mlead->faction == opposed_chars[i].char2) {
			return true;
		}

		i++;
	}

    return false;
}


static bool mon_can_pickup_objects(struct monster *mon)
{
	if (!mon || !mon->race) return false;

	if (!rf_has(mon->race->flags, RF_TAKE_ITEM)) return false;
	if (mon->faction == '@') return false;

	return true;
}

static bool object_can_be_targeted_by_mon(struct chunk *c, struct monster *mon, struct object *obj)
{
	if (!mon || !mon->race) return false;
	if (!obj) return false;
	if (!c) return false;

	if (!mon_can_pickup_objects(mon)) return false;
	if (loc_is_zero(obj->grid)) return false;
	if (obj->held_m_idx) return false;
	if (obj->mimicking_m_idx) return false;
	if (react_to_slay(obj, mon)) return false;
	if (!los(c, mon->grid, obj->grid)) return false;
	if (square(c, obj->grid)->mon > 0) return false;

	return true;
}

static void mon_find_target(struct chunk *c, struct monster *mon)
{
    int i;
	bool found = false;
	int score = 0;

	for (i = 1; i < c->mon_max; i++) {
		struct monster *other = cave_monster(c, i);
		// check if the monster exists and is an enemy and is visible
		if (!other || !other->race) continue;
		if (!mon_will_attack_mon(mon, other)) continue;
		if (!los(c, mon->grid, other->grid)) continue;

		// better target if it's closer or more threatening
		int dist = distance(mon->grid, other->grid) + 1;
		assert(dist > 0);
		int currscore = 100 / dist + other->race->level / 5;
		if (found && currscore <= score) continue;

		// best so far, target
		mon->target.midx = i;
		mon->target.who = TARGET_WHO_MONSTER;
		score = currscore;
		found = true;
	}
	for (i = 0; i < c->obj_max && mon_can_pickup_objects(mon); i++) {
		// check if the object exists and is a legitimate target
		struct object *obj = c->objects[i];
		if (!obj) continue;
		if (!object_can_be_targeted_by_mon(c, mon, obj)) continue;

		// closer and more valuable objects are better
		int dist = distance(mon->grid, obj->grid) + 1;
		assert(dist > 0);
		assert(mon->maxhp > 0);
		int ignore = mon->race->level * 10 * (mon->maxhp - mon->hp) / mon->maxhp;
		int currscore = object_value_real(obj, obj->number) / (dist * dist) - ignore;
		if (found && currscore <= score) continue;

		// best so far, target
		mon->target.oidx = i;
		mon->target.who = TARGET_WHO_OBJECT;
		score = currscore;
		found = true;
	}
	// player has to be an enemy and either sensable or known
	if (mon_will_attack_player(mon, player) &&
			(monster_can_see_player(mon) || monster_can_hear(mon) || monster_can_smell(mon) ||
				mflag_has(mon->mflag, MFLAG_AWARE))) {
		int dist = distance(mon->grid, player->mon.grid) + 1;
		assert(dist > 0);
		int currscore = 100 / dist + player->lev / 4;
		if (!found || currscore >= score) {
			mon->target.who = TARGET_WHO_PLAYER;
			score = currscore;
			found = true;
		}
	}
	if (!found)	{
		// if we have no foes but we're a player ally cluster around the player
		if (mon_will_follow_player(mon, player)) {
			mon->target.who = TARGET_WHO_PLAYER;
		} else {
			mon->target.who = TARGET_WHO_NONE;
		}
	}
}

bool mon_check_target(struct chunk *c, struct monster *mon)
{
	bool recheck = false;
	if (mon->target.who == TARGET_WHO_MONSTER) {
		struct monster *other = cave_monster(c, mon->target.midx);
		if (!other || !other->race) {
			// target doesn't exist anymore
			mon->target.who = TARGET_WHO_NONE;
			recheck = true;
		}
		if (!los(c, mon->grid, other->grid)) {
			// can't see their target, go to where we last saw them
			mon->target.who = TARGET_WHO_GRID;
			recheck = true;
		}
	}
	else if (mon->target.who == TARGET_WHO_PLAYER) {
		if (monster_cannot_target_player(mon)) {
			recheck = true;
			mon->target.who = TARGET_WHO_NONE;
		}

		if (!monster_can_see_player(mon)) {
			// if we can't see the player we should see if we have better targets
			recheck = true;
			if (!monster_can_hear(mon) && !monster_can_smell(mon)) {
				// if we can't sense the player at all go to last known location
				mon->target.who = TARGET_WHO_GRID;
			}
		}

		if (!mon_will_attack_player(mon, player)) {
			// if we're on the player's side we should see if there's someone we want to attack
			recheck = true;
			if (!mon_will_follow_player(mon, player)) {
				mon->target.who = TARGET_WHO_NONE;
			}
		}
	}
	else if (mon->target.who == TARGET_WHO_OBJECT) {
		struct object *obj = c->objects[mon->target.oidx];
		if (!obj ||	!object_can_be_targeted_by_mon(c, mon, obj)) {
			// object is gone or no longer legitimate
			mon->target.who = TARGET_WHO_NONE;
			recheck = true;
		}
	}
	else if (mon->target.who == TARGET_WHO_GRID) {
		if (loc_eq(mon->grid, mon->target.grid)) {
			mon->target.who = TARGET_WHO_NONE;
		}
		recheck = true;
	}
	else {
		// don't have a target, should see if one has appeared
		mon->target.who = TARGET_WHO_NONE;
		recheck = true;
	}

	if (recheck) {
		mon_find_target(c, mon);
	}

	if (mon->target.who != TARGET_WHO_PLAYER && mon->target.who != TARGET_WHO_GRID &&
			!monster_can_see_player(mon) && !monster_can_smell(mon) && !monster_can_hear(mon)) {
		// can't sense the player at all, maybe they've vanished entirely
		mflag_off(mon->mflag, MFLAG_AWARE);
	}

	return mon->target.who != TARGET_WHO_NONE;
}

/**
 * ------------------------------------------------------------------------
 * Monster movement routines
 * These routines, culminating in get_move(), choose if and where a monster
 * will move on its turn
 * ------------------------------------------------------------------------ */

static int item_score(struct object *obj)
{
	int score = 0;
	if (!obj) return score;

	score += obj->ac;
	score += object_to_ac(obj);
	score += obj->to_h;

	if (wield_slot_type(obj) == EQUIP_WEAPON) {
		score += obj->dd * (obj->ds + 1);
		score += obj->to_d * 2;
	}

	return score;
}

static bool monster_turn_equip_item(struct monster *mon)
{
	if (!mflag_has(mon->mflag, MFLAG_CHECK_EQ)) {
		return false;
	}
	const struct player_body *body = mon->race->body;
	struct object *equipped, *to_equip = NULL, *to_unequip = NULL;
	int i, best_best_benefit = 0;
	struct equip_slot *slot;
	char mdesc[80], odesc[80];
	bool did_something = false;

	if (!body || body->count == 0) {
		return false;
	}

	// list of all equipped items in their slots
	struct object **obj_slot = mem_zalloc(sizeof(struct object *) * body->count);
	for (equipped = mon->equipped_obj; equipped; equipped = equipped->next) {
		int type = wield_slot_type(equipped);
		if (type == -1) {
			continue;
		}
		for (slot = body->slots, i = 0; slot && i < body->count; slot = slot->next, ++i) {
			if (slot->type == type) {
				obj_slot[i] = equipped;
				break;
			}
		}
	}

	// check every slot
	for (slot = body->slots, i = 0; slot && i < body->count; slot = slot->next, ++i) {
		struct object *curr, *best = obj_slot[i];
		int curr_score, best_score = best ? item_score(best) : 0;
		// check every item that could be in that slot
		for (curr = mon->held_obj; curr; curr = curr->next) {
			if (wield_slot_type(curr) != slot->type) {
				continue;
			}
			curr_score = item_score(curr);
			if (curr_score > best_score) {
				best = curr;
				best_score = curr_score;
			}
		}

		// save the object to equip / unequip that has the best difference in score
		if (best && best != obj_slot[i]) {
			int best_benefit = best_score - (obj_slot[i] ? item_score(obj_slot[i]) : 0);
			if (best_benefit > best_best_benefit) {
				best_best_benefit = best_benefit;
				if (obj_slot[i]) {
					assert(obj_slot[i]);
					to_unequip = obj_slot[i];
					to_equip = NULL;
				}
				else {
					assert(best);
					to_equip = best;
					to_unequip = NULL;
				}
			}
		}
	}

	mem_free(obj_slot);

	monster_desc(mdesc, sizeof(mdesc), mon, MDESC_TARG | MDESC_CAPITAL);

	if (to_unequip) {
		pile_excise(&mon->equipped_obj, to_unequip);
		monster_carry(cave, mon, to_unequip);
		if (monster_is_visible(mon)) {
			object_desc(odesc, sizeof(odesc), to_unequip, ODESC_TERSE | ODESC_PREFIX, player);
			msg("%s unequips %s.", mdesc, odesc);
		}
		assert(player->cave->objects[to_unequip->oidx] == to_unequip->known);
		assert(cave->objects[to_unequip->oidx] == to_unequip);
		did_something = true;

		mflag_on(mon->mflag, MFLAG_UPDATE);
	}
	else if (to_equip) {
		if (to_equip->number > 1) {
			to_equip = object_split(to_equip, 1);
		} else {
			pile_excise(&mon->held_obj, to_equip);
		}
		monster_equip(cave, mon, to_equip);
		if (monster_is_visible(mon)) {
			object_see(player, to_equip);
			object_desc(odesc, sizeof(odesc), to_equip, ODESC_PREFIX, player);
			msg("%s equips %s.", mdesc, odesc);
		}
		assert(cave->objects[to_equip->oidx] == to_equip);
		assert(player->cave->objects[to_equip->oidx] == to_equip->known);
		did_something = true;
	}

	// looked through everything and no changes to make, so we can stop rechecking
	if (!did_something) {
		mflag_off(mon->mflag, MFLAG_CHECK_EQ);
	}

	return did_something;
}

/**
 * Calculate minimum and desired combat ranges.  -BR-
 *
 * Afraid monsters will set this to their maximum flight distance.
 * Currently this is recalculated every turn - if it becomes a significant
 * overhead it could be calculated only when something has changed (monster HP,
 * chance of escaping, etc)
 */
static void get_move_find_range(struct monster *mon)
{
	uint16_t p_lev, m_lev;
	uint16_t p_chp, p_mhp;
	uint16_t m_chp, m_mhp;
	uint32_t p_val, m_val;

	/* Monsters will run up to z_info->flee_range grids out of sight */
	int flee_range = z_info->max_sight + z_info->flee_range;

	/* All "afraid" monsters will run away */
	if (mon->m_timed[TMD_AFRAID] || rf_has(mon->race->flags, RF_FRIGHTENED)) {
		mon->min_range = flee_range;
	} else if (mon->group_info[PRIMARY_GROUP].role == MON_GROUP_BODYGUARD) {
		/* Bodyguards don't flee */
		mon->min_range = 1;
	} else {
		/* Minimum distance - stay at least this far if possible */
		mon->min_range = 1;

		/* Taunted monsters just want to get in your face */
		if (player->mon.m_timed[TMD_TAUNT]) return;

		/* Examine player power (level) */
		p_lev = player->lev;

		/* Hack - increase p_lev based on specialty abilities */

		/* Examine monster power (level plus morale) */
		m_lev = mon->race->level + (mon->midx & 0x08) + 25;

		/* Simple cases first */
		if (!mon_will_attack_player(mon, player)) {
			mon->min_range = 0;
		} else if (m_lev + 3 < p_lev) {
			mon->min_range = flee_range;
		} else if (m_lev - 5 < p_lev) {
			/* Examine player health */
			p_chp = player->mon.hp;
			p_mhp = player->mon.maxhp;

			/* Examine monster health */
			m_chp = mon->hp;
			m_mhp = mon->maxhp;

			/* Prepare to optimize the calculation */
			p_val = (p_lev * p_mhp) + (p_chp << 2);	/* div p_mhp */
			m_val = (m_lev * m_mhp) + (m_chp << 2);	/* div m_mhp */

			/* Strong players scare strong monsters */
			if (p_val * m_mhp > m_val * p_mhp)
				mon->min_range = flee_range;
		}
	}

	if (mon->min_range < flee_range) {
		/* Creatures that don't move never like to get too close */
		if (rf_has(mon->race->flags, RF_NEVER_MOVE))
			mon->min_range += 3;

		/* Spellcasters that don't strike never like to get too close */
		if (rf_has(mon->race->flags, RF_NEVER_BLOW))
			mon->min_range += 3;
	}

	/* Maximum range to flee to */
	if (!(mon->min_range < flee_range)) {
		mon->min_range = flee_range;
	} else if (mon->cdis < z_info->turn_range) {
		/* Nearby monsters won't run away */
		mon->min_range = 1;
	}

	/* Now find preferred range */
	mon->best_range = mon->min_range;

	/* Archers are quite happy at a good distance */
	if (monster_loves_archery(mon)) {
		mon->best_range += 3;
	}

	/* Breathers like point blank range */
	if (mon->race->freq_innate > 24) {
		if (monster_breathes(mon) && (mon->hp > mon->maxhp / 2)) {
			mon->best_range = MAX(1, mon->best_range);
		}
	} else if (mon->race->freq_spell > 24) {
		/* Other spell casters will sit back and cast */
		mon->best_range += 3;
	}
}

/**
 * Choose the best direction for a bodyguard.
 *
 * The idea is to stay close to the group leader, but attack the player if the
 * chance arises
 */
static bool get_move_bodyguard(struct monster *mon)
{
	int i;
	struct monster *leader = monster_group_leader(cave, mon);
	int dist;
	struct loc best;
	bool found = false;

	if (!leader) return false;

	/* Get distance */
	dist = distance(mon->grid, leader->grid);

	/* If currently adjacent to the leader, we can afford a move */
	if (dist <= 1) return false;

	/* If the leader's too out of sight and far away, save yourself */
	if (!los(cave, mon->grid, leader->grid) && (dist > 10)) return false;

	/* Check nearby adjacent grids and assess */
	for (i = 0; i < 8; i++) {
		/* Get the location */
		struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);
		int new_dist = distance(grid, leader->grid);
		int char_dist = distance(grid, player->mon.grid);

		/* Bounds check */
		if (!square_in_bounds(cave, grid)) {
			continue;
		}

		/* There's a monster blocking that we can't deal with */
		if (!monster_can_kill(mon, grid) && !monster_can_move(mon, grid)){
			continue;
		}

		/* There's damaging terrain */
		if (monster_hates_grid(mon, grid)) {
			continue;
		}

		/* Closer to the leader is always better */
		if (new_dist < dist) {
			best = grid;
			found = true;
			/* If there's a grid that's also closer to the player, that wins */
			if (char_dist < mon->cdis) {
				break;
			}
		}
	}

	/* If we found one, set the target */
	if (found) {
		mon->target.grid = best;
		return true;
	}

	return false;
}


/**
 * Choose the best direction to advance toward the player, using sound or scent.
 *
 * Ghosts and rock-eaters generally just head straight for the player. Other
 * monsters try sight, then current sound as saved in cave->noise.grids[y][x],
 * then current scent as saved in cave->scent.grids[y][x].
 *
 * This function assumes the monster is moving to an adjacent grid, and so the
 * noise can be louder by at most 1.  The monster target grid set by sound or
 * scent tracking in this function will be a grid they can step to in one turn,
 * so is the preferred option for get_move() unless there's some reason
 * not to use it.
 *
 * Tracking by 'scent' means that monsters end up near enough the player to
 * switch to 'sound' (noise), or they end up somewhere the player left via 
 * teleport.  Teleporting away from a location will cause the monsters who
 * were chasing the player to converge on that location as long as the player
 * is still near enough to "annoy" them without being close enough to chase
 * directly.
 */
static bool get_move_advance(struct monster *mon, bool *track)
{
	int i;
	struct loc target = monster_is_decoyed(mon) ? cave_find_decoy(cave) :
		player->mon.grid;

	int base_hearing = mon->race->hearing
		- player->mon.state.skills[SKILL_STEALTH] / 15;
	int current_noise = base_hearing
		- cave->noise.grids[mon->grid.y][mon->grid.x];
	int best_scent = 0;

	struct loc best_grid;
	struct loc backup_grid;
	bool found = false;
	bool found_backup = false;

	/* Bodyguards are special */
	if (mon->group_info[PRIMARY_GROUP].role == MON_GROUP_BODYGUARD) {
		if (get_move_bodyguard(mon)) {
			return true;
		}
	}

	/* If the monster can pass through nearby walls, do that */
	if (monster_passes_walls(mon) && !monster_near_permwall(mon)) {
		mon->target.grid = target;
		return true;
	}

	/* If the player can see monster, set target and run towards them */
	if (monster_can_see_player(mon)) {
		mon->target.grid = target;
		return true;
	}

	/* Try to use sound */
	if (monster_can_hear(mon)) {
		/* Check nearby sound, giving preference to the cardinal directions */
		for (i = 0; i < 8; i++) {
			/* Get the location */
			struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);
			int heard_noise = base_hearing - cave->noise.grids[grid.y][grid.x];

			/* Bounds check */
			if (!square_in_bounds(cave, grid)) {
				continue;
			}

			/* Must be some noise */
			if (cave->noise.grids[grid.y][grid.x] == 0) {
				continue;
			}

			/* There's a monster blocking that we can't deal with */
			if (!monster_can_kill(mon, grid) && !monster_can_move(mon, grid)) {
				continue;
			}

			/* There's damaging terrain */
			if (monster_hates_grid(mon, grid)) {
				continue;
			}

			/* If it's better than the current noise, choose this direction */
			if (heard_noise > current_noise) {
				best_grid = grid;
				found = true;
				break;
			} else if (heard_noise == current_noise) {
				/* Possible move if we can't actually get closer */
				backup_grid = grid;
				found_backup = true;
				continue;
			}
		}
	}

	/* If both vision and sound are no good, use scent */
	if (monster_can_smell(mon) && !found) {
		for (i = 0; i < 8; i++) {
			/* Get the location */
			struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);
			int smelled_scent;

			/* If no good sound yet, use scent */
			smelled_scent = mon->race->smell
				- cave->scent.grids[grid.y][grid.x];
			if ((smelled_scent > best_scent) &&
				(cave->scent.grids[grid.y][grid.x] != 0)) {
				best_scent = smelled_scent;
				best_grid = grid;
				found = true;
			}
		}
	}

	/* Set the target */
	if (found) {
		mon->target.grid = best_grid;
		*track = true;
		return true;
	} else if (found_backup) {
		/* Move around to try and improve position */
		mon->target.grid = backup_grid;
		*track = true;
		return true;
	}

	/* No reason to advance */
	return false;
}

/**
 * Choose a random passable grid adjacent to the monster since is has no better
 * strategy.
 */
static struct loc get_move_random(struct monster *mon)
{
	int attempts[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	int nleft = 8;

	while (nleft > 0) {
		int itry = randint0(nleft);
		struct loc trygrid;

		trygrid = loc_sum(mon->grid, ddgrid_ddd[attempts[itry]]);
		if (square_is_monster_walkable(cave, trygrid) &&
				!monster_hates_grid(mon, trygrid)) {
			return ddgrid_ddd[attempts[itry]];
		} else {
			int tmp = attempts[itry];

			--nleft;
			attempts[itry] = attempts[nleft];
			attempts[nleft] = tmp;
		}
	}

	return loc(0, 0);
}

/**
 * Choose a "safe" location near a monster for it to run toward.
 *
 * A location is "safe" if it can be reached quickly and the player
 * is not able to fire into it (it isn't a "clean shot").  So, this will
 * cause monsters to "duck" behind walls.  Hopefully, monsters will also
 * try to run towards corridor openings if they are in a room.
 *
 * This function may take lots of CPU time if lots of monsters are fleeing.
 *
 * Return true if a safe location is available.
 */
static bool get_move_find_safety(struct monster *mon)
{
	int i, dy, dx, d, dis, gdis = 0;

	const int *y_offsets;
	const int *x_offsets;

	/* Start with adjacent locations, spread further */
	for (d = 1; d < 10; d++) {
		struct loc best = loc(0, 0);

		/* Get the lists of points with a distance d from (fx, fy) */
		y_offsets = dist_offsets_y[d];
		x_offsets = dist_offsets_x[d];

		/* Check the locations */
		for (i = 0, dx = x_offsets[0], dy = y_offsets[0];
		     dx != 0 || dy != 0;
		     i++, dx = x_offsets[i], dy = y_offsets[i]) {
			struct loc grid = loc_sum(mon->grid, loc(dx, dy));

			/* Skip illegal locations */
			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Skip locations in a wall */
			if (!square_ispassable(cave, grid)) continue;

			/* Ignore too-distant grids */
			if (cave->noise.grids[grid.y][grid.x] >
				cave->noise.grids[mon->grid.y][mon->grid.x] + 2 * d)
				continue;

			/* Ignore damaging terrain if they can't handle it */
			if (monster_hates_grid(mon, grid)) continue;

			/* Check for absence of shot (more or less) */
			if (!square_isview(cave, grid)) {
				/* Calculate distance from player */
				dis = distance(grid, player->mon.grid);

				/* Remember if further than previous */
				if (dis > gdis) {
					best = grid;
					gdis = dis;
				}
			}
		}

		/* Check for success */
		if (gdis > 0) {
			/* Good location */
			mon->target.grid = best;
			return (true);
		}
	}

	/* No safe place */
	return (false);
}

/**
 * Choose a good hiding place near a monster for it to run toward.
 *
 * Pack monsters will use this to "ambush" the player and lure him out
 * of corridors into open space so they can swarm him.
 *
 * Return true if a good location is available.
 */
static bool get_move_find_hiding(struct monster *mon)
{
	int i, dy, dx, d, dis, gdis = 999, min;

	const int *y_offsets, *x_offsets;

	/* Closest distance to get */
	min = distance(player->mon.grid, mon->grid) * 3 / 4 + 2;

	/* Start with adjacent locations, spread further */
	for (d = 1; d < 10; d++) {
		struct loc best = loc(0, 0);

		/* Get the lists of points with a distance d from monster */
		y_offsets = dist_offsets_y[d];
		x_offsets = dist_offsets_x[d];

		/* Check the locations */
		for (i = 0, dx = x_offsets[0], dy = y_offsets[0];
		     dx != 0 || dy != 0;
		     i++, dx = x_offsets[i], dy = y_offsets[i]) {
			struct loc grid = loc_sum(mon->grid, loc(dx, dy));

			/* Skip illegal locations */
			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Skip occupied locations */
			if (!square_isempty(cave, grid)) continue;

			/* Check for hidden, available grid */
			if (!square_isview(cave, grid) &&
				projectable(cave, mon->grid, grid, PROJECT_STOP)) {
				/* Calculate distance from player */
				dis = distance(grid, player->mon.grid);

				/* Remember if closer than previous */
				if (dis < gdis && dis >= min) {
					best = grid;
					gdis = dis;
				}
			}
		}

		/* Check for success */
		if (gdis < 999) {
			/* Good location */
			mon->target.grid = best;
			return (true);
		}
	}

	/* No good place */
	return (false);
}

/**
 * Provide a location to flee to, but give the player a wide berth.
 *
 * A monster may wish to flee to a location that is behind the player,
 * but instead of heading directly for it, the monster should "swerve"
 * around the player so that it has a smaller chance of getting hit.
 */
static bool get_move_flee(struct monster *mon)
{
	int i;
	struct loc best = loc(0, 0);
	int best_score = -1;

	/* Taking damage from terrain makes moving vital */
	if (!monster_taking_terrain_damage(cave, mon)) {
		/* If the player is not currently near the monster, no reason to flow */
		if (mon->cdis >= mon->best_range) {
			return false;
		}

		/* Monster is too far away to use sound or scent */
		if (!monster_can_hear(mon) && !monster_can_smell(mon)) {
			return false;
		}
	}

	/* Check nearby grids, diagonals first */
	for (i = 7; i >= 0; i--) {
		int dis, score;

		/* Get the location */
		struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);

		/* Bounds check */
		if (!square_in_bounds(cave, grid)) continue;

		/* Calculate distance of this grid from our target */
		dis = distance(grid, mon->target.grid);

		/* Score this grid
		 * First half of calculation is inversely proportional to distance
		 * Second half is inversely proportional to grid's distance from player
		 */
		score = 5000 / (dis + 3) - 500 /(cave->noise.grids[grid.y][grid.x] + 1);

		/* No negative scores */
		if (score < 0) score = 0;

		/* Ignore lower scores */
		if (score < best_score) continue;

		/* Save the score */
		best_score = score;

		/* Save the location */
		best = grid;
	}

	/* Set the immediate target */
	mon->target.grid = best;

	/* Success */
	return true;
}

/**
 * Choose the basic direction of movement, and whether to bias left or right
 * if the main direction is blocked.
 *
 * Note that the input is an offset to the monster's current position, and
 * the output direction is intended as an index into the side_dirs array.
 */
static int get_move_choose_direction(struct loc offset)
{
	int dir = 0;
	int dx = offset.x, dy = offset.y;

	/* Extract the "absolute distances" */
	int ay = ABS(dy);
	int ax = ABS(dx);

	/* We mostly want to move vertically */
	if (ay > (ax * 2)) {
		/* Choose between directions '8' and '2' */
		if (dy > 0) {
			/* We're heading down */
			dir = 2;
			if ((dx > 0) || (dx == 0 && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading up */
			dir = 8;
			if ((dx < 0) || (dx == 0 && turn % 2 == 0))
				dir += 10;
		}
	}

	/* We mostly want to move horizontally */
	else if (ax > (ay * 2)) {
		/* Choose between directions '4' and '6' */
		if (dx > 0) {
			/* We're heading right */
			dir = 6;
			if ((dy < 0) || (dy == 0 && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading left */
			dir = 4;
			if ((dy > 0) || (dy == 0 && turn % 2 == 0))
				dir += 10;
		}
	}

	/* We want to move down and sideways */
	else if (dy > 0) {
		/* Choose between directions '1' and '3' */
		if (dx > 0) {
			/* We're heading down and right */
			dir = 3;
			if ((ay < ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading down and left */
			dir = 1;
			if ((ay > ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		}
	}

	/* We want to move up and sideways */
	else {
		/* Choose between directions '7' and '9' */
		if (dx > 0) {
			/* We're heading up and right */
			dir = 9;
			if ((ay > ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		} else {
			/* We're heading up and left */
			dir = 7;
			if ((ay < ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		}
	}

	return dir;
}

/**
 * Choose "logical" directions for monster movement
 *
 * This function is responsible for deciding where the monster wants to move,
 * and so is the core of monster "AI".
 *
 * First, we work out how best to advance toward the player:
 * - Try to head toward the player directly if we can pass through walls or
 *   if we can see them
 * - Failing that follow the player by sound, or failing that by scent
 * - If none of that works, just head in the general direction
 * Then we look at possible reasons not to just advance:
 * - If we're part of a pack, try to lure the player into the open
 * - If we're afraid, try to find a safe place to run to, and if no safe place
 *   just run in the opposite direction to the advance move
 * - If we can see the player and we're part of a group, try and surround them
 *
 * The function then returns false if we're already where we want to be, and
 * otherwise sets the chosen direction to step and returns true.
 */
static bool get_move(struct monster *mon, int *dir, bool *good)
{
	//struct loc target;
	bool group_ai = rf_has(mon->race->flags, RF_GROUP_AI);

	/* Offset to current position to move toward */
	struct loc grid = loc(0, 0);

	/* Monsters will run up to z_info->flee_range grids out of sight */
	int flee_range = z_info->max_sight + z_info->flee_range;

	bool done = false;
	bool attacking = false;

	/* Calculate range */
	get_move_find_range(mon);

	/* L: Don't assume we're heading towards the player */
	if (mon->target.who == TARGET_WHO_NONE) {
		if (one_in_(5)) {
			mon->target.grid = get_move_random(mon);
		} else {
			mon->target.grid = mon->grid;
		}
		grid = loc_diff(mon->target.grid, mon->grid);
	} else if (mon->target.who == TARGET_WHO_GRID) {
		// we're heading towards a grid so keep the grid intact
		grid = loc_diff(mon->target.grid, mon->grid);
	} else if (mon->target.who == TARGET_WHO_MONSTER) {
		mon->target.grid = cave->monsters[mon->target.midx].grid;
		grid = loc_diff(mon->target.grid, mon->grid);
		attacking = true;
	} else if (mon->target.who == TARGET_WHO_OBJECT) {
		mon->target.grid = cave->objects[mon->target.oidx]->grid;
		grid = loc_diff(mon->target.grid, mon->grid);
	} else if (get_move_advance(mon, good)) {
		/* We have a good move, use it */
		grid = loc_diff(mon->target.grid, mon->grid);
		mflag_on(mon->mflag, MFLAG_TRACKING);
		attacking = true;
	} else {
		/* Try to follow someone who knows where they're going */
		struct monster *tracker = group_monster_tracking(cave, mon);
		if (tracker && los(cave, mon->grid, tracker->grid)) { /* Need los? */
			grid = loc_diff(tracker->grid, mon->grid);
			/* No longer tracking */
			mflag_off(mon->mflag, MFLAG_TRACKING);
		} else {
			if (mflag_has(mon->mflag, MFLAG_TRACKING)) {
				/* Keep heading to the most recent goal. */
				grid = loc_diff(mon->target.grid, mon->grid);
			}
			if (loc_is_zero(grid)) {
				/* Try a random move and no longer track. */
				grid = get_move_random(mon);
				mflag_off(mon->mflag, MFLAG_TRACKING);
			}
		}
	}

	/* Monster is taking damage from terrain */
	if (monster_taking_terrain_damage(cave, mon)) {
		/* Try to find safe place */
		if (get_move_find_safety(mon)) {
			/* Set a course for the safe place */
			get_move_flee(mon);
			grid = loc_diff(mon->target.grid, mon->grid);
			done = true;
		}
	}

	/* Normal animal packs try to get the player out of corridors. */
	if (!done && group_ai && !monster_passes_walls(mon) && attacking) {
		int i, open = 0;

		/* Count empty grids next to player */
		for (i = 0; i < 8; i++) {
			/* Check grid around the player for room interior (room walls count)
			 * or other empty space */
			struct loc test = loc_sum(mon->target.grid, ddgrid_ddd[i]);
			if (square_ispassable(cave, test) || square_isroom(cave, test)) {
				/* One more open grid */
				open++;
			}
		}

		/* Not in an empty space and strong player */
		if ((open < 5) && (player->mon.hp > player->mon.maxhp / 2)) {
			/* Find hiding place for an ambush */
			if (get_move_find_hiding(mon)) {
				done = true;
				grid = loc_diff(mon->target.grid, mon->grid);

				/* No longer tracking */
				mflag_off(mon->mflag, MFLAG_TRACKING);
			}
		}
	}

	/* Not hiding and monster is afraid */
	if (!done && (mon->min_range == flee_range)) {
		/* Try to find safe place */
		if (get_move_find_safety(mon)) {
			/* Set a course for the safe place */
			get_move_flee(mon);
			grid = loc_diff(mon->target.grid, mon->grid);
		} else {
			/* Just leg it away from the player */
			grid = loc_diff(loc(0, 0), grid);
		}

		/* No longer tracking */
		mflag_off(mon->mflag, MFLAG_TRACKING);
		done = true;
	}

	/* Monster groups try to surround the player if they're in sight */
	if (!done && group_ai && square_isview(cave, mon->grid) && attacking) {
		int i;
		struct loc grid1 = mon->target.grid;

		/* If we are not already adjacent */
		if (mon->cdis > 1) {
			/* Find an empty square near the player to fill */
			int tmp = randint0(8);
			for (i = 0; i < 8; i++) {
				/* Pick squares near player (pseudo-randomly) */
				grid1 = loc_sum(mon->target.grid, ddgrid_ddd[(tmp + i) % 8]);

				/* Ignore filled grids */
				if (!square_isempty(cave, grid1)) continue;

				/* Try to fill this hole */
				break;
			}
		}

		/* Head in the direction of the chosen grid */
		grid = loc_diff(grid1, mon->grid);
	}

	/* Check if the monster has already reached its target */
	if (loc_is_zero(grid)) return (false);

	/* Pick the correct direction */
	*dir = get_move_choose_direction(grid);

	/* Want to move */
	return (true);
}


/**
 * ------------------------------------------------------------------------
 * Monster turn routines
 * These routines, culminating in monster_turn(), decide how a monster uses
 * its turn
 * ------------------------------------------------------------------------ */
/**
 * Lets the given monster attempt to reproduce.
 *
 * Note that "reproduction" REQUIRES empty space.
 *
 * Returns true if the monster successfully reproduced.
 */
bool multiply_monster(const struct monster *mon)
{
	struct loc grid;
	bool result;
	struct monster_group_info info = { mon->group_info[PRIMARY_GROUP].index, MON_GROUP_MEMBER };

	/*
	 * Pick an empty location except for uniques:  they can never
	 * multiply (need a check here as the ones in place_new_monster()
	 * are not sufficient for a unique shape of a shapechanged monster
	 * since it may have zero for cur_num in the race structure for the
	 * shape).
	 */
	if (!monster_is_shape_unique(mon) && scatter_ext(cave, &grid,
			1, mon->grid, 1, true, square_isempty) > 0) {
		/* Create a new monster (awake, no groups) */
		// L: now with groups!
		result = place_new_monster(cave, grid, mon->race, false, true,
				info, ORIGIN_DROP_BREED);
		/*
		 * Fix so multiplying a revealed camouflaged monster creates
		 * another revealed camouflaged monster.
		 */
		if (result) {
			struct monster *child = square_monster(cave, grid);

			if (child && monster_is_camouflaged(child)
					&& !monster_is_camouflaged(mon)) {
				become_aware(cave, child);
			}
		}
	} else {
		result = false;
	}

	/* Result */
	return (result);
}

/**
 * Attempt to reproduce, if possible.  All monsters are checked here for
 * lore purposes, the unfit fail.
 */
static bool monster_turn_multiply(struct monster *mon)
{
	int k = 0, y, x;

	struct monster_lore *lore = get_lore(mon->race);

	/* Too many breeders on the level already */
	if (cave->num_repro >= z_info->repro_monster_max) return false;

	/* No breeding in single combat */
	if (player->upkeep->arena_level) return false;  

	/* Count the adjacent monsters */
	for (y = mon->grid.y - 1; y <= mon->grid.y + 1; y++) {
		for (x = mon->grid.x - 1; x <= mon->grid.x + 1; x++) {
			if (square(cave, loc(x, y))->mon > 0) k++;
		}
	}

	/* Multiply slower in crowded areas */
	if ((k < 4) && (k == 0 || one_in_(k * z_info->repro_monster_rate))) {
		/* Successful breeding attempt, learn about that now */
		if (monster_is_visible(mon)) {
			rf_on(lore->flags, RF_MULTIPLY);
		}

		/* Leave now if not a breeder */
		if (!rf_has(mon->race->flags, RF_MULTIPLY)) {
			return false;
		}

		/* Try to multiply */
		if (multiply_monster(mon)) {
			/* Make a sound */
			if (monster_is_visible(mon)) {
				sound(MSG_MULTIPLY);
			}

			/* Multiplying takes energy */
			return true;
		}
	}

	return false;
}

/**
 * Check if a monster should stagger (that is, step at random) or not.
 * Always stagger when confused, but also deal with random movement for
 * RAND_25 and RAND_50 monsters.
 */
static enum monster_stagger monster_turn_should_stagger(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	int chance = 0, confused_chance, roll;

	/* Increase chance of being erratic for every level of confusion */
	int conf_level = monster_effect_level(mon, TMD_CONFUSED);
	while (conf_level) {
		int accuracy = 100 - chance;
		accuracy *= (100 - CONF_ERRATIC_CHANCE);
		accuracy /= 100;
		chance = 100 - accuracy;
		conf_level--;
	}
	confused_chance = chance;

	/* RAND_25 and RAND_50 are cumulative */
	if (rf_has(mon->race->flags, RF_RAND_25)) {
		chance += 25;
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_RAND_25);
	}

	if (rf_has(mon->race->flags, RF_RAND_50)) {
		chance += 50;
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_RAND_50);
	}

	roll = randint0(100);
	return (roll < confused_chance) ?
		 CONFUSED_STAGGER :
		 ((roll < chance) ? INNATE_STAGGER : NO_STAGGER);
}


/**
 * Helper function for monster_turn_can_move() to display a message for a
 * confused move into non-passable terrain.
 */
static void monster_display_confused_move_msg(struct monster *mon,
											  const char *m_name,
											  struct loc new)
{
	if (monster_is_visible(mon) && monster_is_in_view(mon)) {
		const char *m = square_feat(cave, new)->confused_msg;

		msg("%s %s.", m_name, (m) ? m : "stumbles");
	}
}


/**
 * Helper function for monster_turn_can_move() to slightly stun a monster
 * on occasion due to bumbling into something.
 */
static void monster_slightly_stun_by_move(struct monster *mon)
{
	if (mon->m_timed[TMD_STUN] < 5 && one_in_(3)) {
		mon_inc_timed(mon, TMD_STUN, 3, 0);
	}
}


/**
 * Work out if a monster can move through the grid, if necessary bashing 
 * down doors in the way.
 *
 * Returns true if the monster is able to move through the grid.
 */
static bool monster_turn_can_move(struct monster *mon, const char *m_name,
								  struct loc new, bool confused,
								  bool *did_something)
{
	struct monster_lore *lore = get_lore(mon->race);

	/* Always allow an attack upon the player or decoy. */
	if (square_isplayer(cave, new) || square_isdecoyed(cave, new)) {
		return true;
	}

	/* Dangerous terrain in the way */
	if (!confused && monster_hates_grid(mon, new)) {
		return false;
	}

	/* Floor is open? */
	if (square_ispassable(cave, new)) {
		return true;
	}

	/* Permanent wall in the way */
	if (square_isperm(cave, new)) {
		if (confused) {
			*did_something = true;
			monster_display_confused_move_msg(mon, m_name, new);
			monster_slightly_stun_by_move(mon);
		}
		return false;
	}

	/* Normal wall, door, or secret door in the way */

	/* There's some kind of feature in the way, so learn about
	 * kill-wall and pass-wall now */
	if (monster_is_visible(mon)) {
		rf_on(lore->flags, RF_PASS_WALL);
		rf_on(lore->flags, RF_KILL_WALL);
		rf_on(lore->flags, RF_SMASH_WALL);
	}

	/* Monster may be able to deal with walls and doors */
	if (rf_has(mon->race->flags, RF_PASS_WALL)) {
		return true;
	} else if (rf_has(mon->race->flags, RF_SMASH_WALL)) {
		/* Remove the wall and much of what's nearby */
		square_smash_wall(cave, new);

		/* Note changes to viewable region */
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		return true;
	} else if (rf_has(mon->race->flags, RF_KILL_WALL)) {
		/* Remove the wall */
		square_destroy_wall(cave, new);

		/* Note changes to viewable region */
		if (square_isview(cave, new))
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		return true;
	} else if (square_iscloseddoor(cave, new)|| square_issecretdoor(cave, new)){
		/* Don't allow a confused move to open a door. */
		bool can_open = rf_has(mon->race->flags, RF_OPEN_DOOR) &&
			!confused;
		/* During a confused move, a monster only bashes sometimes. */
		bool can_bash = rf_has(mon->race->flags, RF_BASH_DOOR) &&
			(!confused || one_in_(3));
		bool will_bash = false;

		/* Take a turn */
		if (can_open || can_bash) *did_something = true;

		/* Learn about door abilities */
		if (!confused && monster_is_visible(mon)) {
			rf_on(lore->flags, RF_OPEN_DOOR);
			rf_on(lore->flags, RF_BASH_DOOR);
		}

		/* If creature can open or bash doors, make a choice */
		if (can_open) {
			/* Sometimes bash anyway (impatient) */
			if (can_bash) {
				will_bash = one_in_(2) ? true : false;
			}
		} else if (can_bash) {
			/* Only choice */
			will_bash = true;
		} else {
			/* Door is an insurmountable obstacle */
			if (confused) {
				*did_something = true;
				monster_display_confused_move_msg(mon, m_name, new);
				monster_slightly_stun_by_move(mon);
			}
			return false;
		}

		/* Now outcome depends on type of door */
		if (square_islockeddoor(cave, new)) {
			/* Locked door -- test monster strength against door strength */
			int k = square_door_power(cave, new);
			if (randint0(mon->hp / 10) > k) {
				if (will_bash) {
					msg("%s slams against the door.", m_name);
				} else {
					msg("%s fiddles with the lock.", m_name);
				}

				/* Reduce the power of the door by one */
				square_set_door_lock(cave, new, k - 1);
			}
			if (confused) {
				/* Didn't learn above; apply now since attempted to bash. */
				if (monster_is_visible(mon)) {
					rf_on(lore->flags, RF_BASH_DOOR);
				}
				/* When confused, can stun itself while bashing. */
				monster_slightly_stun_by_move(mon);
			}
		} else {
			/* Closed or secret door -- always open or bash */
			if (square_isview(cave, new))
				player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			if (will_bash) {
				square_smash_door(cave, new);

				msg("You hear a door burst open!");
				disturb(player);

				if (confused) {
					/* Didn't learn above; apply since bashed the door. */
					if (monster_is_visible(mon)) {
						rf_on(lore->flags, RF_BASH_DOOR);
					}
					/* When confused, can stun itself while bashing. */
					monster_slightly_stun_by_move(mon);
				}

				/* Fall into doorway */
				return true;
			} else {
				square_open_door(cave, new);
			}
		}
	} else if (confused) {
		*did_something = true;
		monster_display_confused_move_msg(mon, m_name, new);
		monster_slightly_stun_by_move(mon);
	}

	return false;
}

/**
 * Try to break a glyph.
 */
static bool monster_turn_attack_glyph(struct monster *mon, struct loc new)
{
	assert(square_iswarded(cave, new));

	/* Break the ward */
	if (randint1(z_info->glyph_hardness) < mon->race->level) {
		struct trap_kind *rune = lookup_trap("glyph of warding");

		/* Describe observable breakage */
		if (square_isseen(cave, new)) {
			msg("The rune of protection is broken!");
		}

		/* Break the rune */
		assert(rune);
		square_remove_all_traps_of_type(cave, new, rune->tidx);

		return true;
	}

	/* Unbroken ward - can't move */
	return false;
}

/**
 * Try to push past / kill another monster.  Returns true on success.
 */
static bool monster_turn_try_push(struct monster *mon, const char *m_name,
								  struct loc new)
{
	struct monster *mon1 = square_monster(cave, new);
	struct monster_lore *lore = get_lore(mon->race);

	/* Kill weaker monsters */
	int kill_ok = monster_can_kill(mon, new);

	/* Move weaker monsters if they can swap places */
	/* (not in a wall) */
	int move_ok = (monster_can_move(mon, new) &&
				   square_ispassable(cave, mon->grid));

	if (kill_ok || move_ok) {
		/* Get the names of the monsters involved */
		char n_name[80];
		monster_desc(n_name, sizeof(n_name), mon1, MDESC_IND_HID);

		/* Learn about pushing and shoving */
		if (monster_is_visible(mon)) {
			rf_on(lore->flags, RF_KILL_BODY);
			rf_on(lore->flags, RF_MOVE_BODY);
		}

		/* Reveal camouflaged monsters */
		if (monster_is_camouflaged(mon1))
			become_aware(cave, mon1);

		/* Note if visible */
		if (monster_is_visible(mon) && monster_is_in_view(mon))
			msg("%s %s %s.", m_name, kill_ok ? "tramples over" : "pushes past",
				n_name);

		/* Monster ate another monster */
		if (kill_ok)
			delete_monster(cave, new);

		monster_swap(mon->grid, new);
		return true;
	}

	return false;
}

/**
 * Grab all objects from the grid.
 */
static void monster_turn_grab_objects(struct monster *mon, const char *m_name,
									  struct loc new)
{
	struct monster_lore *lore = get_lore(mon->race);
	struct object *obj;
	bool visible = monster_is_visible(mon);

	/* Learn about item pickup behavior */
	if (visible && square_object(cave, new)) {
		rf_on(lore->flags, RF_TAKE_ITEM);
		rf_on(lore->flags, RF_KILL_ITEM);
	}

	/* Abort if can't pickup/kill */
	if (!rf_has(mon->race->flags, RF_TAKE_ITEM) &&
			!rf_has(mon->race->flags, RF_KILL_ITEM)) {
		return;
	}

	/* Take or kill objects on the floor */
	obj = square_object(cave, new);
	while (obj) {
		char o_name[80];
		bool safe = obj->artifact ? true : false;
		struct object *next = obj->next;

		/* Skip mimicked objects */
		if (obj->mimicking_m_idx) {
			obj = next;
			continue;
		}

		// L: make sure object is known if it should be
		if (square_isview(cave, new)) {
			object_see(player, obj);
		}

		/* Get the object name */
		object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_FULL, player);

		/* React to objects that hurt the monster */
		if (react_to_slay(obj, mon)) {
			safe = true;
		}

		/* Try to pick up, or crush */
		if (safe) {
			/* Only give a message for "take_item" */
			if (rf_has(mon->race->flags, RF_TAKE_ITEM)
					&& visible
					&& square_isview(cave, new)
					&& !ignore_item_ok(player, obj)) {
				/* Dump a message */
				msg("%s tries to pick up %s, but fails.", m_name, o_name);
			}
		} else if (rf_has(mon->race->flags, RF_TAKE_ITEM)) {
			/* Try to carry */
			assert(player->cave->objects[obj->oidx] == obj->known);
			assert(cave->objects[obj->oidx] == obj);
			square_excise_object(cave, new, obj);
			assert(player->cave->objects[obj->oidx] == obj->known);
			assert(cave->objects[obj->oidx] == obj);
			if (monster_carry(cave, mon, obj)) {
				assert(!player->cave->objects[obj->oidx] || player->cave->objects[obj->oidx] == obj->known);
				assert(cave->objects[obj->oidx] == obj || !cave->objects[obj->oidx]);
				/* Describe observable situations */
				if (square_isseen(cave, new) && !ignore_item_ok(player, obj)) {
					assert(obj->known);
					msg("%s picks up %s.", m_name, o_name);
				}

				/* Delete the object */
				square_note_spot(cave, new);
				square_light_spot(cave, new);

				assert(!player->cave->objects[obj->oidx] || player->cave->objects[obj->oidx] == obj->known);
				assert(cave->objects[obj->oidx] == obj || !cave->objects[obj->oidx]);
			} else if (!floor_carry(cave, new, obj, NULL)) {
				drop_near(cave, &obj, 0, new, false, false);
			}
		} else {
			/* Describe observable situations */
			if (square_isseen(cave, new) && !ignore_item_ok(player, obj)) {
				msgt(MSG_DESTROY, "%s crushes %s.", m_name, o_name);
			}

			/* Delete the object */
			square_delete_object(cave, new, obj, true, true);
		}

		/* Next object */
		obj = next;
	}
}

static bool monster_turn_talk(struct monster *mon)
{
	if (!mflag_has(mon->mflag, MFLAG_TALKING)) {
		return false;
	}
	if (mon->target.who == TARGET_WHO_MONSTER ||
			mon->reaction <= MON_REACT_NO_TALK) {
		mflag_off(mon->mflag, MFLAG_TALKING);
		return false;
	}
	return true;
}


/**
 * Process a monster's turn
 *
 * In several cases, we directly update the monster lore
 *
 * Note that a monster is only allowed to "reproduce" if there
 * are a limited number of "reproducing" monsters on the current
 * level.  This should prevent the level from being "swamped" by
 * reproducing monsters.  It also allows a large mass of mice to
 * prevent a louse from multiplying, but this is a small price to
 * pay for a simple multiplication method.
 *
 * XXX Monster fear is slightly odd, in particular, monsters will
 * fixate on opening a door even if they cannot open it.  Actually,
 * the same thing happens to normal monsters when they hit a door
 *
 * In addition, monsters which *cannot* open or bash down a door
 * will still stand there trying to open it...  XXX XXX XXX
 *
 * Technically, need to check for monster in the way combined
 * with that monster being in a wall (or door?) XXX
 */
static void monster_turn(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);

	bool did_something = false;

	int i, x, y;
	int dir = 0;
	enum monster_stagger stagger;
	bool tracking = false;
	char m_name[80];

	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), mon,
		MDESC_CAPITAL | MDESC_IND_HID | MDESC_COMMA);

	// L: become aware if we aren't yet
	if (monster_can_see_player(mon)) {
		monster_become_aware(mon);
	}

	// L: notice secret doors if they're open
	if (one_in_(2) && rf_has(mon->race->flags,  RF_SAPIENT) && mon_will_attack_player(mon, player)) {
		for (y = 1; y < cave->height - 1; ++y) {
			for (x = 1; x < cave->height - 1; ++x) {
				if (square_in_bounds_fully(cave, loc(x, y)) &&
						square(cave, loc(x, y))->feat == FEAT_OPEN_SECRET &&
						los(cave, mon->grid, loc(x, y))) {
					square_set_feat(cave, loc(x, y), FEAT_OPEN);
					if (monster_is_in_view(mon)) {
						char desc[64];
						monster_desc(desc, sizeof(desc), mon, MDESC_STANDARD);
						msg("%s notices a secret door!", desc);
					}
				}
			}
		}
	}

	/* If we're in a web, deal with that */
	if (square_iswebbed(cave, mon->grid)) {
		/* Learn web behaviour */
		if (monster_is_visible(mon)) {
			rf_on(lore->flags, RF_PASS_WEB);
		}

		/* If we can pass, no need to clear */
		if (!rf_has(mon->race->flags, RF_PASS_WEB)) {
			/* Learn wall behaviour */
			if (monster_is_visible(mon)) {
				rf_on(lore->flags, RF_PASS_WALL);
				rf_on(lore->flags, RF_KILL_WALL);
			}

			/* Now several possibilities */
			if (rf_has(mon->race->flags, RF_PASS_WALL)) {
				/* Insubstantial monsters go right through */
			} else if (monster_passes_walls(mon)) {
				/* If you can destroy a wall, you can destroy a web */
				struct trap_kind *web = lookup_trap("web");

				assert(web);
				square_remove_all_traps_of_type(cave,
					mon->grid, web->tidx);
			} else if (rf_has(mon->race->flags, RF_CLEAR_WEB)) {
				/* Clearing costs a turn (assume there are no other "traps") */
				struct trap_kind *web = lookup_trap("web");

				assert(web);

				// L: clearing webs is somewhat difficult
				// L: but you can clear and move in the same turn
				if (one_in_(10)) {
					if (monster_is_visible(mon)) {
						msg("%s clears a web.", m_name);
						rf_on(lore->flags, RF_CLEAR_WEB);
					}
					square_remove_all_traps_of_type(cave,
							mon->grid, web->tidx);
				}
				else {
					if (one_in_(5)) {
						msg("%s struggles in a web.", m_name);
					}
					return;
				}
			} else {
				if (one_in_(25)) {
					msg("%s struggles futilely in a web.", m_name);
				}
				/* Stuck */
				return;
			}
		}
	}

	// L: check our target
	mon_check_target(cave, mon);

	/* Let other group monsters know about the player */
	monster_group_rouse(cave, mon);

	/* Try to multiply - this can use up a turn */
	if (monster_turn_multiply(mon)) {
		return;
	}

	if (monster_turn_talk(mon)) {
		return;
	}

	if (monster_turn_equip_item(mon)) {
		return;
	}

	/* Attempt a ranged attack */
	if (make_ranged_attack(mon)) {
		return;
	}

	/* Work out what kind of movement to use - random movement or AI */
	stagger = monster_turn_should_stagger(mon);
	if (stagger == NO_STAGGER) {
		/* If there's no sensible move, we're done */
		if (!get_move(mon, &dir, &tracking)) {
			/* L: grab stuff we're on top of */
			monster_turn_grab_objects(mon, m_name, mon->grid);
			return;
		}
	}

	/* Try to move first in the chosen direction, or next either side of the
	 * chosen direction, or next at right angles to the chosen direction.
	 * Monsters which are tracking by sound or scent will not move if they
	 * can't move in their chosen direction. */
	for (i = 0; i < 5 && !did_something; i++) {
		/* Get the direction (or stagger) */
		int d = (stagger != NO_STAGGER) ? ddd[randint0(8)] : side_dirs[dir][i];

		/* Get the grid to step to or attack */
		struct loc new = loc_sum(mon->grid, ddgrid[d]);

		/* Tracking monsters have their best direction, don't change */
		if ((i > 0) && stagger == NO_STAGGER &&
			!square_isview(cave, mon->grid) && tracking) {
			break;
		}

		/* Check if we can move */
		if (!monster_turn_can_move(mon, m_name, new,
								   stagger == CONFUSED_STAGGER, &did_something))
			continue;

		/* Try to break the glyph if there is one.  This can happen multiple
		 * times per turn because failure does not break the loop */
		if (square_iswarded(cave, new) && !monster_turn_attack_glyph(mon, new)) {
			continue;
		}

		if (square_iswebbed(cave, new)) {
			if (!monster_passes_walls(mon) &&
					!rf_has(mon->race->flags, RF_PASS_WEB) &&
					!one_in_(5)) {
				continue;
			}
		}

		/* Break a decoy if there is one */
		if (square_isdecoyed(cave, new)) {
			/* Learn about if the monster attacks */
			if (monster_is_visible(mon)) {
				rf_on(lore->flags, RF_NEVER_BLOW);
			}

			/* Some monsters never attack */
			if (rf_has(mon->race->flags, RF_NEVER_BLOW)) {
				continue;
			}

			/* Wait a minute... */
			square_destroy_decoy(cave, new);
			did_something = true;
			break;
		}

		/* The player is in the way. */
		if (square_isplayer(cave, new)) {
			/* Learn about if the monster attacks */
			if (monster_is_visible(mon)) {
				rf_on(lore->flags, RF_NEVER_BLOW);
			}

			/* Some monsters never attack */
			if (rf_has(mon->race->flags, RF_NEVER_BLOW)) {
				continue;
			}

			if (!mon_will_attack_player(mon, player) && stagger != CONFUSED_STAGGER) {
				if (mon->target.who == TARGET_WHO_PLAYER) return;
			    continue;
			}

			/* Otherwise, attack the player */
			make_attack_normal(mon, player);

			did_something = true;
			break;
		} else {
			/* Some monsters never move */
			if (rf_has(mon->race->flags, RF_NEVER_MOVE)) {
				/* Learn about lack of movement */
				if (monster_is_visible(mon))
					rf_on(lore->flags, RF_NEVER_MOVE);

				return;
			}
		}

		/* A monster is in the way, try to push past/kill */
		struct monster *other = square_monster(cave, new);
		if (other && mon_will_attack_mon(mon, other)) {
			did_something = monster_attack_monster(mon, other);
		} else if (other) {
			did_something = monster_turn_try_push(mon, m_name, new);
		} else {
			/* Otherwise we can just move */
			monster_swap(mon->grid, new);
			did_something = true;
		}

		/* Scan all objects in the grid, if we reached it */
		if (mon == square_monster(cave, new)) {
			monster_turn_grab_objects(mon, m_name, new);
		}
	}

	if (did_something) {
		// L: see equipped items
		if (monster_is_visible(mon)) {
			square_know_equipped_object(cave, mon->grid, NULL);
		}

		/* Learn about no lack of movement */
		if (monster_is_visible(mon)) {
			rf_on(lore->flags, RF_NEVER_MOVE);
		}

		/* Possible disturb */
		if (monster_is_visible(mon) && monster_is_in_view(mon) && 
				OPT(player, disturb_near) && mon_will_attack_player(mon, player)) {
			disturb(player);
		}
	}

	/* Out of options - monster is paralyzed by fear (unless attacked) */
	if (!did_something && mon->m_timed[TMD_AFRAID]) {
		int amount = mon->m_timed[TMD_AFRAID];
		mon_clear_timed(mon, TMD_AFRAID, MON_TMD_FLG_NOMESSAGE);
		mon_inc_timed(mon, TMD_PARALYZED, amount, MON_TMD_FLG_NOTIFY);
	}

	/* If we see an unaware monster do something, become aware of it */
	if (did_something && monster_is_camouflaged(mon)) {
		become_aware(cave, mon);
	}
}


/**
 * ------------------------------------------------------------------------
 * Processing routines that happen to a monster regardless of whether it
 * gets a turn, and/or to decide whether it gets a turn
 * ------------------------------------------------------------------------ */
/**
 * Determine whether a monster is active or passive
 */
static bool monster_check_active(struct monster *mon)
{
	// is the monster resting alongside the player?
	bool rwp = mon_will_follow_player(mon, player) &&
			distance(mon->grid, player->mon.grid) < 5 &&
			player_is_resting(player);
	
	if (mon->target.who != TARGET_WHO_NONE && !rwp) {
		// L: monster is currently doing something
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (mon->hp < mon->maxhp) {
		/* Monster is hurt */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (square_isview(cave, mon->grid) && !rwp) {
		/* Monster can "see" the player (checked backwards) */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (monster_taking_terrain_damage(cave, mon)) {
		/* Monster is taking damage from the terrain */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (mon->m_timed[TMD_POISONED] || mon->m_timed[TMD_TOXIC]) {
		// L: monster is taking ongoing damage
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else {
		/* Otherwise go passive */
		mflag_off(mon->mflag, MFLAG_ACTIVE);
	}

	return mflag_has(mon->mflag, MFLAG_ACTIVE);
}

/**
 * Wake a monster or reduce its depth of sleep
 *
 * Chance of waking up is dependent only on the player's stealth, but the
 * amount of sleep reduction takes into account the monster's distance from
 * the player.  Currently straight line distance is used; possibly this
 * should take into account dungeon structure.
 * 
 * L: rewritten; there is a minimum distance for each monster for it to
 * decrease sleep now
 */
static void monster_reduce_sleep(struct monster *mon)
{
	// Get the name
	char m_name[80];
	monster_desc(m_name, sizeof(m_name), mon, 
		MDESC_CAPITAL | MDESC_IND_HID | MDESC_COMMA);

	/* Aggravation */
	if (player_of_has(player, OF_AGGRAVATE)) {

		/* Wake the monster, make it aware */
		monster_wake(mon, false, 100);

		/* Notify the player if aware */
		if (monster_is_obvious(mon)) {
			msg("%s wakes up.", m_name);
			equip_learn_flag(player, OF_AGGRAVATE);
		}
	} else {
		int stealth = player->mon.state.skills[SKILL_STEALTH] / 5;
		int local_noise = cave->noise.grids[mon->grid.y][mon->grid.x];
		int local_smell = cave->scent.grids[mon->grid.y][mon->grid.x];
		bool woke_up = false;
		int curr = mon->m_timed[TMD_ASLEEP];
		// L: monster wakes up faster if it's louder or they can smell but not if they can see
		int distfact = MAX(0, 40 - local_noise * 2 - local_smell - stealth) / 4;
		// L: sleep reduction increases exponentially
		int16_t sred = 1 << MIN(14, distfact) / MAX(stealth + 1, 1);

		/* Note a complete wakeup */
		/* L: also note getting close to wakeup */
		if (curr <= sred) {
			woke_up = true;
			mon_check_target(cave, mon);
		} else if (((curr & 0xf) < sred) &&
		        (curr - sred <= 64) &&
				monster_is_obvious(mon)) {
			msg("%s stirs.", m_name);
		}

		/* Monster wakes up a bit */
		if (sred) {
			mon_dec_timed(mon, TMD_ASLEEP, sred, MON_TMD_FLG_NOTIFY);
		}

		/* Update knowledge */
		if (monster_is_obvious(mon)) {
			struct monster_lore *lore = get_lore(mon->race);
			if (!woke_up && lore->ignore < UCHAR_MAX)
				lore->ignore++;
			else if (woke_up && lore->wake < UCHAR_MAX)
				lore->wake++;
			lore_update(mon->race, lore);
		}
	}
}

/**
 * Process a monster's timed effects, e.g. decrease them.
 *
 * Returns true if the monster is skipping its turn.
 */
bool process_monster_timed(struct monster *mon)
{
	/* If the monster is asleep or just woke up, then it doesn't act */
	if (mon->m_timed[TMD_ASLEEP]) {
		monster_reduce_sleep(mon);
		return true;
	}

	if (mon->m_timed[TMD_FAST])
		mon_dec_timed(mon, TMD_FAST, 1, 0);

	if (mon->m_timed[TMD_SLOW])
		mon_dec_timed(mon, TMD_SLOW, 1, 0);

	if (mon->m_timed[TMD_PARALYZED])
		mon_dec_timed(mon, TMD_PARALYZED, 1, 0);

	if (mon->m_timed[TMD_DISEN])
		mon_dec_timed(mon, TMD_DISEN, 1, 0);

	if (mon->m_timed[TMD_STUN])
		mon_dec_timed(mon, TMD_STUN, 1, MON_TMD_FLG_NOTIFY);

	if (mon->m_timed[TMD_CONFUSED]) {
		mon_dec_timed(mon, TMD_CONFUSED, 1, MON_TMD_FLG_NOTIFY);
	}

	if (mon->m_timed[TMD_CHANGED]) {
		mon_dec_timed(mon, TMD_CHANGED, 1, MON_TMD_FLG_NOTIFY);
	}

	if (mon->m_timed[TMD_AFRAID]) {
		int d = randint1(mon->race->level / 10 + 1);
		mon_dec_timed(mon, TMD_AFRAID, d, MON_TMD_FLG_NOTIFY);
	}

	if (mon->m_timed[TMD_TOXIC]) {
		mon_dec_timed(mon, TMD_TOXIC, 1, 0);
		mon_inc_timed(mon, TMD_POISONED, 5, 0);
	} else if (mon->m_timed[TMD_POISONED]) {
		mon_dec_timed(mon, TMD_POISONED, 1, 0);
	}

	if (mon->m_timed[TMD_SUFFOCATE]) {
		mon_dec_timed(mon, TMD_SUFFOCATE, 1, 0);
		if (one_in_(3)) {
			mon_inc_timed(mon, TMD_SLOW, 5, 0);
		}
		if (one_in_(3)) {
			mon_inc_timed(mon, TMD_STUN, 5, 0);
		}
		if (one_in_(3)) {
			mon_inc_timed(mon, TMD_CONFUSED, 5, 0);
		}
	}

	if (mon->m_timed[TMD_SUMMONED]) {
		mon_dec_timed(mon, TMD_SUMMONED, 1, 0);
		if (!mon->m_timed[TMD_SUMMONED]) {
			char mdesc[80];
			monster_desc(mdesc, sizeof(mdesc), mon, MDESC_TARG | MDESC_CAPITAL);
			msg("%s vanishes!", mdesc);
			delete_monster_idx(cave, mon->midx);
			return true;
		}
	}

	// L: these are typically only player timed effcts (currently)
	
	if (mon->m_timed[TMD_RAD_POIS]) {
		struct loc g = mon->grid;
		effect_simple(EF_PROJECT_LOS, source_monster(mon->midx), "2d9", PROJ_MON_POIS, 0, 0, g.y, g.x, NULL);
	}
	
	if (mon->m_timed[TMD_CALL_STORM] && one_in_(5)) {
		struct loc l = mon->grid;
		char *dam = format("2d%i", my_int_sqrt(mon->m_timed[TMD_CALL_STORM]) + 49);
		int rad = one_in_(3) ? 1 : 0;
		effect_simple(EF_RANDOM_MON_DAMAGE, source_monster(mon->midx), dam, PROJ_ELEC, rad, 0, l.y, l.x, NULL);
	}

	/* Timed healing */
	if (mon->m_timed[TMD_HEAL]) {
		bool ident = false;
		effect_simple(EF_HEAL_HP, source_monster(mon->midx), "30", 0, 0, 0, 0, 0, &ident);
	}

	// L: player stuff end

	/* Always miss turn if held or commanded, one in STUN_MISS_CHANCE chance
	 * of missing if stunned,  */
	if (mon->m_timed[TMD_PARALYZED] /*|| mon->m_timed[MON_TMD_COMMAND]*/) {
		return true;
	} else if (mon->m_timed[TMD_STUN]) {
		return one_in_(STUN_MISS_CHANCE);
	} else {
		return false;
	}
}

#if 0
/**
 * Monster regeneration of HPs.
 */
static void regen_monster(struct monster *mon, int num)
{
	/* Regenerate (if needed) */
	if (mon->hp < mon->maxhp) {
		/* Base regeneration */
		int frac = mon->maxhp / 100;

		/* Minimal regeneration rate */
		if (!frac) frac = 1;

		/* Some monsters regenerate quickly */
		if (rf_has(mon->race->flags, RF_HI_REGEN)) frac *= 15;
		else if (rf_has(mon->race->flags, RF_REGENERATE)) frac *= 2;

		/* Multiply by number of regenerations */
		frac *= num;

		/* Regenerate */
		mon->hp += frac;

		/* Do not over-regenerate */
		if (mon->hp > mon->maxhp) mon->hp = mon->maxhp;

		/* Redraw (later) if needed */
		if (player->upkeep->health_who == mon) {
			player->upkeep->redraw |= (PR_HEALTH);
		}
	}
}
#endif


/**
 * ------------------------------------------------------------------------
 * Monster processing routines to be called by the main game loop
 * ------------------------------------------------------------------------ */
/**
 * Process all the "live" monsters, once per game turn.
 *
 * During each game turn, we scan through the list of all the "live" monsters,
 * (backwards, so we can excise any "freshly dead" monsters), energizing each
 * monster, and allowing fully energized monsters to move, attack, pass, etc.
 *
 * This function and its children are responsible for a considerable fraction
 * of the processor time in normal situations, greater if the character is
 * resting.
 */
void process_monsters(int minimum_energy)
{
	int i;
	int mspeed;

	/* Only process some things every so often */
	bool targcheck = false;

	/* Regenerate hitpoints and mana every 128 game turns */
	if (!(turn & 0x7f)) {
		//regen = true;
		// Recheck monsters' targets every 2000ish turns
		if (!(turn & 0x7ff)) {
			targcheck = true;
		}
	}

	/* Process the monsters (backwards) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		struct monster *mon;
		bool moving;

		/* Handle "leaving" */
		if (player->is_dead || player->upkeep->generate_level) break;

		/* Get a 'live' monster */
		mon = cave_monster(cave, i);
		if (!mon->race) continue;

		/* Ignore monsters that have already been handled */
		if (mflag_has(mon->mflag, MFLAG_HANDLED)) {
			continue;
		}

		// L: recheck targets every so often
		if (targcheck) {
			mon_check_target(cave, mon);
		}

		update_mon_state(mon);

		/* Not enough energy to move yet */
		if (mon->energy < minimum_energy) continue;

		/* Does this monster have enough energy to move? */
		moving = mon->energy >= z_info->move_energy ? true : false;

		/* Prevent reprocessing */
		mflag_on(mon->mflag, MFLAG_HANDLED);

		/* Handle monster regeneration if requested */
		// L: done elsewhere
		/*if (regen) {
			regen_monster(mon, 1);
		}*/

		/* Calculate the net speed */
		mspeed = mon->state.speed;
		/*if (mon->m_timed[TMD_FAST])
			mspeed += 10;
		if (mon->m_timed[TMD_SLOW]) {
			int slow_level = monster_effect_level(mon, TMD_SLOW);
			mspeed -= (2 * slow_level);
		}*/

		/* Give this monster some energy */
		mon->energy += turn_energy(mspeed);

		/* End the turn of monsters without enough energy to move */
		if (!moving) {
			continue;
		}

		/* Use up "some" energy */
		mon->energy -= z_info->move_energy;

		/* Mimics lie in wait */
		if (monster_is_mimicking(mon)) continue;

		/* Check if the monster is active */
		if (monster_check_active(mon)) {
			bool take_turn;
			/* Set this monster to be the current actor */
			cave->mon_current = i;

			/* Process timed effects - skip turn if necessary */
			take_turn = !process_monster_timed(mon);

			// L: pmt can now kill the monster
			if (mon && mon->race) {
				if (take_turn) {
					/* The monster takes its turn */
					monster_turn(mon);
				}

				/*
				 * For symmetry with the player, monster can take
				 * terrain damage after its turn.
				 */
				monster_take_terrain_damage(mon);
				monster_take_timed_damage(mon, turn_energy(mspeed));

				/* Monster is no longer current */
				cave->mon_current = -1;
			}
		}
	}

	/* Update monster visibility after this */
	/* XXX This may not be necessary */
	player->upkeep->update |= PU_MONSTERS;
}

/**
 * Clear 'moved' status from all monsters.
 *
 * Clear noise if appropriate.
 */
void reset_monsters(void)
{
	int i;
	struct monster *mon;

	/* Process the monsters (backwards) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		/* Access the monster */
		mon = cave_monster(cave, i);

		/* Monster is ready to go again */
		mflag_off(mon->mflag, MFLAG_HANDLED);
	}
}

/**
 * Allow monsters on a frozen persistent level to recover
 */
void restore_monsters(void)
{
	int i;
	struct monster *mon;

	/* Get the number of turns that have passed */
	int num_turns = turn - cave->turn;

	/* Process the monsters (backwards) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		int status, status_red;

		/* Access the monster */
		mon = cave_monster(cave, i);

		/* Regenerate */
		//regen_monster(mon, num_turns / 100);

		/* Handle timed effects */
		status_red = num_turns * turn_energy(mon->state.speed) / z_info->move_energy;
		if (status_red > 0) {
			for (status = 0; status < TMD_MAX; status++) {
				if (mon->m_timed[status]) {
					mon_dec_timed(mon, status, status_red, 0);
				}
			}
		}
	}
}
