/* This is server program, which creates a server and listens for
    messages from clients */


#include "my_server.h"


pthread_mutex_t mutexes[MAXCLIENTS];

void* handle_connection(void* memory) {
    int ret = 0;

    printf("Entered new thread\n");

    char dir[MAX_PATH];
    char* dirp = getcwd(dir, MAX_PATH);
    if (dirp == NULL) {
        ERROR(errno);
        exit(EXIT_FAILURE);
    }

    printf("Current thread directory:%s\n", dir);

    int old_id = -1;

    while (1) {
        /* Read data from pipe and determine what to do with client */
        struct message msg;
        //memset(&msg, '\0', sizeof(struct message));

        /* read message data */
        //ret = read(*((int*) client_pipe), &msg, sizeof(struct message));
        /* Update info */
        memcpy(&msg, memory, sizeof(struct message));
        pthread_mutex_lock(&mutexes[msg.id]);
        memcpy(&msg, memory, sizeof(struct message));


        // printf("Bytes read from client pipe: %d\n", ret);
        // if (ret != sizeof(struct message)) {
        //     printf("Error reading from pipe\n");
        //     if (ret < 0) {
        //         ERROR(errno);
        //     }
        //     exit(EXIT_FAILURE);
        // }

        /* Print info */
        printf("Message received:\n");
        printf("ID: %d\n", msg.id);
        printf("Command: %s\n", msg.cmd);
        printf("Data: %s\n", msg.data);

        char* addr = inet_ntoa(msg.client_data.sin_addr);
        if (addr == NULL) {
            printf("Client address invalid\n");
        }

        printf("Client address: %s\n", addr);

        /* Handle client's command */   
        if (strncmp(msg.cmd, LS, LS_LEN) == 0) {
            /* Printing current directory */
            D(printf("Executing LS command from %s directory\n", dir);)

            /* Redirect output to pipe and then read it */
            int ls_pipe[2];
            //close(ls_pipe[1]); // not writing
            ret = pipe(ls_pipe);
            if (ret < 0) {
                ERROR(errno);
                exit(EXIT_FAILURE);
            }



            int status = 0;

            int pid = fork();
            if (pid < 0) {
                ERROR(errno);
                exit(EXIT_FAILURE);
            }
            if (pid == 0) {
                char cmd[] = "ls";
                char* arg[3];
                arg[0] = "ls";
                arg[1] = dir;
                arg[2] = NULL;
                ret = dup2(ls_pipe[1], STDOUT_FILENO);
                close(ls_pipe[0]); // not reading
                //close(ls_pipe[1]);
                if (ret < 0) {
                    ERROR(errno);
                    exit(EXIT_FAILURE);
                }   
                execvp(arg[0],arg);

                ERROR(errno);
                exit(EXIT_FAILURE);
            }

            // /* read from pipe to buf */
            wait(&status);

            char buf[BUFSIZ];
            buf[BUFSIZ - 1] = '\0';
            ret = read(ls_pipe[0], buf, MSGSIZE);
            if (ret < 0) {
                ERROR(errno);
                exit(EXIT_FAILURE);
            }

            printf("Bytes read from pipe: %d\n", ret);

            printf("LS result: %s\n", buf);

            memcpy(&(msg.data), buf, MSGSIZE);

            close(ls_pipe[0]);
            close(ls_pipe[1]);
        } else if (strncmp(msg.cmd, CD, CD_LEN) == 0) {
            printf("Cwd: %s\n", dir);
            memcpy((void*) dir, &msg.data, MSGSIZE);
            printf("Changing cwd to %s\n", msg.data);
        }

        /* Here we will simply use pipe to transfer data */
        /* Or maybe use ports? */

        int sk = socket(AF_INET, SOCK_DGRAM, 0);
        if (sk < 0) {
            ERROR(errno);
            exit(EXIT_FAILURE);
        }


        printf("SENDING MESSAGE BACK TO CLIENT\n");
        printf("ID: %d\n", msg.id);
        printf("Command: %s\n", msg.cmd);
        printf("Data: %s\n", msg.data);
        ret = send_message(sk, &msg, sizeof(struct message), &msg.client_data);
        if (ret < 0) {
            ERROR(errno);
            exit(EXIT_FAILURE);
        }

        close(sk);
        printf("MESSAGE SENT\n");
    }

    return NULL;
}


