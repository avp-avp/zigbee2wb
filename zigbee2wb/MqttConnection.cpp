#include "zigbee2wb.h"
#include "MqttConnection.h"

CWBControl::ControlType getType(string name) {
	if (name == "Switch") return CWBControl::Switch;
	else if (name == "Alarm") return CWBControl::Alarm;
	else if (name == "PushButton") return CWBControl::PushButton;
	else if (name == "Range") return CWBControl::Range;
	else if (name == "Rgb") return CWBControl::Rgb;
	else if (name == "Text") return CWBControl::Text;
	else if (name == "Generic") return CWBControl::Generic;
	else if (name == "Temperature") return CWBControl::Temperature;
	else if (name == "RelativeHumidity") return CWBControl::RelativeHumidity;
	else if (name == "AtmosphericPressure") return CWBControl::AtmosphericPressure;
	else if (name == "SoundLevel") return CWBControl::SoundLevel;
	else if (name == "PrecipitationRate") return CWBControl::PrecipitationRate;
	else if (name == "WindSpeed") return CWBControl::WindSpeed;
	else if (name == "PowerPower") return CWBControl::PowerPower;
	else if (name == "PowerConsumption") return CWBControl::PowerConsumption;
	else if (name == "VoltageVoltage") return CWBControl::VoltageVoltage;
	else if (name == "WaterFlow") return CWBControl::WaterFlow;
	else if (name == "WaterTotal") return CWBControl::WaterTotal;
	else if (name == "Resistance") return CWBControl::Resistance;
	else if (name == "GasConcentration") return CWBControl::GasConcentration;
	else if (name == "Lux") return CWBControl::Lux;
	return CWBControl::Error;
}

CZigbeeWBDevice::CZigbeeWBDevice(string Name, string Description)
	:wbDevice(Name, Description), modelTemplate(NULL) {		
};

CMqttConnection::CMqttConnection(CConfigItem config, string mqttHost, CLog* log)
	:m_isConnected(false), mosquittopp("Zigbee2WB"), m_bStop(false), m_ZigbeeWb("zigbee", "Zigbee")
{
	if (mqttHost!="") m_Server = mqttHost;
	else m_Server = config.getStr("mqtt/host", false, "wirenboard");

	m_BaseTopic = config.getStr("mqtt/base_topic", false, "zigbee2mqtt");
	
	configValues converters;
	config.getNode("converters").getValues(converters);
	for_each_const(configValues, converters, converter)
	{
		configValues values;
		converter->second.getValues(values);
		for_each_const(configValues, values, value) {
			string val = value->second.getStr("");
			m_Converters_z2w[converter->first][value->first] = val;
			m_Converters_w2z[converter->first][val] = value->first;
		}
	}

	configValues models;
	config.getNode("models").getValues(models);
	for_each_const(configValues, models, model) {
		string name = model->first;
		configValues controls;
		m_ModelTemplates[name].jsonControl = model->second.getInt("json_template", false, false);

		model->second.getNode("controls").getValues(controls);
		for_each_const(configValues, controls, control) {
			string type_name = control->second.getStr("type");
			string converter = control->second.getStr("converter", false, "");
			m_ModelTemplates[name].controls[control->first].type = getType(type_name);
			if (m_ModelTemplates[name].controls[control->first].type == CWBControl::Error) 
				throw CHaException(CHaException::ErrBadParam, "Unknown type: %s", type_name.c_str());
			
			if (converter!="") {
				if (m_Converters_z2w.find(converter)!=m_Converters_z2w.end()) {
					m_ModelTemplates[name].controls[control->first].converter_z2w = &m_Converters_z2w[converter];
					m_ModelTemplates[name].controls[control->first].converter_w2z = &m_Converters_w2z[converter];					
				} else 
					throw CHaException(CHaException::ErrBadParam, "Unknown converter: %s", converter.c_str());
			}
		}
	}

	m_ZigbeeWb.addControl("state", CWBControl::Text, true);
	m_ZigbeeWb.addControl("log_level", CWBControl::Text, true);
	m_ZigbeeWb.addControl("permit_join", CWBControl::Text, true);
	m_ZigbeeWb.addControl("log", CWBControl::Text, true);
	m_Log = log;
	connect(m_Server.c_str());
	loop_start();

}

