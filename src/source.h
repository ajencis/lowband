#ifndef EFFECT_SOURCE_H
#define EFFECT_SOURCE_H

#include "z-type.h"

/*
 * Structure that tells you where an effect came from
 */
struct source {
	enum {
		SRC_NONE,
		SRC_TRAP,
		SRC_PLAYER,
		SRC_MONSTER,
		SRC_OBJECT,
		SRC_CHEST_TRAP,
		SRC_TERRAIN_ELEM,
		SRC_GRID
	} what;

	union {
		struct trap *trap;
		int monster;
		struct object *object;
		struct chest_trap *chest_trap;
		struct terrain_element *t_elem;
		struct loc grid;
	} which;
};

/*
 * Generate different forms of the source for projection and effect
 * functions
 */
struct source source_none(void);
struct source source_trap(struct trap *);
struct source source_monster(int who);
struct source source_player(void);
struct source source_object(struct object *);
struct source source_chest_trap(struct chest_trap *chest_trap);
struct source source_t_elem(struct terrain_element *t_elem);
struct source source_grid(struct loc grid);

#endif /* EFFECT_SOURCE_H */
