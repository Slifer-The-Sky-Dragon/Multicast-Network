#include <stdio.h>
#include <string.h>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

const int MSG_SZ = 8;

int main(int argc, char* argv[]) {
    int opt = atoi(argv[1]);
    int fd;
    if (opt == -1)
        fd = open("fifo", O_RDONLY);
    else {
        if (opt == 0)
            mkfifo("fifo", 0666);
        
        fd = open("fifo", O_WRONLY);
    }

    fd_set readfds;

    for (;;) {
        FD_ZERO(&readfds);
        if (opt != -1)
            FD_SET(STDIN_FILENO, &readfds);
        else
            FD_SET(fd, &readfds);

        int res = select(fd + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buf[MSG_SZ];
            bzero(buf, MSG_SZ);
            
            int n = read(STDIN_FILENO, buf, MSG_SZ);

            if (n == 0)
                abort();

            write(fd, buf, strlen(buf));
        }

        else if (FD_ISSET(fd, &readfds)) {
            char buf[MSG_SZ];
            bzero(buf, MSG_SZ);

            int n = read(fd, buf, MSG_SZ);
            if (n == 0) {
                write(STDOUT_FILENO, "OOPS", 4);
                abort();
            }
            string res = "recieved shit is: " + string(buf) + '\n';

            write(STDOUT_FILENO, res.c_str(), res.size());
        }
    }
}
