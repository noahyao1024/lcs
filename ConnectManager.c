#include "ConnectManager.h"

Connect* connect_pool[MAX_CONNECT];
Logger* connect_warning_logger;

pthread_t thread_pool[MAX_THREAD];

int do_accept() {
    pid_t pid = fork();
    if (-1 == pid) {
        return -1;
    }
    if (0 == pid) {
        // child process
        exit(0);
    }
    return 0;
}

int do_handle_connection() {
    return 0;
}

int handle_connection(int fd, Logger* connect_warning_logger) {
    int thread_index = fd%MAX_THREAD;
    pthread_t thread;
    connect_warning_logger->warning(connect_warning_logger, "Accept Success, fd[%d]", fd);
    if(-1 == thread_pool[thread_index]) {
        // create thread
        pthread_create(&thread, NULL, (void *)&do_handle_connection, (void*) fd);
    } else {
    }
}

int create_and_bind(const char* port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    result = rp = NULL;
    int ret, socket_fd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    ret = getaddrinfo(NULL, port, &hints, &result);
    if(0 != ret) {
        connect_warning_logger->warning(connect_warning_logger, "getaddrinfo error[%s]", gai_strerror(ret));
        return -1;
    }

    for(rp = result; rp != NULL; rp=rp->ai_next) {
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(socket_fd == -1) {
            connect_warning_logger->warning(connect_warning_logger, "Error create socket", "");
            continue;
        }

        ret = bind(socket_fd, rp->ai_addr, rp->ai_addrlen);
        if(ret == 0) {
            // Bind success
            break;
        }

        if(-1 == ret) {
            connect_warning_logger->warning(connect_warning_logger, "fail to bind[%s]", gai_strerror(ret));
        }
        ret = close(socket_fd);
        if(0 != ret) {
            connect_warning_logger->warning(connect_warning_logger, "fail to close socket", "");
        }
    }

    if(NULL == result) {
        connect_warning_logger->warning(connect_warning_logger, "Could not get valid address info [%s]", port);
        return -1;
    }

    freeaddrinfo(result);
    return socket_fd;
}

static int make_socket_non_blocking(int socket_fd) {
    int flags, ret;

    flags = fcntl(socket_fd, F_GETFL, 0);
    if(flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    ret = fcntl(socket_fd, F_SETFL, flags); /* write this fd non-block */
    if(ret == -1) {
        perror("fcntl");
        return -1;
    }
    return 0;
}

int create_server(const char* port, int* myerrno, char* errmsg) {

    memset(thread_pool, 0, MAX_THREAD*sizeof(pthread_t));
    // init var
    memset(connect_pool, 0, MAX_CONNECT * sizeof(Connect));
    connect_warning_logger = (Logger*)malloc(sizeof(Logger));
    char warning_file_name[MAX_LOG_FILENAME_LEN];
    sprintf(warning_file_name, "%s/%s", CONNECT_LOG_PATH, CONNECT_LOG_WARNING_FILENAME);
    //printf("Log file : %s\n", warning_file_name);
    init_logger(connect_warning_logger, LOG_TYPE_FILE, warning_file_name);

    int socket_fd = -1;
    int epoll_fd = -1;
    int ret = -1;
    char* socket_read_buf = (char*)malloc(MAX_BUF_SIZE);
    struct epoll_event* events;
    struct epoll_event event;

    socket_fd = create_and_bind(port);
    if(socket_fd == -1) {
        strcpy(errmsg, "Create and Bind Failed");
        return -1;
    }

    ret = make_socket_non_blocking(socket_fd);
    if(ret == -1) {
        strcpy(errmsg, "make_socket_non_blocking failed");
        return -1;
    }

    ret = listen(socket_fd, SOMAXCONN);
    if(ret == -1) {
        strcpy(errmsg, "Listen failed");
        return -1;
    }

    epoll_fd = epoll_create(1);
    if(epoll_fd == -1) {
        strcpy(errmsg, "Epoll_create failed");
        return -1;
    }

    event.data.fd = socket_fd;
    event.events = EPOLLIN | EPOLLET;
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event); /* add socket to event queue */
    if(ret == -1) {
        strcpy(errmsg, "Call Epoll_ctl failed");
        return -1;
    }

    // allocate the memory
    events = calloc(MAXEVENTS, sizeof event);

    /* The event loop */
    while(1) {
        int n, i, cfd; // event fd index
        n = epoll_wait(epoll_fd, events, MAXEVENTS, -1);

        /* blocking return ready fd */
        for(i = 0; i < n; i++) {
            cfd = events[i].data.fd;
            if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                connect_warning_logger->warning(connect_warning_logger, "Event Error, Event[%d]", events[i].events);
                close(events[i].data.fd);
            } else if(socket_fd == events[i].data.fd) {
                while(1) {
                    // accept a connection
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    in_len = sizeof in_addr;
                    infd = accept(socket_fd, &in_addr, &in_len);
                    if(infd == -1) {
                        connect_warning_logger->warning(connect_warning_logger, "Accept Failed", "");
                        break;
                    }

                    // Save it in Connect Pool, fd is the Key
                    Connect* new_conn = malloc(sizeof(Connect));
                    new_conn->status = 1;
                    connect_pool[infd] = new_conn;

                    ret = getnameinfo(&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf, NI_NUMERICHOST | NI_NUMERICSERV);
                    if(ret == -1) {
                        connect_warning_logger->warning(connect_warning_logger, "getnameinfo failed", "");
                        break;
                    }

                    /* Make the incoming socket non-blocking and add it to the
                     * list of fds to monitor. */
                    ret = make_socket_non_blocking(infd);
                    if(ret == -1) {
                        connect_warning_logger->warning(connect_warning_logger, "make_socket_non_blocking failed", "");
                        break;
                    }

                    handle_connection(infd, connect_warning_logger);

                    /*
                    // add to epoll
                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event);
                    if(ret == -1) {
                    connect_warning_logger->warning(connect_warning_logger, "epoll_ctl failed", "");
                    break;
                    }
                    */

                    // Accept Finally Successfully
                    break;
                }
            } else {
                /*
                   unsigned int count;
                   count = read(cfd, socket_read_buf, MAX_BUF_SIZE);
                   if(count == -1) {
                   connect_warning_logger->warning(connect_warning_logger, "fail to read buf", "");
                   connect_pool[cfd]->status = CONNECT_STATUS_ERROR;
                   break;
                   } else if(count == 0) {
                // reach the end
                connect_warning_logger->warning(connect_warning_logger, "Read Socket buf Success", "");
                connect_pool[cfd]->status = CONNECT_STATUS_DONE;

                // get resource back
                free(connect_pool[cfd]);
                connect_pool[cfd] = NULL;
                break;
                } else {
                socket_read_buf[count] = '\0';
                //do_accept();
                printf(socket_read_buf);
                }
                */
            }
        }
    }

    free(events);
    free(connect_warning_logger);

    close(socket_fd);
    close(epoll_fd);

    close_logger(connect_warning_logger);
    return EXIT_SUCCESS;
}
