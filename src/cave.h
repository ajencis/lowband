/**
 * \file cave.h
 * \brief Matters relating to the current dungeon level
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

#ifndef CAVE_H
#define CAVE_H

#include "z-type.h"
#include "z-bitflag.h"

struct player;
struct monster;
struct monster_group;

extern const int16_t ddd[9];
extern const int16_t ddx[10];
extern const int16_t ddy[10];
extern const struct loc ddgrid[10];
extern const int16_t ddx_ddd[9];
extern const int16_t ddy_ddd[9];
extern const struct loc ddgrid_ddd[9];
extern const int16_t clockwise_ddd[9];
extern const struct loc clockwise_grid[9];
extern const int *dist_offsets_y[10];
extern const int *dist_offsets_x[10];
extern const uint8_t side_dirs[20][8];

typedef bool (*feat_predicate)(int feat);

int loc_to_dir(struct loc grid);

enum {
	DIR_UNKNOWN = 0,
	DIR_NW = 7,
	DIR_N = 8,
	DIR_NE = 9,
	DIR_W = 4,
	DIR_TARGET = 5,
	DIR_NONE = 5,
	DIR_E = 6,
	DIR_SW = 1,
	DIR_S = 2,
	DIR_SE = 3,
};

/**
 * Square flags
 */

enum
{
	#define SQUARE(a,b) SQUARE_##a,
	#include "list-square-flags.h"
	#undef SQUARE
	SQUARE_MAX
};


#define MAX_CAVE_HEIGHT 256
#define MAX_CAVE_WIDTH 512

#define SQUARE_SIZE                FLAG_SIZE(SQUARE_MAX)

#define sqinfo_has(f, flag)        flag_has_dbg(f, SQUARE_SIZE, flag, #f, #flag)
#define sqinfo_next(f, flag)       flag_next(f, SQUARE_SIZE, flag)
#define sqinfo_is_empty(f)         flag_is_empty(f, SQUARE_SIZE)
#define sqinfo_is_full(f)          flag_is_full(f, SQUARE_SIZE)
#define sqinfo_is_inter(f1, f2)    flag_is_inter(f1, f2, SQUARE_SIZE)
#define sqinfo_is_subset(f1, f2)   flag_is_subset(f1, f2, SQUARE_SIZE)
#define sqinfo_is_equal(f1, f2)    flag_is_equal(f1, f2, SQUARE_SIZE)
#define sqinfo_on(f, flag)         flag_on_dbg(f, SQUARE_SIZE, flag, #f, #flag)
#define sqinfo_off(f, flag)        flag_off(f, SQUARE_SIZE, flag)
#define sqinfo_wipe(f)             flag_wipe(f, SQUARE_SIZE)
#define sqinfo_setall(f)           flag_setall(f, SQUARE_SIZE)
#define sqinfo_negate(f)           flag_negate(f, SQUARE_SIZE)
#define sqinfo_copy(f1, f2)        flag_copy(f1, f2, SQUARE_SIZE)
#define sqinfo_union(f1, f2)       flag_union(f1, f2, SQUARE_SIZE)
#define sqinfo_inter(f1, f2)       flag_inter(f1, f2, SQUARE_SIZE)
#define sqinfo_diff(f1, f2)        flag_diff(f1, f2, SQUARE_SIZE)


/**
 * Terrain flags
 */
enum
{
	#define TF(a,b) TF_##a,
	#include "list-terrain-flags.h"
	#undef TF
	TF_MAX
};

enum {
	#define T_ELEM(x) TE_##x,
	#include "list-terrain-elements.h"
	#undef T_ELEM
	TE_MAX
};

#define TF_SIZE                FLAG_SIZE(TF_MAX)

#define tf_has(f, flag)        flag_has_dbg(f, TF_SIZE, flag, #f, #flag)

/**
 * Information about terrain features.
 *
 * At the moment this isn't very much, but eventually a primitive flag-based
 * information system will be used here.
 */
struct feature_kind {
	char *name;
	char *desc;
	int fidx;

	struct feature_kind *mimic;		/**< Feature to mimic or NULL for no mimicry */
	uint8_t priority;			/**< Display priority */

