// Common includes for all zigbee2wb modules.
#pragma once

#include <unistd.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

using namespace std;

#include "../haconfig.inc"

#include "../libs/libutils/Exception.h"
#include "../libs/libutils/logging.h"
#include "../libs/libutils/strutils.h"

#ifdef HAVE_JSON_JSON_H
#	include "json/json.h"
#elif defined(HAVE_JSONCPP_JSON_JSON_H)
#	include "jsoncpp/json/json.h"
#else
#	error json.h not found
#endif

#include "../libs/libwb/WBDevice.h"
