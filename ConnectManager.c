#include "ConnectManager.h"

Logger* connect_warning_logger;

MyConnect* connect_pool[MAX_CONNECT];
MyThread* thread_pool[MAX_THREAD];

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

int do_handle_connection(void* arg) {
    int epoll_fd = (int)arg;
    char socket_read_buf[MAX_BUF_SIZE];
    printf("Listen epoll_fd[%d]\n", epoll_fd);
    struct epoll_event* events = calloc(MAXEVENTS/MAX_CONNECT, sizeof(struct epoll_event));

    while(1) {
        int i, cfd;
        int n = epoll_wait(epoll_fd, events, MAXEVENTS/MAX_CONNECT, -1);
        for(i = 0; i < n; i++) {
            cfd = events[i].data.fd;
            if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
            } else {
                unsigned int count;
                memset(socket_read_buf, NULL, MAX_BUF_SIZE*sizeof(char));
                count = read(cfd, socket_read_buf, MAX_BUF_SIZE);
                if(count == -1) {
                    printf("read error\n");
                    continue;
                }
                if(0 == count) {
                    // read finish
                    close(cfd);
                    continue;
                }
                socket_read_buf[count] = "\0";
                printf("Read : [%s]\n", socket_read_buf);
            }
        }
    }
}

int handle_connection(int fd, Logger* connect_warning_logger) {
    int thread_index = fd%MAX_THREAD;
    connect_warning_logger->warning(connect_warning_logger, "Accept Success, fd[%d]", fd);
    if(NULL == thread_pool[thread_index]) {
        MyThread* process_thread = malloc(sizeof(MyThread));
        // create thread
        int epoll_fd = epoll_create(1);
        if(-1 == epoll_fd) {
            connect_warning_logger->warning(connect_warning_logger, "Fail to create epoll", "");
        }
        struct epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET;
        int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event); /* add socket to event queue */
        if(-1 == ret) {
            connect_warning_logger->warning(connect_warning_logger, "Fail to epoll_ctl", "");
        }
        pthread_t* pthread = (pthread_t*) malloc(sizeof(pthread_t));
        pthread_create(pthread, NULL, (void *)&do_handle_connection, (void *) epoll_fd);
        process_thread->thread = pthread;
        process_thread->event = &event;
        process_thread->epoll_fd = epoll_fd;
        thread_pool[thread_index] = process_thread;
        printf("create_thread success, thread_index[%d]\n", thread_index);
    } else {
        printf("thread exist, just add the fd to the event queue, thread_index\n", thread_index);
        int epoll_fd = thread_pool[thread_index] -> epoll_fd;
        struct epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLET;
        int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event); /* add socket to event queue */
        if(-1 == ret) {
            connect_warning_logger->warning(connect_warning_logger, "Fail to epoll_ctl", "");
            return -1;
        }
    }
    return 0;
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
    // init var
    memset(thread_pool, NULL, MAX_THREAD * sizeof(MyThread*));
    memset(connect_pool, NULL, MAX_CONNECT * sizeof(MyConnect*));
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

                    // Save it in MyConnect Pool, fd is the Key
                    MyConnect* new_conn = malloc(sizeof(MyConnect));
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

                    // Accept Finally Successfully
                    break;
                }
            } else {
            }
        }
    }

    // clean resource
    free(events);
    free(connect_warning_logger);

    close(socket_fd);
    close(epoll_fd);

    close_logger(connect_warning_logger);

    // free MyConnetc
    // free MyThread
    return EXIT_SUCCESS;
}
