/**
 * \file source.c
 * \brief Type that allows various different origins for an effect
 *
 * Copyright (c) 2016 Andi Sidwell
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

#include "source.h"
#include "cave.h"
#include "monster.h"
#include "mon-desc.h"
#include "obj-desc.h"
#include "player.h"
#include "player-enum.h"
#include "z-util.h"

struct source source_none(void)
{
	struct source src;
	src.what = SRC_NONE;
	return src;
}

struct source source_trap(struct trap *trap)
{
	struct source src;
	src.what = SRC_TRAP;
	src.which.trap = trap;
	return src;
}

struct source source_player(void)
{
	struct source src;
	src.what = SRC_PLAYER;
	return src;
}

struct source source_monster(int who)
{
	struct source src;

	if (who == PLAYER_MON_MIDX) {
		return source_player();
	}

	src.what = SRC_MONSTER;
	src.which.monster = who;
	return src;
}

struct source source_object(struct object *object)
{
	struct source src;
	src.what = SRC_OBJECT;
	src.which.object = object;
	return src;
}

struct source source_chest_trap(struct chest_trap *chest_trap)
{
	struct source src;
	src.what = SRC_CHEST_TRAP;
	src.which.chest_trap = chest_trap;
	return src;
}

struct source source_grid(struct loc grid)
{
	struct source src;
	src.what = SRC_GRID;
	src.which.grid = grid;
	return src;
}



void death_message_by_source(struct source origin, char *buf, size_t bufsize)
{
	switch (origin.what) {
		case SRC_TRAP:
		case SRC_CHEST_TRAP:
		{
			strnfmt(buf, bufsize, "a trap");
			break;
		}

		case SRC_MONSTER:
		{
			const struct monster *mon = cave_monster(cave, origin.which.monster);
			monster_desc(buf, bufsize, mon, MDESC_DIED_FROM);
			break;
		}

		case SRC_OBJECT:
		{
			const struct object *obj = origin.which.object;
			object_desc(buf, bufsize, obj, ODESC_SINGULAR | ODESC_TERSE, player);
			break;
		}

		case SRC_PLAYER:
		{
			strnfmt(buf, bufsize, "yourself");
			break;
		}

		default:
		{
			strnfmt(buf, bufsize, "a bug");
			break;
		}
	}
}

