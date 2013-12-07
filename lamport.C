#include<sys/types.h>
#include <algorithm>
#include <stdlib.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<cstdio>
#include<unistd.h>
#include <dirent.h>
#include<cstring>
#define MAXLEN 1000

int filterByChoosing(const struct dirent* entry);
int filterByNumber(const struct dirent* entry);

class ProcessMutex{
private:    
    char mChoosingFileName[MAXLEN]; //the path of the choosing file for this process
    char mNumberFileName[MAXLEN];   //the path to the number file of this process
    char fileDirPath[MAXLEN];       //the directory path where the lock files are placed.
    int number ;                    //the last number that this process wrote into its file.
    int pid ;                       //the process id of this process.
    bool lockAcquired ;
public:
    ProcessMutex(const char* mFileDirPath, const char * hostname, int pid);
    ~ProcessMutex();
    void lock();
    void release();
private:
    int getNumberFromFile(const char* filepath);
    int getPidFromFileName(const char* filepath);

};

ProcessMutex::ProcessMutex(const char *dir_path, const char* hostname, int process_id){
    this->number = 0;
    this->pid = process_id;
    //build the file name for this process
    strcpy(fileDirPath,dir_path);
    strcpy(mNumberFileName,fileDirPath);
    strcat(mNumberFileName,"/");
    char pidstring[MAXLEN];
    sprintf(pidstring,"%d:",pid);
    strcat(mNumberFileName,pidstring);
    strcat(mNumberFileName,hostname);
    strcpy(mChoosingFileName,mNumberFileName);
    strcat(mChoosingFileName,".choosing");
    strcat(mNumberFileName,".number");
    lockAcquired = false;
    printf("\nFile directory : %s",fileDirPath);
    printf("\nChoosing path : %s",mChoosingFileName);
    printf("\nNumber path : %s",mNumberFileName);
}
ProcessMutex::~ProcessMutex(){
    remove(mChoosingFileName);
    remove(mNumberFileName);
}
void ProcessMutex::lock(){
    
    char filePath[2*MAXLEN];

    //first signal your intention to acquire a lock by creating a .choosing file
    int chooseFd = open(mChoosingFileName,O_CREAT,S_IRWXU|S_IROTH);
    close(chooseFd);    

    //loop through all the number files sorted in pid order, get the max number and write your own number file.
    struct dirent** numberList;
    int nfiles = scandir(fileDirPath,&numberList,filterByNumber,alphasort);
    for(int i=0; i< nfiles;i++){
        strcpy(filePath,fileDirPath);
        strcat(filePath,"/");
        strcat(filePath,numberList[i]->d_name);
        this->number = std::max(this->number,getNumberFromFile(filePath));
        free(numberList[i]);
    }
    (this->number)++;
    int numFd = open(mNumberFileName,O_CREAT|O_WRONLY,S_IRWXU|S_IROTH);
    int wc=write(numFd,&(this->number),sizeof(int));
    //printf("\nThe write code is %d",wc);
    if(wc < 0 )
        perror(NULL);
    close(numFd);

    //remove your .choosing file
    remove(mChoosingFileName);
    printf("\nChose the number %d",this->number);

    //now loop through all the .choosing files, sorted in pid order and check if they exist
    struct dirent** choosingList;
    int cfiles = scandir(fileDirPath,&choosingList,filterByChoosing,alphasort);
    for(int i =0 ; i < cfiles; ++i){
        strcpy(filePath,fileDirPath);
        strcat(filePath,"/");
        strcat(filePath,choosingList[i]->d_name);
        free(choosingList[i]);
        while(access(filePath,F_OK)!=-1){;
            /* wait as long as the choosing file for this process exists */
            printf("\nWaiting for prcess  :%s to chose",filePath);
       }
       /* wait as long as processes with number smaller than yours are in their critical sections */
        int fileNumber = 0;
        int filePid = 0;
        do{
            fileNumber = getNumberFromFile(filePath);
            filePid = getPidFromFileName(filePath);
            printf("\nGot file number %d and process number %d ",fileNumber,filePid);
        }while(fileNumber!=0 && ((fileNumber<this->number)||((fileNumber==this->number)&&(filePid<this->pid))));
    }

    
    //if you reach here you have got the lock !
    lockAcquired = true;
    return ;

        
}
void ProcessMutex::release(){
    //set the number file to zero.
    this->number = 0;
    remove(mNumberFileName);
    lockAcquired = false;
}
inline int ProcessMutex::getPidFromFileName(const char* filePath){
    char pidstring[MAXLEN];
    int len  =  strrchr(filePath,':') -strrchr(filePath,'/')-1;
    strncpy(pidstring,strrchr(filePath,'/')+1,len);
    pidstring[len] = '\0';
    //printf("\nThe pid string from filename is %s of length %d",pidstring,len);
    return atoi(pidstring);
}

inline int ProcessMutex::getNumberFromFile(const char * chfilePath){
    char filePath[MAXLEN];
    if(strcmp(strrchr(chfilePath,'.'),".choosing")==0){
        int len = strrchr(chfilePath,'.')-chfilePath +1;
        strncpy(filePath,chfilePath,len);
        strcpy(filePath+len,"number");
    }
    else 
        strcpy(filePath,chfilePath);
    //printf("\nThe spliced number file path is %s ",filePath);
    int fd = open(filePath,O_RDONLY);
    if(fd==-1)
        return 0;
    int readNumber=-1;
    int rc = read(fd,(void*)&readNumber,sizeof(int));
    //printf("\nThe read code is %d",rc);
    if(rc < 0 )
        perror(NULL);
    if(rc==0)
        readNumber =0;
    close(fd);
    return readNumber;
}
int filterByChoosing(const struct dirent* entry){
    if(strcmp(strrchr(entry->d_name,'.'),".choosing")==0)
        return 1;
    else return 0;
}
int filterByNumber(const struct dirent* entry){
    if(strcmp(strrchr(entry->d_name,'.'),".number")==0)
        return 1;
    else return 0;
}

int main(int argc , char * argv[]){

    setbuf(stdout,NULL);
    setbuf(stderr,NULL);
    char hostnamestr[MAXLEN];
    int sleepsecs = atoi(argv[1]);
    gethostname(hostnamestr,MAXLEN);
    ProcessMutex mutex("/home/adityar/sbox/testMutex/",hostnamestr,getpid());
    int i = 1000;
    while(i--){
        mutex.lock();
        printf("\n---------------------Entered critical section---------------------");

       //to test for mutual exclusion , try and create and then remove the same file inside each process's critical section. only the process has the permission to do this so that
        //any other process will fail. if any of the parallel processes fails, then our critical section is not safe and the algorithm does not work.
        int testfd = open("/home/adityar/sbox/lamport/resource.file",O_CREAT|O_EXCL,S_IRWXU);
        if(testfd < 0)
            printf("\nRace condition detected");
        close(testfd);
        //sleep for a certain number of seconds to see if other processes try and enter your critical section
        //sleep(sleepsecs);
        remove("/home/adityar/sbox/lamport/resource.file");
        //FILE * resource = fopen("/home/adityar/sbox/lamport/resource.file","a");
        //printf("\n\t\t\t sleeping for %d seconds.",sleepsecs);
        //fprintf(resource,"\nProcess no: %d wrote 1",getpid());
        //fprintf(resource,"\nProcess no: %d wrote 2",getpid());
        //fclose(resource);
       // sleep(sleepsecs);
        mutex.release();
        printf("\n---------------------Left critical section-------------------------------");
    }



}
