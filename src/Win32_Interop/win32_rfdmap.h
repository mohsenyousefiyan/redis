/*
 * Copyright (c), Microsoft Open Technologies, Inc.
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <errno.h>
#define INCL_WINSOCK_API_PROTOTYPES 0 // Important! Do not include Winsock API definitions to avoid conflicts with API entry points defnied below.
#include <WinSock2.h>
#include "ws2tcpip.h"
#include <map>
#include <queue>

using namespace std;

typedef int RFD;   // Redis File Descriptor

typedef struct {
    SOCKET socket;
    void*  state;
    int    flags;
} SocketInfo;

/* In UNIX File Descriptors increment by one for each new one. Windows handles 
 * do not follow the same rule.  Additionally UNIX uses a 32-bit int to 
 * represent a FD while Windows_x64 uses a 64-bit value to represent a handle.
 * There is no documented guarantee that a Windows SOCKET value will be 
 * entirely constrained in 32-bits (though it seems to be currently). SOCKETs
 * should be treated as an opaque value and not be cast to a 32-bit int. In 
 * order to not break existing code that relies on the maximum FD value to 
 * indicate the number of handles that have been created (and other UNIXisms),
 * this code maps SOCKET handles to a virtual FD number starting at 3 (0, 1 and
 * 2 are reserved for stdin, stdout and stderr).
 */
class RFDMap {
public:
    static RFDMap& getInstance();

private:
    RFDMap();
    RFDMap(RFDMap const&);          // Don't implement to guarantee singleton semantics
    void operator=(RFDMap const&);  // Don't implement to guarantee singleton semantics

private:
    map<SOCKET, RFD>     SocketToRFDMap;
    map<int, RFD>        PosixFDToRFDMap;
    map<RFD, SocketInfo> RFDToSocketInfoMap;
    map<RFD, int>        RFDToPosixFDMap;
    queue<RFD>           RFDRecyclePool;

private:
    CRITICAL_SECTION mutex;

public:
    const static int INVALID_RFD = -1;
    const static int INVALID_FD  = -1;

private:
    /*
      Gets the next available Redis File Descriptor. RFDs are always
      non-negative integers, with the first three being reserved for
      stdin(0), stdout(1) and stderr(2).
    */
    RFD getNextRFDAvailable();

    const static int FIRST_RESERVED_RFD_INDEX = 0;
    const static int LAST_RESERVED_RFD_INDEX  = 2;
    int next_available_rfd = LAST_RESERVED_RFD_INDEX + 1;

public:
    /*
      Adds a socket to SocketToRFDMap and to RFDToSocketInfoMap.
      Returns the RFD value for the socket.
      Returns INVALID_RFD if the socket is already added to the collection.
    */
    RFD addSocket(SOCKET s);

    /* Removes a socket from SocketToRFDMap. */
    void removeSocketToRFD(SOCKET s);

    /*
      Removes a RFD from RFDToSocketInfoMap.
      It frees the associated RFD adding it to RFDRecyclePool.
    */
    void removeRFDToSocketInfo(RFD rfd);

    /*
      Adds a posixFD (used with low-level CRT posix file functions) to RFDToPosixFDMap.
      Returns the RFD value for the posixFD.
      Returns the existing RFD if the posixFD is already present in the collection.
    */
    RFD addPosixFD(int posixFD);

    /*
      Removes a socket from RFDToPosixFDMap.
      It frees the associated RFD adding it to RFDRecyclePool.
    */
    void removePosixFD(int posixFD);

    /*
      Returns the socket associated with a RFD.
      Returns INVALID_SOCKET if the socket is not found.
    */
    SOCKET lookupSocket(RFD rfd);

    /*
      Returns a pointer to the socket info structure associated with a RFD.
      Return NULL if the info socket info structure is not found.
    */
    SocketInfo* lookupSocketInfo(RFD rfd);

    /*
      Returns the posixFD associated with a RFD.
      Returns INVALID_FD if the posixFD is not found.
    */
    int lookupPosixFD(RFD rfd);
};
