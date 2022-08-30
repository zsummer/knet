
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
    KNTS_HANDSHAKE_PB,
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
    KNetSocket(s32 inst_id)
    {
        KNetEnv::prof(KNT_STT_SKT_INSTRUCT_EVENTS)++;
        inst_id_ = inst_id;
        reset();
        LogDebug() <<  *this;
    }
    ~KNetSocket()
    {
        KNetEnv::prof(KNT_STT_SKT_DESTRUCT_EVENTS)++;
        if (state_ != KNTS_INVALID || skt_ != INVALID_SOCKET)
        {
            KNetEnv::error_count()++;
        }
        LogDebug() << *this;
    }
    
    s32 init(const char* localhost, u16 localport, const char* remote_ip, u16 remote_port)
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

        last_active_ = KNetEnv::now_ms();
        skt_ = socket(local_.family(), SOCK_DGRAM, 0);
        if (skt_ == INVALID_SOCKET)
        {
            return -5;
        }
        KNetEnv::prof(KNT_STT_SKT_INIT_EVENTS)++;
        state_ = KNTS_LOCAL_INITED;
        ret = bind(skt_, local_.sockaddr_ptr(), local_.sockaddr_len());
        if (ret != 0)
        {
            destroy();
            return -6;
        }
        
        local_.reset_from_socket(skt_);
        state_ = KNTS_BINDED;
        KNetEnv::prof(KNT_STT_SKT_BIND_EVENTS)++;
        LogInfo() << "bind local:" << local_.debug_string();
        LogInfo() << *this;
        return 0;
    }



    s32 send_pkt(const char* pkg_data,  s32 len, KNetAddress& remote)
    {
        KNetEnv::prof(KNT_STT_SKT_SND_EVENTS)++;
        KNetEnv::prof(KNT_STT_SKT_SND_BYTES)+= len;
        s32 ret = sendto(skt_, pkg_data, len, 0, remote.sockaddr_ptr(), remote.sockaddr_len());
        if (ret == SOCKET_ERROR)
        {
            return -1;
        }
        return 0;
    }

    s32 recv_pkt(char* buf,  s32& len, KNetAddress& remote, s64 now_ms)
    {
        int addr_len = sizeof(remote.real_addr_.in6);
        int ret = recvfrom(skt_, buf, len, 0, (sockaddr*)&remote.real_addr_.in6, &addr_len);
        if (ret <= 0)
        {
            len = 0;
            LogError() << "error:" << KNetEnv::error_code();
            return -1;
        }
        len = ret;
        KNetEnv::prof(KNT_STT_SKT_RCV_EVENTS)++;
        KNetEnv::prof(KNT_STT_SKT_RCV_BYTES)+= ret;
        last_active_ = now_ms;
        return 0;
    }


    s32 destroy()
    {
        if (state_ == KNTS_INVALID)
        {
            KNetEnv::error_count()++;
            return -1;
        }

        if (refs_ > 0 && !(flag_ & KNTS_SERVER))
        {
            KNetEnv::error_count()++;
            return -2;
        }

        if (refs_ > 1 && (flag_ & KNTS_SERVER))
        {
            KNetEnv::error_count()++;
            return -3;
        }

        if (refs_  < 0)
        {
            KNetEnv::error_count()++;
            return -4;
        }
        KNetEnv::prof(KNT_STT_SKT_DESTROY_EVENTS)++;
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
        last_active_ = KNetEnv::now_ms();
        return 0;
    }

    


public:
    void reset()
    {
        skt_ = INVALID_SOCKET;
        slot_id_ = 0;
        state_ = KNTS_INVALID;
        flag_ = KNTS_NONE;
        refs_ = 0;
        client_session_inst_id_ = -1;
        last_active_ = KNetEnv::now_ms(); 
        reset_probe();
    }
    bool is_server() const { return flag_ & KNTS_SERVER; }

    s32 inst_id_;
    u8  slot_id_;
    s32 refs_;
    s32 client_session_inst_id_;
    u16 state_;
    u16 flag_;
    SOCKET skt_;
    KNetAddress local_;
    KNetAddress remote_;
 public:
     s64 last_active_;

public:
    void reset_probe()
    {
        probe_seq_id_ = 0;
        probe_last_ping_ = 0;
        probe_avg_ping_ = 0;
        probe_snd_cnt_ = 0;
        probe_rcv_cnt_ = 0;
        probe_shake_id_ = 0;
    }

public:
    u64 probe_seq_id_;
    s64 probe_last_ping_;
    s64 probe_avg_ping_;
    u32 probe_snd_cnt_;
    u32 probe_rcv_cnt_;
    u64 probe_shake_id_;
};


inline FNLog::LogStream& operator <<(FNLog::LogStream& ls, const KNetSocket& s)
{
    if (s.flag_ & KNTS_SERVER)
    {
        ls << "server: inst_id:" << s.inst_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)s.flag_ <<", refs:" << s.refs_ << ", local:" << s.local_.debug_string() ;
    }
    else if (s.flag_ & KNTS_CLINET)
    {
        ls << "client: inst_id:" << s.inst_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)s.flag_ << ", refs:" << s.refs_ << ", local:" << s.local_.debug_string() << ", remote:" << s.remote_.debug_string();
    }
    else
    {
        ls << "none: inst_id:" << s.inst_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)s.flag_ << ", refs:" << s.refs_ << ", local:" << s.local_.debug_string() << ", remote:" << s.remote_.debug_string();
    }
    return ls;
}


#endif   
