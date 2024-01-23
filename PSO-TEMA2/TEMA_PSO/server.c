#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <wait.h>
#include <libgen.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/types.h>
#define PORT 12345
#define MAX_CLIENTI 5
#define UPLOAD_TYPE 1
#define DELETE_TYPE 2
#define MOVE_TYPE 3
#define UPDATE_TYPE 4

typedef struct file
{
    const char* filepath;
    int writingInProgress;
    int readingInProgress;
    int counterList;
    char**words;
    uint32_t numarOctetiPath;
    int numarOctetiContinut;
    pthread_mutex_t mutexRead;
    pthread_mutex_t mutexWrite;
    pthread_cond_t condRead;
    pthread_cond_t condWrite;

}file;

typedef struct threadArgs
{
    char** saveptr;
    int clientSocket;
}threadArgs;

pthread_t threadClienti[MAX_CLIENTI];
pthread_t threadSIGNAL;
pthread_t threadUPDATE;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexUpdate=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexClose=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexLogger=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexFileList = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
pthread_cond_t condClose=PTHREAD_COND_INITIALIZER;
FILE*logger;
bool inchidere=0;
bool SeExecuta = true;
int numarClienti = 0;
int indexFisierUPLOAD=0;
struct file listaFisiere[300];
int threadWakeUp=0;
char pathForExecute[100];
char oldPath[100];
int flagForUpdate=0; // un flag care detetrmina ce operatie se executa in handlerul threadului de update
void addElement(char* buffer);
void removeElement(char*buffer);
void moveElement(char*oldBuffer,char*newBuffer);
void updateFile(char*buffer);
void constructWordFrequency(char***vectorWord,const char*filepath);
void writeInLogger(char*operatie,char*file,char*word);
struct file GetFileDetails(const char*pathname);

void startReading(file f) {
    pthread_mutex_lock(&f.mutexRead);
    while (f.writingInProgress == 1) {
        pthread_cond_wait(&f.condRead, &f.mutexRead);
    }
    f.readingInProgress = 1;
    pthread_mutex_unlock(&f.mutexRead);
}

void finishReading(file f) {
    pthread_mutex_lock(&f.mutexRead);
    f.readingInProgress = 0;
    pthread_cond_broadcast(&f.condWrite); // Semnalizează pentru scriere
    pthread_mutex_unlock(&f.mutexRead);
}

void startWriting(file f) {
    pthread_mutex_lock(&f.mutexWrite);
    while (f.readingInProgress == 1) {  
        pthread_cond_wait(&f.condWrite, &f.mutexWrite);
    }
    f.writingInProgress = 1;
    pthread_mutex_unlock(&f.mutexWrite);
}

void finishWriting(file f) {
    pthread_mutex_lock(&f.mutexWrite);
    f.writingInProgress = 0;
    pthread_cond_broadcast(&f.condRead); // Semnalizează pentru citire
    pthread_mutex_unlock(&f.mutexWrite);
}

//sincronizari citire si scriere


