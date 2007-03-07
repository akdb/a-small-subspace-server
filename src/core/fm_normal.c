
/* dist: public */

#include <string.h>
#include <limits.h>

#include "asss.h"


/* cfghelp: Team:InitialSpec, arena, bool, def: 0
 * If players entering the arena are always assigned to spectator mode. */
#define INITIALSPEC(ch) cfg->GetInt(ch, "Team", "InitialSpec", 0)

/* cfghelp: Team:IncludeSpectators, arena, bool, def: 0
 * Whether to include spectators when enforcing maximum freq sizes. */
#define INCLSPEC(ch) cfg->GetInt(ch, "Team", "IncludeSpectators", 0)

/* cfghelp: General:MaxPlaying, arena, int, def: 100
 * This is the most players that will be allowed to play in the arena at
 * once. Zero means no limit. */
#define MAXPLAYING(ch) cfg->GetInt(ch, "General", "MaxPlaying", 100)

/* cfghelp: Misc:MaxXres, arena, int, def: 0
 * Maximum screen width allowed in the arena. Zero means no limit. */
#define MAXXRES(ch) cfg->GetInt(ch, "Misc", "MaxXres", 0)

/* cfghelp: Misc:MaxYres, arena, int, def: 0
 * Maximum screen height allowed in the arena. Zero means no limit. */
#define MAXYRES(ch) cfg->GetInt(ch, "Misc", "MaxYres", 0)

/* cfghelp: Team:MaxFrequency, arena, int, range: 1-10000, def: 10000
 * One more than the highest frequency allowed. Set this below
 * PrivFreqStart to disallow private freqs. */
#define MAXFREQ(ch) cfg->GetInt(ch, "Team", "MaxFrequency", 10000)


local Iplayerdata *pd;
local Iconfig *cfg;
local Ichat *chat;
local Icapman *capman;
local Imodman *mm;


local int count_current_playing(Arena *arena)
{
	Player *p;
	Link *link;
	int playing = 0;
	pd->Lock();
	FOR_EACH_PLAYER(p)
		if (p->status == S_PLAYING &&
		    p->arena == arena &&
		    p->p_ship != SHIP_SPEC)
			playing++;
	pd->Unlock();
	return playing;
}


local int count_freq(Arena *arena, int freq, Player *excl, int inclspec)
{
	Player *p;
	Link *link;
	int t = 0;
	pd->Lock();
	FOR_EACH_PLAYER(p)
		if (p->arena == arena &&
		    p->p_freq == freq &&
		    p != excl &&
		    ( p->p_ship != SHIP_SPEC || inclspec ) )
			t++;
	pd->Unlock();
	return t;
}

local int get_max_for_freq(ConfigHandle ch, int f)
{
	/* cfghelp: Team:PrivFreqStart, arena, int, range: 0-9999, def: 100
	 * Freqs above this value are considered private freqs. */
	if (f > 0 && f >= cfg->GetInt(ch, "Team", "PrivFreqStart", 100))
		/* cfghelp: Team:MaxPerPrivateTeam, arena, int, def: 0
		 * The maximum number of players on a private freq. Zero means
		 * no limit. */
		return cfg->GetInt(ch, "Team", "MaxPerPrivateTeam", 0);
	else
		/* cfghelp: Team:MaxPerTeam, arena, int, def: 0
		 * The maximum number of players on a public freq. Zero means no
		 * limit. */
		return cfg->GetInt(ch, "Team", "MaxPerTeam", 0);
}


local int FindLegalShip(Arena *arena, int freq, int ship)
{
	/* cfghelp: Team:FrequencyShipTypes, arena, bool, def: 0
	 * If this is set, freq 0 will only be allowed to use warbirds, freq
	 * 1 can only use javelins, etc. */
	int clockwork = cfg->GetInt(arena->cfg,
			"Team", "FrequencyShipTypes", 0);

	if (clockwork)
	{
		/* we don't want to switch the ships of speccers, even in FST */
		if (ship == SHIP_SPEC || freq < 0 || freq > SHIP_SHARK)
			return SHIP_SPEC;
		else
			return freq;
	}
	else
	{
		/* no other options for now */
		return ship;
	}
}


