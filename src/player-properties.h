/**
 * \file player-properties.h
 * \brief Class and race abilities
 *
 * Copyright (c) 1997-2020 Ben Harrison, James E. Wilson, Robert A. Koeneke,
 * Leon Marrick, Bahman Rabii, Nick McConnell
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

#ifndef PLAYER_PROPS_H
#define PLAYER_PROPS_H

enum {
    PLAYER_FLAG_NONE,
    PLAYER_FLAG_SPECIAL,
    PLAYER_FLAG_RACE,
    PLAYER_FLAG_CLASS,
    PLAYER_FLAG_POWER,
    PLAYER_FLAG_SKILL
};

enum player_ability_types {
    PY_ABIL_ELEMENT,
    PY_ABIL_POWER,
    PY_ABIL_SKILL,
    PY_ABIL_PLAYER,
    PY_ABIL_OBJECT,
    PY_ABIL_SPECIAL,
    PY_ABIL_MAX
};

struct player_ability *lookup_player_ability(int idx, int type);
bool class_has_ability(const struct player_class *class,
					   struct player_ability *ability);
bool race_has_ability(const struct player_race *race,
					  struct player_ability *ability);
void do_cmd_abilities(void);

const char *ability_subchoice_title(const struct player_ability *parent);
int ability_subchoice_choices(struct player_ability *parent);
const char *ability_subchoice_name(int id, const struct player_ability *parent);
bool make_ability_subchoice(struct player *p);

#endif /* !PLAYER_PROPS_H */
