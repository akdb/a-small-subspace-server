
#ifndef __CAPMAN_H
#define __CAPMAN_H

/* Icapman
 *
 * manages capabilities and authority for all players. other modules
 * should query this to discover if a player has a certain authority.
 *
 * capabilities are named in the following way:
 *
 * if a player has the capabilty "cmd_<some command>", he can use that
 * command "untargetted" (that is, typed as a public message).
 *
 * if a player has the capability "privcmd_<some command>", he can use
 * that command directed at a player or freq (private or team messages).
 *
 * other capabilites (e.g., "seeprivarenas") don't follow any special
 * naming convention.
 */

#define MAXGROUPLEN 32


typedef struct Icapman
{
	int (*HasCapability)(int pid, const char *cap);
	/* returns true if the given player has the given capability. */

	char *(*GetGroup)(int pid);
	void (*SetGroup)(int pid, const char *group);
	/* gets/sets the group of the player as specified. these functions
	 * are dependant on one specific implementation of capabilities, and
	 * should only be used from very few places. */
} Icapman;


#endif
