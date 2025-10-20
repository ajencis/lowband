/**
 * \file cmd-cave.c
 * \brief Chest and door opening/closing, disarming, running, resting, &c.
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
#include "cmds.h"
#include "effects.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-path.h"
#include "player-properties.h"
#include "player-quest.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "store.h"
#include "ui-mon-list.h"
#include "trap.h"


static bool check_can_take_stairs(struct player *p, int time)
{
    if (p->mon.m_timed[TMD_CUT])
	{
		msg("You would bleed out on the stairs.");
		return false;
	}
	else if (p->mon.m_timed[TMD_POISONED])
	{
		msg("You would die of poison on the stairs.");
		return false;
	}
	else if (p->mon.m_timed[TMD_BLOODLUST])
	{
		msg("There's no-one to fight on the stairs.");
		return false;
	}
	else if (p->mon.m_timed[TMD_FOOD] < 10 + time / 10)
	{
		msg("You should eat before you take the stairs.");
		return false;
	}
	return true;
}

/**
 * L: try to clear a web from the current square
 * \return whether player should thereby skip their turn
 */
/*static bool clear_web(struct player *p)
{
	int dam;

	if (!square_iswebbed(cave, p->mon.grid)) {
		return false;
	}
	if (player_of_has(p, OF_PASS_WEB)) {
		return false;
	}
	if (pf_has(p->mon.state.pflags, PF_PASS_WALL)) {
		return false;
	}

	dam = adj_str_web(p->mon.state.stat_ind[STAT_STR]);
	dam = randint1(dam);

	square_reduce_feat_size(cave, p->mon.grid, FEAT_WEB, dam);

	if (square_has_feat(cave, p->mon.grid, FEAT_WEB)) {
		msg("You struggle against the web.");
	}
	else {
		msg("You clear the web.");
	}

	return false;
}*/


/**
 * Go up one level
 */
void do_cmd_go_up(struct command *cmd)
{
	int ascend_to;
	int time;

	/* Verify stairs */
	if (!square_isupstairs(cave, player->mon.grid)) {
		do_cmd_navigate_up(cmd);
		return;
	}

	/* Force descend */
	if (OPT(player, birth_force_descend)) {
		msg("Nothing happens!");
		return;
	}
	
	ascend_to = dungeon_get_next_level(player, player->depth, -randint1(player->depth / 10 + 2));
	
	if (ascend_to >= player->depth) {
		msg("You can't go up from here!");
		return;
	}

	time = player->depth * (player->depth - ascend_to) * 100 + 5000;

	/* L: make sure the player won't die on the stairs */
	if (!check_can_take_stairs(player, time)) return;

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* L: prepare to take time on the stairs */
	player->upkeep->taking_stairs = time / 10;

	/* Success */
	msgt(MSG_STAIRS_UP, "You enter a maze of up staircases.");

	/* Create a way back */
	player->upkeep->create_up_stair = false;
	player->upkeep->create_down_stair = true;
	
	/* Change level */
	dungeon_change_level(player, ascend_to);
}


/**
 * Go down one level
 */
void do_cmd_go_down(struct command *cmd)
{
	int descend_to = dungeon_get_next_level(player, player->depth, randint1(player->depth / 10 + 2));
	int time; int posstime;

	/* Verify stairs */
	if (!square_isdownstairs(cave, player->mon.grid)) {
		do_cmd_navigate_down(cmd);
		return;
	}

	/* Paranoia, no descent from z_info->max_depth - 1 */
	if (player->depth == z_info->max_depth - 1) {
		msg("The dungeon does not appear to extend deeper");
		return;
	}

	time = descend_to * (descend_to - player->depth) * 100 + 5000;
	posstime = descend_to * (player->depth / 10 + 2) * 100 + 5000;

	if (!check_can_take_stairs(player, posstime)) return;

	/* Warn a force_descend player if they're going to a quest level */
	if (OPT(player, birth_force_descend)) {
		descend_to = dungeon_get_next_level(player,
			player->max_depth, 1);
		if (is_quest(player, descend_to) &&
			!get_check("Are you sure you want to descend? "))
			return;
	}

	/* Hack -- take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* L: prepare to take time on the stairs */
	player->upkeep->taking_stairs = time / 10;

	/* Success */
	msgt(MSG_STAIRS_DOWN, "You enter a maze of down staircases.");

	/* Create a way back */
	player->upkeep->create_up_stair = true;
	player->upkeep->create_down_stair = false;

	/* Change level */
	dungeon_change_level(player, descend_to);
}



/**
 * Determine if a given grid may be "opened"
 */
static bool do_cmd_open_test(struct player *p, struct loc grid)
{
	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		msg("You see nothing there.");
		return false;
	}

	/* Must be a closed door */
	if (!square_iscloseddoor(p->cave, grid)) {
		msgt(MSG_NOTHING_TO_OPEN, "You see nothing there to open.");
		/*if (square_iscloseddoor(p->cave, grid)) {
			square_forget(cave, grid);
			square_light_spot(cave, grid);
		}*/
		return false;
	}

	return true;
}


/**
 * Perform the basic "open" command on doors
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_open_aux(struct loc grid)
{
	bool more = false;

	/* Verify legality */
	if (!do_cmd_open_test(player, grid)) return (false);

	/* Locked door */
	if (square_islockeddoor(cave, grid)) {
		int chance = calc_unlocking_chance(player,
			square_door_power(cave, grid), no_light(player));

		if (randint0(100) < chance) {
			/* Message */
			msgt(MSG_LOCKPICK, "You have picked the lock.");

			/* Open the door */
			square_open_door(cave, grid);

			/* Update the visuals */
			square_memorize_feats(player, cave, grid);
			square_light_spot(cave, grid);
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			/* Experience */
			/* Removed to avoid exploit by repeatedly locking and unlocking */
			/* player_exp_gain(player, 1); */
		} else {
			event_signal(EVENT_INPUT_FLUSH);

			/* Message */
			msgt(MSG_LOCKPICK_FAIL, "You failed to pick the lock.");

			/* We may keep trying */
			more = true;
		}
	} else {
		/* Closed door */
		square_open_door(cave, grid);
		square_memorize_feats(player, cave, grid);
		square_light_spot(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		sound(MSG_OPENDOOR);
	}

	/* Result */
	return (more);
}



/**
 * Open a closed/locked/jammed door or a closed/locked chest.
 *
 * Unlocking a locked chest is worth one experience point; since doors are
 * player lockable, there is no experience for unlocking doors.
 */
void do_cmd_open(struct command *cmd)
{
	struct loc grid;
	int dir;
	struct object *obj;
	bool more = false;
	int err;
	struct monster *mon;

	/* Get arguments */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;
		int n_closed_doors, n_locked_chests;

		n_closed_doors = count_feats(&grid1, square_iscloseddoor, false);
		n_locked_chests = count_chests(&grid1, CHEST_OPENABLE);

		/*
		 * If prompting for a direction, allow the player's square as
		 * an option if there's a chest nearby.
		 */
		if (n_closed_doors + n_locked_chests == 1) {
			dir = motion_dir(player->mon.grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, n_locked_chests > 0)) {
			return;
		}
	}

	/* Get location */
	grid = loc_sum(player->mon.grid, ddgrid[dir]);

	/* Check for chest */
	obj = chest_check(player, grid, CHEST_OPENABLE);

	/* Check for door */
	if (!obj && !do_cmd_open_test(player, grid)) {
		/* Cancel repeat */
		disturb(player);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->mon.grid, ddgrid[dir]);

		/* Check for chest */
		obj = chest_check(player, grid, CHEST_OPENABLE);
	}

	/* Monster */
	mon = square_monster(cave, grid);
	if (mon) {
		/* Camouflaged monsters surprise the player */
		if (monster_is_camouflaged(mon)) {
			become_aware(cave, mon);

			/* Camouflaged monster wakes up and becomes aware */
			monster_wake(mon, false, 100);
		} else {
			/* Message */
			msg("There is a monster in the way!");

			/* Attack */
			py_attack(player, grid);
		}
	} else if (obj) {
		/* Chest */
		more = do_cmd_open_chest(grid, obj);
	} else {
		/* Door */
		more = do_cmd_open_aux(grid);
	}

	/* Cancel repeat unless we may continue */
	if (!more) disturb(player);
}


/**
 * Determine if a given grid may be "closed"
 */
