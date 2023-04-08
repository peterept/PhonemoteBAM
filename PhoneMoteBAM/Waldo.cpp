#include "pch.h"

#include "Waldo.h"

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

int split(char *s, char **pvals, size_t valslen, char sep, char **pstr, char eol) {
    int count = 0;
    char *e = s;
    char ch;
    if (s) {
        while (*s) {
            ch = *e;
            if (ch == sep || ch == eol || ch == '\0') {
                *pvals++ = s;
                count++;
                *e++ = '\0';
                s = e;
                if (ch == eol || ch == '\0') {
                    break;
                }
            } else {
                e++;
            }
        }
    }

    if (pstr) {
        *pstr = s;
    }
    return count;
}

Waldo::~Waldo() {
    stop();
}

void Waldo::start(long deviceId, u_short port) {
    if (running) {
        stop();
    }
    this->deviceId = deviceId;
    this->port = port;

    running = true;
    hThread = CreateThread(NULL, 0, &StartThreadStatic, this, 0, 0);
}

void Waldo::stop() {
    if (running) {
        running = false;
        WaitForSingleObject(hThread, 2000);
        hThread = NULL;
    }
}

void Waldo::setDeviceId(long deviceId) {
    lock.lock();
    if (this->deviceId != deviceId) {
        this->deviceId = deviceId;
    }
    lock.unlock();
}

long Waldo::getDeviceId() {
    lock.lock();
    long deviceId = this->deviceId;
    lock.unlock();
    return deviceId;
}

void Waldo::getState(WaldoState *outState) {
    lock.lock();
    memcpy(outState, &state, sizeof(state));
    lock.unlock();
}

bool Waldo::isReceiving() {
    const DWORD thresholdTicks = 1000; // 1 second old means not receiving

    lock.lock();
    bool isReceving = (GetTickCount() - state.local.time) < thresholdTicks;
    lock.unlock();

    return isReceving;
}

void Waldo::parseState(const char *name, char **grps, size_t grpc, WaldoState *outState) {

    const size_t MAX_VALS = 32;
    char *vals[MAX_VALS];
    int valc;

    // info
    if (strcmp(name, "!") == 0 && grpc >= 7) {
        outState->info.version = std::stoi(grps[0]);
        outState->info.deviceId = std::stoi(grps[2]);
        outState->info.sessionId = std::stoull(grps[3]);
        outState->info.seq = std::stoul(grps[4]);
        outState->info.time = std::stoull(grps[5]);
        // ping response
        if (strlen(grps[6]) > 0) {
            outState->ping.time = (DWORD)std::stoul(grps[6]);
            outState->ping.valid = true;
        }
        outState->info.valid = true;
    }
    // ar
    else if (strcmp(name, "ar") == 0 && grpc >= 1) {
        // transform
        valc = split(grps[0], vals, MAX_VALS);
        if (valc >= 16) {
            for (int i = 0; i < 16; i++) {
                outState->ar.transform[i] = std::stod(vals[i]);
            }
        }
        outState->ar.valid = true;
    }
    // face
    else if (strcmp(name, "f") == 0 && grpc >= 1) {
        valc = split(grps[0], vals, MAX_VALS);
        if (valc >= 2) {
            outState->face.isTracked = (std::stoi(vals[1]) == 1);
            if (outState->face.isTracked && grpc >= 4) {
                // face transform
                valc = split(grps[1], vals, MAX_VALS);
                if (valc >= 16) {
                    for (int i = 0; i < 16; i++) {
                        outState->face.transformFace[i] = std::stod(vals[i]);
                    }
                }
                // left eye transform
                valc = split(grps[2], vals, MAX_VALS);
                if (valc >= 16) {
                    for (int i = 0; i < 16; i++) {
                        outState->face.transformLeftEye[i] = std::stod(vals[i]);
                    }
                }
                // right eye transform
                valc = split(grps[3], vals, MAX_VALS);
                if (valc >= 16) {
                    for (int i = 0; i < 16; i++) {
                        outState->face.transformRightEye[i] = std::stod(vals[i]);
                    }
                }
            }
            outState->face.valid = true;
        }
    }
    // img
    else if (strcmp(name, "img") == 0 && grpc >= 5) {
        // physical width, height
        valc = split(grps[1], vals, MAX_VALS);
        if (valc < 2) {
            return;
        }
        outState->trackedImage.physicalWidth = std::stod(vals[0]);
        outState->trackedImage.physicalHeight = std::stod(vals[1]);
        // estimated scale
        outState->trackedImage.estimatedScale = std::stod(grps[2]);
        // x, y, z
        valc = split(grps[3], vals, MAX_VALS);
        if (valc < 3) {
            return;
        }
        outState->trackedImage.x = std::stod(vals[0]);
        outState->trackedImage.y = std::stod(vals[1]);
        outState->trackedImage.z = std::stod(vals[2]);
        // transform
        valc = split(grps[4], vals, MAX_VALS);
        if (valc < 16) {
            return;
        }
        for (int i = 0; i < 16; i++) {
            outState->trackedImage.transformMatrix[i] = std::stod(vals[i]);
        }
        outState->trackedImage.valid = true;
    }
}

bool Waldo::parseMessage(char *buffer, WaldoState *outState) {

    const size_t MAX_VALS = 32;
    char *grps[MAX_VALS];
    int grpc;
    char *s = buffer;
    bool checkHeader = true;
    // waldo packet start with !
    if (*s == '!') {
        while (grpc = split(s, grps, MAX_VALS, '|', &s)) {
            if (grpc >= 1) {
                parseState(grps[0], &grps[1], grpc - 1, outState);
                // first line is the header with deviceid, session, sequence which can
                // detect if we should keep this message or not (and if not abort early)
                if (checkHeader) {
                    if (shouldKeepMessage(outState) == false) {
                        return false;
                    }
                    checkHeader = false;
                }
            }
        }
    }
    return true;
}

