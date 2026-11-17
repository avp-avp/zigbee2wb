#include "TestHelpers.h"

static long long FixedClock()
{
	return 1700000000000LL;
}

// A bridge connected to a fake broker, with the devices from data/bridge_devices.json.
struct CBridgeFixture
{
	CFakeTransport mqtt;
	CBridge bridge;

	CBridgeFixture(const string &config = "{}")
		:bridge(Settings(config), &mqtt, SilentLog())
	{
		bridge.setClock(FixedClock);
	}

	void loadDevices(const string &baseTopic = "zigbee2mqtt")
	{
		bridge.onMessage(baseTopic + "/bridge/devices", ReadDataFile("bridge_devices.json"));
	}

	void addDevice(const string &name, const string &modelId, const string &exposes = "[]")
	{
		bridge.onMessage("zigbee2mqtt/bridge/devices",
			"[{\"type\":\"Router\",\"friendly_name\":\"" + name + "\",\"model_id\":\"" + modelId + "\","
			"\"interview_completed\":true,\"definition\":{\"exposes\":" + exposes + "}}]");
	}

	string control(const string &device, const string &name) const
	{
		return mqtt.payload("/devices/" + device + "/controls/" + name);
	}

	string meta(const string &device, const string &name, const string &meta) const
	{
		return mqtt.payload("/devices/" + device + "/controls/" + name + "/meta/" + meta);
	}

	// The JSON metadata of a control, parsed.
	Json::Value metaJson(const string &device, const string &name) const
	{
		return ParseJson(mqtt.payload("/devices/" + device + "/controls/" + name + "/meta"));
	}

	// Payload of the latest "set" request sent to zigbee2mqtt for the device.
	string setRequest(const string &device, const string &base = "zigbee2mqtt") const
	{
		return mqtt.payload(base + "/" + device + "/set");
	}

	bool hasMeta(const string &device, const string &name, const string &meta) const
	{
		return mqtt.wasPublished("/devices/" + device + "/controls/" + name + "/meta/" + meta);
	}
};