static bool do_cmd_close_test(struct player *p, struct loc grid)
{
	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		/* Message */
		msg("You see nothing there.");

		/* Nope */
		return (false);
	}

 	/* Require open/broken door */
	if (!square_isopendoor(cave, grid) && !square_isbrokendoor(cave, grid)) {
		/* Message */
		msg("You see nothing there to close.");
		if (square_isopendoor(p->cave, grid)
				|| square_isbrokendoor(p->cave, grid)) {
			square_forget_feats(p, grid);
			square_light_spot(cave, grid);
		}

		/* Nope */
		return (false);
	}

	/* Don't allow if player is in the way. */
	if (square(cave, grid)->mon < 0) {
		/* Message */
		msg("You're standing in that doorway.");

		/* Nope */
		return (false);
	}

	/* Okay */
	return (true);
}


/**
 * Perform the basic "close" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_close_aux(struct loc grid)
{
	bool more = false;

	/* Verify legality */
	if (!do_cmd_close_test(player, grid)) return (false);

	/* Broken door */
	if (square_isbrokendoor(cave, grid)) {
		msg("The door appears to be broken.");
	} else {
		/* Close door */
		square_close_door(cave, grid);
		square_memorize_feats(player, cave, grid);
		square_light_spot(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		sound(MSG_SHUTDOOR);
	}

	/* Result */
	return (more);
}


/**
 * Close an open door.
 */
void do_cmd_close(struct command *cmd)
{
	struct loc grid;
	int dir;
	int err;

	bool more = false;

	/* Get arguments */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;

		/* Count open doors */
		if (count_feats(&grid1, square_isopendoor, false) == 1) {
			dir = motion_dir(player->mon.grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, false)) {
			return;
		}
	}

	/* Get location */
	grid = loc_sum(player->mon.grid, ddgrid[dir]);

	/* Verify legality */
	if (!do_cmd_close_test(player, grid)) {
		/* Cancel repeat */
		disturb(player);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->mon.grid, ddgrid[dir]);
	}

	/* Monster - alert, then attack */
	if (square(cave, grid)->mon > 0) {
		msg("There is a monster in the way!");
		py_attack(player, grid);
	} else {
		/* Door - close it */
		more = do_cmd_close_aux(grid);
	}

	/* Cancel repeat unless told not to */
	if (!more) disturb(player);
}


/**
 * Determine if a given grid may be "tunneled"
 */
static bool do_cmd_tunnel_test(struct player *p, struct loc grid)
{

	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		msg("You see nothing there.");
		return (false);
	}

	/* Titanium */
	if (square_isperm(cave, grid)) {
		msg("This seems to be permanent rock.");
		if (!square_isperm(p->cave, grid)) {
			square_memorize_feats(p, cave, grid);
			square_light_spot(cave, grid);
		}
		return (false);
	}

	/* Must be a wall/door/etc */
	if (!(square_isdiggable(cave, grid) || square_iscloseddoor(cave, grid))) {
		msg("You see nothing there to tunnel.");
		if (square_isdiggable(p->cave, grid)
				|| square_iscloseddoor(p->cave, grid)) {
			square_forget_feats(p, grid);
			square_light_spot(cave, grid);
		}
		return (false);
	}

	/* Okay */
	return (true);
}


/**
 * Tunnel through wall.  Assumes valid location.
 *
 * Note that it is impossible to "extend" rooms past their
 * outer walls (which are actually part of the room).
 *
 * Attempting to do so will produce floor grids which are not part
 * of the room, and whose "illumination" status do not change with
 * the rest of the room.
 */
static bool twall(struct loc grid, int feat)
{
	/* Paranoia -- Require a wall or door or some such */
	if (!(square_isdiggable(cave, grid) || square_iscloseddoor(cave, grid))) {
		return (false);
	}

	/* Sound */
	sound(MSG_DIG);

	/* Forget the wall */
	square_forget_feats(player, grid);

	// remove that feat
	square_remove_feat(cave, grid, feat);

	/* Update the visuals */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Result */
	return (true);
}


/**
 * Perform the basic "tunnel" command
 *
 * Assumes that no monster is blocking the destination.
 * Uses twall() (above) to do all "terrain feature changing".
 * Returns true if repeated commands may continue.
 */
static bool do_cmd_tunnel_aux(struct loc grid)
{
	bool more = false;
	int digging_chances[DIGGING_MAX], chance;
	bool okay = false;
	bool digger_swapped = false;
	int weapon_slot = slot_by_type(&player->mon, EQUIP_WEAPON, true);
	struct object *current_weapon = slot_object(&player->mon, weapon_slot);
	struct object *best_digger = NULL;
	struct player_state local_state;
	struct player_state *used_state = &player->mon.state;
	int oldn = 1, dig_idx;
	const char *with_clause = current_weapon == NULL ? "with your hands" : "with your weapon";
	struct feature *feat;

	for (feat = square_feat(cave, grid); feat; feat = feat->next) {
		if (feat_is_diggable(feat->kind->fidx)) break;
	}

	if (!feat) return false;

	/* Verify legality */
	if (!do_cmd_tunnel_test(player, grid)) return (false);

	/* Find what we're digging with and our chance of success */
	best_digger = player_best_digger(player, false);
	if (weapon_slot >= 0 && best_digger != current_weapon &&
			(!current_weapon || obj_can_takeoff(current_weapon))) {
		digger_swapped = true;
		with_clause = "with your swap digger";
		/* Use only one without the overhead of gear_obj_for_use(). */
		if (best_digger) {
			oldn = best_digger->number;
			best_digger->number = 1;
		}
		player->mon.body.slots[weapon_slot].obj = best_digger;
		memcpy(&local_state, &player->mon.state, sizeof(local_state));
		calc_bonuses(player, &player->mon, &local_state, false, true);
		used_state = &local_state;
	}
	calc_digging_chances(used_state, digging_chances);

	/* Do we succeed? */
	dig_idx = feat->kind->dig;
	//dig_idx = square_digging(cave, grid);
	if (dig_idx < 1 || dig_idx > DIGGING_MAX) {
		msg("%s has misconfigured digging chance; please report this bug.",
			(feat->kind->name) ?
			feat->kind->name :
			format("Terrain index %d", feat->kind->fidx));
		dig_idx = DIGGING_GRANITE + 1;
	}
	chance = digging_chances[dig_idx - 1];
	okay = (chance > randint0(1600));

	/* Swap back */
	if (digger_swapped) {
		if (best_digger) {
			best_digger->number = oldn;
		}
		player->mon.body.slots[weapon_slot].obj = current_weapon;
		calc_bonuses(player, &player->mon, &local_state, false, true);
	}

	/* Success */
	if (okay && twall(grid, feat->kind->fidx)) {
		/* Rubble is a special case - could be handled more generally NRM */

		if (feat_is_rubble(feat->kind->fidx)) {
			/* Message */
			msg("You have removed the rubble %s.", with_clause);

			/* Place an object (except in town) */
			if ((randint0(100) < 10) && player->depth) {
				/* Create a simple object */
				place_object(cave, grid, player->depth, false, false,
							 ORIGIN_RUBBLE, 0);

				/* Observe the new object */
				if (square_object(cave, grid)
						&& !ignore_item_ok(player,
						square_object(cave, grid))
						&& square_isseen(cave, grid)) {
					msg("You have found something!");
				}
			}
		} else if (feat_is_treasure(feat->kind->fidx)) {
			/* Found treasure */
			place_gold(cave, grid, player->depth, ORIGIN_FLOOR);
			msg("You have found something digging %s!", with_clause);
		} else {
			msg("You have finished the tunnel %s.", with_clause);
		}
		/* On the surface, new terrain may be exposed to the sun. */
		if (cave->depth == 0) {
			expose_to_sun(cave, grid, is_daytime());
		}
		/* Update the visuals. */
		square_memorize_feats(player, cave, grid);
		square_light_spot(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	} else if (chance > 0) {
		/* Failure, continue digging */
		if (feat_is_rubble(feat->kind->fidx)) {
			msg("You dig in the rubble %s.", with_clause);
		} else {
			msg("You tunnel into the %s %s.",
				feat->kind->name,
				with_clause);
		}
		more = true;
	} else {
		/* Don't automatically repeat if there's no hope. */
		if (feat_is_rubble(feat->kind->fidx)) {
			msg("You dig in the rubble %s with little effect.", with_clause);
		} else {
			msg("You chip away futilely %s at the %s.", with_clause,
				square_apparent_name(player->cave, grid));
		}
	}

	/* Result */
	return (more);
}


/**
 * Tunnel through "walls" (including rubble and doors, secret or otherwise)
 *
 * Digging is very difficult without a "digger" weapon, but can be
 * accomplished by strong players using heavy weapons.
 */
void do_cmd_tunnel(struct command *cmd)
{
	struct loc grid;
	int dir;
	bool more = false;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false)) {
		return;
	}

	/* Get location */
	grid = loc_sum(player->mon.grid, ddgrid[dir]);

	/* Oops */
	if (!do_cmd_tunnel_test(player, grid)) {
		/* Cancel repeat */
		disturb(player);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->mon.grid, ddgrid[dir]);
	}

	/* Attack any monster we run into */
	if (square(cave, grid)->mon > 0) {
		msg("There is a monster in the way!");
		py_attack(player, grid);
	} else {
		/* Tunnel through walls */
		more = do_cmd_tunnel_aux(grid);
	}

	/* Cancel repetition unless we can continue */
	if (!more) disturb(player);
}