local int BalanceFreqs(Arena *arena, Player *excl, int inclspec)
{
	Player *i;
	Link *link;
	int counts[CFG_MAX_DESIRED] = { 0 }, min = INT_MAX, j;
	int best[CFG_MAX_DESIRED] = { 0 }, num = -1;

	int max = get_max_for_freq(arena->cfg, 0);
	/* cfghelp: Team:DesiredTeams, arena, int, def: 2
	 * The number of teams that the freq balancer will form as players
	 * enter. */
	int desired = cfg->GetInt(arena->cfg,
			"Team", "DesiredTeams", 2);

	if (desired < 1) desired = 1;
	if (desired > CFG_MAX_DESIRED) desired = CFG_MAX_DESIRED;

	/* get counts */
	pd->Lock();
	FOR_EACH_PLAYER(i)
		if (i->arena == arena &&
		    i->p_freq < desired &&
		    i != excl &&
		    ( i->p_ship != SHIP_SPEC || inclspec ) )
			counts[i->p_freq]++;
	pd->Unlock();

	for (j = 0; j < desired; j++)
		if (counts[j] < min)
		{
			min = counts[j];
			num = 1;
			best[0] = j;
		}
		else if (counts[j] == min)
		{
			best[num++] = j;
		}

	if (num <= 0) /* shouldn't happen */
		return 0;
	else if (max == 0 || min < max) /* we found a spot */
	{
		int rnd;
		Iprng *r = mm->GetInterface(I_PRNG, ALLARENAS);

		if (r)
		{
			rnd = r->Rand();
			mm->ReleaseInterface(r);
		}
		else
			rnd = current_ticks();

		num = rnd % num;

		return best[num];
	}
	else /* no spots within desired freqs */
	{
		/* try incrementing freqs until we find one with < max players */
		j = desired;
		while (count_freq(arena, j, excl, inclspec) >= max)
			j++;
		return j;
	}
}

local int screen_res_allowed(Player *p, ConfigHandle ch)
{
	int max_x = MAXXRES(ch);
	int max_y = MAXYRES(ch);
	if((max_x == 0 || p->xres <= max_x) && (max_y == 0 || p->yres <= max_y))
		return 1;

	if (chat)
		chat->SendMessage(p,
			"Maximum allowed screen resolution is %dx%d in this arena",
			max_x, max_y);

	return 0;
}

local void Initial(Player *p, int *ship, int *freq)
{
	Arena *arena = p->arena;
	int f, s = *ship;
	int maxplaying;
	ConfigHandle ch;

	if (!arena) return;

	ch = arena->cfg;

	if (((maxplaying = MAXPLAYING(ch)) > 0 &&
	     count_current_playing(arena) >= maxplaying) ||
	    p->flags.no_ship ||
	    !screen_res_allowed(p, ch) ||
	    INITIALSPEC(ch))
		s = SHIP_SPEC;

	if (s == SHIP_SPEC)
	{
		f = arena->specfreq;
	}
	else
	{
		/* we have to assign him to a freq */
		f = BalanceFreqs(arena, p, INCLSPEC(ch));
		if (f < 0 || f >= MAXFREQ(ch))
		{
			f = arena->specfreq;
			s = SHIP_SPEC;
		}
		/* and make sure the ship is still legal */
		s = FindLegalShip(arena, f, s);
	}

	*ship = s; *freq = f;
}

/* FIXME we use this in Ship now. Rearrange or fwd declare everything */
local void Freq(Player *p, int *ship, int *freq);

local void Ship(Player *p, int *ship, int *freq)
{
	Arena *arena = p->arena;
	int f = *freq, s = *ship;
	int maxplaying;
	ConfigHandle ch;

	if (!arena) return;

	ch = arena->cfg;

	/* always allow switching to spec */
	if (s >= SHIP_SPEC)
	{
		f = arena->specfreq;
	}
	/* otherwise, he's changing to a ship */
	/* check lag */
	else if (p->flags.no_ship)
	{
		if (chat)
			chat->SendMessage(p,
					"You have too much lag to play in this arena.");
		goto deny;
	}
	/* allowed res; this prints out its own error message */
	else if (!screen_res_allowed(p, ch))
	{
		goto deny;
	}
	/* check if changing from spec and too many playing */
	else if (p->p_ship == SHIP_SPEC &&
	         (maxplaying = MAXPLAYING(ch)) > 0 &&
	         count_current_playing(arena) >= maxplaying)
	{
		if (chat)
			chat->SendMessage(p,
					"There are too many people playing in this arena.");
		goto deny;
	}
	/* ok, allowed change */
	/* when ships == freq, support ship change -> freq change custom */
	else if (cfg->GetInt(arena->cfg, "Team", "FrequencyShipTypes", 0))
	{
		f = s;
		Freq(p, &s, &f);
	}
	else
	{
		/* check if he's changing from speccing on the spec freq, or on a
		 * regular freq that's full */
		if (p->p_ship == SHIP_SPEC)
		{
			int need_balance = FALSE;
			if (f == arena->specfreq)
				/* leaving spec mode on spec freq, always reassign */
				need_balance = TRUE;
			else
			{
				/* unspeccing from a non-spec freq. only reassign if full.
				 * note: we can always do this count assuming IncludeSpectators
				 * is false. the reasoning is: we know that p is currently on f.
				 * if IncludeSpectators is true, then there are <= max people on
				 * f in total, so count_freq(a, f, p, TRUE) must be < max, so
				 * the condition will never be true. only if IncludeSpectators
				 * is false does count >= max have a chance of being true. */
				int max = get_max_for_freq(ch, f);
				if (max > 0 && count_freq(arena, f, p, FALSE) >= max)
					need_balance = TRUE;
			}

			if (need_balance)
			{
				f = BalanceFreqs(arena, p, INCLSPEC(ch));
				if (f < 0 || f >= MAXFREQ(ch))
				{
					f = arena->specfreq;
					s = SHIP_SPEC;
				}
			}
		}
		/* and make sure the ship is still legal */
		s = FindLegalShip(arena, f, s);
	}

	*ship = s; *freq = f;
	return;

deny:
	*ship = p->p_ship;
	*freq = p->p_freq;
}


