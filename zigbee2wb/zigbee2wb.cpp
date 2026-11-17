#include "zigbee2wb.h"
#include "../libs/libutils/Config.h"
#include "../libs/libutils/thread.h"
#include "Settings.h"
#include "MqttConnection.h"

int main(int argc, char* argv[])
{
	string mqttHost;
	string configName = "zigbee2wb.json";

	int c;
	while ((c = getopt(argc, argv, "c:m:")) != -1) {
		switch (c) {
		case 'c':
			configName = optarg;
			break;
		case 'm':
			mqttHost = optarg;
			break;
		}
	}

	if (!configName.length())
	{
		printf("Config not defined\n");
		return -1;
	}

	try
	{
		CConfig config;
		config.Load(configName);

		CConfigItem debug = config.getNode("debug");
		if (debug.isNode())
			CLog::Init(&debug);

		CSettings settings = CSettings::Load(config.getRoot());
		if (mqttHost != "")
			settings.mqttHost = mqttHost;

		CMqttConnection mqttConn(settings, CLog::Default());

		while (!mqttConn.isStopped())
			Sleep(1000);
	}
	catch (CHaException ex)
	{
		CLog* log = CLog::Default();
		log->Printf(0, "Failed with exception %d(%s)", ex.GetCode(), ex.GetMsg().c_str());
	}
	return 0;
}
