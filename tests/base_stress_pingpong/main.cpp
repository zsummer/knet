

#include "knet_base.h"
#include "knet_socket.h"
#include "knet_turbo.h"
#include <memory>
KNetEnv env_init_;


#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	KNetEnv::serialize(); \
	return -1;\
}

s32 test_session_connect_mix(s32 session_count, bool double_stream, s32 send_times, s32 interval,  s32 data_len)
{

	LogInfo() << "";
	LogInfo() << "============================================================================================";
	LogInfo() << "         stress: session:" << session_count << "  double_stream:" << double_stream << " send_times:" << send_times << "  interval:" << interval << "ms  one data:" << data_len;
	LogInfo() << "============================================================================================";


	if (data_len > KNT_KCP_DATA_SIZE)
	{
		data_len = KNT_KCP_DATA_SIZE;
	}
	if (send_times == 0)
	{
		send_times = 1;
	}
	KNetEnv::clean_prof();
	KNetEnv::error_count() = 0;

	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 ,0, 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 ,0, 0 });

	std::shared_ptr< KNetTurbo> sp_tb = std::make_shared<KNetTurbo>();
	KNetTurbo& turbo = *sp_tb;

	s32 ret = turbo.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870  ,0, 0 });
	if (double_stream)
	{
		mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 ,0, 0 });
	}
	

	KNetOnConnect on_connect = [&](KNetTurbo& turbo, KNetSession& session, bool connected, u16 state, s64 time_out)
	{
		if (connected)
		{
			LogDebug() << "connected";
		}
		else
		{
			LogError() << "connect error: last state:" << state <<", time_out:" << time_out;
			KNetEnv::serialize();
			exit(-2);
			return;
		}
		static char buf[KNT_KCP_DATA_SIZE];
		for (s32 i = 0; i < KNT_KCP_DATA_SIZE; i++)
		{
			buf[i] = 'a';
		}
		for (s32 i = 0; i < send_times; i++)
		{
			turbo.send_data(session, 0, buf, data_len, KNetEnv::now_ms());
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



	turbo.set_on_data(on_data);

	turbo.set_on_disconnect(on_disconnect);

	for (s32 i = 0; i < session_count; i++)
	{
		KNetSession* session = NULL;
		ret = turbo.create_connect(mc, session);
		if (ret != 0)
		{
			volatile int aa = 0;
		}
		KNetAssert(ret == 0, "");
		KNetAssert(session != NULL, "");
		ret = turbo.start_connect(*session, on_connect, 15000);
		if (ret != 0)
		{
			volatile int aa = 0;
		}
		KNetAssert(ret == 0, "");
		KNetAssert(KNetEnv::error_count() == 0, "");

		for (s32 i = 0; i < 10; i++)
		{
			ret = turbo.do_tick();
			KNetAssert(ret == 0, "");
		}
		if ((i%10) ==4)
		{
			//std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		}

	}


	s32 loop_count = 10 * 1000 / interval;

	s64 now = KNetEnv::now_ms();
	for (s32 i = 0; i < loop_count; i++)
	{
		ret = turbo.do_tick();
		KNetAssert(ret == 0, "");
		if (i %(loop_count/10) == 0)
		{
			LogInfo() << "now loop:" << i << "/" << loop_count;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}

	LogInfo() << "";
	LogInfo() << "==========================================================";
	LogInfo() << "session_count:" << session_count << ", double_stream:" << double_stream << ", interval:" << interval;

	KNetEnv::serialize();





	ret = turbo.stop();
	KNetAssert(ret == 0, "rst skt ");

	if (true)
	{
		for (auto& s : turbo.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : turbo.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}


	LogInfo() << "finish.";
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::user_count(KNTP_SKT_ALLOC) == KNetEnv::user_count(KNTP_SKT_FREE), "");
	KNetAssert(KNetEnv::user_count(KNTP_SES_ALLOC) == KNetEnv::user_count(KNTP_SES_FREE), "");
	return 0;
}


int main()
{
	FNLog::FastStartDebugLogger();
	FNLog::BatchSetChannelConfig(FNLog::GetDefaultLogger(), FNLog::CHANNEL_CFG_PRIORITY, FNLog::PRIORITY_INFO);
	LogInfo() << "start up";

	KNetAssert(test_session_connect_mix(1, false, 1, 10, 500) == 0, "");
	KNetAssert(test_session_connect_mix(50, false, 1, 10, 500) == 0, "");
	KNetAssert(test_session_connect_mix(70, false, 1, 10, 1000) == 0, "");
	KNetAssert(test_session_connect_mix(30, true, 20, 10, 1000) == 0, "");




	return 0;
}

