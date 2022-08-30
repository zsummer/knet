
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
#ifndef _KNET_SELECT_H_
#define _KNET_SELECT_H_
#include "knet_base.h"
#include <chrono>
#include "knet_env.h"

#ifndef KNET_MAX_SOCKETS
#define KNET_MAX_SOCKETS 10
#endif // KNET_MAX_SOCKETS

static_assert(KNET_MAX_SOCKETS >= 10, "");
static_assert(KNET_MAX_SOCKETS < 1024, "");

class KNetSocket;
using KNetSockets = zarray<KNetSocket, KNET_MAX_SOCKETS>;



class KNetSelect
{
public:
    virtual void on_readable(KNetSocket&, s64 now_ms) = 0;
    s32 do_select(KNetSockets& sets, s64 wait_ms);
};





#endif



