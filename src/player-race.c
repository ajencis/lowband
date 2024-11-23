/**
 * \file player-race.c
 * \brief Player races
 *
 * Copyright (c) 2011 elly+angband@leptoquark.net. See COPYING.
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
#include "monster.h"
#include "player.h"

struct player_race *player_id2race(guid id)
{
	struct player_race *r;
	for (r = races; r; r = r->next)
		if (guid_eq(r->ridx, id))
			break;
	return r;
}

static int max_evol_lev(const struct monster_race *mr)
{
	const struct evolution *curr;
	int maxlev = mr->level;

	for (curr = mr->evol; curr; curr = curr->next) {
		int currlev = max_evol_lev(curr->race);
		maxlev = MAX(maxlev, currlev);
	}
	
	return maxlev;
}

int max_race_evol_lev(struct player_race *r)
{
	const struct evolution *curr;
	int maxlev = 0;

	for (curr = r->evol; curr; curr = curr->next) {
		int currlev = max_evol_lev(curr->race);
		maxlev = MAX(maxlev, currlev);
	}

	return maxlev;
}
