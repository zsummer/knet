
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
#ifndef _KNET_ADDR_H_
#define _KNET_ADDR_H_
#include "knet_base.h"
#include <chrono>
#include "knet_env.h"

static constexpr s32 KNET_READABLE_ADDR_LEN = INET6_ADDRSTRLEN + 10;

struct KNetAddress
{
    union
    {
        sockaddr_in6 in6;
        sockaddr_in in4;
        sockaddr in;
    } real_addr_;
    char debug_string_[KNET_READABLE_ADDR_LEN];
    const char* debug_string() const { return debug_string_; }

    KNetAddress()
    {
        real_addr_.in.sa_family = AF_UNSPEC;
        debug_string_[0] = '\0';
    }

    s32 family() const { return real_addr_.in.sa_family; }
    u16 port() const { return family() == AF_INET6 ? ntohs(real_addr_.in6.sin6_port) : ntohs(real_addr_.in4.sin_port); }
    s32 sockaddr_len() const { return family() == AF_INET6 ? sizeof(real_addr_.in6) : sizeof(real_addr_.in4); }


    sockaddr* sockaddr_ptr() { return &real_addr_.in; }
    sockaddr& sockaddr_ref() { return real_addr_.in; }
    sockaddr_in* sockaddr_in_ptr() { return &real_addr_.in4; }
    sockaddr_in& sockaddr_in_ref() { return real_addr_.in4; }
    sockaddr_in6* sockaddr_in6_ptr() { return &real_addr_.in6; }
    sockaddr_in6& sockaddr_in6_ref() { return real_addr_.in6; }




    s32 reset_v4()
    {
        real_addr_.in4.sin_addr.s_addr = INADDR_ANY;
        real_addr_.in4.sin_family = AF_INET;
        format_v4();
        return 0;
    }
    s32 reset_v6()
    {
        real_addr_.in6.sin6_addr = IN6ADDR_ANY_INIT;
        real_addr_.in6.sin6_family = AF_INET6;
        format_v6();
        return 0;
    }

    s32 reset(s32 family)
    {
        if (family == AF_INET)
        {
            return reset_v4();
        }
        else
        {
            return reset_v6();
        }
        return 0;
    }



    s32 reset(const sockaddr& addr)
    {
        if (addr.sa_family == AF_INET)
        {
            real_addr_.in = addr;
        }
        else if (addr.sa_family == AF_INET6)
        {
            real_addr_.in6 = *(sockaddr_in6*)&addr;
        }
        format();
        //other not support 
        return 0;
    }

    u32 check_family(const char* ip)
    {
        u32 result = AF_INET;
        while (ip != NULL && *ip != '\0')
        {
            if (*ip == ':')
            {
                result = AF_INET6;
            }
            if (!std::isxdigit(*ip) && !std::isblank(*ip) && *ip != '.' && *ip !=':')
            {
                return AF_UNSPEC;
            }
            ip++;
        }
        return result;
    }

    void clean_address()
    {
        memset(&real_addr_, 0, sizeof(real_addr_));
    }

    s32 reset_ip_v4(const char* ip)
    {
        s32 ret = inet_pton(AF_INET, ip, &real_addr_.in4.sin_addr);
        if (ret < 0)
        {
            return KNetEnv::error_code();
        }
        return 0;
    }
    s32 reset_ip_v6(const char* ip)
    {
        s32 ret = inet_pton(AF_INET6, ip, &real_addr_.in6.sin6_addr);
        if (ret < 0)
        {
            return KNetEnv::error_code();
        }
        return 0;
    }
    s32 reset_port_v4(u16 port)
    {
        real_addr_.in4.sin_port = htons((u16)port);
        return 0;
    }
    s32 reset_port_v6(u16 port)
    {
        real_addr_.in6.sin6_port = htons((u16)port);
        return 0;
    }


    s32 reset(const char * ip, u16 port)
    {
        u32 family = check_family(ip);
        if (family == AF_UNSPEC)
        {
            return -1;
        }
        clean_address();
        if (family == AF_INET)
        {
            real_addr_.in4.sin_family = family;
            reset_ip_v4(ip);
            reset_port_v4(port);
            format_v4();
        }
        else if (family == AF_INET6)
        {
            real_addr_.in6.sin6_family = family;
            reset_ip_v6(ip);
            reset_port_v6(port);
            format_v6();
        }
        return 0;
    }

    s32 reset_from_socket(SOCKET s)
    {
        int len = sizeof(real_addr_);
        int ret = getsockname(s, &real_addr_.in, &len);
        format();
        return ret;
    }


    void format_v4()
    {
        debug_string_[0] = '\0';
        if (inet_ntop(family(), &real_addr_.in4.sin_addr, debug_string_, KNET_READABLE_ADDR_LEN - 1) != NULL)
        {
            char* p = &debug_string_[0];
            while (*p != '\0')
            {
                p++;
            }
            sprintf(p, ":%d", ntohs(real_addr_.in4.sin_port));
        }
    }


    void format_v6()
    {
        debug_string_[0] = '\0';
        if (inet_ntop(family(), &real_addr_.in6.sin6_addr, debug_string_, KNET_READABLE_ADDR_LEN - 1) != NULL)
        {
            char* p = &debug_string_[0];
            while (*p != '\0')
            {
                p++;
            }
            sprintf(p, ":%d", ntohs(real_addr_.in6.sin6_port));
        }
    }

    void format()
    {
        if (family() == AF_INET)
        {
            format_v4();
        }
        else if (family() == AF_INET6)
        {
            format_v6();
        }
    }



    void fill_cmsghdr(cmsghdr* cmsg) const
    {
#if defined(__linux__) || defined(__ANDROID__)
        if (family() == AF_INET4)
        {
            cmsg->cmsg_level = IPPROTO_IP;
            cmsg->cmsg_type = IP_PKTINFO;
            cmsg->cmsg_len = CMSG_LEN(sizeof(in_pktinfo));
            auto pkt_info = (in_pktinfo*)CMSG_DATA(cmsg);
            ::memset(pkt_info, 0, cmsg->cmsg_len);
            pkt_info->ipi_spec_dst = real_addr_.in4.in_addr_;
        }
        else if (family() == AF_INET6)
        {
            cmsg->cmsg_level = IPPROTO_IPV6;
            cmsg->cmsg_type = IPV6_PKTINFO;
            cmsg->cmsg_len = CMSG_LEN(sizeof(in6_pktinfo));
            auto pkt_info = (in6_pktinfo*)CMSG_DATA(cmsg);
            ::memset(pkt_info, 0, cmsg->cmsg_len);
            pkt_info->ipi6_addr = real_addr_.in6.in6_addr_;
        }
#endif
    }
};




#endif



