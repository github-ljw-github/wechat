#include "common.h"

// 准备WAV格式参数
void prepare_wav_params(wav_format *wav)
{
	wav->format.fmt_id = FMT;
	wav->format.fmt_size = LE_INT(16);
	wav->format.fmt = LE_SHORT(WAV_FMT_PCM);
	wav->format.channels = LE_SHORT(2);         // 声道数目
	wav->format.sample_rate = LE_INT(44100);    // 采样频率
	wav->format.bits_per_sample = LE_SHORT(16); // 量化位数
	wav->format.block_align = LE_SHORT(wav->format.channels
				* wav->format.bits_per_sample/8);
	wav->format.byte_rate = LE_INT(wav->format.sample_rate
				* wav->format.block_align);
	wav->data.data_id = DATA;
	//wav->data.data_size = LE_INT(DURATION_TIME
	//			* wav->format.byte_rate);
	wav->head.id = RIFF;
	wav->head.format = WAVE;
	wav->head.size = LE_INT(36 + wav->data.data_size);
}

// 设置WAV格式参数
void set_wav_params(pcm_container *sound, wav_format *wav)
{
	// 1：定义并分配一个硬件参数空间
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_hw_params_alloca(&hwparams);

	// 2：初始化硬件参数空间
	snd_pcm_hw_params_any(sound->handle, hwparams);

	// 3：设置访问模式为交错模式（即帧连续模式）
	snd_pcm_hw_params_set_access(sound->handle, hwparams,
			 SND_PCM_ACCESS_RW_INTERLEAVED);

	// 4：设置量化参数
	snd_pcm_format_t pcm_format = SND_PCM_FORMAT_S16_LE;
	snd_pcm_hw_params_set_format(sound->handle,
					hwparams, pcm_format);
	sound->format = pcm_format;

	// 5：设置声道数目
	snd_pcm_hw_params_set_channels(sound->handle,
		hwparams, LE_SHORT(wav->format.channels));
	sound->channels = LE_SHORT(wav->format.channels);

	// 6：设置采样频率
	// 注意：最终被设置的频率被存放在来exact_rate中
	uint32_t exact_rate = LE_INT(wav->format.sample_rate);
	snd_pcm_hw_params_set_rate_near(sound->handle,
				hwparams, &exact_rate, 0);

	// 7：设置buffer size为声卡支持的最大值
	snd_pcm_uframes_t buffer_size;
	snd_pcm_hw_params_get_buffer_size_max(hwparams,
					&buffer_size);
	snd_pcm_hw_params_set_buffer_size_near(sound->handle,
				hwparams, &buffer_size);

	// 8：根据buffer size设置period size
	snd_pcm_uframes_t period_size = buffer_size / 4;
	snd_pcm_hw_params_set_period_size_near(sound->handle,
				hwparams, &period_size, 0);

	// 9：安装这些PCM设备参数
	snd_pcm_hw_params(sound->handle, hwparams);

	// 10：获取buffer size和period size
	// 注意：他们均以 frame 为单位 （frame = 声道数 * 量化级）
	snd_pcm_hw_params_get_buffer_size(hwparams,
				&sound->frames_per_buffer);
	snd_pcm_hw_params_get_period_size(hwparams,
				&sound->frames_per_period, 0);

	// 11：保存一些参数
	sound->bits_per_sample =
		snd_pcm_format_physical_width(pcm_format);
	sound->bytes_per_frame =
		sound->bits_per_sample/8 * wav->format.channels;

	// 12：分配一个周期数据空间
	sound->period_buf =
		(uint8_t *)calloc(1,
		sound->frames_per_period * sound->bytes_per_frame);

	printf("period_buf size: %lu\n", sound->frames_per_period * sound->bytes_per_frame);
}

void recording_prepare(pcm_container *sound, wav_format *wav)
{
	// 打开PCM设备文件
	int ret = snd_pcm_open(&sound->handle, "default",
				SND_PCM_STREAM_CAPTURE, 0);
	if(ret != 0)
	{
		printf("[%d]: %d\n", __LINE__, ret);
		perror("snd_pcm_open( ) failed");
		exit(1);
	}

	// 准备并设置WAV格式参数
	prepare_wav_params(wav);
	set_wav_params(sound, wav);
}


