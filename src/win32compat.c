
#include <stdio.h>
#include <string.h>

#include "win32compat.h"


/* taken from
 * http://www2.ics.hawaii.edu/~esb/2001fall.ics451/strcasestr.html */
const char * strcasestr(const char* haystack, const char* needle)
{
	int i;
	int nlength = strlen (needle);
	int hlength = strlen (haystack);

	if (nlength > hlength) return NULL;
	if (hlength <= 0) return NULL;
	if (nlength <= 0) return haystack;
	/* hlength and nlength > 0, nlength <= hlength */
	for (i = 0; i <= (hlength - nlength); i++)
		if (strncasecmp (haystack + i, needle, nlength) == 0)
			return haystack + i;
	/* substring not found */
	return NULL;
}


int mkstemp(char *template)
{
	if (mktemp(template) == NULL)
		return -1;
	else
		return open(template);
}


struct DIR
{
	struct _finddata_t fi;
	long handle;
	long lastres;
	struct dirent de;
};

DIR *opendir(const char *reqpath)
{
	char path[PATH_MAX];
	DIR *dir = amalloc(sizeof(*dir));
	snprintf(path, sizeof(path), "%s/*", path);
	dir->fh = _findfirst(path, &dir->fi);
	dir->lastres = dir->fh;
	return dir;
}

struct dirent *readdir(DIR *dir)
{
	if (dir->lastres != -1)
	{
		astrncpy(de->d_name, dir->fi.name, sizeof(de->d_name));
		dir->lastres = _findnext(dir->fh, &dir->fi);
		return &dir->de;
	}
	else
		return NULL;
}

void closedir(DIR *dir)
{
	if (dir->fh != -1)
		_findclose(dir->fh);
	afree(dir);
}


