#include <signal.h>
#include<stdio.h>
#include<string.h>  
#include<stdlib.h>    
#include<sys/socket.h>
#include<arpa/inet.h> 
#include<unistd.h>    
#include<pthread.h> 
#include <assert.h> 
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include "restart.h"
#include <sys/stat.h>
#include <dirent.h>
#include <semaphore.h>

int clients_count = 0;
char clients_dir[64][128];
char serverdir[1024];
char serverlog[1024];
int tasks = 0;
char bufferlog[2014];


typedef struct {
    int client_sock;
    int is_full;
} handler_t;

typedef struct {
    void (*function)(void *);
    void *argument;
} copy_requests;

typedef struct {                	//*** struct to hold a queue in thread pool 
    copy_requests *request_queue;	//*** an array we use this as a queue
    int q_size;						//*** size of queue 
    int front;						//*** index of first element in queue 
    int rear;						//*** rest of queue 
    int service_num;				//*** umber of service
} Workbench;

typedef struct {
	sem_t empty;
	sem_t full;
	pthread_mutex_t lock;
	pthread_cond_t condi;
}synchron_veriable;


typedef struct {
  synchron_veriable syncron;
  pthread_t *threads;
  int threads_num;
  Workbench workbench;
  int done_flag;
  int num_working;
}th_pool;


th_pool *pool;
void *do_task(void *threadpool);
int close_pool(th_pool *pool, int flags);
th_pool *pool_creater(int thread_count, int queue_size);
int add_task(th_pool *pool, void (*routine)(void *), void *arg);
 
int sock_handler;
void server_handler(int s){
	printf("(PID %d)Caught signal %d\n", getpid(), s);
	task_t sigint;
	
	sprintf(sigint.message, "Server(PID:%d) will die(SIGINT).\n", getpid()); 
	sigint.msg_size = strlen(sigint.message);
	sigint.type = 2222;
	write(sock_handler , &sigint , sizeof(sigint));
	bzero(&sigint, sizeof(sigint));
    exit(1); 

}


void delete_client(char *name) {
	int i;
	for (i = 0; i < clients_count; i++) {
		if (strcmp(name, clients_dir[i]) == 0) 
			sprintf(clients_dir[i], "%s", clients_dir[clients_count-1]);
	}
	clients_count--;
}


int is_in_clients(char *name) {
	int i;
	for (i = 0; i < clients_count; i++) {
		if (strcmp(name, clients_dir[i]) == 0) 
			return 1;
	}
	return 0;
}

//the thread function
void connection_handler(void *);
 
