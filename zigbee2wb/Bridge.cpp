#include <chrono>
#include <exception>

#include "Bridge.h"
#include "JsonUtils.h"
#include "Topics.h"

static long long SystemClockMs()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

const char *CBridge::REFRESH = "refresh";
const char *CBridge::SYNC_TOPIC = "/zigbee2wb/sync";

CBridge::CBridge(const CSettings &settings, IMqttTransport *transport, CLog *log)
	:m_Settings(settings), m_Transport(transport), m_Log(log), m_Clock(SystemClockMs),
	m_BridgeDevice("zigbee", "Zigbee"), m_Refreshing(false), m_SyncCounter(0)
{
	m_BridgeDevice.addControl("state", CWBControl::Text, true);
	m_BridgeDevice.addControl("log_level", CWBControl::Text, true);
	m_BridgeDevice.addControl("permit_join", CWBControl::Switch, true);
	m_BridgeDevice.addControl("log", CWBControl::Text, true);
	m_BridgeDevice.addControl(REFRESH, CWBControl::PushButton, false);
}

CZigbeeDevice *CBridge::findDevice(const string &name) const
{
	CDevices::const_iterator i = m_Devices.find(name);
	return i == m_Devices.end() ? NULL : i->second.get();
}

void CBridge::onConnect()
{
	m_Transport->subscribe(SYNC_TOPIC);
	m_Transport->subscribe(DeviceDriverFilter());
	m_Transport->subscribe(DeviceMetaFilter());
	subscribeToCommands(m_BridgeDevice);

	if (m_Refreshing) {
		// The connection was lost in the middle of a refresh: start it over.
		beginCleanup();
		return;
	}

	for_each_const(CDevices, m_Devices, device)
		subscribeToCommands(device->second->wbDevice());

	publishDevice(m_BridgeDevice);
	subscribeToZigbee();
}

// zigbee2mqtt keeps bridge/devices, bridge/info and bridge/state retained, so
// (re)subscribing makes the broker send them again.
void CBridge::subscribeToZigbee()
{
	m_Transport->subscribe(ZigbeeFilter(m_Settings.baseTopic));
}

void CBridge::onMessage(const string &topic, const string &payload, bool retained)
{
	m_Log->Printf(5, "CBridge::onMessage(%s=%s)", topic.c_str(), payload.c_str());

	try
	{
		dispatch(topic, payload, retained);
	}
	catch (CHaException &ex)
	{
		m_Log->Printf(0, "Exception %s (%d) on %s", ex.GetMsg().c_str(), ex.GetCode(), topic.c_str());
	}
	catch (std::exception &ex)
	{
		m_Log->Printf(0, "Exception %s on %s", ex.what(), topic.c_str());
	}
}

void CBridge::dispatch(const string &topic, const string &payload, bool retained)
{
	if (topic == SYNC_TOPIC) {
		if (m_Refreshing && payload == m_SyncToken)
			finishCleanup();
		return;
	}

	CTopic parsed = ParseTopic(m_Settings.baseTopic, topic);

	if (m_Refreshing) {
		// Nothing is created until the broker is clean, see beginCleanup().
		cleanUp(topic, payload, retained);
		trackDriver(parsed, payload);
		return;
	}

	switch (parsed.kind)
	{
	case CTopic::BridgeState:    onBridgeState(payload); break;
	case CTopic::BridgeLogging:  onBridgeLogging(payload); break;
	case CTopic::BridgeInfo:     onBridgeInfo(payload); break;
	case CTopic::BridgeDevices:  onBridgeDevices(payload); break;
	case CTopic::DeviceState:    onDeviceState(parsed.device, payload); break;
	case CTopic::ControlCommand: onControlCommand(parsed.device, parsed.control, payload); break;
	case CTopic::DeviceDriver:
	case CTopic::DeviceMeta:     trackDriver(parsed, payload); break;
	case CTopic::BridgeGroups:   // groups are not supported
	case CTopic::Unknown:
		break;
	}
}

void CBridge::onBridgeState(const string &payload)
{
	// {"state": "online"}; plain "online"/"offline" in zigbee2mqtt 1.x.
	string state = payload;
	if (!payload.empty() && payload[0] == '{')
		state = JsonString(ParseJson(payload)["state"]);

	m_BridgeDevice.set("state", state);
	publishChanges();
}