int GetFileIndex(const char*pathname)
{   
    for(int i=0;i<listaFisiere->counterList;i++)
        if(strcmp(pathname,listaFisiere[i].filepath)==0)
            return i;             //index pentru stergere
    return -1;
}
void updateFile(char*buffer)
{   pthread_mutex_lock(&mutexFileList);
    for(int i=0;i<listaFisiere->counterList;i++)
        if(strcmp(buffer,listaFisiere[i].filepath)==0)        //functie update fisier apelata din thread update
        {   
            int fd=open(buffer,O_RDONLY);
            char*buff=(char*)malloc(sizeof(char)*1024);
            startReading(listaFisiere[GetFileIndex(buffer)]);
            int bytesRead=read(fd,buff,1024);
            finishReading(listaFisiere[GetFileIndex(buffer)]);
            close(fd);
            listaFisiere[i].numarOctetiContinut=bytesRead;
            constructWordFrequency(&listaFisiere[i].words,listaFisiere[i].filepath);
        }
        pthread_mutex_unlock(&mutexFileList);

}
char* execute_LIST()
{   writeInLogger("LIST",NULL,NULL);
    char*bufferToReturn=(char*)malloc(sizeof(char)*1024);       //lista
    for(int i=0;i<listaFisiere->counterList;i++)
    {
        strcat(bufferToReturn,listaFisiere[i].filepath);
        strcat(bufferToReturn,"\n");
    }
    bufferToReturn[strlen(bufferToReturn)]='\0';
    return bufferToReturn;

}
char* execute_download(char* messageReceived)
{   writeInLogger("DOWNLOAD",NULL,NULL);     //download
    char*bufferToReturn=(char*)malloc(sizeof(char)*2048);      
    strcpy(bufferToReturn,"");
    char*p=strtok(messageReceived,",");
    p=strtok(NULL,",");
    p=strtok(NULL,",");
    int indexFile=GetFileIndex(p);
    int fd=open(listaFisiere[indexFile].filepath,O_RDONLY);
    if(fd<0)
    {
        perror("FISIERUL NU EXISTA!");
        char error[2];
        error[0]=0x1+60;
        error[1]='\0';
        strcat(bufferToReturn,error);
        return bufferToReturn;
    }
    else
    { 
        char*buffer=(char*)malloc(sizeof(char)*1024);
        startReading(listaFisiere[indexFile]);
        int bytesRead=read(fd,buffer,1024);   
          finishReading(listaFisiere[indexFile]);      //sincronizare
        if(bytesRead<0)
        {
            perror("Eroare la citire!");
            char error[2];
            error[0]=0x40+60;
            error[1]='\0';
            strcat(bufferToReturn,error);
        }
        else
        {   char code[2];
            code[0]=0x0+60;
            code[1]='\0';
            strcpy(bufferToReturn,"");
            strcat(bufferToReturn,code);
            strcat(bufferToReturn,",");
            char size[10];
            snprintf(size,10,"%d",bytesRead);
            strcat(bufferToReturn,size);
            strcat(bufferToReturn,",");
            strcat(bufferToReturn,buffer);
        }
        return bufferToReturn;
    }

}
char* execute_upload(char* messageReceived) {
    char* bufferToReturn = (char*)malloc(sizeof(char) * 2048);
    pthread_mutex_lock(&mutexUpdate);
    flagForUpdate=UPLOAD_TYPE;
    threadWakeUp = 1; // schimbare variabila pentru trezire thread de Update
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutexUpdate);


    char** safety = (char**)malloc(sizeof(char*) * 1024);
    char* p = __strtok_r(messageReceived, ",", safety); // idOp
    p = __strtok_r(NULL, ",", safety);                   // nrOctetiPath
    p = __strtok_r(NULL, ",", safety);                   // path;
    const char* numeFisier = basename(p);
    p = __strtok_r(NULL, ",", safety); // nrOctetiContinut
    int size = atoi(p);
    p = __strtok_r(NULL, ",", safety); // continut fisier

    char path[100];
    snprintf(path, sizeof(path), "serverFolder/%s", numeFisier);
    strcpy(pathForExecute,path);
    writeInLogger("UPLOAD",path,NULL);
    int indexFile=GetFileIndex(path);
    int fd = open(path, O_WRONLY | O_CREAT);

    if (fd < 0) {
        perror("Eroare la deschiderea fisierului");
        snprintf(bufferToReturn, 2048, "Eroare la deschiderea fisierului");
    } else {
        int bytesWrite = write(fd, p, size);            //sincronizare
        if (bytesWrite < 0) {
            perror("Eroare la scrierea in fisier");
            snprintf(bufferToReturn, 2048, "Eroare la scrierea in fisier");
        } else {
            
            snprintf(bufferToReturn,20,"%c",(char)(0x0+60));
        }
        close(fd);
    }

    free(safety);
    return bufferToReturn;
}
void constructWordFrequency(char***vectorWord,const char*filepath)
{  
    char**vectorCuvinte=(char**)malloc(sizeof(char*)*1024); //1024 de cuvinte
    int* vectorFrecventa=(int*)malloc(sizeof(int)*1024);
    int* indexes=(int*)malloc(sizeof(int)*1024);
    for(int i=0;i<1024;i++)
        vectorFrecventa[i]=1;
    int counterCuvinte=0;
    *vectorWord=(char**)malloc(sizeof(char*)*10);
    for(int i=0;i<10;i++)
        (*vectorWord)[i]=(char*)malloc(sizeof(char)*50);
    char buffer[1024];
    int fd=open(filepath, O_RDONLY);
    int bytesRead=read(fd,buffer,1024);
    if(bytesRead<0)
    {
        perror("Eroare la citire");
        return;
    }
    char**saveCopy=(char**)malloc(sizeof(char*)*1024);
    char*word=__strtok_r(buffer," \n",saveCopy);
    while(word!=NULL)
    {   
        vectorCuvinte[counterCuvinte]=strdup(word);
        counterCuvinte++;
        word=__strtok_r(NULL," \n",saveCopy);
        
    }
    for(int i=0;i<counterCuvinte;i++)
        for(int j=i+1;j<counterCuvinte;j++)
        {   if(vectorCuvinte[i]!=NULL && vectorCuvinte[j]!=NULL)
                if(strcmp(vectorCuvinte[i],vectorCuvinte[j])==0)
                {
                    vectorFrecventa[i]++;
                    vectorCuvinte[j]=NULL;
                }
        }
    for(int i=0;i<counterCuvinte;i++)
        indexes[i]=i;
    for(int i=0;i<counterCuvinte;i++)    
        for(int j=i+1;j<counterCuvinte;j++)
        {
            if(vectorFrecventa[i]<vectorFrecventa[j])
            {
                int aux=vectorFrecventa[i];
                vectorFrecventa[i]=vectorFrecventa[j];
                vectorFrecventa[j]=aux;
                aux=indexes[i];
                indexes[i]=indexes[j];
                indexes[j]=aux;
            }
        }
    int counterWords=0;
    for(int i=0;i<counterCuvinte && counterWords<10;i++)
    {
        if(vectorCuvinte[indexes[i]]!=NULL)
            strcpy((*vectorWord)[counterWords++],vectorCuvinte[indexes[i]]);

    }
}
void construct_fileList(char buffer[])
{
    listaFisiere->counterList=0;
    char** helper=(char**)malloc(100*sizeof(char));
    char*line=__strtok_r(buffer,"\n",helper);
    while(line!=NULL)
    {   
        listaFisiere[listaFisiere->counterList].filepath=strdup(line);
        listaFisiere[listaFisiere->counterList].numarOctetiPath=strlen(line);
        int fd=open(listaFisiere[listaFisiere->counterList].filepath,O_RDONLY);
        char buff[1024];
        listaFisiere[listaFisiere->counterList].numarOctetiContinut=read(fd,buff,1024); 
        close(fd);

        constructWordFrequency(&listaFisiere[listaFisiere->counterList].words,listaFisiere[listaFisiere->counterList].filepath);
        listaFisiere->counterList++;
        line=__strtok_r(NULL,"\n",helper);
    }

}
void addElement(char* buffer)
{   pthread_mutex_lock(&mutexFileList);
    listaFisiere[listaFisiere->counterList].filepath=strdup(buffer);
    listaFisiere[listaFisiere->counterList].numarOctetiPath=strlen(buffer);
    constructWordFrequency(&listaFisiere[listaFisiere->counterList].words,listaFisiere[listaFisiere->counterList].filepath);
    listaFisiere->counterList++;
    pthread_mutex_unlock(&mutexFileList);

    
}
void moveElement(char*oldBuffer,char*newBuffer)
{
    pthread_mutex_lock(&mutexFileList);
    for(int i=0;i<listaFisiere->counterList;i++)
        if(strcmp(oldBuffer,listaFisiere[i].filepath)==0)
            listaFisiere[i].filepath=strdup(newBuffer);
    if(listaFisiere->counterList==0)
        listaFisiere->counterList++;
    pthread_mutex_unlock(&mutexFileList);
}
void removeElement(char*buffer)
{   pthread_mutex_lock(&mutexFileList);
    int poz;
    for(int i=0;i<listaFisiere->counterList;i++)
        if(strcmp(listaFisiere[i].filepath,buffer)==0)
            poz=i;
    for(int i=poz;i<listaFisiere->counterList;i++)
        listaFisiere[i]=listaFisiere[i+1];
    listaFisiere->counterList--;
    pthread_mutex_unlock(&mutexFileList);
}
char* getDirectories(char*path)
{   
    char* lastPosition=strrchr(path,'/');
    char*toReturn=(char*)malloc(sizeof(char)*250);
    for(int i=0;i<lastPosition-path;i++)
        toReturn[i]=path[i];
    return toReturn;
}
char*execute_delete(char*messageReceived)
{   
    char*bufferToReturn=(char*)malloc(sizeof(char)*3);
    pthread_mutex_lock(&mutexUpdate);
    threadWakeUp=1;
    pthread_cond_signal(&cond);
    char** safety=(char**)malloc(sizeof(char*)*10);
    char*p=__strtok_r(messageReceived,",",safety);
    p=__strtok_r(NULL,",",safety);
    p=__strtok_r(NULL,",",safety);
    writeInLogger("DELETE",p,NULL);
    if(remove(p)==0)
    {   strcpy(pathForExecute,p);
        flagForUpdate=DELETE_TYPE;
        snprintf(bufferToReturn,2,"%c",(char)(0x0+60));
    }
    else
    {
    snprintf(bufferToReturn,2,"%c",(char)(0x1+60));
    }

    pthread_mutex_unlock(&mutexUpdate);
    return bufferToReturn;
}
bool findInFile(char* word,const char*file)
{
    int fd=open(file,O_RDONLY,0);
    char buffer[2048];
    int bytesRead=read(fd,buffer,2048);        
    if(bytesRead<0)
    {
        printf("Eroare la citire");
    }
    char**safety=(char**)malloc(sizeof(char*));
    char*p =__strtok_r(buffer," ",safety);
    while(p!=NULL)
    {
        if(strcmp(p,word)==0)
            return true;
        p=__strtok_r(NULL," ",safety);
    }

    close(fd);
    return false;
}
char* execute_search(char*messageReceived)
{
    char*bufferToReturn=(char*)malloc(sizeof(char)*3);
    char**safety=(char**)malloc(sizeof(char*));
    char*p=__strtok_r(messageReceived,",",safety); //cod
    p=__strtok_r(NULL,",",safety); //numarOcteti
    p=__strtok_r(NULL,",",safety); //cuvant
    writeInLogger("SEARCH",NULL,p);
    snprintf(bufferToReturn,2,"%c",(char)(0x0+60));
    strcat(bufferToReturn,",");
    for(int i=0;i<listaFisiere->counterList;i++)
    {   bool find=false;
        for(int j=0;j<10;j++)
        {
            if(strcmp(p,listaFisiere[i].words[j])==0)
            {
                strcat(bufferToReturn,listaFisiere[i].filepath);
                strcat(bufferToReturn,",");
                find=true;
                break;
            }
        }
        if(!find)
        {
            if(findInFile(p,listaFisiere[i].filepath))
            {
                strcat(bufferToReturn,listaFisiere[i].filepath);
                strcat(bufferToReturn,",");
            }
        }
    }
    return bufferToReturn;

}
char* execute_move(char*messageReceived)
{
    char*bufferToReturn=(char*)malloc(sizeof(char)*3);
    pthread_mutex_lock(&mutexUpdate);
    threadWakeUp=1;
    pthread_cond_signal(&cond);
    char**safety=(char**)malloc(sizeof(char*)*10);
    char*p=__strtok_r(messageReceived,",",safety); //id
    p=__strtok_r(NULL,",",safety); //octeti oldpath
    p=__strtok_r(NULL,",",safety); //oldpath;
    strcpy(oldPath,p);
    p=__strtok_r(NULL,",",safety); //octeti newPath
    p=__strtok_r(NULL,",",safety); //newPath
    strcpy(pathForExecute,p); //am copiat pentru a executa functia in handleUpdate
    char*directories=getDirectories(p);
    int pid=fork();
    if(pid>0)
    {   bool pathExist=false;
        for(int i=0;i<listaFisiere->counterList;i++)
            if(strcmp(oldPath,listaFisiere[i].filepath)==0)
            {
                pathExist=true;
                break;

            }
        if(pathExist)
        {
            flagForUpdate=MOVE_TYPE;
            int fd=open(pathForExecute,O_CREAT | O_WRONLY);
            int fd2=open(oldPath,O_RDONLY);
            char buffer[2048];
            int bytesRead=read(fd2,buffer,2048);         
            int bytesWrite=write(fd,buffer,bytesRead);
            close(fd);
            close(fd2);
            remove(oldPath);
            

            snprintf(bufferToReturn,2,"%c",(char)(0x0+60));
        }
        else
        {
            snprintf(bufferToReturn,2,"%c",(char)(0x1+60));
        }
        wait(NULL);
    }
    
    if(pid==0)
    {
        execlp("mkdir","mkdir",directories,"-p",(char*)NULL);
    }
    




    pthread_mutex_unlock(&mutexUpdate);
    return bufferToReturn;
}
char* execute_update(char* messageReceived)
{
    char*bufferToReturn=(char*)malloc(sizeof(char)*3);
    pthread_mutex_lock(&mutexUpdate);
    threadWakeUp=1;
    pthread_cond_signal(&cond);
    char**safety=(char**)malloc(sizeof(char*));
    char*word=__strtok_r(messageReceived,",",safety);
    word=__strtok_r(NULL,",",safety); //numarOctetiPath
    word=__strtok_r(NULL,",",safety);   //filepath
    writeInLogger("UPDATE",word,NULL);
    char*path=strdup(word);
    int fd=open(word,O_RDWR);
    bool exist=false;
    for(int i=0;i<listaFisiere->counterList;i++)
        if(strcmp(listaFisiere[i].filepath,word)==0)
            exist=true;
        if(!exist)
        {   
            snprintf(bufferToReturn,2,"%c",(char)(0x1+60));
            close(fd);
            return bufferToReturn;
        }
    word=__strtok_r(NULL,",",safety); //octet start
    int octetStart=atoi(word);
    word=__strtok_r(NULL,",",safety); //dimensiune
    int dimensiune=atoi(word);
    word=__strtok_r(NULL,",",safety); //caractere
    char* characters=(char*)malloc(sizeof(char)*dimensiune);
    for(int i=0;i<dimensiune && i<strlen(word);i++)
        characters[i]=word[i];
    int seek=lseek(fd,octetStart,SEEK_SET);
    startWriting(listaFisiere[GetFileIndex(path)]);
    int bytesWrite=write(fd,characters,strlen(characters));    //sincronizare
    finishWriting(listaFisiere[GetFileIndex(path)]);
    
    if(bytesWrite<0)
    {
        snprintf(bufferToReturn,2,"%c",(char)(0x40+60));
    }
    else
    {
        snprintf(bufferToReturn,2,"%c",(char)(0x00+60));
        flagForUpdate=UPDATE_TYPE;
        strcpy(pathForExecute,path);
        

    }
    close(fd);
    
    

    
    pthread_mutex_unlock(&mutexUpdate);
    return bufferToReturn;

    
}
void handle_signal(int signo)
{   

    if(signo==SIGTERM || signo==SIGINT)
    {   printf("Server-ul se inchide!");
        pthread_mutex_lock(&mutexClose);
        inchidere=1;
        pthread_cond_signal(&condClose);
        pthread_mutex_unlock(&mutexClose);
    }

}

