
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

void KNetController::OnSocketTick(KNetSocket&s, s64 now_ms)
{
	if (s.state_ == KNTS_INVALID)
	{
		return;
	}
	if (s.refs_ == 0)
	{
		LogDebug() << s;
		//s.DestroySocket();
		return;
	}
}

void KNetController::OnSocketReadable(KNetSocket& s, s64 now_ms)
{
	LogDebug() << s;
	int len = KNT_UPKT_SIZE;
	KNetAddress remote;
	s32 ret = s.recv_pkt(pkg_rcv_, len, remote, now_ms);
	if (ret != 0)
	{
		return ;
	}

	if (len < KNetUHDR::HDR_SIZE)
	{
		return;
	}
	KNetUHDR uhdr;
	const char* p = pkg_rcv_;
	p = knet_decode_hdr(p, uhdr);
	if (check_hdr(uhdr, p,  len - KNetUHDR::HDR_SIZE) != 0)
	{
		LogDebug() << "check uhdr error. ";
		return;
	}

	switch (uhdr.cmd)
	{
	case KNETCMD_CH:
		OnPKTCH(s, uhdr, p, len - KNetUHDR::HDR_SIZE, remote, now_ms);
		break;
	case KNETCMD_SH:
		OnPKTSH(s, uhdr, p, len - KNetUHDR::HDR_SIZE, remote, now_ms);
		break;
	case KNETCMD_PSH:
		break;
	case KNETCMD_RST:
		break;
	case KNETCMD_ECHO:
		OnPKTEcho(s, uhdr, p, len - KNetUHDR::HDR_SIZE, remote, now_ms);
		break;
	default:
		break;
	}




	
}

void  KNetController::PKTCH(KNetSocket& s, KNetUHDR& hdr, KNetPKTCH& ch, KNetSession& session, s64 now_ms)
{
	ch.session_id = session.session_id_;
	ch.key = session.hkey_;
	memcpy(ch.cg, session.sg_, sizeof(session.sg_));
	memcpy(ch.cp, session.sp_, sizeof(session.sp_));
	memset(&ch.dvi, 0, sizeof(ch.dvi));
	memset(ch.noise, 0, sizeof(ch.noise));
}



void  KNetController::PKTSH(KNetSocket& s, KNetUHDR& hdr, KNetPKTSH& sh, KNetSession& session, s64 now_ms)
{
	sh.key = session.hkey_;
	sh.session_id = session.session_id_;
	sh.result = 0;
	sh.noise = 0;
	memcpy(sh.sg, session.sg_, sizeof(session.sg_));
	memcpy(sh.sp, session.sp_, sizeof(session.sp_));
}


void  KNetController::PKTPSH(KNetSocket& s, KNetUHDR& hdr, KNetPKTSH& sh, KNetSession& session, s64 now_ms)
{

}

void KNetController::OnPKTEcho(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
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
	send_packet(s, pkg_rcv_, KNetUHDR::HDR_SIZE + len, remote, now_ms);
}

