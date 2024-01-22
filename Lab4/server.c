#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 8080
#define MAX_ENTRIES 16
#define MAX_OP_LEN 64

const char *err_resp = "response:{code:1}";
const char *ok_resp = "response:{code:0}";

struct entry {
    unsigned char entry_type;
    ino_t ino;
    char name[256];
};

struct entries {
    size_t entries_count;
    struct entry entries[MAX_ENTRIES];
};

struct entry_info {
    unsigned char entry_type;
    ino_t ino;
};


/*
recursively finds the directory with inode=inode
*/

void find_dir(ino_t inode, const char *cur_name, char *res) {

    DIR *dir = opendir(cur_name);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {

        struct stat file_stat;
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", cur_name, entry->d_name);
        if (stat(path, &file_stat) == -1) {
            closedir(dir);
            perror("stat");
            return;
        }


        if (entry->d_type == 4 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".") != 0) {
            printf("check %s: type=%u, ino=%lu\n", entry->d_name, entry->d_type, file_stat.st_ino);
            if (file_stat.st_ino == inode) {
                strcpy(res, path);
                return;
            } else {
                find_dir(inode, path, res);
            }
        }
    }
    closedir(dir);
}

/*
actually gets dir content by name
*/
struct entries list_directory_info(const char *cur_name) {

    struct entries result;
    result.entries_count = 0;


    DIR *dir = opendir(cur_name);
    if (dir == NULL) {
        perror("opendir");
        return result;
    }


    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {

        struct stat file_stat;
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", cur_name, entry->d_name);
        if (stat(path, &file_stat) == -1) {
            perror("stat");
            continue;
        }


        if (result.entries_count < MAX_ENTRIES && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".") != 0) {
            result.entries[result.entries_count].entry_type = entry->d_type;
            result.entries[result.entries_count].ino = file_stat.st_ino;
            strncpy(result.entries[result.entries_count].name, entry->d_name,
                    sizeof(result.entries[result.entries_count].name) - 1);
            result.entries_count++;
        }
    }


    closedir(dir);

    return result;
}


void send(int new_socket, const char *sendbuf) {
    printf("sendbuf %s\n", sendbuf);
    ssize_t sent_bytes = send(new_socket, sendbuf, strlen(sendbuf), 0);
    if (sent_bytes == -1) {
        perror("send");
        exit(1);
    } else printf("sent %ld\n", sent_bytes);
}


void create_entries_json(struct entries my_res, char * result) {
    char *sendbuf = (char *) malloc(1024);
    sprintf(sendbuf, "response:{code:0,entries:{\nentries_count:%ld,\nentries_arr:[\n", my_res.entries_count);
    for (int i = 0; i < my_res.entries_count; i++) {
        char temp[512];
        struct entry en = my_res.entries[i];
        sprintf(temp, "{type:%d,ino:%ld,name:\"%s\"}\n", en.entry_type, en.ino, en.name);
        strcat(sendbuf, temp);
    }
    strcat(sendbuf, "]\n}\n}\n");
    printf("data %s\n size %ld\n", sendbuf, strlen(sendbuf));
    strcpy(result, sendbuf);
    free(sendbuf);
}

void create_entry_info_json(struct entry_info info, char* result) { //check working ok TODO
    char *sendbuf = (char *) malloc(512);
    sprintf(sendbuf, "response:{code:0,entry_info:{");
    char type_temp[256], inode_temp[256];

    sprintf(type_temp, "entry_type:%d,", info.entry_type);
    sprintf(inode_temp, "ino:%ld", info.ino);
    strcat(sendbuf, type_temp);
    strcat(sendbuf, inode_temp);
    strcat(sendbuf, "}}");
    printf("json data: %s\n", sendbuf);
    strcpy(result, sendbuf);
    free(sendbuf);
}


