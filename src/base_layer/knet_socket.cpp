
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
    memset(this, 0, sizeof(*this));
    inst_id_ = inst_id;
    skt_ = INVALID_SOCKET;
}

KNetSocket::~KNetSocket()
{
    if (skt_ != INVALID_SOCKET)
    {
        KNetEnv::error_count()++;
    }
}


s32 KNetSocket::init(const char* localhost, u16 localport, const char* remote_ip, u16 remote_port)
{
    if (skt_ != INVALID_SOCKET)
    {
        return -1;
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

    skt_ = socket(local_.family(), SOCK_DGRAM, 0);
    if (skt_ == INVALID_SOCKET)
    {
        return -5;
    }






    

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




    KNetEnv::call_user(KNTP_SKT_INIT);
    local_.reset_from_socket(skt_);
    //LogInfo() << "bind local:" << local_.debug_string();
    //LogInfo() << *this;
    return 0;
}



s32 KNetSocket::send_packet(const char* pkg_data, s32 len, KNetAddress& remote)
{
    KNetEnv::call_mem(KNTP_SKT_SEND, len);
    probe_snd_cnt_++;
    probe_snd_bytes_ += len;
    last_send_ts_ = KNetEnv::now_ms();
    if (debug_send_lost_ > 0)
    {
        u32 rd = (u32)rand() % 100;
        if (rd < debug_send_lost_)
        {
            return 0;
        }
    }
    s32 ret = sendto(skt_, pkg_data, len, 0, remote.sockaddr_ptr(), remote.sockaddr_len());
    if (ret == SOCKET_ERROR)
    {
        return -1;
    }
    return 0;
}

s32 KNetSocket::recv_packet(char* buf, s32& len, KNetAddress& remote, s64 now_ms)
{
    int ret = 0;
    do 
    {
        socklen_t addr_len = sizeof(remote.real_addr_.in6);
        ret = recvfrom(skt_, buf, len, 0, (sockaddr*)&remote.real_addr_.in6, &addr_len);
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
            //LogError() << "error:" << KNetEnv::error_code();
            return -1;
        }
        if (debug_recv_lost_ > 0)
        {
            u32 rd = (u32)rand() % 100;
            if (rd < debug_recv_lost_)
            {
                continue;
            }
        }
    } while (0);
    len = ret;
    KNetEnv::call_mem(KNTP_SKT_RECV, ret);
    probe_rcv_cnt_++;
    probe_rcv_bytes_ += ret;
    last_recv_ts_ = now_ms;
    return 0;
}


s32 KNetSocket::destroy()
{
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

    KNetEnv::call_user(KNTP_SKT_DESTROY);
    return 0;
}








s32 KNetSocket::set_skt_recv_buffer(s32 size)
{
#ifdef _WIN32
    s32 ret = setsockopt(skt_, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size));
#else
    s32 ret = setsockopt(skt_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
#endif
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}

s32 KNetSocket::set_skt_send_buffer(s32 size)
{
#ifdef _WIN32
    s32 ret = setsockopt(skt_, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size));
#else
    s32 ret = setsockopt(skt_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
#endif
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}







