#include "tcp_client.h"
#include <linux/slab.h>
#include <linux/string.h>

struct socket *sock;
const char *sep = "{},:[] \n";

int parse_code(const char *input_str) {
    char *token;
    char *str;
    int code;
    str = kmalloc(strlen(input_str) + 1, GFP_KERNEL);
    strcpy(str, input_str);
    token = strsep(&str, sep);
    while (token != NULL) {
        if (strcmp(token, "code") == 0) {
            token = strsep(&str, sep);
            code = (int) simple_strtoul(token, NULL, 10);
            printk("response code is %d\n", code);
            if (code != 0) {
                kfree(str);
                return code;
            }
        }
        token = strsep(&str, sep);
    }

    kfree(str);
    return 0;
}

int parse_create_response(const char *input_str, ino_t *result) {
    int code;
    char *token;
    char *str;
    code = parse_code(input_str);
    if (code != 0) return code;


    str = kmalloc(strlen(input_str) + 1, GFP_KERNEL);
    strcpy(str, input_str);
    token = strsep(&str, sep);
    while (token != NULL) {
        if (strcmp(token, "inode") == 0) {
            token = strsep(&str, sep); //token = inode val
            *result = (size_t) simple_strtoul(token, NULL, 10);
            printk("inode is %ld\n", *result);
        }
        token = strsep(&str, sep);
    }

    kfree(str);
    return 0;

}

int parse_entry_info(const char *input_str, struct entry_info *result) {
    char *token;
    char *str;
    int code;
    code = parse_code(input_str);
    if (code != 0) return code;

    str = kmalloc(strlen(input_str) + 1, GFP_KERNEL);
    strcpy(str, input_str);
    token = strsep(&str, "{},:[] \n");
    while (token != NULL) {
        if (strcmp(token, "entry_type") == 0) {
            token = strsep(&str, "{},:[] \n");
            result->entry_type = (size_t) simple_strtoul(token, NULL, 10);
            printk("entry_type %d\n", result->entry_type);

            token = strsep(&str, "{},:[] \n"); //ino
            token = strsep(&str, "{},:[] \n"); //ino-val
            result->ino = (size_t) simple_strtoul(token, NULL, 10);
            printk("ino %ld\n", result->ino);
        }
        token = strsep(&str, "{},:[] \n");
    }

    kfree(str);
    return 0;
}


int parse_entries(const char *input_str, struct entries *result) {
    //TODO: it will return 0 if code=0 but ni info. Fix it (optionnaly)
    char *token;
    char *str;
    int i = 0, cnt = MAX_ENTRIES;
    int code;
    code = parse_code(input_str);
    if (code != 0) return code;

    str = kmalloc(strlen(input_str) + 1, GFP_KERNEL);
    strcpy(str, input_str);


    token = strsep(&str, "{},:[] \n");
    while (token != NULL) {
        if (strcmp(token, "entries_count") == 0) {
            token = strsep(&str, "{},:[] \n");
            result->entries_count = (size_t) simple_strtoul(token, NULL, 10);
            cnt = result->entries_count;

        } else if (strcmp(token, "type") == 0) {
            token = strsep(&str, "{},:[] \n");
            result->entries[i].entry_type = (unsigned char) simple_strtoul(token, NULL, 10);

        } else if (strcmp(token, "ino") == 0) {
            token = strsep(&str, "{},:[] \n");
            result->entries[i].ino = (ino_t) simple_strtoul(token, NULL, 10);

        } else if (strcmp(token, "name") == 0) {
            token = strsep(&str, "{},:[] \n");
            strncpy(result->entries[i].name, token + 1, strlen(token) - 2);
            result->entries[i].name[MAX_NAME_LENGTH - 1] = '\0'; // Ensure null termination

            i++;
            if (i == cnt) break;
        }
        token = strsep(&str, "{},:[] \n");
    }

    kfree(str);
    return 0;
}

int recieve_response(char *response) {

    int received;
    int total_received;
    char *recvbuf = NULL;
    size_t sz = 4096;
    recvbuf = kmalloc(sz, GFP_KERNEL);
    if (recvbuf == NULL) {
        printk("client: recvbuf kmalloc error!");
        return -1;
    }

    memset(recvbuf, 0, sz);

    total_received = 0;
    received = 0;
    while (total_received < sz) {
        struct kvec vec;
        struct msghdr msg;
        memset(&vec, 0, sizeof(vec));
        memset(&msg, 0, sizeof(msg));
        vec.iov_base = recvbuf + total_received;
        vec.iov_len = sz - total_received;
        received = kernel_recvmsg(sock, &msg, &vec, 1, vec.iov_len, MSG_WAITALL);
        if (received < 0) {
            printk("client: kernel_recvmsg error!");
            kfree(recvbuf);
            return received;
        }
        total_received += received;
        if (received == 0) {
            break;
        }
        printk("client: received %s\n", recvbuf);
        strcpy(response, recvbuf); //TODO check this cycle

    }


    // Free the memory allocated for recvbuf
    kfree(recvbuf);
    return 0;

}


