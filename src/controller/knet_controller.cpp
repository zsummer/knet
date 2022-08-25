
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
	int len = KNT_UPKG_SIZE;
	KNetAddress remote;
	s32 ret = s.RecvFrom(pkg_rcv_, len, remote, now_ms);
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
	p = KNetDecodeUHDR(p, uhdr);
	if (!CheckUHDR(uhdr, p,  len - KNetUHDR::HDR_SIZE))
	{
		LogDebug() << "check uhdr error. ";
		return;
	}

	switch (uhdr.cmd)
	{
	case KNETCMD_CH:
		break;
	case KNETCMD_SH:
		break;
	case KNETCMD_PSH:
		break;
	case KNETCMD_RST:
		break;
	case KNETCMD_ECHO:
		OnPKGEcho(s, uhdr, p, len - KNetUHDR::HDR_SIZE, remote, now_ms);
		break;
	default:
		break;
	}




	
}

void  KNetController::PKGCH(KNetSocket& s, KNetUHDR& hdr, KNetPKGCH& ch, KNetSession& session, s64 now_ms)
{
	ch.session_id = session.session_id_;
	ch.key = session.hkey_;
	memcpy(ch.cg, session.sg_, sizeof(session.sg_));
	memcpy(ch.cp, session.sp_, sizeof(session.sp_));
	memset(&ch.dvi, 0, sizeof(ch.dvi));
	memset(ch.noise, 0, sizeof(ch.noise));
}



void  KNetController::PKGSH(KNetSocket& s, KNetUHDR& hdr, KNetPKGSH& sh, KNetSession& session, s64 now_ms)
{
	sh.key = session.hkey_;
	sh.session_id = session.session_id_;
	sh.result = 0;
	sh.noise = 0;
	memcpy(sh.sg, session.sg_, sizeof(session.sg_));
	memcpy(sh.sp, session.sp_, sizeof(session.sp_));
}


void  KNetController::PKGPSH(KNetSocket& s, KNetUHDR& hdr, KNetPKGSH& sh, KNetSession& session, s64 now_ms)
{

}

void KNetController::OnPKGEcho(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (pkg[len - 1] != '\0')
	{
		LogDebug() << "test error.";
		return;
	}
	LogDebug() << "PSH:" << pkg;
	hdr.pkt_id++;
	hdr.mac = KNetCTLMac(pkg, len, hdr);
	KNetEncodeUHDR(pkg_rcv_, hdr);
	SendUPKG(s, pkg_rcv_, KNetUHDR::HDR_SIZE + len, remote, now_ms);
}