/**
 * Determine if a given grid may be "disarmed"
 */
static bool do_cmd_disarm_test(struct player *p, struct loc grid)
{
	/* Must have knowledge */
	if (!square_isknown(cave, grid)) {
		msg("You see nothing there.");
		return false;
	}

	/* Look for a closed, unlocked door to lock */
	if (square_iscloseddoor(cave, grid) && !square_islockeddoor(cave, grid))
		return true;

	/* Look for a trap */
	if (!square_isdisarmabletrap(cave, grid)) {
		msg("You see nothing there to disarm.");
		if (square_isdisarmabletrap(p->cave, grid)) {
			square_memorize_traps(cave, grid);
			square_light_spot(cave, grid);
		}
		return false;
	}

	/* Okay */
	return true;
}


/**
 * Perform the command "lock door"
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_lock_door(struct loc grid)
{
	int i, j, power;
	bool more = false;

	/* Verify legality */
	if (!do_cmd_disarm_test(player, grid)) return false;

	/* Get the "disarm" factor */
	i = player->mon.state.skills[SKILL_DISARM_PHYS];

	/* Penalize some conditions */
	if (player->mon.m_timed[TMD_BLIND] || no_light(player))
		i = i / 10;
	if (player->mon.m_timed[TMD_CONFUSED] || player->mon.m_timed[TMD_IMAGE])
		i = i / 10;

	/* Calculate lock "power" */
	power = m_bonus(7, player->depth);

	/* Extract the difficulty */
	j = i - power;

	/* Always have a small chance of success */
	if (j < 2) j = 2;

	/* Success */
	if (randint0(100) < j) {
		msg("You lock the door.");
		square_set_door_lock(cave, grid, power);
	}

	/* Failure -- Keep trying */
	else if ((i > 5) && (randint1(i) > 5)) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to lock the door.");

		/* We may keep trying */
		more = true;
	}
	/* Failure */
	else
		msg("You failed to lock the door.");

	/* Result */
	return more;
}


/**
 * Perform the basic "disarm" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_disarm_aux(struct loc grid)
{
	int skill, power, chance;
    struct trap *trap = square(cave, grid)->trap;
	bool more = false;

	/* Verify legality */
	if (!do_cmd_disarm_test(player, grid)) return (false);

    /* Choose first player trap */
	while (trap) {
		if (trf_has(trap->flags, TRF_TRAP))
			break;
		trap = trap->next;
	}
	if (!trap)
		return false;

	/* Get the base disarming skill */
	if (trf_has(trap->flags, TRF_MAGICAL))
		skill = player->mon.state.skills[SKILL_DISARM_MAGIC];
	else
		skill = player->mon.state.skills[SKILL_DISARM_PHYS];

	/* Penalize some conditions */
	if (player->mon.m_timed[TMD_BLIND] ||
			no_light(player) ||
			player->mon.m_timed[TMD_CONFUSED] ||
			player->mon.m_timed[TMD_IMAGE])
		skill = skill / 10;

	/* Extract trap power */
	power = cave->depth / 5;

	/* Extract the percentage success */
	chance = skill - power;

	/* Always have a small chance of success */
	if (chance < 2) chance = 2;

	/* Two chances - one to disarm, one not to set the trap off */
	if (randint0(100) < chance) {
		msgt(MSG_DISARM, "You have disarmed the %s.", trap->kind->name);
		player_exp_gain(player, 1 + power, 0);

		/* Trap is gone */
		if (!square_remove_trap(cave, grid, trap, true)) {
			assert(0);
		}
	} else if (randint0(100) < chance) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to disarm the %s.", trap->kind->name);

		/* Player can try again */
		more = true;
	} else {
		msg("You set off the %s!", trap->kind->name);
		hit_trap(grid, -1);
	}

	/* Result */
	return (more);
}


/**
 * Disarms a trap, or a chest
 *
 * Traps must be visible, chests must be known trapped
 */
void do_cmd_disarm(struct command *cmd)
{
	struct loc grid;
	int dir;
	int err;

	struct object *obj;
	bool more = false;
	struct monster *mon;

	/* Get arguments */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;
		int n_traps, n_chests, n_unldoor;

		n_traps = count_feats(&grid1, square_isdisarmabletrap, false);
		n_chests = count_chests(&grid1, CHEST_TRAPPED);
		n_unldoor = count_feats(&grid1, square_isunlockeddoor, false);

		if (n_traps + n_chests + n_unldoor == 1) {
			dir = motion_dir(player->mon.grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, n_chests > 0)) {
			/* If there are chests to disarm, 5 is allowed as a direction */
			return;
		}
	}

	/* Get location */
	grid = loc_sum(player->mon.grid, ddgrid[dir]);

	/* Check for chests */
	obj = chest_check(player, grid, CHEST_TRAPPED);

	/* Verify legality */
	if (!obj && !do_cmd_disarm_test(player, grid)) {
		/* Cancel repeat */
		disturb(player);
		return;
	}

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->mon.grid, ddgrid[dir]);

		/* Check for chests */
		obj = chest_check(player, grid, CHEST_TRAPPED);
	}


	/* Monster */
	mon = square_monster(cave, grid);
	if (mon) {
		if (monster_is_camouflaged(mon)) {
			become_aware(cave, mon);

			monster_wake(mon, false, 100);
		} else {
			msg("There is a monster in the way!");
			py_attack(player, grid);
		}
	} else if (obj)
		/* Chest */
		more = do_cmd_disarm_chest(obj);
	else if (square_iscloseddoor(cave, grid) &&
			 !square_islockeddoor(cave, grid))
		/* Door to lock */
		more = do_cmd_lock_door(grid);
	else
		/* Disarm trap */
		more = do_cmd_disarm_aux(grid);

	/* Cancel repeat unless told not to */
	if (!more) disturb(player);
}

/**
 * Manipulate an adjacent grid in some way
 *
 * Attack monsters, tunnel through walls, disarm traps, open doors.
 *
 * This command must always take energy, to prevent free detection
 * of invisible monsters.
 *
 * The "semantics" of this command must be chosen before the player
 * is confused, and it must be verified against the new grid.
 */
static void do_cmd_alter_aux(int dir)
{
	struct loc grid;
	bool more = false;
	struct object *o_chest_closed;
	struct object *o_chest_trapped;

	/* Get location */
	grid = loc_sum(player->mon.grid, ddgrid[dir]);

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->mon.grid, ddgrid[dir]);
	}

	/* Check for closed chest */
	o_chest_closed = chest_check(player, grid, CHEST_OPENABLE);
	/* Check for trapped chest */
	o_chest_trapped = chest_check(player, grid, CHEST_TRAPPED);

	/* Action depends on what's there */
	if (square(cave, grid)->mon > 0) {
		/* Attack monster */
		py_attack(player, grid);
	} else if (square_isdiggable(player->cave, grid)) {
		/* Tunnel through walls and rubble */
		more = do_cmd_tunnel_aux(grid);
	} else if (square_iscloseddoor(player->cave, grid)) {
		/* Open closed doors */
		more = do_cmd_open_aux(grid);
	} else if (square_isdisarmabletrap(player->cave, grid)) {
		/* Disarm traps */
		more = do_cmd_disarm_aux(grid);
	} else if (o_chest_trapped) {
        	/* Trapped chest */
        	more = do_cmd_disarm_chest(o_chest_trapped);
    	} else if (o_chest_closed) {
        	/* Open chest */
        	more = do_cmd_open_chest(grid, o_chest_closed);
	} else if (square_isopendoor(player->cave, grid)) {
		/* Close door */
        	more = do_cmd_close_aux(grid);
	} else {
		/* Oops */
		msg("You spin around.");
	}

	/* Cancel repetition unless we can continue */
	if (!more) disturb(player);
}

