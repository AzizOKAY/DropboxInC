#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdio.h>
#include "restart.h"
#define BLKSIZE PIPE_BUF
#define MILLION 1000000L
#define D_MILLION 1000000.0
wd_list allDir[1024];
int dir_count = 0;

/* Private functions */



void help() {
	fprintf(stdout, "\n*******************************************************\n\n");
	fprintf(stdout, "*****************       BibakBOX          *************\n");
	fprintf(stdout, "*                                                     *\n");
	fprintf(stdout, "*  This program work as as dropbox                    *\n");
	fprintf(stdout, "*  First save missing file to client from server      *\n");
	fprintf(stdout, "*  Then save missing file to server from client       *\n");
	fprintf(stdout, "*  And whatch client dir and tell server about change *\n");
	fprintf(stdout, "*  For running client :                               *\n");
	fprintf(stdout, "*  BibakBOXClient [dirName] [ip address] [portnumber] *\n");
	fprintf(stdout, "*  For running server :                               *\n");
	fprintf(stdout, "*  BibakBOXServer [dirName] [th_PoolSize] [portnumber]*\n");
	fprintf(stdout, "*                                                     *\n");
	fprintf(stdout, "*                       BibakBOX                      *\n");
	fprintf(stdout, "*******************************************************\n\n");
}

void get_all_dir (char *path) {
	int total = 0, cpid, temp = 0;
	struct stat st;
	struct dirent *entry;
	DIR *dp;
	
	dp = opendir(path);
	if (NULL == dp) {
		fprintf(stderr, "Can not open the given directory %s", path);
		exit(1);
	}	
	else {
		fprintf(stdout, "name of dir:%s.\n", path);
		sprintf(allDir[dir_count].path, "%s", path);
		dir_count++; 
		while((entry = readdir(dp))) {
			if ( strcmp(entry->d_name, "." ) != 0  && strcmp(entry->d_name, ".." )  != 0 ) {
				char new_dir[strlen(path) + strlen(entry->d_name) + 2];
				snprintf(new_dir, sizeof(new_dir), "%s%s%s", path, "/", entry->d_name);
				if (stat(new_dir, &st) == 0 && S_ISDIR(st.st_mode)) {//it is a directory
					get_all_dir (new_dir);
				}
			}
		}
		closedir(dp);
	}
}

