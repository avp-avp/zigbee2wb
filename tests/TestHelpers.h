// Shared fixtures for the unit tests.
#pragma once

#include <fstream>
#include <sstream>

#include "doctest.h"
#include "../zigbee2wb/Bridge.h"
#include "../zigbee2wb/JsonUtils.h"

struct CPublished
{
	string topic, payload;
	bool retain;
};

// Records everything the bridge sends instead of talking to a broker.
class CFakeTransport : public IMqttTransport
{
public:
	vector<CPublished> published;
	string_vector subscriptions, unsubscriptions;

	virtual void publish(const string &topic, const string &payload, bool retain)
	{
		CPublished message = { topic, payload, retain };
		published.push_back(message);
	}

	virtual void subscribe(const string &topic)
	{
		subscriptions.push_back(topic);
	}

	virtual void unsubscribe(const string &topic)
	{
		unsubscriptions.push_back(topic);
	}

	bool isUnsubscribed(const string &topic) const
	{
		for (size_t i = 0; i < unsubscriptions.size(); i++) {
			if (unsubscriptions[i] == topic)
				return true;
		}
		return false;
	}

	void clear()
	{
		published.clear();
		subscriptions.clear();
		unsubscriptions.clear();
	}

	bool wasPublished(const string &topic) const
	{
		return find(topic) != NULL;
	}

	// Payload of the latest message in the topic; fails the test when there is none.
	string payload(const string &topic) const
	{
		const CPublished *message = find(topic);
		if (!message) {
			FAIL_CHECK("nothing was published to " << topic);
			return "";
		}
		return message->payload;
	}

	const CPublished *find(const string &topic) const
	{
		for (size_t i = published.size(); i > 0; i--) {
			if (published[i - 1].topic == topic)
				return &published[i - 1];
		}
		return NULL;
	}

	bool isSubscribed(const string &topic) const
	{
		return count(topic) > 0;
	}

	size_t count(const string &subscription) const
	{
		size_t result = 0;
		for (size_t i = 0; i < subscriptions.size(); i++) {
			if (subscriptions[i] == subscription)
				result++;
		}
		return result;
	}
};

// A logger that prints nothing.
inline CLog *SilentLog()
{
	return CLog::GetLog("nul");
}

inline CConfigItem Config(const string &json)
{
	return CConfigItem(ParseJson(json));
}

inline CSettings Settings(const string &json = "{}")
{
	return CSettings::Load(Config(json));
}

inline string ReadDataFile(const string &name)
{
	string path = string(TEST_DATA_DIR) + "/" + name;
	ifstream file(path.c_str());
	REQUIRE_MESSAGE(file.good(), "cannot open " << path);
	stringstream content;
	content << file.rdbuf();
	return content.str();
}
