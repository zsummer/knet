
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
	void OnPKTEcho(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	void PKTCH(KNetSocket& s, KNetUHDR& hdr, KNetPKTCH& ch, KNetSession& session, s64 now_ms);
	void PKTSH(KNetSocket& s, KNetUHDR& hdr, KNetPKTSH& sh, KNetSession& session, s64 now_ms);
	void PKTPSH(KNetSocket& s, KNetUHDR& hdr, KNetPKTSH& sh, KNetSession& session, s64 now_ms);

	void OnPKTCH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
	void OnPKTSH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
	void OnPKTPSH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

private:
	s32 RemoveSession(KNetSession* session);
	s32 send_packet(KNetSocket&, char* pkg, s32 len, KNetAddress& remote, s64 now_ms);


public:
	KNetSocket* PopFreeSocket();
	void PushFreeSocket(KNetSocket*);

public:
	s32 check_hdr(KNetUHDR& hdr, const char* data, s32 len);
	s32 make_hdr(KNetUHDR& hdr, u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag, const char* pkg_data, s32 len);
	s32 make_hdr(KNetUHDR& hdr, u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag) { return make_hdr(hdr, session_id, pkt_id, version, chl, cmd, flag, snd_data(), snd_data_len()); }
	void write_hdr(KNetUHDR& hdr) { knet_encode_hdr(pkg_snd_, hdr); }
	void set_snd_data_offset(const char* p) { pkg_snd_offset_ = (s32)(p - pkg_snd_); }
	s32 snd_data_len() { return pkg_snd_offset_ - KNT_UHDR_SIZE; }
	s32 snd_len() { return pkg_snd_offset_; }

	char* snd_head() { return pkg_snd_; }
	char* snd_data() { return pkg_snd_ + KNT_UHDR_SIZE; }
	char* rcv_head() { return pkg_rcv_; }
	char* rcv_data() { return pkg_rcv_ + KNT_UHDR_SIZE; }
private:
	u32 controller_state_;
	KNetSockets nss_;
	char pkg_rcv_[KNT_UPKT_SIZE];
	s32 pkg_rcv_offset_;
	char pkg_snd_[KNT_UPKT_SIZE];
	s32 pkg_snd_offset_;
	//KNetSessions sessions_;
	std::unordered_map<KNetHandshakeKey, KNetSession*, KNetHandshakeKey::Hash> handshakes_c_;
	std::unordered_map<KNetHandshakeKey, KNetSession*, KNetHandshakeKey::Hash> handshakes_s_;
	std::unordered_map<u64, KNetSession*> establisheds_c_;
	std::unordered_map<u64, KNetSession*> establisheds_s_;
};





#endif



