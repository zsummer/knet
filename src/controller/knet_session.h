
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
#ifndef _KNET_SESSION_H_
#define _KNET_SESSION_H_
#include "knet_base.h"
#include <chrono>
#include "knet_env.h"
#include "knet_select.h"
#include "knet_socket.h"
#include "knet_proto.h"

struct KNetConfig
{
	std::string localhost;
	u16 localport;

	std::string remote_ip;
	u16 remote_port;


};


static const s32 MAX_SESSION_CONFIGS = KNT_MAX_SLOTS;
using KNetConfigs = zarray<KNetConfig, MAX_SESSION_CONFIGS>;


struct KNetSocketSlot
{
	s32 inst_id_;
	KNetAddress remote_;
};

class KNetController;


class KNetSession
{
public:
	KNetSession(s32 inst_id);
	~KNetSession();
	s32 reset();
	s32 init(KNetController& c, u16 flag = KNTF_SERVER);
	s32 destory();

public:
	bool is_server() {return flag_ & KNTF_SERVER;}
public:
	s32 inst_id_;
	u16 state_;
	u16 flag_;
	u64 session_id_;
	u64 shake_id_;
	u64 snd_pkt_id_;


	ikcpcb* kcp_;
	std::string encrypt_key;
	KNetConfigs configs_;
	char sg_[16];
	char sp_[16];
	char box_[16];
	std::array<KNetSocketSlot, KNT_MAX_SLOTS> slots_;

	s64 connect_time_;
	s64 connect_expire_time_;
	s64 connect_resends_;
	KNetOnConnect on_connected_;
public:
	s64 last_send_ts_;
	s64 last_recv_ts_;
};

static const s32 KNT_SESSION_SIZE = sizeof(KNetSession);

inline FNLog::LogStream& operator <<(FNLog::LogStream& ls, const KNetSession& session)
{
	return ls << "[session inst:" << session.inst_id_ << ", session id:" << session.session_id_ << ", state:" << session.state_ <<", flag:" << session.flag_;
}






#endif



