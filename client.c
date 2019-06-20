#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h>  
#include <arpa/inet.h>
#include <unistd.h>    //write - read
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include "restart.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
//#define PORT 8888 

int sock_handler;
char clientdir[1024];
char clientlog[1024];
char buflog[2014];

void client_handler(int s){
	printf("(PID %d)Caught signal %d\n", getpid(), s);
	task_t sigint;
	
	sprintf(sigint.message, "Client(PID:%d) will die(SIGINT).\n", getpid());
	sprintf(sigint.name, "%s", clientdir); 
	sigint.msg_size = strlen(sigint.message);
	sigint.type = 2222;
	write(sock_handler , &sigint , sizeof(sigint));
	bzero(&sigint, sizeof(sigint));
    exit(1); 

}



int main(int argc, char const *argv[]) { 

	struct sockaddr_in address; 
	int sock = 0, valread, bi = 1, portnum; 
	struct sockaddr_in serv_addr; 
	char *hello = "Hello from client", serverdir[1024]; 
	task_t buffer, input; 
	
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = client_handler;
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
	sprintf(clientdir, "%s", argv[1]);
	sprintf(clientlog, "%s.log", argv[1]);
	
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
		fprintf(stdout, "\n[client]:Socket creation error \n"); 
		return -1; 
	} 

	fprintf(stdout, "AM I?\n");
	memset(&serv_addr, '0', sizeof(serv_addr)); 

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(portnum); 
	
	// Convert IPv4 and IPv6 addresses from text to binary form 
	if(inet_pton(AF_INET, argv[2], &serv_addr.sin_addr)<=0) { 
		fprintf(stdout, "\n[client]:Invalid address/ Address not supported \n"); 
		return -1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
		fprintf(stdout, "\n[client]:Connection Failed \n"); 
		return -1; 
	} 
	
	sock_handler = sock;
	//send(sock , hello , strlen(hello) , 0 ); 
	fprintf(stdout, "[client]:%d connected.\n", getpid()); 
	sprintf(buflog, "[client]:%d connected.\n", getpid());
	append_file(clientlog, buflog); 
	valread = read( sock , &buffer, sizeof(buffer)); 
	buffer.message[buffer.msg_size] = '\0';
	fprintf(stdout, "[client]:%s:%s\n", buffer.name, buffer.message);
	sprintf(buflog, "[client]:%s:%s\n", buffer.name, buffer.message);
	append_file(clientlog, buflog);  
	sprintf(buflog, "[client]:%s:%s\n", buffer.name, buffer.message); 
	append_file(clientlog, buflog); 
	
	if (buffer.type == -1000) {
		fprintf(stdout, "Server is full Client will be shut down.\n");
		sprintf(buflog, "Server is full Client will be shut down.\n");
		append_file(clientlog, buflog);
		return 0; 
	}
	
	
	
	sprintf(serverdir, "%s", buffer.name);
	memset(buffer.message, 0, strlen(buffer.message));
	bzero(&buffer, sizeof(buffer));
	
	
	task_t dirName;
	//have to send client dir to server here
	sprintf(dirName.name, "%s", argv[1]);
	printf("Client dirName is %s.\n", dirName.name);
	sprintf(dirName.message, "Client send its directory %s.\n", dirName.name); 
	dirName.msg_size = strlen(dirName.message);
	dirName.type = -1;
	write(sock , &dirName , sizeof(dirName));
	bzero(&dirName, sizeof(dirName));
	
	valread = read( sock , &buffer, sizeof(buffer));
	if (buffer.type == -1001) {
		fprintf(stdout, "There is same name client connected.\n");
		sprintf(buflog, "There is same name client connected.\n");
		append_file(clientlog, buflog);
		return 0; 
	}
	
	//receive missing dir/file from server
	receive_dir(sock);
	//sprintf(main_path, "%s", "client1");
	send_dir(sock, clientdir, 1, argv[1], serverdir);
	
	
	task_t taskdir_done;
	
	valread = read( sock , &taskdir_done, sizeof(taskdir_done)); 
	taskdir_done.message[taskdir_done.msg_size] = '\0';
	bzero(&taskdir_done, sizeof(taskdir_done));
	
    sprintf(taskdir_done.message, "type is -1 and dir sent %s.\n", taskdir_done.name); 
	taskdir_done.msg_size = strlen(taskdir_done.message);
	taskdir_done.type = -1;
	write(sock , &taskdir_done , sizeof(taskdir_done));
	bzero(&taskdir_done, sizeof(taskdir_done));
	
	watch_client(clientdir, sock, serverdir);
	
	
	
	return 0; 
} 









