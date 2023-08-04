#include "utils.h"

#define CONTROL_SOCKET_PORT "8021"

#define white() printf("\033[0;0m")
#define red() printf("\033[0;31m")
#define green() printf("\033[0;32m")
#define yellow() printf("\033[0;33m]")
#define cyan() printf("\033[0;36m")

#define clear() printf("\033[H\033[J")

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 

int parse_ftp_code(char* response)
{
    return atoi(strtok(response, " "));
}

int lookup(char *needle, const char **haystack, int count)
{
  int i;
  for(i=0;i<count; i++){
    if(strcmp(needle,haystack[i])==0)return i;
  }
  return -1;
}

int lookup_cmd(char *cmd)
{
    if(cmd == NULL)
        return -2;
    const int cmdlist_count = sizeof(cmdlist_str)/sizeof(char *);
    return lookup(cmd, cmdlist_str, cmdlist_count);
}

char* ftp_command(char* command_code, char* arg)
{
    char* command = malloc(strlen(command_code) + strlen(arg) + 4); // space, CRLF, null
    memset(command, '\0', sizeof(command));
    strcpy(command, command_code);
    strcpy(command + strlen(command_code), " ");
    strcpy(command + strlen(command_code) + 1, arg);
    strcpy(command + strlen(command), "\r\n");

    return command;
}

void refresh_cd(char* cd, int control_fd)
{
    memset(cd, 0, 1024);
    char* command = ftp_command("PWD", ""); //always the same
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, cd, 1024, 0);

    strcpy(cd, strrchr(cd, ' ') + 2);
    cd[strlen(cd) - 2] = '\0';
}

void refresh_pasv(int* data_fd, int control_fd, struct addrinfo* hints, struct addrinfo** res)
{
    char buf[128];
    char* command = ftp_command("PASV", "");
    int len = strlen(command);
    int status;
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, sizeof(buf), 0);

    if(parse_ftp_code(buf) != 227)
    {
        fprintf(stderr, "Could not enter passive mode.\nReturned %s\n", buf);
        return;
    }

    char pasv_connect_data[25]; // no malloc needed, max is 24 chars
    strcpy(pasv_connect_data, (strrchr(buf + 4, ' ') + 2)); // cut out starting ( w/ copy
    pasv_connect_data[strlen(pasv_connect_data) - 2] = '\0'; // cut out ending )

    // tokenize (crypto reference 🤓)
    int i = 0;
    char ip_string[16]; // max 15 chars (255.255.255.255)
    *ip_string = '\0';
    char port_string[6]; // max 5 chars (65535)
    char *curr = strtok(pasv_connect_data, ",");

    while(curr != NULL)
    {
        if(i < 4) // ip
        {
            strcat(ip_string, curr);
            if(i < 3)
                strcat(ip_string, ".");
        }
        else // port
        {
            int port = atoi(curr) * 256;
            curr = strtok(NULL, ",");
            port += atoi(curr);
            sprintf(port_string, "%d", port);
        }
        curr = strtok(NULL, ",");
        i++;
    }

    // hostname, port, hints, result, for data
    if ((status = getaddrinfo(ip_string, port_string, hints, res)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return;
    }

    if((*data_fd = socket((*res)->ai_family, (*res)->ai_socktype, (*res)->ai_protocol)) == -1) //shoutout getaddrinfo part 2
    {
        int err = errno;
        fprintf(stderr, "Error allocating socket descriptor\n Code %d\n", err);
        freeaddrinfo(*res);
        return;
    }
    else if(connect(*data_fd, (*res)->ai_addr, (*res)->ai_addrlen) == -1)
    {
        int err = errno;
        fprintf(stderr, "Error connecting to socket\nCode %d\n", err);
        freeaddrinfo(*res);
        return;
    }
}

void connecting_troll()
{
    srand(time(NULL));
    yellow();
    for ( int loop = 0; loop < rand() % 8; ++loop) {
        for ( int each = 0; each < 4; ++each) {
            printf ( "\rConnecting%.*s   \b\b\b", each, "...");
            fflush ( stdout);//force printing as no newline in output
            usleep ( 250000);
        }
    }
    white();
    printf ( "\n");
}

void cmd_ascii(bool* ascii_mode)
{
    if(*ascii_mode == true)
    {
        printf("Already in ascii mode\n");
    }
    else
    {
        *ascii_mode = true;
        printf("Switched to ascii mode\n");
    }
}

void cmd_binary(bool* ascii_mode)
{
    if(!*ascii_mode == true)
    {
        printf("Already in binary mode\n");
    }
    else
    {
        *ascii_mode = false;
        printf("Switched to binary mode\n");
    }
}

void cmd_cd(char* path, int control_fd)
{
    if(path == NULL)
    {
        printf("usage: cd [path]\n");
        return;
    }
    char buf[128];
    char* command = ftp_command("CWD", path);
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, 1024, 0);

    if(parse_ftp_code(buf) != 250)
    {
        printf("Failed to change directory\n");
    }
}

