#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"

void doWark();
void dummy_handler();
void dummy_handler2();
void test_sig_kill();
void test_sigprocmask();
void test_signal();
void test_sig_stop_cont();
void test_override_sig_kill();
void test_signal_communication();
void test_sig_kill_communication();

int main(int argc, char *argv[]){
	switch (atoi(argv[1])){
		case 1:
			test_sig_stop_cont();
			break;
		case 2:
			test_sig_kill();
			break;
		case 3:
			test_sigprocmask();
			break;
		case 4:
			test_signal();
			break;
		case 5:
			test_override_sig_kill();
			break;
		case 6:
			test_signal_communication();
			break;
		case 7:
			test_sig_kill_communication();
			break;
		default:
			printf(1,"There are 7 test function, but got : %d!!!",atoi(argv[1]));
	}
     exit();
}

void doWork(int pid){
	printf(1, "Pid '%d' started doing work.\n", pid);
	for (int i=0; i<50;i++){
		printf(1,"Pid '%d' is printing %d.\n", pid, i);
	}		
	printf(1, "Pid '%d' finished.\n", pid);
}

void test_sig_stop_cont(){
    printf(1,"Testing SIG_STOP and SIG_CONT.\n");
	int pid = fork();
	
	if(pid == 0){
		printf(1, "New process is running.\n");
		doWork(getpid());
		printf(1, "New process is stopping.\n");
		kill(getpid(),SIGSTOP);
		printf(1,"New process continued.\n");	
		printf(1,"New process finished.\n");
		exit();
	}
	else{
		printf(1,"Original process going to sleep.\n");	
		sleep(1500);
		printf(1,"Original process after sleeping.\n");	
		printf(1,"Original process sending signal -> SIGCONT on son.\n");	
		kill(pid,SIGCONT);
		printf(1,"Original process after sending signal SIGCONT.\n");
		printf(1,"Original process waiting for son to finish.\n");			
		wait();
		printf(1,"Original process finishing work..\n");		
	}
	printf(1,"test_sig_stop_cont SUCCESS!\n");
}

void test_sig_kill(){
	printf(1,"Testing SIG_KILL.\n");
	
    int pid = fork();
    if(pid == 0){
        printf(1, "test_sigKill running...\n");
        kill(getpid(),SIGKILL);
        printf(1, "test_sigKill FAIL!\n");
    } else {
        wait();
        printf(1, "test_sigKill SUCCESS!\n");
    }
}
 
void test_sigprocmask(){
	printf(1,"Testing SIG_PROC_MASK.\n");
	
    int newMask = 1234;
    int oldMask = sigprocmask(newMask);
    if(newMask == sigprocmask(oldMask)){
        printf(1, "test_sigprocmask  SUCCESS!\n");
    } else {
        printf(1, "test_sigprocmask FAIL!\n");
    }
}
 
void test_signal(){
	printf(1,"Testing SIGNAL.\n");
	
    int pid = fork();
    int dummy_signal = 5;
    if(pid == 0){
        if((int)signal(dummy_signal, dummy_handler) != SIG_DFL){
            printf(1,"test_signal FAIL!\n");
        } else {
            kill(getpid(), dummy_signal);
            if(signal(dummy_signal, dummy_handler2) != dummy_handler){
                printf(1, "test_signal FAIL!\n");
            } else {
                kill(getpid(), dummy_signal);
            }
        }
    } else{
        wait();
        printf(1,"test_signal SUCCESS!\n");
    }
}

void test_override_sig_kill(){
	printf(1,"Testing OVERRIDE_SIG_KILL.\n");
	
	int pid = fork();

	if(pid == 0){
		if(signal(SIGSTOP,dummy_handler) < 0){
			printf(1,"test_override_sig_kill FAIL!.\n");
			exit();
		}
		printf(1,"New process is running\n.");
		if(kill(getpid(),SIGSTOP) < 0){
			printf(1,"test_override_sig_kill FAIL!.\n");
			exit();
		}// shouldn't stop
		printf(1,"test_override_sig_kill SUCCESS\n.");
	}

	else{
		wait();
	}
}

void test_signal_communication(){
	printf(1,"Testing SIGNAL_COMMUNICATION.\n");
	
    int pid = fork();
    int dummy_signal = 5;
    if(pid == 0){
        if((int)signal(dummy_signal, dummy_handler) != SIG_DFL){
            printf(1,"test_signal FAIL!\n");
        } else {
            sleep(1000);
            kill(getpid(), SIGKILL);
        }
    } else{
		sleep(100);
        kill(pid, dummy_signal);
        wait();
        printf(1,"test_signal SUCCESS!\n");
    }
}

void test_sig_kill_communication(){
	printf(1,"Testing SIGNAL_KILL_COMMUNICATION.\n");
	
    int pid = fork();
    if(pid == 0){
        while(1);
    } else {
        if(kill(pid,SIGKILL) == -1){
            printf(1, "test_sigKill FAIL!\n");
        }
        wait();
        printf(1, "test_sigKill SUCCESS!\n");
    }
}
 
void dummy_handler(int num){
    printf(1, "dummy_handler called with %d\n", num);
}
 
void dummy_handler2(int num){
    printf(1, "dummy_handler2 called with %d\n", num);
}