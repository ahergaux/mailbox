#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void client_arrived(int descr){
    char *buff=malloc(100*sizeof(char));
    if(descr<0){
        perror("accept error");
        return;
    }        
    int r;
    printf("client arrivé\n");
    while((r=read(descr,buff,sizeof(buff)-1))>0){
        buff[r]='\0';
        printf("Message recu, %d octets recu\n",r);
        printf("Renvoie du message..\n");
        write(descr,"Vous avez dis : ",17);
        write(descr,buff,sizeof(buff)-1);
        printf("Message envoyé\n\n");
        free(buff);
        buff=malloc(100*sizeof(char));
    }
    close(descr);
    printf("client parti\n");
    
    return;
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
