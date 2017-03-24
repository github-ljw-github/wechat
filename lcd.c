//////////////////////////////////////////////////////////////////
//
//  Copyright(C), 2013-2016, GEC Tech. Co., Ltd.
//
//  File name: GPLE/lcd.c
//
//  Author: Vincent Lin (林世霖)  微信公众号：秘籍酷
//
//  Date: 2017-3
//  
//  Description: 控制LCD设备
//
//  GitHub: github.com/vincent040   Bug Report: 2437231462@qq.com
//
//////////////////////////////////////////////////////////////////

#include "common.h"

char * init_lcd(struct fb_var_screeninfo *vinfo)
{
	// 准备LCD屏幕
	int lcd = open("/dev/fb0", O_RDWR|O_EXCL);

	// 获取LCD设备的当前参数
	ioctl(lcd, FBIOGET_VSCREENINFO, vinfo);

	// 根据当前LCD设备参数申请适当大小的FRAMEBUFFR
	unsigned long bpp = vinfo->bits_per_pixel;
	char *FB = mmap(NULL, vinfo->xres * vinfo->yres * bpp/8,
		  PROT_READ|PROT_WRITE, MAP_SHARED, lcd, 0);

	// 刚开始运行程序，将屏幕清零
	bzero(FB, vinfo->xres * vinfo->yres * 4);
	return FB;
}

// 将rgb_buffer中的24bits的RGB数据，写入LCD的32bits的显存中
// 并通过xoffset和yoffset指定显示的位置
void write_lcd(unsigned char *rgb_buffer,
			struct image_info *imageinfo,
			char *FB, struct fb_var_screeninfo *vinfo,
			int xoffset, int yoffset)
{
	FB += (800*yoffset + xoffset) * 4;

	int x, y;
	for(x=0; x<(vinfo->yres-yoffset) && x<imageinfo->height; x++)
	{
		for(y=0; y<(vinfo->xres-xoffset) && y<imageinfo->width; y++)
		{
			unsigned long lcd_offset = (vinfo->xres*x + y) * 4;
			unsigned long bmp_offset = (imageinfo->width*x+y) *
						    imageinfo->pixel_size;

			memcpy(FB + lcd_offset + vinfo->red.offset/8,
			       rgb_buffer + bmp_offset + 0, 1);
			memcpy(FB + lcd_offset + vinfo->green.offset/8,
			       rgb_buffer + bmp_offset + 1, 1);
			memcpy(FB + lcd_offset + vinfo->blue.offset/8,
			       rgb_buffer + bmp_offset + 2, 1);
		}
	}
}