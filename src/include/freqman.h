
/* dist: public */

#ifndef __FREQMAN_H
#define __FREQMAN_H

/** the interface id for Ibalancer */
#define I_BALANCER "balancer-2"

/** The interface struct for Ibalancer.
 * This struct is designed to be unique per arena (unlike enforcer).
 * The balancer defines how teams should be balanced, but leaves the
 * actual balancing code in the freqman.
 */
typedef struct Ibalancer
{
	INTERFACE_HEAD_DECL
	/* pyint: use, impl */

	/**
	 * Return an integer representing the player's balance metric.
	 * Freqman will use this to ensure the teams are balanced.
	 */
	int (*GetPlayerMetric)(Player *p);
	/* pyint: player -> int */

	/**
	 * TODO
	 */
	int (*GetMaxMetric)(Arena *arena, int freq);
	/* pyint: arena, int -> int */

	/**
	 * This must yield the same value when freq1 and freq2 are interchanged.
	 * TODO
	 */
	int (*GetMaximumDifference)(Arena *arena, int freq1, int freq2);
	/* pyint: arena, int, int -> int */

} Ibalancer;

/** the adviser id for Aenforcer */
#define A_ENFORCER "enforcer-1"

/** The adviser struct for the enforcers.
 * These are designed to be implemented by non-core modules and
 * registered on a per-arena basis.
 */
typedef struct Aenforcer
{
	ADVISER_HEAD_DECL
	/* pyadv: use */

	/**
	 * Return the allowable ships (1 = Warbird, 128 = Shark, 255 = all ships).
	 * Return 0 to allow only spectator.
	 * Fill err_buf only if the returned mask doesn't allow them to change
	 * to the desired ship
	 * NOTE: ship shouldn't factor into the returned mask.
	 * Only write to err_buf if it's non-null
	 */
	shipmask_t (*GetAllowableShips)(Player *p, int ship, int freq, char *err_buf, int buf_len);
	/* pyadv: player, int, int, string inout, int buflen -> int */
	// python users: this will return a (int, string) tuple

	/**
	 * Returns a boolean indicating whether the player can switch to
	 * new_freq.
	 * Only write to err_buf it it's non-null
	 */
	int (*CanChangeFreq)(Player *p, int new_freq, char *err_buf, int buf_len);
	/* pyadv: player, int, string inout, int buflen -> int */
	// python users: this will return a (int, string) tuple

} Aenforcer;

/** the interface id for Ifreqman */
#define I_FREQMAN "freqman-2"

/** The interface struct for Ifreqman.
 * This interface is the second revision of Ifreqman. This implementation
 * is designed to use the above Ienforcer and Ibalancer. The actual freqman
 * module is implemented as a core module and shouldn't need to be replaced
 * for most tasks. Instead it uses the enforcers and balancers to implement
 * non-core functionality.
 */
typedef struct Ifreqman
{
	INTERFACE_HEAD_DECL
	/* pyint: use, impl */

	/** called when a player connects and needs to be assigned to a freq.
	 */
	void (*Initial)(Player *p, int *ship, int *freq);
	/* pyint: player, int inout, int inout -> void */
	// python users: this will return a (int, int) tuple

	/** called when a player requests a ship change.
	 * ship will initially contain the ship request, and freq will
	 * contain the player's current freq. */
	void (*ShipChange)(Player *p, int requested_ship, char *err_buf, int buf_len);
	/* pyint: player, int, string inout, int buflen -> void */
	// python users: this will return a string

	/** called when a player requests a freq change.
	 * ship will initially contain the player's ship, and freq will
	 * contain the requested freq. */
	void (*FreqChange)(Player *p, int requested_freq, char *err_buf, int buf_len);
	/* pyint: player, int, string inout, int buflen -> void */
	// python users: this will return a string

} Ifreqman;

#endif

