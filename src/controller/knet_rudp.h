
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
#ifndef _KNET_RUDP_H_
#define _KNET_RUDP_H_
#include "knet_base.h"
#include "knet_env.h"
#include "ikcp.h"
#include <chrono>



static inline char* ikcp_encode64u(char* p, IUINT64 l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	* (unsigned char*)(p + 0) = (unsigned char)((l >> 0) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >> 8) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >> 24) & 0xff);
	*(unsigned char*)(p + 4) = (unsigned char)((l >> 32) & 0xff);
	*(unsigned char*)(p + 5) = (unsigned char)((l >> 40) & 0xff);
	*(unsigned char*)(p + 6) = (unsigned char)((l >> 48) & 0xff);
	*(unsigned char*)(p + 7) = (unsigned char)((l >> 56) & 0xff);
#else
	memcpy(p, &l, 8);
#endif
	p += 8;
	return p;
}


static inline const char* ikcp_decode64u(const char* p, IUINT64* l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	* l = *(const unsigned char*)(p + 7);
	*l = *(const unsigned char*)(p + 6) + (*l << 8);
	*l = *(const unsigned char*)(p + 5) + (*l << 8);
	*l = *(const unsigned char*)(p + 4) + (*l << 8);
	*l = *(const unsigned char*)(p + 3) + (*l << 8);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 0) + (*l << 8);
#else 
	memcpy(l, p, 8);
#endif
	p += 8;
	return p;
}


static inline char* ikcp_encode64s(char* p, s64 l)
{
	return ikcp_encode64u(p, (IUINT64)l);
}


static inline const char* ikcp_decode64s(const char* p, s64* l)
{
	return ikcp_decode64u(p, (IUINT64*)l);
}




static inline char* ikcp_encode_str(char* p, const char* src, s32 src_len)
{
	memcpy(p, src, src_len);
	p += src_len;
	return p;
}


static inline const char* ikcp_decode_str(const char* p, char* src, s32 src_len)
{
	memcpy(src, p, src_len);
	p += src_len;
	return p;
}




enum KNetCMD
{
	KNETCMD_INVALID = 0,
	KNETCMD_PB = 1,
	KNETCMD_PB_ACK = 2,
	KNETCMD_CH = 3,
	KNETCMD_SH = 4,
	KNETCMD_PSH = 5,
	KNETCMD_RST = 6,
	KNETCMD_ECHO = 7,
};

const static u32 KNT_MAX_SLOTS = 8;

struct KNetHeader
{
	static const s32 HDR_SIZE = 8 * 4;
	u64 session_id;
	u64 pkt_id;
	u16 pkt_size;
	u16 version;
	u8 chl;
	u8 cmd;
	u8 flag;
	u8 slot;
	u64 mac;

	void reset() { memset(this, 0, sizeof(*this)); }
};
static_assert(sizeof(KNetHeader) == KNetHeader::HDR_SIZE, "hdr size is 32");
static_assert(sizeof(KNetHeader) % 8 == 0, "");

static const s32 KNT_UHDR_SIZE = KNetHeader::HDR_SIZE;
static const s32 KNT_UDAT_SIZE = KNT_UPKT_SIZE - KNT_UHDR_SIZE - (KNT_UPKT_SIZE - KNT_UHDR_SIZE)%16;

static inline char* knet_encode_hdr(char* p, const KNetHeader& hdr)
{
	p = ikcp_encode64u(p, hdr.session_id);
	p = ikcp_encode64u(p, hdr.pkt_id);
	p = ikcp_encode16u(p, hdr.pkt_size);
	p = ikcp_encode16u(p, hdr.version);
	p = ikcp_encode8u(p, hdr.chl);
	p = ikcp_encode8u(p, hdr.cmd);
	p = ikcp_encode8u(p, hdr.flag);
	p = ikcp_encode8u(p, hdr.slot);
	p = ikcp_encode64u(p, hdr.mac);
	return p;
}

