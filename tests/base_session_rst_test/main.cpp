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
	KNetOnConnect on_connect = [&](KNetController&c, KNetSession& session, bool connected, u16 state, s64 time_out)
	{
		connect_tested = 0;
		if (!connected)
		{
			connect_tested = -1;
		}
	};

	ret = controller.start_connect(*session, on_connect, 5000);
	KNetAssert(ret == 0, "");

	for (size_t i = 0; i < 10; i++)
	{
		ret = controller.do_tick();
		KNetAssert(ret == 0, "");
		if (connect_tested == 0)
		{
			LogInfo() << "first connected  select :" << i << " c";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	KNetAssert(connect_tested == 0, "");


	s32 count = 0;
	KNetOnData on_data = [&](KNetController& c, KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
	{
		c.close_and_remove_session(&s);
		count++;
	};

	KNetOnDisconnect on_disconnect = [&](KNetController& c, KNetSession& session, bool passive)
	{
		if (session.is_server() && passive)
		{
			LogError() << "has error";
			return;
		}
		if (!session.is_server() && !passive)
		{
			LogError() << "has error";
			return;
		}
		count++;
	};


	controller.set_on_data(on_data);
	controller.set_on_disconnect(on_disconnect);

	controller.send_data(*session, 0, "12345", 6, KNetEnv::now_ms());

	for (size_t i = 0; i < 10; i++)
	{
		ret = controller.do_tick();
		KNetAssert(ret == 0, "");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	KNetAssert(count == 3, "");


	KNetAssert(session->state_ != KNTS_ESTABLISHED, "");

	KNetAssert(controller.remove_connect(session) == 0, "");


	if (true)
	{
		for (auto& s : controller.sessions())
		{
			KNetAssert(s.state_ != KNTS_ESTABLISHED, "rst session ");
		}
		for (auto& s : controller.nss())
		{
			if (s.state_ == KNTS_ESTABLISHED)
			{
				KNetAssert(controller.skt_is_server(s), "rst skt ");
			}
		}
	}

	LogInfo() << "rst test finish.";
	ret = controller.stop();
	KNetAssert(ret == 0, "rst skt ");

	if (true)
	{
		for (auto& s : controller.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : controller.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}


	LogInfo() << "finish.";
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT) == KNetEnv::count(KNT_STT_SKT_FREE_COUNT), "");
	KNetAssert(KNetEnv::count(KNT_STT_SES_CREATE_COUNT) == KNetEnv::count(KNT_STT_SES_DESTROY_COUNT), "");
	return 0;
}


s32 test_session_connect()
{
	KNetEnv::clean_count();
	KNetEnv::error_count() = 0;

	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });

	std::shared_ptr< KNetController> sp_ctl1 = std::make_shared<KNetController>();
	KNetController& controller1 = *sp_ctl1;

	std::shared_ptr< KNetController> sp_ctl2 = std::make_shared<KNetController>();
	KNetController& controller2 = *sp_ctl2;


	s32 ret = controller1.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 });
	KNetSession* session = NULL;
	ret = controller2.create_connect(mc, session);
	KNetAssert(ret == 0, "");
	KNetAssert(session != NULL, "");
	s32 connect_tested = 1;
	KNetOnConnect on_connect = [&](KNetController&c, KNetSession& session, bool connected, u16 state, s64 time_out)
	{
		connect_tested = 0;
		if (!connected)
		{
			connect_tested = -1;
		}
	};

	ret = controller2.start_connect(*session, on_connect, 50000);
	KNetAssert(ret == 0, "");

	for (size_t i = 0; i < 10; i++)
	{
		ret = controller1.do_tick();
		KNetAssert(ret == 0, "");
		ret = controller2.do_tick();
		KNetAssert(ret == 0, "");

		if (connect_tested == 0)
		{
			LogInfo() << "first connected  select :" << i << " c";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	KNetAssert(connect_tested == 0, "");

	s32 count = 0;
	KNetOnData on_data = [&](KNetController& c, KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
	{
		c.close_and_remove_session(&s);
		count++;
	};

	KNetOnDisconnect on_server_disconnect = [&](KNetController& c, KNetSession& session, bool passive)
	{
		if (passive)
		{
			LogError() << "has error";
			return;
		}
		count++;
	};

	KNetOnDisconnect on_client_disconnect = [&](KNetController& c, KNetSession& session, bool passive)
	{
		if (!passive)
		{
			LogError() << "has error";
			return;
		}
		count++;
	};

	controller1.set_on_data(on_data);
	controller1.set_on_disconnect(on_server_disconnect);
	controller2.set_on_disconnect(on_client_disconnect);



	controller2.send_data(*session, 0, "12345", 6, KNetEnv::now_ms());
	for (size_t i = 0; i < 10; i++)
	{
		ret = controller1.do_tick();
		KNetAssert(ret == 0, "");
		ret = controller2.do_tick();
		KNetAssert(ret == 0, "");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	KNetAssert(count == 3, "");


	KNetAssert(session->state_ != KNTS_ESTABLISHED, "");

	KNetAssert(controller2.remove_connect(session) == 0, "");

	if (true)
	{
		for (auto& s : controller1.sessions())
		{
			KNetAssert(s.state_ != KNTS_ESTABLISHED, "rst session ");
		}
		for (auto& s : controller1.nss())
		{
			if (s.state_ == KNTS_ESTABLISHED)
			{
				KNetAssert(controller1.skt_is_server(s), "rst skt ");
			}
		}
	}
	if (true)
	{
		for (auto& s : controller2.sessions())
		{
			KNetAssert(s.state_ != KNTS_ESTABLISHED, "rst session ");
		}
		for (auto& s : controller2.nss())
		{
			if (s.state_ == KNTS_ESTABLISHED)
			{
				KNetAssert(controller2.skt_is_server(s), "rst skt ");
			}
		}
	}


	LogInfo() << "rst test finish.";
	ret = controller1.stop();
	KNetAssert(ret == 0, "rst skt ");
	ret = controller2.stop();
	KNetAssert(ret == 0, "rst skt ");
	if (true)
	{
		for (auto& s : controller1.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : controller1.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}
	if (true)
	{
		for (auto& s : controller2.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : controller2.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}

	LogInfo() << "finish.";
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT) == KNetEnv::count(KNT_STT_SKT_FREE_COUNT), "");
	KNetAssert(KNetEnv::count(KNT_STT_SES_CREATE_COUNT) == KNetEnv::count(KNT_STT_SES_DESTROY_COUNT), "");
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
