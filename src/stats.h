
#ifndef __STATS_H
#define __STATS_H

/* Istats - the statistics/scores manager
 *
 * This module has functions for managing simple scores and statistics.
 *
 */

typedef enum stat_t
{
	STAT_KPOINTS = 0,
	STAT_FPOINTS = 1,
	STAT_KILLS   = 2,
	STAT_DEATHS  = 3,
	STAT_MAX     = 15
} stat_t;

typedef struct Istats
{
	/* increments a particular statistic */
	void (*IncrementStat)(int pid, stat_t stat, int amount);

	/* sends out score updates for a particular arena (or -1 for all) */
	void (*SendUpdates)(int arena);
} Istats;

#endif
