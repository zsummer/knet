
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
#ifndef _KNET_HELPER_H_
#define _KNET_HELPER_H_
#include "knet_base.h"
#include <chrono>
#include "knet_env.h"

static inline bool ikcp_check_str(char* src, s32 src_len)
{
    for (s32 i = 0; i < src_len; i++)
    {
        if (src[i] == '\0')
        {
            return true;
        }
    }
    return false;
}



struct KNetDeviceInfo
{
    static const u32 PKT_SIZE = 128 * 6;
   char device_name[128];
   char device_type[128];
   char device_mac[128];
   char sys_name[128];
   char sys_version[128];
   char os_name[128];
   bool is_valid()
   {
       return ikcp_check_str(device_name, 128) 
           && ikcp_check_str(device_type, 128) 
           && ikcp_check_str(device_mac, 128) 
           && ikcp_check_str(sys_name, 128) 
           && ikcp_check_str(sys_version, 128) 
           && ikcp_check_str(os_name, 128);
   }
};
static_assert(sizeof(KNetDeviceInfo) == KNetDeviceInfo::PKT_SIZE, "");


struct KNetHandshakeKey
{
    static const u32 PKT_SIZE = 8 + 4 + 4 + 8;
    
    s64 system_time;
    u32 sequence_code;
    u32 rand_code;
    u64 mac_code;
    struct Hash
    {
        u64 operator()(const KNetHandshakeKey& key) const
        {
            u64 hash_key = (u64)key.system_time << 16;
            hash_key |= key.rand_code & 0xffff;
            static const u64 h = (0x84222325ULL << 32) | 0xcbf29ce4ULL;
            static const u64 kPrime = (0x00000100ULL << 32) | 0x000001b3ULL;
            hash_key ^= h;
            hash_key *= kPrime;
            return hash_key;
        }
    };
};

static_assert(sizeof(KNetHandshakeKey) == KNetHandshakeKey::PKT_SIZE, "");



inline bool operator   <(const KNetHandshakeKey& key1, const KNetHandshakeKey& key2)
{
    if (key1.system_time < key2.system_time
        || key1.sequence_code < key2.sequence_code
        || key1.mac_code < key2.mac_code
        || key1.rand_code < key2.rand_code)
    {
        return true;
    }
    return false;
}

inline bool operator   ==(const KNetHandshakeKey& key1, const KNetHandshakeKey& key2)
{
    if (key1.system_time == key2.system_time
        && key1.sequence_code == key2.sequence_code
        && key1.mac_code == key2.mac_code
        && key1.rand_code == key2.rand_code)
    {
        return true;
    }
    return false;
}





class KNetHelper
{
public:
    static KNetHandshakeKey CreateKey()
    {
        KNetDeviceInfo kdi;
        memset(&kdi, 0, sizeof(kdi));
        return CreateKey(kdi);
    }
    static KNetHandshakeKey CreateKey(const KNetDeviceInfo& kdi)
    {
        KNetHandshakeKey key;
        key.system_time = KNetEnv::Now();
        key.mac_code = (u32)(kdi.device_name[0] + kdi.device_mac[0] + kdi.device_type[0] + kdi.sys_name[0] + kdi.sys_version[0] + kdi.os_name[0]);
        key.rand_code = (u32)rand();
        key.sequence_code = KNetEnv::CreateSequenceID();
        return key;
    }

    static bool CheckKey(const KNetHandshakeKey& key)
    {
        s64 diff_ms = llabs(key.system_time - KNetEnv::Now());
        if (diff_ms > 5*60*1000)
        {
            return false;
        }
        if (key.sequence_code == 0)
        {
            return false;
        }
        return true;
    }
};




#endif