void list_request(int new_socket, ino_t inode) {
    printf("going to get dir info %ld\n", inode);
    const char *start_dir = ".";
    char dir_name[PATH_MAX] = {'\0'};
    find_dir(inode, start_dir, dir_name);
    if (strlen(dir_name) == 0) {
        send(new_socket, err_resp);
        return;
    }
    struct entries my_res = list_directory_info(dir_name);
    char entries_json[1024];
    create_entries_json(my_res, entries_json);
    send(new_socket, entries_json);
}


void lookup_request(int new_socket, ino_t parent_inode, char *name) {
    const char *start_dir = ".";
    char parent_name[PATH_MAX] = {'\0'};
    find_dir(parent_inode, start_dir, parent_name);
    if (strlen(parent_name) == 0) {
        printf("not found parent dir\n");
        send(new_socket, err_resp);
        return;
    }

    printf("going to search file in dir %s\n", parent_name);

    DIR *dir = opendir(parent_name);
    struct dirent *entry;
    struct entry_info info;

    if (dir == NULL) {
        perror("opendir");
        send(new_socket, err_resp);
        return;
    }

    int file_found = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(name, entry->d_name) == 0) {
            info.entry_type = entry->d_type;
            info.ino = entry->d_ino;
            printf("found %s: type=%u, ino=%lu\n", entry->d_name, info.entry_type, info.ino);
            file_found = 1;
            break;
        }
    }
    if (!file_found) {
        send(new_socket, err_resp);
        return;
    }
    char info_json[512];
    create_entry_info_json(info, info_json);
    send(new_socket, info_json);
}

void create_request(int new_socket, ino_t parent_inode, char *name, int type) {
    printf("create...\n");
    ino_t new_inode;
    const char *start_dir = ".";
    char parent_name[PATH_MAX] = {'\0'};
    find_dir(parent_inode, start_dir, parent_name);
    if (strlen(parent_name) == 0) {
        printf("not found parent dir\n");
        send(new_socket, err_resp);
        return;
    }

    printf("going to create in dir %s\n", parent_name);

    struct stat st;
    char path[PATH_MAX + 2];
    snprintf(path, sizeof(path), "%s/%s", parent_name, name);

    if (type == 8) {
        int fd = open(path, O_CREAT | O_WRONLY, 0644);
        if (fd == -1) {
            perror("Error creating file");
            send(new_socket, err_resp);
            return;

        }
        close(fd);
        if (stat(path, &st) == -1) {
            perror("Error getting file status");
            send(new_socket, err_resp);
            return;

        }
        printf("Inode of new file: %lu\n", st.st_ino);


    } else {
        if (mkdir(path, 0755) == -1) {
            perror("Error creating directory");
            send(new_socket, err_resp);
            return;
        }
        if (stat(path, &st) == -1) {
            perror("Error getting directory status");
            send(new_socket, err_resp);
            return;
        }
        printf("Inode of new directory: %lu\n", st.st_ino);
    }
    new_inode = st.st_ino;
    char response[256];
    sprintf(response, "response{code:0,inode:%ld}", new_inode);
    send(new_socket, response);

}

void unlink_request(int new_socket, ino_t parent_inode, char *name){
    const char *start_dir = ".";
    char parent_name[PATH_MAX] = {'\0'};
    find_dir(parent_inode, start_dir, parent_name);
    if (strlen(parent_name) == 0) {
        printf("not found parent dir\n");
        send(new_socket, err_resp);
        return;
    }

    char path[PATH_MAX + 2];
    snprintf(path, sizeof(path), "%s/%s", parent_name, name);

    int status = unlink(path);
    if (status != 0) send(new_socket, err_resp);
    else send(new_socket, ok_resp);
}

