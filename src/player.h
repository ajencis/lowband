/**
 * \file player.h
 * \brief Player implementation
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2011 elly+angband@leptoquark.net. See COPYING.
 * Copyright (c) 2015 Nick McConnell
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

#ifndef PLAYER_H
#define PLAYER_H

#include "guid.h"
#include "monster.h"
#include "obj-properties.h"
#include "object.h"
#include "option.h"
#include "player-enum.h"


extern const struct player_spell *ref_spell;

struct follower {
	struct follower *next;
	struct monster *mon;
	uint16_t delay;
};

/**
 * Structure for the "quests"
 */
struct quest
{
	struct quest *next;
	uint8_t index;
	char *name;
	uint8_t level;			/* Dungeon level */
	struct monster_race *race;	/* Monster race */
	int cur_num;			/* Number killed (unused) */
	int max_num;			/* Number required (unused) */
};

/**
 * A single equipment slot
 */
struct equip_slot {
	struct equip_slot *next;

	uint16_t type;
	char *name;
	struct object *obj;
};

/**
 * Player race info
 */
struct player_race {
	struct player_race *next;
	const char *name;

	struct monster_race *mon_race;

	unsigned int ridx;

	//int r_mhp;		/**< Hit-dice modifier */
	int r_exp;		/**< Experience factor */

	int b_age;		/**< Base age */
	int m_age;		/**< Mod age */

	int base_hgt;	/**< Base height */
	int mod_hgt;	/**< Mod height */
	int base_wgt;	/**< Base weight */
	int mod_wgt;	/**< Mod weight */

	int infra;		/**< Infra-vision range */

	//struct player_body *body;		/**< Race body */

	//struct evolution *evol;		/**< L: evolutions */

	//int r_adj[STAT_MAX];		/**< Stat bonuses */

	//int r_skills[SKILL_MAX];	/**< Skills */

	//int r_powers[PP_MAX];		/**< L: powers */

	bitflag flags[OF_SIZE];		/**< Racial (object) flags */
	bitflag pflags[PF_SIZE];	/**< Racial (player) flags */

	struct history_chart *history;

	struct element_info el_info[ELEM_MAX]; /**< Resists */
};

/**
 * Blow names for shapechanged players
 */
struct player_blow {
	struct player_blow *next;
	char *name;
};

/**
 * Player shapechange shape info
 */
struct player_shape {
	struct player_shape *next;
	const char *name;

	int sidx;

	int to_a;				/**< Plusses to AC */
	int to_h;				/**< Plusses to hit */
	int to_d;				/**< Plusses to damage */

	int skills[SKILL_MAX];  /**< Skills */
	bitflag flags[OF_SIZE];		/**< Shape (object) flags */
	bitflag pflags[PF_SIZE];	/**< Shape (player) flags */
	int modifiers[OBJ_MOD_MAX];	/**< Stat and other modifiers*/
	struct element_info el_info[ELEM_MAX]; /**< Resists */

	struct effect *effect;	/**< Effect on taking this shape (effects.c) */

	struct player_blow *blows;
	int num_blows;
};

/**
 * Items the player starts with.  Used in player_class and specified in
 * class.txt.
 */
struct start_item {
	int tval;	/**< General object type (see TV_ macros) */
	int sval;	/**< Object sub-type  */
	int min;	/**< Minimum starting amount */
	int max;	/**< Maximum starting amount */
	int *eopts;     /**< Indices (zero terminated array) for birth options which can exclude item */
	struct start_item *next;
};

/**
 * Structure for magic realms
 */
struct magic_realm {
	int index;
	struct magic_realm *next;
	char *code;
	char *name;
	int stat;
	int weight;						/**< L: weight allowance */
	char *verb;
	char *spell_noun;
	char *book_noun;

	int school_modifiers[MS_MAX];		/* L: which schools it does well */
	int realm_special[RLM_SPCL_MAX];	// L: special flags or abilities for the realm
};

/**
 * A structure to hold class-dependent information on spells.
 */
struct class_spell {
	char *name;
	char *text;

	struct effect *effect;	/**< The spell's effect */
	const struct magic_realm *realm;	/**< The magic realm of this spell */

	int school;             /**< L: the school of the spell */

	int sidx;				/**< The index of this spell for this class */
	int bidx;				/**< The index into the player's books array */
	int slevel;				/**< Required level (to learn) */
	int smana;				/**< Required mana (to cast) */
	int sfail;				/**< Base chance of failure */
	int sexp;				/**< Encoded experience bonus */
};

/**
 * A structure to hold non-class-dependent information on spells.
 */
struct player_spell {
	char *name;
	char *text;

