
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <sys/socket.h>
 #define BUFSIZE 1024

int write_all(int sock, char *buf){
    int k = 0;
    int len=strlen(buf);
    while(k<len){
        int rc = write(sock,buf+k,len-k);
        if(rc <= 0){
            perror("write error");
            return -1;
        }
        k += rc;
    }
    return len;
}

int get_response(int sock,char * resp){
    if(read(sock, resp, BUFSIZE)<0){
        perror("read error");
    }
    return 1;
}

 
int speak_to_server(int sock){
    char buf[BUFSIZE];
    char resp[BUFSIZE];
    while(1){
        memset(buf,0,BUFSIZE);
        memset(resp,0,BUFSIZE);
        printf("\n>>>");
        if (fgets(buf,BUFSIZE,stdin)!=NULL) {
                buf[strcspn(buf, "\n")+1] = '\0';
            } else {
                printf("Buf error\n");
                close(sock);
                return -1;
            }
        if(write_all(sock,buf)<0){
            close(sock);
            return -1;
        }
        if(get_response(sock,resp)<0){
            close(sock);
            return -1;
        }
        printf("%s",resp);
        if(strcmp(resp,"[server disconnected]\n\0")==0){
            close(sock);
            return 0;
        }
    }
}



int create_and_connect(char* ip, char* port){
    int descr;
    if((descr=socket(PF_INET6,SOCK_STREAM,0))<0){
        perror("socket");
        exit(1);
    }
    struct sockaddr_in6 adress_sock;
    memset(&adress_sock,0,sizeof(adress_sock));
    adress_sock.sin6_family = AF_INET6;
    adress_sock.sin6_port = htons(atoi(port));
    int r = inet_pton(AF_INET6, ip, &adress_sock.sin6_addr);
    if(r<1){
        perror("pton");
        exit(1);
    }
    r=connect(descr,(struct sockaddr *)&adress_sock,sizeof(adress_sock));
    if(r<0){
    perror("connect"); close(descr);exit(1);
    }
    return descr;
}

int main(int argc, char** argv) {
     if (argc != 3) {
         printf("Syntax error: ./client adress port\n");
         exit(1);
     }

    int sock;
    if ((sock=create_and_connect(argv[1],argv[2]))<0) {
        perror("socket error");
        exit(1);
    }

    speak_to_server(sock);


 }