TEST_SUITE("Bridge") {

TEST_CASE_FIXTURE(CBridgeFixture, "connecting subscribes to zigbee2mqtt and publishes the bridge device") {
	bridge.onConnect();

	CHECK(mqtt.isSubscribed("zigbee2mqtt/#"));
	CHECK(mqtt.isSubscribed("/devices/+/meta/driver"));
	CHECK(mqtt.isSubscribed("/devices/+/meta"));
	CHECK(mqtt.isSubscribed("/devices/zigbee/controls/+/on"));
	CHECK(mqtt.isSubscribed(CBridge::SYNC_TOPIC));
	CHECK(meta("zigbee", "refresh", "type") == "pushbutton");
	CHECK(meta("zigbee", "refresh", "readonly") == "0");
	CHECK(mqtt.payload("/devices/zigbee/meta") == "{\"driver\":\"zigbee2wb\",\"title\":{\"en\":\"Zigbee\"}}");
	CHECK(mqtt.payload("/devices/zigbee/meta/name") == "Zigbee");
	CHECK(meta("zigbee", "state", "type") == "text");
	CHECK(meta("zigbee", "log", "type") == "text");
	CHECK(meta("zigbee", "log_level", "type") == "text");
	CHECK(meta("zigbee", "permit_join", "type") == "switch");
	CHECK(meta("zigbee", "permit_join", "readonly") == "0");
	CHECK(mqtt.find("/devices/zigbee/meta/name")->retain);
}

TEST_CASE_FIXTURE(CBridgeFixture, "reconnecting restores the command subscriptions") {
	loadDevices();
	mqtt.clear();

	bridge.onConnect();
	CHECK(mqtt.isSubscribed("zigbee2mqtt/#"));
	CHECK(mqtt.isSubscribed("/devices/Socket 1/controls/+/on"));
	CHECK(mqtt.isSubscribed("/devices/Outdoor/controls/+/on"));
}

TEST_CASE_FIXTURE(CBridgeFixture, "bridge/devices creates Wiren Board devices") {
	loadDevices();

	CHECK(bridge.deviceCount() == 6);
	CHECK((bridge.findDevice("Coordinator") == NULL));
	REQUIRE((bridge.findDevice("Socket 1") != NULL));

	CHECK(mqtt.payload("/devices/Socket 1/meta/name") == "Socket 1");
	CHECK(mqtt.payload("/devices/Socket 1/meta/driver") == "zigbee2wb");
	CHECK(mqtt.payload("/devices/Socket 1/meta") == "{\"driver\":\"zigbee2wb\",\"title\":{\"en\":\"Socket 1\"}}");
	CHECK(meta("Socket 1", "lastSeen", "type") == "text");
	CHECK(meta("Socket 1", "lastSeen", "readonly") == "1");
	CHECK(mqtt.find("/devices/Socket 1/meta/name")->retain);
	CHECK(mqtt.isSubscribed("/devices/Socket 1/controls/+/on"));

	SUBCASE("controls for values appear only after the first report") {
		CHECK_FALSE(hasMeta("Socket 1", "state", "type"));
	}
	SUBCASE("actions are published as push buttons") {
		CHECK(meta("Z-Switch", "button_1_single", "type") == "pushbutton");
		CHECK(meta("Z-Switch", "button_6_triple", "type") == "pushbutton");
		CHECK_FALSE(hasMeta("Z-Switch", "command", "type"));   // operation_mode is not an action
		CHECK_FALSE(mqtt.wasPublished("/devices/Z-Switch/controls/button_1_single"));
	}
	SUBCASE("set-only values get controls right away") {
		CHECK(meta("Curtain 1", "set_upper_limit", "type") == "text");
		CHECK(meta("Curtain 1", "set_upper_limit", "readonly") == "0");
	}
}

TEST_CASE_FIXTURE(CBridgeFixture, "devices that are not ready are skipped") {
	bridge.onMessage("zigbee2mqtt/bridge/devices",
		"[{\"type\":\"Coordinator\",\"friendly_name\":\"Coordinator\",\"interview_state\":\"SUCCESSFUL\"},"
		"{\"type\":\"Router\",\"friendly_name\":\"pending\",\"interview_state\":\"PENDING\"},"
		"{\"type\":\"Router\",\"friendly_name\":\"joining\",\"interview_state\":\"IN_PROGRESS\"},"
		"{\"type\":\"Router\",\"friendly_name\":\"failed\",\"interview_state\":\"FAILED\",\"interview_completed\":true},"
		"{\"type\":\"Router\",\"friendly_name\":\"old_joining\",\"interview_completed\":false},"
		"{\"type\":\"Router\",\"friendly_name\":\"no_interview_flag\"},"
		"{\"type\":\"Router\",\"interview_state\":\"SUCCESSFUL\"},"
		"\"junk\"]");

	CHECK(bridge.deviceCount() == 0);
	CHECK(mqtt.published.empty());
}

TEST_CASE_FIXTURE(CBridgeFixture, "the interview result is read from interview_state or the deprecated flag") {
	bridge.onMessage("zigbee2mqtt/bridge/devices",
		"[{\"type\":\"Router\",\"friendly_name\":\"new_api\",\"interview_state\":\"SUCCESSFUL\"},"
		"{\"type\":\"Router\",\"friendly_name\":\"old_api\",\"interview_completed\":true}]");

	CHECK((bridge.findDevice("new_api") != NULL));
	CHECK((bridge.findDevice("old_api") != NULL));
}

TEST_CASE_FIXTURE(CBridgeFixture, "a friendly name with slashes is mapped to a topic-safe device") {
	addDevice("kitchen/lamp", "X",
		"[{\"type\":\"binary\",\"property\":\"state\",\"access\":7,\"value_on\":\"ON\",\"value_off\":\"OFF\"}]");

	CHECK(mqtt.payload("/devices/kitchen_lamp/meta/name") == "kitchen/lamp");
	CHECK(mqtt.isSubscribed("/devices/kitchen_lamp/controls/+/on"));

	bridge.onMessage("zigbee2mqtt/kitchen/lamp", "{\"state\":\"ON\"}");
	CHECK(control("kitchen_lamp", "state") == "1");

	bridge.onMessage("/devices/kitchen_lamp/controls/state/on", "0");
	CHECK(setRequest("kitchen/lamp") == "{\"state\":\"OFF\"}");

	SUBCASE("a device whose id is already taken is skipped") {
		mqtt.clear();
		addDevice("kitchen_lamp", "X");
		addDevice("zigbee", "X");
		CHECK(bridge.deviceCount() == 1);
		CHECK(mqtt.published.empty());
	}
	SUBCASE("subtopics of a device are not its state") {
		mqtt.clear();
		bridge.onMessage("zigbee2mqtt/kitchen/lamp/availability", "{\"state\":\"online\"}");
		bridge.onMessage("zigbee2mqtt/kitchen/lamp/set", "{\"state\":\"ON\"}");
		CHECK(mqtt.published.empty());
	}
}

TEST_CASE_FIXTURE(CBridgeFixture, "a device without a definition gets only lastSeen") {
	bridge.onMessage("zigbee2mqtt/bridge/devices",
		"[{\"type\":\"EndDevice\",\"friendly_name\":\"unsupported\",\"interview_completed\":true,\"definition\":null}]");

	REQUIRE((bridge.findDevice("unsupported") != NULL));
	CHECK(bridge.findDevice("unsupported")->wbDevice().controlCount() == 1);
}

TEST_CASE_FIXTURE(CBridgeFixture, "a repeated bridge/devices does not recreate known devices") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/Socket 1", "{\"state\":\"ON\"}");
	mqtt.clear();

	loadDevices();
	CHECK(bridge.deviceCount() == 6);
	CHECK(mqtt.published.empty());
	CHECK(mqtt.subscriptions.empty());
	CHECK(bridge.findDevice("Socket 1")->wbDevice().value("state") == "1");
}

