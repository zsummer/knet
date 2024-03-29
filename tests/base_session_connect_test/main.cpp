#include "knet_base.h"
#include "knet_socket.h"
#include "knet_turbo.h"
#include <memory>
KNetEnv env_init_;


#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	return -1;\
}

s32 test_session_connect_mix()
{

	KNetEnv::clean_prof();
	KNetEnv::error_count() = 0;


	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0  ,0, 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0  ,0, 0 });

	std::shared_ptr< KNetTurbo> sp_tb = std::make_shared<KNetTurbo>();
	KNetTurbo& turbo = *sp_tb;

	s32 ret = turbo.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870  ,0, 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870  ,0, 0 });
	KNetSession* session = NULL;
	ret = turbo.create_connect(mc, session);
	KNetAssert(ret == 0, "");
	KNetAssert(session != NULL, "");
	s32 connect_tested = 1;
	KNetOnConnect on_connect = [&](KNetTurbo& turbo, KNetSession & session, bool connected, u16 state, s64 time_out)
	{
		connect_tested = 0;
		if (!connected)
		{
			connect_tested = -1;
		}
	};

	ret = turbo.start_connect(*session, on_connect, 5000);
	KNetAssert(ret == 0, "");

	for (size_t i = 0; i < 10; i++)
	{
		ret = turbo.do_tick();
		KNetAssert(ret == 0, "");
		if (connect_tested == 0)
		{
			LogInfo() << "first connected  select :" << i << " c";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	KNetAssert(connect_tested == 0, "");



	turbo.close_connect(session);
	for (size_t i = 0; i < 10; i++)
	{
		ret = turbo.do_tick();
		KNetAssert(ret == 0, "");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	if (true)
	{
		for (auto &s : turbo.sessions())
		{
			KNetAssert(s.state_ != KNTS_ESTABLISHED, "rst session ");
		}
		for (auto& s : turbo.nss())
		{
			if (s.state_ == KNTS_ESTABLISHED)
			{
				KNetAssert( turbo.skt_is_server(s), "rst skt ");
			}
		}
	}

	LogInfo() << "rst test finish.";
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


s32 test_session_connect()
{

	KNetEnv::clean_prof();
	KNetEnv::error_count() = 0;


	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 ,0, 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0  ,0, 0 });

	std::shared_ptr< KNetTurbo> sp_tb1 = std::make_shared<KNetTurbo>();
	KNetTurbo& turbo1 = *sp_tb1;

	std::shared_ptr< KNetTurbo> sp_tb2 = std::make_shared<KNetTurbo>();
	KNetTurbo& turbo2 = *sp_tb2;


	s32 ret = turbo1.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870 ,0, 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870  ,0, 0 });
	KNetSession* session = NULL;
	ret = turbo2.create_connect(mc, session);
	KNetAssert(ret == 0, "");
	KNetAssert(session != NULL, "");
	s32 connect_tested = 1;
	KNetOnConnect on_connect = [&](KNetTurbo& turbo, KNetSession& session, bool connected, u16 state, s64 time_out)
	{
		connect_tested = 0;
		if (!connected)
		{
			connect_tested = -1;
		}
	};

	ret = turbo2.start_connect(*session, on_connect, 50000);
	KNetAssert(ret == 0, "");

	for (size_t i = 0; i < 10; i++)
	{
		ret = turbo1.do_tick();
		KNetAssert(ret == 0, "");
		ret = turbo2.do_tick();
		KNetAssert(ret == 0, "");

		if (connect_tested == 0)
		{
			LogInfo() << "first connected  select :" << i << " c";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	KNetAssert(connect_tested == 0, "");



	turbo2.close_connect(session);
	for (size_t i = 0; i < 10; i++)
	{
		ret = turbo1.do_tick();
		KNetAssert(ret == 0, "");
		ret = turbo2.do_tick();
		KNetAssert(ret == 0, "");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	if (true)
	{
		for (auto& s : turbo1.sessions())
		{
			KNetAssert(s.state_ != KNTS_ESTABLISHED, "rst session ");
		}
		for (auto& s : turbo1.nss())
		{
			if (s.state_ == KNTS_ESTABLISHED)
			{
				KNetAssert(turbo1.skt_is_server(s), "rst skt ");
			}
		}
	}
	if (true)
	{
		for (auto& s : turbo2.sessions())
		{
			KNetAssert(s.state_ != KNTS_ESTABLISHED, "rst session ");
		}
		for (auto& s : turbo2.nss())
		{
			if (s.state_ == KNTS_ESTABLISHED)
			{
				KNetAssert(turbo2.skt_is_server(s), "rst skt ");
			}
		}
	}


	LogInfo() << "rst test finish.";
	ret = turbo1.stop();
	KNetAssert(ret == 0, "rst skt ");
	ret = turbo2.stop();
	KNetAssert(ret == 0, "rst skt ");
	if (true)
	{
		for (auto& s : turbo1.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : turbo1.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}
	if (true)
	{
		for (auto& s : turbo2.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : turbo2.nss())
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
	LogInfo() << "start up";
	KNetAssert(test_session_connect_mix() == 0, "");
	KNetAssert(test_session_connect() == 0, "");

	return 0;
}