	uint8_t shopnum;			/**< Which shop does it take you to? */
	uint8_t dig;				/**< How hard is it to dig through? */

	bitflag flags[TF_SIZE];		/**< Terrain flags */

	uint8_t d_attr;				/**< Default feature attribute */
	wchar_t d_char;				/**< Default feature character */

	char *walk_msg;				/**< Message on walking into feature */
	char *run_msg;				/**< Message on running into feature */
	char *hurt_msg;				/**< Message on being hurt by feature */
	char *die_msg;				/**< Message on dying to feature */
	char *confused_msg;			/**< Message on confused monster move into feature */
	char *look_prefix;			/**< Prefix for name in look result */
	char *look_in_preposition;	/**< Preposition in look result when on the terrain */
	int resist_flag;			/**< Monster resist flag for entering feature */

	uint8_t t_elem[TE_MAX];		// L: which terrain elements it produces in what quantity
	char *t_elem_msg;			// L: message used when it produces terrain elements

	int timeout;				// L: loses this much of its size / 100 turns

	int feat_produce;			// L: which other feat it produces
	int feat_produce_quantity;	// L: how much of that feat it produces
	int feat_produce_frequency;	// L: how often it produces that feat / 100 turns

	int proj;					// L: what it projects nearby
	int proj_range;				// L: how far it projects
	int proj_amt;				// L: how much it projects
};

struct feature {
	struct feature *next;
	struct feature_kind *kind;

	int size;
};

extern struct feature_kind *f_info;

struct terrain_element_level {
	struct terrain_element_level *next;
	int min_dur;

	char *name;

	uint8_t d_attr;
	uint8_t d_char;

	int timeout;
	
	int proj;
	int proj_range;

	bitflag flags[TF_SIZE];
};

/**
 * L: information about additional temporary terrain 
 */
struct terrain_element_kind {
	struct terrain_element_kind *next;

	int idx;

	struct terrain_element_level *levels;
	/*uint16_t idx;

	char *name;

	uint8_t d_attr;
	uint8_t d_char;

	int timeout;				// if not 0 it loses one point every  timeout  turns

	int proj;					// the projection it projects on its square
	int proj_range;				// how far from its square it projects, default 0 (only the square in question);
	bitflag flags[TF_SIZE];*/
};

struct terrain_element {
	const struct terrain_element_kind *kind;
	struct terrain_element *next;

	int timer;
};

extern struct terrain_element_kind *te_info;

enum grid_light_level
{
	LIGHTING_LOS = 0,   /* line of sight */
	LIGHTING_TORCH,     /* torchlight */
	LIGHTING_LIT,       /* permanently lit (when not in line of sight) */
	LIGHTING_DARK,      /* dark */
	LIGHTING_MAX
};

struct grid_data {
	uint32_t m_idx;			/* Monster index */
	int f_idx;			/* Feature index */

	struct object_kind *first_kind;	/* The kind of the first item on the grid */
	struct trap *trap;		/* Trap */
	struct terrain_element *t_elem;
	const struct feature *feat;

	bool multiple_objects;	/* Is there more than one item there? */
	bool unseen_object;		/* Is there an unaware object there? */
	bool unseen_money;		/* Is there some unaware money there? */

	enum grid_light_level lighting; /* Light level */
	bool in_view; 			/* Can the player can currently see the grid? */
	bool is_player;
	bool hallucinate;
};

struct square {
	//uint8_t feat_old;
	bitflag *info;
	int light;
	int16_t mon;
	int8_t mana;
	struct object *obj;
	struct trap *trap;
	struct terrain_element *t_elem;

	struct feature *feat;

	int required_rf;		// required race flag to be generated here
};

struct heatmap {
	int16_t **grids;
};

struct connector {
	struct loc grid;
	uint8_t feat;
	bitflag *info;
	struct connector *next;
};

struct chunk {
	char *name;
	int32_t turn;
	int depth;

	uint8_t feeling;
	uint32_t obj_rating;
	uint32_t mon_rating;
	bool good_item;