void cmd_delete(char* path, int control_fd)
{
    if(path == NULL)
    {
        printf("usage: delete [path]\n");
        return;
    }
    char buf[128];
    char* command = ftp_command("RMD", path);
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, 128, 0);

    if(parse_ftp_code(buf) != 250)
    {
        printf("Failed to delete file\n");
    }
    else
    {
        printf("File successfully deleted\n");
    }
}

void cmd_get(char* path, int control_fd, int data_fd)
{

    if(path == NULL)
    {
        printf("usage: get [path]\n");
        return;
    }
    char buf[8192];
    char* command = ftp_command("RETR", path);
    char* filename = strrchr(path, '/') == NULL ? path : strrchr(path, '/') + 1;
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, 8192, 0);

    if(parse_ftp_code(buf) != 150)
    {
        printf("Failed to get file\n");
        recv(control_fd, buf, 8192, 0); // clear out fd on failure to not clog 
        return;
    }

    int filesize = recv(data_fd, buf, 8192, 0);
    FILE* received_file = fopen(filename, "w");

    if(received_file == NULL)
    {
        printf("Failed to open file %s (could be a duplicate name in use)\n", filename);
        recv(control_fd, buf, 8192, 0); 
        return;
    }

    if(filesize == 0)
    {
        printf("File is empty\n");
        recv(control_fd, buf, 8192, 0); 
        return;
    }

    while(filesize != 0)
    {
        fwrite(buf, sizeof(char), filesize, received_file);
        filesize = recv(data_fd, buf, 8192, 0);
    }

    recv(control_fd, buf, 8192, 0);
    if(parse_ftp_code(buf) == 226)
    {
        printf("File %s successfully fetched\n", filename);
    }
    else
    {
        printf("Failed to read file %s\n", filename);
    }
    
    fclose(received_file);
}

void cmd_help()
{
    printf("********************COMMANDS********************\n");
    printf("ascii - set mode of file transfer to ascii (default)\n");
    printf("binary - set mode of file transfer to binary\n");
    printf("cd [path] - change directory on remote machine\n");
    printf("delete [filename] - delete a file in the current remote directory\n");
    printf("get [remote_file] - copies file in current remote directory to current local directory\n");
    printf("help - list of all SwagFTP client commands\n");
    printf("ls [optional path] - list files in current or specified remote directory\n");
    printf("mkdir [directory_name] - make a new directory within current remote directory\n");
    printf("put [local_file] - copy local file to remote machine\n");
    printf("pwd - print the path of the current remote directory\n");
    printf("quit - exit SwagFTP client\n");
    printf("rmdir [path] - delete a directory in the current remote directory\n");
}

void cmd_ls(char* path, int control_fd, int data_fd)
{
    char buf[8192];
    memset(buf, 0, 8192);
    char* command = ftp_command("LIST", path == NULL ? "." : path);
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, 8192, 0);

    if(parse_ftp_code(buf) != 150)
    {
        printf("Not a directory\n");
        recv(control_fd, buf, 8192, 0); // clear out fd on failure to not clog 
        return;
    }

    int filesize = recv(data_fd, buf, 8192, 0);

    while(filesize != 0)
    {
        printf(buf);
        filesize = recv(data_fd, buf, strlen(buf), 0);
    }

    recv(control_fd, buf, 8192, 0);
}

void cmd_mkdir(char* directory_name, int control_fd)
{
    if(directory_name == NULL)
    {
        printf("usage: mkdir [directory_name]\n");
        return;
    }
    char buf[128];
    char* command = ftp_command("MKD", directory_name);
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, 128, 0);

    if(parse_ftp_code(buf) != 257)
    {
        printf("Failed to create directory. Check path or permissions.\n");
    }
    else
    {
        printf("Directory /%s created\n", strrchr(directory_name, '/') == NULL ? directory_name : strrchr(directory_name, '/'));
    }
}

void cmd_put(char* filename, int control_fd, int data_fd)
{
    if(filename == NULL)
    {
        printf("usage: put [filename]\n");
        return;
    }
    char buf[128];
    char send_buf[8192];
    char* command = ftp_command("STOR", filename);
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, 128, 0);

    FILE *send_file = fopen(filename, "rb");
    if(send_file == NULL)
    {
        printf("Error opening file\n");
        return;
    }
    
    off_t offset = 0;
    int sent;
    while((sent = fread(send_buf, 1, 8192, send_file)) > 0)
    {
        send(data_fd, send_buf, sent, 0);
    }

    fclose(send_file);
    printf("File sent\n");
}

