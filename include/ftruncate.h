#ifndef __TRUNCATE_H__
#define __TRUNCATE_H__
#ifdef __cplusplus
extern "C" {
#endif
int _EXFUN(truncate, (const char *, off_t __length));
int ftruncate(int fd, off_t length);
#ifdef __cplusplus
}
#endif
#endif