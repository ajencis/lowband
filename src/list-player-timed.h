/**
 * \file list-player-timed.h
 * \brief timed player properties
 *
 * Fields:
 * symbol - the effect name
 * flag_redraw - the things to be redrawn when the effect is set
 * flag_update - the things to be updated when the effect is set
 */

/* symbol		flag_redraw							flag_update 
    save	stack	resist_flag		time	message_begin			message_end				message_increase*/
TMD(FAST,		PR_STATUS,							PU_BONUS,
    false,	INCR,	0,				50,		MON_MSG_HASTED,			MON_MSG_NOT_HASTED,		MON_MSG_MORE_HASTED)
TMD(SLOW,		PR_STATUS,							PU_BONUS,
    false,	INCR,	RF_NO_SLOW,		50,		MON_MSG_SLOWED,			MON_MSG_NOT_SLOWED,		MON_MSG_MORE_SLOWED)
TMD(BLIND,		PR_MAP,								PU_BONUS | PU_UPDATE_VIEW | PU_MONSTERS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(PARALYZED,	PR_STATUS,							PU_BONUS,
    false,	MAX,	RF_NO_HOLD,		50,		MON_MSG_HELD,			MON_MSG_NOT_HELD,		0)
TMD(ASLEEP,		PR_STATUS,							PU_BONUS,
    true,	NO,		RF_NO_SLEEP,	10000,	MON_MSG_FALL_ASLEEP,	MON_MSG_WAKES_UP,		0)
TMD(CONFUSED,	PR_STATUS,							PU_BONUS,
    false,	MAX,	RF_NO_CONF,		50,		MON_MSG_CONFUSED,		MON_MSG_NOT_CONFUSED,	MON_MSG_MORE_CONFUSED)
TMD(AFRAID,		PR_STATUS,							PU_BONUS,
    true,	INCR,	RF_NO_FEAR,		10000,	MON_MSG_FLEE_IN_TERROR,	MON_MSG_NOT_AFRAID,		MON_MSG_MORE_AFRAID)
TMD(IMAGE,		PR_MAP | PR_MONLIST | PR_ITEMLIST,	PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(POISONED,	PR_STATUS,							PU_BONUS,
    true,	INCR,	RF_IM_POIS,		250,	MON_MSG_POISON,			0,						0)
TMD(TOXIC,		PR_STATUS,							PU_BONUS,
    true,	MAX,	RF_IM_POIS,		250,	MON_MSG_POISON,			0,						0)
TMD(CUT,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(STUN,		PR_STATUS,							PU_BONUS,
    true,	MAX,	RF_NO_STUN,		50,		MON_MSG_DAZED,			MON_MSG_NOT_DAZED,		MON_MSG_MORE_DAZED)
TMD(DISEN,		PR_STATUS,							PU_BONUS,
    false,	MAX,	RF_IM_DISEN,	50,		MON_MSG_DISEN,			MON_MSG_NOT_DISEN,		0)
TMD(CHARMED,	PR_STATUS,							PU_BONUS,
    true,	MAX,	0,				50,		0,						0,						0)
TMD(FOOD,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(PROTEVIL,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(INVULN,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(HERO,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(SHERO,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(SHIELD,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(BLESSED,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(SINVIS,		PR_STATUS,							PU_BONUS | PU_MONSTERS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(SINFRA,		PR_STATUS,							PU_BONUS | PU_MONSTERS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(OPP_ACID,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(OPP_ELEC,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(OPP_FIRE,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(OPP_COLD,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(OPP_POIS,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(OPP_CONF,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(AMNESIA,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(TELEPATHY,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(STONESKIN,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(TERROR,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(SPRINT,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(BOLD,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(SCRAMBLE,   PR_STATUS,		   					PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(TRAPSAFE,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(FASTCAST,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_ACID,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_ELEC,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_FIRE,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_COLD,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_POIS,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_CONF,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_EVIL,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_DEMON,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_VAMP,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(HEAL,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(COMMAND,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(ATT_RUN,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(COVERTRACKS,PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(POWERSHOT,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(TAUNT,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(BLOODLUST,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(BLACKBREATH,PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(STEALTH,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(FREE_ACT,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(REGEN,		PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(RAD_POIS,	PR_STATUS,							PU_BONUS,
    true,	NO,		0,				250,	0,						0,						0)
TMD(CALL_STORM,	PR_STATUS,							0,
    true,	NO,		0,				250,	0,						0,						0)
TMD(PHOENIX,	PR_STATUS,							0,
    true,	NO,		0,				250,	0,						0,						0)
TMD(PHOENIX_CD,	PR_STATUS,							0,
    true,	NO,		0,				250,	0,						0,						0)
TMD(INVIS,		PR_STATUS,							0,
    true,	NO,		0,				250,	0,						0,						0)
TMD(CHANGED,	PR_STATUS,							PU_BONUS,
    false,	MAX,	0,				50,		0,						0,						0)
TMD(SUFFOCATE,	PR_STATUS,							PU_BONUS,
    true,	MAX,	RF_NONLIVING,	50,		MON_MSG_SUFFOCATE,		MON_MSG_NOT_SUFFOCATE,	0)
TMD(SUMMONED,	PR_STATUS,							PU_BONUS,
    false,	NO,		0,				50,		0,						0,						0)
TMD(POLYMORPHED,PR_STATUS,                          PU_BONUS,
    true,   MAX,    0,              250,    0,                      0,                      0)