void cmd_rmdir(char* path, int control_fd)
{
    if(path == NULL)
    {
        printf("usage: rmdir [path]\n");
        return;
    }
    char buf[128];
    char* command = ftp_command("RMD", path);
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv(control_fd, buf, 128, 0);

    if(parse_ftp_code(buf) != 250)
    {
        printf("Failed to remove directory\n");
    }
    else
    {
        printf("Directory removed\n");
    }
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res, *p;
    int status, control_fd, data_fd;
    char ipstr[INET6_ADDRSTRLEN];

    bool ascii_mode = true;

    memset(&hints, 0,  sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(argc != 2 && argc != 3)
    {
        fprintf(stderr, "usage: ftp hostname [optional port]\n");
        return 1;
    }

    // hostname, port, hints, result, for control
    if ((status = getaddrinfo(argv[1], argc == 2 ? CONTROL_SOCKET_PORT : argv[2], &hints, &res)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    if((control_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) //shoutout getaddrinfo
    {
        int err = errno;
        fprintf(stderr, "Error allocating socket descriptor for %s\n Code %d\n", argv[1], err);
        freeaddrinfo(res);
        return 3;
    }
    else if(connect(control_fd, res->ai_addr, res->ai_addrlen) == -1)
    {
        int err = errno;
        fprintf(stderr, "Error connecting to socket for %s\nCode %d\n", argv[1], err);
        freeaddrinfo(res);
        return 3;
    }
    
    freeaddrinfo(res); // free the linked list

    char* command;
    char buf[128];
    int recv_len = recv(control_fd, buf, sizeof(buf), 0);

    if(parse_ftp_code(buf) != 220)
    {
        fprintf(stderr, "FTP Server not ready for new user.\nReturned %s\n", buf);
        return 4;
    }

    char username[17];
    char* password;

    printf("\x1B[32mEnter username (max 16 char):\n");
    white();

    fgets(username, 16, stdin);

    command = ftp_command("USER", username);
    int len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv_len = recv(control_fd, buf, sizeof(buf), 0);

    if(parse_ftp_code(buf) != 331)
    {
        fprintf(stderr, "Username not valid.\nReturned %s\n", buf);
        return 4;
    }

    memset(buf, 0,  sizeof buf);

    password = getpass("\x1B[32mEnter your password:\n");

    command = ftp_command("PASS", password);
    len = strlen(command);
    sendall(control_fd, command, &len);
    free(command);
    recv_len = recv(control_fd, buf, sizeof(buf), 0);

    if(parse_ftp_code(buf) != 230)
    {
        fprintf(stderr, "Password not valid.\nReturned %s\n", buf);
        return 5;
    }
    
    refresh_pasv(&data_fd, control_fd, &hints, &res);

    // connecting_troll(); // for immersion

    char input[128] = "";
    char cd[1024] = "";
    // main loop
    clear();
    white();
    printf("************************************************\n");
    printf("*****************");
    green();
    printf("SwagFTP Client");
    white();
    printf("*****************\n");
    printf("*****");
    red();
    printf("`help`");
    green();
    printf(" for list of commands and usages");
    white();
    printf("*****\n");
    printf("**********");
    cyan();
    printf("\xC2\xA9");
    printf(" BlayKeyBlocks DevTeam 2023");
    white();
    printf("**********\n");
    printf("************************************************\n");
    refresh_cd(cd, control_fd);
    char* curr_arg;
    while(1)
    {
        printf("[");
        green();
        printf("SwagFTP");
        white();
        printf("@%s]$ ", cd);

        fgets(input, 128, stdin);
        if ((strlen(input) > 0) && (input[strlen (input) - 1] == '\n'))
            input[strlen (input) - 1] = '\0';

        curr_arg = strtok(input, " ");
        switch(lookup_cmd(curr_arg))
        {
            case ASCII: cmd_ascii(&ascii_mode); break;
            case BINARY: cmd_binary(&ascii_mode); break;
            case CD: cmd_cd(strtok(NULL, " "), control_fd); refresh_cd(cd, control_fd); break;
            case DELETE: cmd_delete(strtok(NULL, " "), control_fd); break;
            case GET: refresh_pasv(&data_fd, control_fd, &hints, &res);
                        cmd_get(strtok(curr_arg + 4, " "), control_fd, data_fd); break;
            case HELP: cmd_help(); break;
            case LS: refresh_pasv(&data_fd, control_fd, &hints, &res);
                        cmd_ls(strtok(curr_arg + 3, " "), control_fd, data_fd); break;
            case MKDIR: cmd_mkdir(strtok(NULL, " "), control_fd); break;
            case PUT: refresh_pasv(&data_fd, control_fd, &hints, &res);
                        cmd_put(strtok(curr_arg + 4, " "), control_fd, data_fd); break;
            case PWD: printf("Current remote directory:\n%s\n", cd);
            case QUIT: goto end;
            case RMDIR: cmd_rmdir(strtok(NULL, " "), control_fd);
            case -2: break; //newline
            default:
                printf("Command %s not recognized\n", input);
                break;
        }
    }

    end: ;
    printf("Have a nice day!\n");
    return 0;
}