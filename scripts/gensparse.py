#!/usr/bin/env python

# gensparse.py
# generates C code for efficient sparse array manipulation

import sys

# PARAMETERS (change these) --------------------------------------------

if len(sys.argv) > 2:
	# try to read parameters from a parameter file
	execfile(sys.argv[2])
else:
	# the type of the data to be represented
	sourcetype = 'unsigned char'

	# the type of the sparse array to be created
	targettype = 'sparse_arr'

	# the numbers of bits at each level of indirection
	# the numbers should be in order from largest to smallest chunk size
	# that is, the last number is the size (in bits) of the chunks that hold
	# the source types directly.
	bits = [5,2,3]

	# the default (common) value
	default = '0'

	# the function to use for allocating memory
	malloc = 'MALLOC'

	# the function to use to free memory
	free = 'FREE'

	# whether to declare all functions as static
	static = 'static'

	# whether to inline lookup
	inline_lookup = 'inline'

	# whether to inline insert
	inline_insert = 'inline'


# CODE (don't touch below here) ----------------------------------------

# this array is easier to work with in reverse order
bits.reverse()

def maxcoord(idx):
	return 2**bits[idx]


def lsum(l):
	t = 0
	for i in l:
		t = t + i
	return t


def gen_name(idx):
	if idx == 0:
		return sourcetype
	else:
		return 'sparse_chunk_%d_t' % idx


def emit_types(o):
	"typedefs"

	def gen_array_typedef(dat, ptr, name, size):
		if ptr:
			return 'typedef %s *%s[%d][%d];\n' % (dat, name, size, size)
		else:
			return 'typedef %s %s[%d][%d];\n' % (dat, name, size, size)

	for idx in range(len(bits)):
		o.write(
			gen_array_typedef(gen_name(idx),
			                  idx > 0,
			                  gen_name(idx+1),
			                  2**bits[idx]))
	o.write('typedef %s *%s;\n' % (gen_name(len(bits)), targettype))


def emit_init(o):
	"allocate memory/initialize"

	o.write("""
%(static)s %(target)s init_sparse(void)
{
	int x, y;
	%(target)s c;

	c = %(malloc)s(sizeof(%(type)s));
	if (c == NULL)
		return NULL;

	for (x = 0; x < %(max)d; x++)
		for (y = 0; y < %(max)d; y++)
			(*c)[x][y] = NULL;
	return c;
}
""" %
	{
		'target': targettype,
		'type': gen_name(len(bits)),
		'malloc': malloc,
		'max': maxcoord(-1),
		'static': static
	})


def emit_delete(o):
	"release memory"

	def gen_loop(body, idx):
		dict = \
		{
			'max': maxcoord(idx),
			'body': body,
			'type': gen_name(idx),
			'c0': 'c_%d' % idx,
			'c1': 'c_%d' % (idx + 1),
			'free': free
		}

		doloop = """\
	int x, y;
	for (x = 0; x < %(max)d; x++)
		for (y = 0; y < %(max)d; y++)
		{
			%(type)s *%(c0)s = (*%(c1)s)[x][y];
			if (%(c0)s)
			{
%(body)s
			}
		}"""

		dofree = "\n\t%(free)s(%(c1)s);"

		if idx == 0:
			return dofree % dict
		else:
			return (doloop + dofree) % dict

	o.write("""
%(static)s void delete_sparse(%(target)s %(c1)s)
{
%(body)s
}
""" % \
	{
		'target': targettype,
		'c1': 'c_%d' % len(bits),
		'body': reduce(gen_loop, range(len(bits)), ''),
		'static': static
	})


def gen_val(idx):
	return "(*%(n)s)[(x>>%(shift)d)&%(and)d][(y>>%(shift)d)&%(and)d]" % \
	{
		'n': 'c_%d' % (idx+1),
		'shift': lsum(bits[:idx]),
		'and': maxcoord(idx) - 1
	}


def emit_lookup(o):
	"lookup values"

	def gen_loop(body, idx):
		dict = \
			{
				'type': gen_name(idx),
				'c0': 'c_%d' % idx,
				'body': body,
				'def': default,
				'val': gen_val(idx)
			}
		if idx == 0:
			return "\treturn %(val)s;" % dict
		else:
			return """\
	%(type)s *%(c0)s = %(val)s;
	if (%(c0)s)
	{
		%(body)s
	}
	else
		return %(def)s;	
""" % dict

	o.write("""
%(static)s %(inl)s %(source)s lookup_sparse(%(target)s %(c1)s, int x, int y)
{
%(body)s
}
""" % \
	{
		'source': sourcetype,
		'target': targettype,
		'c1': 'c_%d' % len(bits),
		'body': reduce(gen_loop, range(len(bits)), ''),
		'static': static,
		'inl': inline_lookup
	})


def emit_insert(o):
	"inserting values"

	def gen_loop(body, idx):
		dict = \
			{
				'type': gen_name(idx),
				'c0': 'c_%d' % idx,
				'max': maxcoord(idx-1),
				'val': gen_val(idx),
				'body': body,
				'malloc': malloc
			}

		if idx == 0:
			return "\t%s = datum;" % gen_val(0)
		if idx == 1:
			dict['default'] = default
		else:
			dict['default'] = 'NULL'

		return """\
	%(c0)s = %(val)s;
	if (%(c0)s == NULL)
	{
		/* allocate and initialize */
		int i, j;
		%(c0)s = %(malloc)s(sizeof(%(type)s));
		for (i = 0; i < %(max)d; i++)
			for (j = 0; j < %(max)d; j++)
				(*%(c0)s)[i][j] = %(default)s;

		/* place back in higher level */
		%(val)s = %(c0)s;
	}

	/* next level */
%(body)s\
""" % dict

	decls = ''
	for i in range(len(bits)-1):
		decls = decls + ("\t%s *c_%d;\n" % (gen_name(i+1), i+1))

	o.write("""\
%(static)s %(inl)s void insert_sparse(%(target)s %(c1)s, int x, int y, %(source)s datum)
{
	/* variable declarations*/
%(decls)s
	/* body */
%(body)s
}
""" % \
	{
		'decls': decls,
		'source': sourcetype,
		'target': targettype,
		'c1': 'c_%d' % len(bits),
		'body': reduce(gen_loop, range(len(bits)), ''),
		'static': static,
		'inl': inline_insert
	})


def emit_all(o):
	for f in [emit_types, emit_init, emit_delete, emit_lookup, emit_insert]:
		o.write('\n/* section: %s */\n' % f.__doc__)
		f(o)
	o.write('\n/* done */\n')

if len(sys.argv) > 1:
	emit_all(open(sys.argv[1],'w'))
else:
	emit_all(sys.stdout)
