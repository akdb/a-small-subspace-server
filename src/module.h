
#ifndef __MODMAN_H
#define __MODMAN_H


/*
 * Imodman - the module and interface manager
 *
 * LoadModule and UnloadModule are self-explanatory. you probably won't
 * have to use them. note that the arugment to LoadModule is in the form
 * "libraryname:modulename". libraryname can be "int" or "internal" for
 * modules located in the main binary.
 *
 * GetInterface is used to get access to another module's interface. you
 * obviously have to cast the return value to the correct type.
 *
 * (Un)RegisterInterface are used to make interfaces available to other
 * modules. don't forget to make sure you have a unique id.
 *
 * FindPlayer is just a damn useful function. i'm not sure what it's
 * doing here, though.
 *
 * players is the global player array. you probably won't get much done
 * without this.
 *
 * desc is used for modules to give descriptions of themselves. unused
 * for now.
 *
 * InitModuleManager is called only once by main to initalize the module
 * manager.
 * 
 */


#define MAXNAME 32



typedef struct Imodman
{
	int (*LoadModule)(char *specifier);
	void (*UnloadModule)(char *name);
	void (*UnloadAllModules)();

	void * (*GetInterface)(int id);
	void (*RegisterInterface)(int id, void *iface);
	void (*UnregisterInterface)(void *iface);

	int (*FindPlayer)(char *name);

	PlayerData *players;
	char *desc;
} Imodman;


Imodman * InitModuleManager();


#endif

