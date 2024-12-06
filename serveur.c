#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#define BUFSIZE 1024


//@Messagerie
typedef struct s_msg { char sender[9]; char* text;
struct s_msg * next;
} msg;

typedef struct s_mbox{ msg *first, *last;
} mbox;

msg* create_msg(char* author, char* contents){
    if(strlen(author)>8){
        printf("Size max of author exceeded");
        exit(1);
    }
    msg* newmsg=malloc(sizeof(msg));
    if(newmsg==NULL){
        perror("alloc error in create_msg");
        exit(1);
    }
    newmsg->text=malloc(sizeof(strlen(contents)));
    if(newmsg->text==NULL){
        perror("alloc error in create_msg");
        exit(1);
    }
    strncpy(newmsg->sender,author,9);
    strcpy(newmsg->text,contents);
    newmsg->next=NULL;
    
    return newmsg;
}
//pas utilisé
void destroy_msg(msg* mess){
    free(mess->next);
    free(mess);
}

void show_msg(msg* mess){
    printf("Destinataire : %s \n Message : %s\n\n",mess->sender,mess->text);
}

//pas utilisé
void init_mbox(mbox* box){
    box->first=box->last=NULL;
}

void put(mbox* box, msg* mess){
    if(box->first==NULL){
        box->first=mess;
        mess->next=box->last;
        box->last=mess;
    }
    else{
        box->last->next=mess;
        box->last=mess;
    }
}

msg* get(mbox* box){
    msg* temp;
    if(box->first==NULL){
        return NULL;
    }
    else if(box->first==box->last){
        temp=box->last;
        box->first=box->last=NULL;
        return temp;
    }
    else{
        temp=box->first;
        box->first=box->first->next;
        return temp;
    }
}
//pas utilisé
void send_mess(mbox* box, char* author, char* contents){
    msg* newmsg=create_msg(author,contents);
    put(box,newmsg);
}

//Pas utile
void recieve_mess(mbox* box){
    show_msg(get(box));
}

//@Serveur

int nr_clients;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
struct client_data *first,*last;

typedef struct client_data {
    struct client_data *prev,*next;
    int sock;
    pthread_t thread;
    char nick[9];
    mbox box;
}client_data;



