
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/mman.h>
#else
#include <io.h>
#endif

#include "zlib.h"

#include "asss.h"


struct MapDownloadData
{
	u32 checksum, uncmplen, cmplen;
	int optional;
	byte *cmpmap;
	char filename[20];
};


/* GLOBALS */

local Imodman *mm;
local Iplayerdata *pd;
local Iconfig *cfg;
local Inet *net;
local Ilogman *lm;
local Iarenaman *aman;
local Imainloop *ml;

local PlayerData *players;
local ArenaData *arenas;

local LinkedList mapdldata[MAXARENA];

local const char *cfg_newsfile;
local u32 newschecksum, cmpnewssize;
local byte *cmpnews;
local time_t newstime;


/* functions */

local int RefreshNewsTxt(void *dummy)
{
	int fd, fsize;
	time_t newtime;
	uLong csize;
	byte *news, *cnews;
	struct stat st;

	fd = open(cfg_newsfile, O_RDONLY);
	if (fd == -1)
	{
		lm->Log(L_WARN,"<mapnewsdl> News file '%s' not found in current directory", cfg_newsfile);
		return 1; /* let's get called again in case the file's been replaced */
	}

	/* find it's size */
	fstat(fd, &st);
	newtime = st.st_mtime + st.st_ctime;
	if (newtime != newstime)
	{
#ifdef WIN32
		HANDLE hfile, hnews;
#endif
		newstime = newtime;
		fsize = st.st_size;
		csize = 1.0011 * fsize + 35;

		/* mmap it */
#ifndef WIN32
		news = mmap(NULL, fsize, PROT_READ, MAP_SHARED, fd, 0);
		if (news == (void*)-1)
		{
			lm->Log(L_ERROR,"<mapnewsdl> mmap failed in RefreshNewsTxt");
			close(fd);
			return 1;
		}
#else
		hfile = CreateFile(cfg_newsfile,
				GENERIC_READ,
				FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
				0);
		if (hfile == INVALID_HANDLE_VALUE)
		{
			lm->Log(L_ERROR,"<mapnewsdl> CreateFile failed in RefreshNewsTxt, error %d",
					GetLastError());
			return 1;
		}
		hnews = CreateFileMapping(hfile,NULL,PAGE_READONLY,0,0,0);
		if (!hnews)
		{
			CloseHandle(hfile);
			lm->Log(L_ERROR,"<mapnewsdl> CreateFileMapping failed in RefreshNewsTxt, error %d",
					GetLastError());
			return 1;
		}
		news = MapViewOfFile(hnews,FILE_MAP_READ,0,0,0);
		if (!news)
		{
			CloseHandle(hnews);
			CloseHandle(hfile);
			lm->Log(L_ERROR,"<mapnewsdl> mapviewoffile failed in RefreshNewsTxt, error %d", GetLastError());
			return 1;
		}
#endif

		/* calculate crc on mmap'd map */
		newschecksum = crc32(crc32(0, Z_NULL, 0), news, fsize);

		/* allocate space for compressed version */
		cnews = amalloc(csize);

		/* set up packet header */
		cnews[0] = S2C_INCOMINGFILE;
		/* 16 bytes of zero for the name */

		/* compress the stuff! */
		compress(cnews+17, &csize, news, fsize);

		/* shrink the allocated memory */
		cnews = realloc(cnews, csize+17);
		if (!cnews)
		{
			lm->Log(L_ERROR,"<mapnewsdl> realloc failed in RefreshNewsTxt");
			close(fd);
			return 1;
		}
		cmpnewssize = csize+17;

#ifndef WIN32
		munmap(news, fsize);
#else
		UnmapViewOfFile(news);
		CloseHandle(hnews);
		CloseHandle(hfile);
#endif

		if (cmpnews) afree(cmpnews);
		cmpnews = cnews;
		lm->Log(L_DRIVEL,"<mapnewsdl> News file '%s' reread", cfg_newsfile);
	}
	close(fd);
	return 1;
}


local u32 GetNewsChecksum(void)
{
	if (!cmpnews)
		RefreshNewsTxt(0);
	return newschecksum;
}


#include "packets/mapfname.h"

local void SendMapFilename(int pid)
{
	struct MapFilename *mf;
	struct MapDownloadData *data;
	int arena, len, wantopt = WANT_ALL_LVZ(pid);

	arena = pd->players[pid].arena;

	if (LLIsEmpty(mapdldata + arena))
	{
		lm->LogA(L_WARN, "<mapnewsdl>", arena, "Missing map data");
		return;
	}

	if (players[pid].type != T_CONT)
	{
		data = LLGetHead(mapdldata + arena)->data;
		mf = amalloc(21);

		strncpy(mf->files[0].filename, data->filename, 16);
		mf->files[0].checksum = data->checksum;
		len = 21;
	}
	else
	{
		int idx = 0;
		Link *l;

		/* allocate for the maximum possible */
		mf = amalloc(sizeof(mf->files[0]) * LLCount(mapdldata + arena));

		for (l = LLGetHead(mapdldata + arena); l; l = l->next)
		{
			data = l->data;
			if (!data->optional || wantopt)
			{
				strncpy(mf->files[idx].filename, data->filename, 16);
				mf->files[idx].checksum = data->checksum;
				mf->files[idx].size = data->uncmplen;
				idx++;
			}
		}
		len = 1 + sizeof(mf->files[0]) * idx;
	}

	mf->type = S2C_MAPFILENAME;
	net->SendToOne(pid, (byte*)mf, len, NET_RELIABLE);
}



