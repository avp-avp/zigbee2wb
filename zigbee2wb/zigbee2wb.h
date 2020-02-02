// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once


#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <strstream>
#include <chrono>
#include <ctime>

using namespace std;

#include "../libs/libutils/Exception.h"
#include "../libs/libutils/logging.h"
#include "../libs/libutils/strutils.h"
#include "../libs/libutils/thread.h"

#include "../haconfig.inc"

#ifdef HAVE_JSON_JSON_H
#	include "json/json.h"
#elif defined(HAVE_JSONCPP_JSON_JSON_H)
#	include "jsoncpp/json/json.h"
#else 
#	error josn.h not found
#endif

#include "../libs/libwb/WBDevice.h"
#include "../libs/libutils/Config.h"
