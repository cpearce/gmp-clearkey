/*
 * Copyright 2013, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <stdio.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include <initguid.h>

#include <queue>
#include <memory>
#include <vector>
#include <assert.h>
#include <sstream>
#include <string>
#include <map>
#include <ctime>

#include <strsafe.h>

#include "gmp-entrypoints.h"
#include "gmp-video-host.h"
#include "gmp-video-decode.h"
#include "gmp-video-frame-i420.h"
#include "gmp-video-frame-encoded.h"
#include "gmp-task-utils.h"
#include "gmp-audio-host.h"
#include "gmp-audio-decode.h"
#include "gmp-audio-samples.h"
#include "gmp-decryption.h"
#include "gmp-async-shutdown.h"


#ifdef CLEARKEYGMP_EXPORTS
#define GMP_EXPORT __declspec(dllexport)
#else
#define GMP_EXPORT __declspec(dllimport)
#endif
