#define _POSIX_C_SOURCE 200112L

/* C standard library */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* POSIX */
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

/* Linux */
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <syscall.h>

#define FATAL(...)                               \
    do {                                         \
        fprintf(stderr, "strace: " __VA_ARGS__); \
        fputc('\n', stderr);                     \
        exit(EXIT_FAILURE);                      \
    } while (0)

// using Register = unsigned long long;
// struct registers {
//     Register code;
//     Register rdi;
//     Register rsi;
//     Register rdx;
//     Register
//                 (long)regs.r10, (long)regs.r8,  (long)regs.r9);
// }

const int FD = 12322;
int my_open(const char* name, int flags) {
    fprintf(stderr, "MY OPEN: %s\n", name);
    return FD;
}
// int dfd	const char __user * filename	int flags	umode_t mode
int my_openat(int dirfd, const char* name, int flags, mode_t mode) {
    if (name == NULL)
        fprintf(stderr, "!!!\n");
    else
        fprintf(stderr, "MY OPEN AT: %d %d\n", dirfd, flags);
    return openat(dirfd, name, flags, mode);
    // return FD;
}
int my_close(int fd) {
    fprintf(stderr, "MY CLOSE: %d\n", fd);
    return 0;
}

ssize_t my_write(int fd, const void* buf, size_t count) {
    // if(fd <=2) return write(fd, buf, count);
    // char* b = (char*) malloc(count + 1);
    // strncpy(b,(const char*) buf, count);
    fprintf(stderr, "MY WRITE: %d %ld\n", fd, count);  //, b);
    return write(fd, buf, count);
    // free(b);
    // return count;
}
ssize_t my_writev(unsigned long fd, const struct iovec* iov,
                  unsigned long iovcnt) {
    fprintf(stderr, "MY WRITEV\n");
    if (fd > 2) {
        static char buf[129] = "\0";
        ssize_t c = 0;
        for (int i = 0; i != iovcnt; ++i) {
            const auto l = iov[i].iov_len;
            if (l < 129) {
                strncpy(buf, (char*)iov[i].iov_base, l);
                fprintf(stderr, "  %lu %s %lu\n", fd, buf, l);
            } else
                fprintf(stderr, "  %lu %lu\n", fd, iovcnt);
            c += iov[i].iov_len;
        }
        return 1;
    } else
        return writev(fd, iov, iovcnt);
}

ssize_t my_read(int fd, void* buf, size_t count) {
    if (fd <= 2) return read(fd, buf, count);
    char* b = (char*)malloc(count);
    for (int i = 0; i != count; ++i) b[i] = (char)i;
    fprintf(stderr, "MY READ: %d %ld\n", fd, count);
    return count;
}

void retval(user_regs_struct& regs, pid_t pid, unsigned long long ret = 0) {
    regs.rax = ret;
    // regs.rip -= 2;
    ptrace(PTRACE_SETREGS, pid, 0, &regs);
}

static char buf[0x1000];
void* my_mmap(unsigned long long addr, unsigned long len, unsigned long prot,
              unsigned long flags, unsigned long fd, unsigned long off) {
    return mmap((void*)addr, len, prot, flags, fd, off);
    fprintf(stderr,
            "MY MMAP: addr: %llu, len: %lu, prot: %lu, flags: %lu,  fd: %lu, "
            "offset: %lu\n",
            addr, len, prot, flags, fd, off);
    return buf;
}

int main(int argc, char** argv) {
    if (argc <= 1) FATAL("too few arguments: %d", argc);

    pid_t pid = fork();
    switch (pid) {
        case -1: /* error */
            FATAL("%s", strerror(errno));
        case 0: /* child */
            ptrace(PTRACE_TRACEME, 0, 0, 0);
            /* Because we're now a tracee, execvp will block until the parent
             * attaches and allows us to continue. */
            execvp(argv[1], argv + 1);
            FATAL("%s", strerror(errno));
    }

    /* parent */
    waitpid(pid, 0, 0);  // sync with execvp
    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL);
    for (;;) {
        /* Enter next system call */
        if (ptrace(PTRACE_SYSEMU_SINGLESTEP, pid, 0, 0) == -1)
            FATAL("%s", strerror(errno));
        if (waitpid(pid, 0, 0) == -1) FATAL("%s", strerror(errno));

        /* Gather system call arguments */
        struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1)
            FATAL("%s", strerror(errno));
        long code = regs.orig_rax;

        unsigned long long ret = -1;
        if (regs.orig_rax < 1000)  //&& regs.orig_rax != SYS_wait4)
            fprintf(stderr, "%llu\n", regs.orig_rax);
        /* Print a representation of the system call */
        switch (code) {
            case SYS_open:
                fprintf(stderr, "open\n");
                ret = my_open((const char*)regs.rdi, (int)regs.rsi);
                retval(regs, pid, ret);
                break;
            case SYS_openat:
                fprintf(stderr, "openat\n");
                ret = my_openat((int)regs.rdi, (const char*)regs.rsi,
                                (int)regs.rdx, (mode_t)regs.r10);
                // ptrace(PTRACE_POKEUSER, pid, 80, ret);
                // retval(regs, pid, ret);
                break;
            case SYS_openat2:
            case SYS_close:
                fprintf(stderr, "close\n");
                ret = my_close((int)regs.rdi);
                retval(regs, pid, ret);
                break;
            case SYS_read:
                fprintf(stderr, "read\n");
                ret = my_read((int)regs.rdi, (char*)regs.rsi, (size_t)regs.rdx);
                retval(regs, pid, ret);
                break;
            case SYS_write:
                fprintf(stderr, "write\n");
                ret = write((int)regs.rdi, (const char*)regs.rsi,
                            (size_t)regs.rdx);
                // retval(regs, pid, ret);
                break;
            case SYS_pwrite64:
                fprintf(stderr, "pwrite64\n");
                break;
            case SYS_writev:
                fprintf(stderr, "writev\n");
                ret = my_writev((unsigned long)regs.rdi,
                                (const struct iovec*)regs.rsi,
                                (unsigned long)regs.rdx);
                // retval(regs, pid, ret);
                break;
            case SYS_pwritev2:
                fprintf(stderr, "pwritev2\n");
                break;
            case SYS_pwritev:
                fprintf(stderr, "pwritev\n");
                break;
            case SYS_mmap: {
                void* ret =
                    my_mmap(regs.rdi, (unsigned long)regs.rsi,
                            (unsigned long)regs.rdx, (unsigned long)regs.r10,
                            (unsigned long)regs.r8, (unsigned long)regs.r9);
                retval(regs, pid, (unsigned long long)ret);
            } break;
            default:

            {
                // if(regs.orig_rax == SYS_wait4) ptrace(PTRACE_CONT);
                syscall(regs.orig_rax, regs.rsi, regs.rdi, regs.rdx, regs.r10,
                        regs.r8, regs.r9);
                // ptrace(PTRACE_CONT, pid, 0, 0);
                // if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1)
                //     FATAL("%s", strerror(errno));
                // if (waitpid(pid, 0, 0) == -1)
                //     FATAL("%s", strerror(errno));

                /* Get system call result */
                // if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
                //     fputs(" = ?\n", stderr);
                //     if (errno == ESRCH)
                //         exit(regs.rdi); // system call was _exit(2) or
                //         similar
                //     FATAL("%s", strerror(errno));
                // }
                // fprintf(stderr, "%lld = %s\n", regs.rax, strerror (errno));
            }
        }
    }
}