int main(int argc , char *argv[]) {
	handler_t th_handle;
    int socket_desc , c, portnum, threadPoolSize;
    struct sockaddr_in server , client;
    
    struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = server_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	
    
    if (argc != 4 || strcmp(argv[1], "--help") == 0) {
		help();
		return 1;
	}
	
	portnum = atoi(argv[3]);
	if (portnum > 65535 || portnum < 0) {
		fprintf(stdout, "port number must be between 0 and 65535");
		return 1;
	}
	
	threadPoolSize = atoi(argv[2]);
	if (threadPoolSize > 64 || threadPoolSize < 0) {
		fprintf(stdout, "threadPoolSize must be between 0 and 64");
		return 1;
	}
	
    sprintf(serverdir, "%s", argv[1]);
    sprintf(serverlog, "%s.log", argv[1]);
    truncate_file(serverlog);
    printf("This is serverdir:%s.\n", serverdir);
    assert((pool = pool_creater(threadPoolSize + 1, 40)) != NULL);
    fprintf(stderr, "Pool started with %d threads and "
            "queue size of %d\n", threadPoolSize, 40);
    
    sprintf(bufferlog, "Pool started with %d threads and "
            "queue size of %d\n", threadPoolSize, 40);
	append_file(serverlog, bufferlog); 
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(portnum);
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0) {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
     
	pthread_t thread_id;
	
    while( (th_handle.client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("Connection accepted");
        th_handle.is_full = -1;
        if (tasks >= threadPoolSize) { // it is full
        	fprintf(stdout, "Server is full.\n");
        	sprintf(bufferlog, "Server is full.\n");
			append_file(serverlog, bufferlog);
        	th_handle.is_full = 1;
        }
        else {
        	th_handle.is_full = 0;
        }
         
        if (add_task(pool, &connection_handler, (void*) &th_handle) == 0) {
			usleep(10000);
			tasks++;
			fprintf(stdout, "Added %d th clieant\n", tasks);
			sprintf(bufferlog, "Added %d th clieant\n", tasks);
			append_file(serverlog, bufferlog);
			
    	}
         
        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( thread_id , NULL);
        puts("Handler assigned");
    }
     
    if (th_handle.client_sock < 0) {
        perror("accept failed");
        return 1;
    }
    assert(close_pool(pool, 1) == 0);
    return 0;
}
 
/*
 * This will handle connection for each client
 * */
void connection_handler(void *socket_desc)
{
    //Get the socket descriptor
    handler_t handle = *(handler_t*)socket_desc;
    int sock = handle.client_sock;
    sock_handler = sock;
    int read_size;
    task_t message , client_message;
    char serverdd[1024], client_dir[1024];
     
    //Send some messages to the client
    
    fprintf(stdout, "From handler: %d is full value.\n", handle.is_full); 
    if (handle.is_full == 1) {
    	fprintf(stdout, "From handler: Server is full.\n"); 
    	sprintf(bufferlog, "From handler: Server is full.\n"); 
		append_file(serverlog, bufferlog);
    	sprintf(message.message, "Server is full.\n");
    	sprintf(message.name, "%s", serverdir);
    	fprintf(stdout, "len of message is %d.\n", message.msg_size); 
    	message.msg_size = strlen(message.message);
    	message.type = -1000;
    	write(sock , &message , sizeof(message));
    	tasks--;
    	return;
    }
    else
    	message.type = 0;
    
    sprintf(message.message, " I am your connection handler\n");
    sprintf(message.name, "%s", serverdir);
    fprintf(stdout, "len of message is %d.\n", message.msg_size); 
    sprintf(bufferlog, "len of message is %d.\n", message.msg_size);
	append_file(serverlog, bufferlog);
    message.msg_size = strlen(message.message);	
    write(sock , &message , sizeof(message));
    
    
    // have to read client dir here
    task_t client_dirname;
    read_size = read( sock , &client_dirname, sizeof(client_dirname)); 
    if (read_size <= 0) {
    	printf("[server]: Couldnt read client dir name.\n");
    	return;
    }
    else {
    	client_dirname.message[client_dirname.msg_size] = '\0';
    	fprintf(stdout, "[server]:Client message : %s.\n", client_dirname.message); 
    	sprintf(bufferlog, "[server]:Client dir name %s.\n", client_dirname.name);
		append_file(serverlog, bufferlog);
    	fprintf(stdout, "[server]:Client dir name %s.\n", client_dirname.name);
    	sprintf(bufferlog, "[server]:Client dir name %s.\n", client_dirname.name);
		append_file(serverlog, bufferlog);
    	if (is_in_clients(client_dirname.name)) {
    		fprintf(stdout, "[server]:Client online already: %s.\n", client_dirname.name);
    		sprintf(bufferlog, "[server]:Client online already: %s.\n", client_dirname.name);
			append_file(serverlog, bufferlog);
			message.type = -1001;
    		write(sock , &message , sizeof(message));
    		return;
    	}
    	sprintf(clients_dir[clients_count], "%s", client_dirname.name);
    	clients_count++;
    	if (is_in_clients(client_dirname.name)) {}
    }
    
    
    
    sprintf(client_dir, "%s/%s", serverdir, client_dirname.name);
    send_dir(sock, client_dir, 0, client_dir, serverdir);
    
    //tell client i am done 
    task_t taskdir_done;
    sprintf(taskdir_done.message, "type is -1 and dir sent %s.\n", taskdir_done.name); 
	taskdir_done.msg_size = strlen(taskdir_done.message);
	taskdir_done.type = -1;
	write(sock , &taskdir_done , sizeof(taskdir_done));
	bzero(&taskdir_done, sizeof(taskdir_done));
	
    //receive missing file 
    receive_dir(sock);
    
    printf("BEFORE WHILE LOOPP.\n");
    //Receive a message from client
    while( (read_size = recv(sock , &client_message , sizeof(client_message) , 0)) > 0 ) {
    	
	    //end of string marker
		client_message.message[client_message.msg_size] = '\0';
		
		fprintf(stdout, "[server]: client message : %s and type is %d.\n", client_message.message, client_message.type);
		sprintf(bufferlog, "[server]: client message : %s and type is %d.\n", client_message.message, client_message.type);
		append_file(serverlog, bufferlog);
		
		
		if (client_message.type == 1) {
			fprintf(stdout, "type is 1.\n");
			fprintf(stdout, "name of file is %s.\n", client_message.name);
			sprintf(bufferlog, "name of file is %s.\n", client_message.name);
			append_file(serverlog, bufferlog);
			if( access( client_message.name, F_OK ) != -1 ) {
    			printf("file exist.\n");
			}
			else {
    			write_file(&client_message);
			}
			//fprintf(stdout, "%s....\n", client_message.file_content);
			
		}
		if (client_message.type == 0) {
			fprintf(stdout, "type is 0.\n");
			fprintf(stdout, "name of directory is %s.\n", client_message.name);
			sprintf(bufferlog, "name of directory is %s.\n", client_message.name);
			append_file(serverlog, bufferlog);
			
			struct stat sb;

    		if (stat(client_message.name, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        		printf("Directory already exist.\n");
    		}
    		else {
        		printf("Directory does not exist.\n");
        		mkdir(client_message.name, 0777);
    		}
    
			
			memset(client_message.name, 0, strlen(client_message.name));
		}
		
		
		
		if (client_message.type == 2) {
			sprintf(serverdd, "%s/%s", serverdir, client_message.name);
			printf("serverdd is %s.\n", serverdd);
			delete_dir(serverdd);
		}
		
		if (client_message.type == 3) {
			//write(sock , &client_message , sizeof(client_message));
			sprintf(serverdd, "%s/%s", serverdir, client_message.name);
			printf("serverdd is %s.\n", serverdd);
			if( access( serverdd, F_OK ) != -1 ) {
    			printf("file already exist but (overwride).\n");
    			sprintf(client_message.name, "%s", serverdd);
    			write_file(&client_message);
    			
			}
			else {
				sprintf(client_message.name, "%s", serverdd);
    			write_file(&client_message);
			}
			//receive_file(sock, "w", 1111);
		}
		
		if (client_message.type == 2222) {
			fprintf(stdout, "Client died %s.\n", client_message.name);
			sprintf(bufferlog, "Client died %s.\n", client_message.name);
			append_file(serverlog, bufferlog);
			tasks--;
			delete_client(client_message.name);
			break;
		}
		
		//Send the message back to client
	    //write(sock , &client_message , sizeof(client_message));
		
		//clear the message buffer
		memset(client_message.message, 0, 1024);
		//receive_file(sock, "ds", 1024);
			
    }
     
    if(read_size == 0) {
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1) {
        perror("recv failed:");
        tasks--;
        fprintf(stdout, "Number of active clients %d", tasks);
        sprintf(bufferlog, "Number of active clients %d", tasks);
		append_file(serverlog, bufferlog);
        
    }
         
} 


th_pool *pool_creater(int threads_num, int q_size) {
    th_pool *pool;
    int i;

    if((pool = (th_pool *)malloc(sizeof(th_pool))) == NULL) {
        return NULL;
    }

    pool->threads_num = 0;
    pool->workbench.q_size = q_size;
    pool->workbench.front = pool->workbench.rear = pool->workbench.service_num = 0;
    pool->done_flag = pool->num_working = 0;

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * threads_num);
    pool->workbench.request_queue = (copy_requests *)malloc
        (sizeof(copy_requests) * q_size);

    
    if((sem_init(&(pool->syncron.full), 0, q_size) != 0) ||
       (pthread_cond_init(&(pool->syncron.condi), NULL) != 0) ||
       (sem_init(&(pool->syncron.empty), 0, 0) != 0) ||
       (pthread_mutex_init(&(pool->syncron.lock), NULL) != 0) ||
       (pool->threads == NULL) ||
       (pool->workbench.request_queue == NULL)) {
        return NULL;
    }

    for(i = 0; i < threads_num; i++) {
        if(pthread_create(&(pool->threads[i]), NULL,
                          do_task, (void*)pool) != 0) {
            close_pool(pool, 0);
            return NULL;
        }
        pool->threads_num++;
        pool->num_working++;
    }

    return pool;
}

int add_task(th_pool *pool, void (*function)(void *), void *argument) {
    int err = 0;
    int next;

	
    if(pool == NULL || function == NULL || sem_wait(&pool->syncron.full) != 0 ||
    	pthread_mutex_lock(&(pool->syncron.lock)) != 0) {
        return -1;
    }

    next = (pool->workbench.rear + 1) % pool->workbench.q_size;

	if(pool->done_flag) {
		return -1;
	}

	pool->workbench.request_queue[pool->workbench.rear].function = function;
	pool->workbench.request_queue[pool->workbench.rear].argument = argument;
	pool->workbench.rear = next;
	pool->workbench.service_num += 1;
	//fprintf(stdout, "In add task %d.\n", pool->workbench.service_num);

	if(pthread_cond_signal(&(pool->syncron.condi)) != 0 ||
	   pthread_mutex_unlock(&pool->syncron.lock) != 0 ||
	   sem_post(&pool->syncron.empty) != 0) {
		return -1;
	}

    return err;
}

int close_pool(th_pool *pool, int flags) {
    int i;
	
    if(pool->done_flag || pool == NULL || pthread_mutex_lock(&(pool->syncron.lock)) != 0) {
        return -1;
    }

	pool->done_flag = flags;

	if((pthread_cond_broadcast(&(pool->syncron.condi)) != 0) ||
	   (sem_post(&(pool->syncron.empty)) != 0) ||
	   (sem_post(&(pool->syncron.full)) != 0) ||
	   (pthread_mutex_unlock(&(pool->syncron.lock)) != 0)) {
		return -1;
	}

	for(i = 0; i < pool->threads_num; i++) {
		if(pthread_join(pool->threads[i], NULL) != 0) {
		    return -1;
		}
	}
    
    if(pool->num_working > 0) {
        return -1;
    }

    if(pool->threads) {
        free(pool->threads);
        free(pool->workbench.request_queue);
        sem_destroy(&(pool->syncron.empty));
        sem_destroy(&(pool->syncron.full));
        pthread_mutex_lock(&(pool->syncron.lock));
        pthread_mutex_destroy(&(pool->syncron.lock));
        pthread_cond_destroy(&(pool->syncron.condi));
    }
    free(pool);    
    return 0;
}


void *do_task(void *threadpool) {
    th_pool *pool = (th_pool *)threadpool;
    copy_requests task;
	pool->num_working--;
    while(1) {
    	sem_wait(&pool->syncron.empty);
        pthread_mutex_lock(&(pool->syncron.lock));

        while((pool->workbench.service_num == 0) && (!pool->done_flag)) {
            pthread_cond_wait(&(pool->syncron.condi), &(pool->syncron.lock));
        }

        if(pool->done_flag) {
            break;
        }
		
        task.function = pool->workbench.request_queue[pool->workbench.front].function;
        task.argument = pool->workbench.request_queue[pool->workbench.front].argument;
        pool->workbench.front = (pool->workbench.front + 1) % pool->workbench.q_size;
        pool->workbench.service_num -= 1;

        
        pthread_mutex_unlock(&(pool->syncron.lock));
		sem_post(&pool->syncron.full);
        
        (*(task.function))(task.argument);
    }

    pthread_mutex_unlock(&(pool->syncron.lock));
    sem_post(&pool->syncron.full);
    sem_post(&pool->syncron.empty);
    pthread_exit(NULL);
    return(NULL);
}








