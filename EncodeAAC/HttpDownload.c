//
//  HttpDownload.c
//  EncodeAAC
//
//  Created by ZQB on 2020/10/23.
//  Copyright © 2020年 lichao. All rights reserved.
//

#include "HttpDownload.h"
#include <libavutil/avassert.h>
   #include <libavutil/channel_layout.h>
   #include <libavutil/opt.h>
   #include <libavutil/mathematics.h>
    #include <libavutil/timestamp.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>

    #define STREAM_DURATION   10.0
    #define STREAM_FRAME_RATE 25 /* 25 images/s */
    #define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

    #define SCALE_FLAGS SWS_BICUBIC

    // a wrapper around a single output AVStream
    typedef struct OutputStream {
        AVStream *st;
        AVCodecContext *enc;

        /* pts of the next frame that will be generated */
        int64_t next_pts;
        int samples_count;

        AVFrame *frame;
        AVFrame *tmp_frame;

        float t, tincr, tincr2;

        struct SwsContext *sws_ctx;
        struct SwrContext *swr_ctx;
    } OutputStream;


typedef struct IntputDev {

        AVCodecContext  *pCodecCtx;
        AVCodec         *pCodec;
        AVFormatContext *v_ifmtCtx;
        int  videoindex;
        struct SwsContext *img_convert_ctx;
        AVPacket *in_packet;
        AVFrame *pFrame,*pFrameYUV;
    }IntputDev;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
        AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

        printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
                                    av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
                                    av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
                                    av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
                                    pkt->stream_index);
    }

    static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
    {
        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(pkt, *time_base, st->time_base);
        pkt->stream_index = st->index;

        /* Write the compressed frame to the media file. */
        log_packet(fmt_ctx, pkt);
        return av_interleaved_write_frame(fmt_ctx, pkt);
    }

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
                                                 AVCodec **codec,
                                                 enum AVCodecID codec_id)
{
        AVCodecContext *c;
        int i;

        /* find the encoder */
        *codec = avcodec_find_encoder(codec_id);
        if (!(*codec)) {
             fprintf(stderr, "Could not find encoder for '%s'\n",
                                            avcodec_get_name(codec_id));
                exit(1);
            }
    
        ost->st = avformat_new_stream(oc, NULL);
        if (!ost->st) {
                fprintf(stderr, "Could not allocate stream\n");
                exit(1);
            }
        ost->st->id = oc->nb_streams-1;
        c = avcodec_alloc_context3(*codec);
        if (!c) {
                fprintf(stderr, "Could not alloc an encoding context\n");
                exit(1);
            }
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
                        c->width    = 640;
                        c->height   = 480;
                        /* timebase: This is the fundamental unit of time (in seconds) in terms
                         136.             * of which frame timestamps are represented. For fixed-fps content,
                         137.             * timebase should be 1/framerate and timestamp increments should be
                         138.             * identical to 1. */
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
                                     150.                 * This does not happen with normal video, it just happens here as
                                     151.                 * the motion of the chroma plane does not match the luma plane. */
                                c->mb_decision = 2;
                            }
                    break;
            
                    default:
                        break;
                }
    
            /* Some formats want stream headers to be separate. */
            if (oc->oformat->flags & AVFMT_GLOBALHEADER)
                    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

    /**************************************************************/


    /**************************************************************/
    /* video output */

    static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
    {
        AVFrame *picture;
        int ret;

        picture = av_frame_alloc();
        if (!picture)
            return NULL;

        picture->format = pix_fmt;
        picture->width  = width;
        picture->height = height;

        /* allocate the buffers for the frame data */
        ret = av_frame_get_buffer(picture, 32);
        if (ret < 0) {
            fprintf(stderr, "Could not allocate frame data.\n");
            exit(1);
        }

        return picture;
    }

