
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




class KNetSocket
{
public:
    friend class KNetSession;
    friend class KNetTurbo;
    friend class KNetSelect;
    KNetSocket(s32 inst_id);
    ~KNetSocket();
    s32 init(const char* localhost, u16 localport, const char* remote_ip, u16 remote_port);
    s32 send_packet(const char* pkg_data, s32 len, KNetAddress& remote);
    s32 recv_packet(char* buf, s32& len, KNetAddress& remote, s64 now_ms);
    s32 destroy();

public:
    s32 set_skt_recv_buffer(s32 size);
    s32 set_skt_send_buffer(s32 size);

public:
    s32 inst_id()const { return inst_id_; }
    SOCKET skt() const { return skt_; }
    KNetAddress& local() { return local_; }
    const KNetAddress& local() const { return local_; }
    KNetAddress& remote() { return remote_; }
    const KNetAddress& remote() const { return remote_; }

private:
    s32 inst_id_;
    SOCKET skt_;
    KNetAddress local_;
    KNetAddress remote_;
    u64 probe_snd_cnt_;
    u64 probe_rcv_cnt_;
    u64 probe_snd_bytes_;
    u64 probe_rcv_bytes_;
    s64 last_send_ts_;
    s64 last_recv_ts_;
    u32 debug_send_lost_;
    u32 debug_recv_lost_;
    //up param
public:
    u16 state_;
    u16 flag_;
    u8  slot_id_;
    s32 refs_;
    s32 client_session_inst_id_;
    u64 salt_id_;

    u64 probe_seq_id_;
    s64 probe_last_ping_;
    s64 probe_avg_ping_;

    u64 probe_shake_id_;
    u64 user_data_;

    s32 max_recv_loop_count_;

};


inline FNLog::LogStream& operator <<(FNLog::LogStream& ls, const KNetSocket& skt)
{
    if (skt.client_session_inst_id_ != -1)
    {
        return ls << "[skt inst:" << skt.inst_id() << ", skt:" << skt.skt() << ", state:" << skt.state_ << ", flag:" << skt.flag_ <<", slot:" << skt.slot_id_ << ", session inst id:" << skt.client_session_inst_id_ << "]";
    }
    return ls << "[skt inst:" << skt.inst_id() << ", skt:" << skt.skt() << ", state:" << skt.state_ << ", flag:" << skt.flag_ << ", slot:" << skt.slot_id_ << "]";
}


#endif   
