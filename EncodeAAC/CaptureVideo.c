//
//  CaptureVideo.c
//  EncodeAAC
//
//  Created by ZQB on 2020/10/23.
//  Copyright © 2020年 lichao. All rights reserved.
//

#include "CaptureVideo.h"
/* The output bit rate in bit/s */
#define OUTPUT_BIT_RATE 96000
/* The number of output channels */
#define OUTPUT_CHANNELS 2

#define write_file 0
int status = 0;
int frame_index = 0;
int audioPts = 0;
int videoPts = 0;
char *pixel_format = "nv12";
//[[video device]0:Carmen,1:screen :[audio device]] 0：Microphone
char *audioDevice = ":0";
char *videoDevice = "1";
char *outPath = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/video.h264";
char *rtmp = "rtmp://192.168.53.13:1935/live/video";
char *out_type = "flv";
char *out_test_path = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/new.aac";
char *out_test_path1 = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/output.h264";
char *outAAC = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/video.aac";

char *outPCM = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/video.pcm";
char *outPCMs16 = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/videos16.pcm";
char *outYUV = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/video.yuv";
//4:2:0     视频分辨率*帧数*1.5
//4:2:2     视频分辨率*帧数*2
//4:4:4     视频分辨率*帧数*3
//uyvy422
int packSzie = 4147200;
//nv12
//int packSzie = 3110400;
char *videoSize = "1920x1080";
char *framerate = "20";
int video_FPS = 20;
int audio_rate = 44100;
int audioIndex = 1;
int videoIndex = 0;
#define BOOL int
#define TRUE 1
#define FALSE 0
BOOL isReady = FALSE;
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
const char *filter_descr = "movie=/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/icon.png[wm];[in][wm]overlay=0:0";

void setStatus(int statu){
    status = statu;
}

static int init_filters(const char *filters_descr,AVFormatContext *fmt_ctx,AVCodecContext *dec_ctx)
{   
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = fmt_ctx->streams[0]->time_base;
//    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE };
//    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
//                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
//    if (ret < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
//        goto end;
//    }
    
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
             time_base.num, time_base.den,
             dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);
    
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }
    
    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }
    
    
    
    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */
    
    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
    
    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;
    
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    
    return ret;
}

void show_avfoundation_device(){
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    AVDictionary* options = NULL;
    av_dict_set(&options,"list_devices","true",0);
    av_dict_set(&options, "framerate", framerate, 0);
    av_dict_set(&options, "pixel_format", pixel_format, 0);
    //在macOS上 如果是录制屏幕的状态那么video_size设置是无效的
    //最后生成的图像宽高是屏幕的宽高
    av_dict_set(&options, "video_size", videoSize, 0);
    AVInputFormat *iformat = av_find_input_format("avfoundation");
    printf("==AVFoundation Device Info===\n");
    avformat_open_input(&pFormatCtx,"",iformat,&options);
    printf("=============================\n");
}
int open_input_file(char *filename,
                    AVFormatContext **input_format_context,
                    AVCodecContext **input_codec_context){
    //ctx
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    AVCodec *input_codec;
    AVCodecContext *avctx;


    int error=-1;

    fmt_ctx = avformat_alloc_context();
    
    //get format
    AVInputFormat *ifmt=av_find_input_format("avfoundation");
    //  寻找Macos上的摄像头设备
//    av_dict_set(&options, "framerate", framerate, 0);
//    av_dict_set(&options, "pixel_format", pixel_format, 0);
//    //在macOS上 如果是录制屏幕的状态那么video_size设置是无效的
//    //最后生成的图像宽高是屏幕的宽高
//    av_dict_set(&options, "video_size", videoSize, 0);

    //open device
    if((error = avformat_open_input(&fmt_ctx, videoDevice, ifmt, NULL)) < 0 ){
//    if((error = avformat_open_input(&fmt_ctx, out_test_path1, NULL, NULL)) < 0 ){

        fprintf(stderr, "Failed to open audio device, %d\n", error);
        return error;
    }
    if ((error = avformat_find_stream_info(fmt_ctx, 0))<0) {
        printf("Failed to retrieve input stream information");
        return -1;
    }
    
    * input_format_context = fmt_ctx;
    
    /* Find a decoder for the audio stream. */
    //查找解码器
    if (!(input_codec = avcodec_find_decoder((*input_format_context)->streams[0]->codecpar->codec_id))) {
        fprintf(stderr, "Could not find input codec\n");
        avformat_close_input(input_format_context);
        return AVERROR_EXIT;
    }
    
    /* Allocate a new decoding context. */
    //创建解码器上下文
    avctx = avcodec_alloc_context3(input_codec);
    if (!avctx) {
        fprintf(stderr, "Could not allocate a decoding context\n");
        avformat_close_input(input_format_context);
        return AVERROR(ENOMEM);
    }
    
    /* Initialize the stream parameters with demuxer information. */
    //初始化流参数
    error = avcodec_parameters_to_context(avctx, (*input_format_context)->streams[0]->codecpar);
    if (error < 0) {
        avformat_close_input(input_format_context);
        avcodec_free_context(&avctx);
        return error;
    }
    
    /* Open the decoder for the audio stream to use it later. */
    //正式打开解码器
    if ((error = avcodec_open2(avctx, input_codec, NULL)) < 0) {
        fprintf(stderr, "Could not open input codec (error '%s')\n",
                av_err2str(error));
        avcodec_free_context(&avctx);
        avformat_close_input(input_format_context);
        return error;
    }
    
    /* Save the decoder context for easier access later. */
    //返回解码器上下文
    *input_codec_context = avctx;
    return 0;
}