	struct player_spell *next;

	struct effect *effect;	/**< The spell's effect */

	int school[MAX_SPELL_SCHOOLS];	/**< L: the school of the spell */

	int sidx;				/**< The index of this spell */
	int slevel;				/**< Required level (to learn) */
	int smana;				/**< Required mana (to cast) */
	int sfail;				/**< Base chance of failure */
};

/**
 * A structure to hold class-dependent information on spell books.
 */
struct class_book {
	int tval;							/**< Item type of the book */
	int sval;							/**< Item sub-type for book */
	bool dungeon;						/**< Whether this is a dungeon book */
	int num_spells;						/**< Number of spells in this book */
	const struct magic_realm *realm;	/**< The magic realm of this book */
	struct class_spell *spells;			/**< Spells in the book*/
};

/**
 * Information about class magic knowledge
 */
struct class_magic {
	int spell_first;			/**< Level of first spell */
	int spell_weight;			/**< Max armor weight to avoid mana penalties */
	int num_books;				/**< Number of spellbooks */
	struct class_book *books;	/**< Details of spellbooks */
	int total_spells;			/**< Number of spells for this class */
};

/**
 * Player class info
 */
struct player_class {
	struct player_class *next;
	const char *name;
	unsigned int cidx;

	const char *title[10];		/**< Titles */

	int c_adj[STAT_MAX];		/**< Stat modifier */

	int c_skills[SKILL_MAX];	/**< Class skills */
	int x_skills[SKILL_MAX];	/**< Extra skills */

	int c_powers[PP_MAX];       /**< L: Class powers */

	int c_mhp;					/**< Hit-dice adjustment */
	int c_exp;					/**< Experience factor */

	bitflag flags[OF_SIZE];		/**< (Object) flags */
	bitflag pflags[PF_SIZE];	/**< (Player) flags */

	int max_attacks;			/**< Maximum possible attacks */
	int min_weight;				/**< Minimum weapon weight for calculations */
	int att_multiply;			/**< Multiplier for attack calculations */

	struct start_item *start_items; /**< Starting inventory */

	struct class_magic magic;	/**< Magic spells */
	struct magic_realm *realm;	/**< L: realm it casts with */

	bool unlockable;			// L: does it need unlocking

	bool prereqs[ABIL_PRED_MAX];	// L: what is needed to take the class
};

/**
 * Info for player abilities
 */
struct player_ability {
	struct player_ability *next;
	uint16_t index;			/* PF_*, OF_* or element index */
	int type;			/* Ability type (PY_ABIL_*) */
	char *name;			/* Ability name */
	char *desc;			/* Ability description */
	int group;			/* Ability group (set locally when viewing) */
	int value;			/* Resistance value for elements */

	// L: description data for powers
	char *second_verb;
	char *third_verb;
	char *pos_adjective;
	char *neg_adjective;
	char *comment;

	// L: learning data
	int cost;						// L: how much it costs to max out
	int rarity;						// L: how likely it is to be found
	int scale;						// L: does it scale other than linearly with level
	int learn_index;				// L: what is its index of all learnable abilities
	struct player_ability *parent[MAX_ABIL_PARENTS];	// L: what abilities are needed to learn first
	bool prereqs[ABIL_PRED_MAX];		// L: index of prerequisites needed to learn it
};

/**
 * Histories are a graph of charts; each chart contains a set of individual
 * entries for that chart, and each entry contains a text description and a
 * successor chart to move history generation to.
 * For example:
 * 	chart 1 {
 * 		entry {
 * 			desc "You are the illegitimate and unacknowledged child";
 * 			next 2;
 * 		};
 * 		entry {
 * 			desc "You are the illegitimate but acknowledged child";
 * 			next 2;
 * 		};
 * 		entry {
 * 			desc "You are one of several children";
 * 			next 3;
 * 		};
 * 	};
 *
 * History generation works by walking the graph from the starting chart for
 * each race, picking a random entry (with weighted probability) each time.
 */
struct history_entry {
	struct history_entry *next;
	struct history_chart *succ;
	int isucc;
	int roll;
	char *text;
};

struct history_chart {
	struct history_chart *next;
	struct history_entry *entries;
	unsigned int idx;
};

/**
 * Player history information
 *
 * See player-history.c/.h
 */
struct player_history {
	struct history_info *entries;	/**< List of entries */
	size_t next;					/**< First unused entry */
	size_t length;					/**< Current length */
};

#define player_has(p, flag)       (pf_has(p->mon.state.pflags, (flag)))

/**
 * Temporary, derived, player-related variables used during play but not saved
 *
 * XXX Some of these probably should go to the UI
 */
