
/*
* knet License
* Copyright (C) 2019 YaweiZhang <yawei.zhang@foxmail.com>.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once
#ifndef _KNET_SOCKET_H_
#define _KNET_SOCKET_H_

#include "knet_base.h"
#include <chrono>

#include "knet_addr.h"
#include "knet_env.h"


enum KNTS_STATE: u32
{
    KNTS_INVALID = 0,
    KNTS_LOCAL_INITED,
    KNTS_BINDED,
    KNTS_CONNECTED,
    KNTS_HANDSHAKE_CH,
    KNTS_HANDSHAKE_SH,
    KNTS_ESTABLISHED,
    KNTS_LINGER,
};

class KNetSocket
{
public:
    KNetSocket()
    {
        socket_ = INVALID_SOCKET;
        state_ = KNTS_INVALID;
    }
    ~KNetSocket()
    {

    }
    
    s32 InitLocal()



private:
    KNTS_STATE state_;
    SOCKET socket_;
    KNetAddress local_;
    KNetAddress remote_;
};




#endif   
