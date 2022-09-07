

//#define KNET_MAX_SOCKETS 1000
#include "knet_base.h"
#include "knet_socket.h"
#include "knet_controller.h"
#include <memory>
KNetEnv env_init_;


#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	return -1;\
}


s32 test_session_connect_mix(s32 session_count, bool double_stream, s32 send_times, s32 interval, s32 keep,  s32 data_len)
{
	LogInfo() << "";
	LogInfo() << "============================================================================================";
	LogInfo() << "         stress: session:" << session_count <<"  double_stream:" << double_stream <<" send_times:" << send_times <<"  interval:" << interval <<"ms keep:" << keep/1000 <<"s  one data:" << data_len;
	LogInfo() << "============================================================================================";
	if (send_times == 0)
	{
		send_times = 1;
	}
	KNetEnv::clean_prof();
	KNetEnv::error_count() = 0;

	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });

	std::shared_ptr< KNetController> turbos[10];
	for (s32 i = 0; i < 10; i++)
	{
		turbos[i] = std::make_shared<KNetController>();
	}

	KNetController& server_c = *turbos[0];

	s32 ret = server_c.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870 });
	if (double_stream)
	{
		mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 });
	}


	KNetOnConnect on_connect = [&](KNetController& c, KNetSession& session, bool connected, u16 state, s64 time_out)
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


	KNetOnData on_data = [&](KNetController& c, KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
	{
		c.send_data(s, 0, data, len, KNetEnv::now_ms());
	};

	KNetOnDisconnect on_disconnect = [&](KNetController& c, KNetSession& session, bool passive)
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
		std::shared_ptr< KNetController> pc = turbos[i%10];
		KNetController& c = *pc;
		ret = c.create_connect(mc, session);
		if (ret != 0)
		{
			//volatile int aa = 0;
		}
		KNetAssert(ret == 0, "");
		KNetAssert(session != NULL, "");

		ret = c.start_connect(*session, on_connect, 5000);
		if (ret != 0)
		{
			//volatile int aa = 0;
		}
		KNetAssert(ret == 0, "");
		KNetAssert(KNetEnv::error_count() == 0, "");
		for (s32 i = 0; i < 10; i++)
		{
			for (s32 j = 0; j < 10; j++)
			{
				ret = turbos[i]->do_tick();
				KNetAssert(ret == 0, "");
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
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
		if (KNetEnv::user_count(KNTP_SES_CONNECT) == KNetEnv::user_count(KNTP_SES_ON_CONNECTED)
			&& KNetEnv::user_count(KNTP_SES_CONNECT) == KNetEnv::user_count(KNTP_SES_ON_ACCEPT))
		{
			break;
		}
		if (KNetEnv::now_ms() - now > 10 * 1000)
		{
			KNetEnv::serialize();
			KNetAssert(false, "connect timeout");
		}
		
	}


	if (KNetEnv::user_count(KNTP_SES_CONNECT) != KNetEnv::user_count(KNTP_SES_ON_CONNECTED)
		|| KNetEnv::user_count(KNTP_SES_CONNECT) != KNetEnv::user_count(KNTP_SES_ON_ACCEPT))
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
				static char buf[KNT_KCP_DATA_SIZE] = { 0 };
				if (buf[0] == 0)
				{
					for (s32 i = 0; i < KNT_KCP_DATA_SIZE; i++)
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
			LogInfo() << "now loop:" << i ;
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
	LogInfo() << "session_count:" << session_count << ", double_stream:" << double_stream << ", interval:" << interval;

	KNetEnv::serialize();
	

	for (s32 i = 0; i < 10; i++)
	{
		LogInfo() << "turbo[" << i << "] established session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
		LogInfo() << "turbo[" << i << "] established socket:" << turbos[i]->get_socket_count_by_state(KNTS_ESTABLISHED);
	}
	KNetEnv::serialize();

	for (s32 i = 0; i < 10; i++)
	{
		ret = turbos[i]->stop();
		KNetAssert(ret == 0, "");
	}

	for (s32 i = 0; i < 10; i++)
	{
		auto& controller = *turbos[i];

		for (auto& s : controller.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : controller.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}


	
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::user_count(KNTP_SKT_ALLOC_COUNT) == KNetEnv::user_count(KNTP_SKT_FREE_COUNT), "");
	KNetAssert(KNetEnv::user_count(KNTP_SES_CREATE_COUNT) == KNetEnv::user_count(KNTP_SES_DESTROY_COUNT), "");

	LogInfo() << "finish.";
	LogInfo() << "";
	return 0;
}


int main()
{
	FNLog::FastStartDebugLogger();
	FNLog::BatchSetChannelConfig(FNLog::GetDefaultLogger(), FNLog::CHANNEL_CFG_PRIORITY, FNLog::PRIORITY_INFO);
	LogInfo() << "start up";

	KNetAssert(test_session_connect_mix(200, true, 1, 5, 20000, 200) == 0, "");


	return 0;
}

