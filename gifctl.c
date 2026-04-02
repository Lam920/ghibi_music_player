// gifctl.c
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int fd;
    struct sockaddr_un addr;
    char buf[512];
    char cmd[256];
    ssize_t n;
    int i, len;

    if (argc < 2) {
        printf("Usage: %s COMMAND [ARGS]\n", argv[0]);
        return 1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/ghibli_gif.sock");

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    /* Build command string from all arguments */
    len = 0;
    for (i = 1; i < argc && len < sizeof(cmd) - 2; i++) {
        if (i > 1) cmd[len++] = ' ';  /* Add space between args */
        n = strlen(argv[i]);
        if (len + n >= sizeof(cmd) - 1) break;
        strcpy(cmd + len, argv[i]);
        len += n;
    }
    cmd[len] = '\0';

    /* Send command */
    write(fd, cmd, strlen(cmd));
    write(fd, "\n", 1);

    /* Read response */
    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("%s", buf);
        /* Return non-zero if BUSY or ERR */
        if (strncmp(buf, "BUSY", 4) == 0) {
            close(fd);
            return 2;  /* Busy error code */
        }
        if (strncmp(buf, "ERR", 3) == 0) {
            close(fd);
            return 1;  /* Command error */
        }
    }

    close(fd);
    return 0;
}