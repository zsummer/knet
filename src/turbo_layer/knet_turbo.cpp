
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

#include "knet_turbo.h"
#include "knet_socket.h"
#include "knet_proto.h"



KNetTurbo::KNetTurbo()
{
	turbo_state_ = 0;
	tick_cnt_ = 0;
}

KNetTurbo::~KNetTurbo()
{

}

s32 KNetTurbo::recv_one_packet(KNetSocket&s, s64 now_ms)
{
	int len = KNT_UPKT_SIZE;
	KNetAddress remote;
	s32 ret = s.recv_packet(pkg_rcv_, len, remote, now_ms);
	if (ret != 0)
	{
		LogWarn() << "recv_packet error. skt:" << s;
		return -1;
	}
	if (len == 0)
	{
		return len;
	}

	if (len < KNetHeader::HDR_SIZE)
	{
		LogWarn() << "less than hdr size error. skt:" << s;
		return -2;
	}
	KNetHeader hdr;
	const char* p = pkg_rcv_;
	p = knet_decode_hdr(p, hdr);
	if (check_hdr(hdr, p, len - KNetHeader::HDR_SIZE) != 0)
	{
		LogWarn() << "check hdr error.skt:" << s << hdr <<", recv size:" << len;
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



void KNetTurbo::on_readable(KNetSocket& s, s64 now_ms)
{
	//LogDebug() << s;
	s32 loop_count = 1;
	s32 ret = recv_one_packet(s, now_ms);
	while (ret > 0)
	{
		loop_count++;
		ret = recv_one_packet(s, now_ms);
	}
	if (loop_count > s.max_recv_loop_count_)
	{
		s.max_recv_loop_count_ = loop_count;
	}
	return;
}



void KNetTurbo::on_echo(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
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








s32 KNetTurbo::destroy()
{
	for (auto& session : sessions_)
	{
		if (session.state_ == KNTS_INVALID)
		{
			continue;
		}
		close_session(session.inst_id_, false, KNTR_TERMINAL_STOP);
		remove_session(session.inst_id_);
	}
	sessions_.clear();

	for (auto& s : nss_)
	{
		skt_free(s);
	}
	nss_.clear();
	return 0;
}


s32 KNetTurbo::start_server(const KNetConfigs& configs)
{
	bool has_error = false;
	for (auto& c : configs)
	{
		KNetSocket* ns = skt_alloc();
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
		ret = ns->set_skt_recv_buffer(KNET_SERVER_RECV_BUFF_SIZE);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			has_error = true;
			KNetEnv::error_count()++;
			break;
		}
		ret = ns->set_skt_send_buffer(KNET_SERVER_SEND_BUFF_SIZE);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			has_error = true;
			KNetEnv::error_count()++;
			break;
		}
		LogDebug() << "start_server init " << c.localhost << ":" << c.localport ;
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


s32 KNetTurbo::stop()
{
	return destroy();
}


s32 KNetTurbo::create_connect(const KNetConfigs& configs, KNetSession* &session)
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

	session = ses_alloc();
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


s32 KNetTurbo::start_connect(KNetSession& session, KNetOnConnect on_connected, s64 timeout)
{
	if (session.state_ != KNTS_RST && session.state_ != KNTS_LINGER && session.state_ != KNTS_CREATED)
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

		KNetSocket* ns = skt_alloc();
		if (ns == NULL)
		{
			KNetEnv::error_count()++;
			continue;
		}

		s32 ret = ns->init(c.localhost.c_str(), c.localport, c.remote_ip.c_str(), c.remote_port);
		if (ret != 0)
		{
			LogError() <<"start_connect skt init error. session:" << session <<", error:" << ret <<  ", last error code:" << KNetEnv::error_code() << " :  " << c.localhost << ":" << c.localport << " --> " << c.remote_ip << ":" << c.remote_port ;
			KNetEnv::error_count()++;
			continue;
		}


		ret = ns->set_skt_recv_buffer(KNET_CLIENT_RECV_BUFF_SIZE);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			KNetEnv::error_count()++;
			skt_free(*ns);
			continue;
		}
		ret = ns->set_skt_send_buffer(KNET_CLIENT_SEND_BUFF_SIZE);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			KNetEnv::error_count()++;
			skt_free(*ns);
			continue;
		}

		ns->flag_ = KNTF_CLINET;
		ns->slot_id_ = i;
		ns->salt_id_ = session.salt_id_;

		KNetSocketSlot slot;
		slot.inst_id_ = ns->inst_id_;
		slot.remote_ = ns->remote_;
		session.slots_[i] = slot;
		ns->refs_++;
		ns->client_session_inst_id_ = session.inst_id_;

		ns->probe_seq_id_ = KNetEnv::create_seq_id();
		ns->state_ = KNTS_PROBE;

		LogDebug() << "start_connect init slot[" << i << "]: session" << session << "  : " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << ", skt:" << *ns <<", shake:" << ns->probe_shake_id_ <<", salt:" << ns->salt_id_;


		ret = send_probe(*ns, 0);
		if (ret != 0)
		{
			LogError() << "start_connect error on send probe by slot[" << i << "]: session" << session << ", error:" << ret << "  : " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string();
			KNetEnv::error_count()++;
			continue;
		}
		s_count++;
	}

	session.state_ = KNTS_PROBE;
	if (s_count > 0)
	{
		s64 now_ms = KNetEnv::now_ms();
		session.on_connected_ = on_connected;
		session.connect_time_ = now_ms;
		session.connect_expire_time_ = now_ms + timeout;
		session.last_send_ts_ = now_ms;
		KNetEnv::call_user(KNTP_SES_CONNECT);
		return 0;
	}
	close_session(session.inst_id_, false, KNTR_FLOW_FAILED);
	return  -10;
}



