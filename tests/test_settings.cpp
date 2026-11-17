#include "TestHelpers.h"

TEST_SUITE("Settings") {

TEST_CASE("an empty config gives the defaults") {
	CSettings settings = Settings("{}");
	CHECK(settings.mqttHost == "wirenboard");
	CHECK(settings.baseTopic == "zigbee2mqtt");
	REQUIRE(settings.actionFields.size() == 1);
	CHECK(settings.actionFields[0] == "action");
	CHECK(settings.converters.empty());
	CHECK(settings.models.empty());
}

TEST_CASE("mqtt and general sections are read") {
	CSettings settings = Settings(
		"{\"general\":{\"action_field\":\"click\"},"
		"\"mqtt\":{\"host\":\"localhost\",\"base_topic\":\"z2m\"}}");
	CHECK(settings.mqttHost == "localhost");
	CHECK(settings.baseTopic == "z2m");
	CHECK(settings.isActionField("click"));
	CHECK_FALSE(settings.isActionField("action"));
}

TEST_CASE("model templates are read") {
	CSettings settings = Settings(
		"{\"models\":{"
		"\"lumi.sensor_cube.aqgl01\":{\"json_template\":true},"
		"\"TS110F\":{\"controls\":{\"brightness\":{\"type\":\"range\",\"max\":255}}}}}");

	const CModelTemplate *cube = settings.findModel("lumi.sensor_cube.aqgl01");
	REQUIRE((cube != NULL));
	CHECK(cube->jsonControl);
	CHECK(cube->controls.empty());

	const CModelTemplate *dimmer = settings.findModel("TS110F");
	REQUIRE((dimmer != NULL));
	CHECK_FALSE(dimmer->jsonControl);
	REQUIRE(dimmer->controls.count("brightness") == 1);
	const CControlRule &brightness = dimmer->controls.find("brightness")->second;
	CHECK(brightness.type == CWBControl::Range);
	CHECK(brightness.max == 255);
	CHECK(brightness.valueMap.empty());

	CHECK((settings.findModel("unknown") == NULL));
}

TEST_CASE("max is optional in a control rule") {
	CSettings settings = Settings("{\"models\":{\"M\":{\"controls\":{\"mode\":{\"type\":\"text\"}}}}}");
	CHECK(settings.findModel("M")->controls.find("mode")->second.max == -1);
}

TEST_CASE("a control rule can refer to a converter") {
	CSettings settings = Settings(
		"{\"converters\":{\"cover\":{\"OPEN\":\"1\",\"CLOSE\":\"0\"}},"
		"\"models\":{\"M\":{\"controls\":{\"state\":{\"type\":\"switch\",\"converter\":\"cover\"}}}}}");

	REQUIRE(settings.converters.count("cover") == 1);
	const CControlRule &state = settings.findModel("M")->controls.find("state")->second;
	string value;
	CHECK(state.valueMap.toWb("OPEN", value));
	CHECK(value == "1");
	CHECK(state.valueMap.toZigbee("0", value));
	CHECK(value == "CLOSE");
}

TEST_CASE("invalid control rules are rejected") {
	SUBCASE("unknown control type") {
		CHECK_THROWS_AS(Settings("{\"models\":{\"M\":{\"controls\":{\"x\":{\"type\":\"slider\"}}}}}"), CHaException);
	}
	SUBCASE("missing control type") {
		CHECK_THROWS_AS(Settings("{\"models\":{\"M\":{\"controls\":{\"x\":{\"max\":5}}}}}"), CHaException);
	}
	SUBCASE("unknown converter") {
		CHECK_THROWS_AS(
			Settings("{\"models\":{\"M\":{\"controls\":{\"x\":{\"type\":\"switch\",\"converter\":\"nope\"}}}}}"),
			CHaException);
	}
	SUBCASE("converter value is not a string") {
		CHECK_THROWS_AS(Settings("{\"converters\":{\"c\":{\"ON\":1}}}"), CHaException);
	}
}

}
