//
//  leixiaohua.c
//  EncodeAAC
//
//  Created by ZQB on 2020/10/23.
//  Copyright © 2020年 lichao. All rights reserved.
//

#include "leixiaohua.h"
#define STREAM_DURATION   10.0  //视频预设总时长
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */



typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    
    int64_t next_pts;
    int samples_count;
    
    AVFrame *frame;
    AVFrame *tmp_frame;
    
    float t, tincr, tincr2;
    
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;  //相较于标准的结构体少了后面的部分，只有前面的几个参数


typedef struct IntputDev {
    AVCodecContext  *pCodecCtx;          //pCodecCtx=v_ifmtCtx->streams[videoindex]->codec;
    AVCodec         *pCodec;              //pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    AVFormatContext *v_ifmtCtx;          //avformat_alloc_context +  avformat_open_input(&v_ifmtCtx,"/dev/video0",ifmt,NULL)
    int  videoindex;
    struct SwsContext *img_convert_ctx;
    AVPacket *in_packet;          //(AVPacket *)av_malloc(sizeof(AVPacket)); -->av_read_frame得到内容
    AVFrame *pFrame,*pFrameYUV;  //av_frame_alloc---->解码得到pFrame→→格式转换→→pFrameYUV    avpicture_fill((AVPicture *)pFrameYUV, out_buffer..)
}IntputDev;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)  //log日志相关 打印一些信息     Definition at line 70 of file muxing.c.
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    
    printf("pts:%d pts_time:%s duration:%d\n",av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),av_ts2str(pkt->duration));
    /*
     printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
     av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
     av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
     av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
     pkt->stream_index);  //这里pts和dts都源自编码前frame.pts 而duration没有被赋值，是初始化时的0
     */
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)  //Definition at line 81 of file muxing.c.
//pkt写入之前要先处理pkt->pts和pkt->stream_index
{
    /* rescale output packet timestamp values from codec(帧序号) to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    //例如：av_packet_rescale_ts将pkt->pts从帧序号33→时间戳118800
    pkt->stream_index = st->index;
    
    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

static void add_stream(OutputStream *ost, AVFormatContext *oc,  AVCodec **codec,  enum AVCodecID codec_id)
//1.*codec = avcodec_find_encoder(codec_id);   2.ost->st = avformat_new_stream(oc, NULL);         3.ost->enc = c = avcodec_alloc_context3(*codec); 4.填充AVCodecContext *c;
{
    AVCodecContext *c;
    int i;
    
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {  fprintf(stderr, "Could not find encoder for '%s'\n",  avcodec_get_name(codec_id));  exit(1);  }
    
    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {  fprintf(stderr, "Could not allocate stream\n");  exit(1);  }
    
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {  fprintf(stderr, "Could not alloc an encoding context\n");  exit(1);  }
    ost->enc = c;
    
    switch ((*codec)->type) {
        case AVMEDIA_TYPE_AUDIO:
            c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            c->bit_rate    = 64000;
            c->sample_rate = 44100;
            if ((*codec)->supported_samplerates) {
                c->sample_rate = (*codec)->supported_samplerates[0];
                for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                    if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
                }
            }
            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
            c->channel_layout = AV_CH_LAYOUT_STEREO;
            if ((*codec)->channel_layouts) {
                c->channel_layout = (*codec)->channel_layouts[0];
                for (i = 0; (*codec)->channel_layouts[i]; i++) {
                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
                }
            }
            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
            ost->st->time_base = (AVRational){ 1, c->sample_rate };
            break;
            
        case AVMEDIA_TYPE_VIDEO:
            c->codec_id = codec_id;
            
            c->bit_rate = 400000;
            /* Resolution must be a multiple of two. */
            c->width    = 1920;
            c->height   = 1080;
            /* timebase: This is the fundamental unit of time (in seconds) in terms
             * of which frame timestamps are represented. For fixed-fps content,
             * timebase should be 1/framerate and timestamp increments should be
             * identical to 1. */
            ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
            c->time_base       = ost->st->time_base;
            
            c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
            c->pix_fmt       = STREAM_PIX_FMT;
            if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
                /* just for testing, we also add B-frames */
                c->max_b_frames = 2;
            }
            if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
                /* Needed to avoid using macroblocks in which some coeffs overflow.
                 * This does not happen with normal video, it just happens here as
                 * the motion of the chroma plane does not match the luma plane. */
                c->mb_decision = 2;
            }
            break;
            
        default:
            break;
    }
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  //oc的格式需要分离的文件头→→c的格式也需要分离的文件头
}






