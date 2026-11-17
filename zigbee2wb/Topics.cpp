#include "Topics.h"

static bool StartsWith(const string &value, const string &prefix)
{
	return value.compare(0, prefix.size(), prefix) == 0;
}

static CTopic::Kind BridgeTopicKind(const string &name)
{
	if (name == "state")   return CTopic::BridgeState;
	if (name == "logging") return CTopic::BridgeLogging;
	if (name == "info")    return CTopic::BridgeInfo;
	if (name == "devices") return CTopic::BridgeDevices;
	if (name == "groups")  return CTopic::BridgeGroups;
	return CTopic::Unknown;
}

static CTopic ParseZigbeeTopic(const string &path)
{
	CTopic result;
	string_vector parts;
	SplitString(path, '/', parts);

	if (parts.empty())
		return result;

	if (parts[0] == "bridge") {
		if (parts.size() == 2)
			result.kind = BridgeTopicKind(parts[1]);
	}
	else {
		// Whether it is really a device (and not e.g. <device>/availability)
		// is known only to the owner of the device list.
		result.kind = CTopic::DeviceState;
		result.device = path;
	}

	return result;
}

static CTopic ParseWbTopic(const string &path)
{
	CTopic result;
	string_vector parts;
	SplitString(path, '/', parts);

	if (parts.size() == 4 && parts[1] == "controls" && parts[3] == "on") {
		result.kind = CTopic::ControlCommand;
		result.device = parts[0];
		result.control = parts[2];
	}
	else if (parts.size() == 3 && parts[1] == "meta" && parts[2] == "driver") {
		result.kind = CTopic::DeviceDriver;
		result.device = parts[0];
	}
	else if (parts.size() == 2 && parts[1] == "meta") {
		result.kind = CTopic::DeviceMeta;
		result.device = parts[0];
	}

	return result;
}

static const string wbPrefix = "/devices/";

CTopic ParseTopic(const string &baseTopic, const string &topic)
{
	const string zigbeePrefix = baseTopic + "/";

	if (StartsWith(topic, zigbeePrefix))
		return ParseZigbeeTopic(topic.substr(zigbeePrefix.size()));
	if (StartsWith(topic, wbPrefix))
		return ParseWbTopic(topic.substr(wbPrefix.size()));

	return CTopic();
}

string ZigbeeFilter(const string &baseTopic)
{
	return baseTopic + "/#";
}

string ControlCommandFilter(const string &device)
{
	return "/devices/" + device + "/controls/+/on";
}

string ZigbeeSetTopic(const string &baseTopic, const string &device)
{
	return baseTopic + "/" + device + "/set";
}

string DeviceDriverFilter()
{
	return wbPrefix + "+/meta/driver";
}

string DeviceMetaFilter()
{
	return wbPrefix + "+/meta";
}

string DeviceFilter(const string &device)
{
	return wbPrefix + device + "/#";
}

string WbDeviceOf(const string &topic)
{
	if (!StartsWith(topic, wbPrefix))
		return "";

	size_t end = topic.find('/', wbPrefix.size());
	if (end == string::npos)
		return "";

	return topic.substr(wbPrefix.size(), end - wbPrefix.size());
}
