
/* dist: public */

#ifndef __PATHUTIL_H
#define __PATHUTIL_H

/* utility functions for working with search paths and things */


/* macro_expand_string: expands a string containing two-character macro
 * sequences into a destination buffer, using a provided table of
 * replacements. the macro character can be specified. double it to
 * insert it into the output by itself. returns the number of characters
 * in the destionation string, or -1 on error. */

struct replace_table
{
	int repl;
	const char *with;
};

int macro_expand_string(
		char *dest,
		int destlen,
		char *source,
		struct replace_table *repls,
		int replslen,
		char macrochar);

/* find_file_on_path: finds the first of a set of pattern-generated
 * filenames that exist. macrochar is assumed to be '%'. */

int find_file_on_path(
		char *dest,
		int destlen,
		const char *searchpath,
		struct replace_table *repls,
		int replslen);

/* checks if the given path is valid and secure against trying to access
 * files outside of the server root. */
int is_valid_path(const char *path);

#endif

