
/* dist: public */

#include <string.h>

#include "asss.h"
#include "cfghelp.h"

local Ichat *chat;
local Icmdman *cmdman;
local Icfghelp *cfghelp;


local void do_cmd_help(Player *p, const char *cmd)
{
	char buf[256], *t;
	const char *temp = NULL;
	helptext_t ht;

	ht = cmdman->GetHelpText(cmd, p->arena);

	if (ht)
	{
		chat->SendMessage(p, "Help on '?%s':", cmd);
		while (strsplit(ht, "\n", buf, 256, &temp))
		{
			for (t = buf; *t; t++)
				if (*t == '{' || *t == '}')
					*t = '\'';
			chat->SendMessage(p, "  %s", buf);
		}
	}
	else
		chat->SendMessage(p, "Sorry, I don't know anything about ?%s", cmd);
}


local void send_msg_cb(const char *line, void *clos)
{
	chat->SendMessage((Player*)clos, "  %s", line);
}

local void do_list_sections(Player *p)
{
	chat->SendMessage(p, "Known config file sections:");
	wrap_text(cfghelp->all_section_names, 80, ' ', send_msg_cb, p);
}

local void do_list_keys(Player *p, const char *sec)
{
	const struct section_help *sh = cfghelp->find_sec(sec);
	if (sh)
	{
		chat->SendMessage(p, "Known keys in section %s:", sec);
		wrap_text(sh->all_key_names, 80, ' ', send_msg_cb, p);
	}
	else
		chat->SendMessage(p, "I don't know anything about section %s", sec);
}

local void do_setting_help(Player *p, const char *sec, const char *key)
{
	const struct section_help *sh = cfghelp->find_sec(sec);
	if (sh)
	{
		const struct key_help *kh = cfghelp->find_key(sh, key);
		if (kh)
		{
			chat->SendMessage(p, "Help on setting %s:%s",
					sh->name, kh->name);
			if (kh->mod)
				chat->SendMessage(p, "  Requires module: %s", kh->mod);
			chat->SendMessage(p, "  Location: %s", kh->loc);
			chat->SendMessage(p, "  Type: %s", kh->type);
			if (kh->range)
				chat->SendMessage(p, "  Range: %s", kh->range);
			if (kh->def)
				chat->SendMessage(p, "  Default: %s", kh->def);
			wrap_text(kh->helptext, 80, ' ', send_msg_cb, p);
		}
		else
			chat->SendMessage(p, "I don't know anything about key %s", key);
	}
	else
		chat->SendMessage(p, "I don't know anything about section %s", sec);
}


local helptext_t help_help =
"Targets: none\n"
"Args: <command name> | <setting name (section:key)>\n"
"Displays help on a command or config file setting. Use {?help section:}\n"
"to list known keys in that section. Use {?help :} to list known section\n"
"names.\n";

local void Chelp(const char *params, Player *p, const Target *target)
{

	if (params[0] == '?' || params[0] == '*' || params[0] == '!')
		params++;

	if (params[0] == '\0')
		params = "help";

	if (strchr(params, ':'))
	{
		/* setting */
		char secname[MAXSECTIONLEN];
		const char *keyname;

		if (!cfghelp)
		{
			chat->SendMessage(p, "Config file settings help isn't loaded.");
			return;
		}

		keyname = delimcpy(secname, params, MAXSECTIONLEN, ':');

		if (secname[0] == '\0')
			do_list_sections(p);
		else if (keyname[0] == '\0')
			do_list_keys(p, secname);
		else
			do_setting_help(p, secname, keyname);
	}
	else
		/* command */
		do_cmd_help(p, params);
}


EXPORT int MM_help(int action, Imodman *mm, Arena *arena)
{
	if (action == MM_LOAD)
	{
		chat = mm->GetInterface(I_CHAT, ALLARENAS);
		cmdman = mm->GetInterface(I_CMDMAN, ALLARENAS);
		cfghelp = mm->GetInterface(I_CFGHELP, ALLARENAS);
		if (!chat || !cmdman)
			return MM_FAIL;

		cmdman->AddCommand("help", Chelp, help_help);
		return MM_OK;
	}
	else if (action == MM_UNLOAD)
	{
		cmdman->RemoveCommand("help", Chelp);
		mm->ReleaseInterface(chat);
		mm->ReleaseInterface(cmdman);
		mm->ReleaseInterface(cfghelp);
		return MM_OK;
	}
	return MM_FAIL;
}