int main() {

    /* Creating and initializing socket */
    int sk = 0;
    int ret = 0;

    #ifdef UDP
    sk = socket(AF_INET, SOCK_DGRAM, 0);
    #endif
    
    #ifdef INET
    sk = socket(AF_INET, SOCK_STREAM, 0);
    #endif

    #ifdef LOCAL
    sk = socket(AF_UNIX, SOCK_STREAM, 0);
    #endif

    if (sk < 0) {
        ERROR(errno);
        return -1;
    }


    /* init socket address and family */
    #ifdef INET

    struct sockaddr_in sk_addr = {0};
    sk_addr.sin_family = AF_INET;
    sk_addr.sin_port = htons(23456);
    sk_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // fix later

    #endif

    #ifdef LOCAL

    struct sockaddr_un sk_addr = {0};
    sk_addr.sun_family = AF_UNIX;
    strncpy(sk_addr.sun_path, PATH, sizeof(sk_addr.sun_path) - 1);

    #endif

    #ifdef COMM

    struct in_addr addr;
    res = inet_pton(AF_INET, FRIENDIP, &addr);
    if (res != 1) {
        printf("IP address is invalid\n");
    }
    sk_addr.sin_addr.s_addr = addr.s_addr;

    #endif

    #ifdef UDP

    struct sockaddr_in sk_addr;
    sk_addr.sin_family = AF_INET;
    sk_addr.sin_port = htons(PORT);
    sk_addr.sin_addr.s_addr = htonl(INADDR_ANY); // fix later CAREFUL

    /* Set up fixed amount of thread identifiers, each thread identifier associates
    with a particular client */
    pthread_t thread_ids[MAXCLIENTS];

    /* Basically bitmap */
    int id_map[MAXCLIENTS];
    for (int i = 0; i < MAXCLIENTS; ++i) {
        /* Initialize mutexes */
        ret = pthread_mutex_init(&mutexes[i], NULL);
        if (ret < 0) {
            ERROR(errno);
            exit(EXIT_FAILURE);
        }
        id_map[i] = 0;
    }

    /* pipes for transferring data */
    int pipes[MAXCLIENTS * 2];

    /* memory for sharing between threads */
    struct message* memory = (struct message*) calloc(MAXCLIENTS, sizeof(struct message));
    if (memory == NULL) {
        printf("Error callocing memory\n");
        exit(EXIT_FAILURE);
    }

    #endif

    ret = bind(sk, (struct sockaddr*) &sk_addr, sizeof(sk_addr));

    if (ret < 0) {
        ERROR(errno);
        close(sk);
        exit(EXIT_FAILURE);
    }

    /* activate socket and listen for transmissions */

    #ifdef TCP

    res = listen(sk, MAX_QUEUE);

    if (res < 0) {
        ERROR(errno);
        close(sk);
        return -1;
    }

    #endif

    /* Listen for clients */
    while (1) {
        /* client connects to socket */

        #ifdef UDP

        char buf[BUFSIZ];
        struct message msg;
        memset(&msg, '\0', sizeof(struct message));

        /* Receiving message from client */
        struct sockaddr_in client_data;
        socklen_t addrlen = sizeof(client_data);

        ret = recvfrom(sk, &msg, sizeof(struct message), 0, (struct sockaddr*) &client_data, &addrlen);
        if (ret < 0) {
            ERROR(errno);
            exit(EXIT_FAILURE);
        }

        msg.is_new = 1;
        /* Copy client address manually */
        memcpy(&(msg.client_data), &client_data, sizeof(struct sockaddr_in));


        printf("\n\n\nBytes received: %d\n", ret);
        printf("Message size expected: %ld\n", sizeof(struct message));

        char* addr = inet_ntoa(client_data.sin_addr);
        if (addr == NULL) {
            printf("Client address invalid\n");
        }

        printf("Client address: %s\n", addr);
        printf("Client port: %d\n", htons(client_data.sin_port));
        if (ret < 0) {
            ERROR(errno);
            exit(EXIT_FAILURE);
        }

        /* Check whether we need a new thread */
        int exists = lookup(id_map, MAXCLIENTS, msg.id);
        if (exists == 0) {
            printf("New client: %d\n", msg.id);
            id_map[msg.id] = 1;

             /* Create pipe */
            // ret = pipe(pipes + msg.id);
            // if (ret < 0) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }

            // int pipe_out = pipes[msg.id];
            struct message* thread_memory = &memory[msg.id];
            memcpy(thread_memory, &msg, sizeof(struct message));
            /* Handing over this client to a new thread */
            ret = pthread_create(&thread_ids[msg.id], NULL, handle_connection, thread_memory);

        } else {
            struct message* thread_memory = &memory[msg.id];
            memcpy(thread_memory, &msg, sizeof(struct message));
            printf("Old client: %d\n", msg.id);
        }

        /* Transfer the data to corresponding thread */

        /* Decide which message was sent */

        if (strncmp(msg.cmd, PRINT, PRINT_LEN) == 0) {
            // int pipe_in = pipes[msg.id + 1];
            // ret = write(pipe_in, &msg, sizeof(struct message));
            // printf("Bytes written to pipe: %d\n", ret);
            // if (ret != sizeof(struct message)) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }
            // if (ret < 0) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }
            struct message* thread_memory = &memory[msg.id];
            memcpy(thread_memory, &msg, sizeof(struct message));
            pthread_mutex_unlock(&mutexes[msg.id]);

            /* Print message */
            printf("Message from client: %s\n", msg.data);
        } else if (strncmp(msg.cmd, EXIT, EXIT_LEN) == 0) {
            /* Closing server */
                close(sk);
                unlink(PATH);
                exit(EXIT_SUCCESS);
        } else if (strncmp(msg.cmd, LS, LS_LEN) == 0) {
            /* Printing current directory */
            printf("Executing LS command\n");

            /* Redirect output to pipe and then read it */
            // int ls_pipe[2];
            // //close(ls_pipe[1]); // not writing
            // ret = pipe(ls_pipe);
            // if (ret < 0) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }


            // int pid = fork();
            // if (pid < 0) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }
            // if (pid == 0) {
            //     char cmd[] = "ls";
            //     char* arg[2];
            //     arg[0] = "ls";
            //     arg[1] =
            //     arg[1] = NULL;
            //     ret = dup2(ls_pipe[1], STDOUT_FILENO);
            //     close(ls_pipe[0]); // not reading
            //     //close(ls_pipe[1]);
            //     if (ret < 0) {
            //         ERROR(errno);
            //         exit(EXIT_FAILURE);
            //     }   
            //     execvp(arg[0],arg); // add current directory later

            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }

            // /* read from pipe to buf */
            // char buf[BUFSIZ];
            // buf[BUFSIZ - 1] = '\0';
            // ret = read(ls_pipe[0], buf, MSGSIZE);
            // if (ret < 0) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }

            // printf("Bytes read from pipe: %d\n", ret);

            // printf("LS result: %s\n", buf);

            // memcpy(&(msg.data), buf, MSGSIZE);

            // int pipe_in = pipes[msg.id + 1];
            // ret = write(pipe_in, &msg, sizeof(struct message));
            // printf("Bytes written to client pipe: %d\n", ret);
            // if (ret != sizeof(struct message)) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }

            // close(ls_pipe[0]);
            // close(ls_pipe[1]);

            struct message* thread_memory = &memory[msg.id];
            memcpy(thread_memory, &msg, sizeof(struct message));
            pthread_mutex_unlock(&mutexes[msg.id]);

        } else if (strncmp(msg.cmd, CD, CD_LEN) == 0) {
            // printf("Executing CD command\n");

            // int pipe_in = pipes[msg.id + 1];
            // ret = write(pipe_in, &msg, sizeof(struct message));
            // printf("Bytes written to client pipe: %d\n", ret);
            // if (ret != sizeof(struct message)) {
            //     ERROR(errno);
            //     exit(EXIT_FAILURE);
            // }

            struct message* thread_memory = &memory[msg.id];
            memcpy(thread_memory, &msg, sizeof(struct message));
            pthread_mutex_unlock(&mutexes[msg.id]);

        } else if (strncmp(msg.cmd, BROAD, BROAD_LEN) == 0) {
            printf("Broadcasting server IP\n");
            char message[] = "Reply to client";
            ret =  sendto(sk, &message, sizeof(message), 0,
                             (struct sockaddr*) &client_data, sizeof(client_data));
            if (ret < 0) {
                ERROR(errno);
                exit(EXIT_FAILURE);
            }

        } else {
            printf("Command from client not recognized\n");
            printf("Actual command sent: %s\n", msg.cmd);
        }
        
        printf("\n\n\n");
        #endif
        

            


        #ifdef TCP

        int client_sk = 0;

        client_sk = accept(sk, NULL, NULL);

        if (client_sk < 0) {
            ERROR(errno);
            exit(EXIT_FAILURE);
        }
            

        /* read message that was accepted */
        /* buf for accepting command */
        char buf[BUFSZ] = {};
        /* message buffer */
        char msg[BUFSZ] = {};
        res = read(client_sk, buf, BUFSZ);

        if (res < 0 || res >= BUFSZ) {
            //fprintf(stderr, "Unexpected read error or overflow %d\n", res);
            ERROR(errno);
            return -1;
        }

        /* Commands are: PRINT, EXIT */
        if (strcmp(buf, PRINT) == 0) {
            /* read message and print it */
            res = read(client_sk, msg, BUFSZ);
            if (res < 0 || res >= BUFSZ) {
                printf("Unexpected read error or overflow %d\n", res);
                return -1;
            }
        /* Print message */
        printf("Message from client: %s\n", msg);

        /* then change sockaddr if needed */
        #ifdef INET

            struct in_addr addr;
            res = inet_pton(AF_INET, msg, &addr);
            if (res != 1) {
                printf("IP address is invalid\n");
            }
            sk_addr.sin_addr.s_addr = addr.s_addr;

        #endif

        } else if (strcmp(buf, EXIT) == 0) {
            close(client_sk);
            unlink(PATH);
            exit(EXIT_SUCCESS);
        } else {
            printf("Command from client not recognized\n");
        }

            /* finish communication */
        close(client_sk);

        #endif
        // примечание: если файл (или сокет) удалили с файловой системы, им еще могут пользоваться программы которые не закрыли его до закрытия
    }

    return 0;
}