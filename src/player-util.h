/**
 * \file player-util.h
 * \brief Player utility functions
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

#ifndef PLAYER_UTIL_H
#define PLAYER_UTIL_H

#include "cmd-core.h"
#include "player.h"

/* Player regeneration constants */
#define PY_REGEN_FULL		878		/* L: regen factor *2^16 when overfull */
#define PY_REGEN_NORMAL		293		/* Regen factor*2^16 when full */
#define PY_REGEN_WEAK		98		/* Regen factor*2^16 when weak */
#define PY_REGEN_FAINT		33		/* Regen factor*2^16 when fainting */
#define PY_REGEN_HPBASE		14		/* Min amount hp regen*2^16 */
#define PY_REGEN_MNBASE		524		/* Min amount mana regen*2^16 */

/* Player over-exertion */
enum {
	PY_EXERT_NONE = 0x00,
	PY_EXERT_CON = 0x01,
	PY_EXERT_FAINT = 0x02,
	PY_EXERT_SCRAMBLE = 0x04,
	PY_EXERT_CUT = 0x08,
	PY_EXERT_CONF = 0x10,
	PY_EXERT_HALLU = 0x20,
	PY_EXERT_SLOW = 0x40,
	PY_EXERT_HP = 0x80
};

/**
 * Special values for the number of turns to rest, these need to be
 * negative numbers, as postive numbers are taken to be a turncount,
 * and zero means "not resting". 
 */
enum
{
	REST_COMPLETE = -2,
	REST_ALL_POINTS = -1,
	REST_SOME_POINTS = -3
};

/**
 * Minimum number of turns required for regeneration to kick in during resting.
 */
#define REST_REQUIRED_FOR_REGEN 5

int stat_max_max(struct player *p, int stat);

//bool unlock_all(struct player *p);
//bool races_unlock(struct player *p);
//bool tomes_unlock(struct player *p);
//bool player_can_metaprogress(struct player *p);

struct monster_race *race_to_monster(const struct player_race *r);
struct monster_race *lookup_player_monster(const struct player *p);
const struct monster_race *last_evolution(const struct player *p);
void change_player_monster(struct player *p, const struct monster_race *mon, bool init);
bool check_player_monster(struct player *p, bool init);
void player_race_name(struct player *p, char *buf, size_t bufsize);
bool player_increase_stat(struct player *p);
void remove_first_evolution(struct player *p);
void remove_last_evolution(struct player *p);
bool add_evolution(struct player *p, const struct monster_race *mr);
bool select_evolution(struct player *p);
int expected_monster_evol_level(const struct monster_race *mr);
int expected_max_evol_level(const struct player *p);

//int get_power_scale_state(const struct player_state *ps, int power, int scaleto, int level);
//int get_power_scale(const struct player *p, int power, int scaleto);
uint16_t calc_extra_points_array(struct player *p, uint16_t *extra_abil);
void calc_extra_points(struct player *p, struct player_state *ps);
//bool check_learn_powers(struct player *p, int xpgain);
bool obj_can_learn_extra_from(const struct object *obj);
bool learn_extra(struct player *p, const struct player_ability *abil);
bool learn_realm(struct player *p, const struct magic_realm *realm);

//int player_class_power_array(const struct player_class *c, int extra_power, int power);
//int player_class_power(struct player *p, int power);
int player_race_power_array(const struct monster_race *r, int extra_power, int power);
int player_race_power(struct player *p, int power);
//int class_x_skill(const struct player_class *c, int extra, int skill);
//int player_class_x_skill(struct player *p, int skill);
//int class_c_skill(const struct player_class *c, int extra, int skill);
//int player_class_c_skill(struct player *p, int skill);

int player_skill_stat_ind(const struct player *p, const struct player_state *ps, int skill);
void player_skill_stats(struct player *p, struct player_state *ps, int skill, int *stat1, int *stat2);
void skill_stat(const struct magic_realm *realm, const int indices[STAT_MAX], int skill, int *stat1, int *stat2);
int skill_stat_ind(const struct magic_realm *realm, const int indices[STAT_MAX], int skill);
bool player_learn_spell_xp(struct player *p, bool initial, int xp);
int player_bonus_to_cost(int bonus, const struct player_ability *abil, struct player *p);
bool tome_max_learnable_extra_array(bool metaprog, int *learn_array, int *extra_array,
	int *curr_powers, int *curr_skills, struct player *p);