/**************************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)  //申请一个指定长宽像素的AVFrame
{
    AVFrame *picture;
    int ret;
    
    picture = av_frame_alloc();  //this only allocates the AVFrame itself, not the data buffers.
    //Those must be allocated through other means, e.g. with av_frame_get_buffer() or manually.
    if (!picture)
    return NULL;
    
    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;
    
    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);  //为音频或视频数据分配新的缓冲区。
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }
    
    return picture;
}

static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
//1.avcodec_open2(c, codec, &opt);    2.allocate and init a re-usable frame 3.prepar ost->tmp_frame 4.avcodec_parameters_from_context
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;
    
    av_dict_copy(&opt, opt_arg, 0);
    
    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }
    
    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {  fprintf(stderr, "Could not allocate video frame\n");  exit(1);  }
    //printf("ost->frame alloc success fmt=%d w=%d h=%d\n",c->pix_fmt,c->width, c->height);
    
    
    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }
    
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}



// encode one video frame and send it to the muxer  return 1 when encoding is finished, 0 otherwise
static int write_video_frame1(AVFormatContext *oc, OutputStream *ost,AVFrame *frame)
//1.avcodec_encode_video2     2.调用write_frame函数
{
    int ret;
    AVCodecContext *c;
    int got_packet = 0;
    AVPacket pkt = { 0 };
    
    if(frame==NULL)  return 1;
    
    c = ost->enc;
    av_init_packet(&pkt);
    
    
    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);  //pkt.pts搬自frame.pts
    if (ret < 0) {  fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));  exit(1);  }
    
    printf("---video- pkt.pts=%s     ",av_ts2str(pkt.pts));  //av_ts2str即将包含时间戳的int64_t变成char*buf    起始就是输出帧序号
    if (pkt.pts == 0) printf("----st.num=%d st.den=%d codec.num=%d codec.den=%d---------\n",ost->st->time_base.num,ost->st->time_base.den,  c->time_base.num,c->time_base.den);
    //输出流的AVRational 和 编码器的AVRational
    
    if (got_packet) {  ret = write_frame(oc, &c->time_base, ost->st, &pkt);  }
    else {  ret = 0;  }
    
    if (ret < 0) {  fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));  exit(1);  }
    
    return (frame || got_packet) ? 0 : 1;
}


static AVFrame *get_video_frame1(OutputStream *ost,IntputDev* input,int *got_pic)
//1.av_frame_make_writable        2.av_read_frame--avcodec_decode_video2---sws_scale
//数据流：input->in_packet----input->pFrame----ost->frame
{
    
    int ret, got_picture;
    AVCodecContext *c = ost->enc;
    AVFrame * ret_frame=NULL;
    if (av_compare_ts(ost->next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)  //时间戳超过指定的视频持续时间STREAM_DURATION就return
    return NULL;
    
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
    exit(1);
    
    ret = -1;
    do {
        ret = av_read_frame(input->v_ifmtCtx, input->in_packet);
    } while (ret!=0);
    if(input->in_packet->stream_index==input->videoindex){
        ret = avcodec_decode_video2(input->pCodecCtx, input->pFrame, &got_picture, input->in_packet);
        *got_pic=got_picture;
    
        if(ret < 0){
            printf("Decode Error.\n");
            av_free_packet(input->in_packet);
            return NULL;
        }
        if(got_picture){
            sws_scale(input->img_convert_ctx, (const unsigned char* const*)input->pFrame->data, input->pFrame->linesize, 0, input->pCodecCtx->height, ost->frame->data,  ost->frame->linesize);
            ost->frame->pts =ost->next_pts++;
            ret_frame= ost->frame;
        }
    }
    av_free_packet(input->in_packet);
//    if(av_read_frame(input->v_ifmtCtx, input->in_packet)>=0){
//        if(input->in_packet->stream_index==input->videoindex){
//            ret = avcodec_decode_video2(input->pCodecCtx, input->pFrame, &got_picture, input->in_packet);
//            *got_pic=got_picture;
//
//            if(ret < 0){
//                printf("Decode Error.\n");
//                av_free_packet(input->in_packet);
//                return NULL;
//            }
//            if(got_picture){
//                sws_scale(input->img_convert_ctx, (const unsigned char* const*)input->pFrame->data, input->pFrame->linesize, 0, input->pCodecCtx->height, ost->frame->data,  ost->frame->linesize);
//                ost->frame->pts =ost->next_pts++;
//                ret_frame= ost->frame;
//            }
//        }
//        av_free_packet(input->in_packet);
//    }else{
//        return NULL;
//    }
    return ret_frame;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

/**************************************************************/
/* media file output */