snd_pcm_uframes_t read_pcm_data(pcm_container *sound, snd_pcm_uframes_t frames)
{
	snd_pcm_uframes_t exact_frames = 0;
	snd_pcm_uframes_t n = 0;

	uint8_t *p = sound->period_buf;
	while(frames > 0)	
	{
		n = snd_pcm_readi(sound->handle, p, frames);
		if(n < 0)
		{
			perror("snd_pcm_readi() failed");
			break;
		}

		frames -= n;
		exact_frames += n;
		p += (n * sound->bytes_per_frame);
	}

	printf("exact_frames: %d\n", (int)exact_frames);
	return exact_frames;
}

int record(struct tsdev *ts, char *fb_mem, struct fb_var_screeninfo *vinfo)
{
	flipover(BTN3, fb_mem, vinfo);

	pcm_container *sound = calloc(1, sizeof(pcm_container));
	wav_format *wav = calloc(1, sizeof(wav_format));

	recording_prepare(sound, wav);

	// 1：创建一个临时文件，用来存放WAV音频数据
	// int fd = fileno(tmpfile());
	// if(fd == -1)	
	// {
	// 	perror("open() error");
	// 	exit(1);
	// }

	int fd = open("a.wav", O_RDWR | O_CREAT | O_TRUNC, 0644);
	if(fd == -1)
	{
		perror("open() a.wav faield");
		exit(0);
	}

	// 2：留出空位给格式头，录音完毕得到音频文件大小之后再写格式头
	lseek(fd, sizeof(wav->head)+sizeof(wav->format)+sizeof(wav->data), SEEK_SET);

	// 3：开始录音
	uint32_t total_bytes = 0;
	while(ts_state == PRESSED)
	{
		uint32_t frames_read = read_pcm_data(sound, sound->frames_per_period);
		write(fd, sound->period_buf, frames_read * sound->bytes_per_frame);
		total_bytes += (frames_read * sound->bytes_per_frame);
	}

	// 4：写WAV格式的文件头
	wav->data.data_size = total_bytes;
	lseek(fd, 0L, SEEK_SET);
	write(fd, &wav->head, sizeof(wav->head));
	write(fd, &wav->format, sizeof(wav->format));
	write(fd, &wav->data, sizeof(wav->data));

	// 5: 释放相关资源
	snd_pcm_drain(sound->handle);
	snd_pcm_close(sound->handle);

	free(sound->period_buf);
	free(sound);
	free(wav);

	flipover(BTN3, fb_mem, vinfo);
	return fd;
}





/* ========================= 上面是录音，下面是播放 ============================*/






int get_bits_per_sample(wav_format *wav, snd_pcm_format_t *snd_format)  
{     
    if (LE_SHORT(wav->format.fmt) != WAV_FMT_PCM)
        return -1;
      
    switch (LE_SHORT(wav->format.bits_per_sample))
    {  
    case 16:  
        *snd_format = SND_PCM_FORMAT_S16_LE;  
        break;  
    case 8:  
        *snd_format = SND_PCM_FORMAT_U8;  
        break;  
    default:  
        *snd_format = SND_PCM_FORMAT_UNKNOWN;  
        break;  
    }  
  
    return 0;  
}  


