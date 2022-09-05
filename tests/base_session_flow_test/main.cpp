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

s32 test_session_connect_mix()
{

	KNetEnv::clean_count();
	KNetEnv::error_count() = 0;


	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });

	std::shared_ptr< KNetController> sp_ctl = std::make_shared<KNetController>();
	KNetController& controller = *sp_ctl;

	s32 ret = controller.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 });
	KNetSession* session = NULL;
	ret = controller.create_connect(mc, session);
	KNetAssert(ret == 0, "");
	KNetAssert(session != NULL, "");
	s32 connect_tested = 1;
	KNetOnConnect on_connect = [&](KNetSession& session, bool connected, u16 state, s64 time_out)
	{
		connect_tested = 0;
		if (!connected)
		{
			connect_tested = -1;
		}
	};

	ret = controller.start_connect(*session, on_connect, 50000);
	KNetAssert(ret == 0, "");

	for (size_t i = 0; i < 10; i++)
	{
		ret = controller.do_tick();
		KNetAssert(ret == 0, "");
		if (connect_tested == 0)
		{
			LogInfo() << "first connected  select :" << i << " c";
		}
		bool all_socket_connect = 0;
		if (true)
		{

		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	KNetAssert(connect_tested == 0, "");


	controller.send_kcp_data(*session, "12345", 6, KNetEnv::now_ms());
	for (size_t i = 0; i < 1; i++)
	{
		ret = controller.do_tick();
		KNetAssert(ret == 0, "");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}


	controller.close_connect(session);
	for (size_t i = 0; i < 1; i++)
	{
		ret = controller.do_tick();
		KNetAssert(ret == 0, "");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}


	controller.stop();
	for (size_t i = 0; i < 1; i++)
	{
		ret = controller.do_tick();
		if (ret != 0)
		{
			LogError() << "select error";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	LogInfo() << "finish.";
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT) == KNetEnv::count(KNT_STT_SKT_FREE_COUNT), "");
	KNetAssert(KNetEnv::count(KNT_STT_SES_CREATE_COUNT) == KNetEnv::count(KNT_STT_SES_DESTROY_COUNT), "");
	return 0;
}


s32 test_session_connect()
{

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

