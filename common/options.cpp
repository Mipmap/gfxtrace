/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"
#include "options.h"

Options* gOptions = NULL;

#ifdef _UNICODE
    typedef std::wstring tstring;
#else
    typedef std::string tstring;
#endif

// ------------------------------------------------------------------------------------------------
tstring dirname(const tstring& _path)
{
    // These functions actually work on const strings, but the C decl version exposed by the macro 
    // takes non-const TCHAR*.
    TCHAR* pathSepBack = _tcsrchr(const_cast<TCHAR*>(_path.c_str()), TC('\\'));
    TCHAR* pathSepFor = _tcsrchr(const_cast<TCHAR*>(_path.c_str()), TC('/'));
    TCHAR* lastPathSep = pathSepBack > pathSepFor ? pathSepBack : pathSepFor;

    if (!lastPathSep) {
        return tstring(TC(".\\"));
    }

    return tstring(_path.c_str(), lastPathSep);
}

// ------------------------------------------------------------------------------------------------
tstring join(const tstring& _pathA, const tstring& _pathB)
{
    return _pathA + TC("/") + _pathB;
}

// ------------------------------------------------------------------------------------------------
tstring abspath(const tstring& _path)
{
#if _WIN32
    TCHAR tmpPath[_MAX_PATH + 1];
    GetFullPathName(_path.c_str(), _MAX_PATH, tmpPath, NULL);
    tstring retVal = tmpPath;
    return retVal;
#else
    static_assert(0, "abspath needs to be implemented for unknown platform.");
#endif
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void PrintHelp()
{

}

// ------------------------------------------------------------------------------------------------
int ParseInto(int _curArgNum, int _argsNeeded, int _argCount, TCHAR* _argv[], TCHAR** _dest)
{
    if (_curArgNum + _argsNeeded >= _argCount) {
        LogError(TC("Not enough parameters for argument \"%s\""), _argv[_curArgNum]);
        return 0;
    }

    assert(_argsNeeded == 1);
    SafeFree(*_dest);
    (*_dest) = AllocateAndCopy(_argv[_curArgNum + _argsNeeded]);
    return 2;
}

// ------------------------------------------------------------------------------------------------
int ParseRemainingArgsInto(int _curArgNum, int _argCount, TCHAR* _argv[], TCHAR* _exeName, TCHAR** _dest)
{
    SafeFree(*_dest);

    size_t allocSize = _tcslen(_exeName);
    for (int i = _curArgNum; i < _argCount; ++i) {
        allocSize += _tcslen(_argv[i]);
    }

    // Add room for spaces between, and the null terminator at the end
    allocSize += _argCount - _curArgNum + 1;

    (*_dest) = (TCHAR*)malloc(allocSize * sizeof(TCHAR));

    int dstChar = 0;
    while (((*_dest)[dstChar++] = (*_exeName++)) != TC('\0')) { }
    if (_curArgNum < _argCount) {
        (*_dest)[dstChar - 1] = TC(' ');
    }

    for (int i = _curArgNum; i < _argCount; ++i) {
        int srcChar = 0;
        
        while (((*_dest)[dstChar++] = (_argv[i][srcChar++])) != TC('\0')) { }
        if (i + 1 < _argCount) {
            (*_dest)[dstChar - 1] = TC(' ');
        }
    }

    return _argCount - _curArgNum;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
Options::Options()
{
	memset(this, 0, sizeof(*this));

	// Set defaults.
	OutputTraceName = _tcsdup(TC("trace.gft"));
	
	ServerPort = 65536 - 31337;

	CaptureAllTextures = true;
	FixBadFlushBufferRangeArgs = true;
}

// ------------------------------------------------------------------------------------------------
Options::~Options()
{
	SafeDeleteArray(OutputTraceName);
	SafeDeleteArray(ExeName);
	SafeDeleteArray(WorkingDirectory);
	SafeDeleteArray(ProcessArgs);
    SafeDeleteArray(InceptionDllPath);
}

// ------------------------------------------------------------------------------------------------
Options* ParseCommandLine(int argc, TCHAR *argv[])
{
    if (argc <= 1) {
        PrintHelp();
        exit(1);
    }

	Options* retVal = new Options;

    // The dll to be injected (inception.dll) is located right next to the executable, which is 
    // argv[0]. Grab the directory portion of that.
    retVal->InceptionDllPath = AllocateAndCopy(abspath(join(dirname(argv[0]), TC("inception.dll"))).c_str());

    int eztraceArgsEnd = 0;
    for (int i = 1; i < argc; ) {
        int consumed = 0;
        
        TCHAR* curArg = argv[i];

        if (_tcscmp(TC("-p"), curArg) == 0) {
            consumed += ParseInto(i, 1, argc, argv, &(retVal->ExeName));
        } else if (_tcscmp(TC("-w"), curArg) == 0) {
            consumed += ParseInto(i, 1, argc, argv, &(retVal->WorkingDirectory));
        } else if (_tcscmp(TC("-o"), curArg) == 0) {
            consumed += ParseInto(i, 1, argc, argv, &(retVal->OutputTraceName));
        } else if (_tcscmp(TC("-h"), curArg) == 0) {
            PrintHelp();
            exit(0);
        } else {
            if (_tcscmp(TC("--"), curArg) == 0) {
                eztraceArgsEnd = i + 1;
            } else {
                eztraceArgsEnd = i;
            }
            break;
        }

        if (consumed == 0) {
            PrintHelp();
            exit(2);
        }
        i += consumed;
    }

    ParseRemainingArgsInto(eztraceArgsEnd, argc, argv, retVal->ExeName, &(retVal->ProcessArgs));

    bool validArgs = true;
    if (retVal->ExeName == NULL) {
        LogError(TC("Missing required parameter -p"));
        validArgs = false;
    } else {
        if (retVal->WorkingDirectory == NULL) {
            LogInfo(TC("No working directory specified, assuming executable's path."));
            retVal->WorkingDirectory = AllocateAndCopy(dirname(retVal->ExeName).c_str());
        }
    }
    LogInfo(TC("Args to be passed to child process: '%s'"), retVal->ProcessArgs);

    if (validArgs == false) {
        PrintHelp();
        exit(3);
    }

	return retVal;
}

