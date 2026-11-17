#include "TestHelpers.h"

static long long FixedClock()
{
	return 1700000000000LL;
}

static const char *SOCKET =
	"[{\"type\":\"Router\",\"friendly_name\":\"socket\",\"model_id\":\"X\",\"interview_state\":\"SUCCESSFUL\","
	"\"definition\":{\"exposes\":[{\"type\":\"binary\",\"property\":\"state\",\"access\":7,"
	"\"value_on\":\"ON\",\"value_off\":\"OFF\"}]}}]";

// A connected bridge with one device. The broker is played by the test:
// what it would send back is passed to onMessage() by hand.
struct CRefreshFixture
{
	CFakeTransport mqtt;
	CBridge bridge;

	CRefreshFixture()
		:bridge(Settings("{}"), &mqtt, SilentLog())
	{
		bridge.setClock(FixedClock);
		bridge.onConnect();
		bridge.onMessage("zigbee2mqtt/bridge/devices", SOCKET, true);
		bridge.onMessage("zigbee2mqtt/socket", "{\"state\":\"ON\"}");
		// the broker echoes the driver topics of the devices just published
		ownedBy("zigbee", "zigbee2wb");
		ownedBy("socket", "zigbee2wb");
		mqtt.clear();
	}

	void ownedBy(const string &device, const string &driver, bool retained = false)
	{
		bridge.onMessage("/devices/" + device + "/meta/driver", driver, retained);
	}

	// The driver as published in the JSON metadata of the device.
	void metaOf(const string &device, const string &driver, bool retained = true)
	{
		bridge.onMessage("/devices/" + device + "/meta", "{\"driver\":\"" + driver + "\",\"title\":{\"en\":\"x\"}}", retained);
	}

	void pressRefresh()
	{
		bridge.onMessage("/devices/zigbee/controls/refresh/on", "1");
	}

	// The broker returns the marker published by the bridge.
	void deliverMarker()
	{
		bridge.onMessage(CBridge::SYNC_TOPIC, mqtt.payload(CBridge::SYNC_TOPIC));
	}

	bool wasRemoved(const string &topic) const
	{
		const CPublished *message = mqtt.find(topic);
		return message && message->payload.empty() && message->retain;
	}
};