void rmdir_request(int new_socket, ino_t parent_inode, char *name){
    const char *start_dir = ".";
    char parent_name[PATH_MAX] = {'\0'};
    find_dir(parent_inode, start_dir, parent_name);
    if (strlen(parent_name) == 0) {
        printf("not found parent dir\n");
        send(new_socket, err_resp);
        return;
    }

    char path[PATH_MAX + 2];
    snprintf(path, sizeof(path), "%s/%s", parent_name, name);

    int status = rmdir(path);
    if (status != 0) send(new_socket, err_resp);
    else send(new_socket, ok_resp);
}

void parse_request(const char *request, int new_socket) {
    char *str;
    char *token;
    const char *sep = "{},:[] \n";
    str = (char *) malloc(strlen(request) + 1); //TODO check ok malloc
    strcpy(str, request);
    token = strsep(&str, sep);

    //TODO optional: check key name request, ino so on

    while (token != NULL) {
        printf("first token is %s\n", token);
        if (strcmp(token, "method") == 0) {
            token = strsep(&str, sep);
            printf("method name: %s\n", token);

            /*
                request:{method:"list",inode:12345}
                */

            if (strcmp(token, "\"list\"") == 0) {
                token = strsep(&str, sep); //token = "inode"
                token = strsep(&str, sep); //token = inode_val
                printf("inode val %s\n", token);
                ino_t inode = strtoul(token, NULL, 10);
                printf("inode is %ld\n", inode);
                list_request(new_socket, inode);
                return;
            }

                /*
                  request:{method:"lookup",parent_inode:12345,name:"myfile.txt"}
                  */

                /*
                  request:{method:"unlink",parent_inode:12345,name:"myfile.txt"}
                  */

                /*
                  request:{method:"rmdir",parent_inode:12345,name:"mydir"}
                  */

                /*
                request:{method:"create",parent_inode:12345,name:"mynewfile.txt",type:8}
                */

            else if (strcmp(token, "\"lookup\"") == 0 ||
                     strcmp(token, "\"create\"") == 0 ||
                     strcmp(token, "\"unlink\"") == 0 ||
                     strcmp(token, "\"rmdir\"") == 0){
                char op_name[MAX_OP_LEN];
                strcpy(op_name, token);

                token = strsep(&str, sep); //token = "parent_inode"
                token = strsep(&str, sep); //token = parent_inode_val
                ino_t parent_ino = strtoul(token, NULL, 10);

                printf("parent inode is %ld\n", parent_ino);

                token = strsep(&str, sep); //token = "name"
                token = strsep(&str, sep); //token = name str

                char *file_name = (char *) malloc(sizeof(token) + 1);
                strncpy(file_name, token + 1, strlen(token) - 1);
                file_name[strlen(file_name) - 1] = '\0';
                printf("file name is %s\n", file_name);

                if (strcmp(op_name, "\"lookup\"") == 0) {
                    lookup_request(new_socket, parent_ino, file_name);
                    return;
                    free(file_name);
                }
                else if (strcmp(op_name, "\"unlink\"") == 0){
                    unlink_request(new_socket, parent_ino, file_name);
                    return;
                    free(file_name);
                }
                else if (strcmp(op_name, "\"rmdir\"") == 0){
                    rmdir_request(new_socket, parent_ino, file_name);
                    return;
                    free(file_name);
                }
                else {
                    token = strsep(&str, sep); //token = "type"
                    token = strsep(&str, sep); //token = type val
                    int type = strtoul(token, NULL, 10);
                    create_request(new_socket, parent_ino, file_name, type);
                    return;
                    free(file_name);
                }


            }

        }
        token = strsep(&str, sep);
    }
    printf("not parsed and found\n");
    send(new_socket, err_resp);
    free(str);
}


void start() {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t * ) & addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        valread = read(new_socket, buffer, 1024);
        printf("I have just recieved %s\n", buffer);
        parse_request(buffer, new_socket);
        buffer[1024] = {0};
        close(new_socket);
    }
}

int main(int argc, char const *argv[]) {
    start();
    //parse_request("request:{method:\"create\",parent_inode:2756147,name:\"dir1\",type:4}", 0);
    return 0;
}