bool tome_max_learnable_extra(struct player *p, int *learn_array, int *extra_array);
void tome_max_learnable(struct player *p, int *learn_array);
int tome_next_increment(struct player *p, const struct player_ability *abil, int curr_bonus);
int tome_prev_increment(struct player *p, const struct player_ability *abil, int curr_bonus);
const char *lookup_power_name(int power);
const struct player_ability *tome_parent(const struct player_ability *abil);
const struct player_ability *player_ability_by_learn_index(int learn_index);

int antimagic_fail_increase(struct player *p);
int antimagic_radius(struct player *p);

//int unlight_power_state(struct player_state *ps, struct player *p);
//int unlight_power(struct player *p);
//int glow_power_state(struct player_state *ps, struct player *p);
//int glow_power(struct player *p);
int player_grid_visibility(struct loc grid, struct player *p, struct chunk *c);
int unlight_radius(struct player *p);

int dungeon_get_next_level(struct player *p, int dlev, int added);
void player_set_recall_depth(struct player *p);
bool player_get_recall_depth(struct player *p);
void dungeon_change_level(struct player *p, int dlev);
int player_apply_damage_reduction(struct player *p, int dam);
bool take_hit(struct player *p, int dam, const char *kb_str);
void take_max_sp_dam(struct player *p, int dam);
void death_knowledge(struct player *p);
int energy_per_move(struct player *p);
int16_t modify_stat_value(int value, int amount);
void player_scramble_stats(struct player *p);
void player_fix_scramble(struct player *p);
void player_regen_hp(struct player *p);
void regen_hp(struct monster *mon);
void player_regen_mana(struct player *p);
void player_adjust_hp_precise(struct player *p, int32_t hp_gain);
int32_t player_adjust_mana_precise(struct player *p, int32_t sp_gain);
void convert_mana_to_hp(struct player *p, int32_t sp);
bool check_berserk(struct monster *mon, struct monster *o_mon);
void player_update_light(struct player *p);
void player_over_exert(struct player *p, int flag, int chance, int amount);
struct object *player_best_digger(struct player *p, bool forbid_stack);
bool bloodlust_override(struct player *p, struct chunk *c);
int player_check_terrain_damage(struct player *p, struct loc grid, bool actual);
void player_take_terrain_damage(struct player *p, struct loc grid);
struct player_shape *lookup_player_shape(const char *name);
int shape_name_to_idx(const char *name);
struct player_shape *player_shape_by_idx(int index);
bool player_get_resume_normal_shape(struct player *p, struct command *cmd);
void player_resume_normal_shape(struct player *p);
bool player_is_shapechanged(const struct player *p);
bool player_is_trapsafe(const struct player *p);
bool player_can_cast(const struct player *p, bool show_msg);
bool player_can_study(const struct player *p, bool show_msg);
bool player_can_read(const struct player *p, bool show_msg);
bool player_can_fire(struct player *p, bool show_msg);
bool player_can_refuel(struct player *p, bool show_msg);
bool player_can_cast_prereq(void);
bool player_can_study_prereq(void);
bool player_can_read_prereq(void);
bool player_can_fire_prereq(void);
bool player_can_refuel_prereq(void);
bool player_can_debug_prereq(void);
bool player_is_invisible(struct player *p);
//bool player_book_has_unlearned_spells(struct player *p);
bool player_confuse_dir(struct player *p, int *dir, bool too);
bool player_resting_is_special(int16_t count);
bool player_is_resting(const struct player *p);
int16_t player_resting_count(const struct player *p);
void player_resting_set_count(struct player *p, int16_t count);
void player_resting_cancel(struct player *p, bool disturb);
bool player_resting_can_regenerate(const struct player *p);
void player_resting_step_turn(struct player *p);
void player_resting_complete_special(struct player *p);
int player_get_resting_repeat_count(struct player *p);
void player_set_resting_repeat_count(struct player *p, int16_t count);
bool player_of_has(const struct player *p, int flag);
bool player_resists(const struct player *p, int element);
bool player_is_immune(const struct player *p, int element);
void player_place(struct chunk *c, struct player *p, struct loc grid);
void player_handle_post_move(struct player *p, bool eval_trap,
		bool is_involuntary);
void disturb(struct player *p);
void search(struct player *p);
void player_store_gold(struct player *p);
void player_start_turn(struct player *p);

void init_obj_log_file(bool save);
void describe_saveload(const char *msg, bool save);
void describe_object_saveload(const struct object *obj, const char *source, bool save);

#endif /* !PLAYER_UTIL_H */