	int height;
	int width;

	uint16_t feeling_squares; /* How many feeling squares the player has visited */
	int *feat_count;
	int squares_everseen; /* L: how many suares have been XP-checked */

	struct square **squares;
	struct heatmap noise;
	struct heatmap scent;
	struct loc decoy;

	struct object **objects;
	uint16_t obj_max;

	struct monster *monsters;
	uint16_t mon_max;
	uint16_t mon_cnt;
	int mon_current;
	int num_repro;

	struct monster_group **monster_groups;

	struct connector *join;

	const struct feature_kind *feat_default; // L: what feat the cave uses if it has no others

	struct point_set *timeout_points;
	struct point_set *spread_points;
	struct point_set *produce_points;
	struct point_set *project_points;
};

typedef bool (*square_predicate)(struct chunk *c, struct loc grid);

/*** Feature Indexes (see "lib/gamedata/terrain.txt") ***/
enum {
	#define FEAT(x) FEAT_##x,
	#include "list-terrain.h"
	#undef FEAT
	FEAT_MAX
};


struct point_set_match {
	struct point_set *(*set_get)(struct chunk *);
	feat_predicate pred;
	const char *name;
};

extern struct point_set_match point_set_matches[];

/* Current level */
extern struct chunk *cave;
/* Stored levels */
extern struct chunk **chunk_list;
extern uint16_t chunk_list_max;

/* cave-view.c */
int distance(struct loc grid1, struct loc grid2);
bool los(struct chunk *c, struct loc grid1, struct loc grid2);
void update_view(struct chunk *c, struct player *p);
bool no_light(const struct player *p);

/* cave-terrain-elem.c */
bool square_feat_valid(struct chunk *c, struct loc grid);
bool square_force_add_feat(struct chunk *c, struct loc grid, int fidx, int size);
bool feat_incompat_base(int feat1, int feat2);
void cave_set_default_feat(struct chunk *c, int fidx);

bool square_add_feat(struct chunk *c, struct loc grid, int fidx, int size);
bool square_remove_feat(struct chunk *c, struct loc grid, int fidx);
bool square_force_remove_feat(struct chunk *c, struct loc grid, int fidx);
void square_free_feats(struct chunk *c, struct loc grid);

int feat_believed(struct player *p, struct loc grid, int feat);
void square_memorize_feats(struct player *p, const struct chunk *c, struct loc grid);
void square_memorize_struct_feats(struct player *p, const struct chunk *c, struct loc grid);
void square_memorize_feat_real(struct player *p, struct chunk *c, struct loc grid, int fidx);
void square_memorize_feat_one(struct player *p, struct loc grid, const struct feature *feat, bool real);
void square_forget_feats(struct player *p, struct loc grid);
void square_ensure_correct_memorization_by_pred(struct player *p, struct chunk *c, struct loc grid, feat_predicate pred);

bool feat_is_hidden(struct player *p, struct loc grid, int fidx);
void square_copy_feat(struct chunk *from_c, struct chunk *to_c, struct loc from_grid, struct loc to_grid);

void cave_feat_upkeep(struct chunk *c);
void cave_feat_initial_upkeep(struct chunk *c);

struct terrain_element_kind *t_elem_kind_by_idx(int idx);
bool t_elem_has_flag(const struct terrain_element *t_elem, int flag);
const char *t_elem_name(const struct terrain_element *t_elem);
uint8_t t_elem_d_attr(const struct terrain_element *t_elem);
uint8_t t_elem_d_char(const struct terrain_element *t_elem);
int t_elem_timeout(const struct terrain_element *t_elem);
int t_elem_proj(const struct terrain_element *t_elem);
int t_elem_proj_range(const struct terrain_element *t_elem);

bool t_elem_reduces(const struct terrain_element *t_elem);

bool t_elem_is_los(const struct terrain_element_kind *kind);
bool sq_any_t_elem_has_flag(const struct square *sq, int flag);
bool sq_all_t_elem_has_flag(const struct square *sq, int flag);
void square_memorize_t_elem(struct chunk *c, struct loc grid);
int t_elem_timer(struct chunk *c, struct loc grid, uint16_t which);
struct terrain_element *square_t_elem_by_type(struct chunk *c, struct loc grid, uint16_t which);

