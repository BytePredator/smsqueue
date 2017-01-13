#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/wait.h>
#include<sys/errno.h>

extern int errno;       // error NO.
#define PIDFILE "/var/run/smsd.pid"   // PID file
#define MSGPERM 0600    // msg queue permission
#define MSGTXTLEN 1024   // msg text length
#define MSGPHONELEN 15   // msg phone length

int msgqid, rc;
int done;

struct msg_buf{
    long mtype;
    char mphone[MSGPHONELEN];
    char mtext[MSGTXTLEN];
} msg;

int main(int argc,char **argv){
    // create a message queue. If here you get a invalid msgid and use it in msgsnd() or msgrcg(), an Invalid Argument error will be returned.
    if(geteuid() != 0){
        printf("run program as root\n");
        exit(1);
    }
    if(argc<3){
        printf("USAGE: msg_send NUMBER MESSAGE");
        return 1;
    }else{
        // message to send
        msg.mtype = 1; // set the type of message
        sprintf (msg.mphone, "%s", argv[1]); // set the phone of sms
        sprintf (msg.mtext, "%s", argv[2]); /* setting the right time format by means of ctime() */
    }
    if( access( PIDFILE, F_OK ) == -1 ){
        printf("smsd daemon isn't running\n");
        system("./smsd");
        printf("trying to start daemon\n");
        sleep(1);
        if( access( PIDFILE, F_OK ) == -1 ){
            printf("no, it doesn't want. sry :(\n");
            exit(1);
        }
    }
    key_t key = ftok("/var/run/smsd.pid",1);
    if(key<0){
        perror(strerror(errno));
        printf("failed to create a key\n");
        return 1;
    }
    msgqid = msgget(key, MSGPERM);
    if(msgqid==ENOENT){
        perror(strerror(errno));
        printf("message queue doesn't exist\nis the daemon running?\n");
        return 1;
    }else if(msgqid < 0){
        perror(strerror(errno));
        printf("failed to connect to message queue with msgqid = %d\n", msgqid);
        return 1;
    }

    // send the message to queue
    rc = msgsnd(msgqid, &msg, sizeof(msg)-sizeof(long), 0); // the last param can be: 0, IPC_NOWAIT, MSG_NOERROR, or IPC_NOWAIT|MSG_NOERROR.
    if (rc < 0) {
        perror( strerror(errno) );
        printf("msgsnd failed, rc = %d\n", rc);
        return 1;
    }
    printf("msg sent to daemon\n");
    return 0;
}
