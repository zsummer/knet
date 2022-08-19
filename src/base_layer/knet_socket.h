
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

#include "knet_address.h"
#include "knet_env.h"


enum KNTS_STATE: u16
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

enum KNTS_FLAGS : u16
{
    KNTS_NONE = 0,
    KNTS_SERVER = 0x1,
    KNTS_CLINET =0x2,
};

class KNetSocket;
FNLog::LogStream& operator <<(FNLog::LogStream&, const KNetSocket&);

class KNetSocket
{
public:
    friend class KNetSession;
    friend class KNetController;
    friend class KNetSelect;
    KNetSocket(s32 skt_id)
    {
        KNetEnv::Status(KNT_STT_SKT_INSTRUCT_EVENTS)++;
        skt_ = INVALID_SOCKET;
        state_ = KNTS_INVALID;
        flag_ = KNTS_NONE;
        skt_id_ = skt_id;
        refs_ = 0;
        last_active_ = KNetEnv::Now();
        LogDebug() <<  *this;
    }
    ~KNetSocket()
    {
        KNetEnv::Status(KNT_STT_SKT_DESTRUCT_EVENTS)++;
        if (state_ != KNTS_INVALID || skt_ != INVALID_SOCKET)
        {
            KNetEnv::Errors()++;
        }
        LogDebug() << *this;
        
    }
    
    s32 InitSocket(const char* localhost, u16 localport, const char* remote_ip, u16 remote_port)
    {
        if (state_ != KNTS_INVALID)
        {
            return -2;
        }
        s32 ret = local_.reset(localhost, localport);
        if (ret != 0)
        {
            return -3;
        }
        if (remote_ip != NULL)
        {
            ret = remote_.reset(remote_ip, remote_port);
            if (ret != 0)
            {
                return -4;
            }
        }

        last_active_ = KNetEnv::Now();
        skt_ = socket(local_.family(), SOCK_DGRAM, 0);
        if (skt_ == INVALID_SOCKET)
        {
            return -5;
        }
        KNetEnv::Status(KNT_STT_SKT_INIT_EVENTS)++;
        state_ = KNTS_LOCAL_INITED;
        ret = bind(skt_, local_.sockaddr_ptr(), local_.sockaddr_len());
        if (ret != 0)
        {
            DestroySocket();
            return -6;
        }
        
        local_.reset_from_socket(skt_);
        state_ = KNTS_BINDED;
        KNetEnv::Status(KNT_STT_SKT_BIND_EVENTS)++;
        LogInfo() << "bind local:" << local_.debug_string();
        LogInfo() << *this;
        return 0;
    }

    /* // 
    s32 Connect()
    {
        if (state_ != KNTS_BINDED)
        {
            return -1;
        }
        s32 ret = connect(skt_, remote_.sockaddr_ptr(), remote_.sockaddr_len());
        if (ret != 0)
        {
            return -2;
        }
        state_ = KNTS_CONNECTED;
        return 0;
    }
    */


    s32 SendTo()
    {
        KNetEnv::Status(KNT_STT_SKT_SND_EVENTS)++;
        sendto(skt_, "ssss", 4, 0, remote_.sockaddr_ptr(), remote_.sockaddr_len());
        return 0;
    }



    s32 DestroySocket()
    {
        if (state_ == KNTS_INVALID)
        {
            KNetEnv::Errors()++;
            return -1;
        }

        if (refs_ > 0 && !(flag_ & KNTS_SERVER))
        {
            KNetEnv::Errors()++;
            return -2;
        }

        if (refs_ > 1 && (flag_ & KNTS_SERVER))
        {
            KNetEnv::Errors()++;
            return -3;
        }

        if (refs_  < 0)
        {
            KNetEnv::Errors()++;
            return -4;
        }
        KNetEnv::Status(KNT_STT_SKT_DESTROY_EVENTS)++;
        LogInfo() << *this;

        if (skt_ != INVALID_SOCKET)
        {
#ifdef _WIN32
            closesocket(skt_);
#else
            close(skt_);
#endif
            skt_ = INVALID_SOCKET;
        }


        state_ = KNTS_INVALID;
        flag_ = KNTS_NONE;
        refs_ = 0;
        last_active_ = KNetEnv::Now();
        return 0;
    }

    


public:
    s32 skt_id_;
    s32 refs_;
    u16 state_;
    u16 flag_;
    SOCKET skt_;
    KNetAddress local_;
    KNetAddress remote_;
 public:
     s64 last_active_;
};


inline FNLog::LogStream& operator <<(FNLog::LogStream& ls, const KNetSocket& s)
{
    if (s.flag_ & KNTS_SERVER)
    {
        ls << "server: skt_id:" << s.skt_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)s.flag_ <<", refs:" << s.refs_ << ", local:" << s.local_.debug_string() ;
    }
    else if (s.flag_ & KNTS_CLINET)
    {
        ls << "client: skt_id:" << s.skt_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)s.flag_ << ", refs:" << s.refs_ << ", local:" << s.local_.debug_string() << ", remote:" << s.remote_.debug_string();
    }
    else
    {
        ls << "none: skt_id:" << s.skt_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)s.flag_ << ", refs:" << s.refs_ << ", local:" << s.local_.debug_string() << ", remote:" << s.remote_.debug_string();
    }
    return ls;
}


#endif   
