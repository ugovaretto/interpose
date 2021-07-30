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
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/resource.h>
#include <syscall.h>

#define FATAL(...)                               \
    do {                                         \
        fprintf(stderr, "strace: " __VA_ARGS__); \
        fputc('\n', stderr);                     \
        exit(EXIT_FAILURE);                      \
    } while (0)

using RegisterT = unsigned long long;

// template <RAX_T = RegisterT,
//           RDI_T = RegisterT,
//           RSI_T = RegisterT,
//           RDX_T = RegisterT,
//           R10_T = RegisterT,
//           R9_T  = RegisterT,
//           R8_T  = RegisterT>
void print_regs(const user_regs_struct& regs, FILE* f = stderr) {
    fprintf(f,
            "RAX: %d RDI: %lu, RSI: %lu"
            ", RDX: %lu, R10: %lu, R8: %lu"
            ", R9: %lu, RCX: %lu, RBX: %lu\n",
            (int)regs.orig_rax, (unsigned long)regs.rdi,
            (unsigned long)regs.rsi, (unsigned long)regs.rdx,
            (unsigned long)regs.r10, (unsigned long)regs.r8,
            (unsigned long)regs.r9, (unsigned long)regs.rcx,
            (unsigned long)regs.rbx);
}

user_regs_struct get_user_regs(pid_t pid) {
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
        fputs(" = ?\n", stderr);
        if (errno == ESRCH)
            exit(regs.rdi);  // system call was _exit(2) or similar
        FATAL("%s", strerror(errno));
    }
    return regs;
}

user_regs_struct peek_regs(pid_t pid) {
    const int WS = sizeof(void*);
    user_regs_struct regs;
    regs.rax = ptrace(PTRACE_PEEKUSER, pid, WS * RAX, nullptr);
    regs.rdi = ptrace(PTRACE_PEEKUSER, pid, WS * RDI, nullptr);
    regs.rsi = ptrace(PTRACE_PEEKUSER, pid, WS * RSI, nullptr);
    regs.rdx = ptrace(PTRACE_PEEKUSER, pid, WS * RDX, nullptr);
    regs.r10 = ptrace(PTRACE_PEEKUSER, pid, WS * R10, nullptr);
    regs.r8 = ptrace(PTRACE_PEEKUSER, pid, WS * R8, nullptr);
    regs.r9 = ptrace(PTRACE_PEEKUSER, pid, WS * R9, nullptr);
    regs.rcx = ptrace(PTRACE_PEEKUSER, pid, WS * RCX, nullptr);
    return regs;
}

static const char* FNAME = "in";
static int fake_fd = -1;

void sys_call(pid_t pid) {
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) FATAL("%s\n", strerror(errno));
    if (waitpid(pid, 0, 0) == -1) FATAL("%s\n", strerror(errno));
}

void read_string(pid_t pid, unsigned long addr, char* out) {
    const unsigned long begin = (unsigned long)out;
    unsigned long buf = (unsigned long)out;
    while ((buf - begin) < 1024) {
        struct bytes {
            char b[9];
        };
        const unsigned long w = ptrace(PTRACE_PEEKDATA, pid, addr, NULL);
        if (errno) fprintf(stderr, "%s\n", strerror(errno));
        bytes b;
        memset((void*)&b, 0, sizeof(b));
        memmove(&b, &w, sizeof(w));
        const char* n = (const char*)memchr(b.b, 0, sizeof(w));
        int chars = sizeof(w);
        if (n) {
            int offset = (int)(n - (const char*)b.b);
            b.b[offset] = '\0';
            chars = offset + 1;
        }
        memmove((void*)buf, (void*)b.b, chars);
        if (n) break;
        buf += sizeof(w);
        addr += sizeof(w);
    }
}

void writedata(pid_t child, unsigned long addr, const char* str, int len) {
    const int size = sizeof(addr);
    const int step = size;
    union {
        unsigned long val;
        char chars[size];
    } data;
    const int steps = len / size;
    const char* laddr = str;
    for (int i = 0; i != steps; ++i, addr += step, laddr += step) {
        memcpy(data.chars, laddr, size);
        ptrace(PTRACE_POKEDATA, child, addr, data.val);
    }
    if (auto rem = len % size) {
        memcpy(data.chars, laddr, rem);
        ptrace(PTRACE_POKEDATA, child, addr, data.val);
    }
}

