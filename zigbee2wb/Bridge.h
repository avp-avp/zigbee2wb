// The core of the service: translates zigbee2mqtt messages into Wiren Board
// devices and Wiren Board commands into zigbee2mqtt "set" requests.
// It knows nothing about the MQTT client library, see IMqttTransport.
#pragma once
#include "zigbee2wb.h"
#include "Settings.h"
#include "ZigbeeDevice.h"
#include "Topics.h"

class IMqttTransport
{
public:
	virtual ~IMqttTransport() {}
	virtual void publish(const string &topic, const string &payload, bool retain) = 0;
	virtual void subscribe(const string &topic) = 0;
	virtual void unsubscribe(const string &topic) = 0;
};

class CBridge
{
	typedef map<string, unique_ptr<CZigbeeDevice> > CDevices;

public:
	typedef long long (*ClockFunc)();   // milliseconds since the epoch

	CBridge(const CSettings &settings, IMqttTransport *transport, CLog *log);

	// Replaces the system clock, for tests.
	void setClock(ClockFunc clock) { m_Clock = clock; }

	// To be called every time the MQTT connection is established.
	void onConnect();
	// To be called for every received message; `retained` tells that the broker
	// sent a stored message in response to a subscription. Never throws.
	void onMessage(const string &topic, const string &payload, bool retained = false);

	// Removes from the broker every device published by this service (including
	// the ones left by previous runs) and creates the devices anew from what
	// zigbee2mqtt reports. Also started by the "refresh" button of /devices/zigbee.
	void refresh();
	bool isRefreshing() const { return m_Refreshing; }

	static const char *REFRESH;      // the button control of /devices/zigbee
	static const char *SYNC_TOPIC;   // service topic, see beginCleanup()

	// By zigbee2mqtt friendly name; NULL when there is no such device.
	CZigbeeDevice *findDevice(const string &name) const;
	size_t deviceCount() const { return m_Devices.size(); }

private:
	CSettings m_Settings;
	IMqttTransport *m_Transport;
	CLog *m_Log;
	ClockFunc m_Clock;
	CWbDevice m_BridgeDevice;   // state of zigbee2mqtt itself, /devices/zigbee
	CDevices m_Devices;         // by zigbee2mqtt friendly name
	map<string, CZigbeeDevice*> m_DevicesByWbId;

	// Refresh state
	// Driver of a device on the broker as told by its JSON metadata and by the
	// legacy meta/driver subtopic; empty when not published.
	struct CDriverInfo { string fromMeta, fromTopic; };
	map<string, CDriverInfo> m_DriversOnBroker;
	set<string> m_CleaningDevices;      // ids being removed from the broker
	bool m_Refreshing;
	string m_SyncToken;
	int m_SyncCounter;

	void dispatch(const string &topic, const string &payload, bool retained);
	void trackDriver(const CTopic &topic, const string &payload);
	// Whether the device on the broker was published by this service.
	bool isOwnDevice(const CDriverInfo &driver) const;
	void beginCleanup();
	void cleanUp(const string &topic, const string &payload, bool retained);
	void finishCleanup();
	void subscribeToZigbee();

	void onBridgeState(const string &payload);
	void onBridgeLogging(const string &payload);
	void onBridgeInfo(const string &payload);
	void onBridgeDevices(const string &payload);
	void onDeviceState(const string &device, const string &payload);
	void onControlCommand(const string &wbDevice, const string &control, const string &payload);

	static bool isReady(const Json::Value &info);
	void addDevice(const Json::Value &info);
	void subscribeToCommands(const CWbDevice &device);
	void publishDevice(CWbDevice &device);
	void publishChanges();
	void publish(const CMqttMessages &messages);
};
