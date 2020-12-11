//
//  Simplest_aac.h
//  EncodeAAC
//
//  Created by ZQB on 2020/11/18.
//  Copyright © 2020年 lichao. All rights reserved.
//

#ifndef Simplestaac_h
#define Simplestaac_h

#include <stdio.h>
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
int simplest_aac_parser(void);
#endif /* Simplest_aac_h */