bool grid_is_danger(const struct monster *mon, struct chunk *c, struct loc grid);
bool mon_in_t_elem_danger(const struct monster *mon, struct chunk *c);

int burn_square(struct chunk *c, struct loc grid, int power);

struct terrain_element *terrain_element_new(int timer, int idx);
struct terrain_element_level *t_elem_level(const struct terrain_element_kind *kind, int timer);
void terrain_elem_free(struct terrain_element *to_free);
bool terrain_element_add(struct chunk *c, struct loc grid, int idx, uint16_t timer);
bool terrain_elem_remove(struct chunk *c, struct loc grid, uint16_t idx);
int terrain_element_increase_dur(struct chunk *c, struct loc grid, uint16_t idx, uint16_t change);
int terrain_element_reduce_dur(struct chunk *c, struct loc grid, uint16_t idx, uint16_t change);
bool terrain_elem_remove_all(struct chunk *c, struct loc grid);
int terrain_element_change_dur(struct chunk *c, struct loc grid, uint16_t idx, int change);

void t_elem_spread(struct chunk *c);
void t_elem_reduce_durations(struct chunk *c);
void t_elem_effects(struct chunk *c);

bool cave_produce_t_elem(struct chunk *c);

void cave_handle_t_elem(struct chunk *c);
void cave_init_t_elem(struct chunk *c, int times);

/* cave-map.c */
void map_info(struct loc grid, struct grid_data *g);
void square_note_spot(struct chunk *c, struct loc grid);
void square_light_spot(struct chunk *c, struct loc grid);
void light_room(struct loc grid, bool light);
void wiz_light(struct chunk *c, struct player *p, bool full);
void wiz_dark(struct chunk *c, struct player *p, bool full);
void cave_illuminate(struct chunk *c, bool daytime);
void expose_to_sun(struct chunk *c, struct loc grid, bool daytime);
void cave_update_flow(struct chunk *c);
void cave_forget_flow(struct chunk *c);
int all_contiguous_locs(struct chunk *c, struct loc center, struct loc *locs, int locs_size,
	square_predicate pred, bool (*move_pred)(struct chunk *c, struct loc gridfrom, struct loc gridto));

	
/* cave-square.c */

/* FEATURE PREDICATES */
bool feat_is_magma(int feat);
bool feat_is_quartz(int feat);
bool feat_is_granite(int feat);
bool feat_is_treasure(int feat);
bool feat_is_wall(int feat);
bool feat_is_floor(int feat);
bool feat_is_structural(int feat);
bool feat_is_trap_holding(int feat);
bool feat_is_object_holding(int feat);
bool feat_is_monster_walkable(int feat);
bool feat_is_shop(int feat);
bool feat_is_los(int feat);
bool feat_is_passable(int feat);
bool feat_is_projectable(int feat);
bool feat_is_torch(int feat);
bool feat_is_bright(int feat);
bool feat_is_fiery(int feat);
bool feat_is_damaging(int feat);
bool feat_is_no_flow(int feat);
bool feat_is_no_scent(int feat);
bool feat_is_smooth(int feat);
bool feat_is_rubble(int feat);
bool feat_is_open_door(int feat);
bool feat_is_closed_door(int feat);
bool feat_is_broken_door(int feat);
bool feat_is_door(int feat);
bool feat_is_secret_door(int feat);
bool feat_is_permanent(int feat);
bool feat_is_up_stairs(int feat);
bool feat_is_down_stairs(int feat);
bool feat_is_stairs(int feat);
bool feat_is_diggable(int feat);
bool feat_gets_mapped(int fidx);

bool feat_times_out(int fidx);
bool feat_spreads(int fidx);
bool feat_produces(int fidx);
bool feat_projects(int fidx);

