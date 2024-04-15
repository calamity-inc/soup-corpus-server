#include <iostream>

#include <soup/HttpRequest.hpp>
#include <soup/json.hpp>
#include <soup/netIntel.hpp>
#include <soup/Server.hpp>
#include <soup/ServerWebService.hpp>
#include <soup/string.hpp>
#include <soup/Task.hpp>
#include <soup/Thread.hpp>
#include <soup/time.hpp>

using namespace soup;

static time_t g_net_intel_as_of = time::LONGAGO;
static netIntel g_net_intel{};

struct MaintainTask : public Task
{
	Thread thrd;
	netIntel buf;

	void onTick() final
	{
		if (time::millisSince(g_net_intel_as_of) > 1000 * 60 * 60 * 24
			&& !thrd.isRunning()
			)
		{
			std::cout << "Going to fetch updated netIntel..." << std::endl;
			thrd.start([](Capture&& cap)
			{
				cap.get<netIntel*>()->init();
				std::cout << "Got updated netIntel, swapping." << std::endl;
				g_net_intel_as_of = time::millis();
				g_net_intel = std::move(*cap.get<netIntel*>());
			}, &buf);
		}
	}

	int getSchedulingDisposition() const noexcept final
	{
		return LOW_FREQUENCY;
	}
};

int main()
{
	Server serv;

	serv.add<MaintainTask>();

	ServerWebService web_srv([](Socket& s, HttpRequest&& req, ServerWebService&)
	{
		if (req.path.substr(0, 8) == "/asByIp/")
		{
			IpAddr ip;
			if (!ip.fromString(req.path.substr(8)))
			{
				ServerWebService::sendContent(s, "400", "Invalid IP address");
			}
			else if (g_net_intel_as_of == time::LONGAGO)
			{
				ServerWebService::sendContent(s, "503 Service Unavailable", "Still warming up, try again in a few seconds!");
			}
			else if (auto as = g_net_intel.getAsByIp(ip))
			{
				JsonObject obj;
				obj.add("number", (int64_t)as->number);
				obj.add("handle", as->handle);
				obj.add("name", as->name);
				obj.add("is_hosting", as->isHosting(g_net_intel));

				auto body = obj.encodePretty();

				HttpResponse resp;
				resp.setHeader("Access-Control-Allow-Origin", "*");
				resp.setHeader("Content-Type", "application/json");
				resp.setHeader("Content-Length", std::to_string(body.size()));
				resp.body = body;

				ServerWebService::sendResponse(s, "200", resp.toString());
			}
			else
			{
				ServerWebService::send404(s);
			}
		}
		else if (req.path.substr(0, 12) == "/asByNumber/")
		{
			if (g_net_intel_as_of == time::LONGAGO)
			{
				ServerWebService::sendContent(s, "503 Service Unavailable", "Still warming up, try again in a few seconds!");
			}
			else if (auto as = g_net_intel.getAsByNumber(string::toInt<uint32_t>(req.path.substr(12), 0)))
			{
				JsonObject obj;
				obj.add("number", (int64_t)as->number);
				obj.add("handle", as->handle);
				obj.add("name", as->name);
				obj.add("is_hosting", as->isHosting(g_net_intel));

				auto body = obj.encodePretty();

				HttpResponse resp;
				resp.setHeader("Access-Control-Allow-Origin", "*");
				resp.setHeader("Content-Type", "application/json");
				resp.setHeader("Content-Length", std::to_string(body.size()));
				resp.body = body;

				ServerWebService::sendResponse(s, "200", resp.toString());
			}
			else
			{
				ServerWebService::send404(s);
			}
		}
		else if (req.path.substr(0, 14) == "/locationByIp/")
		{
			IpAddr ip;
			if (!ip.fromString(req.path.substr(14)))
			{
				ServerWebService::sendContent(s, "400", "Invalid IP address");
			}
			else if (g_net_intel_as_of == time::LONGAGO)
			{
				ServerWebService::sendContent(s, "503 Service Unavailable", "Still warming up, try again in a few seconds!");
			}
			else if (auto loc = g_net_intel.getLocationByIp(ip))
			{
				JsonObject obj;
				obj.add("country_code", loc->country_code.c_str());
				obj.add("state", loc->state);
				obj.add("city", loc->city);

				auto body = obj.encodePretty();

				HttpResponse resp;
				resp.setHeader("Access-Control-Allow-Origin", "*");
				resp.setHeader("Content-Type", "application/json");
				resp.setHeader("Content-Length", std::to_string(body.size()));
				resp.body = body;

				ServerWebService::sendResponse(s, "200", resp.toString());
			}
			else
			{
				ServerWebService::send404(s);
			}
		}
		else
		{
			ServerWebService::sendText(s,
				"Corpus Offerings:\n"
				"- /asByIp/104.28.202.185\n"
				"- /asByNumber/13335\n"
				"- /locationByIp/104.28.202.185\n"
			);
		}
	});
	serv.bind(80, &web_srv);

	serv.run();
}