ssize_t write_pcm_to_device(pcm_container *pcm, size_t wcount)  
{  
    ssize_t r;  
    ssize_t result = 0;  
    uint8_t *data = pcm->period_buf;  
  
    if(wcount < pcm->frames_per_period)
    {  
        snd_pcm_format_set_silence(pcm->format,   
                    data + wcount * pcm->bytes_per_frame,   
                    (pcm->frames_per_period - wcount) * pcm->channels);  

        wcount = pcm->frames_per_period;  
    }  

    while(wcount > 0)
    {
        r = snd_pcm_writei(pcm->handle, data, wcount);  

        if(r == -EAGAIN || (r >= 0 && (size_t)r < wcount))
        {  
            snd_pcm_wait(pcm->handle, 1000);  
        }
        else if(r == -EPIPE)
        {  
            snd_pcm_prepare(pcm->handle);  
            fprintf(stderr, "<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n");  
        }
        else if(r == -ESTRPIPE)
        {              
            fprintf(stderr, "<<<<<<<<<<<<<<< Need suspend >>>>>>>>>>>>>>>\n");          
        }
        else if(r < 0)
        {  
            fprintf(stderr, "Error snd_pcm_writei: [%s]\n", snd_strerror(r));  
            exit(-1);
        }  
        if(r > 0)
        {  
            result += r;  
            wcount -= r;  
            data += r * pcm->bytes_per_frame;  
        }  
    }  
    return result;  
}  
 

int set_params(pcm_container *pcm, wav_format *wav)  
{  
    snd_pcm_hw_params_t *hwparams;  
  
    // A) 分配参数空间
    //    以PCM设备能支持的所有配置范围初始化该参数空间
    snd_pcm_hw_params_alloca(&hwparams);  
    snd_pcm_hw_params_any(pcm->handle, hwparams);
  
    // B) 设置访问方式为“帧连续交错方式”
    snd_pcm_hw_params_set_access(pcm->handle, hwparams,
                        SND_PCM_ACCESS_RW_INTERLEAVED);
  
    // C) 根据WAV文件的格式信息，设置量化参数
    snd_pcm_format_t format = 0;  
    get_bits_per_sample(wav, &format);
    snd_pcm_hw_params_set_format(pcm->handle, hwparams,
                                               format);
    pcm->format = format;  
  
    // D) 根据WAV文件的格式信息，设置声道数
    snd_pcm_hw_params_set_channels(pcm->handle, hwparams,
                         LE_SHORT(wav->format.channels));
    pcm->channels = LE_SHORT(wav->format.channels);  
  
    // E) 根据WAV文件的格式信息，设置采样频率
    //    如果声卡不支持WAV文件的采样频率，则
    //    选择一个最接近的频率
    uint32_t exact_rate = LE_INT(wav->format.sample_rate);
    snd_pcm_hw_params_set_rate_near(pcm->handle,
                                hwparams, &exact_rate, 0);
  
    // F) 设置buffer大小为声卡支持的最大值
    //    并将处理周期设置为buffer的1/4的大小
    snd_pcm_hw_params_get_buffer_size_max(hwparams,
                                 &pcm->frames_per_buffer);

    snd_pcm_hw_params_set_buffer_size_near(pcm->handle,
                       hwparams, &pcm->frames_per_buffer);

    pcm->frames_per_period = pcm->frames_per_buffer / 4;
    snd_pcm_hw_params_set_period_size(pcm->handle,
                    hwparams, pcm->frames_per_period, 0);
    snd_pcm_hw_params_get_period_size(hwparams,
                             &pcm->frames_per_period, 0);

    // G) 将所设置的参数安装到PCM设备中
    snd_pcm_hw_params(pcm->handle, hwparams);
  
    // H) 由所设置的buffer时间和周期
    //     分配相应的大小缓冲区
    pcm->bits_per_sample =
                  snd_pcm_format_physical_width(format);
    pcm->bytes_per_frame = pcm->bits_per_sample/8 *
                         LE_SHORT(wav->format.channels);
    pcm->period_buf =
              (uint8_t *)malloc(pcm->frames_per_period *
                                  pcm->bytes_per_frame);
  
    return 0;  
}  

int check_wav_format(wav_format *wav)  
{  
    if (wav->head.id!= RIFF ||  
        wav->head.format!= WAVE ||  
        wav->format.fmt_id!= FMT ||  
        wav->format.fmt_size != LE_INT(16) ||  
        (wav->format.channels != LE_SHORT(1) &&
            wav->format.channels != LE_SHORT(2)) ||  
        wav->data.data_id!= DATA)
    {  
        fprintf(stderr, "non standard wav file.\n");  
        return -1;  
    }  
  
    return 0;  
}  
  

