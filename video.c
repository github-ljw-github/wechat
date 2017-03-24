//////////////////////////////////////////////////////////////////
//
//  Copyright(C), 2013-2016, GEC Tech. Co., Ltd.
//
//  File name: GPLE/video.c
//
//  Author: Vincent Lin (林世霖)  微信公众号：秘籍酷
//
//  Date: 2017-3
//  
//  Description: 视频输入——摄像头操作
//
//  GitHub: github.com/vincent040   Bug Report: 2437231462@qq.com
//
//////////////////////////////////////////////////////////////////
#include "common.h"

int init_video(void)
{
	int cam_fd = open("/dev/video3",O_RDWR);

	struct v4l2_fmtdesc *a = calloc(1, sizeof(*a));
	a->index = 0;
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	int ret;
	while((ret=ioctl(cam_fd, VIDIOC_ENUM_FMT, a)) == 0)
	{
		a->index++;
		printf("pixelformat: \"%c%c%c%c\"\n",
				(a->pixelformat >> 0) & 0XFF,
				(a->pixelformat >> 8) & 0XFF,
				(a->pixelformat >>16) & 0XFF,
				(a->pixelformat >>24) & 0XFF);

		printf("description: %s\n", a->description);
	}

	return cam_fd;
}


void show_camfmt(struct v4l2_format *fmt)
{
	printf("camera width : %d \n", fmt->fmt.pix.width);
	printf("camera height: %d \n", fmt->fmt.pix.height);

	printf("camera pixelformat: \n");
	switch(fmt->fmt.pix.pixelformat)
	{
	case V4L2_PIX_FMT_JPEG:
		printf("camera pixelformat: V4L2_PIX_FMT_JPEG\n");
		break;
	case V4L2_PIX_FMT_YUYV:
		printf("camera pixelformat: V4L2_PIX_FMT_YUYV\n");
	}
}

