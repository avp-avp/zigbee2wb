#pragma once
#include "zigbee2wb.h"
#include "mosquittopp.h"

typedef map<string, string_map> CConverters;
typedef string (*ConverterFunc)(string);
typedef map<string, ConverterFunc> CConverterFuncs;

struct CZigbeeControl {
	CWBControl::ControlType type;
	int max;
	string_map *converter_z2w, *converter_w2z;
};

typedef map<string, CZigbeeControl> CZigbeeControlList;
typedef map<string, Json::Value> CJsonMap;

struct CModelTemplate {
	bool jsonControl;
	CZigbeeControlList controls;
};

typedef map<string, CModelTemplate> CModelTemplateList;

struct CZigbeeWBDevice {
	CZigbeeWBDevice(string Name, string Description);
	CModelTemplate *modelTemplate;
	CWBDevice wbDevice;
	CJsonMap exposes;
	CConverterFuncs converters;
};

typedef map<string, CZigbeeWBDevice*> CZigbeeWBDeviceMap;

class CMqttConnection
	:public mosqpp::mosquittopp
{
	string m_Server, m_BaseTopic;
	CLog *m_Log;
	bool m_isConnected, m_bStop;
	CZigbeeWBDeviceMap m_Devices;
	CWBDevice m_ZigbeeWb;
	CConverters m_Converters_z2w, m_Converters_w2z;
	CModelTemplateList m_ModelTemplates;

public:
	CMqttConnection(CConfigItem config, string mqttHost, CLog* log);
	~CMqttConnection();
	void NewMessage(string message);
	bool isStopped() {return m_bStop;};

private:
	virtual void on_connect(int rc);
	virtual void on_disconnect(int rc);
	//virtual void on_publish(int mid);
	virtual void on_message(const struct mosquitto_message *message);
	//virtual void on_subscribe(int mid, int qos_count, const int *granted_qos);
	//virtual void on_unsubscribe(int mid);
	virtual void on_log(int level, const char *str);
	virtual void on_error();

	void sendCommand(string device, string cmd, string data="");
	void SendUpdate();
	void CreateDevice(CZigbeeWBDevice* dev);
	void PublishDevice(CWBDevice* dev);
	void subscribe(const string &topic);
	void publish(const string &topic, const string &payload, bool retain=false);
};