struct player_upkeep {
	bool playing;			/* True if player is playing */
	bool autosave;			/* True if autosave is pending */
	bool generate_level;	/* True if level needs regenerating */
	bool only_partial;		/* True if only partial updates are needed */
	bool dropping;			/* True if auto-drop is in progress */

	int energy_use;			/* Energy use this turn */
	int taking_stairs;      /* L: how long they stay on the stairs */
	int new_spells;			/* Number of spells available */

	struct monster *health_who;			/* Health bar trackee */
	struct monster_race *monster_race;	/* Monster race trackee */
	struct object *object;				/* Object trackee */
	struct object_kind *object_kind;	/* Object kind trackee */

	uint32_t notice;		/* Bit flags for pending actions such as
							 * reordering inventory, ignoring, etc. */
	uint32_t update;		/* Bit flags for recalculations needed
							 * such as HP, or visible area */
	uint32_t redraw;		/* Bit flags for things that /have/ changed,
							 * and just need to be redrawn by the UI,
							 * such as HP, Speed, etc.*/

	int command_wrk;		/* Used by the UI to decide whether
							 * to start off showing equipment or
							 * inventory listings when offering
							 * a choice.  See obj-ui.c */

	bool create_up_stair;	/* Create up stair on next level */
	bool create_down_stair;	/* Create down stair on next level */
	bool light_level;		/* Level is to be lit on creation */
	bool arena_level;		/* Current level is an arena */

	int resting;			/* Resting counter */

	int running;				/* Running counter */
	bool running_firststep;		/* Is this our first step running? */

	struct object **quiver;	/* Quiver objects */
	struct object **inven;	/* Inventory objects */
	int total_weight;		/* Total weight being carried */
	int inven_cnt;			/* Number of items in inventory */
	int equip_cnt;			/* Number of items in equipment */
	int quiver_cnt;			/* Number of items in the quiver */
	int recharge_pow;		/* Power of recharge effect */
	int step_count;			/* Pathfinding: number of steps left */
	int16_t *steps;			/* Pathfinding: steps in reverse order */
	struct loc path_dest;	/* Pathfinding: destination grid */

	struct follower *follow;	/* L: monster trying to get to the player */
	struct loc entered;			/* L: the grid where they entered the level */
};

/**
 * Most of the "player" information goes here.
 *
 * This stucture gives us a large collection of player variables.
 *
 * This entire structure is wiped when a new character is born.
 *
 * This structure is more or less laid out so that the information
 * which must be saved in the savefile precedes all the information
 * which can be recomputed as needed.
 */
struct player {
	const struct player_race *race;
	const struct player_class *class;

	struct monster mon;	// L: player as a monster

	//struct loc grid;	/* Player location */
	struct loc old_grid;/* Player location before leaving for an arena */

	//uint8_t hitdie;		/* Hit dice (sides) */

	int16_t age;		/* Characters age */
	int16_t ht;		/* Height */
	int16_t wt;		/* Weight */

	int32_t au;		/* Current Gold */
	int32_t expfact;

	int16_t max_depth;	/* Max depth */
	int16_t recall_depth;	/* Recall depth */
	int16_t depth;		/* Cur depth */

	int16_t max_lev;	/* Max level */
	int16_t lev;		/* Cur level */

	uint32_t max_exp;	/* Max experience */
	uint32_t exp;		/* Cur experience */
	uint16_t exp_frac;	/* Cur exp frac (times 2^16) */

	uint16_t chp_frac;	/* Cur hit frac (times 2^16) */

	int16_t msp;		/* Max mana pts */
	int16_t csp;		/* Cur mana pts */
	uint16_t csp_frac;	/* Cur mana frac (times 2^16) */
	uint16_t floor_mana;    /* L: amount of mana on the current floor */

	int16_t stat_max[STAT_MAX];	/* Current "maximal" stat values */
	int16_t stat_cur[STAT_MAX];	/* Current "natural" stat values */
	int16_t stat_map[STAT_MAX];	/* Tracks remapped stats from temp stat swap */
	int16_t stat_max_max[STAT_MAX]; /* Cap of increases to stats */

	int16_t word_recall;		/* Word of recall counter */
	int16_t deep_descent;		/* Deep Descent counter */

	int16_t energy;				/* Current energy */
	uint32_t total_energy;		/* Total energy used (including resting) */
	uint32_t resting_turn;		/* Number of player turns spent resting */
	uint32_t search_turn;		// L: Number of player turns spent consecutively searching on the current square

	int16_t food;				/* Current nutrition */

	uint8_t unignoring;			/* Unignoring */

