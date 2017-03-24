//////////////////////////////////////////////////////////////////
//
//  Copyright(C), 2013-2016, GEC Tech. Co., Ltd.
//
//  File name: GPLE/common.h
//
//  Author: Vincent Lin (林世霖)  微信公众号：秘籍酷
//
//  Date: 2017-3
//  
//  Description: 音视频聊天项目——通用头文件
//
//  GitHub: github.com/vincent040   Bug Report: 2437231462@qq.com
//
//////////////////////////////////////////////////////////////////

#ifndef __COMMON_H
#define __COMMON_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <linux/videodev2.h>

#include <linux/input.h>
#include <sys/ioctl.h>
#include <setjmp.h>


/* ============== 液晶屏LCD显示相关 ============== */
#include "jpeglib.h"

struct image_info
{
	int width;
	int height;
	int pixel_size;
};

char * init_lcd(struct fb_var_screeninfo *vinfo);
void write_lcd(unsigned char *rgb_buffer,
			struct image_info *imageinfo,
			char *FB, struct fb_var_screeninfo *vinfo,
			int xoffset, int yoffset);


/* ================== 触摸屏相关 ================= */
#include "tslib.h"

enum {BTN1, BTN2, BTN3, BTN4};

struct tsdev *init_ts(void);
int get_pos(struct tsdev * ts, struct ts_sample *samp);

extern int ts_state;
struct ts_sample *samp;

#define RELEASED 0
#define PRESSED  1

#define OFF -1
#define ON   1
void flipover(int btn, char *fb_mem, struct fb_var_screeninfo *vinfo);
void *wait4released(void *arg);


/* ================== 摄像头相关 ================= */
struct argument
{
	int cam_fd;
	char *fb_mem;
	int xoffset;
	int yoffset;
};

int init_video(void);
void show_camfmt(struct v4l2_format *fmt);
void config_cam(int cam_fd, struct fb_var_screeninfo *vinfo, char *setfmt);
void video_capture_start(int cam_fd, char *fb_mem, int xoffset, int yoffset);
int shooting(unsigned char * jpegdata, int size, int *fb_mem, int xoffset, int yoffset);
void *capturing(void *arg);



/* ================== 音频相关 ================= */
#include "audio.h"
int  record(struct tsdev *ts, char *fb_mem, struct fb_var_screeninfo *vinfo);
void playback(int wavfd, struct tsdev *ts, char *fb_mem, struct fb_var_screeninfo *vinfo);
void recording_prepare(pcm_container *sound, wav_format *wav);
void set_wav_params(pcm_container *sound, wav_format *wav);
void prepare_wav_params(wav_format *wav);
snd_pcm_uframes_t read_pcm_data(pcm_container *sound, snd_pcm_uframes_t frames);

#endif