TEST_CASE_FIXTURE(CBridgeFixture, "a sensor report is published as values with units") {
	loadDevices();
	mqtt.clear();

	bridge.onMessage("zigbee2mqtt/Outdoor",
		"{\"battery\":100,\"humidity\":45.5,\"linkquality\":120,\"pressure\":1013.25,"
		"\"temperature\":21.5,\"voltage\":3025}");

	CHECK(control("Outdoor", "temperature") == "21.5");
	CHECK(meta("Outdoor", "temperature", "type") == "value");
	CHECK(meta("Outdoor", "temperature", "units") == "deg C");
	CHECK(meta("Outdoor", "temperature", "readonly") == "1");
	CHECK(control("Outdoor", "humidity") == "45.5");
	CHECK(meta("Outdoor", "humidity", "type") == "value");
	CHECK(meta("Outdoor", "humidity", "units") == "%, RH");
	CHECK(control("Outdoor", "pressure") == "760.0");
	CHECK(meta("Outdoor", "pressure", "type") == "value");
	CHECK(meta("Outdoor", "pressure", "units") == "mmHg");
	CHECK(control("Outdoor", "voltage") == "3.025");
	CHECK(meta("Outdoor", "voltage", "type") == "value");
	CHECK(meta("Outdoor", "voltage", "units") == "V");
	CHECK(control("Outdoor", "battery") == "100");
	CHECK(meta("Outdoor", "battery", "units") == "%");
	CHECK(meta("Outdoor", "battery", "max") == "100");
	CHECK(meta("Outdoor", "linkquality", "type") == "value");
	CHECK(meta("Outdoor", "linkquality", "max") == "255");
	CHECK_FALSE(hasMeta("Outdoor", "linkquality", "units"));
	CHECK_FALSE(hasMeta("Outdoor", "temperature", "max"));
	CHECK(control("Outdoor", "lastSeen") == "1700000000000");
	CHECK(mqtt.find("/devices/Outdoor/controls/temperature")->retain);

	Json::Value temperature = metaJson("Outdoor", "temperature");
	CHECK(temperature["type"].asString() == "value");
	CHECK(temperature["units"].asString() == "deg C");
	CHECK(temperature["readonly"].asBool());
	CHECK_FALSE(temperature.isMember("max"));
}

