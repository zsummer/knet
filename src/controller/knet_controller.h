
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
#include "knet_rudp.h"


#ifndef KNET_MAX_SESSIONS
#define KNET_MAX_SESSIONS 10
#endif // KNET_MAX_SESSIONS
static_assert(KNET_MAX_SESSIONS >= 10, "");


class KNetSession;
using KNetSessions = zarray<KNetSession, KNET_MAX_SESSIONS>;



class KNetController: public KNetSelect
{
public:
	KNetController();
	~KNetController();
	s32 start_server(const KNetConfigs& configs);
	s32 start_connect(const KNetConfigs& configs, KNetSession*& session);

	s32 remove_session(s32 inst_id);
	s32 remove_session_with_rst(s32 inst_id);



	s32 clean_session();

	s32 do_tick();


	s32 destroy();
	virtual void on_readable(KNetSocket&, s64 now_ms) override;


	void on_echo(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);



public:
	void send_kcp_data(KNetSession& s, char* data, s32 len, s64 now_ms);
	void on_kcp_data(KNetSession& s, char* data, s32 len, s64 now_ms);

	static int kcp_output(const char* buf, int len, ikcpcb* kcp, void* user, int user_id);
	static void kcp_writelog(const char* log, struct IKCPCB* kcp, void* user, int user_id);

public:
	s32 send_probe(KNetSocket& s);
	s32 send_probe_ack(KNetSocket& s, const KNetProbe& probe, KNetAddress& remote);
	void on_probe(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
	void on_probe_ack(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_ch(KNetSocket& s, KNetSession& session);
	void on_ch(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_sh(KNetSocket& s, const KNetCH& ch, const KNetSH& sh, KNetAddress& remote);
	void on_sh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_psh(KNetSession& s, const char* psh_buf, s32 len);
	void on_psh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
	void on_psh(KNetSession& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_rst(KNetSession& s);
	void on_rst(KNetSocket& s, KNetHeader& hdr, KNetAddress& remote, s64 now_ms);
	void on_rst(KNetSession& s, KNetHeader& hdr, KNetAddress& remote, s64 now_ms);

private:

	s32 send_packet(KNetSocket&, char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
	KNetSession* create_session();
	s32 destroy_session(KNetSession* session);

	KNetSocket* create_stream();
	void destroy_stream(KNetSocket*);

	s32 check_hdr(KNetHeader& hdr, const char* data, s32 len);
	s32 make_hdr(KNetHeader& hdr, u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag, const char* pkg_data, s32 len);
	s32 make_hdr(KNetHeader& hdr, u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag) { return make_hdr(hdr, session_id, pkt_id, version, chl, cmd, flag, snd_data(), snd_data_len()); }
	void write_hdr(KNetHeader& hdr) { knet_encode_hdr(pkg_snd_, hdr); }
	void set_snd_data_offset(const char* p) { pkg_snd_offset_ = (s32)(p - pkg_snd_); }
	s32 snd_data_len() { return pkg_snd_offset_ - KNT_UHDR_SIZE; }
	s32 snd_len() { return pkg_snd_offset_; }
	char* snd_head() { return pkg_snd_; }
	char* snd_data() { return pkg_snd_ + KNT_UHDR_SIZE; }
	char* rcv_head() { return pkg_rcv_; }
	char* rcv_data() { return pkg_rcv_ + KNT_UHDR_SIZE; }
private:
	u32  tick_cnt_;
	u32 controller_state_;
	KNetSockets nss_;
	KNetSessions sessions_;
	char pkg_rcv_[KNT_UPKT_SIZE];
	s32 pkg_rcv_offset_;
	char pkg_snd_[KNT_UPKT_SIZE];
	s32 pkg_snd_offset_;
	//KNetSessions sessions_;
	std::unordered_map<u64, KNetSession*> handshakes_s_;
	std::unordered_map<u64, KNetSession*> establisheds_s_;
};





#endif



