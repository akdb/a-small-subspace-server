
#ifndef __FILETRANS_H
#define __FILETRANS_H


#define CB_UPLOADEDFILE ("uploadedfile")
typedef void (*UploadedFileFunc)(int pid, const char *filename);


#define I_FILETRANS "filetrans"

typedef struct Ifiletrans
{
	INTERFACE_HEAD_DECL

	int (*SendFile)(int pid, const char *path, const char *fname);
	void (*RequestFile)(int pid, const char *path, const char *fname);
} Ifiletrans;


#endif