void do_cmd_alter(struct command *cmd)
{
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	do_cmd_alter_aux(dir);
}

static void do_cmd_steal_aux(int dir)
{
	/* Get location */
	struct loc grid = loc_sum(player->mon.grid, ddgrid[dir]);

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Apply confusion */
	if (player_confuse_dir(player, &dir, false)) {
		/* Get location */
		grid = loc_sum(player->mon.grid, ddgrid[dir]);
	}

	/* Attack or steal from monsters */
	if ((square(cave, grid)->mon > 0) && player_has(player, PF_STEAL)) {
		steal_monster_item(square_monster(cave, grid), -1);
	} else {
		/* Oops */
		msg("You spin around.");
	}
}

void do_cmd_steal(struct command *cmd)
{
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	do_cmd_steal_aux(dir);
}

/**
 * Move player in the given direction.
 *
 * This routine should only be called when energy has been expended.
 *
 * Note that this routine handles monsters in the destination grid,
 * and also handles attempting to move into walls/doors/rubble/etc.
 */
void move_player(int dir, bool disarm)
{
	struct loc grid = loc_sum(player->mon.grid, ddgrid[dir]);

	int m_idx = square(cave, grid)->mon;
	struct monster *mon = cave_monster(cave, m_idx);
	bool trapsafe = player_is_trapsafe(player);
	bool trap = square_isdisarmabletrap(cave, grid);
	bool door = square_iscloseddoor(cave, grid);
	bool step = false;

	/* Many things can happen on movement */
	if (m_idx > 0 && mon_will_attack_player(mon, player)) {
		if (monster_is_camouflaged(mon)) {
			become_aware(cave, mon);

			/* Camouflaged monster wakes up and becomes aware */
			monster_wake(mon, false, 100);
		} else {
			py_attack(player, grid);
		}
	} else if (((trap && disarm) || door) && square_isknown(cave, grid)) {
		/* Auto-repeat if not already repeating */
		if (cmd_get_nrepeats() == 0)
			cmd_set_repeat(99);
		do_cmd_alter_aux(dir);
	} else if (trap && player->upkeep->running && !trapsafe) {
		/* Stop running before known traps */
		disturb(player);
		/* No move made so no energy spent. */
		player->upkeep->energy_use = 0;
	} else if (!square_ispassable(cave, grid) &&
			(!pf_has(player->mon.state.pflags, PF_PASS_WALL) || square_isperm(cave, grid))) {

		//int prev_feat_k = square(player->cave, grid)->feat->kind->fidx;
		const char *issue, *prefix, *article;
		struct feature *feat;
		int prev_k, new_k;
		
		feat = first_feat_not_meets_pred(player->cave, grid, feat_is_passable);
		prev_k = feat ? feat->kind->fidx : FEAT_NONE;

		disturb(player);

		square_memorize_feats(player, cave, grid);

		feat = first_feat_not_meets_pred(player->cave, grid, feat_is_passable);
		new_k = feat ? feat->kind->fidx : FEAT_NONE;
		
		issue = square_impassable_name(player->cave, grid);
		if (issue) {
			article = is_a_vowel(issue[0]) ? "an " : "a ";
		} else {
			issue = "something";
			article = "";
		}

		if (new_k == FEAT_NONE || new_k != prev_k) prefix = "There is";
		else prefix = "You feel";

		msgt(MSG_HITWALL, "%s %s%s blocking your way.", prefix, article, issue);

		/* Notice unknown obstacles, mention known obstacles */
		/*if (!square_isknown(cave, grid)) {
			square_memorize(cave, grid);
			issue = square_impassable_name(player->cave, grid);

			if (issue) msgt(MSG_HITWALL, "You feel a %s blocking your way.", issue);
			else msgt(MSG_HITWALL, "You feel something blocking your way.");

			if (square_isrubble(cave, grid)) {
				msgt(MSG_HITWALL,
					 "You feel a pile of rubble blocking your way.");
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			} else if (square_iscloseddoor(cave, grid)) {
				msgt(MSG_HITWALL, "You feel a door blocking your way.");
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			} else {
				msgt(MSG_HITWALL, "You feel a wall blocking your way.");
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			}
		} else {
			issue = square_impassable_name(player->cave, grid);

			if (square_isrubble(cave, grid)) {
				msgt(MSG_HITWALL,
					 "There is a pile of rubble blocking your way.");
				if (!square_isrubble(player->cave, grid)) {
					square_memorize(cave, grid);
					square_light_spot(cave, grid);
				}
			} else if (square_iscloseddoor(cave, grid)) {
				msgt(MSG_HITWALL, "There is a door blocking your way.");
				if (!square_iscloseddoor(player->cave, grid)) {
					square_memorize(cave, grid);
					square_light_spot(cave, grid);
				}
			} else {
				msgt(MSG_HITWALL, "There is a wall blocking your way.");
				if (square_ispassable(player->cave, grid)
						|| square_isrubble(player->cave, grid)
						|| square_iscloseddoor(player->cave, grid)) {
					square_forget(cave, grid);
					square_light_spot(cave, grid);
				}
			}
		}*/
		/*
		 * No move but do not refund energy:  primarily so that
		 * confused moves while blind or without light take energy.
		 */
	} else {
		/* See if trap detection status will change */
		bool old_dtrap = square_isdtrap(cave, player->mon.grid);
		bool new_dtrap = square_isdtrap(cave, grid);
		struct feature *feat;
		char dam_name[256] = { '\0' };

		step = true;

		/* Note the change in the detect status */
		if (old_dtrap != new_dtrap)
			player->upkeep->redraw |= (PR_DTRAP);

		/* Disturb player if the player is about to leave the area */
		if (player->upkeep->running
				&& !player->upkeep->running_firststep
				&& old_dtrap && !new_dtrap) {
			disturb(player);
			/* No move made so no energy spent. */
			player->upkeep->energy_use = 0;
			return;
		}

		/*
		 * If not confused, allow check before moving into damaging
		 * terrain.
		 */
		for (feat = square_feat(cave, grid); feat; feat = feat->next) {
			if (feat_is_damaging(feat->kind->fidx)) {
				if (!dam_name[0]) {
					my_strcat(dam_name, " and ", sizeof dam_name);
				}
				my_strcat(dam_name, feat->kind->name, sizeof dam_name);
			}
		}

		for (feat = square_feat(cave, grid); feat; feat = feat->next) {
			if (feat_is_damaging(feat->kind->fidx)
					&& !player->mon.m_timed[TMD_CONFUSED]) {
				
				int dam_taken = player_check_terrain_damage(player,
					grid, false);

				/*
				 * Check if running, or going to cost more than a
				 * third of hp.
				 */
				if (player->upkeep->running && dam_taken) {
					if (!get_check(feat->kind->run_msg)) {
						player->upkeep->running = 0;
						step = false;
					}
				} else {
					if (dam_taken > player->mon.hp / 3) {
						step = get_check(feat->kind->walk_msg);
					}
				}
			}
		}

		if (!step) player->upkeep->energy_use = 0;
	}

	if (step) {
		if (mon) {
			char mdesc[80];
			monster_desc(mdesc, sizeof mdesc, mon, MDESC_TARG);
			msg("You push past %s.", mdesc);
		}
		
		/* Move player */
		monster_swap(player->mon.grid, grid);
		player_handle_post_move(player, true, false);
		cmdq_push(CMD_AUTOPICKUP);
		/*
		 * The autopickup is a side effect of the move:
		 * whatever command triggered the move will be the
		 * target for CMD_REPEAT rather than repeating the
		 * autopickup, and the autopickup won't trigger
		 * bloodlust.
		 */
		cmdq_peek()->background_command = 1;
	}

	player->upkeep->running_firststep = false;
}

/**
 * Determine if a given grid may be "walked"
 */
