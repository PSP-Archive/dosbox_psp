#include <errno.h>
#include <sys/fcntl.h>
#include <pspstdio.h>

#ifdef __cplusplus
extern "C" {
#endif
int truncate(const char *path, off_t length);
#ifdef __cplusplus
}
#endif


#define __PSP_FILENO_MAX 1024

#define __PSP_IS_FD_VALID(FD) \
		( (FD >= 0) && (FD < __PSP_FILENO_MAX) && (__psp_descriptormap[FD] != NULL) )

typedef enum {
	__PSP_DESCRIPTOR_TYPE_FILE  ,
	__PSP_DESCRIPTOR_TYPE_PIPE ,
	__PSP_DESCRIPTOR_TYPE_SOCKET,
	__PSP_DESCRIPTOR_TYPE_TTY
} __psp_fdman_fd_types;

typedef struct {
	char * filename;
	u8     type;
	u32    sce_descriptor;
	u32    flags;
	u32    ref_count;
} __psp_descriptormap_type;

extern __psp_descriptormap_type *__psp_descriptormap[__PSP_FILENO_MAX];


int ftruncate(int fd, off_t length)
{
	if (!__PSP_IS_FD_VALID(fd)) {
		errno = EBADF;
		return -1;
	}

	switch(__psp_descriptormap[fd]->type)
	{
		case __PSP_DESCRIPTOR_TYPE_FILE:
			if (__psp_descriptormap[fd]->filename != NULL) {
				if (!(__psp_descriptormap[fd]->flags & (O_WRONLY | O_RDWR)))
					break;
				return truncate(__psp_descriptormap[fd]->filename, length);
				/* ANSI sez ftruncate doesn't move the file pointer */
			}
			break;
		case __PSP_DESCRIPTOR_TYPE_TTY:
		case __PSP_DESCRIPTOR_TYPE_PIPE:
		case __PSP_DESCRIPTOR_TYPE_SOCKET:
		default:
			break;
	}

	errno = EINVAL;
	return -1;
}
