
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "asss.h"

#include "packets/login.h"

#include "packets/billmisc.h"

/* prototypes */

/* interface: */
local void SendToBiller(byte *, int, int);
local void AddPacket(byte, PacketFunc);
local void RemovePacket(byte, PacketFunc);
local int GetStatus(void);

/* local: */
local void BillingAuth(int, struct LoginPacket *, void (*)(int, AuthData*));

local int SendPing(void *);
local void SendLogin(int, byte *, int);

local void BAuthResponse(int, byte *, int);
local void BChatMsg(int, byte *, int);
local void BMessage(int, byte *, int);
local void BSingleMessage(int, byte *, int);

local void PChat(int, byte *, int);

local void DefaultCmd(const char *, int, int);


/* global data */

local Inet *net;
local Imainloop *ml;
local Ilogman *log;
local Iconfig *cfg;
local Icmdman *cmd;
local Iplayerdata *pd;
local Imodman *mm;

local void (*CachedAuthDone)(int, AuthData*);
local PlayerData *players;

local Iauth _iauth = { BillingAuth };
local Ibillcore _ibillcore =
{ SendToBiller, AddPacket, RemovePacket, GetStatus };

local int cfg_pingtime, cfg_serverid, cfg_groupid, cfg_scoreid;


int MM_billcore(int action, Imodman *_mm, int arena)
{
	if (action == MM_LOAD)
	{
		mm = _mm;
		mm->RegInterest(I_PLAYERDATA, &pd);
		mm->RegInterest(I_NET, &net);
		mm->RegInterest(I_MAINLOOP, &ml);
		mm->RegInterest(I_LOGMAN, &log);
		mm->RegInterest(I_CONFIG, &cfg);
		mm->RegInterest(I_CMDMAN, &cmd);

		if (!net || !ml || !cfg || !cmd) return MM_FAIL;

		players = pd->players;

		cfg_pingtime = cfg->GetInt(GLOBAL, "Billing", "PingTime", 3000);
		cfg_serverid = cfg->GetInt(GLOBAL, "Billing", "ServerId", 5000),
		cfg_groupid = cfg->GetInt(GLOBAL, "Billing", "GroupId", 1),
		cfg_scoreid = cfg->GetInt(GLOBAL, "Billing", "ScoreId", 5000),

		ml->SetTimer(SendPing, 300, 3000, NULL);

		/* packets from billing server */
		AddPacket(0, SendLogin); /* sent from net when it's time to contact biller */
		AddPacket(B2S_PLAYERDATA, BAuthResponse);
		AddPacket(B2S_CHATMSG, BChatMsg);
		AddPacket(B2S_MESSAGE, BMessage);
		AddPacket(B2S_SINGLEMESSAGE, BSingleMessage);

		/* packets from clients */
		net->AddPacket(C2S_CHAT, PChat);

		cmd->AddCommand(NULL, DefaultCmd);

		mm->RegInterface(I_AUTH, &_iauth);
		mm->RegInterface(I_BILLCORE, &_ibillcore);
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		byte dis = S2B_LOGOFF;

		/* send logoff packet (immediate so it gets there before */
		/* connection drop) */
		SendToBiller(&dis, 1, NET_RELIABLE | NET_IMMEDIATE);
		net->DropClient(PID_BILLER);

		cmd->RemoveCommand(NULL, DefaultCmd);

		RemovePacket(0, SendLogin);
		RemovePacket(B2S_PLAYERDATA, BAuthResponse);
		ml->ClearTimer(SendPing);
		mm->UnregInterface(I_AUTH, &_iauth);
		mm->UnregInterface(I_BILLCORE, &_ibillcore);

		mm->UnregInterest(I_PLAYERDATA, &pd);
		mm->UnregInterest(I_NET, &net);
		mm->UnregInterest(I_MAINLOOP, &ml);
		mm->UnregInterest(I_LOGMAN, &log);
		mm->UnregInterest(I_CONFIG, &cfg);
		mm->UnregInterest(I_CMDMAN, &cmd);
		return MM_OK;
	}
	else if (action == MM_CHECKBUILD)
		return BUILDNUMBER;
	return MM_FAIL;
}


