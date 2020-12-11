//
//  CaptureVideo.h
//  EncodeAAC
//
//  Created by ZQB on 2020/10/23.
//  Copyright © 2020年 lichao. All rights reserved.
//

#ifndef CaptureVideo_h
#define CaptureVideo_h
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"

#include <stdio.h>
void startVideo(void);
//void startAudio(void);
void startRecord(void);
void setStatus(int statu);
#endif /* CaptureVideo_h */
