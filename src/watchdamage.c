
/* dist: public */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asss.h"
#include "watchdamage.h"


#define WATCHCOUNT(pid) \
	( LLCount(watches + pid) + modwatch[pid] )


/* global data */

local Iplayerdata *pd;
local Ichat *chat;
local Ilogman *lm;
local Icmdman *cmd;
local Inet *net;
local Imodman *mm;

local LinkedList watches[MAXPLAYERS];
local short modwatch[MAXPLAYERS];


/* functions to send packets */

local void ToggleWatch(int pid, byte on)
{
	byte pk[2] = { S2C_TOGGLEDAMAGE, on };

	if (pd->players[pid].type == T_CONT)
	{
		net->SendToOne(pid, pk, 2, NET_RELIABLE | NET_PRI_N1);

		/* for temp debugging to make sure we arn't sending more of these packets than we need */
		chat->SendMessage(pid, "(Your damage watching turned %s)", on ? "on" : "off");
	}
}

/* functions to handle increase/decreasing sizes */

local int AddWatch(int pid, int target)
{
	Link *l;

	/* check to see if already on */
	for (l = LLGetHead(watches + target); l; l = l->next)
		if ((short*)l->data - modwatch == pid)
			return -1;

	/* add new int to end of list */
	LLAdd(watches + target, modwatch + pid);

	/* check to see if need to send a packet */
	if (WATCHCOUNT(target) == 1)
		ToggleWatch(target, 1);

	return 1;
}

local void RemoveWatch(int pid, int target)
{
	if (WATCHCOUNT(target) == 0)
		return;

	LLRemoveAll(watches + target, modwatch + pid);

	/* check to see if need to send a packet */
	if (WATCHCOUNT(target) == 0)
		ToggleWatch(target, 0);
}

local void ClearWatch(int pid)
{
	int i;

	/* remove his watches on others */
	for (i = 0; i < MAXPLAYERS; i++)
	{
		RemoveWatch(pid, i);
	}

	/* remove people watching him */
	LLEmpty(watches + pid);
}

local void ModuleWatch(int pid, int on)
{
	if (on)
		modwatch[pid]++;
	else
		modwatch[pid] -= modwatch[pid] > 0 ? 1 : 0;
}

local int WatchCount(int pid)
{
	return WATCHCOUNT(pid);
}


local helptext_t watchdamage_help =
"Targets: player, freq, none\n"
"Args: [0 or 1]\n"
"Turns damage watching on and off. If sent to a player, an argument of 1\n"
"turns it on, 0 turns it off, and no argument toggles. If sent as a\n"
"public command, only {?watchdamage 0} is meaningful, and it turns off\n"
"damage watching on all players.\n";

local void Cwatchdamage(const char *params, int pid, const Target *target)
{
	int c;

	if (pd->players[pid].type != T_CONT)
	{
		if (chat) chat->SendMessage(pid, "This command requires you to use Continuum");
		return;
	}

	if (target->type == T_ARENA)
	{
		if (params[0] == '0' && params[1] == 0)
		{
			/* if sent publicly, turns off all their watches */
			for (c = 0; c < MAXPLAYERS; c++)
				RemoveWatch(pid, c);

			if (chat) chat->SendMessage(pid, "All damage watching turned off");
		}
	}
	else if (target->type == T_PID)
	{
		int t = target->u.pid;

		if (pd->players[t].type != T_CONT)
		{
			if (chat) chat->SendMessage(pid, "watchdamage requires %s to use Continuum", pd->players[t].name);
			return;
		}

		if (params[1] == 0 && (params[0] == '0' || params[0] == '1'))
		{
			/* force either on or off */
			if (params[0] == '0')
			{
				RemoveWatch(pid, t);
				if (chat) chat->SendMessage(pid, "Damage watching on %s turned off", pd->players[t].name);
			}
			else
			{
				AddWatch(pid, t);
				if (chat) chat->SendMessage(pid, "Damage watching on %s turned on", pd->players[t].name);
			}
		}
		else
		{
			/* toggle */
			if (AddWatch(pid, t) == -1)
			{
				RemoveWatch(pid, t);
				if (chat) chat->SendMessage(pid, "Damage watching on %s turned off", pd->players[t].name);
			}
			else
				if (chat) chat->SendMessage(pid, "Damage watching on %s turned on", pd->players[t].name);
		}
	}
	else
	{
		int set[MAXPLAYERS+1], *p;
		Target newtarg = { T_PID };

		pd->TargetToSet(target, set);
		for (p = set; *p != -1; p++)
		{
			newtarg.u.pid = *p;
			Cwatchdamage(params, pid, &newtarg);
		}
	}
}


