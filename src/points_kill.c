
/* dist: public */

#include "asss.h"


/* prototypes */

local void MyKillFunc(Arena *, int, int, int, int);

/* global data */

local Imodman *mm;
local Iplayerdata *pd;
local Iarenaman *aman;
local Iconfig *cfg;
local Istats *stats;

EXPORT int MM_points_kill(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		stats = mm->GetInterface(I_STATS, ALLARENAS);

		if (!stats) return MM_FAIL;
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(stats);
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		mm->RegCallback(CB_KILL, MyKillFunc, arena);
		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		mm->UnregCallback(CB_KILL, MyKillFunc, arena);
		return MM_OK;
	}
	return MM_FAIL;
}


void MyKillFunc(Arena *arena, int killer, int killed, int bounty, int flags)
{
	int tk, pts;

	tk = pd->players[killer].freq == pd->players[killed].freq;
	pts = bounty;

	/* cfghelp: Kill:FlagValue, arena, int, def: 100
	 * The number of extra points to give for each flag a killed player
	 * was carrying. */
	if (flags)
		pts += flags *
			cfg->GetInt(arena->cfg, "Kill", "FlagValue", 100);

	/* cfghelp: Misc:TeamKillPoints, arena, bool, def: 0
	 * Whether points are awarded for a team-kill. */
	if (tk &&
	    cfg->GetInt(arena->cfg, "Misc", "TeamKillPoints", 0))
		pts = 0;

	if (stats)
		stats->IncrementStat(killer, STAT_KILL_POINTS, pts);
}

