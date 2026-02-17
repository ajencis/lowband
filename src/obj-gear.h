/**
 * \file: obj-gear.h
 * \brief management of inventory, equipment and quiver
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

#ifndef OBJECT_GEAR_H
#define OBJECT_GEAR_H

#include "player.h"

/**
 * Player equipment slot types
 */
enum
{
	#define EQUIP(a, b, c, d, e, f) EQUIP_##a,
	#include "list-equip-slots.h"
	#undef EQUIP
	EQUIP_MAX
};

int slot_by_name(struct monster *mon, const char *name);
int slot_by_type(struct monster *mon, int type, bool full);
bool slot_type_is(const struct monster *mon, int slot, int type);
struct object *slot_object(struct monster *mon, int slot);
struct object *equipped_item_by_slot_name(struct monster *mon, const char *name);
int object_slot(struct player_body body, const struct object *obj);
bool object_is_equipped(struct player_body body, const struct object *obj);
bool object_is_carried(struct monster *mon, const struct object *obj);
bool object_is_in_quiver(struct player *p, const struct object *obj);
uint16_t object_pack_total(struct monster *mon, const struct object *obj,
	bool ignore_inscrip, struct object **first);
int pack_slots_used(const struct monster *mon);
const char *equip_mention(struct monster *mon, int slot);
const char *equip_describe(struct monster *mon, int slot);
int wield_slot_type(const struct object *obj);
int wield_slot_type_k(const struct object_kind *obj);
int wield_slot(struct monster *mon, const struct object *obj);
int wield_slot_k(struct monster *mon, const struct object_kind *obj);
bool minus_ac(struct monster *mon);
char gear_to_label(struct player *p, struct object *obj);
struct object *gear_last_item(struct monster *mon);
void gear_insert_end(struct monster *mon, struct object *obj);
struct object *gear_object_for_use(struct monster *mon, struct object *obj,
	int num, bool message, bool *none_left);
int inven_carry_num(const struct monster *mon, const struct object *obj);
bool inven_carry_okay(struct monster *mon, const struct object *obj);
bool player_inven_carry_okay(const struct object *obj);
void inven_item_charges(struct object *obj);
void inven_carry(struct chunk *c, struct monster *mon, struct object *obj, bool absorb,
				 bool message);
void inven_wield(struct chunk *c, struct monster *mon, struct object *obj, int slot, bool verbose);
void inven_takeoff(struct monster *mon, struct object *item);
void inven_drop(struct monster *mon, struct object *obj, int amt);
void combine_pack(struct monster *mon);
bool pack_is_full(struct monster *mon);
bool pack_is_overfull(struct monster *mon);
void pack_overflow(struct monster *mon, struct object *obj);
int preferred_quiver_slot(const struct object *obj);


#endif /* OBJECT_GEAR_H */
