
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
#include "knet_rudp.h"

struct KNetConfig
{
	std::string localhost;
	u16 localport;

	std::string remote_ip;
	u16 remote_port;


};


static const u32 MAX_SESSION_CONFIGS = KNT_MAX_SLOTS;
using KNetConfigs = zarray<KNetConfig, MAX_SESSION_CONFIGS>;


struct KNetSocketSlot
{
	KNetSocketSlot()
	{
		inst_id_ = -1;
	}
	s32 inst_id_;
	KNetAddress remote_;
	s64 last_active_;
};

class KNetSession
{
public:
	KNetSession(s32 inst_id);
	~KNetSession();
	s32 init();
	s32 destroy();
public:
	s32 inst_id_;
	u16 state_;
	u64 session_id_;
	u64 shake_id_;
	u64 snd_pkt_id_;

	std::string encrypt_key;
	KNetConfigs configs_;
	char sg_[16];
	char sp_[16];
	char box_[16];
	std::array<KNetSocketSlot, KNT_MAX_SLOTS> slots_;
};







#endif



