
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


enum KNTState: u16
{
    KNTS_INVALID = 0,
    KNTS_INIT,
    KNTS_BINDED,
    KNTS_CONNECTED,
    KNTS_HANDSHAKE_PB,
    KNTS_HANDSHAKE_CH,
    KNTS_HANDSHAKE_SH,
    KNTS_ESTABLISHED,
    KNTS_RST,
    KNTS_LINGER,
};

enum KNTFlag : u16
{
    KNTF_NONE = 0,
    KNTF_SERVER = 0x1,
    KNTF_CLINET =0x2,
};

class KNetSocket;
FNLog::LogStream& operator <<(FNLog::LogStream&, const KNetSocket&);

class KNetSocket
{
public:
    friend class KNetSession;
    friend class KNetController;
    friend class KNetSelect;
    KNetSocket(s32 inst_id);
    ~KNetSocket();
    
    s32 init(const char* localhost, u16 localport, const char* remote_ip, u16 remote_port);

    s32 send_pkt(const char* pkg_data, s32 len, KNetAddress& remote);

    s32 recv_pkt(char* buf, s32& len, KNetAddress& remote, s64 now_ms);


    s32 destroy();


public:
    void reset()
    {
        skt_ = INVALID_SOCKET;
        slot_id_ = 0;
        state_ = KNTS_INVALID;
        flag_ = KNTF_NONE;
        refs_ = 0;
        client_session_inst_id_ = -1;
        last_active_ = KNetEnv::now_ms(); 
        reset_probe();
    }
    bool is_server() const { return flag_ & KNTF_SERVER; }

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



#endif   