/* SQUARE FEATURE PREDICATES */
bool square_isfloor(struct chunk *c, struct loc grid);
bool square_istrappable(struct chunk *c, struct loc grid);
bool square_isobjectholding(struct chunk *c, struct loc grid);
bool square_isrock(struct chunk *c, struct loc grid);
bool square_isgranite(struct chunk *c, struct loc grid);
bool square_isperm(struct chunk *c, struct loc grid);
bool square_ismagma(struct chunk *c, struct loc grid);
bool square_isquartz(struct chunk *c, struct loc grid);
bool square_ismineral(struct chunk *c, struct loc grid);
bool square_hasgoldvein(struct chunk *c, struct loc grid);
bool square_isrubble(struct chunk *c, struct loc grid);
bool square_issecretdoor(struct chunk *c, struct loc grid);
bool square_isopendoor(struct chunk *c, struct loc grid);
bool square_iscloseddoor(struct chunk *c, struct loc grid);
bool square_isbrokendoor(struct chunk *c, struct loc grid);
bool square_isdoor(struct chunk *c, struct loc grid);
bool square_isstairs(struct chunk *c, struct loc grid);
bool square_isupstairs(struct chunk *c, struct loc grid);
bool square_isdownstairs(struct chunk *c, struct loc grid);
bool square_isshop(struct chunk *c, struct loc grid);
bool square_isplayer(struct chunk *c, struct loc grid);
bool square_isoccupied(struct chunk *c, struct loc grid);
bool square_isknown(struct chunk *c, struct loc grid);
bool square_ismemorybad(struct chunk *c, struct loc grid);
bool square_iswallsurrounded(struct chunk *c, struct loc grid);

/* SQUARE INFO PREDICATES */
bool square_ismark(struct chunk *c, struct loc grid);
bool square_isglow(struct chunk *c, struct loc grid);
bool square_isvault(struct chunk *c, struct loc grid);
bool square_isroom(struct chunk *c, struct loc grid);
bool square_isseen(struct chunk *c, struct loc grid);
bool square_isview(struct chunk *c, struct loc grid);
bool square_wasseen(struct chunk *c, struct loc grid);
bool square_isfeel(struct chunk *c, struct loc grid);
bool square_istrap(struct chunk *c, struct loc grid);
bool square_isinvis(struct chunk *c, struct loc grid);
bool square_iswall_inner(struct chunk *c, struct loc grid);
bool square_iswall_outer(struct chunk *c, struct loc grid);
bool square_iswall_solid(struct chunk *c, struct loc grid);
bool square_iswall(struct chunk *c, struct loc grid);
bool square_ismon_restrict(struct chunk *c, struct loc grid);
bool square_isno_teleport(struct chunk *c, struct loc grid);
bool square_isno_map(struct chunk *c, struct loc grid);
bool square_isno_esp(struct chunk *c, struct loc grid);
bool square_isproject(struct chunk *c, struct loc grid);
bool square_isdtrap(struct chunk *c, struct loc grid);
bool square_isno_stairs(struct chunk *c, struct loc grid);
bool square_hasunknownitem(struct chunk *c, struct loc grid);

