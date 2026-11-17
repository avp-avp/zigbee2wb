// MQTT topic layout of zigbee2mqtt and Wiren Board.
#pragma once
#include "zigbee2wb.h"

struct CTopic
{
	enum Kind
	{
		Unknown,
		BridgeState,     // <base>/bridge/state
		BridgeLogging,   // <base>/bridge/logging
		BridgeInfo,      // <base>/bridge/info
		BridgeDevices,   // <base>/bridge/devices
		BridgeGroups,    // <base>/bridge/groups
		DeviceState,     // <base>/<friendly name>, the name may contain slashes
		ControlCommand,  // /devices/<device>/controls/<control>/on
		DeviceDriver,    // /devices/<device>/meta/driver, legacy
		DeviceMeta       // /devices/<device>/meta, JSON
	};

	Kind kind;
	string device, control;

	CTopic() :kind(Unknown) {}
};

CTopic ParseTopic(const string &baseTopic, const string &topic);

// Subscription filter for everything zigbee2mqtt publishes.
string ZigbeeFilter(const string &baseTopic);
// Subscription filter for commands sent to the controls of a Wiren Board device.
string ControlCommandFilter(const string &device);
// Subscription filters for the driver names of all Wiren Board devices:
// the legacy subtopic and the JSON metadata.
string DeviceDriverFilter();
string DeviceMetaFilter();
// Subscription filter for every topic of a Wiren Board device.
string DeviceFilter(const string &device);
// Id of the Wiren Board device a topic belongs to; empty for other topics.
string WbDeviceOf(const string &topic);
// Topic that accepts a JSON with new property values of a zigbee2mqtt device.
string ZigbeeSetTopic(const string &baseTopic, const string &device);
