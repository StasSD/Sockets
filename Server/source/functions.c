#include "my_server.h"


/* Print message info */
void print_info(struct message* msg) {
    printf("ID: %d\n", msg->id);
    printf("Command: %s\n", msg->cmd);
    printf("Data: %s\n", msg->data);
}

/* return number of command to be sent to server */
int check_input(int argc, char** argv, char** command, char** arg) {
    
    const char usage[] = "Usage: ./client [--exit] [--print] <message>, [ls], [cd] <dir>\n";
    /* check input */
    if (argc < 2) {
        printf(usage);
        return BAD_CMD;
    } 

    printf("Command: %s\n", argv[1]);
    *command = argv[1];

    
    /* handle 2 argc command */
    if (argc == 2) {
        if (strcmp(*command, EXIT) == 0) {
            return EXIT_CMD;
        } else if (strcmp(*command, LS) == 0) {
            return LS_CMD;
        } else if (strcmp(*command, CD) == 0) {
            return CD_CMD;
        } else if (strcmp(*command, BROAD) == 0) {
            return BRCAST_CMD;
        } else if (strcmp(*command, SHELL) == 0) {
            return SHELL_CMD;
        }
        printf(usage);
        return BAD_CMD;
    }
    /* get message from argv and send it */

    if (argc == 3) {
        *arg = argv[2];
        if (arg == NULL) {
            printf(usage);
            return BAD_CMD;
        }
        if (strcmp(*command, PRINT) == 0) {
            return PRINT_CMD;
        } else if (strcmp(*command, CD) == 0) {
            return CD_CMD;
        }
    }

    /* No match */
    printf(usage);

    return BAD_CMD;
}

int send_message(int sk, struct message* msg, int msg_len, struct sockaddr_in* sk_addr) {
    int ret = sendto(sk, msg, msg_len, 0, (struct sockaddr*) sk_addr, sizeof(*sk_addr));
    if (ret < 0) {
        ERROR(errno);
        exit(EXIT_FAILURE);
    }
    return ret;
}

int lookup(int* id_map, int n_ids, pid_t id) {
    if (id_map[id] == 1) {
        return 1;
    }
    return 0;
}


void start_shell(char* buf) {
    printf("Starting shell on server\n");
    int ret = 0;

    int fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    if (fd < 0) {
        ERROR(errno);
        exit(EXIT_FAILURE);
    }

    ret = grantpt(fd);
    if (ret < 0) {
        ERROR(errno);
        exit(EXIT_FAILURE);
    }

    ret = unlockpt(fd);  
    if (ret < 0) {
        ERROR(errno);
        exit(EXIT_FAILURE);
    } 

    char* path = ptsname(fd);
    if (path == NULL) {
        printf("Error in path\n");
        exit(EXIT_FAILURE);
    }

    int resfd = open(path, O_RDWR);
    if (resfd < 0) {
        ERROR(errno);
        exit(EXIT_FAILURE);
    }

    struct termios term;
    term.c_lflag = 0;

    ret = tcsetattr(resfd, 0, &term);
    if (ret < 0) {
        ERROR(errno)    ;
        exit(EXIT_FAILURE);
    }


    pid_t pid = fork();
    if (pid == 0) {
        dup2(resfd, STDIN_FILENO);
        dup2(resfd, STDOUT_FILENO);
        dup2(resfd, STDERR_FILENO);
        execl("/bin/bash", "/bin/bash", NULL);
        exit(EXIT_FAILURE);
    }

    /* Writing command */
    ret = write(fd, "echo $$\n", sizeof("echo $$\n"));
    if (ret != sizeof("echo $$\n")) {
        printf("Error writing to fd\n");
        ERROR(errno);
        exit(EXIT_FAILURE);
    }

    /* Check whether all data has been written with poll */
    struct pollfd pollfds;
    pollfds.fd = fd;
    pollfds.events = POLLIN;
    int wait_ms = 1;

    while ((ret = poll(&pollfds, 1, wait_ms)) != 0) {
        if (pollfds.revents == POLLIN) {
            ret = read(fd, buf, BUFSIZ);
            if (ret < 0) {
                ERROR(errno);
                exit(EXIT_FAILURE);
            }
        }

        printf("Bytes read: %d\n", ret);
        buf[ret] = '\0';
        printf("Data read: %s\n", buf);
        if (ret < 0) {
            ERROR(errno);
            exit(EXIT_FAILURE);
        }
    }
}   

// void enter_command() {
//     int ret = 0;

//     printf("Enter command:\n");

//     char cmd[CMDSIZE];

//     fgets
// }
