#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<signal.h>
#include<time.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/wait.h>
#include<sys/errno.h>

extern int errno;       // error NO.
#define PIDFILE "/var/run/smsd.pid"   // PID file
#define GSMEXEC "/usr/bin/gsmsendsms"   // gsmsendsms path
#define NRETRY 5   // # of retry
#define MSGPERM 0600    // msg queue permission
#define MSGTXTLEN 1024   // msg text length
#define MSGPHONELEN 15   // msg phone length

int msgqid, rc, run;
FILE *fp, *fl;

struct msg_buf{
    long mtype;
    char mphone[MSGPHONELEN];
    char mtext[MSGTXTLEN];
} msg;

void sig_handler(int signo){
    if (access( PIDFILE, F_OK )!=-1)
        run=0;
}

int return_check(FILE* fd){
    char buff[4096], error[]="/usr/bin/gsmsendsms[ERROR]: unexpected response \'+CME ERROR: 100\' when sending \'AT+CMGS=";
    if(fgets(buff, 4096, fd)==NULL||strncmp(buff,error,88)!=0){
        printf("[SMSD] Error: %s\n",buff);
        return 1;
    }
    printf("[SMSD] Ok: %s\n",buff);
    return 0;
}

int main(int argc,char **argv){
    struct sigaction sigIntHandler;
    int nf, retry=0;
    char cmd[256];
    pid_t process_id = 0;
    pid_t sid = 0;
    if(geteuid() != 0){
        printf("run program as root\n");
        exit(1);
    }
    if( access( PIDFILE, F_OK ) != -1 ){
        printf("a smsd daemon is already running\n");
        exit(1);
    }
    // Create child process
    process_id = fork();
    // Indication of fork() failure
    if (process_id < 0){
        printf("fork failed!\n");
        // Return failure in exit status
        exit(1);
    }
    // PARENT PROCESS. Need to kill it.
    if (process_id > 0){
        fp = fopen (PIDFILE, "w+");
        fprintf(fp, "%d\n", process_id);
        fclose(fp);
        exit(0);
    }
    //unmask the file mode
    umask(0);
    //set new session
    sid = setsid();
    if(sid < 0){
        // Return failure
        exit(1);
    }
    // Change the current working directory to root.
    chdir("/");
    // Close stdin. stdout and stderr
    close(STDIN_FILENO);
    // Open a log file in write mode.
    fl = popen("logger","w");
    if(fl == NULL)
        return 1;
    nf = fileno(fl);
    dup2(nf,STDOUT_FILENO);
    dup2(nf,STDERR_FILENO);
    setbuf(stdout,NULL);
    sigIntHandler.sa_handler = sig_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGKILL, &sigIntHandler, NULL);
    sigaction(SIGSTOP, &sigIntHandler, NULL);
    sigaction(SIGABRT, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);

    // create a message queue. If here you get a invalid msgid and use it in msgsnd() or msgrcg(), an Invalid Argument error will be returned.
    key_t key = ftok("/var/run/smsd.pid",1);
    if(key<0){
        perror(strerror(errno));
        printf("[SMSD] failed to create a key\n");
        remove(PIDFILE);
        return 1;
    }
    msgqid = msgget(key, MSGPERM|IPC_CREAT);
    if(msgqid<0){
        printf("[SMSD] failed to create message queue with msgqid = %d, %s\n", msgqid, strerror(errno));
        remove(PIDFILE);
        return 1;
    }
    printf("[SMSD] daemon started\n");
    run=1;
    while(run){
        // read the message from queue
        rc = msgrcv(msgqid, &msg, sizeof(msg)-sizeof(long), 1, IPC_NOWAIT);
        if (rc < 0 && errno!=ENOMSG) {
            perror( strerror(errno) );
            printf("[SMSD] msgrcv failed, rc=%d, %d\n", rc, errno);
            remove(PIDFILE);
            return 1;
        }else if(rc>=0){
            sprintf (msg.mtext, "%.143s", msg.mtext);
            for(retry=0; retry<NRETRY; retry++){
                memset(cmd,0,strlen(cmd));
                printf("[SMSD] %d\\%d to=%s msg=%s\n", retry+1, NRETRY, msg.mphone, msg.mtext);
                strcpy(cmd,GSMEXEC);
                strcat(cmd," \"");
                strcat(cmd,msg.mphone);
                strcat(cmd,"\" \"");
                strcat(cmd,msg.mtext);
                strcat(cmd,"\" 2>&1");
                //int r = system(cmd);
                FILE* test = popen(cmd, "r");
                if(test==NULL||return_check(test)){
                    printf("[SMSD] error sending sms\n");
                }else{
                    printf("[SMSD] sms sent\n");
                    break;
                }
        	    pclose(test);
        	    sleep(1);
            }
        }else
            sleep(1);
    }

    // remove the queue
    rc=msgctl(msgqid,IPC_RMID,NULL);
    if (rc < 0) {
        perror( strerror(errno) );
        printf("[SMSD] msgctl (return queue) failed, rc=%d\n", rc);
        remove(PIDFILE);
        return 1;
    }
    printf("[SMSD] daemon exited\n");
    remove(PIDFILE);
    return 0;
}