void config_cam(int cam_fd, struct fb_var_screeninfo *vinfo, char *setfmt)
{
	// 获取摄像头当前的采集格式
	struct v4l2_format *fmt = calloc(1, sizeof(*fmt));
	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ioctl(cam_fd, VIDIOC_G_FMT, fmt);
	show_camfmt(fmt);

	// 配置摄像头的采集格式
	bzero(fmt, sizeof(*fmt));
	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt->fmt.pix.width = vinfo->xres;
	fmt->fmt.pix.height = vinfo->yres;
	fmt->fmt.pix.field = V4L2_FIELD_INTERLACED;

	if(!strcmp(setfmt, "JPEG"))
		fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
	if(!strcmp(setfmt, "YUV"))
		fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	ioctl(cam_fd, VIDIOC_S_FMT, fmt);

	// 再次获取摄像头当前的采集格式
	bzero(fmt, sizeof(*fmt));
	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl (cam_fd, VIDIOC_G_FMT, fmt);

	if(!strcmp(setfmt, "JPEG") && fmt->fmt.pix.pixelformat != V4L2_PIX_FMT_JPEG)
	{
		printf("\nthe pixel format is NOT JPEG.\n\n");
		exit(1);
	}
	if(!strcmp(setfmt, "YUV") && fmt->fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
	{
		printf("\nthe pixel format is NOT YUV.\n\n");
		exit(1);
	}
}

void video_capture_start(int cam_fd, char *fb_mem, int xoffset, int yoffset)
{
	struct argument  *arg = calloc(1, sizeof(*arg));
	arg->cam_fd = cam_fd;
	arg->fb_mem = fb_mem;
	arg->xoffset= xoffset;
	arg->yoffset= yoffset;

	pthread_t tid;
	pthread_create(&tid, NULL, capturing, (void *)arg);
}

void *capturing(void *arg)
{
	int cam_fd  = ((struct argument *)arg)->cam_fd;
	char *fb_mem= ((struct argument *)arg)->fb_mem;
	int xoffset = ((struct argument *)arg)->xoffset;
	int yoffset = ((struct argument *)arg)->yoffset;

	// 设置即将要申请的摄像头缓存的参数
	int nbuf = 3;

	struct v4l2_requestbuffers reqbuf;
	bzero(&reqbuf, sizeof (reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = nbuf;

	// 使用该参数reqbuf来申请缓存
	ioctl(cam_fd, VIDIOC_REQBUFS, &reqbuf);

	// 根据刚刚设置的reqbuf.count的值，来定义相应数量的struct v4l2_buffer
	// 每一个struct v4l2_buffer对应内核摄像头驱动中的一个缓存
	struct v4l2_buffer buffer[nbuf];
	int length[nbuf];
	unsigned char *start[nbuf];

	int i;
	for(i=0; i<nbuf; i++)
	{
		bzero(&buffer[i], sizeof(buffer[i]));
		buffer[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer[i].memory = V4L2_MEMORY_MMAP;
		buffer[i].index = i;
		ioctl(cam_fd, VIDIOC_QUERYBUF, &buffer[i]);

		length[i] = buffer[i].length;
		start[i] = mmap(NULL, buffer[i].length,	PROT_READ | PROT_WRITE,
				  MAP_SHARED,	cam_fd, buffer[i].m.offset);

		ioctl(cam_fd , VIDIOC_QBUF, &buffer[i]);
	}

	// 启动摄像头数据采集
	enum v4l2_buf_type vtype= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(cam_fd, VIDIOC_STREAMON, &vtype);

	struct v4l2_buffer v4lbuf;
	bzero(&v4lbuf, sizeof(v4lbuf));
	v4lbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4lbuf.memory= V4L2_MEMORY_MMAP;

	i = 0;
	while(1)
	{
		// 从队列中取出填满数据的缓存
		v4lbuf.index = i%nbuf;
		ioctl(cam_fd , VIDIOC_DQBUF, &v4lbuf); // VIDIOC_DQBUF在摄像头没数据的时候会阻塞
		shooting(start[i%nbuf], length[i%nbuf], (int *)fb_mem, xoffset, yoffset);

	 	// 将已经读取过数据的缓存块重新置入队列中 
		v4lbuf.index = i%nbuf;
		ioctl(cam_fd , VIDIOC_QBUF, &v4lbuf);

		i++;
	}
}



int shooting(unsigned char * jpegdata, int size, int *fb_mem, int xoffset, int yoffset)
{
	// 声明解压缩结构体，以及错误管理结构体
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	// 使用缺省的出错处理来初始化解压缩结构体
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	// 配置该cinfo，使其从jpg_buffer中读取jpg_size个字节
	// 这些数据必须是完整的JPEG数据
	jpeg_mem_src(&cinfo, jpegdata, size);

	// 读取JPEG文件的头
	jpeg_read_header(&cinfo, true);

	// 开始解压jpeg数据
	jpeg_start_decompress(&cinfo);

	int width = cinfo.output_width;
	int pixel_size = cinfo.output_components;
	int row_stride = width * pixel_size;

	// 定义一个存放一行数据的缓冲区
	char * buffer;
	buffer = malloc(row_stride);

	// 以行为单位，将JPEG数据解压出来之后，按照RGBA方式填入fb_mem
	int red  = 0;
	int green= 1;
	int blue = 2;

	fb_mem += (yoffset*800 + xoffset);

	while (cinfo.output_scanline < cinfo.output_height)
	{
		jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&buffer, 1);

		// 将buffer中的24bit的图像数据写入32bit的fb_mem
		int i=0, x;
		for(x=0; x<cinfo.output_width; x++)
		{
			*(fb_mem+(cinfo.output_scanline-1)*800+x) =
				buffer[i+blue] |
				buffer[i+green]<<8 |
				buffer[red]<<16;
			i += 3;
		}
	}

	// 释放相关结构体和内存，解压后的图片信息被保留在fb_mem，直接映射到了LCD
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return 0;
}
