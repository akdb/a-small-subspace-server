
#include "asss.h"

/* global data */

local Imodman *mm;
local Iplayerdata *pd;
local Iarenaman *aman;
local Imainloop *ml;
local Iflags *flags;
local Iconfig *cfg;
local Inet *net;
local Istats *stats;

typedef struct periodic_settings
{
	int arena;
	int delay;
	int minplayers;
	Iperiodicpoints *pp;
	/* only for duration of timer: */
	int totalplayers;
	byte *pkt;
} periodic_settings;

local periodic_settings *settings[MAXARENA];


typedef struct freq_data
{
	TreapHead head; /* key is freq */
	int players;
	int flags;
	int points;
} freq_data;


local void reward_enum(TreapHead *node, void *clos)
{
	freq_data *fd = (freq_data*)node;
	periodic_settings *set = clos;
	int points, freq = node->key;

	/* enter in packet */
	points = set->pp->GetPoints(
			set->arena,
			freq,
			fd->players,
			set->totalplayers,
			fd->flags);

	/* enter in packet */
	*(set->pkt++) = (freq>>0) & 0xff;
	*(set->pkt++) = (freq>>8) & 0xff;
	*(set->pkt++) = (points>>0) & 0xff;
	*(set->pkt++) = (points>>8) & 0xff;

	/* set in fd */
	fd->points = points;
}


local int timer(void *set_)
{
	periodic_settings *set = set_;
	int pid, totalplayers = 0, freqcount = 0;
	TreapHead *fdata = NULL;
	freq_data *fd;

	if (set->delay == 0)
	{
		/* this is a signal that we should remove ourself */
		mm->ReleaseInterface(set->pp);
		afree(set);
		return FALSE;
	}

	/* lock status here to avoid repeatedly locking and unlocking it,
	 * and also to avoid deadlock. */
	flags->LockFlagStatus(set->arena);

	/* figure out what freqs we have in this arena, how many players
	 * each has, and how many flags each owns. */
	pd->LockStatus();
	for (pid = 0; pid < MAXPLAYERS; pid++)
		if (pd->players[pid].status == S_PLAYING &&
		    pd->players[pid].arena == set->arena)
		{
			int freq = pd->players[pid].freq;
			fd = (freq_data*)TrGet(fdata, freq);
			if (fd == NULL)
			{
				fd = amalloc(sizeof(*fd));
				fd->head.key = freq;
				fd->players = 0;
				fd->flags = flags->GetFreqFlags(set->arena, freq);
				TrPut(&fdata, &fd->head);
				freqcount++;
			}
			fd->players++;
			totalplayers++;
		}
	pd->UnlockStatus();

	flags->UnlockFlagStatus(set->arena);

	if (totalplayers >= set->minplayers && set->pp)
	{
		/* get packet */
		byte *pkt = amalloc(freqcount*4+1);

		pkt[0] = S2C_PERIODICREWARD;
		set->pkt = pkt + 1;

		/* now calculate points for each freq, filling in packet as we go */
		TrEnum(fdata, reward_enum, set);

		net->SendToArena(set->arena, -1, pkt, freqcount*4+1, NET_RELIABLE);
		afree(pkt);
		set->pkt = NULL; /* just to be safe */

		/* now reward points to all */
		pd->LockStatus();
		for (pid = 0; pid < MAXPLAYERS; pid++)
			if (pd->players[pid].status == S_PLAYING &&
				pd->players[pid].arena == set->arena)
				if ((fd = (freq_data*)TrGet(fdata, pd->players[pid].freq)))
					stats->IncrementStat(pid, STAT_FLAG_POINTS, fd->points);
		pd->UnlockStatus();

		/* i think the client is smart enough that we don't need to do
		 * this:
		stats->SendUpdates();
		*/
	}

	/* free the treap we've been working with */
	TrEnum(fdata, tr_enum_afree, NULL);

	return TRUE;
}


local void aaction(int arena, int action)
{
	int delay;

	if (action == AA_CREATE || action == AA_CONFCHANGED)
		delay = cfg->GetInt(aman->arenas[arena].cfg, "Periodic", "RewardDelay", 0);
	else if (action == AA_DESTROY)
		delay = 0;
	else
		return;

	/* if the setting is different... */
	if (settings[arena] && settings[arena]->delay != delay)
	{
		/* first destroy old one */
		settings[arena]->delay = 0;
		settings[arena] = NULL;
	}

	/* if we need a new one... */
	if (delay && settings[arena] == NULL)
	{
		Iperiodicpoints *pp = mm->GetInterface(I_PERIODIC_POINTS, arena);
		if (pp)
		{
			periodic_settings *set = amalloc(sizeof(*set));
			set->arena = arena;
			set->delay = delay;
			set->minplayers = cfg->GetInt(aman->arenas[arena].cfg, "Periodic", "RewardMinimumPlayers", 0);
			set->pp = pp;
			ml->SetTimer(timer, delay, delay, set);
		}
	}
}


EXPORT int MM_periodic(int action, Imodman *mm_, int arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);
		flags = mm->GetInterface(I_FLAGS, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		net = mm->GetInterface(I_NET, ALLARENAS);
		stats = mm->GetInterface(I_STATS, ALLARENAS);
		if (!pd || !aman || !flags || !cfg || !stats) return MM_FAIL;

		mm->RegCallback(CB_ARENAACTION, aaction, ALLARENAS);

		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		mm->UnregCallback(CB_ARENAACTION, aaction, ALLARENAS);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(ml);
		mm->ReleaseInterface(flags);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(net);
		mm->ReleaseInterface(stats);
		return MM_OK;
	}
	return MM_FAIL;
}
