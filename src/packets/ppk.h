
#ifndef __PACKETS_PPK_H
#define __PACKETS_PPK_H

/* ppk.h - position and weapons packets */

struct Weapons
{
	/* this is a bit field. the whole thing should fit into 16 bits */
	unsigned short type : 5;
	unsigned short level : 2;
	unsigned short bouncing : 1;
	unsigned short shraplevel : 2;
	unsigned short shrap : 5;
	unsigned short alternate : 1;
};

struct ExtraPosData
{
	unsigned energy : 16;
	unsigned s2cping : 16;
	unsigned timer : 16;
	unsigned shields : 1;
	unsigned super : 1;
	unsigned bursts : 4;
	unsigned repels : 4;
	unsigned thors : 4;
	unsigned bricks : 4;
	unsigned decoys : 4;
	unsigned rockets : 4;
	unsigned portals : 4;
	unsigned padding : 2;
};

struct S2CWeapons
{
	u8 type; /* 0x05 */
	i8 rotation;
	u16 time;
	i16 x;
	i16 yspeed;
	u16 playerid;
	i16 xspeed;
	u8 checksum;
	u8 status;
	u8 ping;
	i16 y;
	u16 bounty;
	struct Weapons weapon;
	struct ExtraPosData extra;
};

struct S2CPosition
{
	u8 type; /* 0x28 */
	i8 rotation;
	u16 time;
	i16 x;
	u8 ping;
	u8 bounty;
	u8 playerid;
	i8 status;
	i16 yspeed;
	i16 y;
	i16 xspeed;
	struct ExtraPosData extra;
};

struct C2SPosition
{
	u8 type; /* 0x03 */
	i8 rotation;
	u32 time;
	i16 xspeed;
	i16 y;
	i8 checksum;
	i8 status;
	i16 x;
	i16 yspeed;
	u16 bounty;
	i16 energy;
	struct Weapons weapon;
	struct ExtraPosData extra;
};


#endif