void KNetController::OnPKGCH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{
	if (len < KNetPKGCH::PKT_SIZE)
	{
		LogError() << "ch error";
		return;
	}
	KNetPKGCH ch;
	KNetDecodePKGCH(pkg, ch);
	if (!ch.dvi.is_valid())
	{
		LogError() << "ch decode error";
		return;
	}
	auto iter = handshakes_.find(ch.key);
	if (iter != handshakes_.end())
	{
		KNetPKGSH sh;
		PKGSH(s, hdr, sh, *iter->second, now_ms);
		KNetEncodePKGSH(pkg_snd_ + KNT_UHDR_SIZE, sh);
		MakeUPKG(sh.session_id, 0, 0, 0, KNETCMD_CH, 0, pkg_snd_ + KNT_UHDR_SIZE, KNetPKGCH::PKT_SIZE, now_ms);
		SendUPKG(s, pkg_snd_, KNT_UHDR_SIZE + KNetPKGCH::PKT_SIZE, remote, now_ms);
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

	KNetEnv::Status(KNT_STT_SES_CREATE_EVENTS)++;
	KNetSession* session = new KNetSession();
	session->hkey_ = ch.key;
	if (ch.ch_mac == 0)
	{
		session->session_id_ = KNetEnv::CreateSessionID();
		session->encrypt_key = "";
	}
	else
	{
		session->session_id_ = ch.session_id;
		session->encrypt_key = "";
	}
	handshakes_[session->hkey_] = session;
	KNetPKGSH sh;
	PKGSH(s, hdr, sh, *session, now_ms);
	KNetEncodePKGSH(pkg_snd_ + KNT_UHDR_SIZE, sh);
	MakeUPKG(sh.session_id, 0, 0, 0, KNETCMD_CH, 0, pkg_snd_ + KNT_UHDR_SIZE, KNetPKGCH::PKT_SIZE, now_ms);
	SendUPKG(s, pkg_snd_, KNT_UHDR_SIZE + KNetPKGCH::PKT_SIZE, remote, now_ms);
	return;
}

void KNetController::OnPKGSH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{

}

void KNetController::OnPKGPSH(KNetSocket& s, KNetUHDR& hdr, const char* pkg, s32 len, KNetAddress& remote, s64 now_ms)
{

}



s32 KNetController::Destroy()
{
	CleanSession();

	return 0;
}



s32 KNetController::StartConnect(KNetHandshakeKey hkey, const KNetConfigs& configs)
{
	u32 has_error = 0;
	KNetSession* session = NULL;
	if (!KNetHelper::CheckKey(hkey))
	{
		KNetEnv::Errors()++;
		return -1;
	}

	//if (sessions_.full())
	{
		//KNetEnv::Errors()++;
		//return -1;
	}

	for (u32 i = 0; i<configs.size(); i++)
	{
		auto& c = configs[i];

		KNetSocket* ns = PopFreeSocket();
		if (ns == NULL)
		{
			KNetEnv::Errors()++;
			continue;
		}

		s32 ret = ns->InitSocket(c.localhost.c_str(), c.localport, c.remote_ip.c_str(), c.remote_port);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " --> " << c.remote_ip << ":" << c.remote_port << " has error";
			has_error ++;
			KNetEnv::Errors()++;
			continue;
		}

		LogInfo() << "init " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " success";
		ns->state_ = KNTS_BINDED;
		ns->slot_id_ = i;
		if (session == NULL)
		{
			KNetEnv::Status(KNT_STT_SES_CREATE_EVENTS)++;
			session = new KNetSession();
			session->hkey_ = hkey;
		}
		KNetSocketSlot slot;
		slot.skt_id_ = ns->skt_id_;
		slot.last_active_ = KNetEnv::Now();
		slot.remote_ = ns->remote_;
		session->slots_[i] = slot;
		ns->state_ = KNTS_HANDSHAKE_CH;
		ns->refs_++;


		ret = MakeUPKG(0, 0, 0, 0, KNETCMD_ECHO, 0, "abcde", 6, 0);
		if (ret != 0)
		{
			LogError() << "MakeUPKG " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " has error";
			has_error++;
			continue;
		}
		ret = SendUPKG(*ns, pkg_snd_, KNetUHDR::HDR_SIZE + 6, slot.remote_, 0);
		if (ret != 0)
		{
			LogError() << "SendTo " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " has error";
			has_error++;
			continue;
		}	
	}
	if (has_error == configs.size())
	{
		RemoveSession(session);
		return -1;
	}
	handshakes_.insert(std::make_pair(hkey, session));


	return 0;
}


s32 KNetController::SendUPKG(KNetSocket& s, char* pkg_data, s32 len, KNetAddress& remote, s64 now_ms)
{
	static const s32 offset = offsetof(KNetUHDR, slot);
	*(pkg_data + offset) = s.slot_id_;
	return s.SendTo(pkg_data, len, remote);
}


s32 KNetController::MakeUPKG(u64 session_id, u64 pkt_id, u16 version, u8 chl, u8 cmd, u8 flag, const char* pkg_data, s32 len, s64 now_ms)
{
	if (len + KNetUHDR::HDR_SIZE > KNT_UPKG_SIZE)
	{
		return -1;
	}

	KNetUHDR uhdr;
	uhdr.session_id = session_id;
	uhdr.pkt_id = pkt_id;
	uhdr.pkt_size = KNetUHDR::HDR_SIZE + len;
	uhdr.version = version;
	uhdr.chl = chl;
	uhdr.cmd = cmd;
	uhdr.flag = flag;
	uhdr.slot = 0;
	switch (cmd)
	{
	case KNETCMD_CH:
	case KNETCMD_SH:
	case KNETCMD_ECHO:
	case KNETCMD_RST:
		uhdr.mac = KNetCTLMac(pkg_data, len, uhdr);
		break;
	default:
		uhdr.mac = KNetPSHMac(pkg_data, len, "", uhdr);
		break;
	}
	char *p = KNetEncodeUHDR(pkg_snd_, uhdr);
	memcpy(p, pkg_data, len);
	return 0;
}


