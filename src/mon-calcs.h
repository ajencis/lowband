

#ifndef INCLUDED_MON_CALCS_H
#define INCLUDED_MON_CALCS_H

struct scaling_data {
    int base;       // basic amount;
    int r_xtra;     // amount modified by monster race
    int p_xtra;     // amount modified by player level
};

int mon_lev(const struct monster *mon);

struct scaling_data scaling_data_sum(struct scaling_data sdata1, struct scaling_data sdata2);

int scaling_data_calc_r_xtra(const struct monster_race *mr, struct scaling_data sdata);
int scaling_data_calc_p_xtra(const struct player *p, struct scaling_data sdata);
int scaling_data_calc_mon(const struct monster *mon, struct scaling_data sdata);

struct scaling_data race_skill(const struct monster_race *mr, int which);
struct scaling_data mon_race_skill(const struct monster *mon, int which);
struct scaling_data mon_tome_skill(const struct monster *mon, int which);

struct scaling_data race_power(const struct monster_race *mr, int power);
struct scaling_data mon_race_power(const struct monster *mon, int power);
struct scaling_data mon_tome_power(const struct monster *mon, int power);

int stat_skill_bonus(const struct monster *mon, const struct player_state *state, int which, int curr, char *buf, size_t bufsize);

struct scaling_data race_power(const struct monster_race *mr, int power);

struct scaling_data mon_race_power(const struct monster *mon, int power);
int mon_class_power(const struct monster *mon, int power);

void calc_mon_bonuses(struct monster *mon, struct player_state *state);
void update_mon_state(struct monster *mon);
void update_mon_attacks(struct monster *mon);
void free_mon_attacks(struct monster *mon);

#endif
