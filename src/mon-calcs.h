

#ifndef INCLUDED_MON_CALCS_H
#define INCLUDED_MON_CALCS_H

int calc_mon_race_power(const struct monster_race *mr, int power);
int mon_lev(const struct monster *mon);

void race_skill(const struct monster_race *mr, int which, int *base, int *xtra);
void mon_race_skill(const struct monster *mon, int which, int *base, int *xtra);
void class_skill(const struct monster *mon, int which, int *base, int *xtra);
void tome_skill(const struct monster *mon, int which, int *base, int *xtra);
int stat_skill_bonus(const struct monster *mon, const struct player_state *state, int which, int curr, char *buf, size_t bufsize);

int mon_race_power(const struct monster *mon, int power);
int mon_class_power(const struct monster *mon, int power);
int mon_tome_power(const struct monster *mon, int power);

void calc_mon_bonuses(struct monster *mon, struct player_state *state);
void update_mon_state(struct monster *mon);
void update_mon_attacks(struct monster *mon);
void free_mon_attacks(struct monster *mon);

#endif
