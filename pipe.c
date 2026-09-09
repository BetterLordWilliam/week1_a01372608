// Week 1 Lab template: pipe.c

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct pipe_result {
    unsigned long iterations;
    unsigned long sum;
};

enum { WORK_ITERATIONS = 20 };

static volatile sig_atomic_t parent_result_ready = 0;

static int todo(const char *step)
{
    fprintf(stderr, "TODO: %s\n", step);
    errno = ENOSYS;
    return -1;
}

static void sleep_for_work_pacing(void)
{
    struct timespec remaining = {
        .tv_sec = 0,
        .tv_nsec = 100 * 1000 * 1000
    };

    while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR) {
        continue;
    }
}

static void parent_handle_ready(int signal_number)
{
    // TODO 1: Set parent_result_ready. Do not call printf(), read(), or close().
	parent_result_ready = 1;
}

static int install_handler(int signal_number, void (*handler)(int), int restart)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = restart ? SA_RESTART : 0;

    return sigaction(signal_number, &sa, NULL);
}

static int write_all(int fd, const void *buffer, size_t length)
{
    // DONE ~~TODO~~ 2: Loop until write() accepts every byte. Retry when errno == EINTR.
    const unsigned char *bytes = buffer;
    size_t off = 0;
    while  ( off < length ) {
	ssize_t n = write(fd, bytes + off, length - off); // signed because results may be negative
	if ( n > 0 ) {
		off += (size_t)n; // casting to unsigned
	} else if ( n == -1 && errno == EINTR ) {
		continue;
	} else {
		return -1;
	}
    }
    return 0;
}

static int read_exact(int fd, void *buffer, size_t length)
{
    // DONE ~~TODO~~ 3: Loop until read() receives exactly length bytes.
    // Treat early EOF as an error and retry when errno == EINTR.
    unsigned char *bytes = buffer;
    size_t off = 0;
    while ( off < len ) {
	    ssize_t n = read(fd, bytes + off, len - off); // signed because results may be negative
	    if ( n > 0 ) {
		    off += (size_t)n;	// casting to unsigned
	    } else if ( n == 0 ) {	// 0 now meaninf EOF
		    return -1;
	    } else {
		    return -1;
	    }
    }
    return 0;
}

static void child_work(int result_write_fd)
{
    struct pipe_result result = {0, 0};

    for (unsigned long iteration = 1; iteration <= WORK_ITERATIONS; iteration++) {
        // TODO 4: Update result.iterations and add iteration to result.sum.
	result.iterations = iteration;
	result.sum += iteration;
        sleep_for_work_pacing();
    }

    // TODO 5:
    // Publish the completed result to the parent, then notify the parent.
    // After the notification, release the pipe descriptor and exit normally.
    // 
    // Requirements:
    // - The complete result must enter the pipe before SIGUSR1 is sent.
    // - Check every system-call/helper result.
    // - Use a nonzero child exit status if an operation fails.
    // - Close the child's write end before exiting.
    
    write_all(result_write_fd, &result, sizeof(result));
    
    if (close(result_write_fd) < 0) {
	    perror("[Child] ");
            _exit(999);
    }

    if (kill(getppid(), SIGUSR1) < 0) {
	    perror("[Child] child process termination");
	    _exit(999);
    }

    _exit(0);
}

static void report_child_status(int status)
{
    if (WIFEXITED(status)) {
        printf("[Parent] Child exited normally with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("[Parent] Child was terminated by signal %d\n", WTERMSIG(status));
    } else {
        printf("[Parent] Child ended in an unexpected state: status=%d\n", status);
    }
}

int main(void)
{
    int result_pipe[2];
    if (pipe(result_pipe) == -1) {
        perror("[Parent] pipe");
        return 1;
    }

    // Provided setup: no SA_RESTART, so SIGUSR1 can interrupt waitpid().
    if (install_handler(SIGUSR1, parent_handle_ready, 0) == -1) {
        perror("[Parent] sigaction(SIGUSR1)");
        close(result_pipe[0]);
        close(result_pipe[1]);
        return 2;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        perror("[Parent] fork");
        close(result_pipe[0]);
        close(result_pipe[1]);
        return 3;
    }

    if (pid == 0) {
        // DONE ~~TODO~~ 6:
        // The child writes the result. It does not read from this pipe.
        // Close the unused read end and check close(). Keep the write end open.
	if (close(result_pipe[0]) == -1) {
		perror("[Child] failed to close the pipe read file descriptor");
		_exit(999);
	}
        child_work(result_pipe[1]);
        _exit(127);
    }

    // DONE ~~TODO~~ 7:
    // The parent reads the child's result. It does not write to this pipe.
    // Close the unused write end and check close(). Keep the read end open.
    if (close(result_pipe[1]) == -1) {
	    perror("[Parent] failed to close the pipe write file descriptor");
	    return 4;
    }

    printf("[Parent] Monitoring child PID %ld\n", (long)pid);

    struct pipe_result result = {0, 0};
    int result_received = 0;
    int notification_reported = 0;
    int observed_child_exit = 0;
    int status = 0;

    for (;;) {
        errno = 0;
        const pid_t waited = waitpid(pid, &status, 0);

        if (waited == pid) {
            observed_child_exit = 1;
            break;
        }

        if (waited == -1 && errno == EINTR) {
            // TODO 8: When parent_result_ready is set:
            // - read_exact() one struct pipe_result from result_pipe[0]
            // - set result_received and notification_reported
            // - clear parent_result_ready
            // - print the SIGUSR1/EINTR checkpoint
            continue;
        }

        if (waited == -1) {
            perror("[Parent] waitpid");
            break;
        }
    }

    // TODO 9:
    // The child may signal and exit before the parent processes the notification.
    // Then waitpid() can return the child PID, so the TODO 8 EINTR branch is skipped.
    // After the loop, handle any unread result and unreported notification once.

    if (observed_child_exit) {
        report_child_status(status);
    }

    // The Parent owns only the read end here. The write end was closed above.
    if (close(result_pipe[0]) == -1) {
        perror("[Parent] close read end");
        return 6;
    }

    const int status_ok = observed_child_exit &&
                          WIFEXITED(status) &&
                          WEXITSTATUS(status) == 0;
    const int result_ok = result_received &&
                          result.iterations == WORK_ITERATIONS &&
                          result.sum == 210;
    const int ok = status_ok && result_ok && notification_reported;

    printf("[Parent] %s: iterations=%lu sum=%lu signal=SIGUSR1\n",
           ok ? "PASS" : "FAIL",
           result.iterations,
           result.sum);

    return ok ? 0 : 7;
}
