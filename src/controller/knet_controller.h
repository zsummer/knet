
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
#include "knet_helper.h"
#include "knet_select.h"
#include "knet_socket.h"
#include "knet_session.h"
#include "zpool.h"
#include "knet_rudp.h"


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
	s32 StartConnect(KNetHandshakeKey hkey, const KNetConfigs& configs);
	s32 RemoveSession(KNetHandshakeKey hkey, u64 session_id);
	s32 CleanSession();

	s32 DoSelect();
	s32 Destroy();
	virtual void OnSocketTick(KNetSocket&, s64 now_ms) override;
	virtual void OnSocketReadable(KNetSocket&, s64 now_ms) override;
	void OnPKGEcho(KNetSocket&s, KNetUHDR&hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
private:
	s32 RemoveSession(KNetSession* session);
	s32 SendUPKG(KNetSocket&, char* pkg_data, s32 len, KNetAddress& remote, s64 now_ms);
	s32 MakeUPKG(u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag, const char* pkg_data, s32 len, s64 now_ms);
	bool CheckUHDR(KNetUHDR& hdr, const char* pkg, s32 len);

public:
	KNetSocket* PopFreeSocket();
	void PushFreeSocket(KNetSocket*);
private:
	u32 controller_state_;
	KNetSockets nss_;
	char pkg_rcv_[KNT_UPKG_SIZE];
	char pkg_snd_[KNT_UPKG_SIZE];
	//KNetSessions sessions_;
	std::unordered_map<KNetHandshakeKey, KNetSession*, KNetHandshakeKey::Hash> handshakes_;
	std::unordered_map<u64, KNetSession*> establisheds_;
};





#endif