void my_openat(pid_t pid) {
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
        fputs(" = ?\n", stderr);
        if (errno == ESRCH)
            exit(regs.rdi);  // system call was _exit(2) or similar
        FATAL("%s", strerror(errno));
    }
    unsigned long fname = regs.rsi;
    static char buffer[1024] = {0};
    fprintf(stderr, "\n>>>>>>>>>>>>>>>>>>>>>>>\n");
    read_string(pid, fname, buffer);
    fprintf(stderr, "%s", buffer);
    fprintf(stderr, "\n>>>>>>>>>>>>>>>>>>>>>>>\n");
    if (strcmp(buffer, FNAME) == 0) {
        regs.rax = fake_fd;
        ptrace(PTRACE_SETREGS, pid, 0, &regs);
    } else
        sys_call(pid);
}

void my_close(pid_t pid) {
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
        fputs(" = ?\n", stderr);
        if (errno == ESRCH)
            exit(regs.rdi);  // system call was _exit(2) or similar
        FATAL("%s", strerror(errno));
    }
    const int fd = (int)regs.rdi;
    if (fd == fake_fd) {
        regs.rax = 0;
        ptrace(PTRACE_SETREGS, pid, 0, &regs);
    } else
        sys_call(pid);
}

void my_read(pid_t pid) {
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
        fputs(" = ?\n", stderr);
        if (errno == ESRCH)
            exit(regs.rdi);  // system call was _exit(2) or similar
        FATAL("%s", strerror(errno));
    }
    const int fd = (int)regs.rdi;
    if (fd == fake_fd) {
        const char* new_text = "replaced!!";
        writedata(pid, regs.rsi, new_text, strlen(new_text));
        regs.rax = 100;  // strlen(new_text);
        ptrace(PTRACE_SETREGS, pid, 0, &regs);
    } else
        sys_call(pid);
}

int main(int argc, char** argv) {
    if (argc <= 1) FATAL("too few arguments: %d\n", argc);

    rlimit limit;
    getrlimit(RLIMIT_NOFILE, &limit);
    fake_fd = limit.rlim_max;
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
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1)
            FATAL("%s\n", strerror(errno));
        if (waitpid(pid, 0, 0) == -1) FATAL("%s\n", strerror(errno));

        /* Gather system call arguments */
        struct user_regs_struct regs;
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1)
            FATAL("%s", strerror(errno));
        long syscall_code = regs.orig_rax;
        if (syscall_code == 1) fprintf(stderr, "WRITEEE\n");
        /* Print a representation of the system call */
        fprintf(stderr, "BEFORE> %ld(%ld, %ld, %ld, %ld, %ld, %ld)\n",
                (long)regs.orig_rax, (long)regs.rdi, (long)regs.rsi,
                (long)regs.rdx, (long)regs.r10, (long)regs.r8, (long)regs.r9);
        if (syscall_code == 231) break;
        /* Run system call and stop on exit */
        switch (syscall_code) {
            case 0:
                my_read(pid);
                break;
            case 257:
                my_openat(pid);
                break;
            case 3:
                my_close(pid);
                break;
            default:
                sys_call(pid);
                break;
        }
        /* Get system call result */
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
            fputs(" = ?\n", stderr);
            if (errno == ESRCH)
                exit(regs.rdi);  // system call was _exit(2) or similar
            FATAL("%s", strerror(errno));
        }
        /* Print a representation of the system call */
        fprintf(stderr, "AFTER> %ld(%ld, %ld, %ld, %ld, %ld, %ld)\n",
                (long)regs.rax, (long)regs.rdi, (long)regs.rsi, (long)regs.rdx,
                (long)regs.r10, (long)regs.r8, (long)regs.r9);
        /* Print system call result */
        fprintf(stderr, " = %s\n", strerror(errno));
    }
}