static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
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
        if (!ost->frame) {
                fprintf(stderr, "Could not allocate video frame\n");
                exit(1);
            }
    
    
            printf("ost->frame alloc success fmt=%d w=%d h=%d\n",c->pix_fmt,c->width, c->height);
    
    
            /* If the output format is not YUV420P, then a temporary YUV420P
             222.         * picture is needed too. It is then converted to the required
             223.         * output format. */
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




    /*
     245.     * encode one video frame and send it to the muxer
     246.     * return 1 when encoding is finished, 0 otherwise
     247.     */
    static int write_video_frame1(AVFormatContext *oc, OutputStream *ost,AVFrame *frame)
    {
int ret;
AVCodecContext *c;
int got_packet = 0;
AVPacket pkt = { 0 };
    if(frame==NULL)
            return 1;
    c = ost->enc;
    av_init_packet(&pkt);
    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
            fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
            exit(1);
        }
    printf("--------------video- pkt.pts=%s\n",av_ts2str(pkt.pts));
        printf("----st.num=%d st.den=%d codec.num=%d codec.den=%d---------\n",ost->st->time_base.num,ost->st->time_base.den,
                                           c->time_base.num,c->time_base.den);
    
    
            if (got_packet) {
                    ret = write_frame(oc, &c->time_base, ost->st, &pkt);
                } else {
                        ret = 0;
                    }
    
            if (ret < 0) {
                    fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
                    exit(1);
                }
    
            return (frame || got_packet) ? 0 : 1;
        }


    static AVFrame *get_video_frame1(OutputStream *ost,IntputDev* input,int *got_pic)
    {
            int ret, got_picture;
            AVCodecContext *c = ost->enc;
        AVFrame * ret_frame=NULL;
        if (av_compare_ts(ost->next_pts, c->time_base,
                                                  STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
                return NULL;
            /* when we pass a frame to the encoder, it may keep a reference to it
        305.         * internally; make sure we do not overwrite it here */
        if (av_frame_make_writable(ost->frame) < 0)
                exit(1);
    
    
            if(av_read_frame(input->v_ifmtCtx, input->in_packet)>=0){
                if(input->in_packet->stream_index==input->videoindex){
                        ret = avcodec_decode_video2(input->pCodecCtx, input->pFrame, &got_picture, input->in_packet);
                                *got_pic=got_picture;
                
                            if(ret < 0){
                                    printf("Decode Error.\n");
                                    av_free_packet(input->in_packet);
                                    return NULL;
                                }
                            if(got_picture){
                                    //sws_scale(input->img_convert_ctx, (const unsigned char* const*)input->pFrame->data, input->pFrame->linesize, 0, input->pCodecCtx->height, ost->frame->data, ost->frame->linesize);
                                    sws_scale(input->img_convert_ctx, (const unsigned char* const*)input->pFrame->data, input->pFrame->linesize, 0, input->pCodecCtx->height, ost->frame->data,  ost->frame->linesize);
                                    ost->frame->pts =ost->next_pts++;
                                    ret_frame= ost->frame;
                
                                }
                        }
                av_free_packet(input->in_packet);
                    }
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

int mainStart()
{
        OutputStream video_st = { 0 }, audio_st = { 0 };
        const char *filename;
        AVOutputFormat *fmt;
        AVFormatContext *oc;
        AVCodec *audio_codec, *video_codec;
        int ret;
        int have_video = 0, have_audio = 0;
        int encode_video = 0, encode_audio = 0;
        AVDictionary *opt = NULL;
        int i;


    filename = "/Users/zqb/Desktop/算法/1.课件/av_base/EncodeAAC的副本/main.mp4";//argv[1];
//            for (i = 2; i+1 < argc; i+=2) {
//                    if (!strcmp(argv[i], "-flags") || !strcmp(argv[i], "-fflags"))
//                            av_dict_set(&opt, argv[i]+1, argv[i+1], 0);
//                }
            /* allocate the output media context */
        avformat_alloc_output_context2(&oc, NULL, NULL, filename);
        if (!oc) {
                printf("Could not deduce output format from file extension: using MPEG.\n");
                avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
            }
        if (!oc)
                return 1;
                //********add camera read***********//
        IntputDev video_input = { 0 };
        AVCodecContext  *pCodecCtx;
        AVCodec         *pCodec;
        AVFormatContext *v_ifmtCtx;
        //Register Device
        avdevice_register_all();
            v_ifmtCtx = avformat_alloc_context();
                //Linux
        AVInputFormat *ifmt=av_find_input_format("avfoundation");
        if(avformat_open_input(&v_ifmtCtx,"1",ifmt,NULL)!=0){
                printf("Couldn't open input stream./dev/video0\n");
                return -1;
            }
                if(avformat_find_stream_info(v_ifmtCtx,NULL)<0)
            {
                printf("Couldn't find stream information.\n");
                return -1;
            }
            int videoindex=-1;
        for(i=0; i<v_ifmtCtx->nb_streams; i++)
            if(v_ifmtCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
            {
                    videoindex=i;
                    break;
                }
        if(videoindex==-1)
            {
                printf("Couldn't find a video stream.\n");
                return -1;
            }
                pCodecCtx=v_ifmtCtx->streams[videoindex]->codec;
            pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
            if(pCodec==NULL)
                {
                        printf("Codec not found.\n");
                        return -1;
                    }
            if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
                {
                        printf("Could not open codec.\n");
                        return -1;
                    }
    
            AVFrame *pFrame,*pFrameYUV;
            pFrame=av_frame_alloc();
            pFrameYUV=av_frame_alloc();
            unsigned char *out_buffer=(unsigned char *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
            avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
    
            printf("camera width=%d height=%d \n",pCodecCtx->width, pCodecCtx->height);
    
    
            struct SwsContext *img_convert_ctx;
            img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
            AVPacket *in_packet=(AVPacket *)av_malloc(sizeof(AVPacket));
    
    
            video_input.img_convert_ctx=img_convert_ctx;
            video_input.in_packet=in_packet;
    
            video_input.pCodecCtx=pCodecCtx;
            video_input.pCodec=pCodec;
               video_input.v_ifmtCtx=v_ifmtCtx;
            video_input.videoindex=videoindex;
            video_input.pFrame=pFrame;
            video_input.pFrameYUV=pFrameYUV;
    
        //******************************//
    
            fmt = oc->oformat;
    
            /* Add the audio and video streams using the default format codecs
             466.         * and initialize the codecs. */
    
                printf( "fmt->video_codec = %d\n", fmt->video_codec);
    
            if (fmt->video_codec != AV_CODEC_ID_NONE) {
                add_stream(&video_st, oc, &video_codec, fmt->video_codec);
                have_video = 1;
                encode_video = 1;
            }
    
            /* Now that all the parameters are set, we can open the audio and
             477.         * video codecs and allocate the necessary encode buffers. */
            if (have_video)
                open_video(oc, video_codec, &video_st, opt);
    
    
                av_dump_format(oc, 0, filename, 1);
    
                /* open the output file, if needed */
                if (!(fmt->flags & AVFMT_NOFILE)) {
                        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
                        if (ret < 0) {
                                fprintf(stderr, "Could not open '%s': %s\n", filename,
                                                                    av_err2str(ret));
                                return 1;
                            }
                    }
    
            /* Write the stream header, if any. */
            ret = avformat_write_header(oc, &opt);
            if (ret < 0) {
                    fprintf(stderr, "Error occurred when opening output file: %s\n",
                                                    av_err2str(ret));
                    return 1;
                }
    
            int got_pic;
    
            while (encode_video ) {
                    /* select the stream to encode */
                        //encode_video = !write_video_frame(oc, &video_st)
                    AVFrame *frame=get_video_frame1(&video_st,&video_input,&got_pic);
                    if(!got_pic)
                        {
                                usleep(10000);
                                continue;
                
                            }
                    encode_video = !write_video_frame1(oc, &video_st,frame);
                }
    
    
            /* Write the trailer, if any. The trailer must be written before you
             519.         * close the CodecContexts open when you wrote the header; otherwise
             520.         * av_write_trailer() may try to use memory that was freed on
             521.         * av_codec_close(). */
            av_write_trailer(oc);
    
            sws_freeContext(video_input.img_convert_ctx);
    
    
            avcodec_close(video_input.pCodecCtx);
            av_free(video_input.pFrameYUV);
            av_free(video_input.pFrame);
            avformat_close_input(&video_input.v_ifmtCtx);
    
    
            /* Close each codec. */
            if (have_video)
                close_stream(oc, &video_st);
    
            if (!(fmt->flags & AVFMT_NOFILE))
                    /* Close the output file. */
                    avio_closep(&oc->pb);
    
                /* free the stream */
                avformat_free_context(oc);
    
                return 0;
        }