void KNetController::OnPKTCH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (len < KNetPKTCH::PKT_SIZE)
	{
		LogError() << "ch error";
		return;
	}
	KNetPKTCH ch;
	knet_decode_ch(pkg, ch);
	if (!ch.dvi.is_valid())
	{
		LogError() << "ch decode error";
		return;
	}
	auto iter = handshakes_s_.find(ch.key);
	if (iter != handshakes_s_.end())
	{
		KNetPKTSH sh;
		PKTSH(s, hdr, sh, *iter->second, now_ms);
		set_snd_data_offset(knet_encode_sh(snd_data(), sh));
		KNetUHDR shdr;
		make_hdr(shdr, sh.session_id, 0, 0, 0, KNETCMD_SH, 0);
		write_hdr(shdr);
		send_packet(s, snd_head(), snd_len(), remote, now_ms);
		if (hdr.slot < KNT_MAX_SLOTS)
		{
			if (iter->second->slots_[hdr.slot].skt_id_ < 0)
			{
				iter->second->slots_[hdr.slot].last_active_ = now_ms;
				iter->second->slots_[hdr.slot].remote_ = remote;
				iter->second->slots_[hdr.slot].skt_id_ = s.skt_id_;
				s.refs_++;
			}
			else
			{
				if (iter->second->slots_[hdr.slot].skt_id_ != s.skt_id_)
				{
					nss_[iter->second->slots_[hdr.slot].skt_id_].refs_--;
				}
				iter->second->slots_[hdr.slot].last_active_ = now_ms;
				iter->second->slots_[hdr.slot].remote_ = remote;
				iter->second->slots_[hdr.slot].skt_id_ = s.skt_id_;
				s.refs_++;
			}
			return;
		}
		return;
	}

	KNetEnv::prof(KNT_STT_SES_CREATE_EVENTS)++;
	KNetSession* session = new KNetSession();
	session->hkey_ = ch.key;
	if (ch.ch_mac == 0)
	{
		session->session_id_ = KNetEnv::create_session_id();
		session->encrypt_key = "";
	}
	else
	{
		session->session_id_ = ch.session_id;
		session->encrypt_key = "";
	}
	handshakes_s_[session->hkey_] = session;
	establisheds_s_[session->session_id_] = session;
	LogInfo() << "server established sucess";

	KNetPKTSH sh;
	PKTSH(s, hdr, sh, *session, now_ms);
	set_snd_data_offset(knet_encode_sh(snd_data(), sh));
	KNetUHDR shdr;
	make_hdr(shdr, sh.session_id, 0, 0, 0, KNETCMD_SH, 0);
	write_hdr(shdr);
	send_packet(s, snd_head(), snd_len(), remote, now_ms);
	return;
}

void KNetController::OnPKTSH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	LogDebug() << "recv sh";
	if (len < KNetPKTSH::PKT_SIZE)
	{
		LogError() << "sh error";
		return;
	}
	KNetPKTSH sh;
	knet_decode_sh(pkg, sh);

	auto iter = handshakes_c_.find(sh.key);
	if (iter == handshakes_c_.end())
	{
		LogError() << "no this";
		return;
	}
	s.state_ = KNTS_ESTABLISHED;
	establisheds_c_[hdr.session_id] = iter->second;
	LogInfo() << "client established sucess";
}




void KNetController::OnPKTPSH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{

}



s32 KNetController::Destroy()
{
	CleanSession();

	return 0;
}



