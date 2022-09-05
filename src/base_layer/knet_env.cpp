
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


#include "knet_env.h"


s32& KNetEnv::error_count()
{
    static s32 global_errors_ = 0;
    return global_errors_;
}

s64 g_knet_status[KNT_STT_MAX] = { 0 };

s64& KNetEnv::count(KNET_STATUS id)
{
    return g_knet_status[id];
}

void KNetEnv::clean_count()
{
    memset(g_knet_status, 0, sizeof(g_knet_status));
}

u64 KNetEnv::create_seq_id()
{
    static u64 seq_id = 0;
    return ++seq_id;
}


u64 KNetEnv::create_session_id()
{
    static u64 session_id = 0;
    return ++session_id;
}


s32 KNetEnv::fill_device_info(KNetDeviceInfo& dvi)
{
    memset(&dvi, 0, sizeof(dvi));
    return 0;
}