void SendToBiller(byte *data, int length, int flags)
{
	net->SendToOne(PID_BILLER, data, length, flags);
}

void AddPacket(byte pktype, PacketFunc func)
{
	net->AddPacket(pktype + PKT_BILLBASE, func);
}

void RemovePacket(byte pktype, PacketFunc func)
{
	net->RemovePacket(pktype + PKT_BILLBASE, func);
}

int GetStatus(void)
{
	return players[PID_BILLER].status;
}


int SendPing(void *dummy)
{
	int status;
	status = GetStatus();
	if (status == BNET_NOBILLING)
	{	/* no communication yet, send initiation packet */
		byte initiate[8] = { 0x00, 0x01, 0xDA, 0x8F, 0xFD, 0xFF, 0x01, 0x00 };
		SendToBiller(initiate, 8, NET_UNRELIABLE);
		log->Log(L_INFO, "<billcore> Attempting to connect to billing server");
	}
	else if (status == BNET_CONNECTED)
	{	/* connection established, send ping */
		byte ping = S2B_KEEPALIVE;
		SendToBiller(&ping, 1, NET_RELIABLE);
	}
	return 1;
}


void SendLogin(int pid, byte *p, int n)
{
	struct S2BLogin to =
	{
		S2B_LOGIN, cfg_serverid, cfg_groupid, cfg_scoreid,
		"<default zone name>", "password"
	};
	char *t;

	log->Log(L_INFO, "<billcore> Billing server contacted, sending zone information");
	t = cfg->GetStr(GLOBAL, "Billing", "ServerName");
	if (t) astrncpy(to.name, t, 0x80);
	t = cfg->GetStr(GLOBAL, "Billing", "Password");
	if (t) astrncpy(to.pw, t, 0x20);
	SendToBiller((byte*)&to, sizeof(to), NET_RELIABLE);
}


void DefaultCmd(const char *cmd, int pid, int target)
{
	struct S2BCommand *to;

	if (target == TARGET_ARENA)
	{
		to = alloca(strlen(cmd)+6);
		to->type = S2B_COMMAND;
		to->pid = pid;
		strcpy(to->text, cmd);
		SendToBiller((byte*)to, strlen(cmd)+6, NET_RELIABLE);
	}
}


void BillingAuth(int pid, struct LoginPacket *lp, void (*Done)(int, AuthData*))
{
	struct S2BPlayerEntering to =
	{
		S2B_PLAYERLOGIN,
		lp->flags,
		net->GetIP(pid),
		"", "",
		pid,
		lp->D1,
		300, 0
	};

	if (GetStatus() == BNET_CONNECTED)
	{
		astrncpy(to.name, lp->name, 32);
		astrncpy(to.pw, lp->password, 32);
		SendToBiller((byte*)&to, sizeof(to), NET_RELIABLE);
		CachedAuthDone = Done;
	}
	else
	{	/* DEFAULT TO OLD AUTHENTICATION if billing server not available */
		AuthData auth;
		memset(&auth, 0, sizeof(auth));
		auth.code = AUTH_NOSCORES; /* tell client no scores kept */
		astrncpy(auth.name, lp->name, 24);
		Done(pid, &auth);
	}
}


void BAuthResponse(int bpid, byte *p, int n)
{
	struct B2SPlayerResponse *r = (struct B2SPlayerResponse *)p;
	AuthData ad;
	int pid = r->pid;

	memset(&ad, 0, sizeof(ad));
	/*ad.demodata = 0; // FIXME: figure out where in the billing response that is */
	ad.code = r->loginflag;
	astrncpy(ad.name, r->name, 24);
	astrncpy(ad.squad, r->squad, 24);
	/* all scores are local!
	if (n >= sizeof(struct B2SPlayerResponse))
	{
		players[pid].wins = ad.wins = r->wins;
		players[pid].losses = ad.losses = r->losses;
		players[pid].flagpoints = ad.flagpoints = r->flagpoints;
		players[pid].killpoints = ad.killpoints = r->killpoints;
	}
	*/
	CachedAuthDone(pid, &ad);
	/* FIXME: do something about userid and usage information */
	/* FIXME: handle banner data in banner module */
}


