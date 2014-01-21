#pragma once

#include <Windows.h>
#include <stdio.h>
#include <winsock.h>
#include <assert.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

/***** Constants & type definitions *****/
#define VAR_STRING(a) #a

typedef unsigned char		uchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;
typedef unsigned __int64	uint64;
typedef void				*voidptr_t;
/****************************************/

// Included/used libraries
#include "../libraries/stdafx.h"

// Misc
#include "misc/stdafx.h"

// MCommon2
#include "mcommon2/stdafx.h"

// MNetwork
#include "mnetwork/stdafx.h"

// MMassgate
#include "mmassgate/stdafx.h"

// Server modules
#include "Server Modules/stdafx.h"