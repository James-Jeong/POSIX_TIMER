#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
#include<time.h>
#include<unistd.h>

#define SIGTIMER SIGRTMAX
#define ARRAY_NUM 10
#define THREAD_NUM 3

struct TCB{
	int period;
	int thread_id;
	int time_left_to_invoke;
	pthread_cond_t condition;
};

int a[THREAD_NUM];
pthread_mutex_t mutex;
struct TCB TCB_array[ARRAY_NUM];
timer_t timer[THREAD_NUM];

void set_Timer(int sig_no, int sec, int timer_id){
	struct sigevent sig_ev;
	struct itimerspec itval, oitval;

	sig_ev.sigev_notify = SIGEV_SIGNAL;
	sig_ev.sigev_signo = sig_no;
	sig_ev.sigev_value.sival_ptr = &timer[timer_id];

	if(timer_create(CLOCK_REALTIME, &sig_ev, &timer[timer_id]) == 0){
		itval.it_value.tv_sec = 0;
		itval.it_value.tv_nsec = (long)(sec / 10) * (1000000L);
		itval.it_interval.tv_sec = 0;
		itval.it_interval.tv_nsec = (long)(sec / 10) * (1000000L);
		if(timer_settime(timer[timer_id], 0, &itval, &oitval) != 0){
			perror("\ntime_settime ERROR: "); exit(-1);
		}
	}
	else{ perror("\ntimer_create ERROR: "); exit(-1); }
}

void signal_Handler(int sig_no, siginfo_t *info, void* context){
	if(sig_no == SIGTIMER){
		pthread_mutex_lock(&mutex);
		for(int i = 0; i < THREAD_NUM; i++){
			TCB_array[i].time_left_to_invoke -= 1;
			if((TCB_array[i].time_left_to_invoke -= 1) <= 0){
				TCB_array[i].time_left_to_invoke = TCB_array[i].period;
				pthread_cond_signal(&TCB_array[i].condition);
				//printf("Send signal: %d\n", i);
			}
		}
		pthread_mutex_unlock(&mutex);
	}
	else if(sig_no == SIGINT){
		for(int i = 0; i < THREAD_NUM; i++){
			timer_delete(timer[i]);
			perror("\nCtrl + C: "); exit(1);
		}
	}
}

void tt_Thread_Register(int _period, int _thread_id){
	pthread_mutex_lock(&mutex);
	TCB_array[_thread_id].period = _period;
	TCB_array[_thread_id].thread_id = _thread_id;
	TCB_array[_thread_id].time_left_to_invoke = _period;
	pthread_cond_init(&TCB_array[_thread_id].condition, NULL);
	pthread_mutex_unlock(&mutex);
}

int tt_Thread_Invocation(int _thread_id){
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&TCB_array[_thread_id].condition, &mutex);
	//printf("Receive signal: %d\n", _thread_id);
	pthread_mutex_unlock(&mutex);
	return -1;
}

void* tt_Thread(void* _args){
	struct tm *t;
	timer_t timer;
	int ta = *(int*)(_args);
	//printf("pthread_self(): %lu\n", pthread_self());
	//printf("period: %d, thread_id: %d\n", ta, ta-1);
	tt_Thread_Register(ta, ta-1);
	while(tt_Thread_Invocation(ta-1)){
		timer = time(NULL);
		t = localtime(&timer);
		char current_time[20];
		sprintf(current_time, "%04d-%02d-%02d %02d:%02d:%02d", 
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
			t->tm_hour, t->tm_min, t->tm_sec);	
		printf("TIME  : %s\n", current_time);
		printf("TH_ID : %d\n\n", ta-1);
	}
	free(t);
	return NULL;
}

int main(){
	pthread_t th[THREAD_NUM];
	int mutex_state = pthread_mutex_init(&mutex, NULL);
	if(mutex_state){ perror("\nMUTEX CREATE ERROR: "); exit(-1); }

	for(int i = 0; i < THREAD_NUM; i++){ a[i] = i+1; }

	for(int i = 0; i < THREAD_NUM; i++){
		if((pthread_create(&th[i], NULL, tt_Thread, (void*)(&a[i]))) != 0){
			perror("\nPTHREAD CREATE ERROR: "); exit(-1);
		}
	}

	struct sigaction sig_act;
	sigemptyset(&sig_act.sa_mask);
	sig_act.sa_flags = SA_SIGINFO;
	sig_act.sa_sigaction = signal_Handler;
	if(sigaction(SIGTIMER, &sig_act, NULL) == -1){ perror("\nSIGTIMER SIGACTION FAIL: "); exit(-1); }
	if(sigaction(SIGINT, &sig_act, NULL) == -1){ perror("\nSIGINT SIGACTION FAIL: "); exit(-1); }

	for(int i = 0; i < THREAD_NUM; i++){
		set_Timer(SIGTIMER, 100, i);
	}

	//while(1){ pause(); };

	for(int i = 0; i < THREAD_NUM; i++){
		pthread_join(th[i], NULL);
	}

	pthread_mutex_destroy(&mutex);
}