TEST_CASE_FIXTURE(CBridgeFixture, "no deprecated control types are published") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/Outdoor",
		"{\"battery\":100,\"humidity\":45.5,\"pressure\":1013.25,\"temperature\":21.5,\"voltage\":3025}");
	bridge.onMessage("zigbee2mqtt/Socket 1",
		"{\"state\":\"ON\",\"power\":12.5,\"current\":0.05,\"voltage\":230,\"energy\":1.2}");

	const char *allowed[] = { "switch", "pushbutton", "range", "text", "value" };
	for (size_t i = 0; i < mqtt.published.size(); i++) {
		const string &topic = mqtt.published[i].topic;
		const string suffix = "/meta/type";
		if (topic.size() < suffix.size() || topic.compare(topic.size() - suffix.size(), suffix.size(), suffix) != 0)
			continue;

		CAPTURE(topic);
		const string &type = mqtt.published[i].payload;
		bool isAllowed = false;
		for (size_t j = 0; j < sizeof(allowed) / sizeof(allowed[0]); j++)
			isAllowed = isAllowed || type == allowed[j];
		CHECK_MESSAGE(isAllowed, "type " << type);
	}

	CHECK(meta("Socket 1", "power", "units") == "W");
	CHECK(meta("Socket 1", "current", "units") == "A");
	CHECK(meta("Socket 1", "voltage", "units") == "V");
	CHECK(control("Socket 1", "voltage") == "230");
	CHECK(meta("Socket 1", "energy", "units") == "kWh");
}

TEST_CASE_FIXTURE(CBridgeFixture, "metadata is published only when new controls appear") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/Outdoor", "{\"temperature\":21.5}");
	mqtt.clear();

	bridge.onMessage("zigbee2mqtt/Outdoor", "{\"temperature\":22}");
	CHECK(control("Outdoor", "temperature") == "22");
	CHECK(control("Outdoor", "lastSeen") == "1700000000000");
	CHECK(mqtt.published.size() == 2);

	SUBCASE("and again for the next new control") {
		bridge.onMessage("zigbee2mqtt/Outdoor", "{\"humidity\":50}");
		CHECK(meta("Outdoor", "humidity", "units") == "%, RH");
		CHECK(mqtt.payload("/devices/Outdoor/meta/name") == "Outdoor");
	}
}

TEST_CASE_FIXTURE(CBridgeFixture, "a switch is controlled in both directions") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/Socket 1", "{\"state\":\"ON\",\"power\":12.5}");

	CHECK(control("Socket 1", "state") == "1");
	CHECK(meta("Socket 1", "state", "type") == "switch");
	CHECK(meta("Socket 1", "state", "readonly") == "0");
	CHECK(control("Socket 1", "power") == "12.5");

	mqtt.clear();
	bridge.onMessage("/devices/Socket 1/controls/state/on", "0");
	REQUIRE(mqtt.published.size() == 1);
	CHECK(mqtt.published[0].topic == "zigbee2mqtt/Socket 1/set");
	CHECK(mqtt.published[0].payload == "{\"state\":\"OFF\"}");
	CHECK_FALSE(mqtt.published[0].retain);

	bridge.onMessage("/devices/Socket 1/controls/state/on", "1");
	CHECK(setRequest("Socket 1") == "{\"state\":\"ON\"}");

	SUBCASE("read-only and unknown controls are not forwarded") {
		mqtt.clear();
		bridge.onMessage("/devices/Socket 1/controls/power/on", "100");
		bridge.onMessage("/devices/Socket 1/controls/nonsense/on", "1");
		CHECK(mqtt.published.empty());
	}
}

