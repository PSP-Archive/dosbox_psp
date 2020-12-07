#include <pspiofilemgr.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

extern int __psp_cwd_initialized;
extern void __psp_init_cwd(void);

static time_t psp_to_epoch_time(ScePspDateTime psp_time)
{
  struct tm conv_time;
  conv_time.tm_year = psp_time.year;
  conv_time.tm_mon = psp_time.month;
  conv_time.tm_mday = psp_time.day;
  conv_time.tm_hour = psp_time.hour;
  conv_time.tm_min = psp_time.minute;
  conv_time.tm_sec = psp_time.second;
  conv_time.tm_isdst = -1;
  return mktime(&conv_time);
}

int _stat(const char *filename, struct stat *buf)
{
  SceIoStat psp_stat;
  int ret;

  /* Make sure the CWD has been set. */
  if (!__psp_cwd_initialized)
  {
    __psp_init_cwd();
  }

  memset(buf, '\0', sizeof(struct stat));
  if ((ret = sceIoGetstat(filename, &psp_stat)) < 0)
    return ret;

  buf->st_ctime = psp_to_epoch_time(psp_stat.st_ctime);
  buf->st_atime = psp_to_epoch_time(psp_stat.st_atime);
  buf->st_mtime = psp_to_epoch_time(psp_stat.st_mtime);

  buf->st_mode = (psp_stat.st_mode & 0xfff) |
                 ((FIO_S_ISLNK(psp_stat.st_mode)) ? (S_IFLNK) : (0)) |
                 ((FIO_S_ISREG(psp_stat.st_mode)) ? (S_IFREG) : (0)) |
                 ((FIO_S_ISDIR(psp_stat.st_mode)) ? (S_IFDIR) : (0));
  buf->st_size = psp_stat.st_size;
  return 0;
}

/* from stat.h in ps2sdk, this function may be correct */
#define FIO_CST_SIZE 0x0004

int truncate(const char *filename, off_t length)
{
  SceIoStat psp_stat;

  /* Make sure the CWD has been set. */
  if (!__psp_cwd_initialized)
  {
    __psp_init_cwd();
  }

  psp_stat.st_size = length;
  if (length < 0)
  {
    errno = EINVAL;
    return -1;
  }
  return sceIoChstat(filename, &psp_stat, FIO_CST_SIZE);
}

int access(const char *pathname, int mode)
{
  return 0;
}

int mkdir(const char *pathname, mode_t mode)
{
  /* Make sure the CWD has been set. */
  if (!__psp_cwd_initialized)
  {
    __psp_init_cwd();
  }

  return sceIoMkdir(pathname, mode);
}

int rmdir(const char *pathname)
{
  /* Make sure the CWD has been set. */
  if (!__psp_cwd_initialized)
  {
    __psp_init_cwd();
  }

  return sceIoRmdir(pathname);
}