static bool do_cmd_walk_test(struct player *p, struct loc grid)
{
	int m_idx = square(cave, grid)->mon;
	struct monster *mon = cave_monster(cave, m_idx);

	/* Allow attack on obvious monsters if unafraid */
	if (m_idx > 0 && monster_is_obvious(mon)) {
		/* Handle player fear */
		if (player_of_has(p, OF_AFRAID)) {
			/* Extract monster name (or "it") */
			char m_name[80];
			monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

			/* Message */
			msgt(MSG_AFRAID, "You are too afraid to attack %s!", m_name);
			equip_learn_flag(p, OF_AFRAID);

			/* Nope */
			return (false);
		}

		return (true);
	}

	/* If we don't know the grid, allow attempts to walk into it */
	if (!square_isknown(cave, grid)) {
		return true;
	}

	/*
	 * Require open space; if the messaging indicates what is there and
	 * that does not agree with the player's memory then update the
	 * player's memory
	 */
	if (!square_ispassable(cave, grid) && (!pf_has(p->mon.state.pflags, PF_PASS_WALL) || square_isperm(cave, grid))) {
		const char *imp_name, *article;

		if (square_iscloseddoor(p->cave, grid)) {
			return true;
		}

		square_memorize_feats(p, cave, grid);

		imp_name = square_impassable_name(p->cave, grid);
		if (imp_name) {
			article = is_a_vowel(imp_name[0]) ? "an" : "a";
			msgt(MSG_HITWALL, "There is %s %s in the way!", article, imp_name);
		} else {
			msgt(MSG_HITWALL, "There is something in the way!");
		}

		/*if (square_isrubble(cave, grid)) {
			// Rubble
			msgt(MSG_HITWALL, "There is a pile of rubble in the way!");
			if (!square_isrubble(p->cave, grid)) {
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			}
		} else if (square_iscloseddoor(cave, grid)) {
			// Door
			return true;
		} else {
			// Wall
			msgt(MSG_HITWALL, "There is a wall in the way!");
			if (square_ispassable(p->cave, grid)
					|| square_isrubble(p->cave, grid)
					|| square_iscloseddoor(p->cave, grid)) {
				square_forget(cave, grid);
				square_light_spot(cave, grid);
			}
		}*/

		/* Cancel repeat */
		disturb(p);

		/* Nope */
		return (false);
	}

	/* Okay */
	return (true);
}


/**
 * Walk in the given direction.
 */
void do_cmd_walk(struct command *cmd)
{
	struct loc grid;
	int dir;
	bool trapsafe = player_is_trapsafe(player) ? true : false;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK) {
		return;
	}

	/* If we're in a web, deal with that */
	if (monster_turn_web(cave, &player->mon)) return;

	/* Apply confusion if necessary */
	/* Confused movements use energy no matter what */
	if (player_confuse_dir(player, &dir, false)) {
		player->upkeep->energy_use = z_info->move_energy;
	}
	
	/* Verify walkability */
	grid = loc_sum(player->mon.grid, ddgrid[dir]);
	if (!do_cmd_walk_test(player, grid)) {
		return;
	}

	player->upkeep->energy_use = energy_per_move(player);

	/* Attempt to disarm unless it's a trap and we're trapsafe */
	move_player(dir, !(square_isdisarmabletrap(cave, grid) && trapsafe));
}


/**
 * Walk into a trap.
 */
void do_cmd_jump(struct command *cmd)
{
	struct loc grid;
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* If we're in a web, deal with that */
	if (monster_turn_web(cave, &player->mon)) return;

	/* Apply confusion if necessary */
	if (player_confuse_dir(player, &dir, false))
		player->upkeep->energy_use = z_info->move_energy;

	/* Verify walkability */
	grid = loc_sum(player->mon.grid, ddgrid[dir]);
	if (!do_cmd_walk_test(player, grid))
		return;

	player->upkeep->energy_use = energy_per_move(player);

	move_player(dir, false);
}

/**
 * Start running.
 *
 * Note that running while confused is not allowed.
 */
void do_cmd_run(struct command *cmd)
{
	struct loc grid;
	int dir;

	/* Get arguments */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* If we're in a web, deal with that */
	if (monster_turn_web(cave, &player->mon)) return;

	if (player_confuse_dir(player, &dir, true))
		return;

	/* Get location */
	if (dir) {
		grid = loc_sum(player->mon.grid, ddgrid[dir]);
		if (!do_cmd_walk_test(player, grid))
			return;
			
		/* Hack: convert repeat count to running count */
		if (cmd->nrepeats > 0) {
			player->upkeep->running = cmd->nrepeats;
			cmd->nrepeats = 0;
		}
		else {
			player->upkeep->running = 0;
		}
	}

	/* Start run */
	run_step(dir);
}

/**
 * Automatically navigate to the nearest downstairs location.
 *
 * Note that navigating while confused is not allowed.
 */
void do_cmd_navigate_down(struct command *cmd)
{
	int visible_monster_count = 0;

	/* cancel if confused */
	if (player->mon.m_timed[TMD_CONFUSED]) {
		msg("You cannot explore while confused.");
	   	return;
	}

	/* If we're in a web, deal with that */
	if (monster_turn_web(cave, &player->mon)) return;

	/* Screen for visible monsters */
	for (int y = 0; y < cave->height; y++) {
		for (int x = 0; x < cave->width; x++) {
			struct loc grid = loc(x, y);
			
			if (loc_eq(grid, player->mon.grid)) continue;

			if (square_isoccupied(cave, grid)) {
				int m_idx = square(cave, grid)->mon;
				struct monster *mon = cave_monster(cave, m_idx);
				if (monster_is_obvious(mon) && mon_will_attack_player(mon, player)) {
					visible_monster_count++;
					break;
				}
			}
		}
	}

	if (visible_monster_count > 0) {
		msg("Something is here.");
		return;
	}

	assert(!player->upkeep->steps);
	player->upkeep->step_count = path_nearest_known(player, player->mon.grid,
		square_isdownstairs, &player->upkeep->path_dest,
		&player->upkeep->steps);
	if (player->upkeep->step_count > 0) {
		player->upkeep->running = player->upkeep->step_count;
		/* Calculate torch radius */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
		return;
	}

	msg("No known path to downstairs.");
}

/**
 * Automatically navigate to the nearest upstairs location.
 *
 * Note that navigating while confused is not allowed.
 */
void do_cmd_navigate_up(struct command *cmd)
{
	int visible_monster_count = 0;
	/* cancel if confused */
	if (player->mon.m_timed[TMD_CONFUSED]) {
		msg("You cannot explore while confused.");
	   	return;
	}


	/* If we're in a web, deal with that */
	if (monster_turn_web(cave, &player->mon)) return;

	/* Screen for visible monsters */
	for (int y = 0; y < cave->height; y++) {
		for (int x = 0; x < cave->width; x++) {
			struct loc grid = loc(x, y);

			if (loc_eq(grid, player->mon.grid)) continue;

			if (square_isoccupied(cave, grid)) {
				int m_idx = square(cave, grid)->mon;
				struct monster *mon = cave_monster(cave, m_idx);
				if (monster_is_obvious(mon) && mon_will_attack_player(mon, player)) {
					visible_monster_count++;
					break;
				}
			}
		}
	}

	if (visible_monster_count > 0) {
		msg("Something is here.");
		return;
	}

	assert(!player->upkeep->steps);
	player->upkeep->step_count = path_nearest_known(player, player->mon.grid,
		square_isupstairs, &player->upkeep->path_dest,
		&player->upkeep->steps);
	if (player->upkeep->step_count > 0) {
		player->upkeep->running = player->upkeep->step_count;
		/* Calculate torch radius */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
		return;
	}

	msg("No known path to upstairs.");
}

/**
 * Start exploring.
 *
 * Note that exploring while confused is not allowed.
 */
