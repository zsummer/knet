
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


using SocketEvent = void(*)(KNetSocket&, s64 now_ms);


s32 KNetSelect(KNetSockets& sets, SocketEvent on_tick, SocketEvent readable, s64 wait_ms);



#endif