void receive_dir(int sock) {
	task_t buffer, input;
	int valread;
	while(1) {
		sprintf(input.message, "%s%d", " I will write server my pid ", getpid());
		fflush(stdout);
		//if (input.message[0] == 'n')
			//break;
		fprintf(stdout, "[client]:input is : %s.\n", input.message);
		input.msg_size = strlen(input.message);
		send(sock , &input , sizeof(input) , 0 );
		
		valread = read( sock , &buffer, sizeof(buffer)); 
		buffer.message[buffer.msg_size] = '\0';
		
		if (buffer.type == -1) break;
		if (buffer.type == 1) {
			fprintf(stdout, "type is 1.\n");
			fprintf(stdout, "name of file is %s.\n", buffer.name);
			if( access( buffer.name, F_OK ) != -1 ) {
    			printf("file exist.\n");
			}
			else {
    			write_file(&buffer);
			}
			//fprintf(stdout, "%s....\n", buffer.file_content);
			
		}
		else if (buffer.type == 0) {
			fprintf(stdout, "type is 0.\n");
			fprintf(stdout, "name of directory is %s.\n", buffer.name);
			
			struct stat sb;

    		if (stat(buffer.name, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        		printf("Directory already exist.\n");
    		}
    		else {
        		printf("Directory does not exist.\n");
        		mkdir(buffer.name, 0777);
    		}
    
			
			memset(buffer.name, 0, strlen(buffer.name));
		}
		else {
			fprintf(stdout, "type are not 0 or not 1 and not -1.\n");
		}
		fprintf(stdout, "receive send : %s\n",buffer.message );
		memset(buffer.message, 0, strlen(buffer.message));  
	}
}



void send_dir(int sock, char *path, int flag, const char *main_path, char *serverdir) {
	
	char path_c[1024];
	int total = 0, cpid, temp = 0;
	struct stat st;
	struct dirent *entry;
	DIR *dp;
	
	dp = opendir(path);
	if (NULL == dp) {
		fprintf(stderr, "Can not open the given directory %s", path);
		exit(1);
	}	
	else {
		fprintf(stdout, "name of dir:%s.\n", path);
		//send message to client to create dir
		task_t taskdir;
		
		fprintf(stdout, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAA: path is %s.\n", path);
		if (flag == 0) {
			get_client_path(path, path_c, strlen(path));
			fprintf(stdout, "path c is %s.\n", path_c);
		}
		else {
			sprintf(path_c, "%s/%s", serverdir, path);
			fprintf(stdout, "path c is %s.\n", path_c);
		}	
			
		sprintf(taskdir.name, "%s", path_c);
		sprintf(taskdir.message, "type is 0 and name of dir %s.\n", taskdir.name); 
		taskdir.msg_size = strlen(taskdir.message);
		taskdir.type = 0;
		write(sock , &taskdir , sizeof(taskdir));
		bzero(&taskdir, sizeof(taskdir));
		read( sock , &taskdir, sizeof(taskdir));
		bzero(&taskdir, sizeof(taskdir));
		while((entry = readdir(dp))) {
			if ( strcmp(entry->d_name, "." ) != 0  && strcmp(entry->d_name, ".." )  != 0 ) {
				char new_dir[strlen(path) + strlen(entry->d_name) + 2];
				snprintf(new_dir, sizeof(new_dir), "%s%s%s", path, "/", entry->d_name);
				if (stat(new_dir, &st) == 0 && S_ISDIR(st.st_mode)) {//it is a directory
					send_dir (sock, new_dir, flag, main_path, serverdir);
				}
				else {
					task_t tasko;
					fprintf(stdout, "name of file:%s.\n", new_dir);
					sprintf(tasko.name, "%s", new_dir); 
					sprintf(tasko.message, "type is 1 and name of file %s.\n", tasko.name); 
					tasko.msg_size = strlen(tasko.message);
					tasko.type = 1;
					read_file(&tasko);
					if (flag == 0) {
						get_client_path(new_dir, path_c, strlen(path));
						fprintf(stdout, "path c is %s.\n", path_c);
					}	
					else
						sprintf(path_c, "%s%s", "serverdir/", new_dir);
					
						
					sprintf(tasko.name, "%s", path_c);
					//fprintf(stdout, "%s....\n", tasko.file_content);
					write(sock , &tasko , sizeof(tasko));
					bzero(&taskdir, sizeof(taskdir));
					read( sock , &taskdir, sizeof(taskdir));
					bzero(&taskdir, sizeof(taskdir));
				}
			}
		}
		fprintf(stdout, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA %s.\n", main_path);
		if (strcmp(main_path, path) == 0) {
			
			taskdir.type = -1;
			write(sock , &taskdir , sizeof(taskdir));
		}	
		closedir(dp);
	}
	
}


int watch_client(char *path, int sock, char *serverdir) {
	int inotifyFd, wd, j;
     char buf[BUF_LEN] __attribute__ ((aligned(8)));
     ssize_t numRead;
     char *p;
     struct inotify_event *event;
 
 
     inotifyFd = inotify_init();                 /* Create inotify instance */
     if (inotifyFd == -1)
         perror("inotify_init");
 
    /* For each command-line argument, add a watch for all events */
    
     fprintf(stdout, "All dir in given dir ... \n");
	 get_all_dir (path);
	 
	 
     for (j = 0; j < dir_count; j++) {
         wd = inotify_add_watch(inotifyFd, allDir[j].path, IN_ALL_EVENTS);
         allDir[j].wd = wd;
         if (wd == -1) {
             perror("inotify_add_watch");
             return -1;
         }    
 
         printf("Watching %s using wd %d\n", allDir[j].path, wd);
     }
 
     for (;;) {                                  /* Read events forever */
         numRead = read(inotifyFd, buf, BUF_LEN);
         if (numRead == 0)
             perror("read() from inotify fd returned 0!");
 
         if (numRead == -1) {
             perror("read");
             return -1;
         }    
 
         //printf("Read %ld bytes from inotify fd\n", (long) numRead);
 
         /* Process all of the events in buffer returned by read() */
         
 
         for (p = buf; p < buf + numRead; ) {
             event = (struct inotify_event *) p;
             displayInotifyEvent(event, sock, path, serverdir);
 
             p += sizeof(struct inotify_event) + event->len;
         }
     }
}


void get_path_inofy(char *path, struct inotify_event *intf) {
	int i = 0;
	for (i = 0; i < dir_count; i++) {
		if (allDir[i].wd == intf->wd) {
			if (intf->len > 0) {
				sprintf(path, "%s/%s", allDir[i].path, intf->name);
				return;
			}
			else {
				sprintf(path, "%s", allDir[i].path);
				return;
			}	
		}	
	}
}



// This part of code caming from THE LINIX PROGRAMMING INTERFACE BOOK 
// PAGE : http://man7.org/tlpi/code/online/diff/inotify/demo_inotify.c.html
 static void     /* Display information from inotify_event structure */
displayInotifyEvent(struct inotify_event *i, int sock, char *clientdir, char *serverdir)
{
	int rename = 1;
	char buflog[2048], clientlog[1024];
	sprintf(clientlog, "%s.log", clientdir);
    //printf("    wd =%2d; ", i->wd);
    //if (i->cookie > 0)
        //printf("cookie =%4d; ", i->cookie);
	char path[1024];
    //printf("mask = ");
    /*if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
    if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
    if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
    if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
    if (i->mask & IN_CREATE)        printf("IN_CREATE ");
    if (i->mask & IN_DELETE)        printf("IN_DELETE ");
    if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
    if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
    if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");*/
    
    if (i->mask & IN_DELETE) {
    	get_path_inofy(path, i);
    	printf("IN_DELETE wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
    	sprintf(buflog, "IN_DELETE wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
		append_file(clientlog, buflog);
    }	
    if (i->mask & IN_DELETE_SELF) {
    	get_path_inofy(path, i);
    	printf("IN_DELETE_SELF wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
    	sprintf(buflog, "IN_DELETE_SELF wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
		append_file(clientlog, buflog);
    }
    if (i->mask & IN_MODIFY) {
    	get_path_inofy(path, i);
    	printf("IN_MODIFY wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
    	r_w_sock(sock, 3, path, "IN_MODIFY", clientdir, serverdir);
    	sprintf(buflog, "IN_MODIFY wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
		append_file(clientlog, buflog);
    }	
    if (i->mask & IN_MOVE_SELF) {
		get_path_inofy(path, i);
		printf("IN_MOVE_SELF wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
		sprintf(buflog, "IN_MOVE_SELF wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
		append_file(clientlog, buflog);
    }     
    if (i->mask & IN_MOVED_FROM) {
    	get_path_inofy(path, i);
    	printf("IN_MOVED_FROM wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
    	r_w_sock(sock, 2, path, "IN_MOVED_FROM", clientdir, serverdir);
    	sprintf(buflog, "IN_MOVED_FROM wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
		append_file(clientlog, buflog);
    }	
    if (i->mask & IN_MOVED_TO) {
    	get_path_inofy(path, i);
    	printf("IN_MOVED_TO wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
    	r_w_sock(sock, 3, path, "IN_MOVED_TO", clientdir, serverdir);
    	sprintf(buflog, "IN_MOVED_TO wd = %d, name = %s and path is %s.\n", i->wd, i->name, path);
		append_file(clientlog, buflog);
    	
    		
    }	
    /*if (i->mask & IN_OPEN)          printf("IN_OPEN ");
    if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
    if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");*/
    //printf("\n");

    //if (i->len > 0)
        //printf("wd = %d, name = %s\n", i->wd, i->name);
}


void r_w_sock(int sock, int type, char *path, char *msg, char *clientdir, char *serverdir) {
	task_t taskdir;
	sprintf(taskdir.name, "%s", path);
	sprintf(taskdir.message, "%s %s.\n", msg, taskdir.name); 
	taskdir.msg_size = strlen(taskdir.message);
	taskdir.type = type;
	
	if (type == 3) {
		struct stat sb;

		if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    		send_dir(sock, clientdir, 1, clientdir, serverdir);
    		return;
		}
		else {
    		read_file(&taskdir);
		}
	}
	
	
	write(sock , &taskdir , sizeof(taskdir));
	bzero(&taskdir, sizeof(taskdir));
	
	//read( sock , &taskdir, sizeof(taskdir));
	//send_file(sock, "nanany", 1024);
	//read( sock , &taskdir, sizeof(taskdir));
	//send_file(sock, "ds", 1024);
	fprintf(stdout, "[client]: server receive file : %s and type is %d.\n", taskdir.message, taskdir.type);
	if (taskdir.type == 2222) {
		fprintf(stdout, "Server died. last action has not done.\n");
		bzero(&taskdir, sizeof(taskdir));
		exit(1); 
	}
	bzero(&taskdir, sizeof(taskdir));
}


void receive_file(int sock, char *file, int size) {
	char str[1024];
	int readval = read(sock, str, 1024);
	str[readval] = '\0';
	printf("I receive file: %s.\n", str);
	write(sock, "This IS receive send.", strlen("This IS receive send."));
}
void send_file(int sock, char *filename, int size) {
	int fd = r_open2(filename, O_RDONLY), sized = get_size_file(filename), byteread = 0;
	
	int bytes = 0;
	char str[1024];
	printf("DENDENDENDENDEN.\n");
	while (size > bytes) {
		bytes += read(fd, str, sizeof(str));
		write(sock, str, sizeof(str));
		printf("%s", str);
		memset(str, 0, 1024);
	}
	printf("...\n");
	write(sock, "9999", strlen("9999"));
	
}


int delete_dir(const char *dirname) {
	DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];

    if (path == NULL) {
        fprintf(stderr, "Out of memory error\n");
        return 0;
    }
    dir = opendir(dirname);
    if (dir == NULL) {
    	if (remove(dirname) != 0) {
        	perror("Error opendir()");
        	return 0;
        }
        else {
        	printf("%s has been removed.\n", dirname);
        	return 1;
        }
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            snprintf(path, (size_t) PATH_MAX, "%s/%s", dirname, entry->d_name);
            if (entry->d_type == DT_DIR) {
                delete_dir(path);
            }

            /*
             * Here, the actual deletion must be done.  Beacuse this is
             * quite a dangerous thing to do, and this program is not very
             * well tested, we are just printing as if we are deleting.
             */
            printf("Deleting: %s\n", path);
            /*
             * When you are finished testing this and feel you are ready to do the real
             */
              remove(path);
             /* (see "man 3 remove")
             * Please note that I DONT TAKE RESPONSIBILITY for data you delete with this!
             */
        }

    }
    closedir(dir);

    /*
     * Now the directory is emtpy, finally delete the directory itself. (Just
     * printing here, see above) 
     */
     
    //unlink(dirname);
    remove(dirname);
	printf("Deleting: %s\n", dirname);

    return 1;
	
}


void get_client_path(char* path_s, char* path_c, int size) {
	int i = 0;
	while (path_s[i] != '/') {
		i++;
	}
	i++;
	sprintf(path_c, "%s", &path_s[i]);
}


int read_file(task_t *atask) {
	unsigned long size = get_size_file(atask->name);
	int fd, res = 0;
	if (size > 65500)
		atask->file_size = 65500;
	else
		atask->file_size = size;
		
	fprintf(stdout, "file szie %ld.", size);
	atask->file_content = (char*) malloc(size * sizeof(char));
	
	fd = r_open3(atask->name, O_RDONLY, 777);
	res = readblock(fd, atask->file_content, atask->file_size);
	r_close(fd);
	return res;
}


int write_file(task_t *atask) {
	int fd = open(atask->name, O_CREAT | O_WRONLY, 777);
	int res = r_write(fd, atask->file_content, atask->file_size);
	return res;
}


unsigned long get_size_file(const char* filename) {
	FILE *file;
	unsigned long fileLen;

	//Open file
	file = fopen(filename, "rb");
	if (!file) {
		fprintf(stderr, "Unable to open file %s", filename);
		return -1;
	}
	
	//Get file length
	fseek(file, 0, SEEK_END);
	fileLen=ftell(file);
	fseek(file, 0, SEEK_SET);

	return fileLen;
}


void getContentOfFile(const char* filename, char* buffer) {
	FILE *file;
	unsigned long fileLen;

	//Open file
	file = fopen(filename, "rb");
	if (!file) {
		fprintf(stderr, "Unable to open file %s", filename);
		return;
	}
	
	//Get file length
	fseek(file, 0, SEEK_END);
	fileLen=ftell(file);
	fseek(file, 0, SEEK_SET);

	fprintf(stdout, "file size is %ld.\n", fileLen);

	//Read file contents into buffer
	fread(buffer, fileLen, 1, file);
	writeFile("client.txt", buffer);
	fclose(file);
}


void writeFile(const char* filename, char* buffer) {
	FILE *file;
	fprintf(stdout, "...This is buffer : \n %s\n\n\n\n\n\n\n\n\n\n", buffer);
	//Open file
	file = fopen(filename, "wb");
	if (!file) {
		fprintf(stderr, "Unable to open file %s", filename);
		return;
	}

	//Read file contents into buffer
	fwrite (buffer , sizeof(buffer), sizeof(char), file);
	
	fclose(file);
}

int get_total_with_z(const char* filename, int parent_pid) {
	int fd = open(filename, O_RDONLY), i = 0;
	int ppid, pid, total, result = 0;
	size_t bytes_read;
	char ch, line[1024], path[1024];
	if (fd == -1) {
		perror("Error : while Open");
	}
	
	do {
		bytes_read = read(fd, &ch, sizeof(ch));	
		line[i] = ch;
		i++;
		if (ch == '\n') {
			line[i-1] = '\0';
			sscanf( line, "%d%d%d%s", &ppid, &pid, &total, path );
			if (parent_pid == ppid) {
				result += total + get_total_with_z(filename, pid);
			}
			i = 0;
		}
		
	} while(bytes_read == sizeof(ch));
	
	if ( close(fd) == -1)
		perror("Error : while Close file .");
	
	return result;
}
/***
*
*@param
*
*/
void stdout_file(const char* filename) {
	int fd = open(filename, O_RDONLY);
	size_t bytes_read;
	char ch;
	if (fd == -1) {
		perror("Error : while Open");
	}
	
	do {
		bytes_read = read(fd, &ch, sizeof(ch));	
		fprintf(stdout, "%c", ch);
	} while(bytes_read == sizeof(ch));
	
	if ( close(fd) == -1)
		perror("Error : while Close file .");
}

/***
*
*@param
*@param
*/
void get_nth_line(const char* filename, int nth, char* line) {
	int fd = open(filename, O_RDONLY);
	int number_of_line = 0, i = 0;
	size_t bytes_read;
	char ch;
	if (fd == -1) {
		perror("Error : while Open");
	}
	
	do {
		bytes_read = read(fd, &ch, sizeof(ch));
		line[i] = ch;
		i++;	
		if (ch == '\n') {
			
			if (number_of_line == nth) {
				line[i - 1] = '\0';
				break;
			}	
			else i = 0;
			number_of_line = number_of_line + 1;
		}	
	} while(bytes_read == sizeof(ch));
	
	if ( close(fd) == -1)
		perror("Error : while Close file .");
}

/***
*
*
*@param
*/
int get_number_of_line(const char* filename) {
	int fd = open(filename, O_RDONLY);
	int number_of_line = 0;
	size_t bytes_read;
	char ch;
	if (fd == -1) {
		perror("Error : while Open");
	}
	
	do {
		bytes_read = read(fd, &ch, sizeof(ch));	
		if (ch == '\n')
			number_of_line = number_of_line + 1;
	} while(bytes_read == sizeof(ch));
	
	if ( close(fd) == -1)
		perror("Error : while Close file .");
	return number_of_line - 1;
}


/***
*
*@param
*@param filename
*/
int append_file(const char* filename, char* buffer) {
	int res = 0;
	struct flock lock;
	int fd = open(filename, O_NONBLOCK | O_CREAT | O_WRONLY | O_APPEND, 0666);
	
	if (fd == -1) {
		perror("Error : while Open");
	}
	memset (&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	fcntl (fd, F_SETLKW, &lock);
	
	r_write(fd, buffer, strlen(buffer));
	res = r_write(fd, "\n", 1);
	lock.l_type = F_UNLCK;
 	fcntl (fd, F_SETLKW, &lock);
	if ( close(fd) == -1)
		perror("Error : while Close file .");
	return res;
}


int truncate_file(const char *name) {
    int fd;
    fd = open (name, O_TRUNC | O_WRONLY);
    if ( fd >= 0 ) 
        close(fd);
    return fd;
}



static int gettimeout(struct timeval end,
                               struct timeval *timeoutp) {
   gettimeofday(timeoutp, NULL);
   timeoutp->tv_sec = end.tv_sec - timeoutp->tv_sec;
   timeoutp->tv_usec = end.tv_usec - timeoutp->tv_usec;
   if (timeoutp->tv_usec >= MILLION) {
      timeoutp->tv_sec++;
      timeoutp->tv_usec -= MILLION;
   }
   if (timeoutp->tv_usec < 0) {
      timeoutp->tv_sec--;
      timeoutp->tv_usec += MILLION;
   }
   if ((timeoutp->tv_sec < 0) ||
       ((timeoutp->tv_sec == 0) && (timeoutp->tv_usec == 0))) {
      errno = ETIME;
      return -1;
   }
   return 0;
}

/* Restart versions of traditional functions */

int r_close(int fildes) {
   int retval;
   while (retval = close(fildes), retval == -1 && errno == EINTR) ;
   return retval;
}

int r_dup2(int fildes, int fildes2) {
   int retval;
   while (retval = dup2(fildes, fildes2), retval == -1 && errno == EINTR) ;
   return retval;
}


int r_open2(const char *path, int oflag) {
   int retval;
   while (retval = open(path, oflag), retval == -1 && errno == EINTR) ;
   return retval;
}

int r_open3(const char *path, int oflag, mode_t mode) {
   int retval;
   while (retval = open(path, oflag, mode), retval == -1 && errno == EINTR) ;
   return retval;
}

ssize_t r_read(int fd, void *buf, size_t size) {
   ssize_t retval;
   while (retval = read(fd, buf, size), retval == -1 && errno == EINTR) ;
   return retval;
}

pid_t r_wait(int *stat_loc) {
   pid_t retval;
   while (((retval = wait(stat_loc)) == -1) && (errno == EINTR)) ;
   return retval;
}

pid_t r_waitpid(pid_t pid, int *stat_loc, int options) {
   pid_t retval;
   while (((retval = waitpid(pid, stat_loc, options)) == -1) &&
           (errno == EINTR)) ;
   return retval;
}

ssize_t r_write(int fd, void *buf, size_t size) {
   char *bufp;
   size_t bytestowrite;
   ssize_t byteswritten;
   size_t totalbytes;

   for (bufp = buf, bytestowrite = size, totalbytes = 0;
        bytestowrite > 0;
        bufp += byteswritten, bytestowrite -= byteswritten) {
      byteswritten = write(fd, bufp, bytestowrite);
      if ((byteswritten) == -1 && (errno != EINTR))
         return -1;
      if (byteswritten == -1)
         byteswritten = 0;
      totalbytes += byteswritten;
   }
   return totalbytes;
}

/* Utility functions */

struct timeval add2currenttime(double seconds) {
   struct timeval newtime;

   gettimeofday(&newtime, NULL);
   newtime.tv_sec += (int)seconds;
   newtime.tv_usec += (int)((seconds - (int)seconds)*D_MILLION + 0.5);
   if (newtime.tv_usec >= MILLION) {
      newtime.tv_sec++;
      newtime.tv_usec -= MILLION;
   }
   return newtime;
}

int copyfile(int fromfd, int tofd) {
   int bytesread;
   int totalbytes = 0;

   while ((bytesread = readwrite(fromfd, tofd)) > 0)
      totalbytes += bytesread;
   return totalbytes;
}

ssize_t readblock(int fd, void *buf, size_t size) {
   char *bufp;
   ssize_t bytesread;
   size_t bytestoread;
   size_t totalbytes;
 
   for (bufp = buf, bytestoread = size, totalbytes = 0;
        bytestoread > 0;
        bufp += bytesread, bytestoread -= bytesread) {
      bytesread = read(fd, bufp, bytestoread);
      if ((bytesread == 0) && (totalbytes == 0))
         return 0;
      if (bytesread == 0) {
         errno = EINVAL;
         return -1;
      }  
      if ((bytesread) == -1 && (errno != EINTR))
         return -1;
      if (bytesread == -1)
         bytesread = 0;
      totalbytes += bytesread;
   }
   return totalbytes;
}

int readline(int fd, char *buf, int nbytes) {
   int numread = 0;
   int returnval;

   while (numread < nbytes - 1) {
      returnval = read(fd, buf + numread, 1);
      if ((returnval == -1) && (errno == EINTR))
         continue;
      if ((returnval == 0) && (numread == 0))
         return 0;
      if (returnval == 0)
         break;
      if (returnval == -1)
         return -1;
      numread++;
      if (buf[numread-1] == '\n') {
         buf[numread] = '\0';
         return numread;
      }  
   }   
   errno = EINVAL;
   return -1;
}

ssize_t readtimed(int fd, void *buf, size_t nbyte, double seconds) {
   struct timeval timedone;

   timedone = add2currenttime(seconds);
   if (waitfdtimed(fd, timedone) == -1)
      return (ssize_t)(-1);
   return r_read(fd, buf, nbyte);
}

int readwrite(int fromfd, int tofd) {
   char buf[BLKSIZE];
   int bytesread;

   if ((bytesread = r_read(fromfd, buf, BLKSIZE)) < 0)
      return -1;
   if (bytesread == 0)
      return 0;
   if (r_write(tofd, buf, bytesread) < 0)
      return -1;
   return bytesread;
}

int readwriteblock(int fromfd, int tofd, char *buf, int size) {
   int bytesread;

   bytesread = readblock(fromfd, buf, size);
   if (bytesread != size)         /* can only be 0 or -1 */
      return bytesread;
   return r_write(tofd, buf, size);
}

int waitfdtimed(int fd, struct timeval end) {
   fd_set readset;
   int retval;
   struct timeval timeout;
 
   if ((fd < 0) || (fd >= FD_SETSIZE)) {
      errno = EINVAL;
      return -1;
   }  
   FD_ZERO(&readset);
   FD_SET(fd, &readset);
   if (gettimeout(end, &timeout) == -1)
      return -1;
   while (((retval = select(fd+1, &readset, NULL, NULL, &timeout)) == -1)
           && (errno == EINTR)) {
      if (gettimeout(end, &timeout) == -1)
         return -1;
      FD_ZERO(&readset);
      FD_SET(fd, &readset);
   }
   if (retval == 0) {
      errno = ETIME;
      return -1;
   }
   if (retval == -1)
      return -1;
   return 0;
}
