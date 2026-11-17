#include "TestHelpers.h"
#include "../zigbee2wb/ZigbeeDevice.h"

static const long long NOW = 1700000000000LL;

static CExposeMap Exposes(const string &json)
{
	CExposeMap result;
	ParseExposes(ParseJson(json), result);
	return result;
}

static const char *LAMP_EXPOSES =
	"[{\"type\":\"light\",\"features\":["
	"{\"type\":\"binary\",\"property\":\"state\",\"access\":7,\"value_on\":\"ON\",\"value_off\":\"OFF\"},"
	"{\"type\":\"numeric\",\"property\":\"brightness\",\"access\":7,\"value_min\":1,\"value_max\":254}]},"
	"{\"type\":\"binary\",\"property\":\"child_lock\",\"access\":3,\"value_on\":true,\"value_off\":false},"
	"{\"type\":\"numeric\",\"property\":\"voltage\",\"access\":1,\"unit\":\"mV\"},"
	"{\"type\":\"numeric\",\"property\":\"temperature\",\"access\":1,\"unit\":\"\xC2\xB0""C\"},"
	"{\"type\":\"numeric\",\"property\":\"calibration\",\"access\":3,\"unit\":\"\xC2\xB0""C\"},"
	"{\"type\":\"numeric\",\"property\":\"linkquality\",\"access\":1,\"unit\":\"lqi\",\"value_max\":255},"
	"{\"type\":\"enum\",\"property\":\"mode\",\"access\":3,\"values\":[\"auto\",\"manual\"]},"
	"{\"type\":\"enum\",\"property\":\"action\",\"access\":1,\"values\":[\"single\",\"double\"]},"
	"{\"type\":\"enum\",\"property\":\"effect\",\"access\":2,\"values\":[\"blink\"]}]";

static string_vector ActionFields()
{
	return string_vector(1, "action");
}

struct CLamp
{
	CZigbeeDevice device;
	CWbDevice &wb;

	CLamp(const CModelTemplate *model = NULL)
		:device("lamp", Exposes(LAMP_EXPOSES), model, ActionFields()), wb(device.wbDevice())
	{
	}

	bool apply(const string &payload)
	{
		return device.applyState(ParseJson(payload), payload, NOW);
	}

	// The "set" payload for a command; "<rejected>" when the control cannot be written.
	string command(const string &control, const string &value)
	{
		string payload;
		return device.buildSetPayload(control, value, payload) ? payload : "<rejected>";
	}
};

