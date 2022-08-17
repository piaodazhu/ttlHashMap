#include "timewheel.h"

#define L1SHIFT	(TIMEWHEEL_WIDTH * 2)
#define L2SHIFT	TIMEWHEEL_WIDTH
#define L3SHIFT	0
#define LXMUSK	(TIMEWHEEL_SIZE - 1)

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define GET_CUR_TICK(tw)	(tw->cur_tick)
#define	MS_TO_TICKS(tw,ms)	((ms&((1 << tw->_ticksize)-1))==0?(ms>>tw->_ticksize):(ms>>tw->_ticksize)+1)
#define CAL_IDXL1(tick)	((tick>>L1SHIFT)&LXMUSK)
#define CAL_IDXL2(tick)	((tick>>L2SHIFT)&LXMUSK)
#define CAL_IDXL3(tick)	((tick>>L3SHIFT)&LXMUSK)

static void _nop(void *arg) { return; }

static unsigned int incID;
pthread_mutex_t incIDLock = PTHREAD_MUTEX_INITIALIZER;
unsigned int _generateID() {
	unsigned int ret;
	pthread_mutex_lock(&incIDLock);
	ret = incID++;
	pthread_mutex_unlock(&incIDLock);
	return ret;
}

static void _insertToBucket(twbucket_t *bucket, twtasknode_t *node)
{
	pthread_mutex_lock(&bucket->lock);
	node->next = bucket->task_list;
	bucket->task_list = node;
	pthread_mutex_unlock(&bucket->lock);
}

static twtasknode_t* _pluckFromBucket(twbucket_t *bucket)
{
	twtasknode_t* nodelist;
	pthread_mutex_lock(&bucket->lock);
	nodelist = bucket->task_list;
	bucket->task_list = NULL;
	pthread_mutex_unlock(&bucket->lock);
	return nodelist;
}

static twtask_t* _addtasknode(timewheel_t *tw, unsigned int exec_tick, twtasknode_t* node)
{
	unsigned char idxl1 = 0, idxl2 = 0, idxl3 = 0;
	idxl1 = CAL_IDXL1(exec_tick);
	if (idxl1 != tw->ptrL1) {
		_insertToBucket(&tw->twL1[idxl1], node);
		goto done;
	}
	idxl2 = CAL_IDXL2(exec_tick);
	if (idxl2 != tw->ptrL2) {
		_insertToBucket(&tw->twL2[idxl2], node);
		goto done;
	}
	idxl3 = CAL_IDXL3(exec_tick);
	_insertToBucket(&tw->twL3[idxl3], node);
done:
	return &node->task;
}

timewheel_t* tw_new()
{
	timewheel_t *tw = (timewheel_t*)malloc(sizeof(timewheel_t));
	tw_init(tw, TW_TICKSIZE_128MS);
	return tw;
}

void tw_free(timewheel_t *tw)
{
	void *ret;
	if (tw->loop_tid != 0) {
		tw->tw_status = TW_STATUS_EXITED;
	}
	pthread_join(tw->loop_tid, &ret);
	pthread_mutex_destroy(&tw->ref_lock);
	int i;
	twtasknode_t *p, *q;
	for (i = 0; i < TIMEWHEEL_SIZE; i++) {
		p = tw->twL1[i].task_list;
		while (p) {
			q = p->next;
			free(p);
			p = q;
		}
		p = tw->twL2[i].task_list;
		while (p) {
			q = p->next;
			free(p);
			p = q;
		}
		p = tw->twL3[i].task_list;
		while (p) {
			q = p->next;
			free(p);
			p = q;
		}

		pthread_mutex_destroy(&tw->twL1[i].lock);
		pthread_mutex_destroy(&tw->twL2[i].lock);
		pthread_mutex_destroy(&tw->twL3[i].lock);
	}
	free(tw);
}

void tw_init(timewheel_t *tw, unsigned char ticksize)
{
	if (tw == NULL) return;
	pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
	ticksize = MIN(ticksize, TW_TICKSIZE_1024MS);
	tw->cur_tick = 0;
	tw->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	tw->loop_tid = 0;
	memcpy(&tw->ref_lock, &init_mutex, sizeof(init_mutex));
	tw->ref_count = 1;
	tw->tw_status = TW_STATUS_READY;
	tw->_ticksize = ticksize;
	tw->ptrL1 = 0;
	tw->ptrL2 = 0;
	tw->ptrL3 = 0;

	struct itimerspec timersetting;
	// printf("timer setting: %u, %u, %u, %u\n", (1 << ticksize) / 1000, ((1 << ticksize) % 1000) * 1000000, (1 << ticksize) / 1000, ((1 << ticksize) % 1000) * 1000000);
	timersetting.it_value.tv_sec = (1 << ticksize) / 1000;
	timersetting.it_value.tv_nsec = ((1 << ticksize) % 1000) * 1000000;
	timersetting.it_interval.tv_sec = (1 << ticksize) / 1000;
	timersetting.it_interval.tv_nsec = ((1 << ticksize) % 1000) * 1000000;

	timerfd_settime(tw->timer_fd, 0, &timersetting, NULL);

	
	int i;
	for (i = 0; i < TIMEWHEEL_SIZE; i++) {
		tw->twL1[i].task_list = NULL;
		memcpy(&tw->twL1[i].lock, &init_mutex, sizeof(init_mutex));

		tw->twL2[i].task_list = NULL;
		memcpy(&tw->twL2[i].lock, &init_mutex, sizeof(init_mutex));

		tw->twL2[i].task_list = NULL;
		memcpy(&tw->twL2[i].lock, &init_mutex, sizeof(init_mutex));
	}
	return;
}