int send_message(char *message) {
    //send message
    printk("try to send to server %s\n", message);
    int ret;
    struct kvec vec;
    struct msghdr msg;

    vec.iov_base = message;
    vec.iov_len = strlen(message);

    memset(&msg, 0, sizeof(msg));


    ret = kernel_sendmsg(sock, &msg, &vec, 1, strlen(message));
    if (ret < 0) {
        printk("client: kernel_sendmsg error!");
        return ret;
    } else if (ret != strlen(message)) {
        printk("client: ret!=strlen(message)");
    }
    printk("client:send ok!");
    return 0;
}

/*
	initialise tcp connection
*/


int init_serv_connection(void) {
    /*create socket*/
    int err;
    int portnum;
    int ret;
    struct sockaddr_in s_addr;

    err = sock_create_kern(&init_net, PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (err < 0) {
        printk("socket not created\n");
        return -1;
    }
    printk("socket is created\n");

    /*connect to server*/


    portnum = 8080;
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(portnum);
    s_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);


    ret = sock->ops->connect(sock, (struct sockaddr *) &s_addr, sizeof(s_addr), 0);
    if (ret != 0) {
        printk("client:connect error!\n");
        return -1;
    }
    printk("client:connect ok!\n");
    return 0;
}

int make_request(char *request, char *response) {
    int ret;
    //connect to server
    ret = init_serv_connection();
    if (ret != 0) {
        printk("can not connect to server\n");
        return ret;
    }
    //send request
    ret = send_message(request);
    if (ret != 0) {
        printk("can not send message\n");
        return ret;
    }

    //get response

    ret = recieve_response(response);
    if (ret != 0) {
        printk("can not recieve response\n");
        return ret;
    }
    return 0;
}

int list_call(ino_t inode, struct entries *result) { //TODO (remove dublication request-response)
    int ret, code;
    int i;
    char request[256];
    char response[4096]; //TODO size?
    struct entries res_entries;

    //create request
    sprintf(request, "request:{method:\"list\",inode:%ld}", inode);
    ret = make_request(request, response);
    if (ret != 0) return ret;

    code = parse_entries(response, &res_entries);
    printk("request returned %d code\n", code);
    if (code != 0) return code;

    result->entries_count = res_entries.entries_count;

    i = 0;
    while (i < res_entries.entries_count) {
        struct entry en = res_entries.entries[i];
        printk("type:%d,ino:%ld,name:%s\n", en.entry_type, en.ino, en.name);
        result->entries[i].entry_type = en.entry_type;
        result->entries[i].ino = en.ino;
        strcpy(result->entries[i].name, en.name);
        i++;
    }

    sock_release(sock);
    return 0;
}

int lookup_call(ino_t parent_inode, char *name, struct entry_info *result) {

    int ret, code;
    char request[256];
    char response[4096]; //TODO size?
    struct entry_info info;

    //create request
    sprintf(request, "request:{method:\"lookup\",parent_inode:%ld,name:\"%s\"}", parent_inode, name);
    ret = make_request(request, response);
    if (ret != 0) return ret;

    code = parse_entry_info(response, &info);
    if (code != 0) return code;
    printk("type: %d, inode: %ld\n", info.entry_type, info.ino);
    result->entry_type = info.entry_type;
    result->ino = info.ino;

    sock_release(sock);
    return 0;
}

int create_call(ino_t parent_inode, char *name, int type, ino_t *result) {
    int ret, code;
    ino_t new_inode;
    char request[256];
    char response[256];

    //create request
    sprintf(request, "request:{method:\"create\",parent_inode:%ld,name:\"%s\",type:%d}", parent_inode, name, type);
    ret = make_request(request, response);
    if (ret != 0) return ret;
    code = parse_create_response(response, &new_inode);
    if (code != 0) return code;
    *result = new_inode;
    printk("new inode is %ld\n", *result);
    sock_release(sock);
    return 0;
}

int unlink_call(ino_t parent_inode, char *name){
    int ret, code;
    char request[256];
    char response[256];
    //create request
    sprintf(request, "request:{method:\"unlink\",parent_inode:%ld,name:\"%s\"}", parent_inode, name);
    ret = make_request(request, response);
    if (ret != 0) return ret;
    code = parse_code(response);
    if (code != 0) return code;
    sock_release(sock);
    return 0;
}


void sock_release(struct socket *sock) {
    if (sock->ops) {
        struct module *owner = sock->ops->owner;

        sock->ops->release(sock);
        sock->ops = NULL;
        module_put(owner);
    }
    printk("socket is released\n");
}