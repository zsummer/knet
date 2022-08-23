
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


struct KNetDeviceInfo
{
    std::string device_name;
    std::string device_type;
    std::string device_mac;
    std::string sys_name;
    std::string sys_version;
    std::string os_name;


};

struct KNetHandshakeKey
{
    s64 system_time;
    u32 sequence_code;
    u32 mac_code;
    u32 rand_code;
    u32 mac_key;

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
        return CreateKey(kdi);
    }
    static KNetHandshakeKey CreateKey(const KNetDeviceInfo& kdi)
    {
        KNetHandshakeKey key;
        key.system_time = KNetEnv::Now();
        key.mac_code = (u32)(kdi.device_name.length() + kdi.device_mac.length() + kdi.device_type.length() + kdi.sys_name.length() + kdi.sys_version.length() + kdi.os_name.length());
        key.rand_code = (u32)rand();
        key.sequence_code = KNetEnv::CreateSequenceID();
        u64 mac = (u64)key.system_time + key.mac_code + key.rand_code + key.sequence_code;
        mac &= 0xffffffff;
        key.mac_key = (u32)mac;
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
        u64 mac = (u64)key.system_time + key.mac_code + key.rand_code + key.sequence_code;
        mac &= 0xffffffff;
        if (mac != key.mac_key)
        {
            return false;
        }
        return true;
    }
};




#endif



