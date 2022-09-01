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


s32 test_socket_bind()
{
	KNetSocket s1(1);
	KNetSocket s2(2);
	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	ret = s2.init("0.0.0.0", s1.local_.port(), "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	s1.destroy();
	ret = s2.init("0.0.0.0", s1.local_.port(), "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}

s32 test_socket_bind2()
{
	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0});
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });
	KNetController controller;
	s32 ret = controller.start_server(mc);
	KNetAssert(ret == 0, "");

	KNetSocket s1(1);
	KNetSocket s2(2);
	ret = s1.init("127.0.0.1", 19870, "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	ret = s2.init("127.0.0.2", 19870, "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");

	controller.destroy();
	ret = s1.init("127.0.0.1", 19870, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	ret = s2.init("127.0.0.2", 19870, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s1.destroy();
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}

int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";
	KNetAssert(test_socket_bind() == 0, "");
	//KNetAssert(test_socket_bind2() == 0, "");

	KNetEnv::clean_prof();
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
	ret = controller.start_connect(*session);
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
	KNetAssert(KNetEnv::prof(KNT_STT_SKT_ALLOC_EVENTS) == KNetEnv::prof(KNT_STT_SKT_FREE_EVENTS), "");
	KNetAssert(KNetEnv::prof(KNT_STT_SES_CREATE_EVENTS) == KNetEnv::prof(KNT_STT_SES_DESTROY_EVENTS), "");

	return 0;
}

