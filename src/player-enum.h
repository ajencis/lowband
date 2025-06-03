#ifndef INCLUDED_PLAYER_ENUM_H
#define INCLUDED_PLAYER_ENUM_H

#include "object.h"

/**
 * Indexes of the player stats (hard-coded by savefiles).
 */
enum {
	STAT_NONE = -1,
	#define STAT(a) STAT_##a,
	#include "list-stats.h"
	#undef STAT
	STAT_MAX
};

/**
 * Player race and class flags
 */
enum
{
	#define PF(a) PF_##a,
	#include "list-player-flags.h"
	#undef PF
	PF_MAX
};

enum
{
	PP_SCALE_NONE,
	PP_SCALE_LINEAR,
	PP_SCALE_SQUARE,
	PP_SCALE_SQRT
};

/**
 * L: Player race and class powers
 */
enum
{
	PP_NONE,
	#define PP(x) PP_##x,
	#include "list-player-powers.h"
	#undef PP
	PP_MAX,
};

/**
 * L: Player spell schools
 */
enum
{
	MS_NONE,
	#define MS(x) MS_##x,
	#include "list-magic-schools.h"
	#undef MS
	MS_MAX,
};

/**
 * L: special magic realm abilities
 */
enum
{
	#define RLM_SPCL(x) RLM_SPCL_##x,
	#include "list-realm-special.h"
	#undef RLM_SPCL
	RLM_SPCL_MAX
};

// L: predicates for abilities
enum {
    #define PRED(x) ABIL_PRED_##x,
    #include "list-ability-predicates.h"
    #undef PRED
    ABIL_PRED_MAX
};

#define PF_SIZE                FLAG_SIZE(PF_MAX)

#define pf_has(f, flag)        flag_has_dbg(f, PF_SIZE, flag, #f, #flag)
#define pf_next(f, flag)       flag_next(f, PF_SIZE, flag)
#define pf_is_empty(f)         flag_is_empty(f, PF_SIZE)
#define pf_is_full(f)          flag_is_full(f, PF_SIZE)
#define pf_is_inter(f1, f2)    flag_is_inter(f1, f2, PF_SIZE)
#define pf_is_subset(f1, f2)   flag_is_subset(f1, f2, PF_SIZE)
#define pf_is_equal(f1, f2)    flag_is_equal(f1, f2, PF_SIZE)
#define pf_on(f, flag)         flag_on_dbg(f, PF_SIZE, flag, #f, #flag)
#define pf_off(f, flag)        flag_off(f, PF_SIZE, flag)
#define pf_wipe(f)             flag_wipe(f, PF_SIZE)
#define pf_setall(f)           flag_setall(f, PF_SIZE)
#define pf_negate(f)           flag_negate(f, PF_SIZE)
#define pf_copy(f1, f2)        flag_copy(f1, f2, PF_SIZE)
#define pf_union(f1, f2)       flag_union(f1, f2, PF_SIZE)
#define pf_inter(f1, f2)       flag_inter(f1, f2, PF_SIZE)
#define pf_diff(f1, f2)        flag_diff(f1, f2, PF_SIZE)

/**
 * The range of possible indexes into tables based upon stats.
 * Currently things range from 3 to 18/220 = 40.
 */
#define STAT_RANGE 38

/**
 * Player constants
 */
#define PY_MAX_EXP		100000000L	/* Maximum exp */
#define PY_KNOW_LEVEL	30			/* Level to know all runes */
#define PY_MAX_LEVEL	65			/* Maximum level */

/**
 * Flags for player.spell_flags[]
 */
#define PY_SPELL_LEARNED    0x01 	/* Spell has been learned */
#define PY_SPELL_WORKED     0x02 	/* Spell has been successfully tried */
#define PY_SPELL_FORGOTTEN  0x04 	/* Spell has been forgotten */
#define PY_SPELL_AUTOCAST	0x08	// L: should cast if it runs out

#define BTH_PLUS_ADJ    	3 		/* Adjust BTH per plus-to-hit */

/**
 * Ways in which players can be marked as cheaters
 */
#define NOSCORE_WIZARD		0x0002
#define NOSCORE_DEBUG		0x0008
#define NOSCORE_JUMPING     0x0010
#ifdef ALLOW_BORG
#define NOSCORE_BORG		0x0020
#endif

#define PY_MAX_ATTACKS 15

