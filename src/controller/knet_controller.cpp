
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
#include "knet_rudp.h"



KNetController::KNetController()
{
	controller_state_ = 0;
}

KNetController::~KNetController()
{

}


void KNetController::on_readable(KNetSocket& s, s64 now_ms)
{
	LogDebug() << s;
	int len = KNT_UPKT_SIZE;
	KNetAddress remote;
	s32 ret = s.recv_pkt(pkg_rcv_, len, remote, now_ms);
	if (ret != 0)
	{
		return ;
	}

	if (len < KNetHeader::HDR_SIZE)
	{
		return;
	}
	KNetHeader uhdr;
	const char* p = pkg_rcv_;
	p = knet_decode_hdr(p, uhdr);
	if (check_hdr(uhdr, p,  len - KNetHeader::HDR_SIZE) != 0)
	{
		LogDebug() << "check uhdr error. ";
		return;
	}

	switch (uhdr.cmd)
	{
	case KNETCMD_PB:
		on_probe(s, uhdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNETCMD_PB_ACK:
		on_probe_ack(s, uhdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNETCMD_CH:
		on_ch(s, uhdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNETCMD_SH:
		on_sh(s, uhdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNETCMD_PSH:
		on_psh(s, uhdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	case KNETCMD_RST:
		on_rst(s, uhdr, remote, now_ms);
		break;
	case KNETCMD_ECHO:
		on_echo(s, uhdr, p, len - KNetHeader::HDR_SIZE, remote, now_ms);
		break;
	default:
		break;
	}




	
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
	clean_session();

	return 0;
}



s32 KNetController::start_connect(const KNetConfigs& configs, s32& session_inst_id)
{
	u32 has_error = 0;
	session_inst_id = -1;
	KNetSession* session = NULL;
	if (sessions_.full())
	{
		KNetEnv::error_count()++;
		return -1;
	}


	for (u32 i = 0; i<configs.size(); i++)
	{
		auto& c = configs[i];

		KNetSocket* ns = create_stream();
		if (ns == NULL)
		{
			KNetEnv::error_count()++;
			has_error++;
			continue;
		}

		s32 ret = ns->init(c.localhost.c_str(), c.localport, c.remote_ip.c_str(), c.remote_port);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " --> " << c.remote_ip << ":" << c.remote_port << " has error";
			has_error ++;
			KNetEnv::error_count()++;
			continue;
		}

		LogInfo() << "init " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " success";
		ns->state_ = KNTS_BINDED;
		ns->slot_id_ = i;
		ns->flag_ |= KNTS_CLINET;
		if (session_inst_id == -1)
		{
			session = create_session();
			session_inst_id = session->inst_id_;
			session->init();
		}
		ns->client_session_inst_id_ = session_inst_id;
		KNetSocketSlot slot;
		slot.inst_id_ = ns->inst_id_;
		slot.last_active_ = KNetEnv::now_ms();
		slot.remote_ = ns->remote_;
		session->slots_[i] = slot;
		ns->state_ = KNTS_HANDSHAKE_CH;
		ns->refs_++;

		ns->reset_probe();
		ns->probe_seq_id_ = KNetEnv::create_seq_id();
		ns->state_ = KNTS_HANDSHAKE_PB;
		send_probe(*ns);

	}
	if (has_error == configs.size())
	{
		destroy_session(session);
		return -1;
	}
	return 0;
}


s32 KNetController::send_packet(KNetSocket& s, char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	static const s32 offset = offsetof(KNetHeader, slot);
	*(pkg + offset) = s.slot_id_;
	return s.send_pkt(pkg, len, remote);
}


s32  KNetController::send_probe(KNetSocket& s)
{
	KNetHeader hdr;
	KNetProbe probe;
	KNetEnv::fill_device_info(probe.dvi);
	probe.client_ms = KNetEnv::now_ms();
	probe.client_seq_id = s.probe_seq_id_;


	set_snd_data_offset(knet_encode_packet(snd_data(), probe));
	make_hdr(hdr, 0, 0, 0, 0, KNETCMD_PB, 0);
	write_hdr(hdr);

	s32 ret = send_packet(s, snd_head(), snd_len(), s.remote_, probe.client_ms);
	if (ret != 0)
	{
		LogError() << "send_probe " << s.local_.debug_string() << " --> " << s.remote_.debug_string() << " has error";
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
	make_hdr(hdr, 0, 0, 0, 0, KNETCMD_PB_ACK, 0);
	write_hdr(hdr);

	s32 ret = send_packet(s, snd_head(), snd_len(), remote, probe.client_ms);
	if (ret != 0)
	{
		LogError() << "send_probe_ack " << s.local_.debug_string() << " --> " << remote.debug_string() << " has error";
		return -1;
	}
	return 0;
}

void KNetController::on_probe_ack(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (len < KNetProbeAck::PKT_SIZE)
	{
		LogError() << "ch error";
		return;
	}
	KNetProbeAck packet;
	knet_decode_packet(pkg, packet);

	s64 now = KNetEnv::now_ms();
	s64 delay = now - packet.client_ms;
	if (packet.client_seq_id < s.probe_seq_id_)
	{
		LogWarn() << "expire probe ack";
		return;
	}
	if (delay < 0)
	{
		LogWarn() << "error probe ack";
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

	if (s.state_ == KNTS_HANDSHAKE_PB)
	{
		s.probe_shake_id_ = packet.shake_id;
		if (s.client_session_inst_id_ == -1 || s.is_server() || s.client_session_inst_id_ >= (s32)sessions_.size())
		{
			LogError() << "error";
			return;
		}
		KNetSession& session = sessions_[s.client_session_inst_id_];
		if (session.state_ != KNTS_LOCAL_INITED)
		{
			LogDebug() << "session.state_ aready ch";
			return;
		}
		session.state_ = KNTS_HANDSHAKE_CH;
		session.shake_id_ = s.probe_shake_id_;
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
			nss_[slot.inst_id_].state_ = s.state_;
			send_ch(nss_[slot.inst_id_], session);
		}
	}
	return;
}

s32 KNetController::send_ch(KNetSocket& s, KNetSession& session)
{
	KNetHeader hdr;
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
	make_hdr(hdr, 0, 0, 0, 0, KNETCMD_CH, 0);
	write_hdr(hdr);

	s32 ret = send_packet(s, snd_head(), snd_len(), s.remote_, KNetEnv::now_ms());
	if (ret != 0)
	{
		LogError() << "send_probe " << s.local_.debug_string() << " --> " << s.remote_.debug_string() << " has error";
		return -1;
	}
	return 0;
}


void KNetController::on_ch(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (len < KNetCH::PKT_SIZE)
	{
		LogError() << "ch error";
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
				iter->second->slots_[hdr.slot].last_active_ = now_ms;
				iter->second->slots_[hdr.slot].remote_ = remote;
				iter->second->slots_[hdr.slot].inst_id_ = s.inst_id_;
				s.refs_++;
			}
			else
			{
				if (iter->second->slots_[hdr.slot].inst_id_ != s.inst_id_)
				{
					nss_[iter->second->slots_[hdr.slot].inst_id_].refs_--;
				}
				iter->second->slots_[hdr.slot].last_active_ = now_ms;
				iter->second->slots_[hdr.slot].remote_ = remote;
				iter->second->slots_[hdr.slot].inst_id_ = s.inst_id_;
				s.refs_++;
			}
			return;
		}
		return;
	}

	KNetEnv::prof(KNT_STT_SES_CREATE_EVENTS)++;
	KNetSession* session = create_session();
	session->init();
	session->shake_id_ = ch.shake_id;
	session->session_id_ = ch.shake_id;
	session->slots_[hdr.slot].inst_id_ = s.inst_id_;
	session->slots_[hdr.slot].remote_ = remote;

	handshakes_s_[ch.shake_id] = session;
	establisheds_s_[session->session_id_] = session;

	LogInfo() << "server established sucess";

	KNetSH sh;
	sh.noise = 0;
	sh.result = 0;
	sh.session_id = session->session_id_;
	sh.shake_id = ch.shake_id;
	memcpy(sh.sg, session->sg_, sizeof(sh.sg));
	memcpy(sh.sp, session->sp_, sizeof(sh.sp));
	send_sh(s, ch, sh, remote);
	return;
}



s32 KNetController::send_sh(KNetSocket& s, const KNetCH& ch, const KNetSH& sh, KNetAddress& remote)
{
	KNetHeader hdr;
	set_snd_data_offset(knet_encode_packet(snd_data(), sh));
	make_hdr(hdr, sh.session_id, 0, 0, 0, KNETCMD_SH, 0);
	write_hdr(hdr);
	return send_packet(s, snd_head(), snd_len(), remote, KNetEnv::now_ms());
}

void KNetController::on_sh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	LogDebug() << "recv sh";
	if (len < KNetSH::PKT_SIZE)
	{
		LogError() << "sh error";
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
	if (session.state_ != KNTS_HANDSHAKE_CH)
	{
		return;
	}
	session.state_ = KNTS_ESTABLISHED;
	establisheds_c_[sh.session_id] = &session;
	session.session_id_ = sh.session_id;
	for (auto& slot: session.slots_)
	{
		if (slot.inst_id_  == -1 || slot.inst_id_ >= (s32)nss_.size())
		{
			continue;
		}
		nss_[slot.inst_id_].state_ = KNTS_ESTABLISHED;
	}

	LogInfo() << "client established sucess";
	send_psh(session, "123", 4);
}


s32 KNetController::send_psh(KNetSession& s, char* psh_buf, s32 len)
{
	KNetHeader hdr;
	memcpy(snd_data(), psh_buf, len);
	set_snd_data_offset(snd_data() + len);
	make_hdr(hdr, s.session_id_, ++s.snd_pkt_id_, 0, 0, KNETCMD_PSH, 0);
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
	KNetSession* session = NULL;
	if (s.is_server())
	{
		auto iter = establisheds_s_.find(hdr.session_id);
		if (iter != establisheds_s_.end())
		{
			session = iter->second;
		}
	}
	else
	{
		auto iter = establisheds_c_.find(hdr.session_id);
		if (iter != establisheds_c_.end())
		{
			session = iter->second;
		}
	}
	if (session == NULL)
	{
		LogError() << "on_psh error";
		return;
	}
	if (hdr.slot < session->slots_.size())
	{
		session->slots_[hdr.slot].remote_ = remote;
	}

	on_psh(*session, hdr, pkg, len, remote, now_ms);
}

void KNetController::on_psh(KNetSession& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	LogDebug() << "session on psh";
}


s32 KNetController::send_rst(KNetSession& s)
{
	KNetHeader hdr;
	set_snd_data_offset(snd_data());
	make_hdr(hdr, s.session_id_, ++s.snd_pkt_id_, 0, 0, KNETCMD_RST, 0);
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

void KNetController::on_rst(KNetSocket& s, KNetHeader& hdr, KNetAddress& remote, s64 now_ms)
{
	KNetSession* session = NULL;
	if (s.is_server())
	{
		auto iter = establisheds_s_.find(hdr.session_id);
		if (iter != establisheds_s_.end())
		{
			session = iter->second;
		}
	}
	else
	{
		auto iter = establisheds_c_.find(hdr.session_id);
		if (iter != establisheds_c_.end())
		{
			session = iter->second;
		}
	}
	if (session == NULL)
	{
		LogError() << "on_psh error";
		return;
	}
	on_rst(*session, hdr, remote, now_ms);
}

void KNetController::on_rst(KNetSession& s, KNetHeader& hdr, KNetAddress& remote, s64 now_ms)
{
	if (s.state_ != KNTS_ESTABLISHED)
	{
		return;
	}
	s.state_ = KNTS_LINGER;
	send_rst(s);
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
	case KNETCMD_PB:
	case KNETCMD_CH:
	case KNETCMD_SH:
	case KNETCMD_ECHO:
	case KNETCMD_RST:
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
	switch (cmd)
	{
	case KNETCMD_PB:
	case KNETCMD_CH:
	case KNETCMD_SH:
	case KNETCMD_ECHO:
	case KNETCMD_RST:
		hdr.mac = knet_encode_pkt_mac(pkg_data, len, hdr);
		break;
	default:
		hdr.mac = knet_encode_pkt_mac(pkg_data, len, hdr, "");
		break;
	}
	return 0;
}




s32 KNetController::clean_session()
{
	//clean session
	while (!handshakes_s_.empty())
	{
		remove_session(handshakes_s_.begin()->second->shake_id_, 0);
	}

	while (!establisheds_c_.empty())
	{
		remove_session(establisheds_c_.begin()->second->shake_id_, establisheds_c_.begin()->second->session_id_);
	}

	while (!establisheds_s_.empty())
	{
		remove_session(establisheds_s_.begin()->second->shake_id_, establisheds_s_.begin()->second->session_id_);
	}
	for (auto& s : nss_)
	{
		s.destroy();
	}
	nss_.clear();
	return 0;
}



s32 KNetController::remove_connect(s32 session_inst_id)
{
	if (session_inst_id >= (s32)sessions_.size() || session_inst_id < 0)
	{
		return -1;
	}
	return remove_session(sessions_[session_inst_id].shake_id_, sessions_[session_inst_id].session_id_);
}



s32 KNetController::remove_session(u64 shake_id, u64 session_id)
{
	KNetSession* session = NULL;
	if (true)
	{
		auto iter = handshakes_s_.find(shake_id);
		if (iter != handshakes_s_.end())
		{
			session = iter->second;
			handshakes_s_.erase(iter);
		}
	}


	if (true)
	{
		auto iter = establisheds_c_.find(session_id);
		if (iter != establisheds_c_.end())
		{
			if (session != NULL)
			{
				KNetEnv::error_count()++;
			}
			session = iter->second;
			establisheds_c_.erase(iter);
		}
	}

	if (true)
	{
		auto iter = establisheds_s_.find(session_id);
		if (iter != establisheds_s_.end())
		{
			if (session != NULL)
			{
				KNetEnv::error_count()++;
			}
			session = iter->second;
			establisheds_s_.erase(iter);
		}
	}

	return destroy_session(session);
}


KNetSession* KNetController::create_session()
{
	for (u32 i = 0; i < sessions_.size(); i++)
	{
		KNetSession& s = sessions_[i];
		if (s.state_ == KNTS_INVALID)
		{
			KNetEnv::prof(KNT_STT_SES_CREATE_EVENTS)++;
			return &s;
		}
	}

	if (sessions_.full())
	{
		return NULL;
	}
	sessions_.emplace_back(sessions_.size());
	KNetEnv::prof(KNT_STT_SES_ALLOC_EVENTS)++;
	KNetEnv::prof(KNT_STT_SES_CREATE_EVENTS)++;
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

	for (auto s: session->slots_)
	{
		if (s.inst_id_ < 0 || s.inst_id_ >= (s32)nss_.size())
		{
			KNetEnv::error_count()++;
			continue;
		}
		KNetSocket& skt = nss_[s.inst_id_];
		if (skt.inst_id_ != s.inst_id_)
		{
			KNetEnv::error_count()++;
			continue;
		}
		if (skt.refs_ <= 0)
		{
			KNetEnv::error_count()++;
			continue;
		}
		skt.refs_--;
	}
	KNetEnv::prof(KNT_STT_SES_DESTROY_EVENTS)++;
	session->destroy();
	return 0;
}

s32 KNetController::start_server(const KNetConfigs& configs)
{
	bool has_error = false;
	for (auto& c : configs)
	{
		KNetSocket* ns = create_stream();
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
		ns->flag_ |= KNTS_SERVER;
	}

	if (has_error)
	{
		destroy();
		return -1;
	}
	return 0;
}



s32 KNetController::do_tick()
{
	return do_select(nss_, 0);
}


KNetSocket* KNetController::create_stream()
{
	for (u32 i = 0; i < nss_.size(); i++)
	{
		KNetSocket& s = nss_[i];
		if (s.state_ == KNTS_INVALID)
		{
			KNetEnv::prof(KNT_STT_SKT_ALLOC_EVENTS)++;
			return &s;
		}
	}

	if (nss_.full())
	{
		return NULL;
	}
	nss_.emplace_back(nss_.size());
	KNetEnv::prof(KNT_STT_SKT_ALLOC_EVENTS)++;
	return &nss_.back();
}


void KNetController::destroy_stream(KNetSocket* s)
{
	if (s->state_ == KNTS_INVALID)
	{
		return;
	}
	KNetEnv::prof(KNT_STT_SKT_FREE_EVENTS)++;
	s->destroy();

}