	uint8_t skip_cmd_coercion;		/* True if bloodlust check
							should be skipped on
							the next command
							(previous command
							successfully passed
							the bloodlust check
							but then was canceled
							by the user) */
	uint8_t *spell_flags;			/* Spell flags */
	uint8_t *spell_order;			/* Spell order */

	char full_name[PLAYER_NAME_LEN];	/* Full name */
	char died_from[80];					/* Cause of death */
	char *history;						/* Player history */
	struct quest *quests;				/* Quest history */
	uint16_t total_winner;				/* Total winner */

	uint16_t noscore;					/* Cheating flags */

	bool is_dead;						/* Player is dead */

	bool wizard;						/* Player is in wizard mode */

	//int16_t player_hp[PY_MAX_LEVEL];	/* HP gained per level */

	/* Saved values for quickstart */
	int32_t au_birth;					/* Birth gold when option birth_money is false */
	int16_t stat_birth[STAT_MAX];		/* Birth "natural" stat values */
	int16_t ht_birth;					/* Birth Height */
	int16_t wt_birth;					/* Birth Weight */

	struct player_options opts;			/* Player options */
	struct player_history hist;			/* Player history (see player-history.c) */

	//struct player_body body;			/* Equipment slots available */
	struct player_shape *shape;			/* Current player shape */

	//struct object *gear;				/* Real gear */
	struct object *gear_k;				/* Known gear */

	struct object *obj_k;				/* Object knowledge ("runes") */
	struct chunk *cave;					/* Known version of current level */

	//struct player_state state;		/* Calculatable state */
	struct player_state known_state;	/* What the player can know of the above */
	struct player_upkeep *upkeep;		/* Temporary player-related values */

	const struct monster_race **evol_choices;	// L: which monster the player is choosing to evolve into
	int num_evol_choices;				// L: how many evolution choices the player has made

	int16_t *extra_learned;				// L: what the player has learned
	uint16_t *extra_target;				// L: what the player wants to learn
	int16_t *extra_choice;				// L: choices made regarding abilities, eg magic realm
	int sp_burn;						/* L: temporary reduction of max mp */

	uint8_t *player_spell_flags;		/* L: for nonclass spells */
	uint8_t *player_spell_order;		/* L: for nonclass spells */
	const struct magic_realm *realm;	/* L: how the player casts */

	int32_t xp_this_turn;				/* L: how much xp was gained between turns */
	bool searched_this_turn;			// L: have we searched this turn?
	uint32_t monster_xp;				/* L: XP progression towards evolution */
	bool checked_tome_this_expedition;	/* L: have we prompted for a tome this level? */

	bool *unlocked_classes;				// L: which classes the player has unlocked
	bool *unlocked_races;				// L: unlocked races
	uint16_t *unlocked_tomes;			// L: unlocked_powers with quantity

	int16_t curr_noise;					// L: how loud they currently are being
};


/**
 * ------------------------------------------------------------------------
 * Externs
 * ------------------------------------------------------------------------ */

extern struct player_body *bodies;
extern struct player_race *races;
extern struct player_shape *shapes;
extern struct player_class *classes;
extern struct player_ability *player_abilities;
extern struct magic_realm *realms;
extern struct player_spell *spells;

extern const uint32_t player_exp[PY_MAX_LEVEL];
extern struct player *player;

/* player-class.c */
struct player_class *player_id2class(guid id);

/* player.c */
int stat_name_to_idx(const char *name);
const char *stat_idx_to_name(int type);
const struct magic_realm *lookup_realm(const char *code);
bool player_stat_inc(struct player *p, int stat);
bool player_stat_dec(struct player *p, int stat, bool permanent);
bool player_at_max_level(struct player *p);
bool player_can_level_up(struct player *p);
void player_exp_gain(struct player *p, uint32_t amount, uint32_t fract);
void player_exp_lose(struct player *p, int32_t amount, bool permanent);
void player_level_up_one(struct player *p, bool verbose);
void check_level(struct player *p);
void player_flags(struct player *p, bitflag f[OF_SIZE]);
void player_flags_timed(struct player *p, bitflag f[OF_SIZE]);
uint8_t player_hp_attr(struct player *p);
uint8_t player_sp_attr(struct player *p);
bool player_restore_mana(struct player *p, int amt);
size_t player_random_name(char *buf, size_t buflen);
void player_safe_name(char *safe, size_t safelen, const char *name, bool strip_suffix);
void player_cleanup_members(struct player *p);
int player_min_xp_depth(struct player *p);

/* player-race.c */
struct player_race *player_id2race(guid id);
int max_race_evol_lev(struct player_race *r);

#endif /* !PLAYER_H */
