#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#define BUFSIZE 1024

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

int message_end(char *buf,int len){
    char* pos= memchr(buf,'\n',len );
    if (pos==NULL)
        return -1;
    return pos-buf+1;
}

void advance(char *buf, int *plen, int n) {
    memmove(buf, buf + n, *plen - n);
    *plen -= n;
}

int get_question(int sock,char buf[],int *len,char question[]){
    int n=-1;
    while(*len < BUFSIZ) {
        n = message_end(buf, *len);
        if(n >= 0)
            break;
        int rc = read(sock, buf + *len, BUFSIZ - *len);
        if(rc <= 0)
            break;
        *len += rc;
    }
    if(n >= 0){
        memcpy(question,buf,n);
        question[n-1]='\0'; //REMPLACE LE \n par \0
        //que faire avec \r s'il est présent
        if(question[n-2]=='\r'){
            question[n-2]='\0';
        }
        advance(buf, len, n);
        return n;
    }
    return -1;
}

int do_echo(char *args,char *resp){
    snprintf(resp, BUFSIZE, "ok %s\n",args);
    return 0;
}
//J'ai un probleme ici et je ne comprends pas pourquoi ?
int do_rand(char *args,char *resp){
    int r;
    if (args!=NULL) {
        r=rand()%atoi(args);

    }else{
        r=rand();
    }
    snprintf(resp, BUFSIZE, "ok %d\n",r);
    return 0;
}

int do_quit(char *args,char *resp){
    snprintf(resp, BUFSIZE, "[server disconnected]\n");
    return -1;
}

int eval_quest(char question[],char response[]){
    char *cmd = strsep(&question," ");
    char *args=question;
    if (strcmp(cmd,"echo")==0) {
        return do_echo(args,response);
    }else if(strcmp(cmd, "random") == 0){
        return do_rand(args, response);
    }else if(strcmp(cmd, "quit") == 0){
        return do_quit(args, response);
    }else{
        snprintf(response, BUFSIZE, "fail unknown command %s\n", cmd);
        return 0;
    }
}


int write_full(int sock,char *resp, int len){
    if(write(sock,resp,len)<0){
        perror("write error");
        return -1;
    }
    printf("Message envoyé\n\n");
    return 0;
}


void* worker(void* cli){
    char question[BUFSIZE], response[BUFSIZE],buf[BUFSIZE];
    client_data* newCli=(client_data*)cli;
    int len=0;
    while(1) {
        if(get_question(newCli->sock,buf, &len, question) < 0) {
            break;
        }
        if(eval_quest(question, response) < 0){
            break; /* erreur: on deconnecte le client */
        }
        if(write_full(newCli->sock, response,strlen(response)) < 0) {
            perror("could not send message");
            break; /* erreur: on deconnecte le client */
        }
    }
    return cli;
}

void client_arrived(int descr){
    if(descr<0){
        perror("accept error");
        return;
    }
    client_data* newCli=alloc_client(descr);
    printf("Nombres de clients : %d\n",nr_clients);
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