s32 KNetTurbo::close_connect(KNetSession* session)
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
	if (session->state_ == KNTS_CREATED || session->state_ == KNTS_RST || session->state_ == KNTS_LINGER)
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
	return close_session(session->inst_id_, false, KNTR_USER_OPERATE);
}

s32 KNetTurbo::remove_connect(KNetSession* session)
{
	if (session == NULL)
	{
		KNetEnv::error_count()++;
		return -1;
	}
	return remove_session(session->inst_id_);
}



s32 KNetTurbo::close_and_remove_session(KNetSession* session)
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
	s32 ret = close_session(session->inst_id_, false, KNTR_USER_OPERATE);
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



int KNetTurbo::kcp_output(const char* buf, int len, ikcpcb* kcp, void* user, int user_id)
{
	if (user == NULL)
	{
		return -1;
	}
	KNetTurbo* turbo = (KNetTurbo*)user;
	s32 inst_id = user_id;

	if (inst_id < 0 || inst_id >= (s32)turbo->sessions_.size())
	{
		return -2;
	}

	s32 ret = turbo->send_psh(turbo->sessions_[inst_id], 0, buf, len);
	return ret;
}

void KNetTurbo::kcp_writelog(const char* log, struct IKCPCB* kcp, void* user, int user_id)
{
	LogDebug() << "kcp inner log:" << log;
}







s32 KNetTurbo::send_packet(KNetSocket& s, char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	static const s32 offset = offsetof(KNetHeader, slot);
	*(pkg + offset) = s.slot_id_;
	return s.send_packet(pkg, len, remote);
}


s32  KNetTurbo::send_probe(KNetSocket& s, u64 resend)
{
	KNetHeader hdr;
	hdr.reset();
	KNetProbe probe;
	KNetEnv::fill_device_info(probe.dvi);
	probe.client_ms = KNetEnv::now_ms();
	probe.client_seq_id = s.probe_seq_id_;
	probe.shake_id = 0;
	probe.salt_id = s.salt_id_;
	probe.resend = resend;

	set_snd_data_offset(knet_encode_packet(snd_data(), probe));
	make_hdr(hdr, 0, 0, 0, 0, KNTC_PB, 0);
	write_hdr(hdr);

	KNetEnv::call_mem(KNTP_SKT_PROBE, hdr.pkt_size);
	s32 ret = send_packet(s, snd_head(), snd_len(), s.remote(), probe.client_ms);
	if (ret != 0)
	{
		if (s.client_session_inst_id_ != -1 && s.client_session_inst_id_ < (s32)sessions_.size())
		{
			LogError() << "send probe by slot[" << s.slot_id_ << "]: " << s  <<" session:" << sessions_[s.client_session_inst_id_] << ", error:" << ret << "  : " << s.local_.debug_string() << " --> " << s.remote_.debug_string();
		}
		else
		{
			LogError() << "send probe by slot[" << s.slot_id_ << "]: error:" << ret << "  : " << s << s.local_.debug_string() << " --> " << s.remote_.debug_string();
		}
		return -1;
	}
	return 0;
}

