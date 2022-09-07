
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

#include "zprof.h"
#include "knet_env.h"

#define KNetProf ProfRecord<1987, 1, KNTP_MAX>
#define KNetProfInst KNetProf::instance()




s32& KNetEnv::error_count()
{
    static s32 global_errors_ = 0;
    return global_errors_;
}


void KNetEnv::call_user(u32 idx)
{
    KNetProfInst.call_user(idx + KNetProfInst.node_declare_begin_id(), 1, 1);
}
s64 KNetEnv::user_count(u32 idx)
{
    return KNetProfInst.node(idx + KNetProfInst.node_declare_begin_id()).user.c;
}

void KNetEnv::call_mem(u32 idx, s32 bytes)
{
    KNetProfInst.call_mem(idx + KNetProfInst.node_declare_begin_id(), 1, bytes);
}
s64 KNetEnv::mem_count(u32 idx)
{
    return KNetProfInst.node(idx + KNetProfInst.node_declare_begin_id()).mem.c;
}
s64 KNetEnv::mem_bytes(u32 idx)
{
    return KNetProfInst.node(idx + KNetProfInst.node_declare_begin_id()).mem.sum;
}

void KNetEnv::clean_prof()
{
    KNetProfInst.clean_declare_info(false);
}
void KNetEnv::serialize()
{
    auto call_log = [](const ProfSerializeBuffer& buffer)
    {
        LOG_STREAM_DEFAULT_LOGGER(0, FNLog::PRIORITY_INFO, 0, 0, FNLog::LOG_PREFIX_NULL).write_buffer(buffer.buff(), buffer.offset());
    };
    KNetProfInst.serialize(PROF_SER_DELCARE, call_log);
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


#define KNET_REG_NODE(id)  KNetProfInst.regist_node(id + KNetProfInst.node_declare_begin_id(), #id, PROF_COUNTER_DEFAULT,  false, false)

s32 KNetEnv::init_env()
{
#ifdef _WIN32
    WSADATA wsa_data;
    s32 ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (ret != NO_ERROR)
    {
        return -1;
    }
#endif
    KNetProfInst.init_prof("knet prof");
    KNET_REG_NODE(KNTP_NONE);


    KNET_REG_NODE(KNTP_SKT_ALLOC);
    KNET_REG_NODE(KNTP_SKT_FREE);

    KNET_REG_NODE(KNTP_SKT_INIT);
    KNET_REG_NODE(KNTP_SKT_DESTROY);


    KNET_REG_NODE(KNTP_SKT_SEND);
    KNET_REG_NODE(KNTP_SKT_RECV);

    KNET_REG_NODE(KNTP_SKT_PROBE);
    KNET_REG_NODE(KNTP_SKT_R_PROBE);
    KNET_REG_NODE(KNTP_SKT_ON_PROBE);
    KNET_REG_NODE(KNTP_SKT_PROBE_ACK);
    KNET_REG_NODE(KNTP_SKT_ON_PROBE_ACK);
    KNET_REG_NODE(KNTP_SKT_CH);
    KNET_REG_NODE(KNTP_SKT_R_CH);
    KNET_REG_NODE(KNTP_SKT_ON_CH);
    KNET_REG_NODE(KNTP_SKT_SH);
    KNET_REG_NODE(KNTP_SKT_ON_SH);

    KNET_REG_NODE(KNTP_SES_ALLOC);
    KNET_REG_NODE(KNTP_SES_FREE);

    KNET_REG_NODE(KNTP_SES_INIT);
    KNET_REG_NODE(KNTP_SES_DESTROY);

    KNET_REG_NODE(KNTP_SES_SEND);
    KNET_REG_NODE(KNTP_SES_RECV);

    KNET_REG_NODE(KNTP_SES_ON_ACCEPT);
    KNET_REG_NODE(KNTP_SES_ON_CONNECTED);
    KNET_REG_NODE(KNTP_SES_CONNECT);

    KNET_REG_NODE(KNTP_CTL_START_COUNT);
    KNET_REG_NODE(KNTP_CTL_DESTROY_COUNT);

    return 0;
}

void KNetEnv::clean_env()
{
#ifdef _WIN32
    WSACleanup();
#endif
}