TEST_CASE_FIXTURE(CBridgeFixture, "boolean sensors become read-only switches") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/motion1", "{\"occupancy\":true}");
	CHECK(control("motion1", "occupancy") == "1");
	CHECK(meta("motion1", "occupancy", "type") == "switch");
	CHECK(meta("motion1", "occupancy", "readonly") == "1");

	bridge.onMessage("zigbee2mqtt/motion1", "{\"occupancy\":false}");
	CHECK(control("motion1", "occupancy") == "0");
}

TEST_CASE_FIXTURE(CBridgeFixture, "a light gets a switch and ranges") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/zlStairs", "{\"state\":\"OFF\",\"brightness\":128,\"color_temp\":370}");

	CHECK(control("zlStairs", "state") == "0");
	CHECK(control("zlStairs", "brightness") == "128");
	CHECK(meta("zlStairs", "brightness", "type") == "range");
	CHECK(meta("zlStairs", "brightness", "max") == "254");
	CHECK(meta("zlStairs", "color_temp", "min") == "153");
	CHECK(meta("zlStairs", "color_temp", "max") == "500");
	CHECK(metaJson("zlStairs", "brightness")["max"].asInt() == 254);
	CHECK_FALSE(metaJson("zlStairs", "brightness")["readonly"].asBool());

	mqtt.clear();
	bridge.onMessage("/devices/zlStairs/controls/brightness/on", "200");
	CHECK(setRequest("zlStairs") == "{\"brightness\":200}");
}

TEST_CASE_FIXTURE(CBridgeFixture, "a cover state is text and its position is a range") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/Curtain 1", "{\"state\":\"OPEN\",\"position\":40}");

	CHECK(control("Curtain 1", "state") == "OPEN");
	CHECK(meta("Curtain 1", "state", "type") == "text");
	CHECK(meta("Curtain 1", "position", "type") == "range");
	CHECK(meta("Curtain 1", "position", "max") == "100");

	mqtt.clear();
	bridge.onMessage("/devices/Curtain 1/controls/state/on", "CLOSE");
	CHECK(setRequest("Curtain 1") == "{\"state\":\"CLOSE\"}");
}

TEST_CASE_FIXTURE(CBridgeFixture, "a remote action presses its button") {
	loadDevices();
	bridge.onMessage("zigbee2mqtt/Z-Switch", "{\"action\":\"button_2_double\",\"battery\":97}");

	CHECK(control("Z-Switch", "action") == "button_2_double");
	CHECK(control("Z-Switch", "button_2_double") == "1");
	CHECK_FALSE(mqtt.find("/devices/Z-Switch/controls/button_2_double")->retain);

	SUBCASE("the other buttons stay untouched") {
		mqtt.clear();
		bridge.onMessage("zigbee2mqtt/Z-Switch", "{\"action\":\"button_3_hold\"}");
		CHECK(control("Z-Switch", "button_3_hold") == "1");
		CHECK_FALSE(mqtt.wasPublished("/devices/Z-Switch/controls/button_2_double"));
		CHECK_FALSE(mqtt.wasPublished("/devices/Z-Switch/controls/button_1_single"));
	}
	SUBCASE("an old press is not repeated when the device is described again") {
		mqtt.clear();
		bridge.onMessage("zigbee2mqtt/Z-Switch", "{\"voltage\":3000}");   // a new control
		CHECK(hasMeta("Z-Switch", "voltage", "type"));
		CHECK_FALSE(mqtt.wasPublished("/devices/Z-Switch/controls/button_2_double"));
	}
}

TEST_CASE_FIXTURE(CBridgeFixture, "messages for unknown devices are ignored") {
	loadDevices();
	mqtt.clear();

	bridge.onMessage("zigbee2mqtt/stranger", "{\"state\":\"ON\"}");
	bridge.onMessage("/devices/stranger/controls/state/on", "1");
	CHECK(mqtt.published.empty());
	CHECK(bridge.deviceCount() == 6);
}

