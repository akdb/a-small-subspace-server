
/* dist: public */

/* information on the directory server protocol was obtained from
 * Hammuravi's page at
 * http://www4.ncsu.edu/~rniyenga/subspace/old/dprotocol.html
 */


#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#else
#include <winsock.h>
#define close(s) closesocket(s)
#endif

#include <string.h>
#include <stdio.h>

#include "asss.h"


/* the thing to send */
struct S2DInfo
{
	u32 ip;
	u16 port;
	u16 players;
	u16 scoresp;
	u32 version;
	char servername[32];
	char password[48];
	char description[386]; /* fill out to 480 bytes */
};



/* interface pointers */
local Iconfig *cfg;
local Imainloop *ml;
local Iplayerdata *pd;
local Ilogman *lm;
local Imodman *mm;

local LinkedList servers;
local LinkedList packets;
local int sock;


local int SendUpdates(void *dummy)
{
	int n, count = 0;
	Link *link, *l, *k;
	Player *p;

	lm->Log(L_DRIVEL, "<directory> Sending information to directory servers");

	/* figure out player count */
	pd->Lock();
	FOR_EACH_PLAYER(p)
		if (p->status == S_PLAYING && p->type != T_FAKE)
			count++;
	pd->Unlock();

	for (k = LLGetHead(&packets); k; k = k->next)
	{
		struct S2DInfo *data = k->data;
		data->players = count;
		n = offsetof(struct S2DInfo, description) + strlen(data->description) + 1;

		for (l = LLGetHead(&servers); l; l = l->next)
			sendto(
					sock,
					(byte*)data,
					n,
					0,
					(const struct sockaddr *)l->data,
					sizeof(struct sockaddr_in));
	}

	return TRUE;
}


local void init_data(void)
{
	Inet *net = mm->GetInterface(I_NET, ALLARENAS);
	int i;
	int port;
	char connectas[32];
	const char *pwd;

	/* cfghelp: Directory:Password, global, string, def: cane
	 * The password used to send information to the directory
	 * server. */
	pwd = cfg->GetStr(GLOBAL, "Directory", "Password");
	if (!pwd)
		pwd = "cane";

	LLInit(&packets);

	if (!net)
		return;

	for (i = 0; i < 10 && net->GetListenData(i, &port, connectas, sizeof(connectas)); i++)
	{
		const char *t;
		char secname[32];
		struct S2DInfo *data = amalloc(sizeof(*data));

		if (connectas[0])
			snprintf(secname, sizeof(secname), "Directory-%s", connectas);
		else
			astrncpy(secname, "Directory", sizeof(secname));

		/* hmm. should we be setting this to something other than zero?
		 * how about for virtual servers that bind to ips other than the
		 * default one? */
		data->ip = 0;
		data->port = port;
		data->players = 0; /* fill in later */;
		data->scoresp = 1; /* always keep scores */
		/* data->version = ASSSVERSION_NUM; */
		data->version = 134; /* priit's updated dirserv require this */

		/* cfghelp: Directory:Name, global, string
		 * The server name to send to the directory server. Virtual
		 * servers will use section name 'Directory-<vs-name>' for this
		 * and other settings in this section, and will fall back to
		 * 'Directory' if that section doesn't exist. See Net:Listen
		 * help for how to identify virtual servers. */
		if ((t = cfg->GetStr(GLOBAL, secname, "Name")))
			astrncpy(data->servername, t, sizeof(data->servername));
		else if ((t = cfg->GetStr(GLOBAL, "Directory", "Name")))
			astrncpy(data->servername, t, sizeof(data->servername));
		else
			astrncpy(data->servername, "<no name provided>", sizeof(data->servername));

		/* cfghelp: Directory:Description, global, string
		 * The server description to send to the directory server. See
		 * Directory:Name for more information about the section name.
		 * */
		if ((t = cfg->GetStr(GLOBAL, secname, "Description")))
			astrncpy(data->description, t, sizeof(data->description));
		else if ((t = cfg->GetStr(GLOBAL, "Directory", "Description")))
			astrncpy(data->description, t, sizeof(data->description));
		else
			astrncpy(data->description, "<no description provided>", sizeof(data->description));

		astrncpy(data->password, pwd, sizeof(data->password));

		if (connectas[0])
			lm->Log(L_INFO, "<directory> virtual server '%s' on port %d using name '%s'",
					connectas, port, data->servername);
		else
			lm->Log(L_INFO, "<directory> server on port %d using name '%s'",
					port, data->servername);

		LLAdd(&packets, data);
	}

}


local void init_servers(void)
{
	char skey[] = "Server#", pkey[] = "Port#";
	unsigned short i, defport, port;

	LLInit(&servers);

	/* cfghelp: Directory:Port, global, int, def: 4991
	 * The port to connect to for the directory server. */
	defport = cfg->GetInt(GLOBAL, "Directory", "Port", 4991);

	for (i = 1; i < 10; i++)
	{
		const char *name;

		skey[6] = '0' + i;
		pkey[4] = '0' + i;
		name = cfg->GetStr(GLOBAL, "Directory", skey);
		port = cfg->GetInt(GLOBAL, "Directory", pkey, defport);

		if (name)
		{
			struct sockaddr_in *sin;
			struct hostent *ent = gethostbyname(name);
			if (ent && ent->h_length == sizeof(sin->sin_addr))
			{
				sin = amalloc(sizeof(*sin));
				sin->sin_family = AF_INET;
				sin->sin_port = htons(port);
				memcpy(&sin->sin_addr, ent->h_addr, sizeof(sin->sin_addr));
				LLAdd(&servers, sin);
				lm->Log(L_INFO, "<directory> Using '%s' at %s as a directory server",
						ent->h_name, inet_ntoa(sin->sin_addr));
			}
		}
	}
}


local void deinit_servers(void)
{
	LLEnum(&servers, afree);
	LLEmpty(&servers);
	LLEnum(&packets, afree);
	LLEmpty(&packets);
}



EXPORT int MM_directory(int action, Imodman *mm_, Arena *arena)
{
	if (action == MM_LOAD)
	{
		mm = mm_;
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		lm = mm->GetInterface(I_LOGMAN, ALLARENAS);

		if (!cfg || !ml || !pd || !lm)
			return MM_FAIL;

		sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (sock == -1)
			return MM_FAIL;

		{
			struct sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = htons(0);
			sin.sin_addr.s_addr = INADDR_ANY;
			if (bind(sock, (struct sockaddr*)&sin, sizeof(sin)) == -1)
				return MM_FAIL;
		}

		init_data();
		init_servers();

		ml->SetTimer(SendUpdates, 1000, 6000, NULL, NULL);
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		ml->ClearTimer(SendUpdates, NULL);
		deinit_servers();
		close(sock);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(ml);
		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(lm);
		return MM_OK;
	}
	return MM_FAIL;
}

