#ifndef TIMEWHEEL_H
#define TIMEWHEEL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <pthread.h>

#define TIMEWHEEL_WIDTH	8

#define	TIMEWHEEL_SIZE	(1<<TIMEWHEEL_WIDTH)

typedef struct twtask {
	unsigned int	taskid;
	unsigned int	period;//ms
	unsigned char	flags;

	void		(*cb)(void *arg);
	void		*arg;
}twtask_t;
#define TWTASK_FLAG_EXECONECE	0x0
#define TWTASK_FLAG_CANCELLED	0x1
#define TWTASK_FLAG_PERIODIC	0x2

typedef struct twtasknode {
	twtask_t	task;
	unsigned int 	exec_tick;
	struct twtasknode*	next;
}twtasknode_t;

typedef struct twbucket {
	pthread_mutex_t	lock;
	twtasknode_t*	task_list;
}twbucket_t;

typedef struct timewheel {
	unsigned int	cur_tick;
	int		timer_fd;
	pthread_t	loop_tid;
	pthread_mutex_t	ref_lock;
	unsigned short	ref_count;
	unsigned short	tw_status;

	unsigned char	_ticksize;

	unsigned char	ptrL1;
	unsigned char	ptrL2;
	unsigned char	ptrL3;

	twbucket_t	twL1[TIMEWHEEL_SIZE];
	twbucket_t	twL2[TIMEWHEEL_SIZE];
	twbucket_t	twL3[TIMEWHEEL_SIZE];
}timewheel_t;

#define TW_STATUS_READY		0
#define TW_STATUS_RUNNING	1
#define TW_STATUS_STOPPED	2
#define TW_STATUS_EXITED	3

#define TW_TICKSIZE_1MS		0
#define TW_TICKSIZE_2MS		1
#define TW_TICKSIZE_4MS		2
#define TW_TICKSIZE_8MS		3
#define TW_TICKSIZE_16MS	4
#define TW_TICKSIZE_32MS	5
#define TW_TICKSIZE_64MS	6
#define TW_TICKSIZE_128MS	7
#define TW_TICKSIZE_256MS	8
#define TW_TICKSIZE_512MS	9
#define TW_TICKSIZE_1024MS	10

timewheel_t* tw_new();
void tw_free(timewheel_t *tw);
void tw_init(timewheel_t *tw, unsigned char ticksize);

twtask_t* tw_addtask(timewheel_t *tw, unsigned int timeout_ms,
				void (*cb)(void *arg), void *arg);
void tw_changetask(twtask_t *task, void (*cb)(void *arg), void *arg);
void tw_canceltask(twtask_t *task);
twtask_t* tw_settaskperiod(timewheel_t *tw, twtask_t *task, unsigned int period_ms);

// void tw_nexttick(timewheel_t *tw);

// pthread API
pthread_t tw_runthread(timewheel_t *tw);

// timerfd API
int tw_gettimerfd(timewheel_t *tw);
void tw_proctimerev(timewheel_t *tw);

#endif