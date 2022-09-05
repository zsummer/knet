#include "knet_base.h"
#include "knet_socket.h"
#include "knet_controller.h"
KNetEnv env_init_;


#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	return -1;\
}



int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";

	KNetEnv::clean_count();
	KNetEnv::error_count() = 0;


	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });
	KNetController controller;
	s32 ret = controller.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 });
	KNetSession* session = NULL;
	ret = controller.create_connect(mc, session);
	KNetAssert(ret == 0, "");
	ret = controller.start_connect(*session, NULL);
	KNetAssert(ret == 0, "");

	for (size_t i = 0; i < 10; i++)
	{
		ret = controller.do_tick();
		if (ret != 0)
		{
			LogError() << "select error";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	
	controller.send_kcp_data(*session, "12345", 6, KNetEnv::now_ms()); 
	for (size_t i = 0; i < 10; i++)
	{
		ret = controller.do_tick();
		if (ret != 0)
		{
			LogError() << "select error";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}


  	controller.remove_session_with_rst(session->inst_id_);
	for (size_t i = 0; i < 10; i++)
	{
		ret = controller.do_tick();
		if (ret != 0)
		{
			LogError() << "select error";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}


	controller.destroy();
	for (size_t i = 0; i < 10; i++)
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

