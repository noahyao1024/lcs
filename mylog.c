#include "mylog.h"

int default_warning(Logger* this, const char* format, const char* errmsg) {
    if(LOG_TYPE_FILE == this->type) {
        char buf[MAX_LOG_BUFSIZE];
        sprintf(buf, format, errmsg);
        fwrite(buf, sizeof(char), strlen(buf), this->handler);
        fwrite("\n", sizeof(char), strlen("\n"), this->handler);
        fflush(this->handler);
    }
    return 0;
}

int init_logger(Logger* logger, short type, const char* path) {
    memset(logger, 0, sizeof(Logger));
    logger->type = type;
    if(LOG_TYPE_FILE == logger->type) {
        logger->handler = (FILE*)fopen(path, "ab+");
        if(NULL == logger->handler) {
            return -1;
        }
        logger->warning = default_warning;
    }
    return 0;
}

int close_logger(Logger* logger) {
    free(logger);
    return 0;
}