/* SQUARE BEHAVIOR PREDICATES */
bool square_isopen(struct chunk *c, struct loc grid);
bool square_isempty(struct chunk *c, struct loc grid);
bool square_isarrivable(struct chunk *c, struct loc grid);
bool square_canputitem(struct chunk *c, struct loc grid);
bool square_canputterrainelem(struct chunk *c, struct loc grid, uint16_t idx);
bool square_isdiggable(struct chunk *c, struct loc grid);
bool square_iswebbable(struct chunk *c, struct loc grid);
bool square_is_monster_walkable(struct chunk *c, struct loc grid);
bool square_ispassable(struct chunk *c, struct loc grid);
bool square_isprojectable(struct chunk *c, struct loc grid);
bool square_allowsfeel(struct chunk *c, struct loc grid);
bool square_allowslos(struct chunk *c, struct loc grid);
bool square_isstrongwall(struct chunk *c, struct loc grid);
bool square_isbright(struct chunk *c, struct loc grid);
bool square_isfiery(struct chunk *c, struct loc grid);
bool square_islit(struct chunk *c, struct loc grid);
bool square_isdamaging(struct chunk *c, struct loc grid);
bool square_isnoflow(struct chunk *c, struct loc grid);
bool square_isnoscent(struct chunk *c, struct loc grid);
bool square_iswarded(struct chunk *c, struct loc grid);
bool square_isdecoyed(struct chunk *c, struct loc grid);
bool square_iswebbed(struct chunk *c, struct loc grid);
bool square_seemslikewall(struct chunk *c, struct loc grid);
bool square_isinteresting(struct chunk *c, struct loc grid);
bool square_islockeddoor(struct chunk *c, struct loc grid);
bool square_isunlockeddoor(struct chunk *c, struct loc grid);
bool square_isplayertrap(struct chunk *c, struct loc grid);
bool square_isvisibletrap(struct chunk *c, struct loc grid);
bool square_issecrettrap(struct chunk *c, struct loc grid);
bool square_isdisabledtrap(struct chunk *c, struct loc grid);
bool square_isdisarmabletrap(struct chunk *c, struct loc grid);
bool square_dtrap_edge(struct chunk *c, struct loc grid);
bool square_changeable(struct chunk *c, struct loc grid);
bool square_in_bounds(struct chunk *c, struct loc grid);
bool square_in_bounds_fully(struct chunk *c, struct loc grid);
bool square_isbelievedwall(struct chunk *c, struct loc grid);
bool square_isknownpassable(struct chunk *c, struct loc grid);
bool square_suits_stairs_well(struct chunk *c, struct loc grid);
bool square_suits_stairs_ok(struct chunk *c, struct loc grid);
bool square_allows_summon(struct chunk *c, struct loc grid);

const char *square_impassable_name(struct chunk *c, struct loc grid);

struct feature *first_feat_with_flag(struct chunk *c, struct loc grid, int flag);
struct feature *first_feat_meets_pred(struct chunk *c, struct loc grid, bool (*pred)(int));
struct feature *first_feat_not_meets_pred(struct chunk *c, struct loc grid, bool (*pred)(int));
struct feature *square_feat(struct chunk *c, struct loc grid);
bool square_has_feat(const struct chunk *c, struct loc grid, int fidx);
struct feature *square_feat_by_type(struct chunk *c, struct loc grid, int fidx);
bool square_add_feat(struct chunk *c, struct loc grid, int feat, int size);
void square_clear_feats(struct chunk *c, struct loc grid);
bool square_set_feat(struct chunk *c, struct loc grid, int fidx, int size);
bool square_force_set_feat(struct chunk *c, struct loc grid, int fidx, int size);
bool feats_equal(const struct feature *feat1, const struct feature *feat2);
bool square_remove_feats_by_flag(struct chunk *c, struct loc grid, int flag);
bool square_change_feat(struct chunk *c, struct loc grid, int old, int new);
void square_remove_feat_by_type(struct chunk *c, struct loc grid, bool (*pred)(int));

const struct square *square(struct chunk *c, struct loc grid);
int square_light(struct chunk *c, struct loc grid);
struct monster *square_monster(struct chunk *c, struct loc grid);
struct object *square_object(struct chunk *c, struct loc grid);
struct trap *square_trap(struct chunk *c, struct loc grid);
struct terrain_element *square_t_elem(struct chunk *c, struct loc grid);
bool square_holds_object(struct chunk *c, struct loc grid, struct object *obj);
void square_excise_object(struct chunk *c, struct loc grid, struct object *obj);
void square_excise_pile(struct chunk *c, struct loc grid);
void square_excise_all_imagined(struct chunk *p_c, struct chunk *c,
		struct loc grid);
void square_delete_object(struct chunk *c, struct loc grid, struct object *obj, bool do_note, bool do_light);
void square_sense_pile(struct chunk *c, struct loc grid,
		bool (*pred)(const struct object*));
void square_know_pile(struct chunk *c, struct loc grid,
		bool (*pred)(const struct object*));
void square_know_equipped_object(struct chunk *c, struct loc grid,
		bool (*pred)(const struct object*));
int square_num_walls_adjacent(struct chunk *c, struct loc grid);
int square_num_walls_diagonal(struct chunk *c, struct loc grid);


