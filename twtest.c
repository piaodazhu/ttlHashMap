#include <unistd.h>
#include <sys/time.h>
#include "timewheel.h"
timewheel_t tw;
void testtask(void *arg)
{
	int *data = arg;

	struct tm *info;
	time_t rawtime;
	char buffer[80];
	time(&rawtime);
	info = localtime(&rawtime);
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
	
	printf("[%s]testtask: %d\n", buffer, *data);
	return;
}

void counttask(void *arg)
{
	int *data = arg;
	static int count = 0;
	struct tm *info;
	time_t rawtime;
	char buffer[80];
	time(&rawtime);
	info = localtime(&rawtime);
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", info);
	
	count += *data;
	printf("[%s]counttask: %d\n", buffer, count);
	return;
}

int main()
{
	tw_init(&tw, TW_TICKSIZE_1MS);
	pthread_t twpid = tw_runthread(&tw);
	if (twpid == 0) {
		printf("Error in creating timewheel clock thread.\n");
		return 1;
	}
	int data0 = 0, data1 = 1, data2 = 2, data3 = 3;
	twtask_t *task1, *task2, *task3;
	task1 = tw_addtask(&tw, 423, testtask, (void*)&data1);
	task2 = tw_addtask(&tw, 900, testtask, (void*)&data2);
	tw_settaskperiod(&tw, task2, 1000);

	task3 = tw_addtask(&tw, 1700, testtask, (void*)&data3);
	tw_settaskperiod(&tw, task3, 777);

	testtask((void*)&data0);
	sleep(10);
	tw_changetask(task2, counttask, (void*)&data1);
	while (1) {
		sleep(30);
		if (task3->flags != TWTASK_FLAG_CANCELLED) {
			tw_canceltask(task3);
		}
	}
	return 0;
}