void BChatMsg(int pid, byte *p, int len)
{
	struct B2SChat *from = (struct B2SChat*)p;
	struct ChatPacket *to = amalloc(len + 6);
	
	to->pktype = S2C_CHAT;
	to->type = MSG_CHAT;
	sprintf(to->text, "%i:%s", from->channel, from->text);
	net->SendToOne(from->pid, (byte*)to, len+6, NET_RELIABLE);
}


/* this does remote privs as well as ** and *szone messages */
void BMessage(int pid, byte *p, int len)
{
	struct B2SRemotePriv *from = (struct B2SRemotePriv*)p;
	struct ChatPacket *to = alloca(len);
	char *msg = from->text, *t;
	int targ;

	if (msg[0] == ':')
	{	/* remote priv message */
		t = strchr(msg+1, ':');
		if (!t)
		{	/* no matching : */
			log->Log(L_MALICIOUS, "<billcore> Malformed remote private message from biller");
		}
		else
		{
			*t = 0;
			to->pktype = S2C_CHAT;
			to->type = MSG_INTERARENAPRIV;
			strcpy(to->text, t+1);
			if (msg[1] == '#')
			{	/* squad msg */
				int set[MAXPLAYERS], setc = 0, i;
				pd->LockStatus();
				for (i = 0; i < MAXPLAYERS; i++)
					if (	players[i].status == S_CONNECTED &&
							strcasecmp(msg+2, players[i].squad) == 0)
						set[setc++] = i;
				pd->UnlockStatus();
				set[setc] = -1;
				net->SendToSet(set, (byte*)to, strlen(t+1)+6, NET_RELIABLE);
			}
			else
			{	/* normal priv msg */
				targ = pd->FindPlayer(msg+1);
				if (targ >= 0)
					net->SendToOne(targ, (byte*)to, strlen(t+1)+6, NET_RELIABLE);
			}
			*t = ':';
		}
	}
	else
	{	/* ** or *szone message, send to all */
		to->pktype = S2C_CHAT;
		to->type = MSG_ARENA;
		to->sound = 0;
		strcpy(to->text, msg);
		net->SendToAll((byte*)to, strlen(msg)+6, NET_RELIABLE);
		log->Log(L_WARN, "<billcore> Broadcast message from biller: '%s'", msg);
	}
}


void BSingleMessage(int pid, byte *p2, int len)
{
	struct S2BCommand *p = (struct S2BCommand*)p2; /* minor hack: reusing struct */
	struct ChatPacket *to = amalloc(len);

	to->pktype = S2C_CHAT;
	to->type = MSG_ARENA;
	strcpy(to->text, p->text);
	net->SendToOne(p->pid, (byte*)to, len, NET_RELIABLE);
}


void PChat(int pid, byte *p, int len)
{
	struct ChatPacket *from = (struct ChatPacket *)p;
	char *t;
	int l;

	if (from->type == MSG_CHAT)
	{
		struct S2BChat *to = alloca(len+32); /* +32 = diff in packet sizes */

		to->type = S2B_CHATMSG;
		to->pid = pid;
		
		if ((t = strchr(from->text, ';')))
		{
			*t = 0;
			len -= strlen(from->text) + 1; /* don't send bytes after null */
			astrncpy(to->channel, from->text, 32);
			strcpy(to->text, t+1);
			*t = ';';
		}
		else
		{
			strcpy(to->text, from->text);
		}

		SendToBiller((byte*)to, len+32, NET_RELIABLE);
	}
	else if (from->type == MSG_INTERARENAPRIV)
	{
		struct S2BRemotePriv *to = alloca(len + 40); /* long enough for anything */

		t = strchr(from->text+1, ':');
		if (from->text[0] != ':' || !t)
		{
			log->Log(L_MALICIOUS,"<billcore> [%s] Malformed remote private message '%s'", players[pid].name, from->text);
		}
		else
		{
			to->type = S2B_PRIVATEMSG;
			to->groupid = cfg_groupid;
			to->unknown1 = 2;
			*t = 0;
			l = sprintf(to->text, ":%s:(%s)>%s",
					from->text+1,
					players[pid].name,
					t+1);
			SendToBiller((byte*)to, l+12, NET_RELIABLE);
			*t = ':';
		}
	}
}