CMqttConnection::~CMqttConnection()
{
	loop_stop(true);
}

void CMqttConnection::on_connect(int rc)
{
	m_Log->Printf(1, "mqtt::on_connect(%d)", rc);

	if (!rc)
	{
		m_isConnected = true;
	}

	subscribe(m_BaseTopic+"/#");
	for_each_const(CZigbeeWBDeviceMap, m_Devices, device) {
		string topic = "/devices/"+device->first+"/controls/+/on";
		m_Log->Printf(5, "subscribe to %s", topic.c_str());
		subscribe(topic);
	}

	PublishDevice(&m_ZigbeeWb);
	publish(m_BaseTopic+"/bridge/config/devices/get", "");
}

void CMqttConnection::on_disconnect(int rc)
{
	m_isConnected = false;
	m_Log->Printf(1, "mqtt::on_disconnect(%d)", rc);
}

void Parse(string str, Json::Value &obj){
	strstream stream;
	stream<<str;
	stream>>obj;	
}

void CMqttConnection::on_message(const struct mosquitto_message *message)
{
	string payload;

//	m_Log->Printf(3, "%s::on_message(%s, %d bytes)", m_sName.c_str(), message->topic, message->payloadlen);

	if (message->payloadlen && message->payload)
	{
		char* payload_str = new char[message->payloadlen+1];
		memcpy(payload_str, message->payload, message->payloadlen);
		payload_str[message->payloadlen] = 0;
		//payload.assign((const char*)message->payload, message->payloadlen);
		//payload += "\0";
		payload = payload_str;
		//m_Log->Printf(3, "%s", payload_str);
		delete []payload_str;
	}
	
	m_Log->Printf(5, "CMqttConnection::on_message(%s=%s)", message->topic, payload.c_str());
	
	try
	{
		string_vector v;
		SplitString(message->topic, '/', v);
		if (v.size() < 2)
			return;

		if (v[0]=="zigbee2mqtt") {
			if (v[1]=="bridge") {
				if (v.size() >= 2 && v[2]=="config") {
					if (v.size()==3) {
						Json::Value config; Parse(payload, config);
						m_ZigbeeWb.set("log_level", config["log_level"].asString());
						m_ZigbeeWb.set("permit_join", config["permit_join"].asString());
						SendUpdate();
					} else if (v[3]=="devices" && v.size()==4) {
						Json::Value devices; Parse(payload, devices);
						for (Json::ArrayIndex i=0;i<devices.size();i++) {
							Json::Value device = devices[i];
							string ieeeAddr = device["ieeeAddr"].asString();
							if (!device.isMember("friendly_name")) continue;
							string friendly_name = device["friendly_name"].asString();                        
							string type = device["type"].asString();
							string model = device["model"].asString();
							string modelID = device["modelID"].asString();
							string lastSeen = device["lastSeen"].asString();
							m_Log->Printf(5, "Device %s(%s, '%s'/'%s') lastSeen %s", friendly_name.c_str(), type.c_str(), model.c_str(), modelID.c_str(), lastSeen.c_str());
							if (type!="Coordinator") {
								if (m_Devices.find(friendly_name)==m_Devices.end()) {
									CZigbeeWBDevice *dev = new CZigbeeWBDevice(friendly_name, friendly_name); 
									if (m_ModelTemplates.find(modelID)!=m_ModelTemplates.end()) 
										dev->modelTemplate = &m_ModelTemplates[modelID];
									dev->wbDevice.addControl("lastSeen", CWBControl::Text, true);
									dev->wbDevice.set("lastSeen", lastSeen);
									if (dev->modelTemplate) {
										if (dev->modelTemplate->jsonControl)
											dev->wbDevice.addControl("raw", CWBControl::Text, true);

										for_each_const(CZigbeeControlList, dev->modelTemplate->controls, control) {
											dev->wbDevice.addControl(control->first, control->second.type, false);
										}
									}
									CreateDevice(dev);
									string topic = "/devices/"+friendly_name+"/controls/+/on";
									m_Log->Printf(5, "subscribe to %s", topic.c_str());
									subscribe(topic);
								}
								publish(m_BaseTopic+"/"+friendly_name+"/get", "");
							}
						}
					} 
				} else if (v.size() == 3 && m_ZigbeeWb.controlExists(v[2])) {
					m_ZigbeeWb.set(v[2], payload);
					SendUpdate();
				} 
			} else if (m_Devices.find(v[1])!=m_Devices.end()) {
				if (v.size()==2) {
					Json::Value state; Parse(payload, state);
					CZigbeeWBDevice *dev = m_Devices[v[1]];
					Json::Value::Members members = state.getMemberNames();
					const CControlMap *controls = dev->wbDevice.getControls();
					bool needCreate = false;
					bool gotLastSeen = false;

					if (dev->modelTemplate && dev->modelTemplate->jsonControl) 
						dev->wbDevice.set("raw", payload);

					for (Json::Value::Members::iterator i=members.begin();i!=members.end();i++) {
						string name = *i;
						if (name=="lastSeen") gotLastSeen = true;
						string value = state[name].asString();
						if (controls->find(*i)==controls->end()) {
							dev->wbDevice.addControl(*i, CWBControl::Text, false);
							needCreate = true;
						}
						if (dev->modelTemplate) {
							if (dev->modelTemplate->controls.find(name)!=dev->modelTemplate->controls.end()) {
								CZigbeeControl &control = dev->modelTemplate->controls[*i];
								if (control.converter_z2w->find(value)!=control.converter_z2w->end()) 
									value = (*control.converter_z2w)[value];
							}
						} 				
						dev->wbDevice.set(*i, value);
					}

					if (!gotLastSeen) {
						using namespace std::chrono;
						milliseconds ms = duration_cast< milliseconds >(
							system_clock::now().time_since_epoch()
						);
						char buf[50]; sprintf(buf, "%lld", ms.count());
						dev->wbDevice.set("lastSeen", buf);
					}

					if (needCreate) CreateDevice(dev);
					SendUpdate();
				}
			} else if (v.size()==2) {
				publish(m_BaseTopic+"/bridge/config/devices/get", "");
			} 
        } else if (v.size()==6 && v[0]==""&& v[1]=="devices" && v[3]=="controls" && v[5]=="on") {
			string device = v[2]; string control = v[4];
			CZigbeeWBDevice *dev = m_Devices[device];
			string value = payload;
			if (dev->modelTemplate && dev->modelTemplate->controls.find(control)!=dev->modelTemplate->controls.end()) {
				string_map *coverter = dev->modelTemplate->controls[control].converter_w2z;
				if (coverter && coverter->find(value)!=coverter->end()) value = (*coverter)[value];
			}
			publish(m_BaseTopic+"/"+device+"/set/"+control, value);
		}
	}
	catch (CHaException ex)
	{
		m_Log->Printf(0, "Exception %s (%d)", ex.GetMsg().c_str(), ex.GetCode());
	}	
}

