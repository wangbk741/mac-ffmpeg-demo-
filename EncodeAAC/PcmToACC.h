//
//  PcmToACC.h
//  EncodeAAC
//
//  Created by ZQB on 2020/10/21.
//  Copyright © 2020年 lichao. All rights reserved.
//

#ifndef PcmToACC_h
#define PcmToACC_h
#include "libavutil/avutil.h"
#include "libavutil/audio_fifo.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavfilter/avfilter.h"
void start(void);
#endif /* PcmToACC_h */