void init_clients(){
    if(pthread_mutex_lock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
    first=NULL;
    last=NULL;
    if(pthread_mutex_unlock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
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
    newClient->nick[0]='\0';
    if(pthread_mutex_lock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
    if (first==NULL) {
        first=newClient;
        last=newClient;
        newClient->prev=NULL;
        newClient->next=NULL;
    }else{
        last->next=newClient;
        newClient->prev=last;
        last=newClient;
    }
    if(pthread_mutex_unlock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
    return newClient;
}



void free_client(client_data* cli){
    if (pthread_mutex_lock(&lock)!=0) {
        perror("pthread_mutex_lock error");
        exit(1);
    }
    nr_clients--;
    if(nr_clients!=0){
        cli->prev->next=cli->next;
    }
    if (pthread_mutex_unlock(&lock)!=0) {
        perror("pthread_mutex_unlock error");
        exit(1);
    }
    free(cli);
    printf("Nombres de clients : %d\n",nr_clients);
}



client_data* search_client(char* nick){
    client_data* cli=first;
    while(cli!=NULL){
        if(strcmp(cli->nick,nick)==0){
            return cli;
        }
        cli=cli->next;
    }
    return cli;
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
    while(*len < BUFSIZE) {
        n = message_end(buf, *len);
        if(n >= 0)
            break;
        int rc = read(sock, buf + *len, BUFSIZE - *len);
        if(rc <= 0)
            break;
        *len += rc;
    }
    if(n >= 0){
        memcpy(question,buf,n);
        question[n-1]='\0'; 
        if(question[n-2]=='\r'){
            question[n-2]='\0';
        }
        advance(buf, len, n);
        return n;
    }
    return -1;
}

int do_send(char* args, char* resp, client_data* cli){
    char *dst = strsep(&args," ");
    char *contents=args;
    msg* mess=create_msg(cli->nick,contents);
    mbox* boxdst;
    client_data* clidst;
    if((clidst=search_client(dst))==NULL){
        snprintf(resp, BUFSIZE, "alias %s not found\n",dst);
        return 0;
    }
    boxdst=&clidst->box;
    put(boxdst,mess);
    snprintf(resp, BUFSIZE, "ok message send\n");
    return 0;
} 

int do_recv(char* args, char* resp, client_data* cli){
    msg* current;
    int len=0;
    snprintf(resp, BUFSIZE, "No message in your box\n");
    while((current=get(&cli->box))!=NULL){
        snprintf(resp+len, BUFSIZE-len, "From : %s Message : %s\n",current->sender,current->text);
        len=strlen(resp);
    }
    return 0;
} 

int do_echo(char *args,char *resp){
    snprintf(resp, BUFSIZE, "ok %s\n",args);
    return 0;
}

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
    return 1;
}

int do_list(char *args,char *resp){
    client_data* current = first;
    int len=strlen(resp);
    while (current!=NULL) {
        snprintf(resp+len, BUFSIZE-len, "%s\n",current->nick);
        len=strlen(resp);
        current=current->next;
    }
    return 0;
}

int nameOk(char *args){
    int len=strlen(args);
    
    if (len<1 && len>8) {
        return 0;
    }
    for(int i=0;i<len;i++){
        if (isalnum(args[i])!=1) {
            return 0;
        }
    }
    client_data* current = first;
    if(pthread_mutex_lock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
    while (current!=NULL) {
        if (strcmp(current->nick,args)==0) {
            if(pthread_mutex_unlock(&lock)!=0){
                 perror("pthread_mutex_lock error");
               exit(1);
            }
            return 0;
        }
        current=current->next;
    }
    if(pthread_mutex_unlock(&lock)!=0){
        perror("pthread_mutex_lock error");
        exit(1);
    }
    return 1;
}

int do_name(char *args,char *resp,client_data *cli){
    if(nameOk(args)){
        snprintf(resp, BUFSIZE, "ok welcome\n");
        if(pthread_mutex_lock(&lock)!=0){
            perror("pthread_mutex_lock error");
            exit(1);
        }
        strcpy(cli->nick,args);
        if(pthread_mutex_unlock(&lock)!=0){
            perror("pthread_mutex_lock error");
            exit(1);
        }
    }else{
        snprintf(resp, BUFSIZE, "fail alias unavailable\n");
    }
    return 0;
}

int do_help(char * response){
    snprintf(response,BUFSIZE,"List of commands : \n\n\t- help\n\t- random\n\t- echo\n\t- list\n\t- send\n\t- rcv\n\t- alias\n\t- time\n\t- quit");
    return 0;
}

int do_time(char * response){
    time_t current_time;
    current_time = time(NULL);
    strftime(response,BUFSIZE,"%Y-%m-%d %H:%M:%S",localtime(&current_time));
    return 0;
}

int eval_quest(char question[],char response[],client_data *cli){
    char *cmd = strsep(&question," ");
    char *args=question;
    if (strcmp(cmd,"echo")==0) {
        return do_echo(args,response);
    }else if(strcmp(cmd, "random") == 0){
        return do_rand(args, response);
    }else if(strcmp(cmd, "time") == 0){
        return do_time(response);
    }else if(strcmp(cmd, "quit") == 0){
        return do_quit(args, response);
    }else if(strcmp(cmd,"list")==0){
        if(cli->nick[0]!='\0'){
            return do_list(args,response);
        }else{
            snprintf(response, BUFSIZE, "nick not inform, use cmd : alias nick\n");
            return 0;
        }
    }else if(strcmp(cmd,"help")==0){
        return do_help(response);
    }else if(strcmp(cmd,"send")==0){
        if(cli->nick[0]!='\0'){
            return do_send(args,response,cli);
        }else{
            snprintf(response, BUFSIZE, "nick not inform, use cmd : alias nick\n");
            return 0;
        }
    }
    else if(strcmp(cmd,"rcv")==0){
        if(cli->nick[0]!='\0'){
            return do_recv(args,response,cli);
        }else{
            snprintf(response, BUFSIZE, "nick not inform, use cmd : alias nick\n");
            return 0;
        }
    }else if(strcmp(cmd,"alias")==0){
        return do_name(args,response,cli);
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
    printf("Message envoyé\n");
    return 0;
}


void* worker(void* cli){
    char question[BUFSIZE], response[BUFSIZE],buf[BUFSIZE];
    client_data* newCli=(client_data*)cli;
    int len=0;
    int quit=1;
    while(1) {
        memset(question,'\0',BUFSIZE);
        memset(response,'\0',BUFSIZE);
        memset(buf,'\0',BUFSIZE);
        if(get_question(newCli->sock,buf, &len, question) < 0) {
            break;
        }
        if((quit=eval_quest(question, response,newCli)) < 0){
            close(newCli->sock);
            free_client(newCli);
            break;
        }
        if(write_full(newCli->sock, response,strlen(response)) < 0) {
            perror("could not send message");
            close(newCli->sock);
            free_client(newCli);
            break;
        }
        if(quit){
            close(newCli->sock);
            free_client(newCli);
            break;
        }
    }
    return 0;
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
    socklen_t addr_len = sizeof(address_sock);
    memset(&address_sock, 0, addr_len);
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
    char buf[NI_MAXHOST];
    while(1){
        memset(buf,'\0',sizeof(buf));
        sock2=accept(sock,(struct sockaddr *)&address_sock,&addr_len);
        if (sock2<0){
            perror("accept"); continue;
        }
        if(getnameinfo((struct sockaddr *)&address_sock, addr_len, buf, sizeof(buf), NULL,0, 0) == 0){
            printf("connection from %s\n", buf);
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
