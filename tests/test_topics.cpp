#include "TestHelpers.h"
#include "../zigbee2wb/Topics.h"

static CTopic::Kind Kind(const string &topic, const string &base = "zigbee2mqtt")
{
	return ParseTopic(base, topic).kind;
}

TEST_SUITE("Topics") {

TEST_CASE("bridge topics are recognized") {
	CHECK(Kind("zigbee2mqtt/bridge/state") == CTopic::BridgeState);
	CHECK(Kind("zigbee2mqtt/bridge/logging") == CTopic::BridgeLogging);
	CHECK(Kind("zigbee2mqtt/bridge/info") == CTopic::BridgeInfo);
	CHECK(Kind("zigbee2mqtt/bridge/devices") == CTopic::BridgeDevices);
	CHECK(Kind("zigbee2mqtt/bridge/groups") == CTopic::BridgeGroups);
}

TEST_CASE("other bridge topics are ignored") {
	CHECK(Kind("zigbee2mqtt/bridge") == CTopic::Unknown);
	CHECK(Kind("zigbee2mqtt/bridge/config") == CTopic::Unknown);
	CHECK(Kind("zigbee2mqtt/bridge/health") == CTopic::Unknown);
	CHECK(Kind("zigbee2mqtt/bridge/request/permit_join") == CTopic::Unknown);
	CHECK(Kind("zigbee2mqtt/bridge/response/device/rename") == CTopic::Unknown);
}

TEST_CASE("a device state topic carries the device name") {
	CTopic topic = ParseTopic("zigbee2mqtt", "zigbee2mqtt/Socket 1");
	CHECK(topic.kind == CTopic::DeviceState);
	CHECK(topic.device == "Socket 1");
}

TEST_CASE("a friendly name may contain slashes") {
	CTopic topic = ParseTopic("zigbee2mqtt", "zigbee2mqtt/kitchen/floor_light");
	CHECK(topic.kind == CTopic::DeviceState);
	CHECK(topic.device == "kitchen/floor_light");
}

TEST_CASE("the configured base topic is honoured") {
	CHECK(Kind("z2m/bridge/devices", "z2m") == CTopic::BridgeDevices);
	CHECK(Kind("zigbee2mqtt/bridge/devices", "z2m") == CTopic::Unknown);
	CHECK(Kind("z2mx/bridge/devices", "z2m") == CTopic::Unknown);

	CTopic topic = ParseTopic("home/z2m", "home/z2m/lamp");
	CHECK(topic.kind == CTopic::DeviceState);
	CHECK(topic.device == "lamp");
}

TEST_CASE("a Wiren Board command topic carries device and control") {
	CTopic topic = ParseTopic("zigbee2mqtt", "/devices/Socket 1/controls/state/on");
	CHECK(topic.kind == CTopic::ControlCommand);
	CHECK(topic.device == "Socket 1");
	CHECK(topic.control == "state");
}

TEST_CASE("a driver topic carries the device") {
	CTopic topic = ParseTopic("zigbee2mqtt", "/devices/Socket 1/meta/driver");
	CHECK(topic.kind == CTopic::DeviceDriver);
	CHECK(topic.device == "Socket 1");
}

TEST_CASE("a JSON metadata topic carries the device") {
	CTopic topic = ParseTopic("zigbee2mqtt", "/devices/Socket 1/meta");
	CHECK(topic.kind == CTopic::DeviceMeta);
	CHECK(topic.device == "Socket 1");
}

TEST_CASE("WbDeviceOf extracts the device id") {
	CHECK(WbDeviceOf("/devices/lamp/meta") == "lamp");
	CHECK(WbDeviceOf("/devices/Socket 1/controls/state/meta/type") == "Socket 1");
	CHECK(WbDeviceOf("/devices/lamp") == "");
	CHECK(WbDeviceOf("zigbee2mqtt/lamp/set") == "");
	CHECK(WbDeviceOf("") == "");
}

TEST_CASE("other Wiren Board topics are ignored") {
	CHECK(Kind("/devices/lamp/controls/state") == CTopic::Unknown);
	CHECK(Kind("/devices/lamp/controls/state/meta/type") == CTopic::Unknown);
	CHECK(Kind("/devices/lamp/meta/name") == CTopic::Unknown);
	CHECK(Kind("/devices/lamp/controls/state/off") == CTopic::Unknown);
}

TEST_CASE("unrelated topics are ignored") {
	CHECK(Kind("") == CTopic::Unknown);
	CHECK(Kind("zigbee2mqtt") == CTopic::Unknown);
	CHECK(Kind("homeassistant/status") == CTopic::Unknown);
}

TEST_CASE("topic builders") {
	CHECK(ZigbeeFilter("z2m") == "z2m/#");
	CHECK(ControlCommandFilter("lamp") == "/devices/lamp/controls/+/on");
	CHECK(DeviceDriverFilter() == "/devices/+/meta/driver");
	CHECK(DeviceMetaFilter() == "/devices/+/meta");
	CHECK(DeviceFilter("lamp") == "/devices/lamp/#");
	CHECK(ZigbeeSetTopic("z2m", "kitchen/lamp") == "z2m/kitchen/lamp/set");
}

}