int parperBox(
              AVFormatContext *ofmt_ctx,
              AVPacket *inputPkt,
              AVStream *in_stream,
              AVStream *out_stream
){


    int error = -1;
    inputPkt->pts = av_rescale_q_rnd(inputPkt->pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    inputPkt->dts = av_rescale_q_rnd(inputPkt->dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
    inputPkt->duration = av_rescale_q(inputPkt->duration, in_stream->time_base, out_stream->time_base);
    inputPkt->pos = -1;
    switch (inputPkt->stream_index) {
        case 0:
            {
                printf("video: %lld size: %d duration:%lld \n",inputPkt->dts,inputPkt->size,inputPkt->duration);
            }
            break;
        case 1:
            {
                printf("audio %lld size: %d duration:%lld \n",inputPkt->dts,inputPkt->size,inputPkt->duration);
            }
        default:
            break;
    }

    error = av_interleaved_write_frame(ofmt_ctx, inputPkt);
    if (error<0) {
        printf("av_write_frame error：%s\n",av_err2str(error));
        goto cleanup;
    }
    
    av_packet_unref(inputPkt);
    return 0;
    
cleanup:
    return error < 0 ? error : AVERROR_EXIT;
}





static void get_adts_header(AVCodecContext *ctx, uint8_t *adts_header, int aac_length)
{
    uint8_t freq_idx = 4;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
    switch (ctx->sample_rate) {
        case 96000: freq_idx = 0; break;
        case 88200: freq_idx = 1; break;
        case 64000: freq_idx = 2; break;
        case 48000: freq_idx = 3; break;
        case 44100: freq_idx = 4; break;
        case 32000: freq_idx = 5; break;
        case 24000: freq_idx = 6; break;
        case 22050: freq_idx = 7; break;
        case 16000: freq_idx = 8; break;
        case 12000: freq_idx = 9; break;
        case 11025: freq_idx = 10; break;
        case 8000: freq_idx = 11; break;
        case 7350: freq_idx = 12; break;
        default: freq_idx = 4; break;
    }
    uint8_t chanCfg = ctx->channels;
    uint32_t frame_length = aac_length + 7;
    adts_header[0] = 0xFF;
    adts_header[1] = 0xF1;
    adts_header[2] = ((ctx->profile) << 6) + (freq_idx << 2) + (chanCfg >> 2);
    adts_header[3] = (((chanCfg & 3) << 6) + (frame_length  >> 11));
    adts_header[4] = ((frame_length & 0x7FF) >> 3);
    adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
    adts_header[6] = 0xFC;
}




int addNewStream(
    AVFormatContext *out_ctx,
    AVFormatContext *in_ctx
){
    int ret = -1;
    AVStream *in_stream = in_ctx->streams[0];
    AVStream *out_stream = avformat_new_stream(out_ctx, in_stream->codec->codec);
    if (!out_stream) {
        printf( "Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        return  ret;
    }
    //Copy the settings of AVCodecContext
    if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
        printf( "Failed to copy context from input to output stream codec context\n");
        return -1;
    }
    out_stream->codec->codec_tag = 0;
    out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (out_stream->codec->codec_type==AVMEDIA_TYPE_AUDIO) {
        out_ctx->streams[audioIndex] = out_stream;
    }else{
        out_ctx->streams[videoIndex] = out_stream;
    }
    return 0;
}
int create_H264_Stream(
        AVFormatContext *input_ctx,
        AVCodecContext *input_codec_ctx,
                       AVFormatContext **out_ctx,
                       AVCodecContext **out_codec_ctx
){
    int ret = -1;
    AVFormatContext *ofmt_ctx = NULL;
    AVCodec *output_codec = NULL;
    AVCodecContext *output_codec_ctx = NULL;
    //创建输出格式上下
    if (!(ofmt_ctx = avformat_alloc_context())) {
        fprintf(stderr, "Could not allocate output format context\n");
        return AVERROR(ENOMEM);
    }
    AVOutputFormat *fmt = NULL;
    if (write_file) {
        fmt = av_guess_format(NULL,outPath, NULL);
    }else{
        fmt = av_guess_format("h264", NULL, NULL);
    }
    ofmt_ctx->oformat = fmt;

    //查找输出容器的编码器,使用输出格式的上下文的codec查找编码器
    
    output_codec = avcodec_find_encoder(AV_CODEC_ID_H264);//查找编×××
    if(output_codec == NULL)
    {
        printf("find avcodec_find_encoder fail! \n");
        return -1;
    }
    

    //返回编码器
    output_codec_ctx = avcodec_alloc_context3(output_codec);
    if (!output_codec_ctx) {
        fprintf(stderr, "Could not allocate an encoding context\n");
        ret = AVERROR(ENOMEM);
        return ret;
    }
    
    /* Set the basic encoder parameters.
     * The input file's sample rate is used to avoid a sample rate conversion. */
    
    //设置解码基础参数
    output_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    output_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;//input_codec_context->pix_fmt;
    output_codec_ctx->width = input_codec_ctx->width;
    output_codec_ctx->height = input_codec_ctx->height;
    //设置帧率
    output_codec_ctx->time_base.num = 1;
    output_codec_ctx->time_base.den = 25;
    output_codec_ctx->bit_rate = input_codec_ctx->bit_rate;
    //GOP:画面组，一组连续画面（一个完整的画面）
    output_codec_ctx->gop_size = 12;
    //设置量化参数
    output_codec_ctx->qmin = 10;
    output_codec_ctx->qmax = 51;
    //设置B帧最大值,0，表示不需要B帧
    output_codec_ctx->max_b_frames = 1;
    
    
    //h264解码器的设置
    AVDictionary *param = 0;
    if (output_codec_ctx->codec_id == AV_CODEC_ID_H264) {
        // 查看h264.c源码

        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }
    
    /* Open the encoder for the audio stream to use it later. */
    //打开输出格式编码器
    if ((ret = avcodec_open2(output_codec_ctx, output_codec, NULL)) < 0) {
        fprintf(stderr, "Could not open output codec (error '%s')\n",
                av_err2str(ret));
        return ret;
    }
    
    
    //设置流相关
    AVStream *in_stream = input_ctx->streams[0];
    
    AVStream *out_stream = avformat_new_stream(ofmt_ctx, output_codec);
    
    if (!out_stream) {
        printf( "Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        return -1;
    }
    //Copy the settings of AVCodecContext
    //    if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
    //        printf( "Failed to copy context from input to output stream codec context\n");
    //        return -1;
    //    }
    out_stream->time_base = (AVRational) {1, 90000};
    out_stream->r_frame_rate.num = video_FPS;
    out_stream->r_frame_rate.den = 1;
    out_stream->codec->width = input_ctx->streams[0]->codec->width;
    out_stream->codec->height = input_ctx->streams[0]->codec->height;
    out_stream->codec->codec_tag = 0;
    unsigned char sps_pps[23] = { 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x0a, 0xf8, 0x0f, 0x00, 0x44, 0xbe, 0x8,
        0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x38, 0x80 };
    out_stream->codec->extradata_size = 23;
    out_stream->codec->extradata = (uint8_t*)av_malloc(23 + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(out_stream->codec->extradata, sps_pps, 23);
//    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
//    out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    ofmt_ctx->streams[0] = out_stream;
    
    
    *out_ctx = ofmt_ctx;
    *out_codec_ctx = output_codec_ctx;
    return 0;
}
void make_dsi(unsigned int sampling_frequency_index,
              unsigned int channel_configuration,
              unsigned char* dsi )
{
    unsigned int object_type = 2; // AAC LC by default
    dsi[0] = (object_type<<3) | (sampling_frequency_index>>1);
    dsi[1] = ((sampling_frequency_index&1)<<7) | (channel_configuration<<3);
}

int create_AAC_Stream(
                       AVFormatContext *input_ctx,
                       AVCodecContext *input_codec_ctx,
                       AVFormatContext **out_ctx,
                       AVCodecContext **out_codec_ctx){
    int ret = -1;
    AVFormatContext *ofmt_ctx = NULL;
    AVCodec *output_codec = NULL;
    AVCodecContext *output_codec_ctx = NULL;
    //创建输出格式上下
    if (!(ofmt_ctx = avformat_alloc_context())) {
        fprintf(stderr, "Could not allocate output format context\n");
        return AVERROR(ENOMEM);
    }
    
    AVOutputFormat *fmt = NULL;
    if (write_file) {
        fmt = av_guess_format(NULL,outAAC, NULL);
    }else{
        fmt = av_guess_format("aac", NULL, NULL);
    }
    ofmt_ctx->oformat = fmt;
    
    //查找输出容器的编码器,使用输出格式的上下文的codec查找编码器
    output_codec = avcodec_find_encoder_by_name("libfdk_aac");
    

//    output_codec = avcodec_find_encoder_by_name("libfdk_aac");
//    output_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);//查找编×××
    if(output_codec == NULL)
    {
        printf("find avcodec_find_encoder fail! \n");
        return -1;
    }
    
    //返回编码器
    output_codec_ctx = avcodec_alloc_context3(output_codec);
    if (!output_codec_ctx) {
        fprintf(stderr, "Could not allocate an encoding context\n");
        ret = AVERROR(ENOMEM);
        return ret;
    }
    // 对于不同的编码器最优码率不一样，单位bit/s;
    output_codec_ctx->bit_rate = 128000;//input_codec_ctx->bit_rate;
    output_codec_ctx->codec_id = AV_CODEC_ID_AAC;
    output_codec_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    output_codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    output_codec_ctx->sample_rate = input_codec_ctx->sample_rate;
    output_codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;//input_codec_ctx->channel_layout;
    output_codec_ctx->channels = 2;//input_codec_ctx->channels;
    output_codec_ctx->profile = FF_PROFILE_AAC_HE_V2;
    
    
  
    /* Open the encoder for the audio stream to use it later. */
    //打开输出格式编码器
    if ((ret = avcodec_open2(output_codec_ctx, output_codec, NULL)) < 0) {
        fprintf(stderr, "Could not open output codec (error '%s')\n",
                av_err2str(ret));
        return ret;
    }
    
    //设置流相关
    AVStream *out_stream = avformat_new_stream(ofmt_ctx, output_codec_ctx->codec);
    
    if (!out_stream) {
        printf( "Failed allocating output stream\n");
        ret = AVERROR_UNKNOWN;
        return -1;
    }
    //Copy the settings of AVCodecContext
    //    if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
    //        printf( "Failed to copy context from input to output stream codec context\n");
    //        return -1;
    //    }
    out_stream->time_base = (AVRational) {1, audio_rate};
    //AAC一帧的播放时间是= 1024*1000/44100= 22.32ms
    //所以一秒有1000/22.32帧
    out_stream->r_frame_rate.num = 44;
    out_stream->r_frame_rate.den = 1;
    out_stream->codec->codec_tag = 0;
    out_stream->codec->sample_rate = audio_rate;
//    out_stream->codec->extradata = (uint8_t*)av_malloc(2);
//    out_stream->codec->extradata_size = 2;
//    unsigned char dsi1[2];
//    //这个get_sr_index就是根据采样率返回下标，与adts的sampling_frequency_index定义相同，如48000返回3
//    unsigned int sampling_frequency_index = 4;//(unsigned int)get_sr_index((unsigned int)out_stream->codec->sample_rate);
//    make_dsi(sampling_frequency_index, (unsigned int)out_stream->codec->channels, dsi1 );
//    memcpy(out_stream->codec->extradata, dsi1, 2);
//    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    //设置头部信息
//    out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    ofmt_ctx->streams[0] = out_stream;
    *out_ctx = ofmt_ctx;
    *out_codec_ctx = output_codec_ctx;
    return 0;
}
int test1_endian() {
    
    int i = 1;
    
    char *a = (char *)&i;
    
    if (*a == 1)
        
        printf("小端\n");
    
    else
        
        printf("大端\n");
    
    return 0;
    
}


void startVideo(){
    
    test1_endian();
    //video
    AVFormatContext *input_format_context = NULL;
    AVCodecContext *intput_codec_context = NULL;
    AVFormatContext *output_format_context = NULL;
    AVCodecContext *output_codec_context = NULL;
    AVPacket pkt_a;
    AVPacket pkt_v;
    frame_index = 0;
    //set log level
//    av_log_set_level(AV_LOG_DEBUG);
    
    //register audio device
    avdevice_register_all();
    
    //start record
    status = 1;
    show_avfoundation_device();
    int ret = -1;
    
    
    //创建输入流
    ret = open_input_file(NULL, &input_format_context,&intput_codec_context);
    if (ret<0) {
        printf("open_input_file error");
        return;
    }
    FILE *outfile = NULL;
    FILE *out_yuv = NULL;
    if (write_file) {
       outfile  = fopen(outPath, "wb+");
//       out_yuv  = fopen(outYUV, "wb+");
    }
    
    

    ret = create_H264_Stream(input_format_context, intput_codec_context, &output_format_context, &output_codec_context);
    if (ret<0) {
        printf("open_output_file error");
        return;
    }
    
    AVFormatContext *ofmt_ctx = NULL;
    AVOutputFormat *ofmt = NULL;

    avformat_alloc_output_context2(&ofmt_ctx, NULL, out_type, rtmp);
    if (!ofmt_ctx) {
        printf( "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return;
    }
    ofmt = ofmt_ctx->oformat;
    ret = addNewStream(ofmt_ctx, output_format_context);
    if (ret<0) {
        printf("set video stream error");
        return;
    }
    AVStream *out_stream      = NULL;
    AVStream *in_stream       = NULL;
    
    //YUV格式转换
    AVFrame                *pFrame = NULL, *pFrameYUV = NULL;
    unsigned char        *out_buffer = NULL;
    struct SwsContext    *img_convert_ctx = NULL;

    
    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    if (pFrame == NULL || pFrameYUV == NULL)
    {
        printf("memory allocation error\n");
        return;
    }

    pFrame->format = intput_codec_context->pix_fmt;
    pFrame->width  = intput_codec_context->width;
    pFrame->height = intput_codec_context->height;
    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, intput_codec_context->width, intput_codec_context->height, 1));
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
                        AV_PIX_FMT_YUV420P, intput_codec_context->width, intput_codec_context->height, 1);
    img_convert_ctx = sws_getContext(intput_codec_context->width, intput_codec_context->height,                        intput_codec_context->pix_fmt,
                                         intput_codec_context->width, intput_codec_context->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    
    //audio
    AVFormatContext *input_format_context_a = NULL;
    AVCodecContext *intput_codec_context_a = NULL;
    
    AVCodec *input_codec_a;
    frame_index = 0;

    
    input_format_context_a = avformat_alloc_context();
    
    //get format
    AVInputFormat *ifmt=av_find_input_format("avfoundation");
    //open device
//    if((ret = avformat_open_input(&input_format_context_a, out_test_path, NULL, NULL)) < 0 ){
    if((ret = avformat_open_input(&input_format_context_a, audioDevice, ifmt, NULL)) < 0 ){

        printf( "Failed to open audio device, \n");
        return;
    }
    if ((ret = avformat_find_stream_info(input_format_context_a, 0))<0) {
        printf("Failed to retrieve input stream information");
        return ;
    }
    //查找解码器
    if (!(input_codec_a = avcodec_find_decoder(input_format_context_a->streams[0]->codecpar->codec_id))) {
        fprintf(stderr, "Could not find input codec\n");
        avformat_close_input(&input_format_context_a);
        return;
    }
    
    /* Allocate a new decoding context. */
    //创建解码器上下文
    intput_codec_context_a = avcodec_alloc_context3(input_codec_a);
    if (!intput_codec_context_a) {
        fprintf(stderr, "Could not allocate a decoding context\n");
        avformat_close_input(&input_format_context_a);
        return ;
    }
    
    /* Initialize the stream parameters with demuxer information. */
    //初始化流参数
    ret = avcodec_parameters_to_context(intput_codec_context_a, input_format_context_a->streams[0]->codecpar);
    if (ret < 0) {
        avformat_close_input(&input_format_context_a);
        avcodec_free_context(&intput_codec_context_a);
        return;
    }
    
    /* Open the decoder for the audio stream to use it later. */
    //正式打开解码器
    if ((ret = avcodec_open2(intput_codec_context_a, input_codec_a, NULL)) < 0) {
        fprintf(stderr, "Could not open input codec (error '%s')\n",
                av_err2str(ret));
        avcodec_free_context(&intput_codec_context_a);
        avformat_close_input(&input_format_context_a);
        return;
    }
    //编码器设置部分
    AVCodecContext *pCodecCtx_a = NULL;
    AVFormatContext *pFormatCtx_a = NULL;

    
    AVPacket encodePkt;

    ret = create_AAC_Stream(input_format_context_a, intput_codec_context_a, &pFormatCtx_a, &pCodecCtx_a);
    if (ret<0) {
        printf("create_AAC_Stream error");
        return;
    }
    //添加音频流
    ret = addNewStream(ofmt_ctx, pFormatCtx_a);
    if (ret<0) {
        printf("set audio stream error");
        return;
    }
    //Open output file
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, rtmp, AVIO_FLAG_WRITE) < 0) {
            printf( "Could not open output file '%s'", rtmp);
            return;
        }
    }
    //创建头部i信息
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret<0) {
        printf("write header error");
        return ;
    }
    //设置输出frame
    AVFrame *pFrame_a = NULL;
    pFrame_a = av_frame_alloc();
    pFrame_a->nb_samples     = 512;               //单通道一个音频帧的采样数
    pFrame_a->format         = AV_SAMPLE_FMT_S16;  //每个采样的大小
    pFrame_a->channel_layout = AV_CH_LAYOUT_STEREO; //channel layout
    //分配buf
    ret = av_frame_get_buffer(pFrame_a, 0);
    if (ret<0) {
        printf("av_frame_get_buffer error");
        return ;
    }
    
    
    //重采样设置部分
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    
    
    SwrContext* swr_ctx = swr_alloc_set_opts(NULL,                //ctx
                                             AV_CH_LAYOUT_STEREO, //输出channel布局
                                             AV_SAMPLE_FMT_S16,   //输出的采样格式
                                             audio_rate,               //采样率
                                             AV_CH_LAYOUT_STEREO, //输入channel布局
                                             AV_SAMPLE_FMT_FLT,   //输入的采样格式
                                             audio_rate,               //输入的采样率
                                             0, NULL);
    swr_init(swr_ctx);
    //4096/4=1024/2=512
    //创建输入缓冲区
    ret = av_samples_alloc_array_and_samples(&src_data,         //输出缓冲区地址
                                             &src_linesize,     //缓冲区的大小
                                             2,                 //通道个数
                                             512,               //单通道采样个数
                                             AV_SAMPLE_FMT_FLT, //采样格式
                                             0);
    
    //创建输出缓冲区
    ret = av_samples_alloc_array_and_samples(&dst_data,         //输出缓冲区地址
                                             &dst_linesize,     //缓冲区的大小
                                             2,                 //通道个数
                                             512,               //单通道采样个数
                                             AV_SAMPLE_FMT_S16, //采样格式
                                             0);
    
    

    //创建编码器
    AVPacket H264Packet;
    av_init_packet(&H264Packet);
    
    av_init_packet(&encodePkt);
    const AVBitStreamFilter *h264bsfc =  av_bsf_get_by_name("h264_mp4toannexb");

    AVBSFContext *h264Ctx = NULL;
    AVCodecParameters *h264codecpar = NULL;
    //2.过滤器分配内存
    av_bsf_alloc(h264bsfc,&h264Ctx);
    //3. 添加解码器属性

    h264codecpar = output_format_context->streams[0]->codecpar;
    h264codecpar->codec_type = output_format_context->streams[0]->codec->codec_type;
    h264codecpar->codec_id = output_format_context->streams[0]->codec->codec_id;
    h264codecpar->format = output_format_context->streams[0]->codec->codec->pix_fmts[0];
    h264codecpar->width = output_format_context->streams[0]->codec->width;
    h264codecpar->height = output_format_context->streams[0]->codec->height;
    h264codecpar->channel_layout = pFrame->channel_layout;
    h264codecpar->channels = output_format_context->streams[0]->codec->channels;
    h264codecpar->sample_rate = output_format_context->streams[0]->codec->sample_rate;
    avcodec_parameters_copy(h264Ctx->par_in, h264codecpar);

    
    //4. 初始化过滤器上下文
    av_bsf_init(h264Ctx);

    
    //滤镜c初始化