void CBridge::onBridgeLogging(const string &payload)
{
	Json::Value log = ParseJson(payload);
	string level = JsonString(log["level"]);
	if (level == "warning" || level == "error") {
		m_BridgeDevice.set("log", JsonString(log["message"]));
		publishChanges();
	}
}

void CBridge::onBridgeInfo(const string &payload)
{
	Json::Value info = ParseJson(payload);
	const Json::Value &permitJoin = info["permit_join"];

	m_BridgeDevice.set("log_level", JsonString(info["log_level"]));
	m_BridgeDevice.set("permit_join", permitJoin.isBool() && permitJoin.asBool() ? "1" : "0");
	publishChanges();
}

bool CBridge::isReady(const Json::Value &info)
{
	if (JsonString(info["type"]) == "Coordinator" || JsonString(info["friendly_name"]).empty())
		return false;

	// "interview_completed" is deprecated in favour of "interview_state".
	if (info.isMember("interview_state"))
		return JsonString(info["interview_state"]) == "SUCCESSFUL";

	return info["interview_completed"].isBool() && info["interview_completed"].asBool();
}

void CBridge::onBridgeDevices(const string &payload)
{
	Json::Value devices = ParseJson(payload);
	if (!devices.isArray())
		return;

	for (Json::ArrayIndex i = 0; i < devices.size(); i++) {
		const Json::Value &info = devices[i];
		if (info.isObject() && isReady(info) && !findDevice(JsonString(info["friendly_name"])))
			addDevice(info);
	}
}

void CBridge::addDevice(const Json::Value &info)
{
	string name = JsonString(info["friendly_name"]);
	string modelId = JsonString(info["model_id"]);
	m_Log->Printf(5, "Device %s(%s, '%s')", name.c_str(), JsonString(info["type"]).c_str(), modelId.c_str());

	CExposeMap exposes;
	ParseExposes(info["definition"]["exposes"], exposes);

	unique_ptr<CZigbeeDevice> device(
		new CZigbeeDevice(name, exposes, m_Settings.findModel(modelId), m_Settings.actionFields));

	const string &wbId = device->wbDevice().id();
	if (wbId == m_BridgeDevice.id() || m_DevicesByWbId.count(wbId)) {
		m_Log->Printf(1, "Device %s skipped: /devices/%s is already taken", name.c_str(), wbId.c_str());
		return;
	}

	publishDevice(device->wbDevice());
	subscribeToCommands(device->wbDevice());
	m_DevicesByWbId[wbId] = device.get();
	m_Devices[name] = std::move(device);
}

void CBridge::onDeviceState(const string &name, const string &payload)
{
	CZigbeeDevice *device = findDevice(name);
	if (!device || payload.empty())
		return;

	if (device->applyState(ParseJson(payload), payload, m_Clock()))
		publishDevice(device->wbDevice());

	publishChanges();
}

void CBridge::onControlCommand(const string &wbDevice, const string &control, const string &payload)
{
	if (wbDevice == m_BridgeDevice.id()) {
		if (control == REFRESH)
			refresh();
		return;
	}

	map<string, CZigbeeDevice*>::const_iterator i = m_DevicesByWbId.find(wbDevice);
	if (i == m_DevicesByWbId.end()) {
		m_Log->Printf(3, "Command for unknown device %s", wbDevice.c_str());
		return;
	}

	string request;
	if (!i->second->buildSetPayload(control, payload, request)) {
		m_Log->Printf(3, "Command for unknown or read-only control %s/%s", wbDevice.c_str(), control.c_str());
		return;
	}

	m_Transport->publish(ZigbeeSetTopic(m_Settings.baseTopic, i->second->name()), request, false);
}

void CBridge::subscribeToCommands(const CWbDevice &device)
{
	string filter = ControlCommandFilter(device.id());
	m_Log->Printf(5, "subscribe to %s", filter.c_str());
	m_Transport->subscribe(filter);
}

// Publishes the whole device: metadata and current values of all controls.
void CBridge::publishDevice(CWbDevice &device)
{
	CMqttMessages messages;
	device.describe(messages);
	publish(messages);
}