s32 KNetController::StartConnect(KNetShakeID hkey, const KNetConfigs& configs)
{
	u32 has_error = 0;
	KNetSession* session = NULL;
	if (!KNetHelper::CheckKey(hkey))
	{
		KNetEnv::error_count()++;
		return -1;
	}

	//if (sessions_.full())
	{
		//KNetEnv::error_count()++;
		//return -1;
	}

	for (u32 i = 0; i<configs.size(); i++)
	{
		auto& c = configs[i];

		KNetSocket* ns = create_stream();
		if (ns == NULL)
		{
			KNetEnv::error_count()++;
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
		if (session == NULL)
		{
			
			session = create_session();
			session->hkey_ = hkey;
		}
		KNetSocketSlot slot;
		slot.skt_id_ = ns->skt_id_;
		slot.last_active_ = KNetEnv::now_ms();
		slot.remote_ = ns->remote_;
		session->slots_[i] = slot;
		ns->state_ = KNTS_HANDSHAKE_CH;
		ns->refs_++;
		KNetUHDR hdr;
		KNetPKTCH ch;
		PKTCH(*ns, hdr, ch, *session, KNetEnv::now_ms());
		set_snd_data_offset(knet_encode_ch(snd_data(), ch));
		make_hdr(hdr, session->session_id_, session->pkt_id_++, 0, 0, KNETCMD_CH, 0);
		write_hdr(hdr);
		ret = send_packet(*ns, snd_head(), snd_len(), slot.remote_, 0);
		if (ret != 0)
		{
			LogError() << "send_pkt " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " has error";
			has_error++;
			continue;
		}	
	}
	if (has_error == configs.size())
	{
		destroy_session(session);
		return -1;
	}
	handshakes_c_.insert(std::make_pair(hkey, session));
	return 0;
}


s32 KNetController::send_packet(KNetSocket& s, char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	static const s32 offset = offsetof(KNetUHDR, slot);
	*(pkg + offset) = s.slot_id_;
	return s.send_pkt(pkg, len, remote);
}

KNetShakeID KNetController::CreateShakeID()
{
	KNetDeviceInfo dvi;
	KNetEnv::fill_device_info(dvi);
	KNetShakeID key;
	key.system_time = KNetEnv::now_ms();
	key.mac_code = (u32)(dvi.device_name[0] + dvi.device_mac[0] + dvi.device_type[0] + dvi.sys_name[0] + dvi.sys_version[0] + dvi.os_name[0]);
	key.rand_code = (u32)rand();
	key.sequence_code = KNetEnv::create_seq_id();
	return key;
}

s32 KNetController::check_hdr(KNetUHDR& hdr, const char* data, s32 len)
{
	if (hdr.pkt_size != KNetUHDR::HDR_SIZE + len)
	{
		return -1;
	}
	u64 mac = 0;
	switch (hdr.mac)
	{
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


s32 KNetController::make_hdr(KNetUHDR& hdr, u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag, const char* pkg_data, s32 len)
{
	if (len + KNetUHDR::HDR_SIZE > KNT_UPKT_SIZE)
	{
		return -1;
	}

	hdr.session_id = session_id;
	hdr.pkt_id = pkt_id;
	hdr.pkt_size = KNetUHDR::HDR_SIZE + len;
	hdr.version = version;
	hdr.chl = chl;
	hdr.cmd = cmd;
	hdr.flag = flag;
	hdr.slot = 0;
	switch (cmd)
	{
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




s32 KNetController::CleanSession()
{
	//clean session
	while (!handshakes_s_.empty())
	{
		RemoveSession(handshakes_s_.begin()->second->hkey_, 0);
	}
	while (!handshakes_c_.empty())
	{
		RemoveSession(handshakes_c_.begin()->second->hkey_, 0);
	}

	while (!establisheds_c_.empty())
	{
		RemoveSession(establisheds_c_.begin()->second->hkey_, establisheds_c_.begin()->second->session_id_);
	}

	while (!establisheds_s_.empty())
	{
		RemoveSession(establisheds_s_.begin()->second->hkey_, establisheds_s_.begin()->second->session_id_);
	}
	for (auto& s : nss_)
	{
		s.destroy();
	}
	nss_.clear();
	return 0;
}



s32 KNetController::RemoveSession(KNetShakeID hkey, u64 session_id)
{
	KNetSession* session = NULL;
	if (true)
	{
		auto iter = handshakes_c_.find(hkey);
		if (iter != handshakes_c_.end())
		{
			session = iter->second;
			handshakes_c_.erase(iter);
		}
	}
	if (true)
	{
		auto iter = handshakes_s_.find(hkey);
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
	KNetEnv::prof(KNT_STT_SES_CREATE_EVENTS)++;
	KNetSession* session = new KNetSession();
	return session;
}

s32 KNetController::destroy_session(KNetSession* session)
{
	if (session == NULL)
	{
		return 0;
	}
	
	for (auto s: session->slots_)
	{
		if (s.skt_id_ < 0 || s.skt_id_ >= (u32)nss_.size())
		{
			KNetEnv::error_count()++;
			continue;
		}
		KNetSocket& skt = nss_[s.skt_id_];
		if (skt.skt_id_ != s.skt_id_)
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
	delete session;
	return 0;
}

s32 KNetController::StartServer(const KNetConfigs& configs)
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
		Destroy();
		return -1;
	}
	return 0;
}



s32 KNetController::DoSelect()
{
	return Select(nss_, 0);
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













