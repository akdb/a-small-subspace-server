
#ifndef __PACKETS_BANNERS_H
#define __PACKETS_BANNERS_H

/* a single banner */
typedef struct Banner
{
	u8 data[96];
} Banner;


/* updating another player's banner */
struct S2CBanner
{
	u8 type; /* 0x1F */
	i16 pid;
	Banner banner;
};


/* not exactly sure yet */
struct S2CBannerToggle
{
	u8 type; /* 0x1E */
	u8 toggle;
};


/* setting a player's banner */
struct C2SBanner
{
	u8 type; /* 0x19 */
	Banner banner;
};

/* telling billing the player's new banner */
struct S2BBanner
{
	u8 type; /* 0x10 */
	i32 pid;
	Banner banner;
};


#endif