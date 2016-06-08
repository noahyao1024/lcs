#include <stdio.h>

// log type
#define LOG_TYPE_FILE 1
#define LOG_TYPE_CMD 2

#define MAX_LOG_BUFSIZE 1024

struct _Logger;
typedef struct _Logger Logger;

struct _Logger {
    const char* name;
    short type;
    void* handler; // handler, such as a FILE pointer
    int (*warning) (Logger* this, const char* format, const char* errmsg);
};

int init_logger(Logger* logger, short type, const char* path);
int close_logger(Logger* logger);
