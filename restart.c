#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/socket.h> 
#include <sys/inotify.h>
#include <errno.h>   
#include <string.h>    
#include <limits.h>

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif


char serverdir[1024];
typedef struct {
    int type;
    int msg_size;
    unsigned long file_size;
    int name_size;
    char name[256];
    char message[1024]; 
    char *file_content;
} task_t;

typedef struct {
	task_t *array;
	int size;
} task_array;

typedef struct {
	char path[1024];
	int wd;
} wd_list;


#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))


void help();


void receive_file(int sock, char *file, int size);
void send_file(int sock, char *filename, int size);
void r_w_sock(int sock, int type, char *name, char *msg, char *clientdir, char *serverdir);
int delete_dir(const char *dirname);
void get_path_inofy(char *path, struct inotify_event *intf);
void get_all_dir (char *path);
int watch_client(char *path, int sock, char *serverdir);
static void displayInotifyEvent(struct inotify_event *i, int sock, char *path, char *serverdir);
void receive_dir(int sock);
void send_dir(int sock, char *path, int flag, const char *main_path, char *serverdir);
void get_client_path(char* path_s, char* path_c, int size);
int read_file(task_t *atask);
int write_file(task_t *atask);
unsigned long get_size_file(const char* filename);
void getContentOfFile(const char* filename, char* buffer);
void writeFile(const char* filename, char* buffer);
struct timeval add2currenttime(double seconds);
int copyfile(int fromfd, int tofd);
int r_close(int fildes);
int r_dup2(int fildes, int fildes2);
int r_open2(const char *path, int oflag);
int r_open3(const char *path, int oflag, mode_t mode);
ssize_t r_read(int fd, void *buf, size_t size);
pid_t r_wait(int *stat_loc);
pid_t r_waitpid(pid_t pid, int *stat_loc, int options);
ssize_t r_write(int fd, void *buf, size_t size);
ssize_t readblock(int fd, void *buf, size_t size);
int readline(int fd, char *buf, int nbytes);
ssize_t readtimed(int fd, void *buf, size_t nbyte, double seconds);
int readwrite(int fromfd, int tofd);
int readwriteblock(int fromfd, int tofd, char *buf, int size);
int waitfdtimed(int fd, struct timeval end);
int truncate_file(const char* filename);
void stdout_file(const char* filename);
int get_number_of_line(const char* filename);
//ssize_t r_write(int fd, void *buf, size_t size);
int append_file(const char* filename, char* buffer);
int get_total_with_z(const char* filename, int parent_pid);
void get_nth_line(const char* filename, int nth, char* line);
