
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

#include "knet_controller.h"
#include "knet_socket.h"
#include "knet_proto.h"



KNetController::KNetController()
{
	controller_state_ = 0;
	tick_cnt_ = 0;
}

KNetController::~KNetController()
{

}

s32 KNetController::recv_one_packet(KNetSocket&s, s64 now_ms)
{
	int len = KNT_UPKT_SIZE;
	KNetAddress remote;
	s32 ret = s.recv_packet(pkg_rcv_, len, remote, now_ms);
	if (ret != 0)
	{
		return -1;
	}
	if (len == 0)
	{
		return len;
	}

	if (len < KNetHeader::HDR_SIZE)
	{
		LogError() << "check hdr error. ";
		return -2;
	}
	KNetHeader hdr;
	const char* p = pkg_rcv_;
	p = knet_decode_hdr(p, hdr);
	if (check_hdr(hdr, p, len - KNetHeader::HDR_SIZE) != 0)
	{
		LogError() << "check hdr error. ";
		return -3;
	}

	switch (hdr.cmd)
	{
	case KNTC_PB:
		on_probe(s, hdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNTC_PB_ACK:
		on_probe_ack(s, hdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNTC_CH:
		on_ch(s, hdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNTC_SH:
		on_sh(s, hdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNTC_PSH:
		on_psh(s, hdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNTC_RST:
		on_rst(s, hdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNTC_ECHO:
		on_echo(s, hdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	default:
		LogWarn() << "unknown packet:" << hdr;
		break;
	}
	return len;
}



void KNetController::on_readable(KNetSocket& s, s64 now_ms)
{
	//LogDebug() << s;
	s32 ret = recv_one_packet(s, now_ms);
	while (ret > 0)
	{
		ret = recv_one_packet(s, now_ms);
	}
	return;
}



void KNetController::on_echo(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (pkg[len - 1] != '\0')
	{
		LogDebug() << "test error.";
		return;
	}
	LogDebug() << "PSH:" << pkg;
	hdr.pkt_id++;
	hdr.mac = knet_encode_pkt_mac(pkg, len, hdr);
	knet_encode_hdr(pkg_rcv_, hdr);
	send_packet(s, pkg_rcv_, KNetHeader::HDR_SIZE + len, remote, now_ms);
}








s32 KNetController::destroy()
{
	for (auto& session : sessions_)
	{
		if (session.state_ == KNTS_INVALID)
		{
			continue;
		}
		close_session(session.inst_id_, false, KNTA_TERMINAL_STOP);
		remove_session(session.inst_id_);
	}
	sessions_.clear();

	for (auto& s : nss_)
	{
		skt_destroy(s);
	}
	nss_.clear();
	return 0;
}


s32 KNetController::start_server(const KNetConfigs& configs)
{
	bool has_error = false;
	for (auto& c : configs)
	{
		KNetSocket* ns = skt_create();
		if (ns == NULL)
		{
			KNetEnv::error_count()++;
			continue;
		}

		s32 ret = ns->init(c.localhost.c_str(), c.localport, NULL, 0);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			has_error = true;
			KNetEnv::error_count()++;
			break;
		}

		LogInfo() << "init " << c.localhost << ":" << c.localport << " success";
		ns->state_ = KNTS_ESTABLISHED;
		ns->refs_++;
		ns->flag_ |= KNTF_SERVER;
	}

	if (has_error)
	{
		destroy();
		return -1;
	}
	return 0;
}


s32 KNetController::stop()
{
	return destroy();
}


s32 KNetController::create_connect(const KNetConfigs& configs, KNetSession* &session)
{
	if (sessions_.full())
	{
		KNetEnv::error_count()++;
		return -1;
	}
	
	if (configs.empty())
	{
		KNetEnv::error_count()++;
		return -2;
	}

	session = create_session();
	if (session == NULL)
	{
		KNetEnv::error_count()++;
		return -3;
	}

	s32 ret = session->init(*this, KNTF_CLINET);
	if (ret != 0)
	{
		KNetEnv::error_count()++;
		remove_connect(session);
		return -4;
	}

	session->configs_ = configs;
	return 0;
}


s32 KNetController::start_connect(KNetSession& session, KNetOnConnect on_connected, s64 timeout)
{
	if (session.state_ != KNTS_INIT && session.state_ != KNTS_RST && session.state_ != KNTS_LINGER)
	{
		KNetEnv::error_count()++;
		return -1;
	}
	if (session.configs_.empty())
	{
		KNetEnv::error_count()++;
		return -2;
	}
	if (!(session.flag_ & KNTF_CLINET))
	{
		KNetEnv::error_count()++;
		return -3;
	}

	s32 s_count = 0;
	for (u32 i = 0; i < session.configs_.size(); i++)
	{
		auto& c = session.configs_[i];

		KNetSocket* ns = skt_create();
		if (ns == NULL)
		{
			KNetEnv::error_count()++;
			continue;
		}

		s32 ret = ns->init(c.localhost.c_str(), c.localport, c.remote_ip.c_str(), c.remote_port);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " --> " << c.remote_ip << ":" << c.remote_port << " has error";
			KNetEnv::error_count()++;
			continue;
		}

		LogInfo() << "init " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " success";
		ns->state_ = KNTS_INIT;
		ns->flag_ = KNTF_CLINET;
		ns->slot_id_ = i;
		
		KNetSocketSlot slot;
		slot.inst_id_ = ns->inst_id_;
		slot.remote_ = ns->remote_;
		session.slots_[i] = slot;
		ns->refs_++;

		ns->client_session_inst_id_ = session.inst_id_;
		ns->state_change_ts_ = KNetEnv::now_ms();


		ns->probe_seq_id_ = KNetEnv::create_seq_id();
		ns->state_ = KNTS_PROBE;
		ns->state_change_ts_ = KNetEnv::now_ms();
		ret = send_probe(*ns);
		if (ret != 0)
		{
			LogError() << "send_probe " << c.localhost << ":" << c.localport << " --> " << c.remote_ip << ":" << c.remote_port << " has error";
			KNetEnv::error_count()++;
			continue;
		}
		s_count++;
	}

	session.state_ = KNTS_PROBE;
	if (s_count > 0)
	{
		session.on_connected_ = on_connected;
		session.active_time_ = KNetEnv::now_ms();
		session.connect_time_ = session.active_time_;
		session.connect_expire_time_ = session.active_time_ + timeout;
		return 0;
	}
	close_session(session.inst_id_, false, KNTA_FLOW_FAILED);
	return  -10;
}



s32 KNetController::close_connect(KNetSession* session)
{
	if (session == NULL )
	{
		KNetEnv::error_count()++;
		return -1;
	}
	if (session->state_ == KNTS_INVALID)
	{
		KNetEnv::error_count()++;
		return -2;
	}
	if (session->is_server())
	{
		KNetEnv::error_count()++;
		return -3;
	}
	session->on_connected_ = nullptr;
	return close_session(session->inst_id_, false, KNTA_USER_OPERATE);
}

s32 KNetController::remove_connect(KNetSession* session)
{
	if (session == NULL)
	{
		KNetEnv::error_count()++;
		return -1;
	}
	return remove_session(session->inst_id_);
}



s32 KNetController::close_and_remove_session(KNetSession* session)
{
	if (session == NULL )
	{
		KNetEnv::error_count()++;
		return -1;
	}
	if (session->state_ == KNTS_INVALID)
	{
		KNetEnv::error_count()++;
		return -2;
	}
	if (!session->is_server())
	{
		KNetEnv::error_count()++;
		return -3;
	}
	s32 ret = close_session(session->inst_id_, false, KNTA_USER_OPERATE);
	if (ret != 0)
	{
		return -4;
	}
	ret = remove_session(session->inst_id_);
	if (ret != 0)
	{
		return -5;
	}
	return 0;
}



int KNetController::kcp_output(const char* buf, int len, ikcpcb* kcp, void* user, int user_id)
{
	if (user == NULL)
	{
		return -1;
	}
	KNetController* controller = (KNetController*)user;
	s32 inst_id = user_id;

	if (inst_id < 0 || inst_id >= (s32)controller->sessions_.size())
	{
		return -2;
	}

	s32 ret = controller->send_psh(controller->sessions_[inst_id], 0, buf, len);
	return ret;
}

void KNetController::kcp_writelog(const char* log, struct IKCPCB* kcp, void* user, int user_id)
{
	LogDebug() << "kcp inner log:" << log;
}







s32 KNetController::send_packet(KNetSocket& s, char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	static const s32 offset = offsetof(KNetHeader, slot);
	*(pkg + offset) = s.slot_id_;
	return s.send_packet(pkg, len, remote);
}


s32  KNetController::send_probe(KNetSocket& s)
{
	KNetHeader hdr;
	hdr.reset();
	KNetProbe probe;
	KNetEnv::fill_device_info(probe.dvi);
	probe.client_ms = KNetEnv::now_ms();
	probe.client_seq_id = s.probe_seq_id_;


	set_snd_data_offset(knet_encode_packet(snd_data(), probe));
	make_hdr(hdr, 0, 0, 0, 0, KNTC_PB, 0);
	write_hdr(hdr);

	s32 ret = send_packet(s, snd_head(), snd_len(), s.remote(), probe.client_ms);
	if (ret != 0)
	{
		LogError() << "send_probe " << s.local().debug_string() << " --> " << s.remote().debug_string() << " has error";
		return -1;
	}
	return 0;
}

void KNetController::on_probe(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (len < KNetProbe::PKT_SIZE)
	{
		LogError() << "ch error";
		return;
	}
	if (!skt_is_server(s))
	{
		LogError() << "ch error";
		return;
	}
	KNetProbe packet;
	knet_decode_packet(pkg, packet);
	if (!packet.dvi.is_valid())
	{
		LogError() << "packet decode error";
		return;
	}
	send_probe_ack(s, packet, remote);
	return;
}


s32 KNetController::send_probe_ack(KNetSocket& s, const KNetProbe& probe, KNetAddress& remote)
{
	KNetHeader hdr;
	hdr.reset();
	KNetProbeAck ack;
	ack.result = 0;
	ack.client_ms = probe.client_ms;
	ack.client_seq_id = probe.client_seq_id;
	if (probe.shake_id == 0)
	{
		ack.shake_id = ((100 + (u64)rand()) << 32) | KNetEnv::create_seq_id();
	}
	else
	{
		ack.shake_id = probe.shake_id;
	}
	set_snd_data_offset(knet_encode_packet(snd_data(), ack));
	make_hdr(hdr, 0, 0, 0, 0, KNTC_PB_ACK, 0);
	write_hdr(hdr);

	s32 ret = send_packet(s, snd_head(), snd_len(), remote, probe.client_ms);
	if (ret != 0)
	{
		LogError() << "send_probe_ack " << s.local().debug_string() << " --> " << remote.debug_string() << " has error";
		return -1;
	}
	return 0;
}

void KNetController::on_probe_ack(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (len < KNetProbeAck::PKT_SIZE)
	{
		LogError() << "on_probe_ack error";
		return;
	}
	if (skt_is_server(s))
	{
		LogError() << "on_probe_ack error";
		return;
	}
	KNetProbeAck packet;
	knet_decode_packet(pkg, packet);

	s64 now = KNetEnv::now_ms();
	s64 delay = now - packet.client_ms;
	if (packet.client_seq_id < s.probe_seq_id_)
	{
		LogWarn() << "expire probe ack. may be  recv old ack when reconnecting.";
		return;
	}

	s.probe_rcv_cnt_++;
	s.probe_last_ping_ = delay / 2;
	if (s.probe_avg_ping_ > 0)
	{
		s.probe_avg_ping_ = (s.probe_avg_ping_ * 10 / 2 + s.probe_last_ping_ * 10 / 8) / 10;
	}
	else
	{
		s.probe_avg_ping_ = s.probe_last_ping_;
	}

	if (s.state_ == KNTS_PROBE)
	{
		s.probe_shake_id_ = packet.shake_id;
		if (s.client_session_inst_id_ == -1 || skt_is_server(s) || s.client_session_inst_id_ >= (s32)sessions_.size())
		{
			LogError() << "error";
			return;
		}
		KNetSession& session = sessions_[s.client_session_inst_id_];
		if (session.state_ != KNTS_PROBE)
		{
			LogError() << "session.state not KNTS_PROBE ";
			return;
		}
		session.state_ = KNTS_CH;
		session.shake_id_ = s.probe_shake_id_;
		s32 snd_ch_cnt = 0;
		for (auto& slot: session.slots_)
		{
			if (slot.inst_id_ >= (s32)nss_.size())
			{
				LogError() << "error";
				continue;
			}
			if (slot.inst_id_ == -1)
			{
				continue;
			}
			nss_[slot.inst_id_].probe_shake_id_ = s.probe_shake_id_;
			nss_[slot.inst_id_].state_ = session.state_;
			s32 ret = send_ch(nss_[slot.inst_id_], session);
			if (ret == 0)
			{
				snd_ch_cnt++;
			}
		}
		if (snd_ch_cnt == 0 && session.on_connected_)
		{
			auto on = std::move(session.on_connected_);
			session.on_connected_ = NULL;
			u16 state = session.state_;
			close_session(session.inst_id_, false, KNTA_FLOW_FAILED);
			on(*this, session, false, state, 0);
			return;
		}
	}
	return;
}

s32 KNetController::send_ch(KNetSocket& s, KNetSession& session)
{
	KNetHeader hdr;
	hdr.reset();
	KNetCH ch;
	memset(&ch, 0, sizeof(ch));
	KNetEnv::fill_device_info(ch.dvi);
	ch.shake_id = s.probe_shake_id_;
	ch.session_id = session.session_id_;
	if (ch.session_id != 0 && !session.encrypt_key.empty())
	{
		strncpy(ch.cg, session.encrypt_key.c_str(), 16);
		ch.cg[15] = '\0';
	}

	set_snd_data_offset(knet_encode_packet(snd_data(), ch));
	make_hdr(hdr, 0, 0, 0, 0, KNTC_CH, 0);
	write_hdr(hdr);

	s32 ret = send_packet(s, snd_head(), snd_len(), s.remote(), KNetEnv::now_ms());
	if (ret != 0)
	{
		LogError() << "send_probe " << s.local().debug_string() << " --> " << s.remote().debug_string() << " has error";
		return -1;
	}
	return 0;
}


void KNetController::on_ch(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (len < KNetCH::PKT_SIZE)
	{
		LogError() << "on_ch error";
		return;
	}
	if (!skt_is_server(s))
	{
		LogError() << "on_ch error";
		return;
	}

	KNetCH ch;
	knet_decode_packet(pkg, ch);
	if (!ch.dvi.is_valid())
	{
		LogError() << "ch decode error";
		return;
	}


	auto iter = handshakes_s_.find(ch.shake_id);
	if (iter != handshakes_s_.end())
	{
		KNetSH sh;
		sh.noise = 0;
		sh.result = 0;
		sh.session_id = iter->second->session_id_;
		sh.shake_id = ch.shake_id;
		memcpy(sh.sg, iter->second->sg_, sizeof(sh.sg));
		memcpy(sh.sp, iter->second->sp_, sizeof(sh.sp));
		send_sh(s, ch, sh, remote);

		if (hdr.slot < KNT_MAX_SLOTS)
		{
			if (iter->second->slots_[hdr.slot].inst_id_ < 0)
			{
				iter->second->slots_[hdr.slot].remote_ = remote;
				iter->second->slots_[hdr.slot].inst_id_ = s.inst_id();
				s.refs_++;
			}
			else
			{
				if (iter->second->slots_[hdr.slot].inst_id_ != s.inst_id())
				{
					nss_[iter->second->slots_[hdr.slot].inst_id_].refs_--;
				}
				iter->second->slots_[hdr.slot].remote_ = remote;
				iter->second->slots_[hdr.slot].inst_id_ = s.inst_id();
				s.refs_++;
			}
			LogInfo() << "on client new ch(already established): session_id:" << iter->second->session_id_ <<" " << hdr;
			return;
		}
		return;
	}




	KNetSession* session = create_session();
	session->init(*this);
	session->state_ = KNTS_ESTABLISHED;
	session->flag_ = KNTF_SERVER;
	session->shake_id_ = ch.shake_id;
	session->session_id_ = ch.shake_id;

	session->slots_[hdr.slot].inst_id_ = s.inst_id();
	session->slots_[hdr.slot].remote_ = remote;
	s.refs_++;

	handshakes_s_[ch.shake_id] = session;
	establisheds_s_[session->session_id_] = session;

	LogInfo() << "server established sucess: session_id:" << session->session_id_ << hdr;

	KNetSH sh;
	sh.noise = 0;
	sh.result = 0;
	sh.session_id = session->session_id_;
	sh.shake_id = ch.shake_id;
	memcpy(sh.sg, session->sg_, sizeof(sh.sg));
	memcpy(sh.sp, session->sp_, sizeof(sh.sp));
	send_sh(s, ch, sh, remote);
	
	if (on_accept_)
	{
		on_accept_(*this, *session);
	}
	return;
}



s32 KNetController::send_sh(KNetSocket& s, const KNetCH& ch, const KNetSH& sh, KNetAddress& remote)
{
	KNetHeader hdr;
	hdr.reset();
	set_snd_data_offset(knet_encode_packet(snd_data(), sh));
	make_hdr(hdr, sh.session_id, 0, 0, 0, KNTC_SH, 0);
	write_hdr(hdr);
	return send_packet(s, snd_head(), snd_len(), remote, KNetEnv::now_ms());
}

void KNetController::on_sh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	LogDebug() << "recv sh";
	if (len < KNetSH::PKT_SIZE)
	{
		LogError() << "on_sh error";
		return;
	}
	if (skt_is_server(s))
	{
		LogError() << "on_sh error";
		return;
	}

	KNetSH sh;
	knet_decode_packet(pkg, sh);
	if (s.client_session_inst_id_ < 0 || s.client_session_inst_id_ >= (s32)sessions_.size())
	{
		LogError() << "sh error";
		return;
	}
	KNetSession& session = sessions_[s.client_session_inst_id_];
	if (session.state_ != KNTS_CH)
	{
		return;
	}
	session.state_ = KNTS_ESTABLISHED;
	session.session_id_ = sh.session_id;
	for (auto& slot: session.slots_)
	{
		if (slot.inst_id_  == -1 || slot.inst_id_ >= (s32)nss_.size())
		{
			continue;
		}
		nss_[slot.inst_id_].state_ = KNTS_ESTABLISHED;
	}

	LogInfo() << "client established sucess. " << hdr;

	if (session.on_connected_)
	{
		auto on = session.on_connected_;
		session.on_connected_ = nullptr;

		on(*this, session, true, session.state_, 0);
	}
	//send_psh(session, "123", 4);
}


s32 KNetController::send_psh(KNetSession& s, u8 chl, const char* psh_buf, s32 len)
{
	KNetHeader hdr;
	hdr.reset();
	memcpy(snd_data(), psh_buf, len);
	set_snd_data_offset(snd_data() + len);
	make_hdr(hdr, s.session_id_, ++s.snd_pkt_id_, 0, chl, KNTC_PSH, 0);
	write_hdr(hdr);
	s32 send_cnt = 0;
	for (auto& slot : s.slots_)
	{
		if (slot.inst_id_ == -1 || slot.inst_id_ >= (s32)nss_.size())
		{
			continue;
		}
		send_packet(nss_[slot.inst_id_], snd_head(), snd_len(), slot.remote_, KNetEnv::now_ms());
		send_cnt++;
	}
	return send_cnt == 0;
}




void KNetController::on_psh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (s.state_ != KNTS_ESTABLISHED)
	{
		LogWarn() << "skt not established on psh. " << hdr;
		return;
	}

	KNetSession* session = NULL;
	if (skt_is_server(s))
	{
		auto iter = establisheds_s_.find(hdr.session_id);
		if (iter != establisheds_s_.end())
		{
			session = iter->second;
		}
		else
		{
			LogWarn() << "server on_psh no session. " << hdr;;
			return;
		}
	}
	else
	{
		if (s.client_session_inst_id_ < 0 || s.client_session_inst_id_ > (s32)sessions_.size())
		{
			LogError() << "client on_psh error. " << hdr;;
			return;
		}
		session = &sessions_[s.client_session_inst_id_];
	}
	if (session == NULL)
	{
		LogError() << "on_psh error. skt is server:" << skt_is_server(s) <<", session id:" << hdr.session_id;
		return;
	}

	if (session->state_ != KNTS_ESTABLISHED)
	{
		LogError() << "socket on_psh state error. server:" << skt_is_server(s) <<", " << hdr;
		return;
	}
	if (skt_is_server(s))
	{
		if (hdr.slot >= session->slots_.size())
		{
			LogError() << "socket on_psh slot too max. server:" << skt_is_server(s) << ", " << hdr;
			return;
		}

		if (session->slots_[hdr.slot].inst_id_ == -1)
		{
			LogWarn() << "socket on_psh slot not handshark. server:" << skt_is_server(s) << ", " << hdr;
			session->slots_[hdr.slot].inst_id_ = s.inst_id();
			s.refs_++;
		}
		session->slots_[hdr.slot].remote_ = remote;
	}


	on_psh(*session, hdr, pkg, len, remote, now_ms);
}

void KNetController::on_psh(KNetSession& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (hdr.chl == 0)
	{
		ikcp_input(s.kcp_, pkg, len);
		LogDebug() << "session on psh chl 0.";
	}
	else if (on_data_)
	{
		on_data_(*this, s, hdr.chl, pkg, len, now_ms);
	}
}

s32 KNetController::send_rst(KNetSocket& s, u64 session_id, KNetAddress& remote)
{
	KNetHeader hdr;
	hdr.reset();
	set_snd_data_offset(snd_data());
	make_hdr(hdr, session_id, 0, 0, 0, KNTC_RST, 0);
	write_hdr(hdr);
	send_packet(s, snd_head(), snd_len(), remote, KNetEnv::now_ms());
	return 0;
}

s32 KNetController::send_rst(KNetSession& s, s32 code)
{
	KNetHeader hdr;
	hdr.reset();
	KNetRST rst;
	rst.rst_code = code;
	set_snd_data_offset(knet_encode_packet(snd_data(), rst));
	make_hdr(hdr, s.session_id_, ++s.snd_pkt_id_, 0, 0, KNTC_RST, 0);
	write_hdr(hdr);
	s32 send_cnt = 0;
	for (auto& slot : s.slots_)
	{
		if (slot.inst_id_ == -1 || slot.inst_id_ >= (s32)nss_.size())
		{
			continue;
		}
		LogInfo() << "session:" << s.session_id_ << " send rst used slot:" << nss_[slot.inst_id_].slot_id_;
		send_packet(nss_[slot.inst_id_], snd_head(), snd_len(), slot.remote_, KNetEnv::now_ms());
		send_cnt++;
	}
	return send_cnt == 0;
}

void KNetController::on_rst(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (s.state_ == KNTS_RST)
	{
		//LogDebug() << s << " arealdy in fin";
		return;
	}
	if (len < KNetRST::PKT_SIZE)
	{
		LogError() << "on_sh error";
		return;
	}

	KNetRST rst;
	knet_decode_packet(pkg, rst);


	KNetSession* session = NULL;
	if (skt_is_server(s))
	{
		auto iter = establisheds_s_.find(hdr.session_id);
		if (iter != establisheds_s_.end())
		{
			session = iter->second;
		}
		else
		{
			//don't process
			return;
		}
	}
	else
	{
		if (s.client_session_inst_id_ < 0 || s.client_session_inst_id_ >(s32)sessions_.size())
		{
			LogError() << "on_rst error";
			return;
		}
		session = &sessions_[s.client_session_inst_id_];
	}
	if (session == NULL)
	{
		LogError() << "on_psh error";
		return;
	}
	on_rst(*session, hdr, rst.rst_code, remote, now_ms);
}

void KNetController::on_rst(KNetSession& s, KNetHeader& hdr, s32 code, KNetAddress& remote, s64 now_ms)
{
	if (s.state_ != KNTS_ESTABLISHED)
	{
		return;
	}
	LogInfo() << "session on rst:" << hdr <<" rst code:" << code;

	close_session(s.inst_id_, true, code);
	//to do callback   
	if (s.is_server())
	{
		remove_session(s.inst_id_);
	}
	return ;
}

s32 KNetController::check_hdr(KNetHeader& hdr, const char* data, s32 len)
{
	if (hdr.pkt_size != KNetHeader::HDR_SIZE + len)
	{
		return -1;
	}
	u64 mac = 0;
	switch (hdr.mac)
	{
	case KNTC_PB:
	case KNTC_CH:
	case KNTC_SH:
	case KNTC_ECHO:
	case KNTC_RST:
		mac = knet_encode_pkt_mac(data, len, hdr);
		break;
	default:
		mac = knet_encode_pkt_mac(data, len, hdr, "");
		break;
	}
	if (mac != hdr.mac)
	{
		return -2;
	}
	return 0;
}


s32 KNetController::make_hdr(KNetHeader& hdr, u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag, const char* pkg_data, s32 len)
{
	if (len + KNetHeader::HDR_SIZE > KNT_UPKT_SIZE)
	{
		return -1;
	}

	hdr.session_id = session_id;
	hdr.pkt_id = pkt_id;
	hdr.pkt_size = KNetHeader::HDR_SIZE + len;
	hdr.version = version;
	hdr.chl = chl;
	hdr.cmd = cmd;
	hdr.flag = flag;
	hdr.slot = 0;
	hdr.mac = 0;
	switch (cmd)
	{
	case KNTC_PB:
	case KNTC_CH:
	case KNTC_SH:
	case KNTC_ECHO:
	case KNTC_RST:
		hdr.mac = knet_encode_pkt_mac(pkg_data, len, hdr);
		break;
	default:
		hdr.mac = knet_encode_pkt_mac(pkg_data, len, hdr, "");
		break;
	}
	return 0;
}






s32 KNetController::close_session(s32 inst_id, bool passive, s32 code)
{
	if (inst_id < 0 || inst_id >= (s32)sessions_.size())
	{
		KNetEnv::error_count()++;
		return -1;
	}

	KNetSession& session = sessions_[inst_id];
	if (session.state_ == KNTS_INVALID)
	{
		KNetEnv::error_count()++;
		return -2;
	}

	if (session.state_ == KNTS_ESTABLISHED && !passive)
	{
		s32 ret = send_rst(session, code);
		if (ret != 0)
		{
			KNetEnv::error_count()++;
		}
	}

	for (auto& slot : session.slots_)
	{
		if (slot.inst_id_ == -1)
		{
			continue;
		}
		if (slot.inst_id_ >= (s32)nss_.size())
		{
			LogError() << "error";
			KNetEnv::error_count()++;
			continue;
		}
		KNetSocket& s = nss_[slot.inst_id_];
		if (s.refs_ <= 0)
		{
			LogError() << "error";
			KNetEnv::error_count()++;
			continue;
		}

		if (s.flag_ != session.flag_)
		{
			LogError() << "error";
			KNetEnv::error_count()++;
		}

		s.refs_--;
		s.client_session_inst_id_ = -1;
		
		if (s.refs_ == 0)
		{
			skt_destroy(s);
		}
		slot.inst_id_ = -1;
	}

	if (session.flag_ & KNTF_SERVER)
	{
		handshakes_s_.erase(session.shake_id_);
		establisheds_s_.erase(session.session_id_);
	}
	session.on_connected_ = NULL;



	if (session.state_ == KNTS_ESTABLISHED)
	{
		if (passive)
		{
			session.state_ = KNTS_RST;
			LogInfo() << "session:" << session.session_id_ << " to passive rst";
		}
		else
		{
			session.state_ = KNTS_LINGER;
			LogInfo() << "session:" << session.session_id_ << " to linger";
		}

		if (on_disconnect_)
		{
			on_disconnect_(*this, session, passive);
		}
	}
	else
	{
		session.state_ = KNTS_LINGER;
		LogInfo() << "session:" << session.session_id_ << " to linger";
	}
	return 0;
}



s32 KNetController::remove_session(s32 inst_id)
{
	if (inst_id < 0 || inst_id >= (s32)sessions_.size())
	{
		KNetEnv::error_count()++;
		return -1;
	}

	KNetSession& session = sessions_[inst_id];

	if (session.state_ != KNTS_RST && session.state_ != KNTS_LINGER)
	{
		KNetEnv::error_count()++;
		return -2;
	}

	for (auto& slot: session.slots_)
	{
		if (slot.inst_id_ != -1)
		{
			KNetEnv::error_count()++;
			return -3;
		}
	}
	return destroy_session(&session);
}



KNetSession* KNetController::create_session()
{
	for (u32 i = 0; i < sessions_.size(); i++)
	{
		KNetSession& s = sessions_[i];
		if (s.state_ == KNTS_INVALID)
		{
			KNetEnv::count(KNT_STT_SES_CREATE_COUNT)++;
			return &s;
		}
	}

	if (sessions_.full())
	{
		return NULL;
	}
	sessions_.emplace_back(sessions_.size());
	KNetEnv::count(KNT_STT_SES_ALLOC_COUNT)++;
	KNetEnv::count(KNT_STT_SES_CREATE_COUNT)++;
	return &sessions_.back();
}

s32 KNetController::destroy_session(KNetSession* session)
{
	if (session == NULL)
	{
		return 0;
	}
	if (session->state_ == KNTS_INVALID)
	{
		return 0;
	}
	
	KNetEnv::count(KNT_STT_SES_DESTROY_COUNT)++;
	return session->destory();
}




s32 KNetController::do_tick()
{
	tick_cnt_++;
	

	for (auto&s : nss_)
	{
		if (s.state_ != KNTS_INVALID && s.refs_ == 0)
		{
			s32 ret = skt_destroy(s);
			if (ret != 0)
			{
				LogError() << "errro";
			}
		}
	}

	s32 ret = do_select(nss_, 0);
	if (ret != 0)
	{
		LogError() << "errro";
	}

	s64 now_ms = KNetEnv::now_ms();
	for (auto& session : sessions_)
	{
		if (session.state_ == KNTS_LINGER)
		{
			if (abs(session.active_time_ - now_ms) > 5000)
			{
				session.state_ = KNTS_INVALID;
			}
			continue;
		}

		if (session.state_ == KNTS_INVALID)
		{
			continue;
		}

		if (session.state_ == KNTS_ESTABLISHED)
		{
			ikcp_update(session.kcp_, tick_cnt_ * 10);
			s32 len = ikcp_recv(session.kcp_, pkg_rcv_, KNT_UPKT_SIZE);
			if (len > 0)
			{
				on_kcp_data(session, pkg_rcv_, len, KNetEnv::now_ms());
			}
		}

		//connect timeout 
		if (!session.is_server() &&  session.state_ < KNTS_ESTABLISHED && session.state_ >= KNTS_PROBE)
		{
			if (session.connect_expire_time_ < now_ms)
			{
				if (session.on_connected_)
				{
					auto on = std::move(session.on_connected_);
					session.on_connected_ = NULL;
					session.state_ = KNTS_LINGER;
					on(*this, session, false, session.state_, abs(session.connect_time_ - now_ms));
				}
			}
		}

		if (session.state_ == KNTS_ESTABLISHED && session.active_time_ - now_ms > 30000)
		{
			send_rst(session, 1);
		}


	}



	return 0;
}

void KNetController::on_kcp_data(KNetSession& s, const char* data, s32 len, s64 now_ms)
{
	if (on_data_)
	{
		on_data_(*this, s, 0, data, len, now_ms);
	}
}

void KNetController::send_data(KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
{
	if (chl == 0)
	{
		ikcp_send(s.kcp_, data, len);
	}
	else
	{
		send_psh(s, chl, data, len);
	}
}








void KNetController::skt_reset(KNetSocket&s)
{
	s.slot_id_ = 0;
	s.state_ = KNTS_INVALID;
	s.flag_ = KNTF_NONE;
	s.refs_ = 0;
	s.client_session_inst_id_ = -1;
	s.state_change_ts_ = KNetEnv::now_ms();
	s.user_data_ = 0;
	s.probe_seq_id_ = 0;
	s.probe_last_ping_ = 0;
	s.probe_avg_ping_ = 0;
	s.probe_snd_cnt_ = 0;
	s.probe_rcv_cnt_ = 0;
	s.probe_shake_id_ = 0;
}


KNetSocket* KNetController::skt_create()
{
	for (u32 i = 0; i < nss_.size(); i++)
	{
		KNetSocket& s = nss_[i];
		if (s.state_ == KNTS_INVALID)
		{
			KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT)++;
			skt_reset(s);
			return &s;
		}
	}

	if (nss_.full())
	{
		return NULL;
	}
	nss_.emplace_back(nss_.size());
	KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT)++;
	return &nss_.back();
}

s32  KNetController::skt_destroy(KNetSocket& s)
{
	if (s.state_ == KNTS_INVALID)
	{
		return 0;
	}

	if (s.refs_ > 0 && !(s.flag_ & KNTF_SERVER))
	{
		KNetEnv::error_count()++;
		return -2;
	}

	if (s.refs_ > 1 && (s.flag_ & KNTF_SERVER))
	{
		KNetEnv::error_count()++;
		return -3;
	}

	if (s.refs_ < 0)
	{
		KNetEnv::error_count()++;
		return -4;
	}

	s.state_ = KNTS_INVALID;
	s.flag_ = KNTF_NONE;
	s.refs_ = 0;
	s.state_change_ts_ = KNetEnv::now_ms();
	KNetEnv::count(KNT_STT_SKT_FREE_COUNT)++;
	return s.destroy();
}






