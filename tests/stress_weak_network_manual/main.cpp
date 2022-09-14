

#include "knet_base.h"
#include "knet_socket.h"
#include "knet_turbo.h"
#include <memory>
KNetEnv env_init_;

/*
#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	KNetEnv::serialize(); \
	return -1;\
}
*/
void KNetAssert(bool assert_expr, const char* s)
{
	if (!assert_expr)
	{
		LogError() << s;
		KNetEnv::serialize(); 
		exit(-1);
	}
}


s32 test_session_server(u32 lost1, u32 lost2, u32 interval, u32 keep)
{
	LogInfo() << "";
	LogInfo() << "============================================================================================";
	LogInfo() << "stress boot server:   lost1 : " << lost1 << "%, lost2 : " << lost2 ;
	LogInfo() << "============================================================================================";

	KNetEnv::clean_prof();
	KNetEnv::error_count() = 0;

	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 ,0, 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0  ,0, 0 });

	std::shared_ptr< KNetTurbo> turbos[10];
	for (s32 i = 0; i < 10; i++)
	{
		turbos[i] = std::make_shared<KNetTurbo>();
	}

	KNetTurbo& server_c = *turbos[0];
	s32 ret = 0;

    LogInfo() << "start server";
    ret = server_c.start_server(mc);
    KNetAssert(ret == 0, "");



	KNetOnData on_data = [&](KNetTurbo& turbo, KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
	{
		turbo.send_data(s, 0, data, len, KNetEnv::now_ms());
	};

	KNetOnDisconnect on_disconnect = [&](KNetTurbo& turbo, KNetSession& session, bool passive)
	{
		LogDebug() << "close session:" << session.session_id_ << ", is server:" << session.is_server() << ", is passive:" << passive;
	};


	for (s32 i = 0; i < 10; i++)
	{
		turbos[i]->set_on_data(on_data);
		turbos[i]->set_on_disconnect(on_disconnect);
	}


	s64 now = KNetEnv::now_ms();



	now = KNetEnv::now_ms();
	for (s32 i = 0; ; i++)
	{
		for (s32 id = 0; id < 10; id++)
		{
			ret = turbos[id]->do_tick();
			KNetAssert(ret == 0, "");
		}
		if (i % (200) == 0)
		{
			LogInfo() << "now loop:" << i ;
            for (s32 i = 0; i < 10; i++)
            {
                LogInfo() << "turbo[" << i << "] established session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
                LogInfo() << "turbo[" << i << "] established socket:" << turbos[i]->get_socket_count_by_state(KNTS_ESTABLISHED);
            }
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		if (KNetEnv::now_ms() - now > keep )
		{
			break;
		}
	}
	s64 finish_now = KNetEnv::now_ms();

	for (s32 id = 0; id < 10; id++)
	{
		turbos[id]->set_on_data(NULL);
	}
	for (s32 i = 0; i<10; i++)
	{
		for (s32 id = 0; id < 10; id++)
		{
			ret = turbos[id]->do_tick();
			KNetAssert(ret == 0, "");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}


	LogInfo() << "";
	LogInfo() << "==========================================================";
    for (s32 i = 0; i < 10; i++)
    {
        LogInfo() << "turbo[" << i << "] established session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
        LogInfo() << "turbo[" << i << "] established socket:" << turbos[i]->get_socket_count_by_state(KNTS_ESTABLISHED);
    }

	KNetEnv::serialize();
	


	for (s32 i = 0; i < 10; i++)
	{
		turbos[i]->show_info();
		ret = turbos[i]->stop();
		KNetAssert(ret == 0, "");
	}

	for (s32 i = 0; i < 10; i++)
	{
		auto& turbo = *turbos[i];

		for (auto& s : turbo.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : turbo.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}


	
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::user_count(KNTP_SKT_ALLOC) == KNetEnv::user_count(KNTP_SKT_FREE), "");
	KNetAssert(KNetEnv::user_count(KNTP_SES_ALLOC) == KNetEnv::user_count(KNTP_SES_FREE), "");
	KNetEnv::serialize();
	LogInfo() << "finish.";
	LogInfo() << "";
	return 0;
}


s32 test_session_connect(s32 session_count, bool double_stream, u32 lost1, u32 lost2, s32 send_times, s32 interval, s32 keep, s32 data_len)
{
    LogInfo() << "";
    LogInfo() << "============================================================================================";
    LogInfo() << "stress boot client cnt:" << session_count << "  double_stream : " << double_stream << "  lost1 : " << lost1 << "%, lost2 : " << lost2 << " % send_times : " << send_times << "  interval : " << interval << "ms keep : " << keep / 1000 << "s  one data : " << data_len;
    LogInfo() << "============================================================================================";
    if (send_times == 0)
    {
        send_times = 1;
    }
    KNetEnv::clean_prof();
    KNetEnv::error_count() = 0;

    KNetConfigs mc;
    mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 ,0, 0 });
    mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0  ,0, 0 });

    std::shared_ptr< KNetTurbo> turbos[10];
    for (s32 i = 0; i < 10; i++)
    {
        turbos[i] = std::make_shared<KNetTurbo>();
    }

    KNetTurbo& server_c = *turbos[0];
    s32 ret = 0;

    mc.clear();
    mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870, lost1, lost1 });
    if (double_stream)
    {
        mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 , lost2, lost2 });
    }


    KNetOnConnect on_connect = [&](KNetTurbo& turbo, KNetSession& session, bool connected, u16 state, s64 time_out)
    {
        if (connected)
        {
            LogDebug() << "connected";
        }
        else
        {
            LogError() << "connect error: last state:" << state << ", time_out:" << time_out;
            return;
        }
    };


    KNetOnData on_data = [&](KNetTurbo& turbo, KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
    {
        turbo.send_data(s, 0, data, len, KNetEnv::now_ms());
    };

    KNetOnDisconnect on_disconnect = [&](KNetTurbo& turbo, KNetSession& session, bool passive)
    {
        LogDebug() << "close session:" << session.session_id_ << ", is server:" << session.is_server() << ", is passive:" << passive;
    };


    for (s32 i = 0; i < 10; i++)
    {
        turbos[i]->set_on_data(on_data);
        turbos[i]->set_on_disconnect(on_disconnect);
    }

    for (s32 i = 0; i < session_count; i++)
    {
        KNetSession* session = NULL;
        std::shared_ptr< KNetTurbo> pc = turbos[i % 10];
        KNetTurbo& turbo = *pc;
        ret = turbo.create_connect(mc, session);
        if (ret != 0)
        {
            //volatile int aa = 0;
        }
        KNetAssert(ret == 0, "");
        KNetAssert(session != NULL, "");

        ret = turbo.start_connect(*session, on_connect, 5000);
        if (ret != 0)
        {
            //volatile int aa = 0;
        }
        KNetAssert(ret == 0, "");
        KNetAssert(KNetEnv::error_count() == 0, "");
        for (s32 k = 0; k < 10; k++)
        {
            for (s32 id = 0; id < 10; id++)
            {
                ret = turbos[id]->do_tick();
                KNetAssert(ret == 0, "");
            }
        }

        if (i % 20 == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }

    }

    for (s32 i = 0; i < 10; i++)
    {
        LogInfo() << "turbo[" << i << "] connected session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
    }

    s64 now = KNetEnv::now_ms();
    for (s32 i = 0; ; i++)
    {
        for (s32 id = 0; id < 10; id++)
        {
            ret = turbos[id]->do_tick();
            KNetAssert(ret == 0, "");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        if (KNetEnv::user_count(KNTP_SES_CONNECT) == KNetEnv::user_count(KNTP_SES_ON_CONNECTED))
        {
            break;
        }
        if (KNetEnv::now_ms() - now > 10 * 1000)
        {
            KNetEnv::serialize();
            KNetAssert(false, "waiting session connected fail");
        }

    }


    if (KNetEnv::user_count(KNTP_SES_CONNECT) != KNetEnv::user_count(KNTP_SES_ON_CONNECTED))
    {
        KNetAssert(false, "");
    }

    LogInfo() << "all connected:";
    KNetEnv::serialize();

    LogInfo() << "begin first echo per connect";
    for (s32 id = 0; id < 10; id++)
    {
        for (auto& s : turbos[id]->sessions())
        {
            if (s.state_ == KNTS_ESTABLISHED)
            {
                static char buf[10000] = { 0 };
                if (buf[0] == 0)
                {
                    for (s32 i = 0; i < 10000; i++)
                    {
                        buf[i] = 'a';
                    }
                }

                for (s32 i = 0; i < send_times; i++)
                {
                    turbos[id]->send_data(s, 0, buf, data_len, KNetEnv::now_ms());
                }
            }
        }
    }

    LogInfo() << "all connect begin pingpong";


    now = KNetEnv::now_ms();
    for (s32 i = 0; ; i++)
    {
        for (s32 id = 0; id < 10; id++)
        {
            ret = turbos[id]->do_tick();
            KNetAssert(ret == 0, "");
        }
        if (i % (200) == 0)
        {
            LogInfo() << "now loop:" << i;
            for (s32 i = 0; i < 10; i++)
            {
                LogInfo() << "turbo[" << i << "] established session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
                LogInfo() << "turbo[" << i << "] established socket:" << turbos[i]->get_socket_count_by_state(KNTS_ESTABLISHED);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        if (KNetEnv::now_ms() - now > keep)
        {
            break;
        }
    }
    s64 finish_now = KNetEnv::now_ms();

    for (s32 id = 0; id < 10; id++)
    {
        turbos[id]->set_on_data(NULL);
    }
    for (s32 i = 0; i < 10; i++)
    {
        for (s32 id = 0; id < 10; id++)
        {
            ret = turbos[id]->do_tick();
            KNetAssert(ret == 0, "");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }


    LogInfo() << "";
    LogInfo() << "==========================================================";
    LogInfo() << "session_count:" << session_count << ", double_stream:" << double_stream << ", interval:" << interval;
    for (s32 i = 0; i < 10; i++)
    {
        LogInfo() << "turbo[" << i << "] established session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
        LogInfo() << "turbo[" << i << "] established socket:" << turbos[i]->get_socket_count_by_state(KNTS_ESTABLISHED);
    }
    LogInfo() << "skt packet per secod:" << (KNetEnv::mem_count(KNTP_SKT_SEND) + KNetEnv::mem_count(KNTP_SKT_RECV)) / (1.0 * keep / 1000.0);
    LogInfo() << "skt bytes per secod:" << (KNetEnv::mem_bytes(KNTP_SKT_SEND) + KNetEnv::mem_bytes(KNTP_SKT_RECV)) / (1.0 * keep / 1000.0) / 1024.0 / 1024.0 << "m";
    LogInfo() << "ses packet per secod:" << (KNetEnv::mem_count(KNTP_SES_SEND) + KNetEnv::mem_count(KNTP_SES_RECV)) / (1.0 * keep / 1000.0);
    LogInfo() << "ses bytes per secod:" << (KNetEnv::mem_bytes(KNTP_SES_SEND) + KNetEnv::mem_bytes(KNTP_SES_RECV)) / (1.0 * keep / 1000.0) / 1024.0 / 1024.0 << "m";


    KNetEnv::serialize();



    for (s32 i = 0; i < 10; i++)
    {
        turbos[i]->show_info();
        ret = turbos[i]->stop();
        KNetAssert(ret == 0, "");
    }

    for (s32 i = 0; i < 10; i++)
    {
        auto& turbo = *turbos[i];

        for (auto& s : turbo.sessions())
        {
            KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
        }
        for (auto& s : turbo.nss())
        {
            KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
        }
    }



    KNetAssert(KNetEnv::error_count() == 0, "");
    KNetAssert(KNetEnv::user_count(KNTP_SKT_ALLOC) == KNetEnv::user_count(KNTP_SKT_FREE), "");
    KNetAssert(KNetEnv::user_count(KNTP_SES_ALLOC) == KNetEnv::user_count(KNTP_SES_FREE), "");
    KNetEnv::serialize();
    LogInfo() << "finish.";
    LogInfo() << "";
    return 0;
}
int main(int argc, char* argv[])
{
	FNLog::FastStartDebugLogger();
	FNLog::BatchSetChannelConfig(FNLog::GetDefaultLogger(), FNLog::CHANNEL_CFG_PRIORITY, FNLog::PRIORITY_INFO);
	LogInfo() << "start up";
	s32 sessions = 5;
	u32 mod = 3;
	LogInfo() << "input mod[1 server 2 client 3mix]  connects[num]";
	if (argc > 2)
	{
        mod = atoi(argv[1]);
		sessions = atoi(argv[2]);
	}
	if (mod == 1)
	{
		KNetAssert(test_session_server(15, 15, 10, 20000) == 0, "");
	}
	else
    {
        KNetAssert(test_session_connect(sessions, true, 15, 15, 1, 10, 20000, 8000) == 0, "");
	}

	return 0;
}

