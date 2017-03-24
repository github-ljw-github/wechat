#include "common.h"
#include "jpeglib.h"
#include "tslib.h"

// 将jpeg文件的压缩图像数据读出，放到jpg_buffer中去等待解压
unsigned long read_image_from_file(int fd,
				   unsigned char *jpg_buffer,
				   unsigned long jpg_size)
{
	unsigned long nread = 0;
	unsigned long total = 0;

	while(jpg_size > 0)
	{
		nread = read(fd, jpg_buffer, jpg_size);

		jpg_size -= nread;
		jpg_buffer += nread;
		total += nread;
	}
	close(fd);

	return total;
}

int Stat(const char *filename, struct stat *file_info)
{
	int ret = stat(filename, file_info);

	if(ret == -1)
	{
		fprintf(stderr, "[%d]: stat failed: "
			"%s\n", __LINE__, strerror(errno));
		exit(1);
	}

	return ret;
}

void show_jpg(char *jpgname, char *fb_mem, struct fb_var_screeninfo *vinfo, int xoffset, int yoffset)
{
	// 检查指定的文件是否存在
	if(access(jpgname, F_OK))
	{
		printf("%s is NOT found.\n", jpgname);
		exit(0);
	}

	// 读取图片文件属性信息（主要是想知道其大小）
	struct stat file_info;
	Stat(jpgname, &file_info);

	// 并根据其大小分配内存缓冲区jpg_buffer
	unsigned char *jpg_buffer;
	jpg_buffer = (unsigned char *)calloc(1, file_info.st_size);

	// 打开指定的文件，并且将里面的jpg编码数据读出
	int fd = open(jpgname, O_RDONLY);
	read_image_from_file(fd, jpg_buffer, file_info.st_size);


	// 声明解压缩结构体，以及错误管理结构体
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	// 使用缺省的出错处理来初始化解压缩结构体
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	// 配置该cinfo，使其从jpg_buffer中读取jpg_size个字节
	// 这些数据必须是完整的JPEG数据
	jpeg_mem_src(&cinfo, jpg_buffer, file_info.st_size);

	// 读取JPEG文件的头，并判断其格式是否合法
	int ret = jpeg_read_header(&cinfo, true);
	if(ret != 1)
	{
		fprintf(stderr, "[%d]: jpeg_read_header failed: "
			"%s\n", __LINE__, strerror(errno));
		exit(1);
	}

	// 开始解压
	jpeg_start_decompress(&cinfo);

	struct image_info imageinfo;
	imageinfo.width = cinfo.output_width;
	imageinfo.height = cinfo.output_height;
	imageinfo.pixel_size = cinfo.output_components;

	int row_stride = imageinfo.width * imageinfo.pixel_size;

	// 根据图片的尺寸大小，分配一块相应的内存rgb_buffer
	// 用来存放从jpg_buffer解压出来的图像数据
	unsigned long rgb_size;
	unsigned char *rgb_buffer;
	rgb_size = imageinfo.width *
			imageinfo.height * imageinfo.pixel_size;
	rgb_buffer = (unsigned char *)calloc(1, rgb_size);

	// 循环地将图片的每一行读出并解压到rgb_buffer中
	while(cinfo.output_scanline < cinfo.output_height)
	{
		unsigned char *buffer_array[1];
		buffer_array[0] = rgb_buffer +
				(cinfo.output_scanline) * row_stride;
		jpeg_read_scanlines(&cinfo, buffer_array, 1);
	}

	// 解压完了，将jpeg相关的资源释放掉
 	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	free(jpg_buffer);

	// 将rgb_buffer中的RGB图像数据，写入LCD的FRAMEBUFFER中
	write_lcd(rgb_buffer, &imageinfo, fb_mem, vinfo, xoffset, yoffset);
}


void flipover(int btn, char *fb_mem, struct fb_var_screeninfo *vinfo)
{
	static int now1 = OFF;
	static int now2 = OFF;

	char *button = NULL;
	int x, y;

	if(btn == BTN3)
	{
		if(now1 == OFF)
		{
			button = "jpg/btn3_on.jpg";	
		}
		if(now1 == ON)
		{
			button = "jpg/btn3_off.jpg";	
		}
		now1 *= -1;

		x = 0;
		y = 240;
	}

	if(btn == BTN4)
	{
		if(now2 == OFF)
		{
			button = "jpg/btn4_on.jpg";	
		}
		if(now2 == ON)
		{
			button = "jpg/btn4_off.jpg";	
		}
		now2 *= -1;

		x = 720;
		y = 240;
	}

	show_jpg(button, fb_mem, vinfo, x, y);
}

void exit_properly(int sig)
{
	exit(0);
}

int main(int argc, char **argv) 
{
	signal(SIGINT, exit_properly);

	// 初始化LCD
	struct fb_var_screeninfo vinfo;
	char *fb_mem = init_lcd(&vinfo);

	// 显示你指定的素材，到指定的位置
	show_jpg("jpg/btn1.jpg", fb_mem, &vinfo, 0, 0);
	show_jpg("jpg/btn2.jpg", fb_mem, &vinfo, 720, 0);
	show_jpg("jpg/btn3_off.jpg", fb_mem, &vinfo, 0, 240);
	show_jpg("jpg/btn4_off.jpg", fb_mem, &vinfo, 720, 240);
	show_jpg("jpg/background.jpg", fb_mem, &vinfo, 80, 0);

	// 初始化触摸屏
	struct tsdev *ts = init_ts();

	// 初始化摄像头，并查看其所支持的格式
	int cam_fd = init_video();

	// 设置摄像头采集格式
	config_cam(cam_fd, &vinfo, "JPEG"); // "JPEG" 或者 "YUV"

	// 开始捕捉视频数据，并输送到LCD显示出来
	video_capture_start(cam_fd, fb_mem, 80, 0);

	// 循环获取手指触摸的坐标
	struct ts_sample samp;
	int wavfd = -1;
	while(1)
	{
		bzero(&samp, sizeof(samp));

		switch(get_pos(ts, &samp))
		{
		case BTN1:
			printf("btn1 is pressed.\n"); // 开启画中画
			break;

		case BTN2:
			printf("btn2 is pressed.\n"); // 关闭画中画
			break;

		case BTN3:
			printf("btn3 is pressed.\n");
			wavfd = record(ts, fb_mem, &vinfo); // 录音
			break;

		case BTN4:
			printf("btn4 is pressed1.\n");
			playback(wavfd, ts, fb_mem, &vinfo); // 播放
			printf("btn4 is pressed2.\n");
			break;
		}
	}

	return 0;
}

