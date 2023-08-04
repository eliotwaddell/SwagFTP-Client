#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

typedef enum cmdlist 
{ 
  ASCII, BINARY, CD, DELETE, GET, HELP, LS, MKDIR, PUT, PWD, QUIT, RMDIR
} cmdlist;

static const char *cmdlist_str[] =
{
    "ascii", "binary", "cd", "delete", "get", "help", "ls", "mkdir", "put", "pwd",
    "quit", "rmdir"
};