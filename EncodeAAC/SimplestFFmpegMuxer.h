//
//  SimplestFFmpegMuxer.h
//  EncodeAAC
//
//  Created by ZQB on 2020/10/27.
//  Copyright © 2020年 lichao. All rights reserved.
//

#ifndef SimplestFFmpegMuxer_h
#define SimplestFFmpegMuxer_h
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/mathematics.h"

#include <stdio.h>
void startMuxer(void);
#endif /* SimplestFFmpegMuxer_h */
