/**
 * \file init.c
 * \brief Various game initialization routines
 *
 * Copyright (c) 1997 Ben Harrison
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
 *
 * This file is used to initialize various variables and arrays for the
 * Angband game.
 *
 * Several of the arrays for Angband are built from data files in the
 * "lib/gamedata" directory.
 */


#include "angband.h"
#include "buildid.h"
#include "cave.h"
#include "cmd-core.h"
#include "datafile.h"
#include "effects.h"
#include "game-event.h"
#include "game-world.h"
#include "hint.h"
#include "init.h"
#include "message.h"
#include "mon-init.h"
#include "mon-list.h"
#include "mon-make.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-chest.h"
#include "obj-init.h"
#include "obj-list.h"
#include "obj-pile.h"
#include "obj-properties.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "option.h"
#include "parser.h"
#include "player-enum.h"
#include "player.h"
#include "player-properties.h"
#include "player-quest.h"
#include "player-timed.h"
#include "project.h"
#include "randname.h"
#include "trap.h"
#include "ui-entry.h"
#include "ui-entry-init.h"
#include "ui-visuals.h"
#include "z-file.h"
#include "z-util.h"
#include "z-virt.h"

bool play_again = false;

/**
 * Structure (not array) of game constants
 */
struct angband_constants *z_info;

/*
 * Hack -- The special Angband "System Suffix"
 * This variable is used to choose an appropriate "pref-xxx" file
 */
const char *ANGBAND_SYS = "xxx";

/**
 * Various directories. These are no longer necessarily all subdirs of "lib"
 */
char *ANGBAND_DIR_GAMEDATA;
char *ANGBAND_DIR_CUSTOMIZE;
char *ANGBAND_DIR_HELP;
char *ANGBAND_DIR_SCREENS;
char *ANGBAND_DIR_FONTS;
char *ANGBAND_DIR_TILES;
char *ANGBAND_DIR_SOUNDS;
char *ANGBAND_DIR_ICONS;
char *ANGBAND_DIR_USER;
char *ANGBAND_DIR_SAVE;
char *ANGBAND_DIR_PANIC;
char *ANGBAND_DIR_SCORES;
char *ANGBAND_DIR_ARCHIVE;

static const char *slots[] = {
	#define EQUIP(a, b, c, d, e, f) #a,
	#include "list-equip-slots.h"
	#undef EQUIP
	NULL
};

const char *list_obj_flag_names[] = {
	"NONE",
	#define OF(a, b) #a,
	#include "list-object-flags.h"
	#undef OF
	NULL
};

static const char *list_player_powers_names[] = {
	"NONE",
	#define PP(x) #x,
	#include "list-player-powers.h"
	#undef PP
	NULL
};

static const char *list_player_skill_names[] = {
	#define SKILL(x, a, b, c, d, e) #x,
	#include "list-skills.h"
	#undef SKILL
	NULL
};

static const char *list_school_names[] = {
	"NONE",
	#define MS(x) #x,
	#include "list-magic-schools.h"
	#undef MS
	NULL
};

static const char *obj_mods[] = {
	#define STAT(a) #a,
	#include "list-stats.h"
	#undef STAT
	#define OBJ_MOD(a) #a,
	#include "list-object-modifiers.h"
	#undef OBJ_MOD
	NULL
};

const char *list_element_names[] = {
	#define ELEM(a) #a,
	#include "list-elements.h"
	#undef ELEM
	NULL
};

const char *list_proj_names[] = {
	#define ELEM(a) #a,
	#include "list-elements.h"
	#undef ELEM
	#define PROJ(a) #a,
	#include "list-projections.h"
	#undef PROJ
	NULL
};

static const char *effect_list[] = {
	"NONE",
	#define EFFECT(x, a, b, c, d, e, f) #x,
	#include "list-effects.h"
	#undef EFFECT
	"MAX"
};

static const char *trap_flags[] =
{
	#define TRF(a, b) #a,
	#include "list-trap-flags.h"
	#undef TRF
    NULL
};

static const char *terrain_flags[] =
{
	#define TF(a, b) #a,
	#include "list-terrain-flags.h"
	#undef TF
    NULL
};

static const char *mon_race_flags[] =
{
	#define RF(a, b, c, d) #a,
	#include "list-mon-race-flags.h"
	#undef RF
	NULL
};

const char *player_info_flags[] =
{
	#define PF(a) #a,
	#include "list-player-flags.h"
	#undef PF
	NULL
};

static const char *realm_special_names[] =
{
	#define RLM_SPCL(x) #x,
	#include "list-realm-special.h"
	#undef RLM_SPCL
	"MAX" 
};

static const char *ability_predicate_names[] =
{
	#define PRED(x) #x,
	#include "list-ability-predicates.h"
	#undef PRED
	NULL
};

static const char *race_predicate_names[] =
{
	#define RACE_PRED(x) #x,
	#include "list-mon-race-predicates.h"
	#undef RACE_PRED
	NULL
};

static const char *list_feat_names[] =
{
	#define FEAT(x) #x,
	#include "list-terrain.h"
	#undef FEAT
	NULL
};

static const char *list_subprop_type_names[] =
{
	"NONE",
	#define SUB_TYP(x) #x,
	#include "list-subproperty-types.h"
	#undef SUB_TYP
	NULL
};

static int school_idx_by_name(const char *name)
{
	int i;
	for (i = MS_NONE + 1; i < MS_MAX; ++i) {
		if (streq(name, list_school_names[i])) {
			return i;
		}
	}
	return MS_NONE;
}

static int realm_special_by_name(const char *name)
{
	int i;
	for (i = N_ELEMENTS(realm_special_names) - 1; i >= 0; --i) {
		if (streq(name, realm_special_names[i])) {
			return i;
		}
	}
	return -1;
}

static void spell_to_spellbook(const struct player_spell *spell, struct object_kind *kind)
{
	assert(spell && kind);

	kind->spell = spell;

	kind->alloc_max = spell->slevel * 5;
	kind->alloc_min = spell->slevel;
	kind->alloc_prob = MAX(1, 10 - spell->slevel / 11);

	kind->cost = exponentiate(spell->slevel, 3, 1) * 100;
	kind->name = string_make(kind->name);
	kind->name = string_append(kind->name, " of ");
	kind->name = string_append(kind->name, spell->name);
}

static void write_spellbook_kinds(void)
{
	struct object_base *base = &kb_info[TV_BOOK];
	struct object_kind *book, *base_book = lookup_kind(TV_BOOK, 1);
	struct player_spell *spell;
	int i, prev_svals = base->num_svals, prev_k_max = z_info->k_max;

	assert(base_book);

	z_info->k_max += z_info->spell_max;

	k_info = mem_realloc(k_info, sizeof *k_info * z_info->k_max);

	for (i = 0, spell = spells; i < z_info->spell_max && spell; ++i, spell = spell->next) {
		book = &k_info[i + prev_k_max];

		memcpy(book, base_book, sizeof *book);

		book->base = base;
		book->kidx = i + prev_k_max;
		book->sval = prev_svals + i;

		k_info[i + prev_k_max - 1].next = book;
		book->next = NULL;
		
		book->text = NULL;
		book->brands = NULL;
		book->slays = NULL;
		book->curses = NULL;
		book->effect_msg = NULL;
		book->vis_msg = NULL;
		book->flavor = NULL;

		spell_to_spellbook(spell, book);
	}
}

errr grab_effect_data(struct parser *p, struct effect *effect)
{
	const char *type;
	int val;

	if (grab_name("effect", parser_getsym(p, "eff"), effect_list,
				  N_ELEMENTS(effect_list), &val))
		return PARSE_ERROR_INVALID_EFFECT;
	effect->index = val;

	if (parser_hasval(p, "type")) {
		type = parser_getsym(p, "type");

		if (type == NULL)
			return PARSE_ERROR_UNRECOGNISED_PARAMETER;

		/* Check for a value */
		val = effect_subtype(effect->index, type);

		if (val < 0) {
			effect->subtype_temp = string_make(type);
			effect->subtype = -1;
		} else {
			effect->subtype = val;
		}
	}

	if (parser_hasval(p, "radius"))
		effect->radius = parser_getint(p, "radius");

	if (parser_hasval(p, "other"))
		effect->other = parser_getint(p, "other");

	return PARSE_ERROR_NONE;
}

/**
 * Find the default paths to all of our important sub-directories.
 *
 * All of the sub-directories should, for a single-user install, be
 * located inside the main directory, whose location is very system-dependent.
 * For shared installations, typically on Unix or Linux systems, the
 * directories may be scattered - see config.h for more info.
 *
 * This function takes buffers, holding the paths to the "config", "lib",
 * and "data" directories (for example, those could be "/etc/angband/",
 * "/usr/share/angband", and "/var/games/angband").  Some system-dependent
 * expansion/substitution may be done when copying those base paths to the
 * paths Angband uses:  see path_process() in z-file.c for details (Unix
 * implementations, for instance, try to replace a leading ~ or ~username with
 * the path to a home directory).
 *
 * Various command line options may allow some of the important
 * directories to be changed to user-specified directories, most
 * importantly, the "scores" and "user" and "save" directories,
 * but this is done after this function, see "main.c".
 *
 * In general, the initial path should end in the appropriate "PATH_SEP"
 * string.  All of the "sub-directory" paths (created below or supplied
 * by the user) will NOT end in the "PATH_SEP" string, see the special
 * "path_build()" function in "util.c" for more information.
 *
 * Hack -- first we free all the strings, since this is known
 * to succeed even if the strings have not been allocated yet,
 * as long as the variables start out as "NULL".  This allows
 * this function to be called multiple times, for example, to
 * try several base "path" values until a good one is found.
 */
void init_file_paths(const char *configpath, const char *libpath, const char *datapath)
{
	char buf[1024];
	char *userpath = NULL;

	/*** Free everything ***/

	/* Free the sub-paths */
	string_free(ANGBAND_DIR_GAMEDATA);
	string_free(ANGBAND_DIR_CUSTOMIZE);
	string_free(ANGBAND_DIR_HELP);
	string_free(ANGBAND_DIR_SCREENS);
	string_free(ANGBAND_DIR_FONTS);
	string_free(ANGBAND_DIR_TILES);
	string_free(ANGBAND_DIR_SOUNDS);
	string_free(ANGBAND_DIR_ICONS);
	string_free(ANGBAND_DIR_USER);
	string_free(ANGBAND_DIR_SAVE);
	string_free(ANGBAND_DIR_PANIC);
	string_free(ANGBAND_DIR_SCORES);
	string_free(ANGBAND_DIR_ARCHIVE);

	/*** Prepare the paths ***/

#define BUILD_DIRECTORY_PATH(dest, basepath, dirname) { \
	path_build(buf, sizeof(buf), (basepath), (dirname)); \
	dest = string_make(buf); \
}

	/* Paths generally containing configuration data for Angband. */
#ifdef GAMEDATA_IN_LIB
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, libpath, "gamedata");
#else
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, configpath, "gamedata");
#endif
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_CUSTOMIZE, configpath, "customize");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_HELP, libpath, "help");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCREENS, libpath, "screens");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_FONTS, libpath, "fonts");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_TILES, libpath, "tiles");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SOUNDS, libpath, "sounds");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_ICONS, libpath, "icons");

#ifdef PRIVATE_USER_PATH

	/* Build the path to the user specific directory */
	if (strncmp(ANGBAND_SYS, "test", 4) == 0)
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, "Test");
	else
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, VERSION_NAME);
	ANGBAND_DIR_USER = string_make(buf);

#else /* !PRIVATE_USER_PATH */

#ifdef MACH_O_CARBON
	/* Remove any trailing separators, since some deeper path creation functions
	 * don't like directories with trailing slashes. */
	if (suffix(datapath, PATH_SEP)) {
		/* Hacky way to trim the separator. Since this is just for OS X, we can
		 * assume a one char separator. */
		int last_char_index = strlen(datapath) - 1;
		my_strcpy(buf, datapath, sizeof(buf));
		buf[last_char_index] = '\0';
		ANGBAND_DIR_USER = string_make(buf);
	}
	else {
		ANGBAND_DIR_USER = string_make(datapath);
	}
#else /* !MACH_O_CARBON */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_USER, datapath, "user");
#endif /* MACH_O_CARBON */

#endif /* PRIVATE_USER_PATH */

	/* Build the path to the archive directory. */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_ARCHIVE, ANGBAND_DIR_USER, "archive");

#ifdef USE_PRIVATE_PATHS
	userpath = ANGBAND_DIR_USER;
#else /* !USE_PRIVATE_PATHS */
	userpath = (char *)datapath;
#endif /* USE_PRIVATE_PATHS */

	/* Build the path to the score and save directories */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCORES, userpath, "scores");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SAVE, userpath, "save");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_PANIC, userpath, "panic");

#undef BUILD_DIRECTORY_PATH
}


/**
 * Create any missing directories. We create only those dirs which may be
 * empty (user/, save/, scores/, info/, help/). The others are assumed
 * to contain required files and therefore must exist at startup
 * (edit/, pref/, file/, xtra/).
 *
 * ToDo: Only create the directories when actually writing files.
 */
void create_needed_dirs(void)
{
	char dirpath[512];

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_USER, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SAVE, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_PANIC, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SCORES, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_ARCHIVE, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);
}

/**
 * ------------------------------------------------------------------------
 * Initialize game constants
 * ------------------------------------------------------------------------ */