void KNetTurbo::on_probe(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	KNetEnv::call_mem(KNTP_SKT_ON_PROBE, hdr.pkt_size);
	if (len < KNetProbe::PKT_SIZE)
	{
		LogWarn() << "on_probe error len. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	if (!skt_is_server(s))
	{
		LogWarn() << "on_probe error flag. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	KNetProbe packet;
	knet_decode_packet(pkg, packet);
	if (!packet.dvi.is_valid())
	{
		LogWarn() << "on_probe error decode. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	if (packet.resend > 0)
	{
		LogWarn() << "on_probe (from resend). s:" << s << ", hdr:" << hdr << ", shake:" << packet.shake_id << ", salt:" << packet.salt_id;
	}

	send_probe_ack(s, packet, remote);
	return;
}


s32 KNetTurbo::send_probe_ack(KNetSocket& s, const KNetProbe& probe, KNetAddress& remote)
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
	KNetEnv::call_mem(KNTP_SKT_PROBE_ACK, hdr.pkt_size);
	s32 ret = send_packet(s, snd_head(), snd_len(), remote, probe.client_ms);
	if (ret != 0)
	{
		LogWarn() << "send_probe_ack error ret. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return -1;
	}
	return 0;
}

void KNetTurbo::on_probe_ack(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	KNetEnv::call_mem(KNTP_SKT_ON_PROBE_ACK, hdr.pkt_size);
	if (len < KNetProbeAck::PKT_SIZE)
	{
		LogWarn() << "on_probe_ack error len. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	if (skt_is_server(s))
	{
		LogWarn() << "on_probe_ack error flag. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	KNetProbeAck packet;
	knet_decode_packet(pkg, packet);

	s64 now = KNetEnv::now_ms();
	s64 delay = now - packet.client_ms;
	if (packet.client_seq_id < s.probe_seq_id_)
	{
		LogWarn() << "on_probe_ack error expire probe ack. may be  recv old ack when reconnecting. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}

	s.probe_last_ping_ = delay / 2;
	if (s.probe_avg_ping_ > 0)
	{
		s.probe_avg_ping_ = (s.probe_avg_ping_ * 10 / 2 + s.probe_last_ping_ * 10 / 8) / 10;
	}
	else
	{
		s.probe_avg_ping_ = s.probe_last_ping_;
	}

	if (s.state_ != KNTS_PROBE)
	{
		LogDebug() << "on_probe_ack warn skt state. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}


	s.probe_shake_id_ = packet.shake_id;
	if (s.client_session_inst_id_ == -1 || skt_is_server(s) || s.client_session_inst_id_ >= (s32)sessions_.size())
	{
		LogError() << "error";
		return;
	}
	KNetSession& session = sessions_[s.client_session_inst_id_];
	if (session.state_ != KNTS_PROBE)
	{
		LogWarn() << "on_probe_ack error session state. s:" << s << ", hdr:" << hdr  <<", session:" << session << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	session.last_recv_ts_ = now_ms;
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
		s32 ret = send_ch(nss_[slot.inst_id_], session, 0);
		if (ret == 0)
		{
			snd_ch_cnt++;
		}
	}

	if (snd_ch_cnt == 0)
	{
		LogWarn() << "on_probe_ack error send_ch: all slot send failed. s:" << s << ", hdr:" << hdr << ", session:" << session << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
	}

	if (snd_ch_cnt == 0 && session.on_connected_)
	{
		auto on = std::move(session.on_connected_);
		session.on_connected_ = NULL;
		u16 state = session.state_;
		close_session(session.inst_id_, false, KNTR_FLOW_FAILED);
		on(*this, session, false, state, 0);
		return;
	}

	return;
}

s32 KNetTurbo::send_ch(KNetSocket& s, KNetSession& session, u64 resend)
{
	KNetHeader hdr;
	hdr.reset();
	KNetCH ch;
	memset(&ch, 0, sizeof(ch));
	KNetEnv::fill_device_info(ch.dvi);
	ch.shake_id = s.probe_shake_id_;
	ch.salt_id = s.salt_id_;
	ch.session_id = session.session_id_;
	ch.resend = resend;

	if (ch.session_id != 0 && !session.encrypt_key.empty())
	{
		strncpy(ch.cg, session.encrypt_key.c_str(), 16);
		ch.cg[15] = '\0';
	}

	set_snd_data_offset(knet_encode_packet(snd_data(), ch));
	make_hdr(hdr, 0, 0, 0, 0, KNTC_CH, 0);
	write_hdr(hdr);

	KNetEnv::call_mem(KNTP_SKT_CH, hdr.pkt_size);
	session.last_send_ts_ = KNetEnv::now_ms();
	s32 ret = send_packet(s, snd_head(), snd_len(), s.remote(), KNetEnv::now_ms());
	if (ret != 0)
	{
		LogWarn() << "send_ch error ret:" << ret <<".  s:" << s << ", session:" << session << ", hdr : " << hdr << " : " << s.local().debug_string() << " --> " << s.remote().debug_string();
		return -1;
	}
	return 0;
}


void KNetTurbo::on_ch(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	KNetEnv::call_mem(KNTP_SKT_ON_CH, hdr.pkt_size);
	if (len < KNetCH::PKT_SIZE)
	{
		LogWarn() << "on_ch error len. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	if (!skt_is_server(s))
	{
		LogWarn() << "on_ch error flag. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}

	KNetCH ch;
	knet_decode_packet(pkg, ch);
	if (!ch.dvi.is_valid())
	{
		LogWarn() << "on_ch error decode. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}

	if (ch.resend > 0)
	{
		LogWarn() <<" shake_id:" << ch.shake_id << ", salt : " << ch.salt_id <<", skt:" << s;

	}

	auto iter = handshakes_s_.find(ch.shake_id);
	if (iter != handshakes_s_.end())
	{
		if (iter->second->salt_id_ != ch.salt_id)
		{
			LogWarn() << "session:" << iter->second << ", on shakes error.  shake_id:" << ch.shake_id << ", but old salt:" << iter->second->salt_id_ << ", ch salt:" << ch.salt_id;
			return;
		}

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
				LogDebug() << "on client new ch(already established) on new slot: session:" << *iter->second << " " << hdr <<", s:" << s ;
			}
			else
			{
				if (iter->second->slots_[hdr.slot].inst_id_ != s.inst_id())
				{
					nss_[iter->second->slots_[hdr.slot].inst_id_].refs_--;
					iter->second->slots_[hdr.slot].remote_ = remote;
					iter->second->slots_[hdr.slot].inst_id_ = s.inst_id();
					s.refs_++;
					LogWarn() << "on client repeat ch(already established) duplicate slot: session:" << *iter->second << " " << hdr << ", s:" << s;
				}
				else
				{
					iter->second->slots_[hdr.slot].remote_ = remote;
					LogDebug() << "on client repeat ch(already established): session:" << *iter->second << " " << hdr << ", s:" << s;
				}
			}
			
			return;
		}
		return;
	}




	KNetSession* session = ses_alloc();
	if (session == NULL)
	{
		LogWarn() << "too many client session.  can not create new session;   hdr" << hdr << ", s:" << s;
		return ;
	}
	session->init(*this);
	session->state_ = KNTS_ESTABLISHED;
	session->flag_ = KNTF_SERVER;
	session->shake_id_ = ch.shake_id;
	session->salt_id_ = ch.salt_id;
	session->session_id_ = ch.shake_id;

	session->slots_[hdr.slot].inst_id_ = s.inst_id();
	session->slots_[hdr.slot].remote_ = remote;
	s.refs_++;

	handshakes_s_[ch.shake_id] = session;
	establisheds_s_[session->session_id_] = session;

	session->last_recv_ts_ = now_ms;
	session->last_send_ts_ = now_ms;

	LogDebug() << "server accept new connect: s:" << s << ", session:" << *session <<", hdr:" << hdr;

	KNetSH sh;
	sh.noise = 0;
	sh.result = 0;
	sh.session_id = session->session_id_;
	sh.shake_id = ch.shake_id;
	memcpy(sh.sg, session->sg_, sizeof(sh.sg));
	memcpy(sh.sp, session->sp_, sizeof(sh.sp));
	send_sh(s, ch, sh, remote);
	KNetEnv::call_user(KNTP_SES_ON_ACCEPT);
	if (on_accept_)
	{
		on_accept_(*this, *session);
	}
	return;
}



s32 KNetTurbo::send_sh(KNetSocket& s, const KNetCH& ch, const KNetSH& sh, KNetAddress& remote)
{
	KNetHeader hdr;
	hdr.reset();
	set_snd_data_offset(knet_encode_packet(snd_data(), sh));
	make_hdr(hdr, sh.session_id, 0, 0, 0, KNTC_SH, 0);
	write_hdr(hdr);
	KNetEnv::call_mem(KNTP_SKT_SH, hdr.pkt_size);
	return send_packet(s, snd_head(), snd_len(), remote, KNetEnv::now_ms());
}

void KNetTurbo::on_sh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	KNetEnv::call_mem(KNTP_SKT_ON_SH, hdr.pkt_size);
	if (len < KNetSH::PKT_SIZE)
	{
		LogWarn() << "on_sh error len. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	if (skt_is_server(s))
	{
		LogWarn() << "on_sh error flag. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}

	KNetSH sh;
	knet_decode_packet(pkg, sh);
	if (s.client_session_inst_id_ < 0 || s.client_session_inst_id_ >= (s32)sessions_.size())
	{
		LogError() << "on_sh error client_session_inst_id_. s:" << s << ", hdr:" << hdr << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
		return;
	}
	KNetSession& session = sessions_[s.client_session_inst_id_];
	if (session.state_ != KNTS_CH)
	{
		LogDebug() << "on_sh error state. s:" << s << ", hdr:" << hdr <<", session:" << session << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();
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
	session.last_recv_ts_ = now_ms;
	LogDebug() << "client connected: s:" << s << ", hdr:" << hdr << ", session:" << session << "  :   " << s.local().debug_string() << " --> " << remote.debug_string();

	KNetEnv::call_user(KNTP_SES_ON_CONNECTED);
	if (session.on_connected_)
	{
		auto on = session.on_connected_;
		session.on_connected_ = nullptr;
		for (auto& s : session.slots_)
		{
			if (s.inst_id_ == -1)
			{
				continue;
			}
			LogDebug() << "session:" <<  session << ".  slot skt:" << nss_[s.inst_id_];
		}
		on(*this, session, true, session.state_, 0);
	}
	//send_psh(session, "123", 4);
}


s32 KNetTurbo::send_psh(KNetSession& s, u8 chl, const char* psh_buf, s32 len)
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
	s.last_send_ts_ = KNetEnv::now_ms();
	return send_cnt == 0;
}




void KNetTurbo::on_psh(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
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

void KNetTurbo::on_psh(KNetSession& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	s.last_recv_ts_ = KNetEnv::now_ms();
	if (hdr.chl == 0)
	{
		ikcp_input(s.kcp_, pkg, len);
		//LogDebug() << "session on psh chl 0.";
	}
	else if (on_data_)
	{
		KNetEnv::call_mem(KNTP_SES_RECV, len);
		on_data_(*this, s, hdr.chl, pkg, len, now_ms);
	}
}

s32 KNetTurbo::send_rst(KNetSocket& s, u64 session_id, KNetAddress& remote)
{
	KNetHeader hdr;
	hdr.reset();
	set_snd_data_offset(snd_data());
	make_hdr(hdr, session_id, 0, 0, 0, KNTC_RST, 0);
	write_hdr(hdr);
	send_packet(s, snd_head(), snd_len(), remote, KNetEnv::now_ms());
	return 0;
}

s32 KNetTurbo::send_rst(KNetSession& s, s32 code)
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
		LogDebug() << "session:" << s.session_id_ << " send rst used slot:" << nss_[slot.inst_id_].slot_id_;
		send_packet(nss_[slot.inst_id_], snd_head(), snd_len(), slot.remote_, KNetEnv::now_ms());
		send_cnt++;
	}
	return send_cnt == 0;
}

void KNetTurbo::on_rst(KNetSocket& s, KNetHeader& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
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

void KNetTurbo::on_rst(KNetSession& s, KNetHeader& hdr, s32 code, KNetAddress& remote, s64 now_ms)
{
	if (s.state_ != KNTS_ESTABLISHED)
	{
		return;
	}
	LogDebug() << "session on rst:" << hdr <<" rst code:" << code;

	close_session(s.inst_id_, true, code);
	//to do callback   
	if (s.is_server() && on_accept_ == NULL && on_disconnect_ == NULL)
	{
		remove_session(s.inst_id_);
	}

	return ;
}

s32 KNetTurbo::check_hdr(KNetHeader& hdr, const char* data, s32 len)
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


s32 KNetTurbo::make_hdr(KNetHeader& hdr, u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag, const char* pkg_data, s32 len)
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






s32 KNetTurbo::close_session(s32 inst_id, bool passive, s32 code)
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
			skt_free(s);
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
			LogDebug() << "session:" << session.session_id_ << " to passive rst";
		}
		else
		{
			session.state_ = KNTS_LINGER;
			LogDebug() << "session:" << session.session_id_ << " to linger";
		}

		if (on_disconnect_)
		{
			on_disconnect_(*this, session, passive);
		}
	}
	else
	{
		session.state_ = KNTS_LINGER;
		LogDebug() << "session:" << session.session_id_ << " to linger";
	}
	return 0;
}



s32 KNetTurbo::remove_session(s32 inst_id)
{
	if (inst_id < 0 || inst_id >= (s32)sessions_.size())
	{
		KNetEnv::error_count()++;
		return -1;
	}

	KNetSession& session = sessions_[inst_id];

	if (session.state_ != KNTS_RST && session.state_ != KNTS_LINGER && session.state_ != KNTS_CREATED)
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
	return ses_free(&session);
}



KNetSession* KNetTurbo::ses_alloc()
{
	for (u32 i = 0; i < sessions_.size(); i++)
	{
		KNetSession& s = sessions_[i];
		if (s.state_ == KNTS_INVALID)
		{
			KNetEnv::call_user(KNTP_SES_ALLOC);
			s.state_ = KNTS_CREATED;
			return &s;
		}
	}

	if (sessions_.full())
	{
		return NULL;
	}
	sessions_.emplace_back(sessions_.size());
	sessions_.back().state_ = KNTS_CREATED;
	KNetEnv::call_user(KNTP_SES_ALLOC);
	return &sessions_.back();
}

s32 KNetTurbo::ses_free(KNetSession* session)
{
	if (session == NULL)
	{
		return 0;
	}
	if (session->state_ == KNTS_INVALID)
	{
		return 0;
	}
	
	KNetEnv::call_user(KNTP_SES_FREE);
	return session->destory();
}




s32 KNetTurbo::do_tick()
{
	tick_cnt_++;
	
	s64 now_ms = KNetEnv::now_ms();

	for (auto&s : nss_)
	{
		if (s.state_ != KNTS_INVALID && s.refs_ == 0)
		{
			s32 ret = skt_free(s);
			if (ret != 0)
			{
				LogError() << "errro";
			}
			continue;
		}

		on_readable(s, now_ms);
	}

	for (auto& session : sessions_)
	{

		if (session.state_ == KNTS_INVALID)
		{
			continue;
		}

		if (session.state_ == KNTS_ESTABLISHED)
		{
			ikcp_update(session.kcp_, (u32)tick_cnt_ * 10);
			s32 len = ikcp_recv(session.kcp_, pkg_rcv_, KNT_UPKT_SIZE);
			if (len > 0)
			{
				on_kcp_data(session, pkg_rcv_, len, KNetEnv::now_ms());
			}
		}

		if (tick_cnt_ % 10 == 0)
		{
			on_timeout(session, now_ms); //canbe destroy session .
		}
	}



	return 0;
}




s32 KNetTurbo::on_timeout(KNetSession& session, s64 now_ms)
{
	if (session.state_ == KNTS_RST || session.state_ == KNTS_LINGER)
	{
		if (abs(session.last_recv_ts_ - now_ms) > 5000 && abs(session.last_send_ts_ - now_ms) > 5000)
		{
			session.state_ = KNTS_CREATED;
		}
		return 0;
	}

	if (session.state_ == KNTS_ESTABLISHED && (session.last_send_ts_ - now_ms > 60000 && session.last_recv_ts_ - now_ms > 60000))
	{
		close_session(session.inst_id_, false, KNTR_TIMEOUT);
		if (session.is_server() && on_accept_ == NULL && on_disconnect_ == NULL)
		{
			remove_session(session.inst_id_);
		}
		return 0;
	}

	//connect timeout 
	if (!session.is_server() && session.state_ < KNTS_ESTABLISHED && session.state_ >= KNTS_PROBE)
	{
		if (session.connect_expire_time_ < now_ms)
		{
			LogWarn() << "session inst:" << session.inst_id_ << ", session id:" << session.session_id_ << " connect fail.  resend count:" << session.connect_resends_ << ", last state:" << session.state_ << ", shake:" << session.shake_id_ << ", salt:" << session.salt_id_;
			for (auto& s: session.slots_)
			{
				if (s.inst_id_ == -1)
				{
					continue;
				}
				LogWarn() << "session inst:" << session.inst_id_ << ", session id:" << session.session_id_ << " connect fail.  slot skt:" <<nss_[s.inst_id_];
			}
			if (session.on_connected_)
			{
				auto on = std::move(session.on_connected_);
				session.on_connected_ = NULL;
				auto state = session.state_;
				close_session(session.inst_id_, false, KNTR_TIMEOUT);
				on(*this, session, false, state, abs(session.connect_time_ - now_ms));
				return 0;
			}
			close_session(session.inst_id_, false, KNTR_TIMEOUT);
			return 0;
		}

		if (session.state_ == KNTS_PROBE)
		{
			for (auto& s : session.slots_)
			{
				if (s.inst_id_ == -1)
				{
					continue;
				}
				if (abs(nss_[s.inst_id_].last_send_ts_ - now_ms) > 600 && abs(nss_[s.inst_id_].last_recv_ts_ - now_ms) > 600)
				{
					session.connect_resends_++;
					KNetEnv::call_mem(KNTP_SKT_R_PROBE, 1);
					LogWarn() << "skt:" << nss_[s.inst_id_] << " resend probe.  session : " << session << ", shake:" << session.shake_id_ << ", salt:" << session.salt_id_;
					send_probe(nss_[s.inst_id_], session.connect_resends_);
				}
			}
		}

		if (session.state_ == KNTS_CH && abs(session.last_recv_ts_ - now_ms) > 600 && abs(session.last_send_ts_ - now_ms) > 600)
		{

			for (auto& s : session.slots_)
			{
				if (s.inst_id_ == -1)
				{
					continue;
				}
				session.connect_resends_++;
				KNetEnv::call_mem(KNTP_SKT_R_CH, 1);
				LogWarn() << "skt:" << nss_[s.inst_id_] << " resend ch.  session : " << session << ", shake:" << session.shake_id_ << ", salt:" << session.salt_id_;
				send_ch(nss_[s.inst_id_], session, session.connect_resends_);
				
			}
		}
	}


	return 0;
}







void KNetTurbo::on_kcp_data(KNetSession& s, const char* data, s32 len, s64 now_ms)
{
	KNetEnv::call_mem(KNTP_SES_RECV, len);
	if (on_data_)
	{
		on_data_(*this, s, 0, data, len, now_ms);
	}
}

void KNetTurbo::send_data(KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
{
	KNetEnv::call_mem(KNTP_SES_SEND, len);
	if (chl == 0)
	{
		ikcp_send(s.kcp_, data, len);
	}
	else
	{
		send_psh(s, chl, data, len);
	}
}








void KNetTurbo::skt_reset(KNetSocket&s)
{
	s32 inst_id = s.inst_id_;
	memset(&s, 0, sizeof(s));
	s.inst_id_ = inst_id;
	s.skt_ = INVALID_SOCKET;
}


KNetSocket* KNetTurbo::skt_alloc()
{
	for (u32 i = 0; i < nss_.size(); i++)
	{
		KNetSocket& s = nss_[i];
		if (s.state_ == KNTS_INVALID)
		{
			KNetEnv::call_user(KNTP_SKT_ALLOC);
			skt_reset(s);
			s.state_ = KNTS_CREATED;
			return &s;
		}
	}

	if (nss_.full())
	{
		return NULL;
	}
	nss_.emplace_back(nss_.size());
	KNetEnv::call_user(KNTP_SKT_ALLOC);
	return &nss_.back();
}

s32  KNetTurbo::skt_free(KNetSocket& s)
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
	KNetEnv::call_user(KNTP_SKT_FREE);
	return s.destroy();
}


s32 KNetTurbo::get_session_count_by_state(u16 state)
{
	s32 count = 0;
	for (auto& s: sessions_)
	{
		if (s.state_ == state)
		{
			count++;
		}
	}
	return count;
}


s32 KNetTurbo::get_socket_count_by_state(u16 state)
{
	s32 count = 0;
	for (auto& s : nss_)
	{
		if (s.state_ == state)
		{
			count++;
		}
	}
	return count;
}