TEST_SUITE("Refresh") {

TEST_CASE_FIXTURE(CRefreshFixture, "the refresh button starts the cleanup of own devices") {
	pressRefresh();

	CHECK(bridge.isRefreshing());
	CHECK(bridge.deviceCount() == 0);
	CHECK(mqtt.isSubscribed("/devices/socket/#"));
	CHECK(mqtt.isSubscribed("/devices/zigbee/#"));
	CHECK(mqtt.isUnsubscribed("/devices/socket/controls/+/on"));

	const CPublished *marker = mqtt.find(CBridge::SYNC_TOPIC);
	REQUIRE((marker != NULL));
	CHECK_FALSE(marker->payload.empty());
	CHECK_FALSE(marker->retain);
}

TEST_CASE_FIXTURE(CRefreshFixture, "stored topics of own devices are removed") {
	pressRefresh();
	mqtt.clear();

	bridge.onMessage("/devices/socket/meta", "{\"driver\":\"zigbee2wb\"}", true);
	bridge.onMessage("/devices/socket/controls/state", "1", true);
	bridge.onMessage("/devices/socket/controls/state/meta/max", "100", true);
	bridge.onMessage("/devices/zigbee/controls/log", "old error", true);

	CHECK(wasRemoved("/devices/socket/meta"));
	CHECK(wasRemoved("/devices/socket/controls/state"));
	CHECK(wasRemoved("/devices/socket/controls/state/meta/max"));
	CHECK(wasRemoved("/devices/zigbee/controls/log"));
	CHECK(mqtt.published.size() == 4);
}

TEST_CASE_FIXTURE(CRefreshFixture, "only stored messages of the devices being cleaned are removed") {
	pressRefresh();
	mqtt.clear();

	SUBCASE("devices of other drivers are left alone") {
		bridge.onMessage("/devices/wb-gpio/controls/A1_OUT", "1", true);
		bridge.onMessage("/devices/wb-gpio/meta/driver", "wb-gpio", true);
	}
	SUBCASE("live messages are not stored ones") {
		bridge.onMessage("/devices/socket/controls/state", "1", false);
	}
	SUBCASE("the echo of a removal is ignored") {
		bridge.onMessage("/devices/socket/controls/state", "", true);
	}
	SUBCASE("zigbee2mqtt topics are not touched") {
		bridge.onMessage("zigbee2mqtt/bridge/info", "{\"log_level\":\"info\"}", true);
	}
	CHECK(mqtt.published.empty());
}

TEST_CASE_FIXTURE(CRefreshFixture, "devices left by previous runs are removed too") {
	// seen on the broker at startup, but zigbee2mqtt no longer knows the device
	ownedBy("removed_long_ago", "zigbee2wb", true);
	ownedBy("wb-gpio", "wb-gpio", true);

	pressRefresh();
	CHECK(mqtt.isSubscribed("/devices/removed_long_ago/#"));
	CHECK_FALSE(mqtt.isSubscribed("/devices/wb-gpio/#"));

	bridge.onMessage("/devices/removed_long_ago/controls/temperature/meta/type", "temperature", true);
	CHECK(wasRemoved("/devices/removed_long_ago/controls/temperature/meta/type"));
}

TEST_CASE_FIXTURE(CRefreshFixture, "own devices are also recognized by the JSON metadata") {
	metaOf("json_only", "zigbee2wb");
	metaOf("wb-adc", "wb-adc");

	pressRefresh();
	CHECK(mqtt.isSubscribed("/devices/json_only/#"));
	CHECK_FALSE(mqtt.isSubscribed("/devices/wb-adc/#"));
}

TEST_CASE_FIXTURE(CRefreshFixture, "the JSON metadata wins over the legacy driver topic") {
	// another driver took the device over but the stale subtopic is still there
	ownedBy("taken_over", "zigbee2wb", true);
	metaOf("taken_over", "wb-rules");
	// and the other way round
	ownedBy("ours", "something-old", true);
	metaOf("ours", "zigbee2wb");

	pressRefresh();
	CHECK_FALSE(mqtt.isSubscribed("/devices/taken_over/#"));
	CHECK(mqtt.isSubscribed("/devices/ours/#"));
}

TEST_CASE_FIXTURE(CRefreshFixture, "metadata without a driver falls back to the legacy topic") {
	ownedBy("legacy", "zigbee2wb", true);
	ownedBy("foreign", "wb-gpio", true);

	SUBCASE("no driver field") {
		bridge.onMessage("/devices/legacy/meta", "{\"title\":{\"en\":\"x\"}}", true);
		bridge.onMessage("/devices/foreign/meta", "{\"title\":{\"en\":\"x\"}}", true);
	}
	SUBCASE("not JSON") {
		bridge.onMessage("/devices/legacy/meta", "garbage{", true);
		bridge.onMessage("/devices/foreign/meta", "[1, 2]", true);
	}
	SUBCASE("removed") {
		metaOf("legacy", "wb-rules");
		bridge.onMessage("/devices/legacy/meta", "", false);
	}

	pressRefresh();
	CHECK(mqtt.isSubscribed("/devices/legacy/#"));
	CHECK_FALSE(mqtt.isSubscribed("/devices/foreign/#"));
}

TEST_CASE_FIXTURE(CRefreshFixture, "a device taken over by another driver is not removed") {
	ownedBy("socket2", "zigbee2wb", true);
	ownedBy("socket2", "other-driver");

	pressRefresh();
	CHECK_FALSE(mqtt.isSubscribed("/devices/socket2/#"));
}

TEST_CASE_FIXTURE(CRefreshFixture, "nothing is created until the cleanup is over") {
	pressRefresh();
	mqtt.clear();

	bridge.onMessage("zigbee2mqtt/bridge/devices", SOCKET, true);
	bridge.onMessage("zigbee2mqtt/socket", "{\"state\":\"ON\"}");
	bridge.onMessage("zigbee2mqtt/bridge/state", "{\"state\":\"online\"}", true);
	bridge.onMessage("/devices/socket/controls/state/on", "0");

	CHECK(bridge.deviceCount() == 0);
	CHECK(mqtt.published.empty());
}

TEST_CASE_FIXTURE(CRefreshFixture, "after the marker the devices are created anew") {
	pressRefresh();
	deliverMarker();

	CHECK_FALSE(bridge.isRefreshing());
	CHECK(mqtt.isUnsubscribed("/devices/socket/#"));
	CHECK(mqtt.isUnsubscribed("/devices/zigbee/#"));
	// the bridge device is back, with the values it had
	CHECK(mqtt.payload("/devices/zigbee/meta/driver") == "zigbee2wb");
	CHECK(mqtt.payload("/devices/zigbee/controls/refresh/meta/type") == "pushbutton");
	// subscribing again makes the broker resend the retained bridge/devices
	CHECK(mqtt.count("zigbee2mqtt/#") == 1);

	mqtt.clear();
	bridge.onMessage("zigbee2mqtt/bridge/devices", SOCKET, true);
	CHECK(bridge.deviceCount() == 1);
	CHECK(mqtt.payload("/devices/socket/meta/name") == "socket");
	CHECK(mqtt.isSubscribed("/devices/socket/controls/+/on"));

	SUBCASE("stored messages are no longer removed") {
		mqtt.clear();
		bridge.onMessage("/devices/socket/controls/state", "1", true);
		CHECK(mqtt.published.empty());
	}
	SUBCASE("the device works as before") {
		bridge.onMessage("zigbee2mqtt/socket", "{\"state\":\"OFF\"}");
		CHECK(mqtt.payload("/devices/socket/controls/state") == "0");
	}
}

TEST_CASE_FIXTURE(CRefreshFixture, "only the marker of the current refresh ends the cleanup") {
	pressRefresh();
	bridge.onMessage(CBridge::SYNC_TOPIC, "someone else");
	CHECK(bridge.isRefreshing());

	string first = mqtt.payload(CBridge::SYNC_TOPIC);
	deliverMarker();
	CHECK_FALSE(bridge.isRefreshing());

	mqtt.clear();
	pressRefresh();
	CHECK(mqtt.payload(CBridge::SYNC_TOPIC) != first);
	bridge.onMessage(CBridge::SYNC_TOPIC, first);
	CHECK(bridge.isRefreshing());
}

TEST_CASE_FIXTURE(CRefreshFixture, "a second press during a refresh changes nothing") {
	pressRefresh();
	mqtt.clear();
	pressRefresh();
	bridge.refresh();
	CHECK(mqtt.published.empty());
	CHECK(mqtt.subscriptions.empty());
}

TEST_CASE_FIXTURE(CRefreshFixture, "a refresh interrupted by a reconnect starts over") {
	pressRefresh();
	string first = mqtt.payload(CBridge::SYNC_TOPIC);
	mqtt.clear();

	bridge.onConnect();
	CHECK(bridge.isRefreshing());
	CHECK(mqtt.isSubscribed("/devices/socket/#"));
	CHECK(mqtt.isSubscribed(CBridge::SYNC_TOPIC));
	CHECK_FALSE(mqtt.isSubscribed("zigbee2mqtt/#"));
	CHECK(mqtt.payload(CBridge::SYNC_TOPIC) != first);

	deliverMarker();
	CHECK_FALSE(bridge.isRefreshing());
	CHECK(mqtt.isSubscribed("zigbee2mqtt/#"));
}

TEST_CASE_FIXTURE(CRefreshFixture, "other commands to the zigbee device are ignored") {
	bridge.onMessage("/devices/zigbee/controls/nonsense/on", "1");
	CHECK_FALSE(bridge.isRefreshing());
	CHECK(mqtt.published.empty());
}

}
