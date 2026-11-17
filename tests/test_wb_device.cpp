#include "TestHelpers.h"
#include "../zigbee2wb/WbDevice.h"

// Payload of the only message in the topic; fails the test when it is missing.
static const CMqttMessage *Find(const CMqttMessages &messages, const string &topic)
{
	for (size_t i = 0; i < messages.size(); i++) {
		if (messages[i].topic == topic)
			return &messages[i];
	}
	return NULL;
}

static string Payload(const CMqttMessages &messages, const string &topic)
{
	const CMqttMessage *message = Find(messages, topic);
	if (!message) {
		FAIL_CHECK("no message in " << topic);
		return "";
	}
	return message->payload;
}

TEST_SUITE("WbDevice") {

TEST_CASE("MakeId keeps names but removes topic separators and wildcards") {
	CHECK(CWbDevice::MakeId("Socket 1") == "Socket 1");
	CHECK(CWbDevice::MakeId("kitchen/floor_light") == "kitchen_floor_light");
	CHECK(CWbDevice::MakeId("a+b#c") == "a_b_c");
}

TEST_CASE("controls are added once and keep their order") {
	CWbDevice device("lamp", "Lamp");
	CWbControl &state = device.addControl("state", CWBControl::Switch, false);
	CWbControl &power = device.addControl("power", CWBControl::Generic, true);
	CHECK(state.order == 1);
	CHECK(power.order == 2);
	CHECK(device.controlCount() == 2);

	CWbControl &again = device.addControl("state", CWBControl::Text, true);
	CHECK(&again == &state);
	CHECK(again.type == CWBControl::Switch);
	CHECK(device.controlCount() == 2);

	CHECK(device.hasControl("power"));
	CHECK_FALSE(device.hasControl("voltage"));
	CHECK((device.findControl("voltage") == NULL));
	CHECK(device.findControl("power")->readonly);
}

TEST_CASE("values of unknown controls cannot be read or written") {
	CWbDevice device("lamp", "Lamp");
	CHECK_THROWS_AS(device.set("state", "1"), CHaException);
	CHECK_THROWS_AS(device.value("state"), CHaException);
}

TEST_CASE("the device is described with JSON metadata and legacy subtopics") {
	CWbDevice device("lamp", "Kitchen lamp");
	CMqttMessages messages;
	device.describe(messages);

	CHECK(Payload(messages, "/devices/lamp/meta") == "{\"driver\":\"zigbee2wb\",\"title\":{\"en\":\"Kitchen lamp\"}}");
	CHECK(Payload(messages, "/devices/lamp/meta/name") == "Kitchen lamp");
	CHECK(Payload(messages, "/devices/lamp/meta/driver") == "zigbee2wb");
	CHECK(Find(messages, "/devices/lamp/meta")->retain);
}

TEST_CASE("a control is described with JSON metadata and legacy subtopics") {
	CWbDevice device("lamp", "Lamp");
	CWbControl &brightness = device.addControl("brightness", CWBControl::Range, false);
	brightness.setMin(1);
	brightness.setMax(254);
	device.set("brightness", "100");

	CMqttMessages messages;
	device.describe(messages);
	const string topic = "/devices/lamp/controls/brightness";

	CHECK(Payload(messages, topic + "/meta") == "{\"max\":254,\"min\":1,\"order\":1,\"readonly\":false,\"type\":\"range\"}");
	CHECK(Payload(messages, topic + "/meta/type") == "range");
	CHECK(Payload(messages, topic + "/meta/readonly") == "0");
	CHECK(Payload(messages, topic + "/meta/order") == "1");
	CHECK(Payload(messages, topic + "/meta/min") == "1");
	CHECK(Payload(messages, topic + "/meta/max") == "254");
	CHECK(Payload(messages, topic) == "100");
	CHECK(Find(messages, topic)->retain);
	CHECK(Find(messages, topic + "/meta")->retain);
}

TEST_CASE("limits are published only when they are set") {
	CWbDevice device("lamp", "Lamp");
	device.addControl("mode", CWBControl::Text, true);

	CMqttMessages messages;
	device.describe(messages);
	CHECK(Payload(messages, "/devices/lamp/controls/mode/meta") == "{\"order\":1,\"readonly\":true,\"type\":\"text\"}");
	CHECK(Payload(messages, "/devices/lamp/controls/mode/meta/readonly") == "1");
	CHECK((Find(messages, "/devices/lamp/controls/mode/meta/max") == NULL));
	CHECK((Find(messages, "/devices/lamp/controls/mode/meta/min") == NULL));
}

TEST_CASE("units are published only for the value type") {
	CWbDevice device("sensor", "Sensor");
	device.addControl("temperature", CWBControl::Generic, true).units = "deg C";
	device.addControl("position", CWBControl::Range, false).units = "%";

	CMqttMessages messages;
	device.describe(messages);
	CHECK(Payload(messages, "/devices/sensor/controls/temperature/meta") ==
		"{\"order\":1,\"readonly\":true,\"type\":\"value\",\"units\":\"deg C\"}");
	CHECK(Payload(messages, "/devices/sensor/controls/temperature/meta/units") == "deg C");
	CHECK((Find(messages, "/devices/sensor/controls/position/meta/units") == NULL));
}

TEST_CASE("controls without a value are described without one") {
	CWbDevice device("lamp", "Lamp");
	device.addControl("state", CWBControl::Switch, false);

	CMqttMessages messages;
	device.describe(messages);
	CHECK((Find(messages, "/devices/lamp/controls/state") == NULL));
	CHECK((Find(messages, "/devices/lamp/controls/state/meta/type") != NULL));
}

TEST_CASE("only values set since the last publication are collected") {
	CWbDevice device("lamp", "Lamp");
	device.addControl("state", CWBControl::Switch, false);
	device.addControl("power", CWBControl::Generic, true);
	device.set("state", "1");
	device.set("power", "5");

	CMqttMessages messages;
	device.collectChanges(messages);
	CHECK(messages.size() == 2);

	messages.clear();
	device.collectChanges(messages);
	CHECK(messages.empty());

	device.set("power", "5");   // the same value is still a new report
	device.collectChanges(messages);
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].topic == "/devices/lamp/controls/power");
	CHECK(messages[0].payload == "5");
	CHECK(messages[0].retain);

	SUBCASE("describing the device also counts as publication") {
		device.set("state", "0");
		messages.clear();
		device.describe(messages);
		CHECK(Payload(messages, "/devices/lamp/controls/state") == "0");

		messages.clear();
		device.collectChanges(messages);
		CHECK(messages.empty());
	}
}

TEST_CASE("a push button press is not retained and not repeated") {
	CWbDevice device("remote", "Remote");
	device.addControl("single", CWBControl::PushButton, true);
	device.set("single", "1");

	CMqttMessages messages;
	device.describe(messages);
	CHECK(Payload(messages, "/devices/remote/controls/single/meta/type") == "pushbutton");
	CHECK((Find(messages, "/devices/remote/controls/single") == NULL));

	messages.clear();
	device.collectChanges(messages);
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].payload == "1");
	CHECK_FALSE(messages[0].retain);

	messages.clear();
	device.describe(messages);
	device.collectChanges(messages);
	CHECK((Find(messages, "/devices/remote/controls/single") == NULL));
}

}