twtask_t* tw_addtask(timewheel_t *tw, unsigned int timeout_ms, void (*cb)(void *arg), void *arg)
{
	unsigned int timeout_ticks = MS_TO_TICKS(tw, timeout_ms);
	// printf("timeout ticks=%u, ", timeout_ticks);
	if (((1 << (3 * TIMEWHEEL_WIDTH)) - (1 << (2 * TIMEWHEEL_WIDTH))) <= timeout_ticks) 
		return NULL;

	unsigned int exec_tick = timeout_ticks + GET_CUR_TICK(tw);
	// printf("execute tick=%u\n", exec_tick);
	twtasknode_t *ttnode = (twtasknode_t*)malloc(sizeof(twtasknode_t));
	ttnode->exec_tick = exec_tick;
	ttnode->next = NULL;
	ttnode->task.arg = arg;
	ttnode->task.cb = cb;
	ttnode->task.taskid = _generateID();
	ttnode->task.flags = TWTASK_FLAG_EXECONECE;
	ttnode->task.period = 0;

	return _addtasknode(tw, exec_tick, ttnode);
}

twtask_t* tw_settaskperiod(timewheel_t *tw, twtask_t *task, unsigned int period_ms)
{
	unsigned int period_ticks = MS_TO_TICKS(tw, period_ms);
	if (((1 << (3 * TIMEWHEEL_WIDTH)) - (1 << (2 * TIMEWHEEL_WIDTH))) <= period_ticks)
		return NULL;
	task->flags = TWTASK_FLAG_PERIODIC;
	task->period = period_ms;
	return task;
}

void tw_nexttick(timewheel_t *tw)
{
	int l1move, l2move;
	l1move = 0, l2move = 0;
	tw->cur_tick++;
	tw->ptrL3++;
	if (tw->ptrL3 == 0) {
		// printf("l2move\n");
		tw->ptrL2++;
		l2move = 1;
		if (tw->ptrL2 == 0) {
			// printf("l1move\n");
			tw->ptrL1++;
			l1move = 1;
		}
	}

	twtasknode_t *ttnode, *ptr;
	if (l1move == 1) {
		ttnode = _pluckFromBucket(&tw->twL1[tw->ptrL1]);
		while (ttnode != NULL) {
			ptr = ttnode->next;
			if (ttnode->task.flags != TWTASK_FLAG_CANCELLED) {
				_insertToBucket(&tw->twL2[CAL_IDXL2(ttnode->exec_tick)], ttnode);
			} else {
				free(ttnode);
			}
			ttnode = ptr;
		}
	}
	if (l2move == 1) {
		ttnode = _pluckFromBucket(&tw->twL2[tw->ptrL2]);
		while (ttnode != NULL) {
			ptr = ttnode->next;
			if (ttnode->task.flags != TWTASK_FLAG_CANCELLED) {
				_insertToBucket(&tw->twL3[CAL_IDXL3(ttnode->exec_tick)], ttnode);
			} else {
				free(ttnode);
			}
			ttnode = ptr;
		}
	}
	ttnode = _pluckFromBucket(&tw->twL3[tw->ptrL3]);
	while (ttnode != NULL) {
		ptr = ttnode->next;
		if (ttnode->task.flags != TWTASK_FLAG_CANCELLED) {
			// printf("task %u called in tick %u.\n", ttnode->task.taskid, tw->cur_tick);
			if (tw->tw_status == TW_STATUS_RUNNING)
				ttnode->task.cb(ttnode->task.arg);
		}
		
		if (ttnode->task.flags == TWTASK_FLAG_PERIODIC) {
			// printf("task %u reload.\n", ttnode->task.taskid);
			_addtasknode(tw, MS_TO_TICKS(tw, ttnode->task.period) + GET_CUR_TICK(tw), ttnode);
		} else {
			// printf("task %u freed.\n", ttnode->task.taskid);
			free(ttnode);
		}
		ttnode = ptr;
	}
	// printf("tick [%u:%u:%u]\n", tw->ptrL1, tw->ptrL2, tw->ptrL3);
	return;
}

void tw_changetask(twtask_t *task, void (*cb)(void *arg), void *arg)
{
	task->cb = cb;
	task->arg = arg;
	return;
}

void tw_canceltask(twtask_t *task)
{
	tw_changetask(task, _nop, NULL);
	task->flags = TWTASK_FLAG_CANCELLED;
	return;
}


void* _clockdriver(void *arg) {
	timewheel_t *tw = arg;
	tw->tw_status = TW_STATUS_RUNNING;

	int epfd = epoll_create(1);
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.ptr = (void*)tw;
	epoll_ctl(epfd, EPOLL_CTL_ADD, tw->timer_fd, &ev);

	int i, nevents, ret;
	struct epoll_event events[4];
	timewheel_t* evtw;
	uint64_t exp;
	// printf("clock driver loop start\n");
	while (1) {
		if (tw->tw_status == TW_STATUS_EXITED) {
			break;
		}
		nevents = epoll_wait(epfd, events, 4, -1);
		for (i = 0; i < nevents; i++) {
			if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
				printf("timewheel clock driver shutdown...\n");
				pthread_exit(NULL);
			}

			evtw = (timewheel_t*)events[i].data.ptr;
			ret = read(evtw->timer_fd, &exp, sizeof(uint64_t));
			tw_nexttick(evtw);
		}
	}
	pthread_exit(NULL);
}

pthread_t tw_runthread(timewheel_t *tw)
{
	pthread_t ret, tid;
	// printf("tw_runthread\n");
	if (tw == NULL) return 0;
	ret = pthread_create(&tid, NULL, _clockdriver, tw);
	if (ret == 0) {
		tw->loop_tid = tid;
		return tid;
	}
	return 0;
}