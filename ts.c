//////////////////////////////////////////////////////////////////
//
//  Copyright(C), 2013-2016, GEC Tech. Co., Ltd.
//
//  File name: GPLE/ts.c
//
//  Author: Vincent Lin (林世霖)  微信公众号：秘籍酷
//
//  Date: 2017-3
//  
//  Description: 通过tslib获取触摸屏数据
//
//  GitHub: github.com/vincent040   Bug Report: 2437231462@qq.com
//
//////////////////////////////////////////////////////////////////

#include "common.h"
#include "tslib.h"

int ts_state = -1;
struct ts_sample *samp = NULL;
sem_t coor_ready;

void *__ts_state(void *arg)
{
	pthread_detach(pthread_self());

	struct tsdev *ts = (struct tsdev *)arg;

	while(1)
	{
		ts_read(ts, samp, 1);

		if(samp->pressure == 0)
		{
			printf("released....\n");
			ts_state = RELEASED;
		}
		else
		{
			printf("pressed!![%d, %d]\n", samp->x, samp->y);
			ts_state = PRESSED;
			sem_post(&coor_ready);
		}
	}
}


struct tsdev *init_ts(void)
{
	bzero(samp, sizeof(*samp));
	sem_init(&coor_ready);

	struct tsdev *ts = ts_open("/dev/event0", 0);
	if(ts == NULL)
	{
		perror("ts_open() failed");
		exit(0);
	}
	ts_config(ts);

	pthread_t tid;
	pthread_create(&tid, NULL, __ts_state, (void *)ts);

	return ts;
}

int get_pos(struct tsdev *ts)
{
	int pos = -1;

	while(1)
	{
		printf("(%d, %d)\n", samp->x, samp->y);

		if(samp->x < 0 ||
		   samp->x > 800 ||
		   samp->y < 0 ||
		   samp->y > 480 ||
		   (samp->x == 0 && samp->y == 0))
		{
			continue;
		}

		if(samp->x > 0 && samp->x < 80 &&
			samp->y > 0 && samp->y < 240)
		{
			pos = BTN1;
			break;
		}

		if(samp->x > 720 && samp->x < 800
			&& samp->y > 0 && samp->y < 240)
		{
			pos = BTN2;
			break;
		}

		if(samp->x > 0 && samp->x < 80 &&
			samp->y > 240 && samp->y < 480)
		{
			pos = BTN3;
			break;
		}

		if(samp->x > 720 && samp->x < 800
			&& samp->y > 240 && samp->y < 480)
		{
			pos = BTN4;
			break;
		}
	}

	return pos;
}