static inline const char* knet_decode_hdr(const char* p, KNetHeader& hdr)
{
	p = ikcp_decode64u(p, &hdr.session_id);
	p = ikcp_decode64u(p, &hdr.pkt_id);
	p = ikcp_decode16u(p, &hdr.pkt_size);
	p = ikcp_decode16u(p, &hdr.version);
	p = ikcp_decode8u(p, &hdr.chl);
	p = ikcp_decode8u(p, &hdr.cmd);
	p = ikcp_decode8u(p, &hdr.flag);
	p = ikcp_decode8u(p, &hdr.slot);
	p = ikcp_decode64u(p, &hdr.mac);
	return p;
}


static inline u64 knet_encode_pkt_mac(const char* data, s32 len,  KNetHeader& hdr, const char* box = NULL)
{
	u64 mac = hdr.session_id;
	static const u64 h = (0x84222325ULL << 32) | 0xcbf29ce4ULL;
	static const u64 kPrime = (0x00000100ULL << 32) | 0x000001b3ULL;
	mac ^= h;
	mac *= kPrime;
	mac ^= hdr.pkt_id;
	mac *= kPrime;
	mac ^= (u64)hdr.pkt_size << 48;
	mac ^= (u64)hdr.version << 32;
	mac ^= (u64)hdr.chl << 24;
	mac ^= (u64)hdr.cmd << 16;
	mac ^= (u64)hdr.flag << 8;
	mac *= kPrime;

	const unsigned char* p = (const unsigned char*)data;
	const unsigned char* pend = (const unsigned char*)data + len;
	unsigned char* d = (unsigned char*)&mac;
	int i = 0;
	while (p != pend)
	{
		i = i % 8;
		*(d+i) ^= *p++;
	}
	mac *= kPrime;
	return mac;
}


















static inline char* KNetEncodeKNetDeviceInfo(char* p, const KNetDeviceInfo& dvi)
{
	p = ikcp_encode_str(p, dvi.device_name, 128);
	p = ikcp_encode_str(p, dvi.device_type, 128);
	p = ikcp_encode_str(p, dvi.device_mac, 128);
	p = ikcp_encode_str(p, dvi.sys_name, 128);
	p = ikcp_encode_str(p, dvi.sys_version, 128);
	p = ikcp_encode_str(p, dvi.os_name, 128);
	static_assert(128 * 6 == KNetDeviceInfo::PKT_SIZE, "sync change this.");
	return p;
}

static inline const char* KNetDecodeKNetDeviceInfo(const char* p, KNetDeviceInfo& dvi)
{
	p = ikcp_decode_str(p, dvi.device_name, 128);
	p = ikcp_decode_str(p, dvi.device_type, 128);
	p = ikcp_decode_str(p, dvi.device_mac, 128);
	p = ikcp_decode_str(p, dvi.sys_name, 128);
	p = ikcp_decode_str(p, dvi.sys_version, 128);
	p = ikcp_decode_str(p, dvi.os_name, 128);
	static_assert(128 * 6 == KNetDeviceInfo::PKT_SIZE, "sync change this.");
	return p;
}





struct KNetProbe
{
	static const s32 PKT_SIZE = 8 + 8 + 8 + KNetDeviceInfo::PKT_SIZE;
	s64 client_ms;
	u64 client_seq_id;
	u64 shake_id;
	KNetDeviceInfo dvi;
};
static_assert(KNetProbe::PKT_SIZE < KNT_UDAT_SIZE&& KNetProbe::PKT_SIZE> 500, "");
static_assert(sizeof(KNetProbe) == KNetProbe::PKT_SIZE, "");


static inline char* knet_encode_packet(char* p, const KNetProbe& pkt)
{
	p = ikcp_encode64s(p, pkt.client_ms);
	p = ikcp_encode64u(p, pkt.client_seq_id);
	p = ikcp_encode64u(p, pkt.shake_id);
	p = KNetEncodeKNetDeviceInfo(p, pkt.dvi);
	return p;
}

static inline const char* knet_decode_packet(const char* p, KNetProbe& pkt)
{
	p = ikcp_decode64s(p, &pkt.client_ms);
	p = ikcp_decode64u(p, &pkt.client_seq_id);
	p = ikcp_decode64u(p, &pkt.shake_id);
	p = KNetDecodeKNetDeviceInfo(p, pkt.dvi);
	return p;
}