int get_wav_header_info(int fd, wav_format *wav)  
{  
    int n1 = read(fd, &wav->head, sizeof(wav->head)); 
    int n2 = read(fd, &wav->format, sizeof(wav->format));
    int n3 = read(fd, &wav->data,   sizeof(wav->data));
  
    if(n1 != sizeof(wav->head) ||
       n2 != sizeof(wav->format) ||
       n3 != sizeof(wav->data))
    {
        fprintf(stderr, "get_wav_header_info() failed\n");
        return -1;
    }
  
    // if(check_wav_format(wav) < 0)
    //     return -1;
  
    return 0;
}


ssize_t read_pcm_from_wav(int fd, void *buf, size_t count)  
{  
    ssize_t result = 0, res;  
  
    while(count > 0)
    {  
        if ((res = read(fd, buf, count)) == 0)  
            break;  
        if (res < 0)  
            return result > 0 ? result : res;  
        count -= res;  
        result += res;  
        buf = (char *)buf + res;  
    }  
    return result;  
}  
  
  
void play_wav(pcm_container *pcm, wav_format *wav, int fd)  
{  
    int load, ret;  
    off64_t written = 0;  
    off64_t c;  
    off64_t total_bytes = LE_INT(wav->data.data_size);  

    uint32_t period_bytes =
                pcm->frames_per_period * pcm->bytes_per_frame;
  
    load = 0;  
    while (written < total_bytes)
    {
        // 一次循环地读取一个完整的周期数据
        do
        {  
            c = total_bytes - written;
            if (c > period_bytes)
                c = period_bytes;
            c -= load;
  
            if (c == 0)
                break;
            ret = read_pcm_from_wav(fd,
                                pcm->period_buf + load, c);

            if(ret < 0)
            {
                fprintf(stderr, "read() failed.\n");  
                exit(-1);  
            }  

            if (ret == 0)  
                break;  
            load += ret;  
        } while ((size_t)load < period_bytes);  
  
        /* Transfer to size frame */
        load = load / pcm->bytes_per_frame;  
        ret = write_pcm_to_device(pcm, load);  
        if (ret != load)  
            break;  
          
        ret = ret * pcm->bytes_per_frame;  
        written += ret;  
        load = 0;  
    }  
}  
  

void playback(int fd, struct tsdev *ts, char *fb_mem, struct fb_var_screeninfo *vinfo)
{
	// 手指松开按钮之后，开始播放
	while(ts_state == PRESSED)
    {
        printf("pppppppppppppp!\n");    
        usleep(10000);
    }

    printf("aaaaaaaaaaaaaaaaaaa!\n");

    system("./bin/aplay a.wav");
    return;

	flipover(BTN4, fb_mem, vinfo);
	fprintf(stderr, "playing...\n");

    // 1：定义存储WAV文件格式信息的结构体wav
    //    定义存储PCM设备相关信息的结构体playback
    wav_format *wav = calloc(1, sizeof(wav_format));  
    pcm_container *playback = calloc(1, sizeof(pcm_container));

    // 2：获取其相关音频信息
    lseek(fd, 0L, SEEK_SET);
    get_wav_header_info(fd, wav);

    // 3：以回放方式打开PCM设备
    //    并根据从WAV文件的头部获取音频信息，设置PCM设备参数
    snd_pcm_open(&playback->handle, "default", SND_PCM_STREAM_PLAYBACK, 0); 
    set_params(playback, wav);
  
    // 4：将WAV文件中除了头部信息之外的PCM数据读出，并写入PCM设备
    play_wav(playback, wav, fd);
  
    // 5：正常关闭PCM设备
    snd_pcm_drain(playback->handle);  
    snd_pcm_close(playback->handle);  
  
    // 6：释放相关资源
    free(playback->period_buf);  
    free(playback);  
    free(wav);  
    close(fd);  

	fprintf(stderr, "over\n");
	flipover(BTN4, fb_mem, vinfo);
}