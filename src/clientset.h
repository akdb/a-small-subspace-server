
/* dist: public */

#ifndef __CLIENTSET_H
#define __CLIENTSET_H

/* Iclientset
 *
 * this is the interface to the module that manages the client-side
 * settings. it loads them from disk when the arena is loaded and when
 * Reconfigure is called. arenaman calls SendClientSettings as part of
 * the arena response procedure.
 */

#define I_CLIENTSET "clientset-1"

typedef struct Iclientset
{
	INTERFACE_HEAD_DECL

	void (*SendClientSettings)(int pid);

	void (*Reconfigure)(Arena *arena);

	u32 (*GetChecksum)(Arena *arena, u32 key);
} Iclientset;

#endif

