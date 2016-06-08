#include "lcs.h"

int init() {
    pid_t pid = getpid();
    fprintf(stdout, "Server started, pid[%d]\n", pid);
    return 0;
}

int main(int argc, char *argv[]) {
    Logger* general_logger = malloc(sizeof(Logger));
    init_logger(general_logger, LOG_TYPE_FILE, "test.log");
    general_logger->warning(general_logger, "start", "");
    close_logger(general_logger);

    int ret = -1;
    // read conf
    // todo

    /* read opt */
    //todo

    init();

    int myerrno = 0;
    char* errmsg = (char *)malloc(1024);
    memset(errmsg, 0, 1024);
    
    create_server("8989", &myerrno, errmsg);
    return EXIT_SUCCESS;
}