local struct MapDownloadData * compress_map(const char *fname, int docomp)
{
	byte *map, *cmap;
	int mapfd, fsize;
	uLong csize;
	struct stat st;
	const char *mapname;
#ifdef WIN32
	HANDLE hfile, hmap;
#endif
	struct MapDownloadData *data;

	data = amalloc(sizeof(*data));

	/* get basename */
	mapname = strrchr(fname, '/');
	if (!mapname)
		mapname = fname;
	else
		mapname++;
	astrncpy(data->filename, mapname, 20);

	mapfd = open(fname, O_RDONLY);
	if (mapfd == -1)
		goto fail1;

	/* find it's size */
	fstat(mapfd, &st);
	fsize = st.st_size;
	if (docomp)
		csize = 1.0011 * fsize + 35;
	else
		csize = fsize + 17;

	/* mmap it */
#ifndef WIN32
	map = mmap(NULL, fsize, PROT_READ, MAP_SHARED, mapfd, 0);
	if (map == (void*)-1)
	{
		lm->Log(L_ERROR,"<mapnewsdl> mmap failed for map '%s'", fname);
		close(mapfd);
		goto fail1;
	}
#else
	hfile = CreateFile(fname,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			0);
	if (hfile == INVALID_HANDLE_VALUE)
	{
		lm->Log(L_ERROR,"<mapnewsdl> CreateFile failed for map '%s', error %d",
				fname, GetLastError());
		goto fail1;
	}
	hmap = CreateFileMapping(hfile,NULL,PAGE_READONLY,0,0,0);
	if (!hmap)
	{
		CloseHandle(hfile);
		lm->Log(L_ERROR,"<mapnewsdl> CreateFileMapping failed for map '%s', error %d",
				fname, GetLastError());
		goto fail1;
	}
	map = MapViewOfFile(hmap,FILE_MAP_READ,0,0,0);
	if (!map)
	{
		CloseHandle(hmap);
		CloseHandle(hfile);
		lm->Log(L_ERROR,"<mapnewsdl> mmap failed for map '%s'", fname);
		goto fail1;
	}
#endif

	/* calculate crc on mmap'd map */
	data->checksum = crc32(crc32(0, Z_NULL, 0), map, fsize);
	data->uncmplen = fsize;

	/* allocate space for compressed version */
	cmap = amalloc(csize);

	/* set up packet header */
	cmap[0] = S2C_MAPDATA;
	strncpy(cmap+1, mapname, 16);

	if (docomp)
	{
		/* compress the stuff! */
		compress(cmap+17, &csize, map, fsize);
		csize += 17;

		/* shrink the allocated memory */
		data->cmpmap = realloc(cmap, csize);
		if (data->cmpmap == NULL)
		{
			lm->Log(L_ERROR,"<mapnewsdl> realloc failed in compress_map");
			free(cmap);
			goto fail1;
		}
	}
	else
	{
		/* just copy */
		memcpy(cmap+17, map, fsize);
		data->cmpmap = cmap;
	}

	data->cmplen = csize;

#ifndef WIN32
	munmap(map, fsize);
#else
	UnmapViewOfFile(map);
	CloseHandle(hmap);
	CloseHandle(hfile);
#endif
	close(mapfd);

	return data;

fail1:
	afree(data);
	return NULL;
}

local void free_maps(int arena)
{
	Link *l;
	for (l = LLGetHead(mapdldata + arena); l; l = l->next)
	{
		struct MapDownloadData *data = l->data;
		afree(data->cmpmap);
		afree(data);
	}
	LLEmpty(mapdldata + arena);
}


#include "pathutil.h"

local int real_get_filename(int arena, const char *map, char *buffer, int bufferlen)
{
	struct replace_table repls[2] =
	{
		{'a', aman->arenas[arena].name},
		{'m', map}
	};

	if (!map) return -1;

	return find_file_on_path(
			buffer,
			bufferlen,
			CFG_MAP_SEARCH_PATH,
			repls,
			2);
}

