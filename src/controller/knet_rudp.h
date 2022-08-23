
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



/* encode 32 bits unsigned int (lsb) */
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

/* decode 32 bits unsigned int (lsb) */
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




enum KNetCMD
{
	KNETCMD_INVALID = 0,
	KNETCMD_CH = 1,
	KNETCMD_SH = 2,
	KNETCMD_PSH = 3,
	KNETCMD_RST = 4,
	KNETCMD_ECHO = 4,
};


struct KNetUHDR
{
	u64 session_id;
	u64 pkt_id;
	u16 pkt_size;
	u16 version;
	u8 channel_id;
	u8 cmd;
	u16 flag;
	u64 mac;
};
static const u32 KNT_UHDR_SIZE = 8 * 4;
static_assert(sizeof(KNetUHDR) == KNT_UHDR_SIZE, "hdr size is 32");


static inline char* KNetEncodeUHDR(char* p, const KNetUHDR& hdr)
{
	p = ikcp_encode64u(p, hdr.session_id);
	p = ikcp_encode64u(p, hdr.pkt_id);
	p = ikcp_encode16u(p, hdr.pkt_size);
	p = ikcp_encode16u(p, hdr.version);
	p = ikcp_encode8u(p, hdr.channel_id);
	p = ikcp_encode8u(p, hdr.cmd);
	p = ikcp_encode16u(p, hdr.flag);
	p = ikcp_encode64u(p, hdr.mac);
	return p;
}

static inline const char* KNetDecodeUHDR(const char* p, KNetUHDR& hdr)
{
	p = ikcp_decode64u(p, &hdr.session_id);
	p = ikcp_decode64u(p, &hdr.pkt_id);
	p = ikcp_decode16u(p, &hdr.pkt_size);
	p = ikcp_decode16u(p, &hdr.version);
	p = ikcp_decode8u(p, &hdr.channel_id);
	p = ikcp_decode8u(p, &hdr.cmd);
	p = ikcp_decode16u(p, &hdr.flag);
	p = ikcp_decode64u(p, &hdr.mac);
	return p;
}


static inline u64 KNetHSMac(const char* data, s32 len,  KNetUHDR& hdr)
{
	u64 mac = hdr.session_id;
	static const u64 h = (0x84222325ULL << 32) | 0xcbf29ce4ULL;
	static const u64 kPrime = (0x00000100ULL << 32) | 0x000001b3ULL;
	mac ^= h;
	mac *= kPrime;
	mac ^= hdr.pkt_id;
	mac ^= hdr.pkt_size;
	mac ^= hdr.version;
	mac ^= hdr.channel_id;
	mac ^= hdr.cmd;
	mac ^= hdr.flag;

	const unsigned char* p = (const unsigned char*)data;
	const unsigned char* pend = (const unsigned char*)data + len;
	unsigned char* d = (unsigned char*)&mac;
	int i = 0;
	while (p != pend)
	{
		i = i % 8;
		*(d+i) ^= *p++;
	}
	return mac;
}


static inline u64 KNetPSMac(const char* data, s32 len, const char* box, KNetUHDR& hdr)
{
	(void)box;
	return KNetHSMac(data, len, hdr);
}







#endif


