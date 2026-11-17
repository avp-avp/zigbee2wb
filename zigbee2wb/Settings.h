// Service settings loaded from zigbee2wb.json.
#pragma once
#include "zigbee2wb.h"
#include "../libs/libutils/ConfigItem.h"
#include "ValueConverters.h"

// Overrides how one zigbee2mqtt property is shown as a Wiren Board control.
struct CControlRule
{
	CWBControl::ControlType type;
	int max;               // <= 0: use the maximum announced by the device
	CValueMap valueMap;    // optional value dictionary

	CControlRule();
};

typedef map<string, CControlRule> CControlRules;

// Per-model overrides, section "models/<model_id>" of the config.
struct CModelTemplate
{
	bool jsonControl;      // publish the whole state payload to the "raw" control
	CControlRules controls;

	CModelTemplate();
};

typedef map<string, CModelTemplate> CModelTemplates;
typedef map<string, CValueMap> CValueMaps;

struct CSettings
{
	string mqttHost;
	string baseTopic;            // zigbee2mqtt base topic
	string_vector actionFields;  // properties whose values become push buttons
	CValueMaps converters;       // section "converters"
	CModelTemplates models;      // section "models"

	CSettings();

	// Throws CHaException on unknown control types or converters.
	static CSettings Load(const CConfigItem &config);

	bool isActionField(const string &name) const;
	// NULL when the model has no overrides.
	const CModelTemplate *findModel(const string &modelId) const;
};
