
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
#ifndef _KNET_CONTROLLER_H_
#define _KNET_CONTROLLER_H_
#include "knet_base.h"
#include <chrono>
#include "knet_env.h"
#include "knet_select.h"
#include "knet_socket.h"
#include "knet_session.h"
#include "zpool.h"



#ifndef KNET_MAX_SESSIONS
#define KNET_MAX_SESSIONS 10
#endif // KNET_MAX_SESSIONS
static_assert(KNET_MAX_SESSIONS >= 10, "");

class KNetSession;
using KNetSessions = zlist<KNetSession, KNET_MAX_SESSIONS>;



class KNetController: public KNetSelect
{
public:

	KNetController();
	~KNetController();
	s32 StartServer(const KNetConfigs& configs);
	s32 StartConnect(std::string uuid, const KNetConfigs& configs);
	s32 RemoveSession(std::string uuid, u64 session_id);

	s32 DoSelect();
	s32 Destroy();
	virtual void OnSocketTick(KNetSocket&, s64 now_ms) override;
	virtual void OnSocketReadable(KNetSocket&, s64 now_ms) override;
private:
	s32 RemoveSession(KNetSession* session);

public:
	KNetSocket* PopFreeSocket();
	void PushFreeSocket(KNetSocket*);
private:
	u32 controller_state_;
	KNetSockets nss_;
	//KNetSessions sessions_;
	std::unordered_map<std::string, KNetSession*> handshakes_;
	std::unordered_map<u64, KNetSession*> establisheds_;
};





#endif