bool Waldo::shouldKeepMessage(WaldoState *newState) {
    return newState->info.valid && newState->info.deviceId == getDeviceId() &&
           (newState->info.sessionId != state.info.sessionId || newState->info.seq > state.info.seq);
}

void Waldo::calculateStatistics(DWORD lastPingSentTime, long lastPingTime, long *pelapsedTimerMessageCount,
                                long *pElapsedTimeMessageLossCount, DWORD *pdwEndTime) {
    DWORD currTick = GetTickCount();

    // latency
    long latency = -1;
    DWORD timeSinceLastPing = currTick - lastPingSentTime;
    if (timeSinceLastPing < 5000) {
        latency = lastPingTime / 2;
    }

    double FPS = state.local.statistics.fps;
    double loss = state.local.statistics.loss;
    DWORD sampleTime = 2000;
    long delta = (long)*pdwEndTime - (long)currTick;
    if (delta <= 0) {
        long count = state.local.count - *pelapsedTimerMessageCount;
        long lossCount = state.local.lossCount - *pElapsedTimeMessageLossCount;
        *pelapsedTimerMessageCount = state.local.count;
        *pElapsedTimeMessageLossCount = state.local.lossCount;
        *pdwEndTime = currTick + delta + sampleTime;

        // FPS
        FPS = (double)count / ((double)sampleTime / 1000);

        // loss
        loss = (double)lossCount / (double)count;
    }

    // Update statistics
    lock.lock();
    state.local.statistics.latency = latency;
    state.local.statistics.fps = FPS;
    state.local.statistics.loss = loss;
    lock.unlock();
}

DWORD WINAPI Waldo::StartThreadStatic(LPVOID lpParam) {
    return static_cast<Waldo *>(lpParam)->StartThread();
}

DWORD Waldo::StartThread() {
    const int BUFLEN = 2048;
    char buf[BUFLEN];
    WSADATA wsa;
    fd_set fds;
    struct timeval tv;
    int n;
    SOCKET s;
    struct sockaddr_in server, si_other;
    int slen, recv_len;
    slen = sizeof(si_other);

    DWORD dwEndTime = 0;
    long elapsedTimerMessageCount = 0;
    long elapsedTimeMessageLossCount = 0;

    DWORD lastPingSentTime = 0;
    long lastPingTime = -1;

    // Initialise winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        running = false;
        return -1;
    }

start_socket:

    // Create a socket
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        running = false;
        return -1;
    }

    int reuse = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0) {
        running = false;
        return -1;
    }

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    // Bind
    if (bind(s, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        running = false;
        return -1;
    }

    dwEndTime = GetTickCount();

    // keep listening for data
    while (this->running) {
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout

        FD_ZERO(&fds);
        FD_SET(s, &fds);
        n = select(s + 1, &fds, NULL, NULL, &tv);
        if (n > 0) {
            recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_other, &slen);
            if (recv_len == SOCKET_ERROR) {
                // Ignore socket errors (eg: the packet is too large)
                continue;
            } else if (recv_len == 0) {
                // socket was closed
                closesocket(s);
                goto start_socket;
            }

            buf[recv_len] = 0;

            DWORD currTick = GetTickCount();
            WaldoState newState;

            // detect valid incoming messages
            if (parseMessage(buf, &newState)) {

                // update the local packet data & statistics
                newState.local.count = state.local.count + 1;
                newState.local.lossCount = state.local.lossCount + newState.info.seq - state.info.seq - 1;
                newState.local.time = GetTickCount();
                newState.local.statistics.latency =
                    state.local.statistics.latency; // use previous latency - we'll update this else where if we need to
                newState.local.statistics.fps = state.local.statistics.fps;   // use previous fps
                newState.local.statistics.loss = state.local.statistics.loss; // use previous loss
                memcpy(&newState.local.addr, &si_other.sin_addr, sizeof(newState.local.addr));

                // update received state
                lock.lock();
                memcpy(&this->state, &newState, sizeof(newState));
                lock.unlock();

                // Send pings periodically when actively receiving
                DWORD pingExpiry = lastPingSentTime + 1000;
                if (currTick >= pingExpiry) {
                    lastPingSentTime = sendPing(s, (struct sockaddr *)&si_other);
                }
            }

            // pings are always handled
            if (newState.ping.valid) {
                if (newState.ping.time == lastPingSentTime) {
                    // this is what we are waiting for!
                    lastPingTime = currTick - lastPingSentTime;
                }
            }
        }

        calculateStatistics(lastPingSentTime, lastPingTime, &elapsedTimerMessageCount, &elapsedTimeMessageLossCount,
                            &dwEndTime);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}

DWORD Waldo::sendPing(SOCKET s, struct sockaddr *paddr) {
    DWORD currTick = GetTickCount();
    char szPing[32];
    sprintf_s(szPing, "p|%ld", (long)currTick);
    sendto(s, szPing, strlen(szPing) + 1, 0, paddr, sizeof(struct sockaddr));
    return currTick;
}

void Waldo::getLocalIPAddress(char *str, size_t len) {
    WSADATA wsa;

    if (!str || len < 16) {
        return;
    }
    *str = '\0';

    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
        // get local IP
        char szHostName[255];
        gethostname(szHostName, 255);

        // const char* hostname = "localhost";
        struct addrinfo hints, *res;
        struct in_addr addr;

        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_INET;

        if (getaddrinfo(szHostName, NULL, &hints, &res) == 0) {
            addr.S_un = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.S_un;
            const char *result = inet_ntop(AF_INET, &addr, str, len);
        }

        WSACleanup();
    }
}