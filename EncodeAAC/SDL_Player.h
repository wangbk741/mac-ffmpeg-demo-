//
//  SDL_Player.h
//  EncodeAAC
//
//  Created by ZQB on 2020/11/2.
//  Copyright © 2020年 lichao. All rights reserved.
//

#ifndef SDL_Player_h
#define SDL_Player_h
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include <stdio.h>
int init_player(void);
#endif /* SDL_Player_h */
