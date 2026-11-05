#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ps5platform/libc.h>
#include <errno.h>
/* Native titles deny lstat. Opening without following links preserves the
 * store's regular-file checks using the native descriptor API. */
int helix_native_fstatat(int directory,const char *name,struct stat *out,int flags) {
 if(flags & ~AT_SYMLINK_NOFOLLOW){errno=EINVAL;return -1;}
 int fd=ps5_openat(directory,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
 if(fd<0)return -1;
 int result=fstat(fd,out),saved=errno;
 close(fd);errno=saved;return result;
}
/* Model and hook-file guards use the same descriptor-based no-follow check.
 * Native builds remap lstat to this function; host tests use POSIX lstat. */
int helix_native_lstat(const char *name,struct stat *out) {
 return helix_native_fstatat(AT_FDCWD,name,out,AT_SYMLINK_NOFOLLOW);
}