char* execute_command(char*messageReceived,struct threadArgs* thArgs)
{
    char* copieMesaj=(char*)malloc(250);
    strcpy(copieMesaj,messageReceived);
    thArgs->saveptr=(char**)malloc(sizeof(char*)*strlen(copieMesaj));
    char*p=__strtok_r(copieMesaj,",",thArgs->saveptr);
    uint32_t code=(int)p[0]-60;
    if(code==0x0)
    {   
        if(execute_LIST()==NULL)
        {   strcpy(copieMesaj,"");
            copieMesaj[0]=(char)(0x40+60);
        }
        else {  strcpy(copieMesaj,"");
                copieMesaj[0]=(char)(0x0+60);
                strcat(copieMesaj,",");
                strcat(copieMesaj,execute_LIST());

        }
        
    }
    if(code==0x1)
    {   snprintf(copieMesaj,10,"%d",code);
        strcpy(copieMesaj,execute_download(messageReceived));
    }
    if(code==0x2)
    {   snprintf(copieMesaj,10,"%d",code);
        strcpy(copieMesaj,execute_upload(messageReceived));
    }
    if(code==0x4)
    {    snprintf(copieMesaj,10,"%d",code);
         strcpy(copieMesaj,execute_delete(messageReceived));
    }
    if(code==0x8)
    {   snprintf(copieMesaj,10,"%d",code);
        strcpy(copieMesaj,execute_move(messageReceived));
    }
    if(code==0x10)
    {    snprintf(copieMesaj,10,"%d",code);
         strcpy(copieMesaj,execute_update(messageReceived));
    }
    if(code==0x20)
    {   
        snprintf(copieMesaj,10,"%d",code);
        strcpy(copieMesaj,execute_search(messageReceived));
    }
    return copieMesaj;
}

