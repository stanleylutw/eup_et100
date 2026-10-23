/* Emulator-only heap/syscalls: never linked into the EVB firmware. */
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

static unsigned char heap[65536] __attribute__((aligned(8)));
static ptrdiff_t used;
void *_sbrk(ptrdiff_t size)
{
    if (size < -used || size > (ptrdiff_t)sizeof heap-used) { errno=ENOMEM;return (void *)-1; }
    void *p=heap+used;used+=size;return p;
}
int _write(int fd,const void *data,size_t length) { (void)fd;(void)data;return (int)length; }
int _read(int fd,void *data,size_t length) { (void)fd;(void)data;(void)length;return 0; }
int _close(int fd) { (void)fd;return -1; }
int _fstat(int fd,struct stat *st) { (void)fd;st->st_mode=S_IFCHR;return 0; }
int _isatty(int fd) { (void)fd;return 1; }
int _lseek(int fd,int offset,int whence) { (void)fd;(void)offset;(void)whence;return 0; }
int _getpid(void) { return 1; }
int _kill(int pid,int signal) { (void)pid;(void)signal;errno=EINVAL;return -1; }
