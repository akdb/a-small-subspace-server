
/* dist: public */

#include "asss.h"


/* commands */
local void Cgiveowner(const char *, Player *, const Target *);
local void Cfreqkick(const char *, Player *, const Target *);
local helptext_t giveowner_help, freqkick_help;

/* callbacks */
local void MyPA(Player *p, int action, Arena *arena);
local void MyFreqCh(Player *p, int newfreq);
local void MyShipCh(Player *p, int newship, int newfreq);

/* data */
local int ofkey;
#define OWNSFREQ(p) (*(char*)PPDATA(p, ofkey))

local Iplayerdata *pd;
local Iarenaman *aman;
local Igame *game;
local Icmdman *cmd;
local Iconfig *cfg;
local Ichat *chat;
local Imodman *mm;


EXPORT int MM_freqowners(int action, Imodman *_mm, Arena *arena)
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
		if (!pd || !aman || !game || !cmd || !cfg) return MM_FAIL;

		ofkey = pd->AllocatePlayerData(sizeof(char));
		if (ofkey == -1) return MM_FAIL;

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
		pd->FreePlayerData(ofkey);
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


local int CountFreq(Arena *arena, int freq, Player *excl)
{
	int t = 0;
	Player *i;
	Link *link;
	pd->Lock();
	FOR_EACH_PLAYER(i)
		if (i->arena == arena &&
		    i->p_freq == freq &&
		    i != excl)
			t++;
	pd->Unlock();
	return t;
}


local helptext_t giveowner_help =
"Module: freqownsers\n"
"Targets: player\n"
"Args: none\n"
"Allows you to share freq ownership with another player on your current\n"
"private freq. You can't remove ownership once you give it out, but you\n"
"are safe from being kicked off yourself, as long as you have ownership.\n";

void Cgiveowner(const char *params, Player *p, const Target *target)
{
	if (target->type != T_PLAYER)
		return;
	
	if (OWNSFREQ(p) &&
	    p->arena == target->u.p->arena &&
	    p->p_freq == target->u.p->p_freq)
		OWNSFREQ(target->u.p) = 1;
}


local helptext_t freqkick_help =
"Module: freqowners\n"
"Targets: player\n"
"Args: none\n"
"Kicks the player off of your freq. The player must be on your freq and\n"
"must not be an owner himself. The player giving the command, of course,\n"
"must be an owner.\n";

void Cfreqkick(const char *params, Player *p, const Target *target)
{
	Player *t = target->u.p;

	if (target->type != T_PLAYER)
		return;

	if (OWNSFREQ(p) && !OWNSFREQ(t) &&
	    p->arena == t->arena &&
	    p->p_freq == t->p_freq)
	{
		game->SetShip(t, SPEC);
		chat->SendMessage(t, "You have been kicked off the freq by %s",
				p->name);
	}
}


void MyPA(Player *p, int action, Arena *arena)
{
	OWNSFREQ(p) = 0;
}


void MyFreqCh(Player *p, int newfreq)
{
	Arena *arena = p->arena;
	ConfigHandle ch = arena->cfg;

	/* cfghelp: Team:AllowFreqOwners, arena, bool, def: 1
	 * Whether to enable the freq ownership feature in this arena. */
	if (CountFreq(arena, newfreq, p) == 0 &&
	    cfg->GetInt(ch, "Team", "AllowFreqOwners", 1) &&
	    newfreq >= cfg->GetInt(ch, "Team", "PrivFreqStart", 100) &&
	    newfreq != cfg->GetInt(ch, "Team", "SpectatorFrequency", 8025))
	{
		OWNSFREQ(p) = 1;
		chat->SendMessage(p, "You are the now the owner of freq %d. "
				"You can kick people off your freq by sending them "
				"the private message \"?freqkick\", and you can share "
				"your ownership by sending people \"?giveowner\".", newfreq);
	}
	else
		OWNSFREQ(p) = 0;
}


void MyShipCh(Player *p, int newship, int newfreq)
{
	MyFreqCh(p, newfreq);
}