void play()
{
    
    int ret;
    int have_video = 0;
    int encode_video = 0;
    
    //********add camera read***********//  //输入相关
    IntputDev video_input = { 0 };  //总输入
    
    avdevice_register_all();
    
    AVCodecContext  *pCodecCtx;
    AVCodec         *pCodec;
    AVDictionary    *options = NULL;
    AVFormatContext *v_ifmtCtx = avformat_alloc_context();
    av_dict_set(&options,"list_devices","true",0);
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "pixel_format", "uyvy422", 0);
    //在macOS上 如果是录制屏幕的状态那么video_size设置是无效的
    //最后生成的图像宽高是屏幕的宽高
    av_dict_set(&options, "video_size", "1920x1080", 0);
    AVInputFormat *ifmt = av_find_input_format("avfoundation");
    
    if(avformat_open_input(&v_ifmtCtx,"1",ifmt,NULL) != 0){
        printf("Couldn't open input stream./dev/video1\n");
        return;
    }
    
    if(avformat_find_stream_info(v_ifmtCtx,NULL)<0){
        printf("Couldn't find stream information.\n");
        return ;
    }
    
    
    int videoindex=-1;
    for(int i=0; i<v_ifmtCtx->nb_streams; i++){//find video stream index
        if(v_ifmtCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            videoindex=i;
            break;
        }
        if(i == v_ifmtCtx->nb_streams-1){
            printf("Couldn't find a video stream.\n");
            return ;
        }
    }
    
    pCodecCtx=v_ifmtCtx->streams[videoindex]->codec;
    printf("pCodecCtx->width=%d pCodecCtx->height=%d \n",pCodecCtx->width, pCodecCtx->height);
    
    if ((pCodec=avcodec_find_decoder(pCodecCtx->codec_id)) == NULL){
        printf("Codec not found.\n");
        return ;
    }
    
    if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
        printf("Could not open codec.\n");return ;
    }
    
    
    
    AVFrame *pFrame     = av_frame_alloc();
    AVFrame *pFrameYUV     = av_frame_alloc();
    unsigned char *out_buffer=(unsigned char *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
    
    struct SwsContext *img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    AVPacket *in_packet = NULL;
    if ((in_packet = (AVPacket *)av_malloc(sizeof(AVPacket))) == NULL){
        printf("error while av_malloc");
        return ;
    }
    
    
    
    //关联IntputDev video_input和其子成员
    video_input.img_convert_ctx=img_convert_ctx;
    video_input.in_packet=in_packet;
    video_input.pCodecCtx=pCodecCtx;
    video_input.pCodec=pCodec;
    video_input.v_ifmtCtx=v_ifmtCtx;      //AVFormatContext *
    video_input.videoindex=videoindex;
    video_input.pFrame=pFrame;          //packet解码得到
    video_input.pFrameYUV=pFrameYUV;      //pFrame->sws->pFrameYUV
    
    
    
    
    //***********************输出
    OutputStream video_st = { 0 }; //即ost
    const char *filename = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/videoout.flv";
    
    AVFormatContext *oc;
    AVCodec *video_codec;//输出侧的编码器   而341行的AVCodec         *pCodec;  为输入侧的解码器
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {printf("Could not deduce output format from file extension: using MPEG.\n");avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);  return;}
    
    AVOutputFormat *fmt = oc->oformat;  //AVFormatContext-> AVOutputFormat *oformat
    /* Add the audio and video streams using the default format codecs and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);  //OutputStream *ost, AVFormatContext *oc,  AVCodec **codec,  enum AVCodecID codec_id
        /* Add an output stream(ost->st) && add an AVCodecContext(即ost->enc)并填充 */
        //1.*codec = avcodec_find_encoder(codec_id);   2.ost->st = avformat_new_stream(oc, NULL);
        //3.ost->enc = c = avcodec_alloc_context3(*codec); 4.填充AVCodecContext *c;
        have_video = 1;
        encode_video = 1;
    }
    
    
    /* Now that all the parameters are set, we can open the audio and  video codecs and allocate the necessary encode buffers. */
    if (have_video)  open_video(oc, video_codec, &video_st, NULL);
    //1.avcodec_open2(c, codec, &opt);    2.allocate and init a re-usable frame 3.prepar ost->tmp_frame 4.avcodec_parameters_from_context
    
    av_dump_format(oc, 0, filename, 1);
    
    /* open the output file, if needed                     creat AVIOcontext*/
    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&oc->pb, filename, AVIO_FLAG_WRITE) < 0){
            fprintf(stderr, "Could not open '%s': %s\n", filename,  av_err2str(ret));
            return ;
        }
    }
    
    /* Write the stream header, if any. */
    if (avformat_write_header(oc, NULL) < 0){
        fprintf(stderr, "Error occurred when opening output file: %s\n",  av_err2str(ret));
        return ;
    }
    
    int got_pic;
    while (encode_video ) {
        /* select the stream to encode */
        AVFrame *frame=get_video_frame1(&video_st,&video_input,&got_pic);  //frame.pts源自pkt.pts 又源自0不停地+1 即帧序号
        //0.av_compare_tsc超时结束        1.av_frame_make_writable        2.av_read_frame--avcodec_decode_video2---sws_scale->->->得到解码后的->frame
        if(!got_pic)
        {
            usleep(10000);  //单位是微秒（百万分之一秒）。
            continue;
        }
        encode_video = !write_video_frame1(oc, &video_st,frame);  //编码预定时间完成后则返回1→while循环可以结束
    }
    
    
    
    av_write_trailer(oc);
    
    
    //end
    sws_freeContext(video_input.img_convert_ctx);
    
    avcodec_close(video_input.pCodecCtx);
    av_free(video_input.pFrameYUV);
    av_free(video_input.pFrame);
    avformat_close_input(&video_input.v_ifmtCtx);
    
    
    /* Close each codec. */
    if (have_video)  close_stream(oc, &video_st);
    
    if (!(fmt->flags & AVFMT_NOFILE))  avio_closep(&oc->pb);  /* Close the output file. */
    
    /* free the stream */
    avformat_free_context(oc);
    
}
