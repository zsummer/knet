
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

#include "knet_socket.h"


KNetSocket::KNetSocket(s32 inst_id)
{
    KNetEnv::prof(KNT_STT_SKT_INSTRUCT_EVENTS)++;
    inst_id_ = inst_id;
    reset();
    //LogDebug() << *this;
}
KNetSocket::~KNetSocket()
{
    KNetEnv::prof(KNT_STT_SKT_DESTRUCT_EVENTS)++;
    if (state_ != KNTS_INVALID || skt_ != INVALID_SOCKET)
    {
        KNetEnv::error_count()++;
    }
    //LogDebug() << *this;
}





s32 KNetSocket::init(const char* localhost, u16 localport, const char* remote_ip, u16 remote_port)
{
    if (state_ != KNTS_INVALID)
    {
        return -2;
    }
    s32 ret = local_.reset(localhost, localport);
    if (ret != 0)
    {
        return -3;
    }
    if (remote_ip != NULL)
    {
        ret = remote_.reset(remote_ip, remote_port);
        if (ret != 0)
        {
            return -4;
        }
    }

    last_active_ = KNetEnv::now_ms();
    skt_ = socket(local_.family(), SOCK_DGRAM, 0);
    if (skt_ == INVALID_SOCKET)
    {
        return -5;
    }
    KNetEnv::prof(KNT_STT_SKT_INIT_EVENTS)++;
    state_ = KNTS_INIT;
    ret = bind(skt_, local_.sockaddr_ptr(), local_.sockaddr_len());
    if (ret != 0)
    {
        destroy();
        return -6;
    }


#ifdef WIN32
    unsigned long val = 1;
    ioctlsocket(skt_, FIONBIO, &val);

    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;

    u32 SIO_UDP_CONNRESET = IOC_IN | IOC_VENDOR | 12;
    WSAIoctl(skt_, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError),
        NULL, 0, &dwBytesReturned, NULL, NULL);

#else
    fcntl((skt_), F_SETFL, fcntl(skt_, F_GETFL) | O_NONBLOCK);
#endif // WIN32



    local_.reset_from_socket(skt_);
    state_ = KNTS_BINDED;
    KNetEnv::prof(KNT_STT_SKT_BIND_EVENTS)++;
    LogInfo() << "bind local:" << local_.debug_string();
    //LogInfo() << *this;
    return 0;
}



s32 KNetSocket::send_pkt(const char* pkg_data, s32 len, KNetAddress& remote)
{
    KNetEnv::prof(KNT_STT_SKT_SND_EVENTS)++;
    KNetEnv::prof(KNT_STT_SKT_SND_BYTES) += len;
    s32 ret = sendto(skt_, pkg_data, len, 0, remote.sockaddr_ptr(), remote.sockaddr_len());
    if (ret == SOCKET_ERROR)
    {
        return -1;
    }
    return 0;
}

s32 KNetSocket::recv_pkt(char* buf, s32& len, KNetAddress& remote, s64 now_ms)
{
    socklen_t addr_len = sizeof(remote.real_addr_.in6);
    int ret = recvfrom(skt_, buf, len, 0, (sockaddr*)&remote.real_addr_.in6, &addr_len);
    if (ret == 0)
    {
        len = 0;
        return 0;
    }
#ifdef WIN32
    if (ret < 0 && KNetEnv::error_code() == WSAEWOULDBLOCK)
    {
        len = 0;
        return 0;
    }
#else
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        len = 0;
        return 0;
    }
#endif // WIN32
    if (ret < 0)
    {
        len = 0;
        LogError() << "error:" << KNetEnv::error_code();
        return -1;
    }

    len = ret;
    KNetEnv::prof(KNT_STT_SKT_RCV_EVENTS)++;
    KNetEnv::prof(KNT_STT_SKT_RCV_BYTES) += ret;
    last_active_ = now_ms;
    return 0;
}


s32 KNetSocket::destroy()
{
    if (state_ == KNTS_INVALID)
    {
        return 0;
    }

    if (refs_ > 0 && !(flag_ & KNTF_SERVER))
    {
        KNetEnv::error_count()++;
        return -2;
    }

    if (refs_ > 1 && (flag_ & KNTF_SERVER))
    {
        KNetEnv::error_count()++;
        return -3;
    }

    if (refs_ < 0)
    {
        KNetEnv::error_count()++;
        return -4;
    }
    KNetEnv::prof(KNT_STT_SKT_DESTROY_EVENTS)++;
    //LogInfo() << *this;

    if (skt_ != INVALID_SOCKET)
    {
#ifdef _WIN32
        closesocket(skt_);
#else
        close(skt_);
#endif
        skt_ = INVALID_SOCKET;
    }

    state_ = KNTS_INVALID;
    flag_ = KNTF_NONE;
    refs_ = 0;
    last_active_ = KNetEnv::now_ms();
    return 0;
}


FNLog::LogStream& operator <<(FNLog::LogStream& ls, const KNetSocket& s)
{
    if (s.flag_ & KNTF_SERVER)
    {
        ls << "server: inst_id:" << s.inst_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)(u64)s.flag_ << ", refs:" << s.refs_ << ", local:" << s.local_.debug_string();
    }
    else if (s.flag_ & KNTF_CLINET)
    {
        ls << "client: inst_id:" << s.inst_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)(u64)s.flag_ << ", refs:" << s.refs_ << ", local:" << s.local_.debug_string() << ", remote:" << s.remote_.debug_string();
    }
    else
    {
        ls << "none: inst_id:" << s.inst_id_ << ", skt:" << s.skt_ << ", state:" << s.state_ << ", flag:" << (void*)(u64)s.flag_ << ", refs:" << s.refs_ << ", local:" << s.local_.debug_string() << ", remote:" << s.remote_.debug_string();
    }
    return ls;
}

















