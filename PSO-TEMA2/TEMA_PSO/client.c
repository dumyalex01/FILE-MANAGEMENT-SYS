#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <fcntl.h>
#include <libgen.h>
#define PORT 12345                         //DELETE --- pe directoare
#define IP "127.0.0.1"                                          //LA MOVE NU CREAZA FISIER
#define BUFFER_SIZE 1024
#define EXIT_STATUS 10


int connectToServer() {
    int clientSocket;
    struct sockaddr_in serverAddr;

    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la crearea socket-ului client");
        exit(1);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(IP);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Eroare la conectarea la server");
        exit(1);
    }

    return clientSocket;
}

void sendMessageToServer(int clientSocket, char* messageToSend, char* messageToReceive) {
    int sender = send(clientSocket, messageToSend, strlen(messageToSend), 0);
    if (sender <= 0) {
        perror("Eroare la trimiterea mesajului către Server!");
        exit(1);
    }
    int numarCaracterePrimite;
    numarCaracterePrimite = recv(clientSocket, messageToReceive, BUFFER_SIZE, 0);
    if (numarCaracterePrimite <= 0) {
        printf("Aplicatia se va inchide in cateva momente...");
        exit(1);
    }
    messageToReceive[numarCaracterePrimite]='\0';
}

char* createMessage(char* command)
{
    char copieComanda[250];
    char* bufferToReturn=(char*)malloc(sizeof(char)*250);
    uint32_t code;
    strcpy(copieComanda,command);
    char*p = strtok(copieComanda," ");
    if(strncmp(p,"LIST",4)==0)
        code=0x0;
    else if(strncmp(p,"DOWNLOAD",8)==0)
            code=0x1;
    else if(strncmp(p,"UPLOAD",6)==0)
            code=0x2;
    else if(strncmp(p,"DELETE",6)==0)
            code=0x4;
    else if(strncmp(p,"MOVE",4)==0)
            code=0x8;
    else if(strncmp(p,"UPDATE",6)==0)
            code=0x10;
    else if(strncmp(p,"SEARCH",6)==0)
            code=0x20;
    else return "NIMIC";
    int x=code;
    char c=(char)(x+60); // adun 60 pentru a trimite un caracter "valid" prin socket...
    bufferToReturn[0]=c;
    bufferToReturn[1]=',';
    if(code==0x8)
    {
        p=strtok(NULL," ");
        p[strlen(p)]='\0';
        int dimensiuneBuffer=strlen(p);
        char numar_octeti[10];
        snprintf(numar_octeti,10,"%d",dimensiuneBuffer);
        strcat(bufferToReturn,numar_octeti);
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,p); //primul path
        strcat(bufferToReturn,",");
        p=strtok(NULL," ");  //al doilea path
        p[strlen(p)-1]='\0';
        dimensiuneBuffer=strlen(p);
        snprintf(numar_octeti,10,"%d",dimensiuneBuffer);
        strcat(bufferToReturn,numar_octeti);
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,p); //al doilea path
    }
    if(code==0x1 || code==0x4 || code== 0x20)
    {
        p=strtok(NULL," ");
        p[strlen(p)-1]='\0';
        int dimensiuneBuffer=strlen(p);
        char numar_octeti[10];
        snprintf(numar_octeti,10,"%d",dimensiuneBuffer);
        strcat(bufferToReturn,numar_octeti);
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,p);

    }
    if(code==0x2)
    {
        p=strtok(NULL," ");
        p[strlen(p)-1]='\0';
        int dimBuff=strlen(p);
        char numar_octeti[10];
        snprintf(numar_octeti,10,"%d",dimBuff);
        strcat(bufferToReturn,numar_octeti);
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,p);
        strcat(bufferToReturn,",");
        char path[100];
        strcpy(path,p);
        int fd=open(path, O_RDONLY);
        char buff[1024];
        int size=read(fd,buff,1024);
        snprintf(numar_octeti,10,"%d",size);
        strcat(bufferToReturn,numar_octeti);
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,buff);
    }
    if(code==0x10)
    {
        p=strtok(NULL," ");
        p[strlen(p)]='\0';
        int dimPath=strlen(p);
        char numar_octeti[10];
        snprintf(numar_octeti,10,"%d",dimPath);
        strcat(bufferToReturn,numar_octeti);
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,p); //path
        strcat(bufferToReturn,",");
        p=strtok(NULL," "); 
        strcat(bufferToReturn,p);//octetStart
        p=strtok(NULL," ");
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,p); //offset inceput
        p=strtok(NULL," ");  //caractere de adaugat
        strcat(bufferToReturn,",");
        strcat(bufferToReturn,p);
    }

    return bufferToReturn;

    

}
void interpretMessage(char* messageReceived)
{   char copieMesaj[250];
    strcpy(copieMesaj,messageReceived);
    char*p=strtok(copieMesaj,",");
    uint32_t code=(int)p[0]-60;
    switch(code)
    {
        case 0x0:
            printf("succes\n");
            break;
        case 0x1:
            printf("file not found\n");
            break;
        case 0x2:
            printf("permission denied\n");
            break;
        case 0x4:
            printf("out of memory\n");
            break;
        case 0x8:
            printf("server busy\n");
            break;
        case 0x10:
            printf("unknown operation\n");
            break;
        case 0x20:
            printf("bad arguments\n");
            break;
        case 0x40:
            printf("other eror\n");
            break;
        default:
            printf("CEVA DUBIOS S-A INTAMPLAT\n");
            break;
    }
}
void run()
{
    int clientSocket=connectToServer();
    while(1)
    {
        printf(">");
        char* command=(char*)malloc(sizeof(char)*250);
        fgets(command,250,stdin);
        if(strncmp(command,"EXIT",4)==0)
            break;
        else
        {
            char* buff = createMessage(command);
            if(strcmp(buff,"NIMIC")==0)
                printf("Comanda introdusa nu este buna!");
                else{
                        char*mesajPrimit=(char*)malloc(sizeof(char)*250);
                        sendMessageToServer(clientSocket,buff,mesajPrimit);
                        interpretMessage(mesajPrimit);
                        if(strcmp(command,"LIST\n")==0)
                        {   printf("\n");
                            char*p=strtok(mesajPrimit,",");
                            p=strtok(NULL,",");
                            printf("%s",p);
                        }
                        if(strstr(command,"DOWNLOAD"))
                        {
                            char*filepath=(char*)malloc(sizeof(char)*150);
                            strcpy(filepath,"./clientFolder/");
                            char*filename=(char*)malloc(sizeof(char)*100);
                            strcpy(filename,command);
                            char*d=strtok(filename," \n");
                            d=strtok(NULL," \n");
                          
                            filename=basename(d);
                            strcat(filepath,filename);
                            int fd=open(filepath,O_WRONLY | O_CREAT);
                            if(fd<0)
                            {
                                printf("ERROR");
                            }
                            char*p=strtok(mesajPrimit,",");
                            p=strtok(NULL,",");
                            p=strtok(NULL,","); //continut
                            int bytesWrite=write(fd,p,strlen(p));
                            close(fd);
                            free(filepath);
 

                        }
                        if(strstr(command,"SEARCH"))
                        {
                            char*word=strtok(mesajPrimit,",");//cod
                            word=strtok(NULL,","); //primul din lista
                            while(word!=NULL)
                            {
                                 printf("%s\n",word);
                                 word=strtok(NULL,",");
                            }
                        }
                    
            
                }
            
            
        }

    }
}

int main() {
    

    run();
    return 0;
}