void do_cmd_explore(struct command *cmd)
{
	bool visible_monster = false;
	int y, x;
	/* cancel if confused */
	if (player->mon.m_timed[TMD_CONFUSED]) {
		cmd_cancel_repeat();
		msg("You cannot explore while confused.");
	   	return;
	}

	/* If we're in a web, deal with that */
	if (monster_turn_web(cave, &player->mon)) return;

	/* Screen for visible monsters */
	for (y = 0; y < cave->height && !visible_monster; y++) {
		for (x = 0; x < cave->width && !visible_monster; x++) {
			struct loc grid = loc(x, y);
			
			if (loc_eq(grid, player->mon.grid)) continue;

			if (square_isoccupied(cave, grid)) {
				int m_idx = square(cave, grid)->mon;
				struct monster *mon = cave_monster(cave, m_idx);
				if (monster_is_obvious(mon) && mon_will_attack_player(mon, player)) {
					visible_monster = true;
				}
			}
		}
	}

	if (visible_monster) {
		cmd_cancel_repeat();
		msg("Something is here.");
		return;
	}

	assert(!player->upkeep->steps);

	player->upkeep->step_count = path_nearest_known(player, player->mon.grid,
			square_hasunknownitem, &player->upkeep->path_dest, &player->upkeep->steps);
			
	if (player->upkeep->step_count > 0) {
		player->upkeep->running = player->upkeep->step_count;
		/* Calculate torch radius */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
		return;
	}

	player->upkeep->step_count = path_nearest_unknown(player, player->mon.grid,
		&player->upkeep->path_dest, &player->upkeep->steps);

	if (count_neighbors(NULL, cave, player->mon.grid, square_isknown, true) < 9) {
		// don't keep exploring if we're not getting new knowledge
		cmd_cancel_repeat();
	}

	if (count_neighbors(NULL, cave, player->upkeep->path_dest, square_isknown, true) >= 9) {
		// don't keep exploring if we're not going anywhere new
		cmd_cancel_repeat();
	}

	if (player->upkeep->step_count > 0) {
		player->upkeep->running = player->upkeep->step_count;
		/* Calculate torch radius */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
		return;
	}

	cmd_cancel_repeat();
	msg("No apparent path for exploration.");
}


/**
 * Start running with pathfinder.
 *
 * Note that running while confused is not allowed.
 */
void do_cmd_pathfind(struct command *cmd)
{
	struct loc grid;

	/* XXX-AS Add better arg checking */
	cmd_get_arg_point(cmd, "point", &grid);

	if (player->mon.m_timed[TMD_CONFUSED])
		return;

	assert(!player->upkeep->steps);
	player->upkeep->step_count =
		find_path(player, player->mon.grid, grid, &player->upkeep->steps);
	if (player->upkeep->step_count > 0) {
		player->upkeep->path_dest = grid;
		player->upkeep->running = player->upkeep->step_count;
		/* Calculate torch radius */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
	}
}



/**
 * Stay still.  Search.  Enter stores.
 * Pick up treasure if "pickup" is true.
 */
void do_cmd_hold(struct command *cmd)
{
	if (autocast(player)) return;

	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Searching (probably not necessary - NRM)*/
	// L: now necessary
	// L: now no longer necessary again
	//search(player);

	/* Pick things up, not using extra energy */
	do_autopickup(player);

	/* Enter a store if we are on one, otherwise look at the floor */
	if (square_isshop(cave, player->mon.grid)) {
		if (player_is_shapechanged(player)) {
			if (square_shopnum(cave, player->mon.grid) != f_info[FEAT_HOME].shopnum) {
				msg("There is a scream and the door slams shut!");
			}
			return;
		}
		disturb(player);
		event_signal(EVENT_ENTER_STORE);
		event_remove_handler_type(EVENT_ENTER_STORE);
		event_signal(EVENT_USE_STORE);
		event_remove_handler_type(EVENT_USE_STORE);
		event_signal(EVENT_LEAVE_STORE);
		event_remove_handler_type(EVENT_LEAVE_STORE);

		/* Turn will be taken exiting the shop */
		player->upkeep->energy_use = 0;
	} else {
		event_signal(EVENT_SEEFLOOR);
		square_know_pile(cave, player->mon.grid, object_not_in_container_predicate);
	}
}


/**
 * Rest (restores hit points and mana and such)
 */
void do_cmd_rest(struct command *cmd)
{
	int n;

	/* XXX-AS need to insert UI here */
	if (cmd_get_arg_choice(cmd, "choice", &n) != CMD_OK) {
		return;
	}
	/* 
	 * A little sanity checking on the input - only the specified negative 
	 * values are valid. 
	 */
	if (n < 0 && !player_resting_is_special(n)) {
		return;
	}

	/* Do some upkeep on the first turn of rest */
	if (!player_is_resting(player)) {
		player->upkeep->update |= (PU_BONUS);

		/* If a number of turns was entered, remember it */
		if (n > 1) {
			player_set_resting_repeat_count(player, n);
		} else if (n == 1) {
			/* If we're repeating the command, use the same count */
			n = player_get_resting_repeat_count(player);
		}
	}

	/* Set the counter, and stop if told to */
	player_resting_set_count(player, n);
	if (!player_is_resting(player)) {
		return;
	}

	/* Take a turn */
	player_resting_step_turn(player);

	/* Redraw the state if requested */
	handle_stuff(player);

	/* Prepare to continue, or cancel and clean up */
	if (player_resting_count(player) > 0) {
		cmdq_push(CMD_REST);
		cmd_set_arg_choice(cmdq_peek(), "choice", n - 1);
	} else if (player_resting_is_special(n)) {
		cmdq_push(CMD_REST);
		cmd_set_arg_choice(cmdq_peek(), "choice", n);
		player_set_resting_repeat_count(player, 0);
	} else {
		player_resting_cancel(player, false);
	}
}


/**
 * Spend a turn doing nothing
 */
void do_cmd_sleep(struct command *cmd)
{
	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;
}


/**
 * Array of feeling strings for object feelings.
 * Keep strings at 36 or less characters to keep the
 * combined feeling on one row.
 */
static const char *obj_feeling_text[] =
{
	"Looks like any other level.",
	"you sense an item of wondrous power!",
	"there are superb treasures here.",
	"there are excellent treasures here.",
	"there are very good treasures here.",
	"there are good treasures here.",
	"there may be something worthwhile here.",
	"there may not be much interesting here.",
	"there aren't many treasures here.",
	"there are only scraps of junk here.",
	"there is naught but cobwebs here."
};

/**
 * Array of feeling strings for monster feelings.
 * Keep strings at 36 or less characters to keep the
 * combined feeling on one row.
 */
static const char *mon_feeling_text[] =
{
	/* first string is just a place holder to 
	 * maintain symmetry with obj_feeling.
	 */
	"You are still uncertain about this place",
	"Omens of death haunt this place",
	"This place seems murderous",
	"This place seems terribly dangerous",
	"You feel anxious about this place",
	"You feel nervous about this place",
	"This place does not seem too risky",
	"This place seems reasonably safe",
	"This seems a tame, sheltered place",
	"This seems a quiet, peaceful place"
};


/**
 * L: Array of feeling strings for mana feeling
 */
static const char *mana_feeling_text[] =
{
    "There is no mana left here.",
	"This place has very little mana.",
	"There is not much mana here.",
	"This place has limited amounts of mana.",
	"There is a bit of mana here.",
	"This place has some mana.",
	"There is a fair bit of mana here.",
	"This place has quite a bit of mana.",
	"There is a lot of mana here.",
	"Mana abounds in this place.",
	"This place overflows with mana!",
};

/**
 * Display the feeling.  Players always get a monster feeling.
 * Object feelings are delayed until the player has explored some
 * of the level.
 */
void display_feeling(bool obj_only)
{
	uint16_t obj_feeling = cave->feeling / 10;
	uint16_t mon_feeling = cave->feeling - (10 * obj_feeling);
	const char *join;

	/* Don't show feelings for cold-hearted characters */
	if (!OPT(player, birth_feelings)) return;

	/* No useful feeling in town */
	if (!player->depth) {
		msg("Looks like a typical town.");
		return;
	}

	/* Display only the object feeling when it's first discovered. */
	if (obj_only) {
		disturb(player);
     	msg("You feel that %s", obj_feeling_text[obj_feeling]);
		//display_mana_feeling();
		return;
	}

	/* Players automatically get a monster feeling. */
	if (cave->feeling_squares < z_info->feeling_need) {
		msg("%s.", mon_feeling_text[mon_feeling]);
		return;
	}

	/* Verify the feelings */
	if (obj_feeling >= N_ELEMENTS(obj_feeling_text)) {
		obj_feeling = N_ELEMENTS(obj_feeling_text) - 1;
	}

	if (mon_feeling >= N_ELEMENTS(mon_feeling_text)) {
		mon_feeling = N_ELEMENTS(mon_feeling_text) - 1;
	}

	/* Decide the conjunction */
	if ((mon_feeling <= 5 && obj_feeling > 6) ||
			(mon_feeling > 5 && obj_feeling <= 6)) {
		join = ", yet";
	} else {
		join = ", and";
	}

	/* Display the feeling */
	msg("%s%s %s", mon_feeling_text[mon_feeling], join,
		obj_feeling_text[obj_feeling]);

	/* L: display mana feeling */
	//display_mana_feeling();
}

