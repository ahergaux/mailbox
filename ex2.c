#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

int nr_clients;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct client_data {
    int sock;
    pthread_t thread;
}client_data;




void init_clients(){
    nr_clients=0;
}



client_data* alloc_client(int sock){
    client_data* newClient =malloc(sizeof(client_data));
    if (newClient==NULL) {
        perror("allocation for client");
        return NULL;
    }
    
    if(pthread_mutex_lock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
    nr_clients++;
    if(pthread_mutex_unlock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
    newClient->sock=sock;
    return newClient;
}




void free_client(client_data* cli){
    free(cli);
    if (pthread_mutex_lock(&lock)!=0) {
        perror("pthread_mutex_lock error");
        exit(1);
    }
    nr_clients--;
    if (pthread_mutex_unlock(&lock)!=0) {
        perror("pthread_mutex_unlock error");
        exit(1);
    }
}

void* worker(void* cli){
    char *buff=malloc(100*sizeof(char));
    client_data* newCli=(client_data*)cli;
    int r;
    printf("client arrivé\n");
    printf("Nombres de clients : %d\n",nr_clients);
    while((r=read(newCli->sock,buff,99))>0){
        buff[r]='\0';
        printf("Message recu, %d octets recu\n",r);
        printf("Renvoie du message..\n");
        write(newCli->sock,"Vous avez dis : ",17);
        write(newCli->sock,buff,99);
        printf("Message envoyé\n\n");
        memset(buff,0,100);
    }
    close(newCli->sock);
    free_client(newCli);
    printf("client parti\n");
    printf("Nombres de clients : %d\n",nr_clients);
    return NULL;
}


void client_arrived(int descr){
    if(descr<0){
        perror("accept error");
        return;
    }
    client_data* newCli=alloc_client(descr);
    
    if((pthread_create(&newCli->thread,NULL,worker,(void*)newCli))<0){
        perror("pthread create error");
    }
    
}




void listen_port(int port_num){
    int sock;
    if((sock=socket(PF_INET6,SOCK_STREAM,0))<0){
        perror("sock error");
        exit(1);
    }
    
    struct sockaddr_in6 address_sock;
    memset(&address_sock, 0, sizeof(address_sock));
    address_sock.sin6_family=AF_INET6;
    address_sock.sin6_port=htons(port_num);
    int r;
    if((r=bind(sock,(struct sockaddr *)&address_sock,sizeof(struct sockaddr_in6)))<0){
        perror("error bind");
        close(sock);
        exit(1);
    }
    if(listen(sock,1024)){
        perror("error listen");
        close(sock);
        exit(1);
    }
    int sock2;
    printf("j'attends mes clients\n");
    init_clients();
    printf("Nombres de clients : %d\n",nr_clients);
    while(1){
        sock2=accept(sock,NULL,NULL);
        if (sock2<0){
            perror("accept"); continue;
        }
        client_arrived(sock2);
    }
}




int main(int argv,char** args){
    int sock;
    if (argv!=2) {
        printf("Syntax error : ./serveur port \n");
        exit(1);
    }
    listen_port(atoi(args[1]));
    return 0;
}