//    ret = init_filters(filter_descr, input_format_context, intput_codec_context);
//    if (ret<0) {
//        printf("filters error \n");
//        return;
//    }
//    AVFrame *filt_frame = av_frame_alloc();
    
    int64_t cur_pts_v=0,cur_pts_a=0;
    
    FILE *file_aac = NULL;
    FILE *file_pcm = NULL;
    FILE *file_pcm_s16 = NULL;
    if (write_file) {
        file_aac = fopen(outAAC, "wb+");
        file_pcm = fopen(outPCM, "wb+");
        file_pcm_s16 = fopen(outPCMs16, "wb+");
    }
    av_init_packet(&pkt_v);
    av_init_packet(&pkt_a);

    while (1) {
        if (!status) {
            break;
        }
        ret = av_compare_ts(cur_pts_v,input_format_context->streams[0]->time_base,cur_pts_a,input_format_context_a->streams[0]->time_base);
        if(ret <= 0){
            do {
                ret = av_read_frame(input_format_context, &pkt_v);
            } while (ret!=0);
            if (ret>=0) {
                ret = avcodec_send_packet(intput_codec_context, &pkt_v);
                if (ret<0) {
                    printf("send_packet error \n");
                    break;
                }

                while (ret>=0) {
                ret = avcodec_receive_frame(intput_codec_context, pFrame);
                if (ret==0) {

                            //输出出YUV420P数据
                            sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, intput_codec_context->height,
                                      pFrameYUV->data, pFrameYUV->linesize);
                            if (out_yuv) {
                                int y_size=intput_codec_context->width*intput_codec_context->height;
                                fwrite(pFrameYUV->data[0], 1, y_size, out_yuv);
                                fwrite(pFrameYUV->data[1], 1, y_size/4, out_yuv);
                                fwrite(pFrameYUV->data[2], 1, y_size/4, out_yuv);
                                fflush(out_yuv);
                            }
                            in_stream  = output_format_context->streams[0];
                            out_stream = ofmt_ctx->streams[videoIndex];
                            pFrameYUV->format = AV_PIX_FMT_YUV420P;
                            pFrameYUV->width = pFrame->width;
                            pFrameYUV->height = pFrame->height;
                            //转换YUV420数据
                            ret = avcodec_send_frame(output_codec_context, pFrameYUV);
                            if (ret<0) {
                                printf("send pFrameYUV error");
                                break;
                            }
                            while (ret>=0) {
                                ret = avcodec_receive_packet(output_codec_context, &H264Packet);
                                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                                    break;
                                }
                                
                                if (write_file) {
                                    fwrite(H264Packet.data, 1, H264Packet.size, outfile);
                                    fflush(outfile);
                                }

                                
                                
                                
                                //设置流索引
                                H264Packet.stream_index = videoIndex;
                                
                                AVPacket adts_pkt ;
                                av_init_packet(&adts_pkt);
                                //5. AVPacket处理
                                do {
                                    ret = av_bsf_send_packet(h264Ctx, &H264Packet);
                                } while (ret!=0);
                                ret = av_bsf_receive_packet(h264Ctx, &adts_pkt);
                                if (ret==0) {
                                    //普通的无B桢视频(H264 Baseline或者VP8)，PTS/DTS应该是相等的，因为没有延迟编码
                                        AVRational time_base1=in_stream->time_base;
                                        //Duration between 2 frames (us)
                                        int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
                                        //Parameters
                                        adts_pkt.pts=(double)(videoPts*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                                        adts_pkt.dts = adts_pkt.pts;
                                        adts_pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                                    cur_pts_v = adts_pkt.pts;
                                    videoPts ++;

                                    ret = parperBox(ofmt_ctx,&adts_pkt,in_stream,out_stream);
                                    
                                    if (ret<0) {
                                        printf("parperBox error\n");
                                        return;
                                    }
                                }
                                
                                av_packet_unref(&adts_pkt);
                            }
                        }

            }
                
            av_packet_unref(&pkt_v);
        }

        }else{
            do {
                ret = av_read_frame(input_format_context_a, &pkt_a);
            } while (ret!=0);
            if (ret>=0) {
                //pcm
                if (file_pcm) {
                    fwrite(pkt_a.data, 1, pkt_a.size, file_pcm);
                    fflush(file_pcm);
                }

                //        进行内存拷贝，按字节拷贝的
                memcpy((void*)src_data[0], (void*)pkt_a.data, pkt_a.size);
                //重采样
                swr_convert(swr_ctx,                    //重采样的上下文
                            dst_data,                   //输出结果缓冲区
                            512,                        //每个通道的采样数
                            (const uint8_t **)src_data, //输入缓冲区
                            512);                       //输入单个通道的采样数
                
                //将重采样的数据拷贝到 frame 中
                memcpy((void *)pFrame_a->data[0], dst_data[0], dst_linesize);
                if (file_pcm_s16) {
                    fwrite(pFrame_a->data[0], 1, dst_linesize, file_pcm_s16);
                    fflush(file_pcm_s16);
                }
                if (pFormatCtx_a==NULL||pFrame_a==NULL) {
                    printf("here");
                    goto end;
                }
                ret = avcodec_send_frame(pCodecCtx_a, pFrame_a);
                if (ret<0) {
                    printf("send pFrame_a error");
                    break;
                }
                while (ret>=0) {
                    ret = avcodec_receive_packet(pCodecCtx_a, &encodePkt);
                    if (ret==0) {

                        if (write_file) {
                            fwrite(encodePkt.data, 1, encodePkt.size, file_aac);
                            fflush(file_aac);
                        }
                        in_stream = pFormatCtx_a->streams[0];
                        out_stream = ofmt_ctx->streams[audioIndex];
                            AVRational time_base1 = in_stream->time_base;
                            int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
                            encodePkt.pts=(double)(audioPts*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
                            encodePkt.dts = encodePkt.pts;
                            encodePkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);

                        encodePkt.stream_index = audioIndex;
                        cur_pts_a = encodePkt.pts;
                        audioPts ++;
                        
                        ret = parperBox(ofmt_ctx,&encodePkt,in_stream,out_stream);
                        if (ret<0) {
                            printf("parperBox error\n");
                            return;
                        }
                    }else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                        break;
                    }
                }
                av_packet_unref(&pkt_a);

            }
            
        }

    }
    
  
    

    ret = av_write_trailer(ofmt_ctx);
    if (ret<0) {
        printf("av_write_trailer error");
        return;
    }
    
    printf("end");
end:
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    av_bsf_free(&h264Ctx);

    av_packet_unref(&pkt_a);
    av_packet_unref(&pkt_v);

    av_packet_unref(&H264Packet);
    avcodec_close(output_codec_context);
    avcodec_close(intput_codec_context);
    av_free(ofmt_ctx);
    av_free(output_codec_context);
    av_free(intput_codec_context);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    av_frame_free(&pFrame_a);
}



