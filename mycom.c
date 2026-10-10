/*
 * mycom - UART ECHO application.
 *
 * Reads lines from a serial port, answers with the same line with swapped
 * letter case and logs every received line with a timestamp.
 *
 * Usage: ./mycom <device> [baudrate]     (default 115200 8N1)
 */

#define _DEFAULT_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_BAUD 115200
#define LINE_MAX_LEN 512

static volatile sig_atomic_t g_stop;
static FILE *g_log;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void format_time(char *buf, size_t size, const char *fmt)
{
    time_t now = time(NULL);
    struct tm tm_now;

    localtime_r(&now, &tm_now);
    strftime(buf, size, fmt, &tm_now);
}

static void log_line(const char *text)
{
    char ts[32];

    if (g_log == NULL) {
        return;
    }
    format_time(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S");
    fprintf(g_log, "[%s] %s\n", ts, text);
    fflush(g_log);
}

static speed_t baud_to_speed(int baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    default:
        return B0;
    }
}

static int setup_port(int fd, speed_t speed, struct termios *saved)
{
    struct termios tio;

    if (tcgetattr(fd, saved) != 0) {
        return -1;
    }
    tio = *saved;
    cfmakeraw(&tio); /* raw mode, 8 data bits, no parity */
    tio.c_cflag &= ~(CSTOPB | CRTSCTS); /* 1 stop bit, no flow control */
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        return -1;
    }
    tcflush(fd, TCIOFLUSH);
    return 0;
}

static void swap_case(char *s)
{
    for (; *s != '\0'; s++) {
        if (isupper((unsigned char)*s)) {
            *s = (char)tolower((unsigned char)*s);
        } else if (islower((unsigned char)*s)) {
            *s = (char)toupper((unsigned char)*s);
        }
    }
}

/* Writes the whole buffer, retrying on partial writes. */
static int write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);

        if (n < 0) {
            if (errno == EINTR && !g_stop) {
                continue;
            }
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void handle_line(int fd, char *line)
{
    char reply[LINE_MAX_LEN + 3];

    log_line(line);
    swap_case(line);
    snprintf(reply, sizeof(reply), "%s\r\n", line);
    if (write_all(fd, reply, strlen(reply)) != 0) {
        fprintf(stderr, "write: %s\n", strerror(errno));
        log_line("ERROR: write to port failed");
    }
}

static int run(int fd)
{
    char line[LINE_MAX_LEN + 1];
    size_t len = 0;
    int skip_lf = 0;

    while (!g_stop) {
        char c;
        ssize_t n = read(fd, &c, 1);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "read: %s\n", strerror(errno));
            log_line("ERROR: read from port failed");
            return -1;
        }
        if (n == 0) {
            continue;
        }
        if (c == '\n' && skip_lf) {
            skip_lf = 0;
            continue;
        }
        skip_lf = 0;
        if (c == '\r' || c == '\n') {
            skip_lf = (c == '\r');
            line[len] = '\0';
            handle_line(fd, line);
            len = 0;
        } else if (len < LINE_MAX_LEN) {
            line[len++] = c;
        } else {
            /* line too long: flush what we have */
            line[len] = '\0';
            handle_line(fd, line);
            len = 0;
            line[len++] = c;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char log_name[64];
    char msg[256];
    struct sigaction sa;
    struct termios saved;
    speed_t speed;
    int baud = DEFAULT_BAUD;
    int fd;
    int ret = EXIT_SUCCESS;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <device> [baudrate]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 3) {
        baud = atoi(argv[2]);
    }

    format_time(log_name, sizeof(log_name), "%Y%m%d_%H%M%S.log");
    g_log = fopen(log_name, "a");
    if (g_log == NULL) {
        fprintf(stderr, "Cannot open log %s: %s\n", log_name, strerror(errno));
        return EXIT_FAILURE;
    }

    speed = baud_to_speed(baud);
    if (speed == B0) {
        snprintf(msg, sizeof(msg), "ERROR: unsupported baudrate %d", baud);
        fprintf(stderr, "%s\n", msg);
        log_line(msg);
        fclose(g_log);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0) {
        snprintf(msg, sizeof(msg), "ERROR: cannot open %s: %s", argv[1],
                 strerror(errno));
        fprintf(stderr, "%s\n", msg);
        log_line(msg);
        fclose(g_log);
        return EXIT_FAILURE;
    }
    if (setup_port(fd, speed, &saved) != 0) {
        snprintf(msg, sizeof(msg), "ERROR: cannot configure %s: %s", argv[1],
                 strerror(errno));
        fprintf(stderr, "%s\n", msg);
        log_line(msg);
        close(fd);
        fclose(g_log);
        return EXIT_FAILURE;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal; /* no SA_RESTART: read() returns EINTR */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    snprintf(msg, sizeof(msg), "Started on %s at %d 8N1", argv[1], baud);
    printf("%s\n", msg);
    log_line(msg);

    if (run(fd) != 0) {
        ret = EXIT_FAILURE;
    }

    log_line("Stopped");
    printf("Stopped\n");
    tcsetattr(fd, TCSANOW, &saved);
    close(fd);
    fclose(g_log);
    return ret;
}