// Publishes the values changed since the previous call.
void CBridge::publishChanges()
{
	CMqttMessages messages;
	for_each_const(CDevices, m_Devices, device)
		device->second->wbDevice().collectChanges(messages);

	m_BridgeDevice.collectChanges(messages);
	publish(messages);
}

void CBridge::publish(const CMqttMessages &messages)
{
	for_each_const(CMqttMessages, messages, i) {
		m_Transport->publish(i->topic, i->payload, i->retain);
		m_Log->Printf(5, "publish %s=%s", i->topic.c_str(), i->payload.c_str());
	}
}

// Driver name from the JSON metadata of a device; empty when there is none.
static string DriverFromMeta(const string &payload)
{
	if (payload.empty())
		return "";

	try
	{
		Json::Value meta = ParseJson(payload);
		return meta.isObject() ? JsonString(meta["driver"]) : "";
	}
	catch (CHaException &)
	{
		return "";
	}
}

// Remembers the drivers of the devices on the broker, to know which of them
// were published by this service - including the ones left by its previous runs.
void CBridge::trackDriver(const CTopic &topic, const string &payload)
{
	if (topic.kind == CTopic::DeviceMeta)
		m_DriversOnBroker[topic.device].fromMeta = DriverFromMeta(payload);
	else if (topic.kind == CTopic::DeviceDriver)
		m_DriversOnBroker[topic.device].fromTopic = payload;
}

// The JSON metadata is what the conventions define, so it wins; the subtopic is
// the only source for devices published by the versions that did not write JSON.
bool CBridge::isOwnDevice(const CDriverInfo &driver) const
{
	const string &name = driver.fromMeta.empty() ? driver.fromTopic : driver.fromMeta;
	return name == CWbDevice::DriverName();
}

void CBridge::refresh()
{
	if (m_Refreshing)
		return;

	m_Log->Printf(1, "Refresh: removing all %s devices", CWbDevice::DriverName());
	m_Refreshing = true;

	m_CleaningDevices.clear();
	m_CleaningDevices.insert(m_BridgeDevice.id());
	for (map<string, CDriverInfo>::const_iterator i = m_DriversOnBroker.begin(); i != m_DriversOnBroker.end(); i++) {
		if (isOwnDevice(i->second))
			m_CleaningDevices.insert(i->first);
	}
	for_each_const(CDevices, m_Devices, device) {
		const string &wbId = device->second->wbDevice().id();
		m_CleaningDevices.insert(wbId);
		m_Transport->unsubscribe(ControlCommandFilter(wbId));
	}

	m_DevicesByWbId.clear();
	m_Devices.clear();
	beginCleanup();
}

// A retained topic is removed by publishing an empty retained message to it, but
// first the topics have to be found: subscribing to a device makes the broker send
// everything it stores for it. MQTT does not tell when that list ends, so a marker
// is published right after the subscriptions; the broker handles the requests of
// one client in order, therefore the marker comes back after the last stored message.
void CBridge::beginCleanup()
{
	for_each_const(set<string>, m_CleaningDevices, device)
		m_Transport->subscribe(DeviceFilter(*device));

	char token[64];
	snprintf(token, sizeof(token), "%lld-%d", m_Clock(), ++m_SyncCounter);
	m_SyncToken = token;
	m_Transport->publish(SYNC_TOPIC, m_SyncToken, false);
}

void CBridge::cleanUp(const string &topic, const string &payload, bool retained)
{
	// Only stored messages are removed; an empty payload is the echo of a removal.
	if (!retained || payload.empty() || !m_CleaningDevices.count(WbDeviceOf(topic)))
		return;

	m_Log->Printf(5, "remove %s", topic.c_str());
	m_Transport->publish(topic, "", true);
}

void CBridge::finishCleanup()
{
	for_each_const(set<string>, m_CleaningDevices, device)
		m_Transport->unsubscribe(DeviceFilter(*device));

	m_Log->Printf(1, "Refresh: %d devices removed, creating them anew", (int)m_CleaningDevices.size());
	m_CleaningDevices.clear();
	m_Refreshing = false;

	publishDevice(m_BridgeDevice);
	subscribeToZigbee();
}