static enum parser_error parse_constants_level_max(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "monsters"))
		z->level_monster_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_mon_gen(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "chance"))
		z->alloc_monster_chance = value;
	else if (streq(label, "level-min"))
		z->level_monster_min = value;
	else if (streq(label, "town-day"))
		z->town_monsters_day = value;
	else if (streq(label, "town-night"))
		z->town_monsters_night = value;
	else if (streq(label, "repro-max"))
		z->repro_monster_max = value;
	else if (streq(label, "ood-chance"))
		z->ood_monster_chance = value;
	else if (streq(label, "ood-amount"))
		z->ood_monster_amount = value;
	else if (streq(label, "group-max"))
		z->monster_group_max = value;
	else if (streq(label, "group-dist"))
		z->monster_group_dist = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_mon_play(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "break-glyph"))
		z->glyph_hardness = value;
	else if (streq(label, "mult-rate"))
		z->repro_monster_rate = value;
	else if (streq(label, "life-drain"))
		z->life_drain_percent = value;
	else if (streq(label, "flee-range"))
		z->flee_range = value;
	else if (streq(label, "turn-range"))
		z->turn_range = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_dun_gen(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "cent-max"))
		z->level_room_max = value;
	else if (streq(label, "door-max"))
		z->level_door_max = value;
	else if (streq(label, "wall-max"))
		z->wall_pierce_max = value;
	else if (streq(label, "tunn-max"))
		z->tunn_grid_max = value;
	else if (streq(label, "amt-room"))
		z->room_item_av = value;
	else if (streq(label, "amt-item"))
		z->both_item_av = value;
	else if (streq(label, "amt-gold"))
		z->both_gold_av = value;
	else if (streq(label, "pit-max"))
		z->level_pit_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_world(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-depth"))
		z->max_depth = value;
	else if (streq(label, "day-length"))
		z->day_length = value;
	else if (streq(label, "dungeon-hgt"))
		z->dungeon_hgt = value;
	else if (streq(label, "dungeon-wid"))
		z->dungeon_wid = value;
	else if (streq(label, "town-hgt"))
		z->town_hgt = value;
	else if (streq(label, "town-wid"))
		z->town_wid = value;
	else if (streq(label, "feeling-total"))
		z->feeling_total = value;
	else if (streq(label, "feeling-need"))
		z->feeling_need = value;
	else if (streq(label, "stair-skip"))
		z->stair_skip = value;
	else if (streq(label, "move-energy"))
		z->move_energy = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_carry_cap(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "pack-size"))
		z->pack_size = value;
	else if (streq(label, "quiver-size"))
		z->quiver_size = value;
	else if (streq(label, "quiver-slot-size"))
		z->quiver_slot_size = value;
	else if (streq(label, "thrown-quiver-mult"))
		z->thrown_quiver_mult = value;
	else if (streq(label, "floor-size"))
		z->floor_size = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_store(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "inven-max"))
		z->store_inven_max = value;
	else if (streq(label, "turns"))
		z->store_turns = value;
	else if (streq(label, "shuffle"))
		z->store_shuffle = value;
	else if (streq(label, "magic-level"))
		z->store_magic_level = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_obj_make(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-depth"))
		z->max_obj_depth = value;
	else if (streq(label, "great-obj"))
		z->great_obj = value;
	else if (streq(label, "great-ego"))
		z->great_ego = value;
	else if (streq(label, "fuel-torch"))
		z->fuel_torch = value;
	else if (streq(label, "fuel-lamp"))
		z->fuel_lamp = value;
	else if (streq(label, "default-lamp"))
		z->default_lamp = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_player(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-sight"))
		z->max_sight = value;
	else if (streq(label, "max-range"))
		z->max_range = value;
	else if (streq(label, "start-gold"))
		z->start_gold = value;
	else if (streq(label, "food-value"))
		z->food_value = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_melee_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->m_crit_debuff_toh = value;
	} else if (streq(label, "chance-weight-scale")) {
		z->m_crit_chance_weight_scl = value;
	} else if (streq(label, "chance-toh-scale")) {
		z->m_crit_chance_toh_scl = value;
	} else if (streq(label, "chance-level-scale")) {
		z->m_crit_chance_level_scl = value;
	} else if (streq(label, "chance-toh-skill-scale")) {
		z->m_crit_chance_toh_skill_scl = value;
	} else if (streq(label, "chance-offset")) {
		z->m_crit_chance_offset = value;
	} else if (streq(label, "chance-range")) {
		z->m_crit_chance_range = value;
	} else if (streq(label, "power-weight-scale")) {
		z->m_crit_power_weight_scl = value;
	} else if (streq(label, "power-random")) {
		z->m_crit_power_random = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_melee_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct critical_level *new_level;
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}

	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->cutoff = parser_getint(p, "cutoff");
	new_level->dice = parser_getint(p, "dice");
	new_level->add = parser_getint(p, "add");
	new_level->msgt = msgt;

	/* Add it to the end of the linked list. */
	if (z->m_crit_level_head) {
		struct critical_level *cursor = z->m_crit_level_head;

		while (cursor->next && cursor->cutoff < new_level->cutoff) {
			cursor = cursor->next;
		}

		cursor->next = new_level;
	} else {
		z->m_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_ranged_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->r_crit_debuff_toh = value;
	} else if (streq(label, "chance-weight-scale")) {
		z->r_crit_chance_weight_scl = value;
	} else if (streq(label, "chance-toh-scale")) {
		z->r_crit_chance_toh_scl = value;
	} else if (streq(label, "chance-level-scale")) {
		z->r_crit_chance_level_scl = value;
	} else if (streq(label, "chance-launched-toh-skill-scale")) {
		z->r_crit_chance_launched_toh_skill_scl = value;
	} else if (streq(label, "chance-thrown-toh-skill-scale")) {
		z->r_crit_chance_thrown_toh_skill_scl = value;
	} else if (streq(label, "chance-offset")) {
		z->r_crit_chance_offset = value;
	} else if (streq(label, "chance-range")) {
		z->r_crit_chance_range = value;
	} else if (streq(label, "power-weight-scale")) {
		z->r_crit_power_weight_scl = value;
	} else if (streq(label, "power-random")) {
		z->r_crit_power_random = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_ranged_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct critical_level *new_level;
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->cutoff = parser_getint(p, "cutoff");
	new_level->dice = parser_getint(p, "dice");
	new_level->add = parser_getint(p, "add");
	new_level->msgt = msgt;
	/* Add it to the end of the linked list. */
	if (z->r_crit_level_head) {
		struct critical_level *cursor = z->r_crit_level_head;

		while (cursor->next) {
			cursor = cursor->next;
		}
		cursor->next = new_level;
	} else {
		z->r_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_melee_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->o_m_crit_debuff_toh = value;
	} else if (streq(label, "power-toh-scale-numerator")) {
		z->o_m_crit_power_toh_scl_num = value;
	} else if (streq(label, "power-toh-scale-denominator")) {
		z->o_m_crit_power_toh_scl_den = value;
	} else if (streq(label, "chance-power-scale-numerator")) {
		z->o_m_crit_chance_power_scl_num = value;
	} else if (streq(label, "chance-power-scale-denominator")) {
		z->o_m_crit_chance_power_scl_den = value;
	} else if (streq(label, "chance-add-denominator")) {
		z->o_m_crit_chance_add_den = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_melee_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct o_critical_level *new_level;
	unsigned int chance = parser_getuint(p, "chance");
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (chance == 0) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->chance = chance;
	new_level->added_dice = parser_getuint(p, "dice");
	new_level->msgt = msgt;
	/* Add it to the end of the linked list. */
	if (z->o_m_crit_level_head) {
		struct o_critical_level *cursor = z->o_m_crit_level_head;

		while (cursor->next) {
			cursor = cursor->next;
		}
		cursor->next = new_level;
	} else {
		z->o_m_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_ranged_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->o_r_crit_debuff_toh = value;
	} else if (streq(label, "power-launched-toh-scale-numerator")) {
		z->o_r_crit_power_launched_toh_scl_num = value;
	} else if (streq(label, "power-launched-toh-scale-denominator")) {
		z->o_r_crit_power_launched_toh_scl_den = value;
	} else if (streq(label, "power-thrown-toh-scale-numerator")) {
		z->o_r_crit_power_thrown_toh_scl_num = value;
	} else if (streq(label, "power-thrown-toh-scale-denominator")) {
		z->o_r_crit_power_thrown_toh_scl_den = value;
	} else if (streq(label, "chance-power-scale-numerator")) {
		z->o_r_crit_chance_power_scl_num = value;
	} else if (streq(label, "chance-power-scale-denominator")) {
		z->o_r_crit_chance_power_scl_den = value;
	} else if (streq(label, "chance-add-denominator")) {
		z->o_r_crit_chance_add_den = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_ranged_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct o_critical_level *new_level;
	unsigned int chance = parser_getuint(p, "chance");
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (chance == 0) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->chance = chance;
	new_level->added_dice = parser_getuint(p, "dice");
	new_level->msgt = msgt;
	/* Add it to the end of the linked list. */
	if (z->o_r_crit_level_head) {
		struct o_critical_level *cursor = z->o_r_crit_level_head;

		while (cursor->next) {
			cursor = cursor->next;
		}
		cursor->next = new_level;
	} else {
		z->o_r_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_constants(void) {
	struct angband_constants *z = mem_zalloc(sizeof *z);
	struct parser *p = parser_new();

	parser_setpriv(p, z);
	parser_reg(p, "level-max sym label int value", parse_constants_level_max);
	parser_reg(p, "mon-gen sym label int value", parse_constants_mon_gen);
	parser_reg(p, "mon-play sym label int value", parse_constants_mon_play);
	parser_reg(p, "dun-gen sym label int value", parse_constants_dun_gen);
	parser_reg(p, "world sym label int value", parse_constants_world);
	parser_reg(p, "carry-cap sym label int value", parse_constants_carry_cap);
	parser_reg(p, "store sym label int value", parse_constants_store);
	parser_reg(p, "obj-make sym label int value", parse_constants_obj_make);
	parser_reg(p, "player sym label int value", parse_constants_player);
	parser_reg(p, "melee-critical sym label int value",
		parse_constants_melee_critical);
	parser_reg(p, "melee-critical-level int cutoff int dice int add "
		"str msg", parse_constants_melee_critical_level);
	parser_reg(p, "ranged-critical sym label int value",
		parse_constants_ranged_critical);
	parser_reg(p, "ranged-critical-level int cutoff int dice int add "
		"str msg", parse_constants_ranged_critical_level);
	parser_reg(p, "o-melee-critical sym label int value",
		parse_constants_o_melee_critical);
	parser_reg(p, "o-melee-critical-level uint chance uint dice str msg",
		parse_constants_o_melee_critical_level);
	parser_reg(p, "o-ranged-critical sym label int value",
		parse_constants_o_ranged_critical);
	parser_reg(p, "o-ranged-critical-level uint chance uint dice str msg",
		parse_constants_o_ranged_critical_level);
	return p;
}

static errr run_parse_constants(struct parser *p) {
	return parse_file_quit_not_found(p, "constants");
}

static int check_critical_levels(const struct critical_level *head)
{
	/*
	 * Reject if the cutoffs, except for the last one which is unused, do
	 * not strictly increase.
	 */
	if (!head) {
		return 0;
	}
	while (head->next) {
		int prev_cutoff = head->cutoff;

		head = head->next;
		if (head->next && head->cutoff <= prev_cutoff) {
			return 1;
		}
	}
	return 0;
}

static errr finish_parse_constants(struct parser *p) {
	z_info = parser_priv(p);
	parser_destroy(p);
	if (check_critical_levels(z_info->m_crit_level_head)) {
		plog("The cutoffs for melee criticals in constants.txt are "
			"not strictly increasing.");
		return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
	}
	if (check_critical_levels(z_info->r_crit_level_head)) {
		plog("The cutoffs for ranged criticals in constants.txt are "
			"not strictly increasing.");
		return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
	}
	return 0;
}

static void cleanup_critical_levels(struct critical_level *head)
{
	while (head) {
		struct critical_level *target = head;

		head = head->next;
		mem_free(target);
	}
}

static void cleanup_o_critical_levels(struct o_critical_level *head)
{
	while (head) {
		struct o_critical_level *target = head;

		head = head->next;
		mem_free(target);
	}
}

static void cleanup_constants(void)
{
	cleanup_critical_levels(z_info->m_crit_level_head);
	cleanup_critical_levels(z_info->r_crit_level_head);
	cleanup_o_critical_levels(z_info->o_m_crit_level_head);
	cleanup_o_critical_levels(z_info->o_r_crit_level_head);
	mem_free(z_info);
}

struct file_parser constants_parser = {
	"constants",
	init_parse_constants,
	run_parse_constants,
	finish_parse_constants,
	cleanup_constants
};

/**
 * Initialize game constants.
 *
 * Assumption: Paths are set up correctly before calling this function.
 */
void init_game_constants(void)
{
	event_signal_message(EVENT_INITSTATUS, 0, "Initializing constants");
	if (run_parser(&constants_parser))
		quit_fmt("Cannot initialize constants.");
}

/**
 * Free the game constants
 */
static void cleanup_game_constants(void)
{
	cleanup_parser(&constants_parser);
}

/**
 * ------------------------------------------------------------------------
 * Intialize world map
 * ------------------------------------------------------------------------ */
static enum parser_error parse_world_level(struct parser *p) {
	const int depth = parser_getint(p, "depth");
	const char *name = parser_getsym(p, "name");
	const char *up = parser_getsym(p, "up");
	const char *down = parser_getsym(p, "down");
	struct level *last = parser_priv(p);
	struct level *lev = mem_zalloc(sizeof *lev);

	if (last) {
		last->next = lev;
	} else {
		world = lev;
	}
	lev->depth = depth;
	lev->name = string_make(name);
	lev->up = streq(up, "None") ? NULL : string_make(up);
	lev->down = streq(down, "None") ? NULL : string_make(down);
	parser_setpriv(p, lev);
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_world(void) {
	struct parser *p = parser_new();

	parser_reg(p, "level int depth sym name sym up sym down",
			   parse_world_level);
	return p;
}

static errr run_parse_world(struct parser *p) {
	return parse_file_quit_not_found(p, "world");
}

static errr finish_parse_world(struct parser *p) {
	struct level *level_check;

	/* Check that all levels referred to exist */
	for (level_check = world; level_check; level_check = level_check->next) {
		struct level *level_find = world;

		/* Check upwards */
		if (level_check->up) {
			while (level_find && !streq(level_check->up, level_find->name)) {
				level_find = level_find->next;
			}
			if (!level_find) {
				quit_fmt("Invalid level reference %s", level_check->up);
			}
		}

		/* Check downwards */
		level_find = world;
		if (level_check->down) {
			while (level_find && !streq(level_check->down, level_find->name)) {
				level_find = level_find->next;
			}
			if (!level_find) {
				quit_fmt("Invalid level reference %s", level_check->down);
			}
		}
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_world(void)
{
	struct level *level = world;
	while (level) {
		struct level *old = level;
		string_free(level->name);
		string_free(level->up);
		string_free(level->down);
		level = level->next;
		mem_free(old);
	}
}

struct file_parser world_parser = {
	"world",
	init_parse_world,
	run_parse_world,
	finish_parse_world,
	cleanup_world
};


/**
 * ------------------------------------------------------------------------
 * Initialize player properties
 * ------------------------------------------------------------------------ */
/*
 * Keep track of UI entries to be bound to an ability while parsing.  Bind them
 * at the end of parsing and don't pass them along to the stored player_ability
 * structures.
 */
struct player_bound_ui {
	char *name;
	struct player_bound_ui *next;
	int value;
	bool isaux;
	bool isspecial;
};

struct embryo_player_ability {
	struct player_ability ability;
	struct player_bound_ui *boundui;
	struct embryo_player_ability *next;
	int parent_type[MAX_ABIL_PARENTS];
	int parent[MAX_ABIL_PARENTS];
	int subprop_type;
};

static struct embryo_player_ability  *embryo_player_abilities = NULL;

static int abil_type_by_name(const char *name)
{
	if (streq(name, "power")) {
		return PY_ABIL_POWER;
	} else if (streq(name, "skill")) {
		return PY_ABIL_SKILL;
	} else if (streq(name, "player")) {
		return PY_ABIL_PLAYER;
	} else if (streq(name, "object")) {
		return PY_ABIL_OBJECT;
	} else if (streq(name, "element")) {
		return PY_ABIL_ELEMENT;
	}

	return -1;
}

static enum parser_error parse_player_prop_type(struct parser *p) {
	char *type = string_make(parser_getstr(p, "type"));
	struct embryo_player_ability *h = parser_priv(p);
	struct embryo_player_ability *embryo = mem_zalloc(sizeof *embryo);
	int i;

	if (h) {
		h->next = embryo;
	} else {
		embryo_player_abilities = embryo;
	}
	parser_setpriv(p, embryo);

	embryo->ability.type = abil_type_by_name(type);

	string_free(type);

	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		embryo->parent_type[i] = -1;
		embryo->parent[i] = -1;
	}

	embryo->ability.scale_num = 2;
	embryo->ability.scale_den = 3;

	embryo->subprop_type = SUBPROP_TYP_NONE;

	if (embryo->ability.type < 0) {
		return PARSE_ERROR_GENERIC;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	struct embryo_player_ability *embryo = parser_priv(p);
	int index = -1;

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (embryo->ability.type < 0) {
		return PARSE_ERROR_MISSING_PLAY_PROP_TYPE;
	}

	if (embryo->ability.type == PY_ABIL_PLAYER) {
		index = code_index_in_array(player_info_flags, code);
	} else if (embryo->ability.type == PY_ABIL_OBJECT) {
		index = code_index_in_array(list_obj_flag_names, code);
	} else if (embryo->ability.type == PY_ABIL_POWER) {
		index = code_index_in_array(list_player_powers_names, code);
	} else if (embryo->ability.type == PY_ABIL_SKILL) {
		index = code_index_in_array(list_player_skill_names, code);
	}
	if (index >= 0) {
		embryo->ability.index = index;
	} else {
		return PARSE_ERROR_INVALID_PLAY_PROP_CODE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_desc(struct parser *p) {
	struct embryo_player_ability *embryo = parser_priv(p);
	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	embryo->ability.desc = string_append(embryo->ability.desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_name(struct parser *p) {
	const char *desc = parser_getstr(p, "desc");
	struct embryo_player_ability *embryo = parser_priv(p);

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(embryo->ability.name);
	embryo->ability.name = string_make(desc);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_value(struct parser *p) {
	struct embryo_player_ability *embryo = parser_priv(p);
	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	embryo->ability.value = parser_getint(p, "value");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_bindui(struct parser *p) {
	const char *name = parser_getsym(p, "ui");
	const char *value = parser_getsym(p, "uival");
	bool isaux = (parser_getint(p, "aux") != 0);
	struct embryo_player_ability *embryo = parser_priv(p);
	struct player_bound_ui *boundui;
	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	boundui = mem_alloc(sizeof(*boundui));
	boundui->name = string_make(name);
	if (streq(value, "special")) {
		boundui->value = 0;
		boundui->isspecial = true;
	} else {
		long v;
		char* end;

		v = strtol(value, &end, 10);
		if (! *value || *end) {
			string_free(boundui->name);
			mem_free(boundui);
			return PARSE_ERROR_NOT_NUMBER;
		}
		/*
		 * Also reject INT_MIN and INT_MAX so we don't have to check
		 * errno to detect out of range values on platforms where
		 * sizeof(int) == sizeof(long).
		 */
		if (v <= INT_MIN || v >= INT_MAX) {
			string_free(boundui->name);
			mem_free(boundui);
			return PARSE_ERROR_INVALID_VALUE;
		}
		boundui->value = (int) v;
		boundui->isspecial = false;
	}
	boundui->isaux = isaux;
	boundui->next = embryo->boundui;
	embryo->boundui = boundui;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_cost(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	int cost = parser_getint(p, "cost");
	
	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	embryo->ability.cost = cost;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_rarity(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	int rarity = parser_getint(p, "rarity");
	
	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	embryo->ability.rarity = rarity;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_scale(struct parser *p) {
	struct embryo_player_ability *embryo = parser_priv(p);
	int num, den;

	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	num = parser_getint(p, "numerator");
	den = parser_getint(p, "denominator");

	if (num <= 0 || den <= 0) {
		return PARSE_ERROR_GENERIC;
	}

	embryo->ability.scale_num = num;
	embryo->ability.scale_den = den;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_initial(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	int initial = parser_getint(p, "initial");
	
	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	embryo->ability.initial = initial;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_parent(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	int parent_type = abil_type_by_name(parser_getsym(p, "parent-type"));
	const char *parent_code = parser_getsym(p, "parent-code");
	int parent;
	int i;
	
	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (parent_type < 0) {
		return PARSE_ERROR_GENERIC;
	}

	if (!parent_code) {
		return PARSE_ERROR_GENERIC;
	} else if (parent_type == PY_ABIL_POWER) {
		parent = code_index_in_array(list_player_powers_names, parent_code);
	} else if (parent_type == PY_ABIL_SKILL) {
		parent = code_index_in_array(list_player_skill_names, parent_code);
	} else {
		return PARSE_ERROR_GENERIC;
	}

	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		if (embryo->parent[i] < 0) {
			embryo->parent[i] = parent;
			embryo->parent_type[i] = parent_type;
			break;
		}
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_subprop(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	const char *subprop_name = parser_getsym(p, "id");
	int sub_typ_id = code_index_in_array(list_subprop_type_names, subprop_name);
	
	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	if (sub_typ_id < 0) {
		return PARSE_ERROR_GENERIC;
	}

	embryo->subprop_type = sub_typ_id;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_prereq(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	const char *prereq_name = parser_getsym(p, "id");
	int prereq_id = code_index_in_array(ability_predicate_names, prereq_name);

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (prereq_id < 0 || prereq_id >= ABIL_PRED_MAX) {
		return PARSE_ERROR_GENERIC;
	}

	embryo->ability.prereqs[prereq_id] = true;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_verb_second(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	const char *verb_second_name = parser_getstr(p, "verb");

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	string_free(embryo->ability.second_verb);

	embryo->ability.second_verb = string_make(verb_second_name);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_verb_third(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	const char *verb_third_name = parser_getstr(p, "verb");

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	string_free(embryo->ability.third_verb);

	embryo->ability.third_verb = string_make(verb_third_name);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_adj_pos(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	const char *adj_pos_name = parser_getstr(p, "adj");

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	string_free(embryo->ability.pos_adjective);

	embryo->ability.pos_adjective = string_make(adj_pos_name);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_adj_neg(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	const char *adj_neg_name = parser_getstr(p, "adj");

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	string_free(embryo->ability.neg_adjective);

	embryo->ability.neg_adjective = string_make(adj_neg_name);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_comment(struct parser *p)
{
	struct embryo_player_ability *embryo = parser_priv(p);
	const char *comment = parser_getstr(p, "comment");

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	string_free(embryo->ability.comment);

	embryo->ability.comment = string_make(comment);

	return PARSE_ERROR_NONE;
}


static struct parser *init_parse_player_prop(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "type str type", parse_player_prop_type);
	parser_reg(p, "code str code", parse_player_prop_code);
	parser_reg(p, "desc str desc", parse_player_prop_desc);
	parser_reg(p, "name str desc", parse_player_prop_name);
	parser_reg(p, "value int value", parse_player_prop_value);
	parser_reg(p, "bindui sym ui int aux sym uival", parse_player_prop_bindui);
	parser_reg(p, "cost int cost", parse_player_prop_cost);
	parser_reg(p, "rarity int rarity", parse_player_prop_rarity);
	parser_reg(p, "initial int initial", parse_player_prop_initial);
	parser_reg(p, "scale int numerator int denominator", parse_player_prop_scale);
	parser_reg(p, "parent sym parent-type sym parent-code", parse_player_prop_parent);
	parser_reg(p, "subprop sym id", parse_player_prop_subprop);
	parser_reg(p, "prereq sym id", parse_player_prop_prereq);
	parser_reg(p, "verb-second str verb", parse_player_prop_verb_second);
	parser_reg(p, "verb-third str verb", parse_player_prop_verb_third);
	parser_reg(p, "adj-positive str adj", parse_player_prop_adj_pos);
	parser_reg(p, "adj-negative str adj", parse_player_prop_adj_neg);
	parser_reg(p, "comment str comment", parse_player_prop_comment);
	return p;
}

static errr run_parse_player_prop(struct parser *p) {
	return parse_file_quit_not_found(p, "player_property");
}

static struct player_ability *player_prop_lookup(int type, int id)
{
	struct player_ability *prop;

	for (prop = player_abilities; prop; prop = prop->next) {
		if (prop->index == id && prop->type == type) {
			return prop;
		}
	}

	return NULL;
}

static bool power_parents_loop(const struct player_ability *curr, int **array, size_t *array_len, size_t *first_free)
{
	uint16_t i;
	const struct player_ability *parent;

	// check whether we've looped so far
	for (i = 0; i < *first_free; ++i) {
		assert((*array)[i] > PP_NONE && (*array)[i] < PP_MAX);
		if ((*array)[i] == curr->index) {
			return true;
		}
	}

	// if we're out of space extend the array
	if (*first_free >= *array_len) {
		*array_len += 8;
		(*array) = mem_realloc(*array, (sizeof **array) * (*array_len));
		// don't need to initialize cause first available is tracked
	}

	(*array)[*first_free] = curr->index;
	(*first_free)++;

	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		parent = curr->parent[i];

		if (!parent) continue;
		if (parent->type != PY_ABIL_POWER) continue;
		if (power_parents_loop(parent, array, array_len, first_free)) return true;
	}

	(*first_free)--;

	return false;
}

static bool looping_power_parents(void)
{
	const struct player_ability *abil;
	int *array = mem_zalloc(sizeof *array * 16U);
	bool checked[PP_MAX] = { false };
	size_t len = 16U, first = 0, i, j;
	bool loop = false;

	for (i = 0; i < PP_MAX && !loop; ++i) {
		abil = lookup_player_ability(i, PY_ABIL_POWER);

		if (!abil) continue;
		if (checked[i]) continue;

		first = 0;

		if (power_parents_loop(abil, &array, &len, &first)) {
			loop = true;
		}

		for (j = 0; j < first; ++j) {
			assert(array[j] < PP_MAX && array[j] >= 0);
			checked[array[j]] = true;
		}
	}

	mem_free(array);

	return loop;
}

static errr finish_parse_player_prop(struct parser *p) {
	struct embryo_player_ability *embryo = embryo_player_abilities, *next;
	struct player_bound_ui *boundui_cursor;
	struct player_ability *new, *previous = NULL;
	int err = 0, i;

	/* Copy abilities over, making multiple copies for element types */
	player_abilities = mem_zalloc(sizeof(*player_abilities));
	new = player_abilities;
	while (embryo) {
		if (embryo->ability.type == PY_ABIL_ELEMENT) {
			uint16_t ui, n;
			assert(N_ELEMENTS(list_element_names) < 65536);
			n = (uint16_t) N_ELEMENTS(list_element_names);
			for (ui = 0; ui < n - 1; ui++) {
				char *name = string_make(projections[ui].name);
				new->index = ui;
				new->type = embryo->ability.type;
				new->desc = string_make(format("%s %s.", embryo->ability.desc, name));
				my_strcap(name);
				new->name = string_make(format("%s %s", name, embryo->ability.name));
				string_free(name);
				new->value = embryo->ability.value;
				boundui_cursor = embryo->boundui;
				while (boundui_cursor) {
					name = string_make(format("%s<%s>", boundui_cursor->name, list_element_names[ui]));
					(void) bind_player_ability_to_ui_entry_by_name(name, new, boundui_cursor->value, !boundui_cursor->isspecial, boundui_cursor->isaux);
					string_free(name);
					boundui_cursor = boundui_cursor->next;
				}
				if ((ui != n - 2) || embryo->next) {
					previous = new;
					new = mem_zalloc(sizeof(*new));
					previous->next = new;
				}
			}
			string_free(embryo->ability.desc);
			string_free(embryo->ability.name);
			while (embryo->boundui) {
				boundui_cursor = embryo->boundui;
				embryo->boundui = embryo->boundui->next;
				string_free(boundui_cursor->name);
				mem_free(boundui_cursor);
			}
		} else if (embryo->subprop_type == SUBPROP_TYP_NONE) {
			memcpy(new, &embryo->ability, sizeof *new);

			new->sub_id = 0;

			if (!new->desc && (!new->second_verb || !new->third_verb)) {
				plog_fmt("Error: property %s does not have an acceptable description!", new->name);
				err = -1;
			}

			while (embryo->boundui) {
				boundui_cursor = embryo->boundui;
				embryo->boundui = embryo->boundui->next;
				(void) bind_player_ability_to_ui_entry_by_name(boundui_cursor->name, new, boundui_cursor->value, !boundui_cursor->isspecial, boundui_cursor->isaux);
				string_free(boundui_cursor->name);
				mem_free(boundui_cursor);
			}

			if (embryo->next) {
				previous = new;
				new = mem_zalloc(sizeof(*new));
				previous->next = new;
			}
		} else {
			int num_subprops = ability_subprop_max(embryo->subprop_type);

			for (i = 0; i < num_subprops; ++i) {
				if (!abil_subid_valid(i, embryo->subprop_type)) {
					continue;
				}

				memcpy(new, &embryo->ability, sizeof *new);
				new->sub_id = i;

				if (embryo->ability.comment) {
					new->comment = string_make(embryo->ability.comment);
				}
				if (embryo->ability.name) {
					new->name = ability_subprop_name(new, embryo->subprop_type);
				}
				if (embryo->ability.desc) {
					new->desc = string_make(embryo->ability.desc);
				}
				if (embryo->ability.second_verb) {
					new->second_verb = string_make(embryo->ability.second_verb);
				}
				if (embryo->ability.third_verb) {
					new->third_verb = string_make(embryo->ability.third_verb);
				}
				if (embryo->ability.pos_adjective) {
					new->pos_adjective = string_make(embryo->ability.pos_adjective);
				}
				if (embryo->ability.neg_adjective) {
					new->neg_adjective = string_make(embryo->ability.neg_adjective);
				}

				if (!new->desc && (!new->second_verb || !new->third_verb)) {
					plog_fmt("Error: property %s does not have an acceptable description!", new->name);
					err = -1;
				}

				while (embryo->boundui) {
					boundui_cursor = embryo->boundui;
					embryo->boundui = embryo->boundui->next;
					(void) bind_player_ability_to_ui_entry_by_name(boundui_cursor->name, new, boundui_cursor->value, !boundui_cursor->isspecial, boundui_cursor->isaux);
					string_free(boundui_cursor->name);
					mem_free(boundui_cursor);
				}

				previous = new;
				new = mem_zalloc(sizeof(*new));
				previous->next = new;
			}

			if (!embryo->next) {
				mem_free(new);
			}

			string_free(embryo->ability.comment);
			string_free(embryo->ability.name);
			string_free(embryo->ability.desc);
			string_free(embryo->ability.second_verb);
			string_free(embryo->ability.third_verb);
			string_free(embryo->ability.pos_adjective);
			string_free(embryo->ability.neg_adjective);
		}

		embryo = embryo->next;
	}

	// L: find parents
	for (embryo = embryo_player_abilities, next = embryo->next; embryo; embryo = next, next = embryo ? embryo->next : NULL) {
		struct player_ability *prop_base, *prop_parent;
		struct player_ability **parents = mem_zalloc(sizeof prop_base->parent);

		//prop_base = player_prop_lookup(embryo->ability.type, embryo->ability.index);

		for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
			int parent_type = embryo->parent_type[i];
			int parent = embryo->parent[i];
			//prop_base = player_prop_lookup(embryo->ability.type, embryo->ability.index);

			if (parent_type < 0) continue;

			//prop_base = player_prop_by_name(name);
			prop_parent = player_prop_lookup(parent_type, parent);

			assert(prop_parent);

			parents[i] = prop_parent;
		}

		for (prop_base = player_abilities; prop_base; prop_base = prop_base->next) {
			if (prop_base->type == embryo->ability.type && prop_base->index == embryo->ability.index) {
				memcpy(prop_base->parent, parents, sizeof prop_base->parent);
			}
		}

		mem_free(parents);
		mem_free(embryo);
	}

	if (looping_power_parents()) err = -1;

	embryo_player_abilities = NULL;

	assert(z_info);
	z_info->abil_id_max = 0;
	for (new = player_abilities; new; new = new->next) {
		new->id = z_info->abil_id_max++;
	}

	parser_destroy(p);
	return err;
}

static void cleanup_player_prop(void)
{
	struct player_ability *ability = player_abilities;
	while (ability) {
		struct player_ability *totrash = ability;
		ability = ability->next;

		string_free(totrash->desc);
		string_free(totrash->name);

		string_free(totrash->pos_adjective);
		string_free(totrash->neg_adjective);
		string_free(totrash->second_verb);
		string_free(totrash->third_verb);
		string_free(totrash->comment);

		mem_free(totrash);
	}
}

struct file_parser player_property_parser = {
	"player_property",
	init_parse_player_prop,
	run_parse_player_prop,
	finish_parse_player_prop,
	cleanup_player_prop
};

/**
 * ------------------------------------------------------------------------
 * Intialize random names
 * ------------------------------------------------------------------------ */

struct name {
	struct name *next;
	char *str;
};

struct names_parse {
	unsigned int section;
	unsigned int nnames[RANDNAME_NUM_TYPES];
	struct name *names[RANDNAME_NUM_TYPES];
};

static enum parser_error parse_names_section(struct parser *p) {
	unsigned int section = parser_getuint(p, "section");
	struct names_parse *s = parser_priv(p);

	if (section >= RANDNAME_NUM_TYPES) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	s->section = section;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_names_word(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct names_parse *s = parser_priv(p);
	struct name *ns = mem_zalloc(sizeof *ns);

	s->nnames[s->section]++;
	ns->next = s->names[s->section];
	ns->str = string_make(name);
	s->names[s->section] = ns;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_names(void) {
	struct parser *p = parser_new();
	struct names_parse *n = mem_zalloc(sizeof *n);
	n->section = 0;
	parser_setpriv(p, n);
	parser_reg(p, "section uint section", parse_names_section);
	parser_reg(p, "word str name", parse_names_word);
	return p;
}

static errr run_parse_names(struct parser *p) {
	return parse_file_quit_not_found(p, "names");
}

static errr finish_parse_names(struct parser *p) {
	int i;
	unsigned int j;
	struct names_parse *n = parser_priv(p);
	struct name *nm;
	name_sections = mem_zalloc(sizeof(char**) * RANDNAME_NUM_TYPES);
	for (i = 0; i < RANDNAME_NUM_TYPES; i++) {
		name_sections[i] = mem_alloc(sizeof(char*) * (n->nnames[i] + 1));
		for (nm = n->names[i], j = 0; nm && j < n->nnames[i]; nm = nm->next, j++) {
			name_sections[i][j] = nm->str;
		}
		name_sections[i][n->nnames[i]] = NULL;
		while (n->names[i]) {
			nm = n->names[i]->next;
			mem_free(n->names[i]);
			n->names[i] = nm;
		}
	}
	mem_free(n);
	parser_destroy(p);
	return 0;
}

static void cleanup_names(void)
{
	int i, j;
	for (i = 0; i < RANDNAME_NUM_TYPES; i++) {
		for (j = 0; name_sections[i][j]; j++) {
			string_free((char *)name_sections[i][j]);
		}
		mem_free(name_sections[i]);
	}
	mem_free(name_sections);
}

struct file_parser names_parser = {
	"names",
	init_parse_names,
	run_parse_names,
	finish_parse_names,
	cleanup_names
};

/**
 * ------------------------------------------------------------------------
 * Intialize traps
 * ------------------------------------------------------------------------ */

static enum parser_error parse_trap_name(struct parser *p) {
    const char *name = parser_getsym(p, "name");
    const char *desc = parser_getstr(p, "desc");
    struct trap_kind *h = parser_priv(p);

    struct trap_kind *t = mem_zalloc(sizeof *t);
    t->next = h;
    t->name = string_make(name);
	t->desc = string_make(desc);
    parser_setpriv(p, t);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_graphics(struct parser *p) {
    wchar_t glyph = parser_getchar(p, "glyph");
    const char *color = parser_getsym(p, "color");
    int attr = 0;
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->d_char = glyph;
    if (strlen(color) > 1)
		attr = color_text_to_attr(color);
    else
		attr = color_char_to_attr(color[0]);
    if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
    t->d_attr = attr;
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_appear(struct parser *p) {
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->rarity =  parser_getuint(p, "rarity");
    t->min_depth =  parser_getuint(p, "mindepth");
    t->max_num =  parser_getuint(p, "maxnum");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_visibility(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	dice_t *dice;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	dice = dice_new();
	if (!dice_parse_string(dice, parser_getstr(p, "visibility"))) {
		dice_free(dice);
		return PARSE_ERROR_NOT_RANDOM;
	}
	dice_random_value(dice, &t->power);
	dice_free(dice);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_flags(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	char *flags, *s;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "flags")) {
		return PARSE_ERROR_NONE;
	}
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(t->flags, TRF_SIZE, trap_flags, s)) {
			break;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_effect(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*new_effect));
	if (t->effect) {
		effect = t->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		t->effect = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_trap_effect_yx(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_dice(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	dice_t *dice;
	const char *string;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_expr(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}
	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_trap_effect_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*new_effect));
	if (t->effect_xtra) {
		effect = t->effect_xtra;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		t->effect_xtra = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_trap_effect_yx_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect_xtra;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_dice_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	dice_t *dice;
	const char *string;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect_xtra;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_expr_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect_xtra;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}

	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_trap_save_flags(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	char *s, *u;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	s = string_make(parser_getstr(p, "flags"));
	u = strtok(s, " |");
	while (u) {
		if (grab_flag(t->save_flags, OF_SIZE, list_obj_flag_names, u)) {
			break;
		}
		u = strtok(NULL, " |");
	}
	string_free(s);
	return u ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_desc(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->text = string_append(t->text, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg = string_append(t->msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_good(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg_good = string_append(t->msg_good, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_bad(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg_bad = string_append(t->msg_bad, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg_xtra = string_append(t->msg_xtra, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_trap(void) {
    struct parser *p = parser_new();
    parser_setpriv(p, NULL);
    parser_reg(p, "name sym name str desc", parse_trap_name);
    parser_reg(p, "graphics char glyph sym color", parse_trap_graphics);
    parser_reg(p, "appear uint rarity uint mindepth uint maxnum", parse_trap_appear);
    parser_reg(p, "visibility str visibility", parse_trap_visibility);
    parser_reg(p, "flags ?str flags", parse_trap_flags);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_trap_effect);
	parser_reg(p, "effect-yx int y int x", parse_trap_effect_yx);
	parser_reg(p, "dice str dice", parse_trap_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_trap_expr);
	parser_reg(p, "effect-xtra sym eff ?sym type ?int radius ?int other", parse_trap_effect_xtra);
	parser_reg(p, "effect-yx-xtra int y int x", parse_trap_effect_yx_xtra);
	parser_reg(p, "dice-xtra str dice", parse_trap_dice_xtra);
	parser_reg(p, "expr-xtra sym name sym base str expr", parse_trap_expr_xtra);
	parser_reg(p, "save str flags", parse_trap_save_flags);
	parser_reg(p, "desc str text", parse_trap_desc);
	parser_reg(p, "msg str text", parse_trap_msg);
	parser_reg(p, "msg-good str text", parse_trap_msg_good);
	parser_reg(p, "msg-bad str text", parse_trap_msg_bad);
	parser_reg(p, "msg-xtra str text", parse_trap_msg_xtra);
    return p;
}

static errr run_parse_trap(struct parser *p) {
    return parse_file_quit_not_found(p, "trap");
}

static errr finish_parse_trap(struct parser *p) {
	struct trap_kind *t, *n;
	int tidx;

	/* Scan the list for the max id */
	z_info->trap_max = 0;
	t = parser_priv(p);
	while (t) {
		z_info->trap_max++;
		t = t->next;
	}

	trap_info = mem_zalloc((z_info->trap_max + 1) * sizeof(*t));
	tidx = z_info->trap_max - 1;
    for (t = parser_priv(p); t; t = t->next, tidx--) {
		assert(tidx >= 0);

		memcpy(&trap_info[tidx], t, sizeof(*t));
		trap_info[tidx].tidx = tidx;
		if (tidx < z_info->trap_max - 1)
			trap_info[tidx].next = &trap_info[tidx + 1];
		else
			trap_info[tidx].next = NULL;
    }

    t = parser_priv(p);
    while (t) {
		n = t->next;
		mem_free(t);
		t = n;
    }

    parser_destroy(p);
    return 0;
}

static void cleanup_trap(void)
{
	int i;
	for (i = 0; i < z_info->trap_max; i++) {
		string_free(trap_info[i].name);
		mem_free(trap_info[i].text);
		string_free(trap_info[i].desc);
		string_free(trap_info[i].msg);
		string_free(trap_info[i].msg_good);
		string_free(trap_info[i].msg_bad);
		string_free(trap_info[i].msg_xtra);
		free_effect(trap_info[i].effect);
		free_effect(trap_info[i].effect_xtra);
	}
	mem_free(trap_info);
}

struct file_parser trap_parser = {
    "trap",
    init_parse_trap,
    run_parse_trap,
    finish_parse_trap,
    cleanup_trap
};

/**
 * ------------------------------------------------------------------------
 * Intialize terrain
 * ------------------------------------------------------------------------ */

static enum parser_error parse_feat_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	int idx = lookup_feat_code(code);
	struct feature_kind *f;

	if (idx < 0) {
		/*
		 * Of the existing parser errors, PARSE_ERROR_INVALID_VALUE
		 * could also be used; this matches what ui-prefs.c returns
		 * for an unknown feature code or name.
		 */
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	assert(idx < FEAT_MAX);
	f = &f_info[idx];

	f->fidx = idx;
	f->proj = -1;
	f->default_size = 100;

	parser_setpriv(p, f);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (f->name) {
		return PARSE_ERROR_REPEATED_DIRECTIVE;
	}
	f->name = string_make(name);

	if (!f->look_prefix) {
		if (is_a_vowel(f->name[0])) f->look_prefix = string_make("an");
		else f->look_prefix = string_make("a");
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_graphics(struct parser *p) {
	wchar_t glyph = parser_getchar(p, "glyph");
	const char *color = parser_getsym(p, "color");
	int attr = 0;
	struct feature_kind *f = parser_priv(p);

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	f->d_char = glyph;
	if (strlen(color) > 1)
		attr = color_text_to_attr(color);
	else
		attr = color_char_to_attr(color[0]);
	if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
	f->d_attr = attr;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_mimic(struct parser *p) {
	const char *mimic_name = parser_getstr(p, "feat");
	struct feature_kind *f = parser_priv(p);
	int mimic_idx;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Verify that it refers to a valid feature. */
	mimic_idx = lookup_feat_code(mimic_name);
	if (mimic_idx < 0) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	f->mimic = &f_info[mimic_idx];
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_priority(struct parser *p) {
	unsigned int priority = parser_getuint(p, "priority");
	struct feature_kind *f = parser_priv(p);

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	f->priority = priority;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_flags(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	char *flags, *s;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "flags")) {
		return PARSE_ERROR_NONE;
	}
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(f->flags, TF_SIZE, terrain_flags, s)) {
			break;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_digging(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	int dig_idx = parser_getint(p, "dig");

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (dig_idx < DIGGING_RUBBLE + 1 || dig_idx >= DIGGING_MAX + 1) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	f->dig = dig_idx;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_timeout(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	int timeout = parser_getint(p, "timeout");

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	f->timeout = timeout;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_desc(struct parser *p) {
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->desc = string_append(f->desc, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_walk_msg(struct parser *p) {
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->walk_msg = string_append(f->walk_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_run_msg(struct parser *p) {
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->run_msg = string_append(f->run_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_hurt_msg(struct parser *p) {
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->hurt_msg = string_append(f->hurt_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_die_msg(struct parser *p) {
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->die_msg = string_append(f->die_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_confused_msg(struct parser *p) {
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->confused_msg =
		string_append(f->confused_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_look_prefix(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	const char *prefix = parser_getstr(p, "text");

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	string_free(f->look_prefix);

	if (streq(prefix, "NONE")) f->look_prefix = string_make("");
	else f->look_prefix = string_make(prefix);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_look_in_preposition(struct parser *p) {
	struct feature_kind *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->look_in_preposition =
		string_append(f->look_in_preposition, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_resist_flag(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	int flag = lookup_flag(mon_race_flags, parser_getsym(p, "flag"));

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (flag == FLAG_END) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	f->resist_flag = flag;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_feat_produce(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	const char *name = parser_getsym(p, "name");
	int fidx = code_index_in_array(list_feat_names, name);
	int amt = parser_getint(p, "amount"), freq = parser_getint(p, "freq");

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (fidx >= FEAT_MAX || fidx <= FEAT_NONE) {
		return PARSE_ERROR_GENERIC;
	}

	f->feat_produce = fidx;
	f->feat_produce_frequency = freq;
	f->feat_produce_quantity = amt;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_proj(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	int type, range = 0;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	if (parser_hasval(p, "range")) {
		range = parser_getint(p, "range");
	}

	type = code_index_in_array(list_proj_names, parser_getsym(p, "type"));

	if (type < 0) {
		return PARSE_ERROR_GENERIC;
	}

	f->proj = type;
	f->proj_range = range;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_proj_dice(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	dice_t *dice;
	const char *string;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	dice = dice_new();
	if (!dice) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(f->proj_amt);
		f->proj_amt = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_proj_expr(struct parser *p) {
	struct feature_kind *f = parser_priv(p);
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!f->proj_amt) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (!expression) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(f->proj_amt, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}

	expression_free(expression);

	return result;
}

static enum parser_error parse_feat_default_size(struct parser *p)
{
	struct feature_kind *f = parser_priv(p);
	int size = parser_getint(p, "size");

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (size <= 0) {
		return PARSE_ERROR_GENERIC;
	}

	f->default_size = size;

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_feat(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "code str code", parse_feat_code);
	parser_reg(p, "name str name", parse_feat_name);
	parser_reg(p, "graphics char glyph sym color", parse_feat_graphics);
	parser_reg(p, "mimic str feat", parse_feat_mimic);
	parser_reg(p, "priority uint priority", parse_feat_priority);
	parser_reg(p, "flags ?str flags", parse_feat_flags);
	parser_reg(p, "digging int dig", parse_feat_digging);
	parser_reg(p, "timeout int timeout", parse_feat_timeout);
	parser_reg(p, "desc str text", parse_feat_desc);
	parser_reg(p, "walk-msg str text", parse_feat_walk_msg);
	parser_reg(p, "run-msg str text", parse_feat_run_msg);
	parser_reg(p, "hurt-msg str text", parse_feat_hurt_msg);
	parser_reg(p, "die-msg str text", parse_feat_die_msg);
	parser_reg(p, "confused-msg str text", parse_feat_confused_msg);
	parser_reg(p, "look-prefix str text", parse_feat_look_prefix);
	parser_reg(p, "look-in-preposition str text", parse_feat_look_in_preposition);
	parser_reg(p, "resist-flag sym flag", parse_feat_resist_flag);
	parser_reg(p, "produce sym name int amount int freq", parse_feat_feat_produce);
	parser_reg(p, "project sym type ?int range", parse_feat_proj);
	parser_reg(p, "project-dice str dice", parse_feat_proj_dice);
	parser_reg(p, "project-expr sym name sym base str expr", parse_feat_proj_expr);
	parser_reg(p, "default-size int size", parse_feat_default_size);
	/*
	 * Since the layout of the terrain array is fixed by list-terrain.h,
	 * allocate it now and fill in the customizable parts when parsing.
	 */
	f_info = mem_zalloc(FEAT_MAX * sizeof(*f_info));

	return p;
}

static errr run_parse_feat(struct parser *p) {
	return parse_file_quit_not_found(p, "terrain");
}

static errr finish_parse_feat(struct parser *p) {
	int shop_idx = 0, fidx;
	struct feature_kind *kind, *prod;

	for (fidx = 0; fidx < FEAT_MAX; ++fidx) {
		kind = &f_info[fidx];

		/*
		 * Assign shop index based on the order within the other
		 * terrain.
		 */
		if (tf_has(kind->flags, TF_SHOP)) {
			kind->shopnum = ++shop_idx;
		}
		/*
		 * Ensure the prefixes and prepositions end with a space for
		 * ease of use with the targeting code.
		 */
		assert(kind->look_prefix);

		if (kind->look_prefix[0] && !suffix(kind->look_prefix, " ")) {
			kind->look_prefix = string_append(kind->look_prefix, " ");
		}
		if (kind->look_in_preposition && !suffix(
				kind->look_in_preposition, " ")) {
			kind->look_in_preposition =
				string_append(kind->look_in_preposition,
				" ");
		}

		if (kind->feat_produce) {
			prod = &f_info[kind->feat_produce];

			if (feat_incompat_base(fidx, prod->fidx)) {
				quit_fmt("Terrain element %s produces terrain element %s but is not compatible with it!", kind->name, prod->name);
			}
		}
	}
	z_info->store_max = shop_idx;

	parser_destroy(p);
	return 0;
}

static void cleanup_feat(void) {
	int idx;
	for (idx = 0; idx < FEAT_MAX; idx++) {
		string_free(f_info[idx].look_in_preposition);
		string_free(f_info[idx].look_prefix);
		string_free(f_info[idx].confused_msg);
		string_free(f_info[idx].die_msg);
		string_free(f_info[idx].hurt_msg);
		string_free(f_info[idx].run_msg);
		string_free(f_info[idx].walk_msg);
		string_free(f_info[idx].desc);
		string_free(f_info[idx].name);
		dice_free(f_info[idx].proj_amt);
	}
	mem_free(f_info);
}

struct file_parser feat_parser = {
	"terrain",
	init_parse_feat,
	run_parse_feat,
	finish_parse_feat,
	cleanup_feat
};


/**
 * ------------------------------------------------------------------------
 * Intialize player bodies
 * ------------------------------------------------------------------------ */

static enum parser_error parse_body_body(struct parser *p) {
	struct player_body *h = parser_priv(p);
	struct player_body *b = mem_zalloc(sizeof *b);

	b->next = h;
	b->name = string_make(parser_getstr(p, "name"));
	parser_setpriv(p, b);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_body_slot(struct parser *p) {
	struct player_body *b = parser_priv(p);
	struct equip_slot *slot;
	int n;

	if (!b) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the last valid slot, then allocate a new one */
	slot = b->slots;
	if (!slot) {
		b->slots = mem_zalloc(sizeof(struct equip_slot));
		slot = b->slots;
	} else {
		while (slot->next) slot = slot->next;
		slot->next = mem_zalloc(sizeof(struct equip_slot));
		slot = slot->next;
	}

	n = lookup_flag(slots, parser_getsym(p, "slot"));
	if (!n) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	slot->type = n;
	slot->name = string_make(parser_getsym(p, "name"));
	b->count++;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_body(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "body str name", parse_body_body);
	parser_reg(p, "slot sym slot sym name", parse_body_slot);
	return p;
}

static errr run_parse_body(struct parser *p) {
	return parse_file_quit_not_found(p, "body");
}

static errr finish_parse_body(struct parser *p) {
	struct player_body *b;
	int i;
	bodies = parser_priv(p);

	/* Scan the list for the max slots */
	z_info->equip_slots_max = 0;
	for (b = bodies; b; b = b->next) {
		if (b->count > z_info->equip_slots_max) {
			z_info->equip_slots_max = b->count;
		}
	}

	/* Allocate the slot list and copy */
	for (b = bodies; b; b = b->next) {
		struct equip_slot *s_new;

		s_new = mem_zalloc(z_info->equip_slots_max * sizeof(*s_new));
		if (b->slots) {
			struct equip_slot *s_temp, *s_old = b->slots;

			/* Allocate space and copy */
			for (i = 0; i < z_info->equip_slots_max; i++) {
				memcpy(&s_new[i], s_old, sizeof(*s_old));
				s_old = s_old->next;
				if (!s_old) break;
			}

			/* Make next point correctly */
			for (i = 0; i < z_info->equip_slots_max; i++)
				if (s_new[i].next)
					s_new[i].next = &s_new[i + 1];

			/* Tidy up */
			s_old = b->slots;
			s_temp = s_old;
			while (s_temp) {
				s_temp = s_old->next;
				mem_free(s_old);
				s_old = s_temp;
			}
		}
		b->slots = s_new;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_body(void)
{
	struct player_body *b = bodies;
	struct player_body *next;
	int i;

	while (b) {
		next = b->next;
		string_free((char *)b->name);
		for (i = 0; i < b->count; i++)
			string_free((char *)b->slots[i].name);
		mem_free(b->slots);
		mem_free(b);
		b = next;
	}
}

struct file_parser body_parser = {
	"body",
	init_parse_body,
	run_parse_body,
	finish_parse_body,
	cleanup_body
};

/**
 * ------------------------------------------------------------------------
 * Initialize player histories
 * ------------------------------------------------------------------------ */

static struct history_chart *histories;

static struct history_chart *findchart(struct history_chart *hs,
									   unsigned int idx) {
	for (; hs; hs = hs->next)
		if (hs->idx == idx)
			break;
	return hs;
}

static enum parser_error parse_history_chart(struct parser *p) {
	struct history_chart *oc = parser_priv(p);
	struct history_chart *c;
	struct history_entry *e = mem_zalloc(sizeof *e);
	unsigned int idx = parser_getuint(p, "chart");

	if (!(c = findchart(oc, idx))) {
		c = mem_zalloc(sizeof *c);
		c->next = oc;
		c->idx = idx;
		parser_setpriv(p, c);
	}

	e->isucc = parser_getint(p, "next");
	e->roll = parser_getint(p, "roll");

	e->next = c->entries;
	c->entries = e;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_history_phrase(struct parser *p) {
	struct history_chart *h = parser_priv(p);

	if (!h)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	assert(h->entries);
	h->entries->text = string_append(h->entries->text, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_history(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "chart uint chart int next int roll", parse_history_chart);
	parser_reg(p, "phrase str text", parse_history_phrase);
	return p;
}

static errr run_parse_history(struct parser *p) {
	return parse_file_quit_not_found(p, "history");
}

static errr finish_parse_history(struct parser *p) {
	struct history_chart *c;
	struct history_entry *e, *prev, *next;
	histories = parser_priv(p);

	/* Go fix up the entry successor pointers. We can't compute them at
	 * load-time since we may not have seen the successor history yet. Also,
	 * we need to put the entries in the right order; the parser actually
	 * stores them backwards, which is not desirable.
	 */
	for (c = histories; c; c = c->next) {
		e = c->entries;
		prev = NULL;
		while (e) {
			next = e->next;
			e->next = prev;
			prev = e;
			e = next;
		}
		c->entries = prev;
		for (e = c->entries; e; e = e->next) {
			if (!e->isucc)
				continue;
			e->succ = findchart(histories, e->isucc);
			if (!e->succ) {
				return -1;
			}
		}
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_history(void)
{
	struct history_chart *c, *next_c;
	struct history_entry *e, *next_e;

	c = histories;
	while (c) {
		next_c = c->next;
		e = c->entries;
		while (e) {
			next_e = e->next;
			mem_free(e->text);
			mem_free(e);
			e = next_e;
		}
		mem_free(c);
		c = next_c;
	}
}

struct file_parser history_parser = {
	"history",
	init_parse_history,
	run_parse_history,
	finish_parse_history,
	cleanup_history
};

/**
 * ------------------------------------------------------------------------
 * Intialize player races
 * ------------------------------------------------------------------------ */

static enum parser_error parse_p_race_name(struct parser *p) {
	struct player_race *h = parser_priv(p);
	struct player_race *r = mem_zalloc(sizeof *r);
	struct player_race *or;

	// L: default to first race parsed
	or = h;
	while (or && or->next)  {
		or = or->next;
	}
	if (or) {
		memcpy(r, or, sizeof(*or));
	}

	// L: no default player flags though
	pf_wipe(r->pflags);

	r->next = h;
	r->name = string_make(parser_getstr(p, "name"));
	
	parser_setpriv(p, r);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_exp(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_exp = parser_getint(p, "exp");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_infravision(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->infra = parser_getint(p, "infra");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_history(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->history = findchart(histories, parser_getuint(p, "hist"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_age(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->b_age = parser_getint(p, "base_age");
	r->m_age = parser_getint(p, "mod_age");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_height(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->base_hgt = parser_getint(p, "base_hgt");
	r->mod_hgt = parser_getint(p, "mod_hgt");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_weight(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->base_wgt = parser_getint(p, "base_wgt");
	r->mod_wgt = parser_getint(p, "mod_wgt");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_obj_flags(struct parser *p) {
	struct player_race *r = parser_priv(p);
	char *flags;
	char *s;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(r->flags, OF_SIZE, list_obj_flag_names, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_play_flags(struct parser *p) {
	struct player_race *r = parser_priv(p);
	char *flags;
	char *s;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(r->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_values(struct parser *p) {
	struct player_race *r = parser_priv(p);
	char *s;
	char *t;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");

	while (t) {
		int value = 0;
		int index = 0;
		bool found = false;
		if (!grab_index_and_int(&value, &index, list_element_names, "RES_", t)) {
			found = true;
			r->el_info[index].res_level = value;
		}
		if (!found)
			break;

		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

/* L: parse monster source */
static enum parser_error parse_p_race_monster(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	const char *s = parser_getsym(p, "monster");
	struct monster_race *mon;
	
	if (!s)
		return PARSE_ERROR_GENERIC;

	mon = lookup_monster(s);
	if (!mon)
		return PARSE_ERROR_INVALID_MONSTER;

	r->mon_race = mon;

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_p_race(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_p_race_name);
	parser_reg(p, "exp int exp", parse_p_race_exp);
	parser_reg(p, "infravision int infra", parse_p_race_infravision);
	parser_reg(p, "history uint hist", parse_p_race_history);
	parser_reg(p, "age int base_age int mod_age", parse_p_race_age);
	parser_reg(p, "height int base_hgt int mod_hgt", parse_p_race_height);
	parser_reg(p, "weight int base_wgt int mod_wgt", parse_p_race_weight);
	parser_reg(p, "obj-flags ?str flags", parse_p_race_obj_flags);
	parser_reg(p, "player-flags ?str flags", parse_p_race_play_flags);
	parser_reg(p, "values str values", parse_p_race_values);
	parser_reg(p, "monster sym monster", parse_p_race_monster);
	return p;
}

static errr run_parse_p_race(struct parser *p) {
	return parse_file_quit_not_found(p, "p_race");
}

static int p_race_compare(const void *r1, const void *r2)
{
	struct player_race *pr1 = *(struct player_race **)r1, *pr2 = *(struct player_race **)r2;
	int result = 0, i;

	for (i = 0; i < SKILL_MAX; ++i) {
		result += ABS(pr1->mon_race->skills[i] - 75);
		result -= ABS(pr2->mon_race->skills[i] - 75);
	}

	for (i = 0; i < STAT_MAX; ++i) {
		result += ABS(pr1->mon_race->stat_mod[i]) * 10;
		result -= ABS(pr2->mon_race->stat_mod[i]) * 10;
	}

	if (result) return SGN(result);

	return strcmp(pr1->name, pr2->name);
}

static errr finish_parse_p_race(struct parser *p) {
	struct player_race *r;
	size_t num = 0, curr;
	char r_name[80];
	struct player_race **array;

	for (r = parser_priv(p); r; r = r->next) {
		if (!r->mon_race) {
			strnfmt(r_name, sizeof r_name, "%s", r->name);
			my_struncap_full(r_name);

			r->mon_race = lookup_monster(r_name);

			assert(r->mon_race);
		}

		num++;
	}

	array = mem_zalloc(sizeof *array * num);

	for (r = parser_priv(p), curr = 0; r; r = r->next, curr++) {
		assert(curr < num);
		array[curr] = r;
	}

	assert(curr == num);

	sort(array, num, sizeof *array, p_race_compare);

	races = array[0];

	for (r = races, curr = 0; curr < num; r = r->next, curr++) {
		if (curr + 1 == num) {
			r->next = NULL;
		}
		else {
			r->next = array[curr + 1];
		}

		r->ridx = curr;
	}

	mem_free(array);

	z_info->pr_max = num;

	parser_destroy(p);
	return 0;
}

static void cleanup_p_race(void)
{
	struct player_race *p = races;
	struct player_race *next;
	//struct evolution *e;

	while (p) {
		next = p->next;
		string_free((char *)p->name);
		/*e = p->evol;
		while (e) {
			struct evolution *en = e->next;
			mem_free(e);
			e = en;
		}*/
		mem_free(p);
		p = next;
	}
}

struct file_parser p_race_parser = {
	"p_race",
	init_parse_p_race,
	run_parse_p_race,
	finish_parse_p_race,
	cleanup_p_race
};

/**
 * ------------------------------------------------------------------------
 * Initialize player magic realms
 * ------------------------------------------------------------------------ */
static enum parser_error parse_realm_name(struct parser *p) {
	struct magic_realm *h = parser_priv(p);
	struct magic_realm *realm = mem_zalloc(sizeof *realm);
	const char *name = parser_getstr(p, "name");

	realm->index = z_info->realm_max++;

	realm->next = h;
	parser_setpriv(p, realm);
	realm->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_stat(struct parser *p) {
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	realm->stat = stat_name_to_idx(parser_getsym(p, "stat"));
	if (realm->stat < 0) {
		return PARSE_ERROR_INVALID_SPELL_STAT;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_verb(struct parser *p) {
	const char *verb = parser_getstr(p, "verb");
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(realm->verb);
	realm->verb = string_make(verb);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_spell_noun(struct parser *p) {
	const char *spell = parser_getstr(p, "spell");
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(realm->spell_noun);
	realm->spell_noun = string_make(spell);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_book_noun(struct parser *p) {
	const char *book = parser_getstr(p, "book");
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(realm->book_noun);
	realm->book_noun = string_make(book);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_weight(struct parser *p) 
{
	int weight = parser_getint(p, "weight");
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	realm->weight = weight;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_school_aptitude(struct parser *p) 
{
	struct magic_realm *realm = parser_priv(p);
	const char *school_name = parser_getsym(p, "school");
	int mod = parser_getint(p, "mod");
	int school_ind;

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	// special: all schools
	if (streq(school_name, "ALL")) {
		int i;
		for (i = 0; i < MS_MAX; ++i) {
			realm->school_modifiers[i] += mod;
		}
	}
	else {
		school_ind = school_idx_by_name(school_name);

		if (school_ind <= MS_NONE) {
			return PARSE_ERROR_GENERIC;
		}

		realm->school_modifiers[school_ind] += mod;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_special(struct parser *p)
{
	struct magic_realm *realm = parser_priv(p);
	int which = realm_special_by_name(parser_getsym(p, "which"));
	int amt;

	if (parser_hasval(p, "amount")) {
		amt = parser_getint(p, "amount");
	}
	else {
		amt = 1;
	}

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	if (which < 0 || which >= RLM_SPCL_MAX) {
		return PARSE_ERROR_GENERIC;
	}

	realm->realm_special[which] = amt;

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_realm(void) {
	z_info->realm_max = 0;
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_realm_name);
	parser_reg(p, "stat sym stat", parse_realm_stat);
	parser_reg(p, "verb str verb", parse_realm_verb);
	parser_reg(p, "spell-noun str spell", parse_realm_spell_noun);
	parser_reg(p, "book-noun str book", parse_realm_book_noun);
	parser_reg(p, "weight int weight", parse_realm_weight);
	parser_reg(p, "school sym school int mod", parse_realm_school_aptitude);
	parser_reg(p, "special sym which ?int amount", parse_realm_special);
	return p;
}

static errr run_parse_realm(struct parser *p) {
	return parse_file_quit_not_found(p, "realm");
}

static errr finish_parse_realm(struct parser *p) {
	realms = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_realm(void)
{
	struct magic_realm *p = realms;
	struct magic_realm *next;

	while (p) {
		next = p->next;
		string_free(p->name);
		string_free(p->verb);
		string_free(p->spell_noun);
		string_free(p->book_noun);
		mem_free(p);
		p = next;
	}
}

struct file_parser realm_parser = {
	"realm",
	init_parse_realm,
	run_parse_realm,
	finish_parse_realm,
	cleanup_realm
};

/**
 * ------------------------------------------------------------------------
 * Intialize player shapechange shapes
 * ------------------------------------------------------------------------ */

static enum parser_error parse_shape_name(struct parser *p) {
	struct player_shape *h = parser_priv(p);
	struct player_shape *shape = mem_zalloc(sizeof *shape);

	shape->next = h;
	shape->name = string_make(parser_getstr(p, "name"));
	parser_setpriv(p, shape);
	shape->sidx = z_info->shape_max++;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_combat(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	shape->to_h = parser_getint(p, "to-h");
	shape->to_d = parser_getint(p, "to-d");
	shape->to_a = parser_getint(p, "to-a");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_disarm_phys(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_DISARM_PHYS] = parser_getint(p, "disarm");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_disarm_magic(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_DISARM_MAGIC] = parser_getint(p, "disarm");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_save(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_SAVE] = parser_getint(p, "save");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_stealth(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_STEALTH] = parser_getint(p, "stealth");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_search(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_SEARCH] = parser_getint(p, "search");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_melee(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "melee");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_throw(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_TO_HIT_THROW] = parser_getint(p, "throw");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_dig(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_DIGGING] = parser_getint(p, "dig");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_obj_flags(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	char *flags;
	char *s;

	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(shape->flags, OF_SIZE, list_obj_flag_names, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_play_flags(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	char *flags;
	char *s;

	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(shape->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_values(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	char *s;
	char *t;

	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");

	while (t) {
		int value = 0;
		int index = 0;
		bool found = false;
		if (!grab_int_value(shape->modifiers, obj_mods, t))
			found = true;
		if (!grab_index_and_int(&value, &index, list_element_names, "RES_", t)) {
			found = true;
			shape->el_info[index].res_level = value;
		}
		if (!found)
			break;

		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_effect(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*effect));
	if (shape->effect) {
		effect = shape->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		shape->effect = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_shape_effect_yx(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_dice(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;
	dice_t *dice;
	const char *string;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_expr(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}
	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_shape_effect_msg(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	effect->msg = string_append(effect->msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_blow(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct player_blow *blow;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	blow = mem_zalloc(sizeof(*blow));
	blow->name = string_make(parser_getstr(p, "blow"));
	blow->next = shape->blows;
	shape->blows = blow;
	shape->num_blows++;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_shape(void) {
	struct parser *p = parser_new();
	z_info->shape_max = 0;
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_shape_name);
	parser_reg(p, "combat int to-h int to-d int to-a", parse_shape_combat);
	parser_reg(p, "skill-disarm-phys int disarm", parse_shape_skill_disarm_phys);
	parser_reg(p, "skill-disarm-magic int disarm", parse_shape_skill_disarm_magic);
	parser_reg(p, "skill-save int save", parse_shape_skill_save);
	parser_reg(p, "skill-stealth int stealth", parse_shape_skill_stealth);
	parser_reg(p, "skill-search int search", parse_shape_skill_search);
	parser_reg(p, "skill-melee int melee", parse_shape_skill_melee);
	parser_reg(p, "skill-throw int throw", parse_shape_skill_throw);
	parser_reg(p, "skill-dig int dig", parse_shape_skill_dig);
	parser_reg(p, "obj-flags ?str flags", parse_shape_obj_flags);
	parser_reg(p, "player-flags ?str flags", parse_shape_play_flags);
	parser_reg(p, "values str values", parse_shape_values);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_shape_effect);
	parser_reg(p, "effect-yx int y int x", parse_shape_effect_yx);
	parser_reg(p, "dice str dice", parse_shape_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_shape_expr);
	parser_reg(p, "effect-msg str text", parse_shape_effect_msg);
	parser_reg(p, "blow str blow", parse_shape_blow);
	return p;
}

static errr run_parse_shape(struct parser *p) {
	return parse_file_quit_not_found(p, "shape");
}

static errr finish_parse_shape(struct parser *p) {
	shapes = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_shape(void)
{
	struct player_shape *shape = shapes;
	struct player_shape *next;

	while (shape) {
		struct player_blow *blow = shape->blows;
		next = shape->next;
		string_free((char *)shape->name);
		free_effect(shape->effect);
		while (blow) {
			struct player_blow *next_blow = blow->next;
			string_free(blow->name);
			mem_free(blow);
			blow = next_blow;
		}
		mem_free(shape);
		shape = next;
	}
}

struct file_parser shape_parser = {
	"shape",
	init_parse_shape,
	run_parse_shape,
	finish_parse_shape,
	cleanup_shape
};


/**
 * ------------------------------------------------------------------------
 * L: Initialize generic spells
 * ------------------------------------------------------------------------*/

static enum parser_error parse_spell_name(struct parser *p) {
	struct player_spell *s = parser_priv(p);
	struct player_spell *spell = mem_zalloc(sizeof *spell);
	int i;

	spell->name = string_make(parser_getsym(p, "name"));

	spell->slevel = parser_getint(p, "level");

	if (parser_hasval(p, "mana")) {
		spell->smana = parser_getint(p, "mana");
	} else {
		spell->smana = my_int_sqrt(spell->slevel) + 2;
	}

	if (parser_hasval(p, "fail")) {
		spell->sfail = parser_getint(p, "fail");
	} else {
		spell->sfail = spell->slevel / 5 + 40;
	}

	for (i = 0; i < MAX_SPELL_SCHOOLS; ++i) {
		spell->school[i] = MS_NONE;
	}
	
	spell->next = s;

	spell->sidx = z_info->spell_max;
	++z_info->spell_max;

	parser_setpriv(p, spell);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_spell_effect(struct parser *p) {
	struct player_spell *s = parser_priv(p);
	struct effect *effect, *new_effect;

	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*effect));
	if (s->effect) {
		effect = s->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		s->effect = new_effect;
	}

	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_spell_effect_yx(struct parser *p) {
	struct player_spell *s = parser_priv(p);
	struct effect *effect;

	/* If there is no effect, assume that this is human and not parser error. */
	effect = s->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_spell_dice(struct parser *p) {
	struct player_spell *s = parser_priv(p);
	struct effect *effect;
	dice_t *dice;
	const char *string;

	/* If there is no effect, assume that this is human and not parser error. */
	effect = s->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_spell_expr(struct parser *p) {
	struct player_spell *s = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	/* If there is no effect, assume that this is human and not parser error. */
	effect = s->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}

	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_spell_effect_msg(struct parser *p) {
	struct player_spell *s = parser_priv(p);
	struct effect *effect;

	/* If there is no effect, assume that this is human and not parser error. */
	effect = s->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	effect->msg = string_append(effect->msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_spell_desc(struct parser *p) {
	struct player_spell *s = parser_priv(p);

	s->text = string_append(s->text, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_spell_school(struct parser *p) {
	struct player_spell *s = parser_priv(p);
	const char *school_name;
	int school_idx;
	int i;

	school_name = parser_getsym(p, "school");

	school_idx = MS_NONE;
	for (i = 0; list_school_names[i]; i++) {
		if (streq(school_name, list_school_names[i])) {
			school_idx = i;
		}
	}

	if (school_idx == MS_NONE) return PARSE_ERROR_GENERIC;

	for (i = 0; i < MAX_SPELL_SCHOOLS; i++) {
		if (s->school[i] == MS_NONE) {
			s->school[i] = school_idx;
			break;
		}
	}

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_spell(void) {
	struct parser *p = parser_new();
	z_info->spell_max = 0;
	parser_setpriv(p, NULL);
	parser_reg(p, "spell sym name int level ?int mana ?int fail", parse_spell_name);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_spell_effect);
	parser_reg(p, "effect-yx int y int x", parse_spell_effect_yx);
	parser_reg(p, "dice str dice", parse_spell_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_spell_expr);
	parser_reg(p, "effect-msg str text", parse_spell_effect_msg);
	parser_reg(p, "desc str desc", parse_spell_desc);
	parser_reg(p, "school sym school", parse_spell_school);
	return p;
}

static errr run_parse_spell(struct parser *p) {
	return parse_file_quit_not_found(p, "p_spell");
}

static errr finish_parse_spell(struct parser *p) {
	spells = parser_priv(p);
	parser_destroy(p);

	write_spellbook_kinds();
	
	return 0;
}

static void cleanup_spell(void)
{
	struct player_spell *spell = spells;
	struct player_spell *next;

	while (spell) {
		next = spell->next;
		string_free(spell->name);
		string_free(spell->text);
		free_effect(spell->effect);
		mem_free(spell);
		spell = next;
	}
}

struct file_parser spell_parser = {
	"spell",
	init_parse_spell,
	run_parse_spell,
	finish_parse_spell,
	cleanup_spell
};


/**
 * ------------------------------------------------------------------------
 * Initialize player classes
 * ------------------------------------------------------------------------ */

/*
 * Used to remember the maximum number of books for the current class and the
 * maximum number of spells in the current book while parsing so bounds
 * checking can be done.
 */
//static int class_max_books = 0;
//static int book_max_spells = 0;

static enum parser_error parse_class_name(struct parser *p) {
	struct player_class *h = parser_priv(p);
	struct player_class *c = mem_zalloc(sizeof *c);
	c->name = string_make(parser_getstr(p, "name"));
	c->next = h;
	parser_setpriv(p, c);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_stats(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	c->c_adj[STAT_STR] = parser_getint(p, "str");
	c->c_adj[STAT_INT] = parser_getint(p, "int");
	c->c_adj[STAT_WIS] = parser_getint(p, "wis");
	c->c_adj[STAT_DEX] = parser_getint(p, "dex");
	c->c_adj[STAT_CON] = parser_getint(p, "con");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_disarm_phys(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DISARM_PHYS] = parser_getint(p, "base");
	c->x_skills[SKILL_DISARM_PHYS] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_disarm_magic(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "base");
	c->x_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_device(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DEVICE] = parser_getint(p, "base");
	c->x_skills[SKILL_DEVICE] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_save(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_SAVE] = parser_getint(p, "base");
	c->x_skills[SKILL_SAVE] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_stealth(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_STEALTH] = parser_getint(p, "base");
	c->x_skills[SKILL_STEALTH] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_search(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_SEARCH] = parser_getint(p, "base");
	c->x_skills[SKILL_SEARCH] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_melee(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "base");
	c->x_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_shoot(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "base");
	c->x_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_throw(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "base");
	c->x_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_dig(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DIGGING] = parser_getint(p, "base");
	c->x_skills[SKILL_DIGGING] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_magic(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_MAGIC] = parser_getint(p, "base");
	c->x_skills[SKILL_MAGIC] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_health(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_HEALTH] = parser_getint(p, "base");
	c->x_skills[SKILL_HEALTH] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_monster(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_MONSTER] = parser_getint(p, "base");
	c->x_skills[SKILL_MONSTER] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}


/* L: parse abilities */
static enum parser_error parse_class_power(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	int i = 0;
	char *s = string_make(parser_getsym(p, "name"));
	int v = parser_getint(p, "value");

	while (list_player_powers_names[i] && !streq(list_player_powers_names[i], s))
	    i++;
	string_free(s);
	if (!list_player_powers_names[i])
        return PARSE_ERROR_GENERIC;

	c->c_powers[i] = v;

	return PARSE_ERROR_NONE;
}

/*
static enum parser_error parse_class_hitdie(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_mhp = parser_getint(p, "mhp");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_exp(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_exp = parser_getint(p, "exp");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_max_attacks(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->max_attacks = parser_getint(p, "max-attacks");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_min_weight(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->min_weight = parser_getint(p, "min-weight");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_str_mult(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->att_multiply = parser_getint(p, "att-multiply");
	return PARSE_ERROR_NONE;
}
*/

static enum parser_error parse_class_title(struct parser *p) {
	struct player_class *c = parser_priv(p);
	int n, i;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	n = (int) N_ELEMENTS(c->title);
	for (i = 0; i < n; i++) {
		if (!c->title[i]) {
			c->title[i] = string_make(parser_getstr(p, "title"));
			break;
		}
	}

	return (i >= n) ? PARSE_ERROR_TOO_MANY_ENTRIES : PARSE_ERROR_NONE;
}

static int lookup_option(const char *name)
{
	int result = 1;

	while (1) {
		if (result >= OPT_MAX) {
			return 0;
		}
		if (streq(option_name(result), name)) {
			return result;
		}
		++result;
	}
}

static enum parser_error parse_class_equip(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct start_item *si;
	int tval, sval;
	char *eopts;
	char *s;
	int *einds;
	int nind, nalloc;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		plog("unknown tval");
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}

	sval = lookup_sval(tval, parser_getsym(p, "sval"));
	if (sval < 0) {
		plog("unknown sval");
		return PARSE_ERROR_UNRECOGNISED_SVAL;
	}

	eopts = string_make(parser_getsym(p, "eopts"));
	einds = NULL;
	nind = 0;
	nalloc = 0;
	s = strtok(eopts, " |");
	while (s) {
		bool negated = false;
		int ind;

		if (prefix(s, "NOT-")) {
			negated = true;
			s += 4;
		}
		ind = lookup_option(s);
		if (ind > 0 && option_type(ind) == OP_BIRTH) {
			if (nind >= nalloc - 2) {
				if (nalloc == 0) {
					nalloc = 2;
				} else {
					nalloc *= 2;
				}
				einds = mem_realloc(einds,
					nalloc * sizeof(*einds));
			}
			einds[nind] = (negated) ? -ind : ind;
			einds[nind + 1] = 0;
			++nind;
		} else if (!streq(s, "none")) {
			mem_free(einds);
			string_free(eopts);
			return PARSE_ERROR_INVALID_OPTION;
		}
		s = strtok(NULL, " |");
	}
	string_free(eopts);

	si = mem_zalloc(sizeof *si);
	si->tval = tval;
	si->sval = sval;
	si->min = parser_getuint(p, "min");
	si->max = parser_getuint(p, "max");
	si->eopts = einds;

	if (si->min > 99 || si->max > 99) {
		mem_free(si->eopts);
		mem_free(si);
		return PARSE_ERROR_INVALID_ITEM_NUMBER;
	}

	si->next = c->start_items;
	c->start_items = si;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_obj_flags(struct parser *p) {
	struct player_class *c = parser_priv(p);
	char *flags;
	char *s;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(c->flags, OF_SIZE, list_obj_flag_names, s))
			break;
		s = strtok(NULL, " |");
	}

	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_class_play_flags(struct parser *p) {
	struct player_class *c = parser_priv(p);
	char *flags;
	char *s;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(c->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}

	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_class_realm(struct parser *p)
{
	struct player_class *c = parser_priv(p);
	const char *name = parser_getsym(p, "realm");
	struct magic_realm *realm;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	for (realm = realms; realm; realm = realm->next) {
		if (streq(realm->name, name)) {
			c->realm = realm;
		}
	}

	if (!c->realm) {
		return PARSE_ERROR_GENERIC;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_unlockable(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	c->unlockable = parser_getint(p, "unlockable");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_prereq(struct parser *p) {
	struct player_class *c = parser_priv(p);
	const char *prereq_name = parser_getsym(p, "id");
	int prereq_id = code_index_in_array(race_predicate_names, prereq_name);

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	c->prereqs[prereq_id] = true;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_class(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_class_name);
	parser_reg(p, "stats int str int int int wis int dex int con",
			   parse_class_stats);
	parser_reg(p, "skill-disarm-phys int base int incr",
			   parse_class_skill_disarm_phys);
	parser_reg(p, "skill-disarm-magic int base int incr",
			   parse_class_skill_disarm_magic);
	parser_reg(p, "skill-device int base int incr", parse_class_skill_device);
	parser_reg(p, "skill-save int base int incr", parse_class_skill_save);
	parser_reg(p, "skill-stealth int base int incr", parse_class_skill_stealth);
	parser_reg(p, "skill-search int base int incr", parse_class_skill_search);
	parser_reg(p, "skill-melee int base int incr", parse_class_skill_melee);
	parser_reg(p, "skill-shoot int base int incr", parse_class_skill_shoot);
	parser_reg(p, "skill-throw int base int incr", parse_class_skill_throw);
	parser_reg(p, "skill-dig int base int incr", parse_class_skill_dig);
	parser_reg(p, "skill-magic int base int incr", parse_class_skill_magic);
	parser_reg(p, "skill-health int base int incr", parse_class_skill_health);
	parser_reg(p, "skill-monster int base int incr", parse_class_skill_monster);
	parser_reg(p, "power sym name int value", parse_class_power);
	parser_reg(p, "title str title", parse_class_title);
	parser_reg(p, "equip sym tval sym sval uint min uint max sym eopts",
			   parse_class_equip);
	parser_reg(p, "obj-flags ?str flags", parse_class_obj_flags);
	parser_reg(p, "player-flags ?str flags", parse_class_play_flags);
	parser_reg(p, "realm sym realm", parse_class_realm);
	parser_reg(p, "unlockable int unlockable", parse_class_unlockable);
	parser_reg(p, "prereq sym id", parse_class_prereq);
	return p;
}

static void ensure_proper_power_parents(struct player_class *c, int power)
{
	int curr = c->c_powers[power];
	int parent_powers = 0;
	int i;
	int num_power_parents = 0;
	struct player_ability *abil = lookup_player_ability(power, PY_ABIL_POWER);

	assert(abil);
	if (curr <= 0) return;

	for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
		struct player_ability *parent = abil->parent[i];
		if (!parent) continue;
		if (parent->type != PY_ABIL_POWER) continue;

		++num_power_parents;

		parent_powers += c->c_powers[parent->index];
	}

	if (num_power_parents > 0) {
		int to_add = ((curr - parent_powers) / 2 + num_power_parents - 1) / num_power_parents;
		if (to_add > 0) {
			for (i = 0; i < MAX_ABIL_PARENTS; ++i) {
				struct player_ability *parent = abil->parent[i];

				if (parent && parent->type == PY_ABIL_POWER) {
					// round it up to the nearest 5
					c->c_powers[parent->index] += to_add;
					int to_round = 5 - c->c_powers[parent->index] % 5;
					if (to_round < 5) c->c_powers[parent->index] += to_round;
					ensure_proper_power_parents(c, parent->index);
				}
			}
		}
	}
}

static errr run_parse_class(struct parser *p) {
	return parse_file_quit_not_found(p, "class");
}

static errr finish_parse_class(struct parser *p) {
	struct player_class *c;
	int num = 0;

	classes = parser_priv(p);
	for (c = classes; c; c = c->next) num++;

	z_info->c_max = num;

	for (c = classes; c; c = c->next, num--) {
		assert(num);
		c->cidx = num - 1;
	}

	// L: ensure classes have parents to support child properties
	for (c = classes; c; c = c->next) {
		int i;
		for (i = PP_NONE + 1; i < PP_MAX; ++i) {
			ensure_proper_power_parents(c, i);
		}
	}

	parser_destroy(p);
	return 0;
}

static void cleanup_class(void)
{
	struct player_class *c = classes;
	struct player_class *next;
	struct start_item *item, *item_next;
	struct class_spell *spell;
	struct class_book *book;
	int i, j;

	while (c) {
		next = c->next;
		item = c->start_items;
		while(item) {
			item_next = item->next;
			mem_free(item->eopts);
			mem_free(item);
			item = item_next;
		}
		for (i = 0; i < c->magic.num_books; i++) {
			book = &c->magic.books[i];
			for (j = 0; j < book->num_spells; j++) {
				spell = &book->spells[j];
				string_free(spell->name);
				string_free(spell->text);
				free_effect(spell->effect);
			}
			mem_free(book->spells);
		}
		mem_free(c->magic.books);
		for (i = (int) N_ELEMENTS(c->title) - 1; i >= 0; --i) {
			string_free((char *)c->title[i]);
		}
		string_free((char *)c->name);
		mem_free(c);
		c = next;
	}
}

struct file_parser class_parser = {
	"class",
	init_parse_class,
	run_parse_class,
	finish_parse_class,
	cleanup_class
};

/**
 * ------------------------------------------------------------------------
 * Intialize flavors
 * ------------------------------------------------------------------------ */

static wchar_t flavor_glyph;
static unsigned int flavor_tval;

static enum parser_error parse_flavor_flavor(struct parser *p) {
	struct flavor *h = parser_priv(p);
	struct flavor *f = mem_zalloc(sizeof *f);

	const char *attr;
	int d_attr;

	f->next = h;

	f->fidx = parser_getuint(p, "index");
	f->tval = flavor_tval;
	f->d_char = flavor_glyph;

	if (parser_hasval(p, "sval"))
		f->sval = lookup_sval(f->tval, parser_getsym(p, "sval"));
	else
		f->sval = SV_UNKNOWN;

	attr = parser_getsym(p, "attr");
	if (strlen(attr) == 1)
		d_attr = color_char_to_attr(attr[0]);
	else
		d_attr = color_text_to_attr(attr);

	if (d_attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
	f->d_attr = d_attr;

	if (parser_hasval(p, "desc"))
		f->text = string_append(f->text, parser_getstr(p, "desc"));

	parser_setpriv(p, f);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_flavor_kind(struct parser *p) {
	int tval = tval_find_idx(parser_getsym(p, "tval"));

	if (tval <= 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	flavor_glyph = parser_getchar(p, "glyph");
	flavor_tval = (unsigned int) tval;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_flavor(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);

	parser_reg(p, "kind sym tval char glyph", parse_flavor_kind);
	parser_reg(p, "flavor uint index sym attr ?str desc", parse_flavor_flavor);
	parser_reg(p, "fixed uint index sym sval sym attr ?str desc", parse_flavor_flavor);

	return p;
}

static errr run_parse_flavor(struct parser *p) {
	return parse_file_quit_not_found(p, "flavor");
}

static errr finish_parse_flavor(struct parser *p) {
	flavors = parser_priv(p);
	parser_destroy(p);

	return 0;
}

static void cleanup_flavor(void)
{
	struct flavor *f, *next;

	f = flavors;
	while(f) {
		next = f->next;
		/* Hack - scrolls get randomly-generated names */
		if (f->tval != TV_SCROLL)
			mem_free(f->text);
		mem_free(f);
		f = next;
	}
}

struct file_parser flavor_parser = {
	"flavor",
	init_parse_flavor,
	run_parse_flavor,
	finish_parse_flavor,
	cleanup_flavor
};


/**
 * ------------------------------------------------------------------------
 * Initialize hints
 * ------------------------------------------------------------------------ */

static enum parser_error parse_hint(struct parser *p) {
	struct hint *h = parser_priv(p);
	struct hint *new = mem_zalloc(sizeof *new);

	new->hint = string_make(parser_getstr(p, "text"));
	new->next = h;

	parser_setpriv(p, new);
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_hints(void) {
	struct parser *p = parser_new();
	parser_reg(p, "H str text", parse_hint);
	return p;
}

static errr run_parse_hints(struct parser *p) {
	return parse_file_quit_not_found(p, "hints");
}

static errr finish_parse_hints(struct parser *p) {
	hints = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_hints(void)
{
	struct hint *h, *next;

	h = hints;
	while(h) {
		next = h->next;
		string_free(h->hint);
		mem_free(h);
		h = next;
	}
}

struct file_parser hints_parser = {
	"hints",
	init_parse_hints,
	run_parse_hints,
	finish_parse_hints,
	cleanup_hints
};

/**
 * ------------------------------------------------------------------------
 * Game data initialization
 * ------------------------------------------------------------------------ */


/*
 * L: parse effects later in case they're getting initialized before their subtypes
 * are
 */
static void fill_in_effect_subtype(struct effect *ef, const char *source_type, const char *source_name)
{
	struct effect *temp;

	for (temp = ef; temp; temp = temp->next) {
		if (!temp->subtype_temp) continue;

		temp->subtype = effect_subtype(temp->index, temp->subtype_temp);

		if (temp->subtype < 0) {
			quit_fmt("Error: unrecognized effect subtype %s (%s %s)!", temp->subtype_temp, source_type, source_name);
		}

		string_free(temp->subtype_temp);
		temp->subtype_temp = NULL;
	}
}

static void fill_all_effect_subtypes(void)
{
	struct player_spell *ps;
	struct monster_spell *ms;

	for (ps = spells; ps; ps = ps->next) {
		fill_in_effect_subtype(ps->effect, "spell", ps->name);
	}

	for (ms = monster_spells; ms; ms = ms->next) {
		fill_in_effect_subtype(ms->effect, "monster spell", format("idx %i", ms->index));
	}
}


/**
 * A list of all the above parsers, plus those found in mon-init.c and
 * obj-init.c
 */
static struct {
	const char *name;
	struct file_parser *parser;
} pl[] = {
	{ "world", &world_parser },
	{ "projections", &projection_parser },
	{ "ui renderers", &ui_entry_renderer_parser },
	{ "ui entries", &ui_entry_parser },
	{ "features", &feat_parser },
	{ "object bases", &object_base_parser },
	{ "slays", &slay_parser },
	{ "brands", &brand_parser },
	{ "monster pain messages", &pain_parser },
	{ "bodies", &body_parser },
	{ "curses", &curse_parser },
	{ "player shapes", &shape_parser },
	{ "objects", &object_parser }, // L: must be after player shapes
	{ "player properties", &player_property_parser },
	{ "player spells", &spell_parser }, // L: must be after objects
	{ "magic realms", &realm_parser },
	{ "player classes", &class_parser }, // L: must be after spells and realms
	{ "activations", &act_parser },
	{ "ego-items", &ego_parser },
	{ "history charts", &history_parser },
	{ "artifacts", &artifact_parser },
	{ "object properties", &object_property_parser },
	{ "timed effects", &player_timed_parser },
	{ "blow methods", &meth_parser },
	{ "blow effects", &eff_parser },
	{ "monster spells", &mon_spell_parser },
	{ "monster bases", &mon_base_parser }, // L: must be after bodies
	{ "summons", &summon_parser }, // L: must be after monster bases
	{ "monsters", &monster_parser }, // L: must be after player spells
	{ "player races", &p_race_parser }, // L: must be after monsters
	{ "monster pits" , &pit_parser },
	{ "monster lore" , &lore_parser },
	{ "traps", &trap_parser },
	{ "chest_traps", &chest_trap_parser },
	{ "quests", &quests_parser },
	{ "flavours", &flavor_parser },
	{ "hints", &hints_parser },
	{ "random names", &names_parser }
};

/**
 * Initialize just the internal arrays.
 * This should be callable by the test suite, without relying on input, or
 * anything to do with a user or savefiles.
 *
 * Assumption: Paths are set up correctly before calling this function.
 */
void init_arrays(void)
{
	unsigned int i;

	for (i = 0; i < N_ELEMENTS(pl); i++) {
		char *msg = string_make(format("Initializing %s...", pl[i].name));
		event_signal_message(EVENT_INITSTATUS, 0, msg);
		string_free(msg);
		if (run_parser(pl[i].parser)) {
			quit_fmt("Cannot initialize %s.", pl[i].name);
		}
	}

	fill_all_effect_subtypes();
}

/**
 * Free all the internal arrays
 */
static void cleanup_arrays(void)
{
	unsigned int i;

	for (i = 1; i < N_ELEMENTS(pl); i++)
		cleanup_parser(pl[i].parser);

	cleanup_parser(pl[0].parser);
}

static struct init_module arrays_module = {
	.name = "arrays",
	.init = init_arrays,
	.cleanup = cleanup_arrays
};


extern struct init_module z_quark_module;
extern struct init_module generate_module;
extern struct init_module rune_module;
extern struct init_module obj_make_module;
extern struct init_module ignore_module;
extern struct init_module mon_make_module;
extern struct init_module player_module;
extern struct init_module store_module;
extern struct init_module messages_module;
extern struct init_module options_module;
extern struct init_module ui_player_module;
extern struct init_module ui_equip_cmp_module;

static struct init_module *modules[] = {
	&z_quark_module,
	&messages_module,
	&ui_visuals_module, /* This needs to load before monsters and objects. */
	&arrays_module,
	&player_module,
	&generate_module,
	&rune_module,
	&obj_make_module,
	&ignore_module,
	&mon_make_module,
	&store_module,
	&options_module,
	&ui_player_module,
	&ui_equip_cmp_module,
	NULL
};

/**
 * Initialise Angband's data stores and allocate memory for structures,
 * etc, so that the game can get started.
 *
 * The only input/output in this file should be via event_signal_string().
 * We cannot rely on any particular UI as this part should be UI-agnostic.
 * We also cannot rely on anything else having being initialised into any
 * particlar state.  Which is why you'd be calling this function in the
 * first place.
 *
 * Old comment, not sure if still accurate:
 * Note that the "graf-xxx.prf" file must be loaded separately,
 * if needed, in the first (?) pass through "TERM_XTRA_REACT".
 */
bool init_angband(void)
{
	int i;

	event_signal(EVENT_ENTER_INIT);

	init_game_constants();

	/* Initialise modules */
	for (i = 0; modules[i]; i++) {
		if (modules[i]->init) {
			modules[i]->init();
		}
	}

	/* Initialize some other things */
	event_signal_message(EVENT_INITSTATUS, 0, "Initializing other stuff...");

	/* List display codes */
	monster_list_init();
	object_list_init();

	/* Initialise RNG */
	event_signal_message(EVENT_INITSTATUS, 0, "Getting the dice rolling...");
	Rand_init();

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_mimic *mm;
		for (mm = r_info[i].mimic_kinds; mm; mm = mm->next) {
			assert(mm->kind);
			assert(mm->kind->kidx < z_info->k_max * 2);
		}
	}

	return true;
}

/**
 * Free all the stuff initialised in init_angband()
 */
void cleanup_angband(void)
{
	int i;

	/* Free the chunk list */
	for (i = 0; i < chunk_list_max; i++) {
		wipe_mon_list(chunk_list[i], player);
		cave_free(chunk_list[i]);
	}
	mem_free(chunk_list);
	chunk_list = NULL;

	for (i = 0; modules[i]; i++) {
		if (modules[i]->cleanup) {
			modules[i]->cleanup();
		}
	}

	event_remove_all_handlers();

	/* Free the main cave */
	if (cave) {
		cave_free(cave);
		cave = NULL;
		character_dungeon = false;
	}

	monster_list_finalize();
	object_list_finalize();

	cleanup_game_constants();

	cmdq_release();

	alloced_mem_list_destroy(true);

	if (play_again) {
		return;
	}

	/* Free the format() buffer */
	vformat_kill();

	/* Free the directories */
	string_free(ANGBAND_DIR_GAMEDATA);
	string_free(ANGBAND_DIR_CUSTOMIZE);
	string_free(ANGBAND_DIR_HELP);
	string_free(ANGBAND_DIR_SCREENS);
	string_free(ANGBAND_DIR_FONTS);
	string_free(ANGBAND_DIR_TILES);
	string_free(ANGBAND_DIR_SOUNDS);
	string_free(ANGBAND_DIR_ICONS);
	string_free(ANGBAND_DIR_USER);
	string_free(ANGBAND_DIR_SAVE);
	string_free(ANGBAND_DIR_PANIC);
	string_free(ANGBAND_DIR_SCORES);
	string_free(ANGBAND_DIR_ARCHIVE);
}
