
#include "asss.h"

#include "settings/soccer.h"


#define MAXFREQ 8


struct ArenaScores
{
	int score[MAXFREQ];
};

/* prototypes */

local void MyGoal(int, int, int, int, int);
local void MyAA(int, int);

/* global data */

local struct ArenaScores scores[MAXARENA];

local Imodman *mm;
local Iplayerdata *pd;
local Iballs *balls;
local Iarenaman *aman;
local Iconfig *cfg;
local Ichat *chat;


EXPORT int MM_points_goal(int action, Imodman *mm_, int arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		balls = mm->GetInterface(I_BALLS, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(balls);
		mm->ReleaseInterface(pd);
		return MM_OK;
	}
	else if (action == MM_ATTACH)
	{
		mm->RegCallback(CB_ARENAACTION, MyAA, arena);
		mm->RegCallback(CB_GOAL, MyGoal, arena);
		return MM_OK;
	}
	else if (action == MM_DETACH)
	{
		mm->UnregCallback(CB_GOAL, MyGoal, arena);
		mm->UnregCallback(CB_ARENAACTION, MyAA, arena);
		return MM_OK;
	}
	return MM_FAIL;
}


void MyAA(int arena, int action)
{
	if (action == AA_CREATE)
	{
		int i;
		for (i = 0; i < 2; i++)
			scores[arena].score[i] = 0;
	}
}


void MyGoal(int arena, int pid, int bid, int x, int y)
{
	int mode, freq = -1, i;
	int teamset[MAXPLAYERS], nmeset[MAXPLAYERS];
	int teamc = 0, nmec = 0;

	mode = cfg->GetInt(aman->arenas[arena].cfg, "Soccer", "Mode", 0);

	switch (mode)
	{
		case GOAL_ALL:
			freq = balls->balldata[arena].balls[bid].freq;
			break;

		case GOAL_LEFTRIGHT:
			freq = x < 512 ? 1 : 0;
			break;

		case GOAL_TOPBOTTOM:
			freq = y < 512 ? 1 : 0;
			break;

		case GOAL_QUADRENTS_3_1:
		case GOAL_QUADRENTS_1_3:
		case GOAL_WEDGES_3_1:
		case GOAL_WEDGES_1_3:
			/* not implemented */
			break;
	}

	if (freq >= 0 && freq < MAXFREQ)
		scores[arena].score[freq]++;

	pd->LockStatus();
	for (i = 0; i < MAXPLAYERS; i++)
		if (pd->players[i].status == S_PLAYING &&
		    pd->players[i].arena == arena)
		{
			if (pd->players[i].freq == freq)
				teamset[teamc++] = i;
			else
				nmeset[nmec++] = i;
		}
	pd->UnlockStatus();

	teamset[teamc] = nmeset[nmec] = -1;
	chat->SendSetSoundMessage(teamset, SOUND_GOAL, "Team Goal! by %s  Reward:1", pd->players[pid].name);
	chat->SendSetSoundMessage(nmeset, SOUND_GOAL, "Enemy Goal! by %s  Reward:1", pd->players[pid].name);
	chat->SendArenaMessage(arena,"Score: Freq zero:%u, Freq one:%u, Freq two:%u, Freq three:%u",scores[arena].score[0],scores[arena].score[1],scores[arena].score[2],scores[arena].score[3]);
}