void *handleClient(void *args) {
    struct threadArgs* thArgs=(struct threadArgs*)args;
    char messageReceived[250];
    printf("Client conectat: %d\n", thArgs->clientSocket);

    while(1) {
        memset(messageReceived, 0, sizeof(messageReceived));
        int bytes_received = recv(thArgs->clientSocket, messageReceived, sizeof(messageReceived), 0);
        char* messageToSend=execute_command(messageReceived,thArgs);

        if(bytes_received > 0) {
            
            send(thArgs->clientSocket, messageToSend,strlen(messageToSend), 0);
          
        }
    }
}
void constructInitialList()
{   
    int pipe_fd[2];
    int ret=pipe(pipe_fd);
    if(ret<0)
    {
        perror("Eroare la construirea pipe-ului");
        return;
    }
    int pid=fork();
    if(pid>0)
    {
        close(pipe_fd[1]);
        char buffer[1024];
        int bytesRead=read(pipe_fd[0],buffer,1024);
        pthread_mutex_lock(&mutexFileList);
        construct_fileList(buffer);
        pthread_mutex_unlock(&mutexFileList);
        wait(NULL);

    }
    if(pid==0)
    {
        close(pipe_fd[0]);
        dup2(pipe_fd[1],1);
        execlp("find", "find", "serverFolder", "-type", "f", (char *)NULL);
    }
}
void* handleExit(void*args)
{   
        pthread_mutex_lock(&mutexClose);
        while (inchidere==0) {
            pthread_cond_wait(&condClose, &mutexUpdate);    //thread-ul "adoarme"
        } 
        SeExecuta=0;
        for(int i=0;i<numarClienti;i++)
            pthread_join(threadClienti[i],NULL);
        pthread_join(threadUPDATE,NULL);
        pthread_mutex_unlock(&mutexClose);
}
void* handleUpdate(void*args)
{
    constructInitialList();
    while(1)
    {
        pthread_mutex_lock(&mutexUpdate);
        while (threadWakeUp==0) {
            pthread_cond_wait(&cond, &mutexUpdate);    //thread-ul "adoarme"
        } 
        threadWakeUp = 0;
        pthread_mutex_unlock(&mutexUpdate);
        pthread_mutex_lock(&mutexUpdate);
        printf("Thread-ul de update s-a trezit si executa operatia %d\n",flagForUpdate);
        fflush(stdout);
        if(flagForUpdate==UPLOAD_TYPE)
        {
            addElement(pathForExecute); // adauga in lista
        }
        if(flagForUpdate==DELETE_TYPE)
        {
            removeElement(pathForExecute);
        }
        if(flagForUpdate==MOVE_TYPE)
        {
            removeElement(oldPath);
            addElement(pathForExecute);
        }
        if(flagForUpdate==UPDATE_TYPE)
        {
            updateFile(pathForExecute);
        }
        pthread_mutex_unlock(&mutexUpdate);
    }
    

}

