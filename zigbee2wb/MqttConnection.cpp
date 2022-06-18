#include "zigbee2wb.h"
#include "MqttConnection.h"

CWBControl::ControlType getWBType(string name) {
	if (name == "temperature")      return CWBControl::Temperature;
	else if (name == "humidity")    return CWBControl::RelativeHumidity;
	else if (name == "voltage")     return CWBControl::Voltage;
	else if (name == "linkquality") return CWBControl::Generic;
	else if (name == "battery")     return CWBControl::Generic;
	else if (name == "pressure")    return CWBControl::AtmosphericPressure;
	else if (name == "state_right") return CWBControl::Switch;
	else if (name == "state_left")  return CWBControl::Switch;
	else if (name == "occupancy")   return CWBControl::Switch;
	else if (name == "contact")     return CWBControl::Switch;
	else if (name == "state")       return CWBControl::Switch;


	return CWBControl::Text;
}

string hPa_Converter(string value)
{
	return ftoa(atof(value)/1.33322387415, 1);
}

string mV_Converter(string value)
{
	return ftoa(0.001*atoi(value), 3);
}

ConverterFunc getConverter(string unit) {
	if (unit == "hPa")      return hPa_Converter;
	if (unit == "mV")       return mV_Converter;
	
	return NULL;
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
			m_ModelTemplates[name].controls[control->first].type = CWBControl::getType(type_name);
			m_ModelTemplates[name].controls[control->first].max = control->second.getInt("max", false, -1);

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
				if (v.size() == 3 && v[2]=="state") {
					if (payload=="online") {
						publish(m_BaseTopic+"/bridge/config/devices/get", "");
					}
				}
				else if (v.size() >= 3 && v[2]=="config") {
					if (v.size()==3) {
						Json::Value config; Parse(payload, config);
						m_ZigbeeWb.set("log_level", config["log_level"].asString());
						m_ZigbeeWb.set("permit_join", config["permit_join"].asString());
						SendUpdate();
					} 
				} 
				else if (v.size()==3 && v[2]=="devices") 
				{
					Json::Value devices; Parse(payload, devices);
					for (Json::Value::iterator iDev=devices.begin();iDev!=devices.end();iDev++) {
						Json::Value device = *iDev;
						string type = device["type"].asString();
						bool interview_completed = device["interview_completed"].asBool();
						if (type=="Coordinator" || !device.isMember("friendly_name") || !interview_completed) continue;

						string ieee_address = device["ieee_address"].asString();
						string friendly_name = device["friendly_name"].asString();   
						Json::Value definition = device["definition"];     
						string model = definition["model"].asString();
						string model_id = device["model_id"].asString(); 
						string lastSeen = device["lastSeen"].asString();
						m_Log->Printf(5, "Device %s(%s, '%s'/'%s') lastSeen %s", friendly_name.c_str(), type.c_str(), model.c_str(), model_id.c_str(), lastSeen.c_str());

						if (m_Devices.find(friendly_name)==m_Devices.end()) {
							CZigbeeWBDevice *dev = new CZigbeeWBDevice(friendly_name, friendly_name); 
							CModelTemplateList::iterator iModelTemplate = m_ModelTemplates.find(model_id);
							dev->wbDevice.addControl("lastSeen", CWBControl::Text, true);
							dev->wbDevice.set("lastSeen", lastSeen);
							string gettableControl = "linkquality"; 

							Json::Value exposes = definition["exposes"];
							for (Json::Value::iterator iExpose=exposes.begin();iExpose!=exposes.end();iExpose++) {
								string name = (*iExpose)["property"].asString();
								string type = (*iExpose)["type"].asString();
								Json::Value expose;
								if (name.length()){
									expose = *iExpose;
								} else {
									if (!(*iExpose)["features"].isArray()) {
										m_Log->Printf(1, "Device %s has bad expose(property and features not found)");
										continue;
									}
									if ((*iExpose)["features"].size()!=1) {
										m_Log->Printf(1, "Device %s features size != 1");
										continue;
									}
									name = (*iExpose)["features"][0]["property"].asString();
									if (!name.length()){
										m_Log->Printf(1, "Device %s expose name not found");
										continue;
									}
									expose = (*iExpose)["features"][0];
								}
								dev->exposes[name] = expose;

								if ((type=="switch" || type=="binary") && iModelTemplate==m_ModelTemplates.end()) {
									string on  = expose["value_on" ].asString();
									string off = expose["value_off"].asString();
									if (on!="" && off!="") {
										string dev = friendly_name+"/"+name;
										m_Converters_w2z[dev]["1"] = on;
										m_Converters_z2w[dev][on] = "1";
										m_Converters_w2z[dev]["0"] = off;
										m_Converters_z2w[dev][off] = "0";
										m_ModelTemplates["#"+friendly_name].controls[name].type = CWBControl::Switch;
										m_ModelTemplates["#"+friendly_name].controls[name].converter_z2w = &m_Converters_z2w[dev];
										m_ModelTemplates["#"+friendly_name].controls[name].converter_w2z = &m_Converters_w2z[dev];					
										iModelTemplate = m_ModelTemplates.find("#"+friendly_name);
									}
								}

								if (expose["access"].asInt()&4) gettableControl = expose["property"].asString();
								m_Log->Printf(6, "Device %s add expose %s type %s", friendly_name.c_str(), name.c_str(), type.c_str());
							}
							
							if (iModelTemplate!=m_ModelTemplates.end()) {
								dev->modelTemplate = &iModelTemplate->second;
								if (dev->modelTemplate->jsonControl)
									dev->wbDevice.addControl("raw", CWBControl::Text, true);
							}

							CreateDevice(dev);
							string topic = "/devices/"+friendly_name+"/controls/+/on";
							m_Log->Printf(5, "subscribe to %s", topic.c_str());
							subscribe(topic);
							// publish(m_BaseTopic+"/"+friendly_name+"/get", "{\""+gettableControl+"\": \"\"}");
						}
					}
					} else if (v.size() == 3 && m_ZigbeeWb.controlExists(v[2])) {
					m_ZigbeeWb.set(v[2], payload);
					SendUpdate();
				} 
			} else if (m_Devices.find(v[1])!=m_Devices.end()) {
				if (v.size()==2 && message->payloadlen) {
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
						if (!state[name].isConvertibleTo(Json::stringValue)) continue;
						string value = state[name].asString();

						CZigbeeControl *control_template = NULL;
						if (dev->modelTemplate) {
							if (dev->modelTemplate->controls.find(name)!=dev->modelTemplate->controls.end()) {
								control_template = &dev->modelTemplate->controls[name];
							}
						} 			

						if (controls->find(name)==controls->end()) {
							m_Log->Printf(5, " Control %s  not found", name.c_str());
							if (dev->exposes.find(name)==dev->exposes.end()) continue;
							Json::Value expose = dev->exposes[name];
							int access = expose["access"].asInt();
							string type = expose["type"].asString();
							string expose_name = expose["name"].asString();
							if (name!=expose_name) {
								m_Log->Printf(6, " Control %s!=%s", name.c_str(), expose_name.c_str());
							}
							string unit = expose["unit"].asString();
							string property = expose["property"].asString();
							int value_max = expose["value_max"].asInt();
							CWBControl::ControlType wbType = control_template ? control_template->type : getWBType(property);
							ConverterFunc converter = getConverter(unit);
							if (converter) {
								dev->converters[name] = converter;
							}

							dev->wbDevice.addControl(*i, wbType, access&2?false:true);
							if (control_template && control_template->max>0) 
								dev->wbDevice.enrichControl(*i, "max", itoa(control_template->max));
							else if (value_max)
								dev->wbDevice.enrichControl(*i, "max", itoa(value_max));

							needCreate = true;
							m_Log->Printf(5, " Control %s(%s, '%s(%d)'/'%s') access %d", name.c_str(), type.c_str(), property.c_str(), wbType, unit.c_str(), access);
						}
						if (control_template && control_template->converter_z2w && control_template->converter_z2w->find(value)!=control_template->converter_z2w->end()) {
							value = (*control_template->converter_z2w)[value];
						} 		
						else if (dev->converters.find(name)!=dev->converters.end()) {
							value = dev->converters[name](value);
						}
						dev->wbDevice.set(name, value);
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
