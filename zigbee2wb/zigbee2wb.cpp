#include "zigbee2wb.h"
#include "MqttConnection.h"

int main(int argc, char* argv[])
{
	string mqttHost;
	string configName = "zigbee2wb.json";

	int c;
	//~ int digit_optind = 0;
	//~ int aopt = 0, bopt = 0;
	//~ char *copt = 0, *dopt = 0;
	while ((c = getopt(argc, argv, "c:m:")) != -1) {
		//~ int this_option_optind = optind ? optind : 1;
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
		{
			CLog::Init(&debug);
		}

		CLog* log = CLog::Default();

		CMqttConnection mqttConn(config.getRoot(), mqttHost, log);

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