#define MAX_SPELL_SCHOOLS 3

#define UNLIGHT_MAX_POWER 10

#define MAX_ABIL_PARENTS 3

#define PLAYER_MON_MIDX -5

/**
 * Terrain that the player has a chance of digging through
 */
enum {
	DIGGING_RUBBLE = 0,
	DIGGING_MAGMA,
	DIGGING_QUARTZ,
	DIGGING_GRANITE,
	DIGGING_DOORS,

	DIGGING_MAX
};

/**
 * L: whether the light in the grid in question is too bright or dark
 */
enum {
	PY_SEE_VISIBLE = 0,
	PY_SEE_TOO_DARK,
	PY_SEE_TOO_BRIGHT
};

enum attack_roll_special_effects {
	#define TMD(a, b, c, d, e, f, g, h, i, j) ATK_SPCL_TMD_##a,
	#include "list-player-timed.h"
	#undef TMD
	ATK_SPCL_DEATH_TOUCH,
	ATK_SPCL_BREATH,
	ATK_SPCL_MAX
};

/**
 * Skill indexes
 */
enum {
	#define SKILL(x, a, b, c, d, e) SKILL_##x,
	#include "list-skills.h"
	#undef SKILL
	SKILL_MAX
};


/* L: defines an attack ready to be rolled */
struct attack_roll {
	int ddice;
	int dsides;
	int to_hit;
	int to_dam;
	int special[ATK_SPCL_MAX];
	const char *message;
	char name[32];
	int blows;
	int proj_type;
	int attack_skill;	/* which skill it uses to decide accuracy */
	int accuracy_stat;	/* stat that determines accuracy */
	int damage_stat;	/* stat that determines damage */
	struct object *obj;	/* what weapon is it using */
	int range;			/* how far it can go (eg for a gaze) */
	int crit_chance;	/* % chance of a critical hit */
};


/**
 * A player 'body'
 */
struct player_body {
	struct player_body *next;

	char *name;
	uint16_t count;
	struct equip_slot *slots;
};


/**
 * All the variable state that changes when you put on/take off equipment.
 * Player flags are not currently variable, but useful here so monsters can
 * learn them.
 */
struct player_state {
	int stat_add[STAT_MAX];	/**< Equipment stat bonuses */
	int stat_ind[STAT_MAX];	/**< Indexes into stat tables */
	int stat_use[STAT_MAX];	/**< Current modified stats */
	int stat_top[STAT_MAX];	/**< Maximal modified stats */

	int skills[SKILL_MAX];		/**< Skills */
	int powers[PP_MAX];         /**< L: Powers */

	int speed;			/**< Current speed */

	int num_blows;		/**< Number of blows x100 */
	int num_shots;		/**< Number of shots x10 */
	int num_moves;		/**< Number of extra movement actions */

	int ammo_mult;		/**< Ammo multiplier */
	int ammo_tval;		/**< Ammo variety */

	int ac;				/**< Base ac */
	int dam_red;		/**< Damage reduction */
	int perc_dam_red;	/**< Percentage damage reduction */
	int to_a;			/**< Bonus to ac */
	int to_h;			/**< Bonus to hit */
	int to_d;			/**< Bonus to dam */

	int see_infra;		/**< Infravision range */

	int cur_light;		/**< Radius of light (if any) */

	bool heavy_wield;	/**< Heavy weapon */
	bool heavy_shoot;	/**< Heavy shooter */
	bool bless_wield;	/**< Blessed (or blunt) weapon */

	bool cumber_armor;	/**< Mana draining armor */

	uint32_t expfact;        /**< L: now changes based on int */

	bitflag flags[OF_SIZE];					/**< Status flags from race and items */
	bitflag pflags[PF_SIZE];				/**< Player intrinsic flags */
	struct element_info el_info[ELEM_MAX];	/**< Resists from race and items */

	int num_attacks;							/**< L: number of attacks they currently have available */
	bool has_ranged_attack;						/**< L: whether they have a ranged attack */
	struct attack_roll attacks[PY_MAX_ATTACKS];	/**< L: attacks they currently have available */
	struct attack_roll ranged_attack;			/**< L: attack with shooter */

	int extra_points_used;	/**< L: number of points the player has spent on extra */
	int extra_points_max;	/**< L: number of points the character could spend on extra */
};

#endif