TEST_CASE_FIXTURE(CBridgeFixture, "empty and malformed payloads do not break the bridge") {
	loadDevices();
	mqtt.clear();

	bridge.onMessage("zigbee2mqtt/Socket 1", "");
	bridge.onMessage("zigbee2mqtt/Socket 1", "{\"state\":");
	bridge.onMessage("zigbee2mqtt/bridge/devices", "not json");
	bridge.onMessage("zigbee2mqtt/bridge/devices", "{}");
	bridge.onMessage("zigbee2mqtt/bridge/logging", "");
	bridge.onMessage("zigbee2mqtt/bridge/info", "[");
	CHECK(mqtt.published.empty());

	bridge.onMessage("zigbee2mqtt/Socket 1", "{\"state\":\"ON\"}");
	CHECK(control("Socket 1", "state") == "1");
}

TEST_CASE_FIXTURE(CBridgeFixture, "groups and other bridge topics are ignored") {
	bridge.onMessage("zigbee2mqtt/bridge/groups", "[{\"friendly_name\":\"all\",\"scenes\":[]}]");
	bridge.onMessage("zigbee2mqtt/bridge/config", "{\"permit_join\":true}");
	bridge.onMessage("zigbee2mqtt/bridge/event", "{\"type\":\"device_joined\",\"data\":{}}");
	CHECK(bridge.deviceCount() == 0);
	CHECK(mqtt.published.empty());
}

TEST_CASE_FIXTURE(CBridgeFixture, "bridge state is shown in the zigbee device") {
	SUBCASE("json") {
		bridge.onMessage("zigbee2mqtt/bridge/state", "{\"state\":\"offline\"}");
		CHECK(control("zigbee", "state") == "offline");
	}
	SUBCASE("plain text of zigbee2mqtt 1.x") {
		bridge.onMessage("zigbee2mqtt/bridge/state", "online");
		CHECK(control("zigbee", "state") == "online");
	}
}

TEST_CASE_FIXTURE(CBridgeFixture, "warnings and errors from the bridge log are published") {
	bridge.onMessage("zigbee2mqtt/bridge/logging", "{\"level\":\"info\",\"message\":\"started\"}");
	CHECK(mqtt.published.empty());

	bridge.onMessage("zigbee2mqtt/bridge/logging",
		"{\"level\":\"warning\",\"message\":\"low battery\",\"namespace\":\"z2m\"}");
	CHECK(control("zigbee", "log") == "low battery");

	bridge.onMessage("zigbee2mqtt/bridge/logging", "{\"level\":\"error\",\"message\":\"publish failed\"}");
	CHECK(control("zigbee", "log") == "publish failed");
}

TEST_CASE_FIXTURE(CBridgeFixture, "bridge info is shown in the zigbee device") {
	bridge.onMessage("zigbee2mqtt/bridge/info",
		"{\"version\":\"2.13.0\",\"log_level\":\"info\",\"permit_join\":true,\"permit_join_end\":1733666394}");
	CHECK(control("zigbee", "log_level") == "info");
	CHECK(control("zigbee", "permit_join") == "1");

	bridge.onMessage("zigbee2mqtt/bridge/info", "{\"log_level\":\"debug\",\"permit_join\":false}");
	CHECK(control("zigbee", "log_level") == "debug");
	CHECK(control("zigbee", "permit_join") == "0");
}

TEST_CASE("permit_join sends a timed request and waits for bridge info") {
	CBridgeFixture f("{\"mqtt\":{\"base_topic\":\"z2m\"}}");
	const string command = "/devices/zigbee/controls/permit_join/on";
	const string request = "z2m/bridge/request/permit_join";

	f.bridge.onMessage(command, "1", true);
	f.bridge.onMessage(command, "invalid");
	CHECK(f.mqtt.published.empty());

	f.bridge.onMessage(command, "1");
	REQUIRE(f.mqtt.published.size() == 1);
	CHECK(f.mqtt.payload(request) == "{\"time\":254}");
	CHECK_FALSE(f.mqtt.published[0].retain);

	f.bridge.onMessage("z2m/bridge/info", "{\"permit_join\":true}");
	CHECK(f.control("zigbee", "permit_join") == "1");

	f.mqtt.clear();
	f.bridge.onMessage(command, "0");
	REQUIRE(f.mqtt.published.size() == 1);
	CHECK(f.mqtt.payload(request) == "{\"time\":0}");
	CHECK_FALSE(f.mqtt.published[0].retain);

	f.bridge.onMessage("z2m/bridge/info", "{\"permit_join\":false}");
	CHECK(f.control("zigbee", "permit_join") == "0");
}

