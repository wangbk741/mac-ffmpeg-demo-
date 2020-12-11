//
//  testc.c
//  myapp
//
//  Created by lichao on 2020/1/30.
//  Copyright © 2020年 lichao. All rights reserved.
//

#include "testc.h"
#include <string.h>

static int rec_status = 0;
const char *out = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/audio.aac";

void set_status(int status){
    rec_status = status;
}

//[in]
//[out]
//ret
//@brief encode audio data
static void encode(AVCodecContext *ctx,
            AVFrame *frame,
            AVPacket *pkt,
            FILE *output){
    
    int ret = 0;
    
    //将数据送编码器
    ret = avcodec_send_frame(ctx, frame);
    
    //如果ret>=0说明数据设置成功
    while(ret >= 0){
        //获取编码后的音频数据,如果成功，需要重复获取，直到失败为止
        ret = avcodec_receive_packet(ctx, pkt);
        
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            return;
        }else if( ret < 0){
            printf("Error, encoding audio frame\n");
            exit(-1);
        }
        
        //write file
        printf("write data:%d \n",pkt->size);
        fwrite(pkt->data, 1, pkt->size, output);
        fflush(output);
    }
    
    return;
}

//[in]
//[out]
//
static AVCodecContext* open_coder(){
    
    //打开编码器
//    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVCodec *codec = avcodec_find_encoder_by_name("libfdk_aac");
    
    //创建 codec 上下文
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;          //输入音频的采样大小
    codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;    //输入音频的channel layout
    codec_ctx->channels = 2;                            //输入音频 channel 个数
    codec_ctx->sample_rate = 44100;                     //输入音频的采样率
    codec_ctx->bit_rate = 0; //AAC_LC: 128K, AAC HE: 64K, AAC HE V2: 32K
    codec_ctx->profile = FF_PROFILE_AAC_HE_V2; //阅读 ffmpeg 代码
    
//    codec_ctx->sample_fmt = codec->sample_fmts[0];
//    codec_ctx->codec = codec;
//    codec_ctx->sample_rate = 44100;
//    codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
//    codec_ctx->channels = 2;
//    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    //打开编码器
    if(avcodec_open2(codec_ctx, codec, NULL)<0){
        //
        
        return NULL;
    }
    
    return codec_ctx;
}

static
SwrContext* init_swr(){
    
    SwrContext *swr_ctx = NULL;
    
    //channel, number/
    swr_ctx = swr_alloc_set_opts(NULL,                //ctx
                                 AV_CH_LAYOUT_STEREO, //输出channel布局
                                 AV_SAMPLE_FMT_S16,   //输出的采样格式
                                 44100,               //采样率
                                 AV_CH_LAYOUT_STEREO, //输入channel布局
                                 AV_SAMPLE_FMT_FLT,   //输入的采样格式
                                 44100,               //输入的采样率
                                 0, NULL);
    
    if(!swr_ctx){
        
    }
    
    if(swr_init(swr_ctx) < 0){
        
    }
    
    return swr_ctx;
}

/**
  * @brief open audio device
  * @return succ: AVFormatContext*, fail: NULL
  */
static
AVFormatContext* open_dev(){
    
    int ret = 0;
    char errors[1024] = {0, };
    
    //ctx
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    
    fmt_ctx = avformat_alloc_context();
    av_dict_set(&options,"list_devices","true",0);

    //[[video device]:[audio device]]
    char *devicename = ":0";
    //get format
    AVInputFormat *iformat = av_find_input_format("avfoundation");
    
    //open device
    if((ret = avformat_open_input(&fmt_ctx, devicename, iformat, NULL)) < 0 ){
        av_strerror(ret, errors, 1024);
        fprintf(stderr, "Failed to open audio device, [%d]%s\n", ret, errors);
        return NULL;
    }
    if ((ret = avformat_find_stream_info(fmt_ctx, 0))<0) {
        printf("Failed to retrieve input stream information");
        return NULL;
    }
    av_dump_format(fmt_ctx, 0, devicename, 0);

    return fmt_ctx;
}

/**
 * @brief xxxx
 * @return xxx
 */
static
AVFrame* create_frame(){
    
    AVFrame *frame = NULL;
    
    //音频输入数据
    frame = av_frame_alloc();
    if(!frame){
        printf("Error, No Memory!\n");
        goto __ERROR;
    }
    
    //set parameters
    frame->nb_samples     = 512;                //单通道一个音频帧的采样数
    frame->format         = AV_SAMPLE_FMT_S16;  //每个采样的大小
    frame->channel_layout = AV_CH_LAYOUT_STEREO; //channel layout
    
    //alloc inner memory
    av_frame_get_buffer(frame, 0); // 512 * 2 * = 2048
    if(!frame->data[0]){
        printf("Error, Failed to alloc buf in frame!\n");
        //内存泄漏
        goto __ERROR;
    }
    
    return frame;
    
__ERROR:
    if(frame){
        av_frame_free(&frame);
    }
    
    return NULL;
}

static
void alloc_data_4_resample(uint8_t ***src_data,
                           int *src_linesize,
                           uint8_t *** dst_data,
                           int *dst_linesize){
    //4096/4=1024/2=512
    //创建输入缓冲区
    av_samples_alloc_array_and_samples(src_data,         //输出缓冲区地址
                                       src_linesize,     //缓冲区的大小
                                       2,                 //通道个数
                                       512,               //单通道采样个数
                                       AV_SAMPLE_FMT_FLT, //采样格式
                                       0);
    
    //创建输出缓冲区
    av_samples_alloc_array_and_samples(dst_data,         //输出缓冲区地址
                                       dst_linesize,     //缓冲区的大小
                                       2,                 //通道个数
                                       512,               //单通道采样个数
                                       AV_SAMPLE_FMT_S16, //采样格式
                                       0);
}

