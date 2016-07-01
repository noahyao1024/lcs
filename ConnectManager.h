/***************************************************************************
 * 
 * Copyright (c) 2016 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/

/**
 * @file ConnentManager.h
 * @author yaokun(com@baidu.com)
 * @date 2016/05/20 17:05:45
 * @brief 
 *  
 **/

#ifndef  __CONNENTMANAGER_H_
#define  __CONNENTMANAGER_H_

#define CONNECT_LOG_PATH "."
#define CONNECT_LOG_WARNING_FILENAME "connect.log.wf"

#define MAX_LOG_FILENAME_LEN 200
#define MAX_THREAD 30

#define CONNECT_STATUS_ERROR 1
#define CONNECT_STATUS_DONE 0

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "const.h"

#include "mylog.h"

typedef struct {
    short status;
    short type;
    //int (*protocol_header)(char* buf, const char* header); // 协议
} MyConnect;

typedef struct {
    struct epoll_event* event;
    pthread_t* thread;
    int epoll_fd;
} MyThread;

int create_server(const char* port, int* myerrno, char* errmsg);

// do accept

#endif  //__CONNENTMANAGER_H_