TEST_SUITE("ZigbeeDevice") {

TEST_CASE("a new device has service controls, action buttons and set-only controls") {
	CLamp lamp;
	CHECK(lamp.device.name() == "lamp");
	CHECK(lamp.wb.id() == "lamp");
	CHECK(lamp.wb.title() == "lamp");

	CHECK(lamp.wb.controlCount() == 4);
	CHECK(lamp.wb.findControl("lastSeen")->type == CWBControl::Text);
	CHECK(lamp.wb.findControl("lastSeen")->readonly);
	CHECK(lamp.wb.findControl("single")->type == CWBControl::PushButton);
	CHECK(lamp.wb.findControl("double")->type == CWBControl::PushButton);
	CHECK(lamp.wb.findControl("single")->readonly);
	// "effect" can only be set, so it will never be reported
	CHECK(lamp.wb.findControl("effect")->type == CWBControl::Text);
	CHECK_FALSE(lamp.wb.findControl("effect")->readonly);
}

TEST_CASE("the Wiren Board id is a topic-safe form of the friendly name") {
	CZigbeeDevice device("kitchen/floor light", CExposeMap(), NULL, ActionFields());
	CHECK(device.name() == "kitchen/floor light");
	CHECK(device.wbDevice().id() == "kitchen_floor light");
	CHECK(device.wbDevice().title() == "kitchen/floor light");
}

TEST_CASE("enum values become buttons only for action fields") {
	CZigbeeDevice device("lamp", Exposes(LAMP_EXPOSES), NULL, string_vector(1, "mode"));
	CHECK(device.wbDevice().findControl("auto")->isPushButton());
	CHECK_FALSE(device.wbDevice().hasControl("single"));
}

TEST_CASE("a control is created when its value is reported for the first time") {
	CLamp lamp;
	CHECK(lamp.apply("{\"brightness\": 128}"));

	const CWbControl *brightness = lamp.wb.findControl("brightness");
	CHECK(brightness->type == CWBControl::Range);
	CHECK_FALSE(brightness->readonly);
	CHECK(brightness->hasMin);
	CHECK(brightness->min == 1);
	CHECK(brightness->max == 254);
	CHECK(lamp.wb.value("brightness") == "128");

	SUBCASE("the next report only updates the value") {
		CHECK_FALSE(lamp.apply("{\"brightness\": 5}"));
		CHECK(lamp.wb.value("brightness") == "5");
	}
}

TEST_CASE("measurements are generic values with units") {
	CLamp lamp;
	lamp.apply("{\"temperature\": 21.5, \"linkquality\": 90}");

	const CWbControl *temperature = lamp.wb.findControl("temperature");
	CHECK(temperature->type == CWBControl::Generic);
	CHECK(temperature->units == "deg C");
	CHECK(temperature->readonly);
	CHECK_FALSE(temperature->hasMax);
	CHECK(lamp.wb.value("temperature") == "21.5");

	const CWbControl *linkquality = lamp.wb.findControl("linkquality");
	CHECK(linkquality->type == CWBControl::Generic);
	CHECK(linkquality->units == "");
	CHECK(linkquality->max == 255);
}

TEST_CASE("a settable number without a limit is a writable value") {
	CLamp lamp;
	lamp.apply("{\"calibration\": -1.5}");
	CHECK(lamp.wb.findControl("calibration")->type == CWBControl::Generic);
	CHECK(lamp.wb.findControl("calibration")->units == "deg C");
	CHECK_FALSE(lamp.wb.findControl("calibration")->readonly);
}

TEST_CASE("values that are not exposed are ignored") {
	CLamp lamp;
	CHECK_FALSE(lamp.apply("{\"update_available\": false, \"color\": {\"x\": 1}}"));
	CHECK_FALSE(lamp.wb.hasControl("update_available"));
	CHECK_FALSE(lamp.wb.hasControl("color"));
}

TEST_CASE("structured values are ignored") {
	CLamp lamp;
	lamp.apply("{\"brightness\": 1}");
	lamp.apply("{\"brightness\": [1, 2]}");
	CHECK(lamp.wb.value("brightness") == "1");
}

TEST_CASE("on/off payloads are translated to a switch") {
	CLamp lamp;
	lamp.apply("{\"state\": \"ON\"}");
	CHECK(lamp.wb.findControl("state")->type == CWBControl::Switch);
	CHECK(lamp.wb.value("state") == "1");

	lamp.apply("{\"state\": \"OFF\"}");
	CHECK(lamp.wb.value("state") == "0");

	lamp.apply("{\"child_lock\": true}");
	CHECK(lamp.wb.value("child_lock") == "1");

	SUBCASE("unknown payloads pass through") {
		lamp.apply("{\"state\": \"TOGGLE\"}");
		CHECK(lamp.wb.value("state") == "TOGGLE");
	}
}

TEST_CASE("units are converted") {
	CLamp lamp;
	lamp.apply("{\"voltage\": 3025}");
	CHECK(lamp.wb.findControl("voltage")->type == CWBControl::Generic);
	CHECK(lamp.wb.findControl("voltage")->units == "V");
	CHECK(lamp.wb.value("voltage") == "3.025");

	lamp.apply("{\"voltage\": 2900}");
	CHECK(lamp.wb.value("voltage") == "2.900");
}

TEST_CASE("commands are sent as JSON with the types zigbee2mqtt uses") {
	CLamp lamp;
	lamp.apply("{\"state\":\"OFF\",\"brightness\":1,\"child_lock\":false,\"mode\":\"auto\",\"calibration\":0}");

	SUBCASE("a switch uses the announced payloads") {
		CHECK(lamp.command("state", "1") == "{\"state\":\"ON\"}");
		CHECK(lamp.command("state", "0") == "{\"state\":\"OFF\"}");
		CHECK(lamp.command("child_lock", "1") == "{\"child_lock\":true}");
		CHECK(lamp.command("child_lock", "0") == "{\"child_lock\":false}");
	}
	SUBCASE("numbers stay numbers") {
		CHECK(lamp.command("brightness", "200") == "{\"brightness\":200}");
		CHECK(lamp.command("calibration", "-1.5") == "{\"calibration\":-1.5}");
	}
	SUBCASE("text stays text") {
		CHECK(lamp.command("mode", "manual") == "{\"mode\":\"manual\"}");
		CHECK(lamp.command("effect", "blink") == "{\"effect\":\"blink\"}");
		CHECK(lamp.command("brightness", "max") == "{\"brightness\":\"max\"}");
		CHECK(lamp.command("state", "TOGGLE") == "{\"state\":\"TOGGLE\"}");
	}
}

TEST_CASE("commands for read-only and unknown controls are rejected") {
	CLamp lamp;
	lamp.apply("{\"voltage\": 3000}");
	CHECK(lamp.command("voltage", "1") == "<rejected>");
	CHECK(lamp.command("lastSeen", "1") == "<rejected>");
	CHECK(lamp.command("single", "1") == "<rejected>");
	CHECK(lamp.command("no_such_control", "1") == "<rejected>");
	CHECK(lamp.command("brightness", "1") == "<rejected>");   // not reported yet
}

TEST_CASE("an action presses the matching button") {
	CLamp lamp;
	lamp.apply("{\"action\": \"double\"}");
	CHECK(lamp.wb.value("action") == "double");
	CHECK(lamp.wb.value("double") == "1");
	CHECK(lamp.wb.value("single") == "");

	SUBCASE("an unknown action only updates the action control") {
		size_t controls = lamp.wb.controlCount();
		lamp.apply("{\"action\": \"triple\"}");
		CHECK(lamp.wb.value("action") == "triple");
		CHECK(lamp.wb.controlCount() == controls);
	}
	SUBCASE("an action named like an ordinary control does not touch it") {
		lamp.apply("{\"brightness\": 7}");
		lamp.apply("{\"action\": \"brightness\"}");
		CHECK(lamp.wb.value("brightness") == "7");
	}
}

TEST_CASE("lastSeen") {
	CLamp lamp;

	SUBCASE("is taken from the clock by default") {
		lamp.apply("{\"brightness\": 1}");
		CHECK(lamp.wb.value("lastSeen") == "1700000000000");
	}
	SUBCASE("is taken from last_seen when zigbee2mqtt reports it") {
		lamp.apply("{\"brightness\": 1, \"last_seen\": \"2026-11-15T10:00:00.000Z\"}");
		CHECK(lamp.wb.value("lastSeen") == "2026-11-15T10:00:00.000Z");
		lamp.apply("{\"last_seen\": 1763200000000}");
		CHECK(lamp.wb.value("lastSeen") == "1763200000000");
	}
	SUBCASE("is updated even by a payload that is not an object") {
		CHECK_FALSE(lamp.apply("[]"));
		CHECK(lamp.wb.value("lastSeen") == "1700000000000");
	}
}

TEST_CASE("a model template overrides type and maximum") {
	CModelTemplate model;
	model.controls["brightness"].type = CWBControl::Generic;
	model.controls["brightness"].max = 1000;
	model.controls["linkquality"].type = CWBControl::Text;

	CLamp lamp(&model);
	lamp.apply("{\"brightness\": 10, \"linkquality\": 50}");
	CHECK(lamp.wb.findControl("brightness")->type == CWBControl::Generic);
	CHECK(lamp.wb.findControl("brightness")->max == 1000);
	// no maximum in the rule: the one announced by the device is used
	CHECK(lamp.wb.findControl("linkquality")->type == CWBControl::Text);
	CHECK(lamp.wb.findControl("linkquality")->max == 255);
}

TEST_CASE("a model template value map is applied in both directions") {
	CModelTemplate model;
	model.controls["mode"].type = CWBControl::Switch;
	model.controls["mode"].valueMap.add("auto", "1");
	model.controls["mode"].valueMap.add("manual", "0");

	CLamp lamp(&model);
	lamp.apply("{\"mode\": \"auto\"}");
	CHECK(lamp.wb.findControl("mode")->type == CWBControl::Switch);
	CHECK(lamp.wb.value("mode") == "1");
	CHECK(lamp.command("mode", "0") == "{\"mode\":\"manual\"}");
}

TEST_CASE("a value map wins over a unit converter") {
	CModelTemplate model;
	model.controls["voltage"].type = CWBControl::Text;
	model.controls["voltage"].valueMap.add("0", "empty");

	CLamp lamp(&model);
	lamp.apply("{\"voltage\": 0}");
	CHECK(lamp.wb.value("voltage") == "empty");
	lamp.apply("{\"voltage\": 3000}");
	CHECK(lamp.wb.value("voltage") == "3.000");
}

TEST_CASE("announced on/off payloads win over the model template") {
	CModelTemplate model;
	model.controls["state"].type = CWBControl::Text;
	model.controls["state"].valueMap.add("ON", "enabled");

	CLamp lamp(&model);
	lamp.apply("{\"state\": \"ON\"}");
	CHECK(lamp.wb.findControl("state")->type == CWBControl::Switch);
	CHECK(lamp.wb.value("state") == "1");
}

TEST_CASE("json_template adds a control with the raw payload") {
	CModelTemplate model;
	model.jsonControl = true;

	CLamp lamp(&model);
	REQUIRE(lamp.wb.hasControl("raw"));
	lamp.apply("{\"brightness\": 7}");
	CHECK(lamp.wb.value("raw") == "{\"brightness\": 7}");
}

TEST_CASE("devices do not share rules") {
	CModelTemplate model;
	model.controls["state"].type = CWBControl::Text;

	CLamp first(&model);
	first.apply("{\"state\": \"ON\"}");
	CHECK(first.wb.value("state") == "1");
	CHECK(model.controls["state"].type == CWBControl::Text);
	CHECK(model.controls["state"].valueMap.empty());
}

}
