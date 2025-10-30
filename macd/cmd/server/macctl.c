#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#define PORT 4466
#define BUF_SIZE 4096
#define DIR ".remotelink/bin"
#define LINK_DIR ".remotelink/files"

static const char* get_app(const char* p) {
    const char* s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static char* abspath(const char* p) {
    char* a = realpath(p, NULL);
    if (!a) { perror("路径错"); exit(1); }
    return a;
}

static void init_dir(char* d) {
    struct stat st;
    if (stat(d, &st) || !S_ISDIR(st.st_mode)) {
        if (mkdir(d, 0755) && errno != EEXIST) {
            perror("创建目录失败");
            exit(1);
        }
    }
}

static char* message(const char* command, int start, int argc, char* argv[]){

    char* msg = malloc(BUF_SIZE);
    int len = snprintf(msg, BUF_SIZE, "%s ", command);
    for (int i = start; i < argc; i++) {
        char* param = argv[i];
        char abs_path[PATH_MAX];
        struct stat st;
        if (stat(param, &st) == 0) {
            realpath(param, abs_path);
        }
        // 判断参数是什么类型的
        if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) {
            // 提取文件名或者目录名
            char link[PATH_MAX];
            char* fs_name = strrchr(param, '/') ? strrchr(param, '/') + 1 : param;
            len += snprintf(msg + len, BUF_SIZE - len, "FILE|%s ", fs_name);


            snprintf(link, PATH_MAX, "%s/%s/%s",getenv("HOME"), LINK_DIR, fs_name);
            symlink(abs_path, link);
            
        } else {
            // 其他
            len += snprintf(msg + len, BUF_SIZE - len, "%s",param);
        }  
    }
    msg[len++] = '\n';
    return strdup(msg);
}

static int conn() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {.sin_family=AF_INET, .sin_port=htons(PORT)};
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
        close(s);
        return -1;
    }
    return s;
}

// 显示帮助信息
static void show_help() {
    printf("用法: macctl <选项> [参数]\n");
    printf("支持的选项:\n");
    printf("  link <程序名称>    - 在~/.remotelink 创建链接\n");
    printf("  程序名称 [参数]    - 发送code命令到本地服务\n");
    printf("  -h                - 显示帮助信息\n");
}

int main(int argc, char* argv[]) {
    char command[BUF_SIZE];
    int start = 1;

    // 处理帮助选项
    if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        show_help();
        return 0;
    }

    strncpy(command, get_app(argv[0]), BUF_SIZE-1);

    // 处理link命令
    if (!strcmp(command, "macctl") && argc >= 2) {

        if (!strcmp(argv[1], "link")){
            if (argc < 3) { fprintf(stderr, "用法: macctl link <名称>\n"); return 1; }
            char dir[PATH_MAX], link[PATH_MAX], self[PATH_MAX];
            snprintf(dir, PATH_MAX, "%s/%s", getenv("HOME"), DIR);
            init_dir(dir);
            
            ssize_t len = readlink("/proc/self/exe", self, sizeof(self)-1);
            if (len == -1) { perror("获取程序路径失败"); return 1; }
            self[len] = '\0';
            
            char* abs = abspath(self);
            snprintf(link, PATH_MAX, "%s/%s", dir, argv[2]);
            if (symlink(abs, link) < 0) { perror("创建链接失败"); return 1; }

            return 0;

        }else{
            strncpy(command, argv[1], BUF_SIZE-1);
            start = 2;
        }
    }

    char* msg = message(command, start, argc, argv);

    // 网络发送
    int sock = conn();

    if (sock < 0) { perror("连接失败"); return 1; }
    
    if (send(sock, msg, strlen(msg), 0) < 0) {
        fprintf(stderr, "command not found: %s\n", command);
        close(sock);
        return 1;
    }

    close(sock);
    return 0;
}