local void ArenaAction(int arena, int action)
{
	/* clear any old maps lying around */
	if (action == AA_CREATE || action == AA_DESTROY)
		free_maps(arena);

	if (action == AA_CREATE)
	{
		struct MapDownloadData *data;
		char lvzname[256], fname[256];
		const char *lvzs, *tmp = NULL;

		/* first add the map itself */
		if (real_get_filename(
					arena,
					cfg->GetStr(aman->arenas[arena].cfg, "General", "Map"),
					fname,
					256) != -1)
		{
			data = compress_map(fname, 1);
			if (data)
				LLAdd(mapdldata + arena, data);
		}

		/* now look for lvzs */
		lvzs = cfg->GetStr(aman->arenas[arena].cfg, "Misc", "LevelFiles");
		if (!lvzs) lvzs = cfg->GetStr(aman->arenas[arena].cfg, "General", "Lvzs");
		while (strsplit(lvzs, ",: ", lvzname, 256, &tmp))
			if (real_get_filename(arena, lvzname, fname, 256) != -1)
			{
				data = compress_map(fname, 0);
				if (data)
					LLAdd(mapdldata + arena, data);
			}
	}
}


local void PMapRequest(int pid, byte *p, int len)
{
	int arena = players[pid].arena;

	if (p[0] == C2S_MAPREQUEST)
	{
		struct MapDownloadData *data;
		Link *l;
		unsigned short lvznum = (len == 3) ? p[1] | p[2]<<8 : 0, idx;

		if (ARENA_BAD(arena))
		{
			lm->Log(L_MALICIOUS, "<mapnewsdl> [%s] Map request before entering arena",
					players[pid].name);
			return;
		}

		/* find the right spot */
		for (idx = lvznum, l = LLGetHead(mapdldata + arena); idx && l; idx--, l = l->next) ;

		if (!l || !l->data)
		{
			lm->LogA(L_WARN, "mapnewsdl", arena, "Can't find lvz number %d", lvznum);
			return;
		}

		data = l->data;

		if (!data->cmpmap)
		{
			lm->LogA(L_ERROR, "mapnewsdl", arena, "Missing compressed map!");
			return;
		}

		net->SendToOne(pid, data->cmpmap, data->cmplen, NET_RELIABLE | NET_PRESIZE);
		lm->Log(L_DRIVEL,"<mapnewsdl> {%s} [%s] Sending map/lvz %d",
				arenas[arena].name,
				players[pid].name,
				lvznum);
	}
	else if (p[0] == C2S_NEWSREQUEST)
	{
		if (cmpnews)
		{
			net->SendToOne(pid, cmpnews, cmpnewssize, NET_RELIABLE | NET_PRESIZE);
			lm->Log(L_DRIVEL,"<mapnewsdl> [%s] Sending news.txt", players[pid].name);
		}
		else
			lm->Log(L_WARN, "<mapnewsdl> News request, but compressed news doesn't exist");
	}
}



/* interface */

local Imapnewsdl _int =
{
	INTERFACE_HEAD_INIT(I_MAPNEWSDL, "mapnewsdl")
	SendMapFilename, GetNewsChecksum
};


EXPORT int MM_mapnewsdl(int action, Imodman *mm_, int arena)
{
	if (action == MM_LOAD)
	{
		/* get interface pointers */
		mm = mm_;
		pd = mm->GetInterface(I_PLAYERDATA, ALLARENAS);
		net = mm->GetInterface(I_NET, ALLARENAS);
		lm = mm->GetInterface(I_LOGMAN, ALLARENAS);
		cfg = mm->GetInterface(I_CONFIG, ALLARENAS);
		ml = mm->GetInterface(I_MAINLOOP, ALLARENAS);
		aman = mm->GetInterface(I_ARENAMAN, ALLARENAS);

		if (!net || !cfg || !lm || !ml || !aman || !pd) return MM_FAIL;

		players = pd->players;
		arenas = aman->arenas;

		for (arena = 0; arena < MAXARENA; arena++)
			LLInit(mapdldata + arena);

		/* set up callbacks */
		net->AddPacket(C2S_MAPREQUEST, PMapRequest);
		net->AddPacket(C2S_NEWSREQUEST, PMapRequest);
		mm->RegCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);

		/* reread news every 5 min */
		ml->SetTimer(RefreshNewsTxt, 50, 
				cfg->GetInt(GLOBAL, "General", "NewsRefreshMinutes", 5)
				* 60 * 100, NULL);

		/* cache some config data */
		cfg_newsfile = cfg->GetStr(GLOBAL, "General", "NewsFile");
		if (!cfg_newsfile) cfg_newsfile = "news.txt";
		newstime = 0; cmpnews = NULL;

		mm->RegInterface(&_int, ALLARENAS);
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		if (mm->UnregInterface(&_int, ALLARENAS))
			return MM_FAIL;
		net->RemovePacket(C2S_MAPREQUEST, PMapRequest);
		net->RemovePacket(C2S_NEWSREQUEST, PMapRequest);
		mm->UnregCallback(CB_ARENAACTION, ArenaAction, ALLARENAS);

		/* clear any old maps lying around */
		for (arena = 0; arena < MAXARENA; arena++)
			free_maps(arena);

		afree(cmpnews);
		ml->ClearTimer(RefreshNewsTxt);

		mm->ReleaseInterface(pd);
		mm->ReleaseInterface(net);
		mm->ReleaseInterface(lm);
		mm->ReleaseInterface(cfg);
		mm->ReleaseInterface(ml);
		mm->ReleaseInterface(aman);
		return MM_OK;
	}
	return MM_FAIL;
}