/* Feature placers */
//void square_set_feat_old(struct chunk *c, struct loc grid, int feat);
void square_set_mon(struct chunk *c, struct loc grid, int midx);
void square_set_obj(struct chunk *c, struct loc grid, struct object *obj);
void square_set_trap(struct chunk *c, struct loc grid, struct trap *trap);
void square_add_trap(struct chunk *c, struct loc grid);
void square_add_glyph(struct chunk *c, struct loc grid, int type);
void square_add_web(struct chunk *c, struct loc grid);
void square_add_stairs(struct chunk *c, struct loc grid, int depth);
void square_add_door(struct chunk *c, struct loc grid, bool closed);
void square_t_elem_add(struct chunk *c, struct loc grid, uint16_t idx, int timer);

/* Feature modifiers */
void square_open_door(struct chunk *c, struct loc grid);
void square_close_door(struct chunk *c, struct loc grid);
void square_smash_door(struct chunk *c, struct loc grid);
void square_unlock_door(struct chunk *c, struct loc grid);
void square_destroy_door(struct chunk *c, struct loc grid);
void square_destroy_trap(struct chunk *c, struct loc grid);
void square_disable_trap(struct chunk *c, struct loc grid);
void square_destroy_decoy(struct chunk *c, struct loc grid);
void square_tunnel_wall(struct chunk *c, struct loc grid);
void square_destroy_wall(struct chunk *c, struct loc grid);
void square_smash_wall(struct chunk *c, struct loc grid);
void square_destroy(struct chunk *c, struct loc grid);
void square_earthquake(struct chunk *c, struct loc grid);
void square_upgrade_mineral(struct chunk *c, struct loc grid);
void square_destroy_rubble(struct chunk *c, struct loc grid);
void square_force_floor(struct chunk *c, struct loc grid);
void square_t_elem_remove(struct chunk *c, struct loc grid, int idx);
void square_t_elem_remove_all(struct chunk *c, struct loc grid);


int square_shopnum(struct chunk *c, struct loc grid);
int square_digging(struct chunk *c, struct loc grid);
const char *square_apparent_name(struct chunk *c, struct loc grid);
const char *square_apparent_look_prefix(struct chunk *c, struct loc grid);
const char *square_apparent_look_in_preposition(struct chunk *c, struct loc grid);

//void square_memorize(struct chunk *c, struct loc grid);
//void square_true_memorize(struct chunk *c, struct loc grid);
//void square_forget(struct chunk *c, struct loc grid);
void square_mark(struct chunk *c, struct loc grid);
void square_unmark(struct chunk *c, struct loc grid);

/* cave.c */
int motion_dir(struct loc source, struct loc target);
struct loc next_grid(struct loc grid, int dir);
int lookup_feat(const char *name);
int lookup_feat_code(const char *code);
const char *get_feat_code_name(int idx);
struct chunk *cave_new(int height, int width);
void cave_connectors_free(struct connector *join);
void cave_free(struct chunk *c);
void list_object(struct chunk *c, struct object *obj);
void delist_object(struct chunk *c, struct object *obj);
void object_lists_check_integrity(struct chunk *c, struct chunk *c_k);
void scatter(struct chunk *c, struct loc *place, struct loc grid, int d,
			 bool need_los);
int scatter_ext(struct chunk *c, struct loc *places, int n, struct loc grid,
		int d, bool need_los, bool (*pred)(struct chunk *, struct loc));

struct monster *cave_monster(struct chunk *c, int idx);
int cave_monster_max(struct chunk *c);
int cave_monster_count(struct chunk *c);

int count_feats(struct loc *grid,
				bool (*test)(struct chunk *c, struct loc grid), bool under);
int count_neighbors(struct loc *match, struct chunk *c, struct loc grid,
	bool (*test)(struct chunk *c, struct loc grid), bool under);
struct loc cave_find_decoy(struct chunk *c);
void square_average_mana(struct chunk *c, struct loc grid);
int available_mana(struct chunk *c, struct loc grid);

void cave_known(struct player *p);

#endif /* !CAVE_H */
