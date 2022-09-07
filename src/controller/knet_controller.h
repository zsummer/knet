
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
#include "knet_proto.h"




class KNetSession;
using KNetSessions = zarray<KNetSession, KNET_MAX_SESSIONS>;



class KNetController: public KNetSelect
{
public:
	friend class KNetSession;
	KNetController();
	~KNetController();

public:
	KNetOnAccept set_on_accept(KNetOnAccept on_accept) { KNetOnAccept old = on_accept_; on_accept_ = on_accept; return old; }
	KNetOnDisconnect set_on_disconnect(KNetOnDisconnect on_disconnect) { KNetOnDisconnect old = on_disconnect_; on_disconnect_ = on_disconnect; return old; }
	KNetOnData set_on_data(KNetOnData on_data) { KNetOnData old = on_data_; on_data_ = on_data; return old; }
private:
	KNetOnAccept on_accept_;
	KNetOnDisconnect on_disconnect_;
	KNetOnData on_data_;
public:
	s32 start_server(const KNetConfigs& configs);

	s32 create_connect(const KNetConfigs& configs, KNetSession*& session);
	s32 start_connect(KNetSession& session, KNetOnConnect on_connected, s64 timeout);
	s32 close_connect(KNetSession* session);
	s32 remove_connect(KNetSession* session);


	s32 close_and_remove_session(KNetSession* session);



public:
	s32 do_tick();

	s32 stop();
public:
	void send_data(KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms);
	void on_kcp_data(KNetSession& s, const char* data, s32 len, s64 now_ms);

public:
	s32 get_session_count_by_state(u16 state);
	s32 get_socket_count_by_state(u16 state);


private:
	s32 on_timeout(KNetSession& session, s64 now_ms);

	s32 close_session(s32 inst_id, bool passive, s32 code);
	s32 remove_session(s32 inst_id);
	s32 destroy();
	s32 recv_one_packet(KNetSocket&, s64 now_ms);
	virtual void on_readable(KNetSocket&, s64 now_ms) override;
	



private:
	static int kcp_output(const char* buf, int len, ikcpcb* kcp, void* user, int user_id);
	static void kcp_writelog(const char* log, struct IKCPCB* kcp, void* user, int user_id);


	s32 send_probe(KNetSocket& s);
	void on_probe(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_probe_ack(KNetSocket& s, const KNetProbe& probe, KNetAddress& remote);
	void on_probe_ack(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_ch(KNetSocket& s, KNetSession& session);
	void on_ch(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_sh(KNetSocket& s, const KNetCH& ch, const KNetSH& sh, KNetAddress& remote);
	void on_sh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_psh(KNetSession& s, u8 chl, const char* psh_buf, s32 len);
	void on_psh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
	void on_psh(KNetSession& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);

	s32 send_rst(KNetSocket& s, u64 session_id, KNetAddress& remote);
	s32 send_rst(KNetSession& s, s32 code);
	void on_rst(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);
	void on_rst(KNetSession& s, KNetHeader& hdr, s32 code, KNetAddress& remote, s64 now_ms);

	void on_echo(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms);


	s32 send_packet(KNetSocket&, char* pkg, s32 len, KNetAddress& remote, s64 now_ms);


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
	void skt_reset(KNetSocket&);

	KNetSocket* skt_alloc();
	s32  skt_free(KNetSocket& s);
	
	KNetSession* ses_alloc();
	s32 ses_free(KNetSession* session);


public:
	bool skt_is_server(KNetSocket& s) { return s.flag_ & KNTF_SERVER; }
	KNetSockets& nss() { return nss_; }
	KNetSessions& sessions() { return sessions_; }
	std::unordered_map<u64, KNetSession*>& handshakes_s() { return handshakes_s_; }
	std::unordered_map<u64, KNetSession*>& establisheds_s() { return establisheds_s_; }

private:
	u64  tick_cnt_;
	u32 controller_state_;
	KNetSockets nss_;
	KNetSessions sessions_;
	char pkg_rcv_[KNT_UPKT_SIZE];
	s32 pkg_rcv_offset_;
	char pkg_snd_[KNT_UPKT_SIZE];
	s32 pkg_snd_offset_;

	//char kcp_rcv_[KCP_RECV_BUFF_LEN];
	//s32 kcp_rcv_offset_;


	//KNetSessions sessions_;
	std::unordered_map<u64, KNetSession*> handshakes_s_;
	std::unordered_map<u64, KNetSession*> establisheds_s_;


};





#endif



