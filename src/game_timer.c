
/* dist: public */

#include <string.h>
#include <stdlib.h>

#include "asss.h"


#define MAXWARNMSGS 5


/* interface pointers */
local Imodman *mm;
local Iplayerdata *pd;
local Iarenaman *aman;
local Iconfig *cfg;
local Ilogman *lm;
local Icmdman *cmd;
local Ichat *chat;
local Imainloop *ml;

/* timer data */
typedef struct
{
	int gamelen;
	int enabled;
	ticks_t timeout;
	ticks_t warnmsgs[MAXWARNMSGS];
} timerdata;

int tdkey;


local int TimerMaster(void *nothing)
{
	ticks_t now = current_ticks();
	int j;
	Link *link;
	Arena *arena;
	timerdata *td;

	aman->Lock();
	FOR_EACH_ARENA_P(arena, td, tdkey)
		if (td->enabled && TICK_GT(now, td->timeout))
		{
			lm->LogA(L_DRIVEL, "game_timer", arena, "timer expired");
			DO_CBS(CB_TIMESUP, arena, GameTimerFunc, (arena));
			chat->SendArenaSoundMessage(arena, SOUND_HALLELLULA, "NOTICE: Game over");
			if (td->gamelen)
				td->timeout = TICK_MAKE(now+td->gamelen);
			else
			{
				td->enabled = 0;
				td->timeout = 0;
			}
		}
		else if (td->enabled)
		{
			now = TICK_DIFF(td->timeout, current_ticks())/100;
			for (j = 0; j < MAXWARNMSGS; j++)
				if (now && td->warnmsgs[j] == now)
				{
					if (!(td->warnmsgs[j]%60))
						chat->SendArenaMessage(arena, "NOTICE: %u minute%s remaining.", now/60, now == 60 ? "" : "s");
					else
						chat->SendArenaMessage(arena, "NOTICE: %u seconds remaining.", now);
				}
		}
	aman->Unlock();
	return TRUE;
}


local void ArenaAction(Arena *arena, int action)
{
	int i, savedlen;
	const char *warnvals, *tmp = NULL;
	char num[16];
	timerdata *td = P_ARENA_DATA(arena, tdkey);

	if (action == AA_CREATE)
		memset(td, 0, sizeof(*td));

	if (action == AA_CREATE || action == AA_CONFCHANGED)
	{
		/* FIXME: document this setting */
		warnvals = cfg->GetStr(arena->cfg, "Misc", "TimerWarnings");
		for (i = 0; i < MAXWARNMSGS; i++)
			td->warnmsgs[i] = 0;
		if (warnvals)
			for (i = 0; i < MAXWARNMSGS && strsplit(warnvals, " ,", num, sizeof(num), &tmp); i++)
				td->warnmsgs[i] = strtol(num, NULL, 0);

		savedlen = td->gamelen;
		/* cfghelp: Misc:TimedGame, arena, int, def: 0
		 * How long the game timer lasts (in ticks). Zero to disable. */
		td->gamelen = cfg->GetInt(arena->cfg, "Misc", "TimedGame", 0);
		if (action == AA_CREATE && td->gamelen)
		{
			td->enabled = 1;
			td->timeout = TICK_MAKE(current_ticks()+td->gamelen);
		}
		else if (action == AA_CONFCHANGED && !savedlen && td->gamelen)
		{
			/* switch to timedgame immediately */
			td->enabled = 1;
			td->timeout = TICK_MAKE(current_ticks()+td->gamelen);
		}
	}
	else if (action == AA_DESTROY)
	{
		td->enabled = 0;
		td->timeout = 0;
	}
}



local helptext_t time_help =
"Targets: none\n"
"Args: none\n"
"Returns amount of time left in current game.\n";