void writeInLogger(char*operatie,char*file,char*word)
{
    pthread_mutex_lock(&mutexLogger);
    time_t timpCurent;
    struct tm *infoTimp;
    
    time(&timpCurent);
    
    infoTimp = localtime(&timpCurent);
    char day[2];
    char month[2];
    char hour[2];
    char minute[2];
    char sec[2];
    snprintf(day,2,"%d",infoTimp->tm_mday);
    snprintf(month,2,"%d",infoTimp->tm_mon);
    snprintf(hour,2,"%d",infoTimp->tm_hour);
    snprintf(minute,2,"%d",infoTimp->tm_min);
    snprintf(sec,2,"%d",infoTimp->tm_sec);
    if(word!=NULL)
        fprintf(logger,"%s-%s %s:%s:%s %s %s\n",day,month,hour,minute,sec,operatie,word);
    else
        if(file!=NULL)
            fprintf(logger,"%s-%s %s:%s:%s %s %s\n",day,month,hour,minute,sec,operatie,file);
    else fprintf(logger,"%s-%s %s:%s:%s %s\n",day,month,hour,minute,sec,operatie);

    pthread_mutex_unlock(&mutexLogger);
}


int main() {
    logger=fopen("logger.txt","w+");
    if (logger == NULL) {
        perror("Eroare la deschiderea fișierului de logare");
        return 1;
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Eroare la crearea socket-ului");
        return 1;
    }

    int clientSocket[MAX_CLIENTI];
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Eroare la legarea socket-ului la adresa și port");
        return 1;
    }

    if (listen(serverSocket, MAX_CLIENTI) == -1) {
        perror("Eroare la ascultarea conexiunilor");
        return 1;
    } else {
        printf("Serverul așteaptă conexiuni!\n");
    }

    // Crează descriptorul epoll
    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("Eroare la crearea descriptorului epoll");
        return 1;
    }

    // Adaugă serverSocket la epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = serverSocket;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSocket, &event) == -1) {
        perror("Eroare la adăugarea serverSocket la epoll");
        close(epollfd);
        return 1;
    }

    // Adaugă descriptorul de semnal la epoll
    int signalFd[2];
    if (pipe(signalFd) == -1) {
        perror("Eroare la crearea pipe-ului pentru semnal");
        close(epollfd);
        return 1;
    }

    event.events = EPOLLIN;
    event.data.fd = signalFd[0];
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, signalFd[0], &event) == -1) {
        perror("Eroare la adăugarea descriptorului de semnal la epoll");
        close(epollfd);
        close(signalFd[0]);
        close(signalFd[1]);
        return 1;
    }

  

    // Lansează thread-urile
    pthread_create(&threadUPDATE, NULL, &handleUpdate, NULL);
    pthread_create(&threadSIGNAL, NULL, &handleExit, NULL);

    while (SeExecuta) {
        struct epoll_event events[MAX_CLIENTI + 1];
        int nfds = epoll_wait(epollfd, events, MAX_CLIENTI + 1, -1);
        if (nfds == -1) {
            perror("Eroare la așteptarea evenimentelor epoll");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == serverSocket) {
                // Acțiune asociată cu serverSocket (conexiune nouă, date primite, etc.)
                struct sockaddr_in clientAddr;
                socklen_t clientAddrLen = sizeof(clientAddr);
                clientSocket[numarClienti] = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
                if (clientSocket[numarClienti] == -1) {
                    perror("Eroare la acceptarea conexiunii");
                    continue;
                }

                if (numarClienti < MAX_CLIENTI) {
                    struct threadArgs* thArg = (struct threadArgs*)malloc(sizeof(struct threadArgs));
                    thArg->clientSocket = clientSocket[numarClienti];
                    pthread_create(&threadClienti[numarClienti], NULL, &handleClient, thArg);
                    pthread_mutex_lock(&mutex);
                    numarClienti++;
                    pthread_mutex_unlock(&mutex);
                } else {
                    printf("Serverul este ocupat. Refuzarea conexiunii clientului %d.\n", clientSocket[numarClienti]);
                    char buff[2];
                    snprintf(buff, 2, "%c", (char)(0x8 + 40));
                    send(clientSocket[numarClienti], buff, 2, 0);
                    close(clientSocket[numarClienti]);
                }
            } else if (events[i].data.fd == signalFd[0]) {
                // Acțiune asociată cu semnalul
                char buffer[1];
                if (read(signalFd[0], buffer, sizeof(buffer)) == -1) {
                    perror("Eroare la citirea din descriptorul de semnal");
                } else {
                    handle_signal(buffer[0]);
                }
            }
        }
    }

    // Închide descriptorii și termină thread-urile
    close(serverSocket);
    close(epollfd);
    close(signalFd[0]);
    close(signalFd[1]);

    fclose(logger);

    return 0;
}