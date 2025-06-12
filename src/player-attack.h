/**
 * \file player-attack.h
 * \brief Attacks (both throwing and melee) by the player
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

#ifndef PLAYER_ATTACK_H
#define PLAYER_ATTACK_H

#include "cmd-core.h"
#include "player.h"

struct player;

struct attack_result {
    bool success;
    int dmg;
    uint32_t msg_type;
    char *hit_verb;
};

/**
 * A list of the different hit types and their associated special message
 */
struct hit_types {
	uint32_t msg_type;
	const char *text;
};

/**
 * ranged_attack is a function pointer, used to execute a kind of attack.
 *
 * This allows us to abstract details of throwing, shooting, etc. out while
 * keeping the core projectile tracking, monster cleanup, and display code
 * in common.
 */
typedef struct attack_result (*ranged_attack) (struct player *p,
											   struct object *obj,
											   struct loc grid);

extern void do_cmd_fire(struct command *cmd);
extern void do_cmd_fire_at_nearest(void);
extern void do_cmd_throw(struct command *cmd);
extern void do_cmd_melee(struct command *cmd);


/* L: new */
bool death_touch_extra_dam(const struct py_attack_roll *aroll, int crit_power, int *dice, int *sides);
bool get_unarmed_punch(struct player *p, struct player_state *ps,
		struct py_attack_roll *aroll, int attack_div);
bool get_unarmed_kick(struct player *p, struct player_state *ps,
		struct py_attack_roll *aroll, int attack_div);
bool get_unarmed_touch(struct player *p, struct player_state *ps,
		struct py_attack_roll *aroll, int attack_div);
bool get_melee_weapon_attack(struct player *p, struct player_state *ps, struct object *obj,
		struct py_attack_roll *aroll, int attack_div);
struct py_attack_roll get_shooter_weapon_attack(struct player *p, struct player_state *ps,
												struct object *shooter);
int get_monster_attacks(struct player *p, struct player_state *ps, struct monster_race *mr,
		struct py_attack_roll *aroll, int maxnum, int *attacknum, bool ranged);

bool monster_can_be_attacked(struct player *p, const struct py_attack_roll *aroll,
		struct monster *mon, char *buf, size_t bufsize);
bool player_can_attack_monster(struct player *p, struct monster *mon);

extern int breakage_chance(const struct object *obj, bool hit_target);
int chance_of_melee_hit_base(const struct player *p,
	struct py_attack_roll *aroll);
extern bool test_hit(int to_hit, int ac);
void hit_chance(random_chance *, int, int);
void apply_deadliness(int *die_average, int deadliness);
extern void py_attack(struct player *p, struct loc grid);
extern bool py_attack_real(struct player *p, struct loc grid, bool *fear, struct py_attack_roll *aroll);

/* These are public for use by unit test cases. */
struct attack_result make_ranged_shot(struct player *p, struct object *ammo,
		struct loc grid);
struct attack_result make_ranged_throw(struct player *p, struct object *obj,
		struct loc grid);

#endif /* !PLAYER_ATTACK_H */