local helptext_t watchwatchdamage_help = NULL;

local void Cwatchwatchdamage(const char *params, int pid, const Target *target)
{
	int i, mw = 0, tot = 0;
	Link *l;
	int on[MAXPLAYERS];

	memset(on, 0, sizeof(on));

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (modwatch[i])
			mw++;

		if (modwatch[i] || !LLIsEmpty(watches + i))
			tot++;

		for (l = LLGetHead(watches + i); l; l = l->next)
			on[((short*)l->data) - modwatch]++;
	}

	chat->SendMessage(pid, "wwd: total players reporting damage: %d", tot);
	chat->SendMessage(pid, "wwd: players reporting damage to modules: %d", mw);
	for (i = 0; i < MAXPLAYERS; i++)
		if (on[i])
			chat->SendMessage(pid, "wwd: %s is watching damage on %d players",
					pd->players[i].name, on[i]);
}


local void PAWatch(int pid, int action, int arena)
{
	/* if he leaves arena, clear all watches on him and his watches */
	if (action == PA_LEAVEARENA)
		ClearWatch(pid);
}

local void PDamage(int pid, byte *p, int len)
{
	struct C2SWatchDamage *wd = (struct C2SWatchDamage *)p;
	struct S2CWatchDamage s2cwd;
	Arena *arena = pd->players[pid].arena;
	Link *l;

	if (sizeof(struct C2SWatchDamage) != len)
	{
		if (lm) lm->Log(L_MALICIOUS, "<watchdamage> [pid=%d] Bad size for damage", pid);
		return;
	}

	if (!arena) return;

	s2cwd.type = S2C_DAMAGE;
	s2cwd.damageuid = pid;
	s2cwd.tick = wd->tick;
	s2cwd.shooteruid = wd->shooteruid;
	s2cwd.weapon = wd->weapon;
	s2cwd.energy = wd->energy;
	s2cwd.damage = wd->damage;
	s2cwd.unknown = wd->unknown;

	/* forward all damage packets to those watching */
	for (l = LLGetHead(watches + pid); l; l = l->next)
		net->SendToOne((short*)l->data - modwatch, (byte*)&s2cwd, sizeof(struct S2CWatchDamage), NET_RELIABLE | NET_PRI_N1);

	/* do callbacks only if a module is watching, since these can go in mass spamming sometimes */
	if (modwatch[pid] > 0)
		DO_CBS(CB_PLAYERDAMAGE, arena, PlayerDamage, (arena, pid, &s2cwd));
}


local Iwatchdamage _int =
{
	INTERFACE_HEAD_INIT(I_WATCHDAMAGE, "watchdamage")
	AddWatch, RemoveWatch, ClearWatch, ModuleWatch, WatchCount
};


EXPORT int MM_watchdamage(int action, Imodman *_mm, int arena)
{
	int c;
	if (action == MM_LOAD)
	{
		mm = _mm;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		lm = mm->GetInterface(I_LOGMAN, ALLARENAS);
		cmd = mm->GetInterface(I_CMDMAN, ALLARENAS);
		net = mm->GetInterface(I_NET, ALLARENAS);

		if (!cmd || !net) return MM_FAIL;

		mm->RegCallback(CB_PLAYERACTION, PAWatch, ALLARENAS);

		net->AddPacket(C2S_DAMAGE, PDamage);

		cmd->AddCommand("watchdamage", Cwatchdamage, watchdamage_help);
		cmd->AddCommand("watchwatchdamage", Cwatchwatchdamage, watchwatchdamage_help);

		mm->RegInterface(&_int, ALLARENAS);

		for (c = 0; c < MAXPLAYERS; c++)
		{
			LLInit(watches + c);
			modwatch[c] = 0;
		}

		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		if (mm->UnregInterface(&_int, ALLARENAS))
			return MM_FAIL;

		cmd->RemoveCommand("watchdamage", Cwatchdamage);
		cmd->RemoveCommand("watchwatchdamage", Cwatchwatchdamage);

		net->RemovePacket(C2S_DAMAGE, PDamage);

		mm->UnregCallback(CB_PLAYERACTION, PAWatch, ALLARENAS);

		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(lm);
		mm->ReleaseInterface(cmd);
		mm->ReleaseInterface(net);

		for (c = 0; c < MAXPLAYERS; c++)
			LLEmpty(watches + c);

		return MM_OK;
	}
	return MM_FAIL;
}


