
/* dist: public */

#include "asss.h"


/* commands */
local void Cgiveowner(const char *, int, const Target *);
local void Cfreqkick(const char *, int, const Target *);
local helptext_t giveowner_help, freqkick_help;

/* callbacks */
local void MyPA(int pid, int action, int arena);
local void MyFreqCh(int pid, int newfreq);
local void MyShipCh(int pid, int newship, int newfreq);

/* data */
local char ownsfreq[MAXPLAYERS];

local Iplayerdata *pd;
local Iarenaman *aman;
local Igame *game;
local Icmdman *cmd;
local Iconfig *cfg;
local Ichat *chat;
local Imodman *mm;


EXPORT int MM_freqowners(int action, Imodman *_mm, int arena)
{
	if (action == MM_LOAD)
	{
		mm = _mm;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);
		game = mm->GetInterface(I_GAME, ALLARENAS);
		cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);

		mm->RegCallback(CB_PLAYERACTION, MyPA, ALLARENAS);
		mm->RegCallback(CB_FREQCHANGE, MyFreqCh, ALLARENAS);
		mm->RegCallback(CB_SHIPCHANGE, MyShipCh, ALLARENAS);

		cmd->AddCommand("giveowner", Cgiveowner, giveowner_help);
		cmd->AddCommand("freqkick", Cfreqkick, freqkick_help);

		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		cmd->RemoveCommand("giveowner", Cgiveowner);
		cmd->RemoveCommand("freqkick", Cfreqkick);
		mm->UnregCallback(CB_PLAYERACTION, MyPA, ALLARENAS);
		mm->UnregCallback(CB_FREQCHANGE, MyFreqCh, ALLARENAS);
		mm->UnregCallback(CB_SHIPCHANGE, MyShipCh, ALLARENAS);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(aman);
		mm->ReleaseInterface(game);
		mm->ReleaseInterface(cmd);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(chat);
		return MM_OK;
	}
	return MM_FAIL;
}


local int CountFreq(int arena, int freq, int excl)
{
	int t = 0, i;
	pd->LockStatus();
	for (i = 0; i < MAXPLAYERS; i++)
		if (pd->players[i].arena == arena &&
		    pd->players[i].freq == freq &&
		    i != excl)
			t++;
	pd->UnlockStatus();
	return t;
}


local helptext_t giveowner_help =
"Module: freqownsers\n"
"Targets: player\n"
"Args: none\n"
"Allows you to share freq ownership with another player on your current\n"
"private freq. You can't remove ownership once you give it out, but you\n"
"are safe from being kicked off yourself, as long as you have ownership.\n";

void Cgiveowner(const char *params, int pid, const Target *target)
{
	if (target->type != T_PID)
		return;
	
	if (ownsfreq[pid] &&
	    pd->players[pid].arena == pd->players[target->u.pid].arena &&
	    pd->players[pid].freq == pd->players[target->u.pid].freq)
		ownsfreq[target->u.pid] = 1;
}


local helptext_t freqkick_help =
"Module: freqowners\n"
"Targets: player\n"
"Args: none\n"
"Kicks the player off of your freq. The player must be on your freq and\n"
"must not be an owner himself. The player giving the command, of course,\n"
"must be an owner.\n";

void Cfreqkick(const char *params, int pid, const Target *target)
{
	int t = target->u.pid;

	if (target->type != T_PID)
		return;

	if (ownsfreq[pid] && !ownsfreq[t] &&
	    pd->players[pid].arena == pd->players[t].arena &&
	    pd->players[pid].freq == pd->players[t].freq)
	{
		game->SetShip(t, SPEC);
		chat->SendMessage(t, "You have been kicked off the freq by %s",
				pd->players[pid].name);
	}
}


void MyPA(int pid, int action, int arena)
{
	ownsfreq[pid] = 0;
}


void MyFreqCh(int pid, int newfreq)
{
	int arena = pd->players[pid].arena;
	ConfigHandle ch;

	ch = aman->arenas[arena].cfg;

	/* cfghelp: Team:AllowFreqOwners, arena, bool, def: 1
	 * Whether to enable the freq ownership feature in this arena. */
	if (CountFreq(arena, newfreq, pid) == 0 &&
	    cfg->GetInt(ch, "Team", "AllowFreqOwners", 1) &&
	    newfreq >= cfg->GetInt(ch, "Team", "PrivFreqStart", 100) &&
	    newfreq != cfg->GetInt(ch, "Team", "SpectatorFrequency", 8025))
	{
		ownsfreq[pid] = 1;
		chat->SendMessage(pid, "You are the now the owner of freq %d. "
				"You can kick people off your freq by sending them "
				"the private message \"?freqkick\", and you can share "
				"your ownership by sending people \"?giveowner\".", newfreq);
	}
	else
		ownsfreq[pid] = 0;
}


void MyShipCh(int pid, int newship, int newfreq)
{
	MyFreqCh(pid, newfreq);
}