/**
 * L: show roughly how much mana is in the current level
 */
void display_mana_feeling(void)
{
	if (!player->msp) return;
	if (cave->feeling_squares < z_info->feeling_need) return;

	uint16_t max_mana_feeling = N_ELEMENTS(mana_feeling_text) - 1;
	uint16_t mana_feeling = MIN((player->floor_mana + 14) / 15, max_mana_feeling);

	msg("%s", mana_feeling_text[mana_feeling]);
}

void display_xp_feeling(void)
{
	if (cave->depth <= 0) return;
	if (cave->depth >= player_min_xp_depth(player)) return;

	msg("You feel you can't learn any more from this depth.");
}


void do_cmd_feeling(void)
{
	display_feeling(false);
	display_xp_feeling();
}

/**
 * Make a monster perform an action.
 *
 * Currently possible actions are cast a random spell, drop a random item,
 * stand still, or move (attacking any intervening monster).
 */
void do_cmd_mon_command(struct command *cmd)
{
	struct monster *mon = get_commanded_monster();
	struct monster_lore *lore = NULL;
	char m_name[80];

	assert(mon);
	lore = get_lore(mon->race);

	/* Get the monster name */
	monster_desc(m_name, sizeof(m_name), mon,
		MDESC_CAPITAL | MDESC_IND_HID | MDESC_COMMA);

	switch (cmd->code) {
		case CMD_READ_SCROLL: {
			/* Actually 'r'elease monster */
			//mon_clear_timed(mon, MON_TMD_COMMAND, MON_TMD_FLG_NOTIFY);
			player_clear_timed(player, TMD_COMMAND, true, false);
			break;
		}
		case CMD_CAST: {
			int dir = DIR_UNKNOWN;
			struct monster *t_mon = NULL;
			bitflag f[RSF_SIZE];
			bool seen = player->mon.m_timed[TMD_BLIND] ? false : true;
			int spell_index;

			/* Choose a target monster */
			target_set_monster(NULL);
			get_aim_dir(&dir);
			t_mon = target_get_monster();
			if (!t_mon) {
				msg("No target monster selected!");
				return;
			}
			mon->target.midx = t_mon->midx;

			/* Pick a random spell and cast it */
			rsf_copy(f, mon->race->spell_flags);
			spell_index = choose_attack_spell(f, true, true);
			if (!spell_index) {
				msg("This monster has no spells!");
				return;
			}
			do_mon_spell(spell_index, mon, seen);

			/* Remember what the monster did */
			if (seen) {
				rsf_on(lore->spell_flags, spell_index);
				if (mon_spell_is_innate(spell_index)) {
					/* Innate spell */
					if (lore->cast_innate < UCHAR_MAX)
						lore->cast_innate++;
				} else {
					/* Bolt or Ball, or Special spell */
					if (lore->cast_spell < UCHAR_MAX)
						lore->cast_spell++;
				}
			}
			if (player->is_dead && (lore->deaths < SHRT_MAX)) {
				lore->deaths++;
			}
			lore_update(mon->race, lore);

			break;
		}
		case CMD_DROP: {
			char o_name[80];
			struct object *obj = get_random_monster_object(mon);
			if (!obj) break;
			obj->held_m_idx = 0;
			pile_excise(&mon->gear, obj);
			drop_near(cave, &obj, 0, mon->grid, true, false);
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_FULL, player);
			if (!ignore_item_ok(player, obj)) {
				msg("%s drops %s.", m_name, o_name);
			}

			break;
		}
		case CMD_HOLD: {
			/* Do nothing */
			break;
		}
		case CMD_WALK: {
			int dir;
			struct loc grid;
			bool can_move = false;
			bool has_hit = false;
			struct monster *t_mon = NULL;

			/* Get arguments */
			if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
				return;
			grid = loc_sum(mon->grid, ddgrid[dir]);

			/* Don't let immobile monsters be moved */
			if (rf_has(mon->race->flags, RF_NEVER_MOVE)) {
				msg("The monster can not move.");
				return;
			}

			/* Monster there - attack */
			t_mon = square_monster(cave, grid);
			if (t_mon) {
				/* Attack the monster */
				if (monster_attack_monster(mon, t_mon)) {
					has_hit = true;
				} else {
					can_move = false;
				}
			} else if (square_ispassable(cave, grid)) {
				/* Floor is open? */
				can_move = true;
			} else if (square_isperm(cave, grid)) {
				/* Permanent wall in the way */
				can_move = false;
			} else {
				/* There's some kind of feature in the way, so learn about
				 * kill-wall and pass-wall now */
				if (monster_is_visible(mon)) {
					rf_on(lore->flags, RF_PASS_WALL);
					rf_on(lore->flags, RF_KILL_WALL);
					rf_on(lore->flags, RF_SMASH_WALL);
				}

				/* Monster may be able to deal with walls and doors */
				if (rf_has(mon->race->flags, RF_PASS_WALL)) {
					can_move = true;
				} else if (rf_has(mon->race->flags, RF_KILL_WALL)) {
					/* Remove the wall */
					square_destroy_wall(cave, grid);
					can_move = true;
				} else if (rf_has(mon->race->flags, RF_SMASH_WALL)) {
					/* Remove everything */
					square_smash_wall(cave, grid);
					can_move = true;
				} else if (square_iscloseddoor(cave, grid) ||
						   square_issecretdoor(cave, grid)) {
					bool can_open = rf_has(mon->race->flags, RF_OPEN_DOOR);
					bool can_bash = rf_has(mon->race->flags, RF_BASH_DOOR);

					/* Learn about door abilities */
					if (monster_is_visible(mon)) {
						rf_on(lore->flags, RF_OPEN_DOOR);
						rf_on(lore->flags, RF_BASH_DOOR);
					}

					/* If the monster can deal with doors, prefer to bash */
					if (can_bash || can_open) {
						/* Now outcome depends on type of door */
						if (square_islockeddoor(cave, grid)) {
							/* Test strength against door strength */
							int k = square_door_power(cave, grid);
							if (randint0(mon->hp / 10) > k) {
								if (can_bash) {
									msg("%s slams against the door.", m_name);
								} else {
									msg("%s fiddles with the lock.", m_name);
								}

								/* Reduce the power of the door by one */
								square_set_door_lock(cave, grid, k - 1);
							}
						} else {
							/* Closed or secret door -- always open or bash */
							if (can_bash) {
								square_smash_door(cave, grid);

								msg("You hear a door burst open!");

								/* Fall into doorway */
								can_move = true;
							} else {
								square_open_door(cave, grid);
								can_move = true;
							}
						}
					}
				}
			}

			if (has_hit) {
				break;
			} else if (can_move) {
				monster_swap(mon->grid, grid);
				player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
			} else {
				msg("The way is blocked.");
			}
			break;
		}
		default: {
			msg("Valid commands: move, stand still, 'd'rop, 'm'agic, or 'r'elease.");
			return;
		}
	}


	/* Take a turn */
	player->upkeep->energy_use = z_info->move_energy;
}


static int hiring_price(struct monster *mon, struct player *p)
{
	assert(mon && mon->race);
	struct monster_race *pmonr = lookup_player_monster(p);
	int lev = MAX(mon->race->level, mon->race->level / 2 + 5);
	int dist = distance(mon->grid, player->mon.grid) + 10; // no yelling from a distance!
	int uniq = monster_is_unique(mon) ? 3 : 1;
	int allied = pmonr && mon->race->d_char == pmonr->d_char ? 2 : 3; // already friends :)

	int total = lev * lev * dist * uniq * allied;
	int mult = 1;

	total *= MON_REACT_MAX - mon->reaction;
	total /= MON_REACT_MAX - MON_REACT_ALLY;

	// round a bit
	while (total >= 100) {
		total /= 10;
		mult *= 10;
	}
	total /= 5;
	mult *= 5;

	return MAX(0, total * mult);
}

static int teaching_price(struct monster *mon, struct player *p)
{
	return 10 << (mon->race->level / 5);
}

/*static int teaching_price(struct monster *mon, struct player *p, int power)
{
	int currpower = p->extra_powers[power];
	int maxpower = mon->race->level;
	int i;
	int16_t result = 10;

	for (i = 0; i < (currpower + maxpower) / 4; ++i) {
		if ((i % 3) == 1) {
			result *= 5;
			result /= 2;
		}
		else {
			result *= 2;
		}

		if (result > INT16_MAX / 10) break; 
	}

	return result;
}*/