bool KNetController::CheckUHDR(KNetUHDR& hdr, const char* pkg, s32 len)
{
	if (hdr.pkt_size != KNetUHDR::HDR_SIZE + len)
	{
		return false;
	}
	u64 mac = 0;
	switch (hdr.mac)
	{
	case KNETCMD_CH:
	case KNETCMD_SH:
	case KNETCMD_ECHO:
	case KNETCMD_RST:
		mac = KNetCTLMac(pkg, len, hdr);
		break;
	default:
		mac = KNetPSHMac(pkg, len, "", hdr);
		break;
	}
	return mac == hdr.mac;
}



s32 KNetController::CleanSession()
{
	//clean session
	while (!handshakes_.empty())
	{
		RemoveSession(handshakes_.begin()->second->hkey_, 0);
	}

	while (!establisheds_.empty())
	{
		RemoveSession(establisheds_.begin()->second->hkey_, establisheds_.begin()->second->session_id_);
	}
	for (auto& s : nss_)
	{
		s.DestroySocket();
	}
	nss_.clear();
	return 0;
}



s32 KNetController::RemoveSession(KNetHandshakeKey hkey, u64 session_id)
{
	KNetSession* session = NULL;
	if (true)
	{
		auto iter = handshakes_.find(hkey);
		if (iter != handshakes_.end())
		{
			session = iter->second;
			handshakes_.erase(iter);
		}
	}
	if (true)
	{
		auto iter = establisheds_.find(session_id);
		if (iter != establisheds_.end())
		{
			if (session != NULL)
			{
				KNetEnv::Errors()++;
			}
			session = iter->second;
			establisheds_.erase(iter);
		}
	}
	return RemoveSession(session);
}




s32 KNetController::RemoveSession(KNetSession* session)
{
	if (session == NULL)
	{
		return 0;
	}
	
	for (auto s: session->slots_)
	{
		if (s.skt_id_ < 0 || s.skt_id_ >= (u32)nss_.size())
		{
			KNetEnv::Errors()++;
			continue;
		}
		KNetSocket& skt = nss_[s.skt_id_];
		if (skt.skt_id_ != s.skt_id_)
		{
			KNetEnv::Errors()++;
			continue;
		}
		if (skt.refs_ <= 0)
		{
			KNetEnv::Errors()++;
			continue;
		}
		skt.refs_--;
	}
	KNetEnv::Status(KNT_STT_SES_DESTROY_EVENTS)++;
	delete session;
	return 0;
}

s32 KNetController::StartServer(const KNetConfigs& configs)
{
	bool has_error = false;
	for (auto& c : configs)
	{
		KNetSocket* ns = PopFreeSocket();
		if (ns == NULL)
		{
			KNetEnv::Errors()++;
			continue;
		}

		s32 ret = ns->InitSocket(c.localhost.c_str(), c.localport, NULL, 0);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			has_error = true;
			KNetEnv::Errors()++;
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


KNetSocket* KNetController::PopFreeSocket()
{
	for (u32 i = 0; i < nss_.size(); i++)
	{
		KNetSocket& s = nss_[i];
		if (s.state_ == KNTS_INVALID)
		{
			KNetEnv::Status(KNT_STT_SKT_ALLOC_EVENTS)++;
			return &s;
		}
	}

	if (nss_.full())
	{
		return NULL;
	}
	nss_.emplace_back(nss_.size());
	KNetEnv::Status(KNT_STT_SKT_ALLOC_EVENTS)++;
	return &nss_.back();
}


void KNetController::PushFreeSocket(KNetSocket* s)
{
	if (s->state_ == KNTS_INVALID)
	{
		return;
	}
	KNetEnv::Status(KNT_STT_SKT_FREE_EVENTS)++;
	s->DestroySocket();

}