struct KNetProbeAck
{
	static const s32 PKT_SIZE = 8 * 4;
	s64 result;
	s64 client_ms;
	u64 client_seq_id;
	u64 shake_id;
};
static_assert(sizeof(KNetProbeAck) == KNetProbeAck::PKT_SIZE, "");


static inline char* knet_encode_packet(char* p, const KNetProbeAck& pkt)
{
	p = ikcp_encode64s(p, pkt.result);
	p = ikcp_encode64s(p, pkt.client_ms);
	p = ikcp_encode64u(p, pkt.client_seq_id);
	p = ikcp_encode64u(p, pkt.shake_id);
	return p;
}

static inline const char* knet_decode_packet(const char* p, KNetProbeAck& pkt)
{
	p = ikcp_decode64s(p, &pkt.result);
	p = ikcp_decode64s(p, &pkt.client_ms);
	p = ikcp_decode64u(p, &pkt.client_seq_id);
	p = ikcp_decode64u(p, &pkt.shake_id);
	return p;
}






struct KNetCH
{
	static const s32 PKT_SIZE = 8 + 8 + 16*2 + KNetDeviceInfo::PKT_SIZE + 400;
	u64 shake_id;
	u64 session_id;
	char cg[16];
	char cp[16];
	KNetDeviceInfo dvi;
	char noise[400];
};
static_assert(KNetCH::PKT_SIZE < KNT_UDAT_SIZE&& KNetCH::PKT_SIZE> 1200, "");
static_assert(sizeof(KNetCH) == KNetCH::PKT_SIZE, "");

static inline char* knet_encode_packet(char* p, const KNetCH& pkt)
{
	p = ikcp_encode64u(p, pkt.shake_id);
	p = ikcp_encode64u(p, pkt.session_id);
	p = ikcp_encode_str(p, pkt.cg, 16);
	p = ikcp_encode_str(p, pkt.cp, 16);
	p = KNetEncodeKNetDeviceInfo(p, pkt.dvi);
	p = ikcp_encode_str(p, pkt.noise, 400);
	return p;
}

static inline const char* knet_decode_packet(const char* p, KNetCH& pkt)
{
	p = ikcp_decode64u(p, &pkt.shake_id);
	p = ikcp_decode64u(p, &pkt.session_id);
	p = ikcp_decode_str(p, pkt.cg, 16);
	p = ikcp_decode_str(p, pkt.cp, 16);
	p = KNetDecodeKNetDeviceInfo(p, pkt.dvi);
	p = ikcp_decode_str(p, pkt.noise, 400);
	return p;
}




struct KNetSH
{
	static const s32 PKT_SIZE = 4+4+8+8+16*2;
	s32 result;
	s32 noise;
	u64 shake_id;
	u64 session_id;
	char sg[16];
	char sp[16];
};
static_assert(sizeof(KNetSH) == KNetSH::PKT_SIZE, "");

static inline char* knet_encode_packet(char* p, const KNetSH& pkt)
{
	p = ikcp_encode32u(p, pkt.result);
	p = ikcp_encode32u(p, pkt.noise);
	p = ikcp_encode64u(p, pkt.shake_id);
	p = ikcp_encode64u(p, pkt.session_id);
	p = ikcp_encode_str(p, pkt.sg, 16);
	p = ikcp_encode_str(p, pkt.sp, 16);
	return p;
}

static inline const char* knet_decode_packet(const char* p, KNetSH& pkt)
{
	p = ikcp_decode32u(p, (u32*)&pkt.result);
	p = ikcp_decode32u(p, (u32*)&pkt.noise);
	p = ikcp_decode64u(p, (u64*)&pkt.shake_id);
	p = ikcp_decode64u(p, (u64*)&pkt.session_id);
	p = ikcp_decode_str(p, pkt.sg, 16);
	p = ikcp_decode_str(p, pkt.sp, 16);
	return p;
}





using KNetOnConnect = std::function<void(KNetSession& session, bool connected, s32 error_code)>;




#endif