local void Ctime(const char *params, Player *p, const Target *target)
{
	Arena *arena = p->arena;
	int mins, secs;
	int tout;
	timerdata *td = P_ARENA_DATA(arena, tdkey);

	if (td->enabled)
	{
		tout = TICK_DIFF(td->timeout, current_ticks());
		mins = tout/60/100;
		secs = (tout/100)%60;
		chat->SendMessage(p, "Time left: %d minutes %d seconds", mins, secs);
	}
	else if (td->timeout)
	{
		 mins = td->timeout/60/100;
		 secs = (td->timeout/100)%60;
		 chat->SendMessage(p, "Timer paused at:  %d minutes %d seconds", mins, secs);
	}
	else
		chat->SendMessage(p, "Time left: 0 minutes 0 seconds");
}


local helptext_t timer_help =
"Targets: none\n"
"Args: <minutes>[:<seconds>]\n"
"Set arena timer to minutes:seconds, only in arenas with TimedGame setting\n"
"off. Note, that the seconds part is optional, but minutes must always\n"
"be defined (even if zero). If successful, server replies with ?time response.\n";

local void Ctimer(const char *params, Player *p, const Target *target)
{
	Arena *arena = p->arena;
	int mins = 0, secs = 0;
	timerdata *td = P_ARENA_DATA(arena, tdkey);

	if (td->gamelen == 0)
	{
		char *end;
		mins = strtol(params, &end, 10);
		if (end != params)
		{
			if ((end = strchr(end, ':')))
				secs = strtol(end+1, NULL, 10);
			td->enabled = 1;
			td->timeout = TICK_MAKE(current_ticks()+(60*100*mins)+(100*secs));
			Ctime(params, p, target);
		}
		else chat->SendMessage(p, "timer format is: '?timer mins[:secs]'");
	}
	else chat->SendMessage(p, "Timer is fixed to Misc:TimedGame setting.");
}


local helptext_t timereset_help =
"Targets: none\n"
"Args: none\n"
"Reset a timed game, but only in arenas with Misc:TimedGame in use.\n";

local void Ctimereset(const char *params, Player *p, const Target *target)
{
	Arena *arena = p->arena;
	timerdata *td = P_ARENA_DATA(arena, tdkey);
	ticks_t gamelen = td->gamelen;

	if (gamelen)
	{
		td->timeout = TICK_MAKE(current_ticks() + gamelen);
		Ctime(params, p, target);
	}
}


local helptext_t pausetimer_help =
"Targets: none\n"
"Args: none\n"
"Pauses the timer. The timer must have been created with ?timer.\n";

local void Cpausetimer(const char *params, Player *p, const Target *target)
{
	Arena *arena = p->arena;
	timerdata *td = P_ARENA_DATA(arena, tdkey);

	if (td->gamelen) return;

	if (td->enabled)
	{
		td->enabled = 0;
		td->timeout -= current_ticks();
		chat->SendMessage(p,"Timer paused at:  %d minutes %d seconds",
							td->timeout/60/100, (td->timeout/100)%60);
	}
	else if (td->timeout)
	{
		chat->SendMessage(p,"Timer resumed at: %d minutes %d seconds",
							td->timeout/60/100, (td->timeout/100)%60);
		td->enabled = 1;
		td->timeout += current_ticks();
	}
}



EXPORT int MM_game_timer(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		lm = mm->GetInterface(I_LOGMAN, ALLARENAS);
		cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);

		tdkey = aman->AllocateArenaData(sizeof(timerdata));
		if (tdkey == -1) return MM_FAIL;

		mm->RegCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);

		ml->SetTimer(TimerMaster, 100, 100, NULL, NULL);

		cmd->AddCommand("timer", Ctimer, timer_help);
		cmd->AddCommand("time", Ctime, time_help);
		cmd->AddCommand("timereset", Ctimereset, timereset_help);
		cmd->AddCommand("pausetimer", Cpausetimer, pausetimer_help);

		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		cmd->RemoveCommand("timer", Ctimer);
		cmd->RemoveCommand("time", Ctime);
		cmd->RemoveCommand("timereset", Ctimereset);
		cmd->RemoveCommand("pausetimer", Cpausetimer);
		mm->UnregCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);
		ml->ClearTimer(TimerMaster, NULL);
		aman->FreeArenaData(tdkey);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(lm);
		mm->ReleaseInterface(cmd);
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(ml);
		return MM_OK;
	}
	return MM_FAIL;
}