/**
 */
static
void free_data_4_resample(uint8_t **src_data, uint8_t **dst_data){
    //释放输入输出缓冲区
    if(src_data){
        av_freep(&src_data[0]);
    }
    av_freep(&src_data);
    
    if(dst_data){
        av_freep(&dst_data[0]);
    }
    av_freep(&dst_data);
}

/**
 */
static
void read_data_and_encode(AVFormatContext *fmt_ctx, //
                          AVCodecContext *c_ctx,
                          SwrContext* swr_ctx,
                          FILE *outfile){
    
    int ret = 0;
    
    //pakcet
    AVPacket pkt;
    AVFrame *frame = NULL;
    AVPacket *newpkt = NULL;
    
    //重采样缓冲区
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;

    frame = create_frame();
    if(!frame){
        //printf(...)
        goto __ERROR;
    }
    frame->format = AV_SAMPLE_FMT_S16;
    frame->channels = 2;
    //音频通道布局
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    frame->nb_samples = 1024;//一帧音频存放的样本数量
    ret = av_frame_get_buffer(frame,0);
    if (ret < 0)
    {
        goto __ERROR;
    }

    
    newpkt = av_packet_alloc(); //分配编码后的数据空间
    if(!newpkt){
        printf("Error, Failed to alloc buf in frame!\n");
        goto __ERROR;
    }
    
    //分配重采样输入/输出缓冲区
    alloc_data_4_resample(&src_data, &src_linesize, &dst_data, &dst_linesize);
    
    //read data from device
    while( rec_status) {
        if ((ret = av_read_frame(fmt_ctx, &pkt)) < 0) {
            if( ret == AVERROR_EOF){
//                printf("见底了 \n");
            }else{
//                printf("无法获取pkt \n");
            }
        }else{
            //进行内存拷贝，按字节拷贝的
            memcpy((void*)src_data[0], (void*)pkt.data, pkt.size);
            //重采样
            swr_convert(swr_ctx,                    //重采样的上下文
                        dst_data,                   //输出结果缓冲区
                        512,                        //每个通道的采样数
                        (const uint8_t **)src_data, //输入缓冲区
                        512);                       //输入单个通道的采样数

            //将重采样的数据拷贝到 frame 中
            memcpy((void *)frame->data[0], dst_data[0], dst_linesize);
//            printf("pkt %d \n",dst_linesize);
//            fwrite(dst_data[0], 1, dst_linesize, outfile);
//            fflush(outfile);
            //encode
            encode(c_ctx, frame, newpkt, outfile);
        }
        
        av_packet_unref(&pkt); //release pkt
    }
    
    //强制将编码器缓冲区中的音频进行编码输出
    encode(c_ctx, NULL, newpkt, outfile);

__ERROR:
    //释放 AVFrame 和 AVPacket
    if(frame){
        av_frame_free(&frame);
    }
    
    if(newpkt){
        av_packet_free(&newpkt);
    }
    
    //释放重采样缓冲区
    free_data_4_resample(src_data, dst_data);
}

void rec_audio() {
  
    //context
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVCodecContext *c_ctx = NULL;
    SwrContext* swr_ctx = NULL;

    //set log level
    av_log_set_level(AV_LOG_DEBUG);
    
    //register audio device
    avdevice_register_all();
    
    //start record
    rec_status = 1;
    
    //create file
    //char *out = "/Users/lichao/Downloads/av_base/audio.pcm";
    FILE *outfile = fopen(out, "wb+");
    if(!outfile){
        printf("Error, Failed to open file!\n");
        goto __ERROR;
    }
//获取输入上下文
    ifmt_ctx = open_dev();
    if(!ifmt_ctx){
        printf("Error, Failed to open device!\n");
        goto __ERROR;
    }
//输出上下文
    
    
    //打开编码器上下文
    c_ctx = open_coder();
    if(!c_ctx){
        printf("...");
        goto __ERROR;
    }

    //初始化重采样上下文
    swr_ctx = init_swr();
    if(!swr_ctx){
        printf("Error, Failed to alloc buf in frame!\n");
        goto __ERROR;
    }
//    AVPacket pkt;
//    FILE *file = fopen(out, "wb+");
//    while (1) {
//        if (av_read_frame(fmt_ctx, &pkt)<0) {
//            //            printf("read error\n");
//        }else{
//            fwrite(pkt.data, 1, pkt.size, file);
//            printf("%d \n",pkt.size);
//        }
//        av_packet_unref(&pkt);
//    }
    //encode
    read_data_and_encode(ifmt_ctx, c_ctx, swr_ctx, outfile);

__ERROR:
    //释放重采样的上下文
    if(swr_ctx){
        swr_free(&swr_ctx);
    }

    if(c_ctx){
        avcodec_free_context(&c_ctx);
    }
    
    //close device and release ctx
    if(ifmt_ctx) {
        avformat_close_input(&ifmt_ctx);
    }
    
    if(outfile){
        //close file
        fclose(outfile);
    }

    av_log(NULL, AV_LOG_DEBUG, "finish!\n");
    
    return;
}

#if 0
int main(int argc, char *argv[])
{
    rec_audio();
    return 0;
}
#endif