TEST_CASE("the configured base topic is used everywhere") {
	CBridgeFixture f("{\"mqtt\":{\"base_topic\":\"z2m\"}}");
	f.bridge.onConnect();
	CHECK(f.mqtt.isSubscribed("z2m/#"));

	f.loadDevices("zigbee2mqtt");
	CHECK(f.bridge.deviceCount() == 0);

	f.loadDevices("z2m");
	CHECK(f.bridge.deviceCount() == 6);

	f.bridge.onMessage("z2m/Socket 1", "{\"state\":\"ON\"}");
	CHECK(f.control("Socket 1", "state") == "1");

	f.bridge.onMessage("/devices/Socket 1/controls/state/on", "0");
	CHECK(f.setRequest("Socket 1", "z2m") == "{\"state\":\"OFF\"}");

	f.bridge.onMessage("z2m/bridge/state", "online");
	CHECK(f.control("zigbee", "state") == "online");
}

TEST_CASE("model templates from the config are applied") {
	CBridgeFixture f(
		"{\"converters\":{\"cover\":{\"OPEN\":\"1\",\"CLOSE\":\"0\"}},"
		"\"models\":{"
		"\"TS110F\":{\"controls\":{\"brightness\":{\"type\":\"range\",\"max\":255}}},"
		"\"TS0601\":{\"controls\":{\"state\":{\"type\":\"switch\",\"converter\":\"cover\"}}},"
		"\"lumi.weather\":{\"json_template\":true}}}");

	SUBCASE("type and maximum") {
		f.addDevice("dimmer", "TS110F", "[{\"type\":\"numeric\",\"property\":\"brightness\",\"access\":7}]");
		f.bridge.onMessage("zigbee2mqtt/dimmer", "{\"brightness\":40}");
		CHECK(f.meta("dimmer", "brightness", "type") == "range");
		CHECK(f.meta("dimmer", "brightness", "max") == "255");
	}
	SUBCASE("converter") {
		f.loadDevices();
		f.bridge.onMessage("zigbee2mqtt/Curtain 1", "{\"state\":\"OPEN\"}");
		CHECK(f.meta("Curtain 1", "state", "type") == "switch");
		CHECK(f.control("Curtain 1", "state") == "1");

		f.bridge.onMessage("/devices/Curtain 1/controls/state/on", "0");
		CHECK(f.setRequest("Curtain 1") == "{\"state\":\"CLOSE\"}");
	}
	SUBCASE("raw json control") {
		f.loadDevices();
		CHECK(f.meta("Outdoor", "raw", "type") == "text");
		CHECK_FALSE(f.hasMeta("motion1", "raw", "type"));

		f.bridge.onMessage("zigbee2mqtt/Outdoor", "{\"temperature\":3}");
		CHECK(f.control("Outdoor", "raw") == "{\"temperature\":3}");
	}
}

TEST_CASE("the action field name is configurable") {
	CBridgeFixture f("{\"general\":{\"action_field\":\"click\"}}");
	f.addDevice("button", "WXKG01LM",
		"[{\"type\":\"enum\",\"property\":\"click\",\"access\":1,\"values\":[\"single\",\"long\"]}]");

	CHECK(f.meta("button", "long", "type") == "pushbutton");
	f.bridge.onMessage("zigbee2mqtt/button", "{\"click\":\"long\"}");
	CHECK(f.control("button", "long") == "1");
}

}
