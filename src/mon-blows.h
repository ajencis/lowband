/**
 * \file mon-blows.h
 * \brief Functions for managing monster melee.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
 *               2013 Ben Semmler
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

#ifndef MON_BLOWS_H
#define MON_BLOWS_H

#include "player.h"
#include "monster.h"

struct blow_message {
	char *act_msg;
	struct blow_message *next;
};

struct blow_method {
	char *name;
	bool cut;
	bool stun;
	bool miss;
	bool phys;
	int msgt;
	struct blow_message *messages;
	char *fmessage;
	int num_messages;
	char *desc;
	struct blow_method *next;

	int equip_slot;			/* L: the slot it uses */
	int skill;				/* L: the skill it uses to hit */
};

extern struct blow_method *blow_methods;

/**
 * Storage for context information for effect handlers called in
 * make_attack_normal().
 *
 * The members of this struct are initialized in an order-dependent way
 * (to be more cross-platform). If the members change, make sure to change
 * any initializers. Ideally, this should eventually used named initializers.
 */
typedef struct melee_effect_handler_context_s {
	struct player * const p;	/* Target (if player) */
	struct monster * const mon;	/* Attacker */
	struct monster * const t_mon;	/* Target (if other monster) */
	const int rlev;
	const struct blow_method *method;
	const int ac;
	const char *ddesc;		/* short monster name for death
						messages; unused if target is
						not the player */
	bool obvious;
	bool blinked;
	int damage;
	const char *m_name;		/* monster name for messaging */
} melee_effect_handler_context_t;

/**
 * Melee blow effect handler.
 */
typedef void (*melee_effect_handler_f)(melee_effect_handler_context_t *);

struct blow_effect {
	char *name;
	int power;
	int eval;
	char *desc;
	uint8_t lore_attr;		/* Color of the attack used in lore text */
	uint8_t lore_attr_resist;	/* Color used in lore text when resisted */
	uint8_t lore_attr_immune;	/* Color used in lore text when resisted strongly */
	char *effect_type;
	int resist;
	int lash_type;
	struct blow_effect *next;
	int mtimed;					/* L: the timed effect to apply to a monster taking this attack */
};

extern struct blow_effect *blow_effects;

/* Functions */
int blow_index(const char *name);
char *monster_blow_method_action(const struct blow_method *method, int midx);
extern melee_effect_handler_f melee_handler_for_blow_effect(const char *name);

#endif /* MON_BLOWS_H */