void do_cmd_diplomacy(struct command *cmd)
{
	int dir;
	struct monster *mon = NULL;
	int new_cmd;
	char mdesc[80];

	if (player->mon.m_timed[TMD_CONFUSED] > 0) {
		msg("You are too confused to talk!");
		return;
	}

	if (cmd_get_target(cmd, "target", &dir) != CMD_OK) {
		return;
	}

	mon = smite_target_get(dir);

	if (!mon || !mon->race) {
		return;
	}

	monster_desc(mdesc, sizeof(mdesc), mon, MDESC_TARG | MDESC_CAPITAL);

	monster_become_aware(mon);

	player->upkeep->energy_use = z_info->move_energy * 5;
	monster_wake(mon, false, 100);
	mflag_off(mon->mflag, MFLAG_TALKING);

	if (mon->target.who == TARGET_WHO_MONSTER) {
		msg("%s is busy!", mdesc);
		return;
	}
	if (mon->reaction <= MON_REACT_NO_TALK) {
		msg("%s doesn't want to talk.", mdesc);
		return;
	}

	new_cmd = textui_do_diplomacy(player, mon, "You have nothing about which to talk.");

	if (new_cmd != CMD_DIP_GIFT && new_cmd != CMD_DIP_HIRE && new_cmd != CMD_DIP_LEARN) return;

	mflag_on(mon->mflag, MFLAG_TALKING);
	cmdq_push(new_cmd);
	cmd_set_arg_target(cmdq_peek(), "target", dir);

	return;
}


void do_cmd_dip_hire(struct command *cmd) {
	int dir;
	struct monster *mon = NULL;
	struct loc target;

	if (player->mon.m_timed[TMD_CONFUSED] > 0) {
		msg("You are too confused to talk!");
	}

	if (cmd_get_target(cmd, "target", &dir) != CMD_OK) {
		return;
	}

	cmdq_push(CMD_DIPLOMACY);
	cmd_set_arg_target(cmdq_peek(), "target", dir);

	if (dir == DIR_TARGET) {
		if (target_okay()) {
			target_get(&target);
			mon = square_monster(cave, target);
		}
	} else if (dir != DIR_UNKNOWN) {
		int i, range = z_info->max_sight;
		struct loc direction = loc_sum(player->mon.grid, loc(range * ddx[dir], range * ddy[dir]));
		int path_n;
		struct loc path_g[256];
		path_n = project_path(cave, path_g, range, player->mon.grid, direction, 0);
		for (i = 0; i < path_n; i++) {
			target = path_g[i];
			mon = square_monster(cave, target);
			if (mon) {
				break;
			}
		}
	}

	if (!mon || !mon->race) {
		return;
	}

	if (mon->faction == '@') {
		msg("They are already your companion!");
		return;
	}
	if (mon_will_attack_player(mon, player)) {
		msg("They don't look like they want to talk.");
		return;
	}

	monster_wake(mon, false, 100);
	player->upkeep->energy_use = z_info->move_energy * 5;

	int price = hiring_price(mon, player);

	char desc[80];
	monster_desc(desc, sizeof(desc), mon, MDESC_TARG);
	bool result = get_check(format("Hire %s for %i gold? ", desc, price));

	if (result && player->au >= price) {
		player->au -= price;
		mon->faction = '@';
		my_strcap(desc);
		msg("%s agrees to follow you.", desc);
		races_unlock(player);
	}
	else if (result) {
		msg("You can't afford their price!");
	}
}

static bool gift_tester(const struct object *obj)
{
	return object_value(obj, 1) > 0;
}

void do_cmd_dip_gift(struct command *cmd)
{
	int dir;
	struct object *selection, *gift;
	bool none_left;
	struct monster *mon;
	struct loc target;
	int value, quantity;
	int reactbonus;

	if (cmd_get_target(cmd, "target", &dir) != CMD_OK) {
		return;
	}

	cmdq_push(CMD_DIPLOMACY);
	cmd_set_arg_target(cmdq_peek(), "target", dir);

	if (dir == DIR_TARGET) {
		if (target_okay()) {
			target_get(&target);
			mon = square_monster(cave, target);
		}
	} else if (dir != DIR_UNKNOWN) {
		int i, range = z_info->max_sight;
		struct loc direction = loc_sum(player->mon.grid, loc(range * ddx[dir], range * ddy[dir]));
		int path_n;
		struct loc path_g[256];
		path_n = project_path(cave, path_g, range, player->mon.grid, direction, 0);
		for (i = 0; i < path_n; i++) {
			target = path_g[i];
			mon = square_monster(cave, target);
			if (mon) {
				break;
			}
		}
	}

	if (!mon || !mon->race) return;

	if (cmd_get_item(cmd, "gift", &selection, "Give which item? ", "You have nothing to give.", gift_tester, USE_INVEN)) {
		return;
	}

	if (selection->number == 1) {
		quantity = 1;
	} else if (cmd_get_quantity(cmd, "quantity", &quantity, selection->number) != CMD_OK) {
		return;
	}

	gift = gear_object_for_use(&player->mon, selection, quantity, true, &none_left);
	value = object_value_real(gift, gift->number);

	monster_carry(cave, mon, gift);

	reactbonus = (randint1(value) + value / 2) / (mon->race->level + 25);

	reaction_change(mon, reactbonus);

	player->upkeep->energy_use = z_info->move_energy * 5;
}

void do_cmd_dip_learn(struct command *cmd)
{
	int i;
	int dir;
	struct monster *mon;
	int cost;
	int *max_learn, *base_max_learn, *extra_max_learn;
	//bool has_power = false;
	bool did_learn = false, can_learn;

	if (cmd_get_target(cmd, "target", &dir) != CMD_OK) {
		return;
	}

	cmdq_push(CMD_DIPLOMACY);
	cmd_set_arg_target(cmdq_peek(), "target", dir);

	mon = smite_target_get(dir);
	cost = teaching_price(mon, player);

	if (!mon || !mon->race) return;

	if (!player_can_learn_from_monster(player, mon)) return;

	max_learn = mem_zalloc(sizeof *max_learn * z_info->learn_max);
	base_max_learn = mem_zalloc(sizeof *base_max_learn * z_info->learn_max);
	extra_max_learn	= mem_zalloc(sizeof *extra_max_learn * z_info->learn_max);

	for (i = PP_NONE + 1; i < PP_MAX; ++i) {

		if (mon->powers[i]) {
			extra_max_learn[i] = mon->race->level;
			/*int new_target = MIN(mon->race->level, player->extra_target[i]);
			if (max_learn[i] < new_target) {
				temp_max_learn[i] = new_target;
				can_learn = true;
				//has_power = true;
			}*/
			
			/* &&
				mon->race->level > player->extra_tar[i] &&
				player->extra_powers[i] < 50 &&
				player_bonus_to_cost(player->extra_powers[i], i, player) <
					player_bonus_to_cost(player->extra_powers[i] + 1, i, player)) {*/

			/*if (get_check(format("Ask to learn %s? (%i gp) ", pname, cost))) {
				result = i;
				break;
			}*/
		}
	}

	can_learn = tome_max_learnable_extra(player, max_learn, extra_max_learn);
	tome_max_learnable(player, base_max_learn);

	if (!can_learn) {
		msg("You have nothing to learn from them!");
	} else if (player->au < cost) {
		msg("You can't afford their price!");
	} else {
		get_learn(player, max_learn, false);

		for (i = PP_NONE + 1; !did_learn && i < PP_MAX; ++i) {
			if (player->extra_target[i] > base_max_learn[i]) {
				did_learn = true;
			}
		}

		if (did_learn) {
			player->au -= cost;
		}
	}

	mem_free(max_learn);
	mem_free(base_max_learn);
	mem_free(extra_max_learn);
}

void do_cmd_learn(struct command *cmd)
{
	int *max_learn = mem_zalloc(sizeof *max_learn * z_info->learn_max);
	int tmp;
	bool birth = false;

	if (cmd_get_arg_number(cmd, "birth", &tmp) == CMD_OK) {
		birth = tmp;
	}

	tome_max_learnable(player, max_learn);
	get_learn(player, max_learn, birth);

	mem_free(max_learn);
}