void CMqttConnection::on_log(int level, const char *str)
{
	m_Log->Printf(max(10,level), "mqtt::on_log(%d, %s)", level, str);
}

void CMqttConnection::on_error()
{
	m_Log->Printf(1, "mqtt::on_error()");
}

void CMqttConnection::CreateDevice(CZigbeeWBDevice* dev)
{
	m_Devices[dev->wbDevice.getName()] = dev;
	PublishDevice(&dev->wbDevice);
}

void CMqttConnection::PublishDevice(CWBDevice* dev)
{
	string_map v;
	dev->createDeviceValues(v);
	for_each(string_map, v, i)
	{
		publish(i->first, i->second, true);
		m_Log->Printf(5, "publish %s=%s", i->first.c_str(), i->second.c_str());
	}
}

void CMqttConnection::SendUpdate()
{
	string_map v;

	for_each(CZigbeeWBDeviceMap, m_Devices, dev)
	{
		if (dev->second)
			dev->second->wbDevice.updateValues(v);
	}

	m_ZigbeeWb.updateValues(v);

	for_each(string_map, v, i)
	{
		publish(i->first, i->second, true);
		m_Log->Printf(5, "publish %s=%s", i->first.c_str(), i->second.c_str());
	}
}


void CMqttConnection::subscribe(const string &topic) {
	mosqpp::mosquittopp::subscribe(NULL, topic.c_str());
}
void CMqttConnection::publish(const string &topic, const string &payload, bool retain) {
	mosqpp::mosquittopp::publish(NULL, topic.c_str(), payload.length(), payload.c_str(), 0, retain);
}