local void Freq(Player *p, int *ship, int *freq)
{
	Arena *arena = p->arena;
	int f = *freq, s = *ship;
	int inclspec, maxfreq, maxplaying;
	ConfigHandle ch;

	if (!arena) return;

	ch = arena->cfg;
	inclspec = INCLSPEC(ch);
	maxfreq = MAXFREQ(ch);

	/* special case: speccer re-entering spec freq */
	if (s == SHIP_SPEC && f == arena->specfreq)
		return;

	if (f < 0 || f >= maxfreq)
	{
		/* he requested a bad freq. drop him elsewhere. */
		f = BalanceFreqs(arena, p, inclspec);
		if (f < 0 || f >= maxfreq)
		{
			f = arena->specfreq;
			s = SHIP_SPEC;
		}
	}
	else
	{
		/* check to make sure the new freq is ok */
		int count = count_freq(arena, f, p, inclspec);
		int max = get_max_for_freq(ch, f);
		if (max > 0 && count >= max)
		{
			/* the freq has too many people, assign him to another */
			f = BalanceFreqs(arena, p, inclspec);
			if (f < 0 || f >= maxfreq)
			{
				f = arena->specfreq;
				s = SHIP_SPEC;
			}
		}
		/* cfghelp: Team:ForceEvenTeams, arena, boolean, def: 0
		 * Whether players can switch to more populous teams. */
		else if (cfg->GetInt(ch, "Team", "ForceEvenTeams", 0))
		{
			int old = count_freq(arena, p->p_freq, p, inclspec);

			/* we pick their freq if they are coming from spec */
			if (p->p_ship == SHIP_SPEC && p->p_freq == arena->specfreq)
			{
				f = BalanceFreqs(arena, p, inclspec);
				if (f < 0 || f >= maxfreq)
				{
					f = arena->specfreq;
					s = SHIP_SPEC;
				}
			}
			/* ForceEvenTeams */
			else if (old < count)
			{
				if (chat)
					chat->SendMessage(p,
						"Changing frequencies would make the teams uneven.");
				*freq = p->p_freq;
				*ship = p->p_ship;
				return;
			}
		}
	}

	/* make sure he has an appropriate ship for this freq */
	s = FindLegalShip(arena, f, s);

	/* check if this change brought him out of spec and there are too
	 * many people playing. */
	if (s != SHIP_SPEC &&
	    p->p_ship == SHIP_SPEC &&
	    (maxplaying = MAXPLAYING(ch)) > 0 &&
	    count_current_playing(arena) >= maxplaying)
	{
		s = p->p_ship;
		f = p->p_freq;
		if (chat)
			chat->SendMessage(p,
					"There are too many people playing in this arena.");
	}

	*ship = s; *freq = f;
}


local Ifreqman fm_int =
{
	INTERFACE_HEAD_INIT(I_FREQMAN, "fm-normal")
	Initial, Ship, Freq
};

EXPORT int MM_fm_normal(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		capman = mm->GetInterface(I_CAPMAN, ALLARENAS);
		if (!pd || !cfg)
			return MM_FAIL;
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		if (fm_int.head.refcount)
			return MM_FAIL;
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(capman);
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		mm->RegInterface(&fm_int, arena);
		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		mm->UnregInterface(&fm_int, arena);
		return MM_OK;
	}
	return MM_FAIL;
}

