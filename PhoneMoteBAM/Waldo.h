#pragma once

#include <mutex>
#include <vector>
#include <winsock2.h>

int split(char *s, char **pvals, size_t valslen, char sep = ',', char **pstr = NULL, char eol = '\n');

struct WaldoState {
    struct {
        DWORD time;
        long count;
        long lossCount;
        IN_ADDR addr;
        struct {
            double fps;
            long latency;
            double loss;
        } statistics;
    } local;
    struct {
        bool valid;
        long version;
        long deviceId;
        unsigned long long sessionId;
        DWORD seq;
        unsigned long long time;
    } info;
    struct {
        bool valid;
        DWORD time;
    } ping;
    struct {
        bool valid;
        double x() {
            return transform[12];
        };
        double y() {
            return transform[13];
        };
        double z() {
            return transform[14];
        };
        double transform[16];
    } ar;
    struct {
        bool valid;
        bool isTracked;
        double x() {
            return transformFace[12];
        };
        double y() {
            return transformFace[13];
        };
        double z() {
            return transformFace[14];
        };
        double transformFace[16];
        double leftEyeX() {
            return transformLeftEye[12];
        };
        double leftEyeY() {
            return transformLeftEye[13];
        };
        double leftEyeZ() {
            return transformLeftEye[14];
        };
        double transformLeftEye[16];
        double rightEyeX() {
            return transformRightEye[12];
        };
        double rightEyeY() {
            return transformRightEye[13];
        };
        double rightEyeZ() {
            return transformRightEye[14];
        };
        double transformRightEye[16];
    } face;
    struct {
        bool valid;
        double physicalWidth;
        double physicalHeight;
        double estimatedScale;
        double x;
        double y;
        double z;
        double transformMatrix[16];
    } trackedImage;

    WaldoState() {
        info.valid = false;
        ping.valid = false;
        ar.valid = false;
        face.valid = false;
        trackedImage.valid = false;
        local.time = 0;
        local.lossCount = 0;
        local.statistics.fps = 0.0;
        local.statistics.latency = -1;
        local.statistics.loss = 0.0;
    }
};

class Waldo {
  public:
    Waldo() : running(false), hThread(NULL), deviceId(0), port(3010) {
    }
    ~Waldo();

    void start(long deviceId, u_short pert = 3010);
    void stop();
    void setDeviceId(long deviceId);
    long getDeviceId();
    void getState(WaldoState *outState);
    bool isReceiving();

    static void getLocalIPAddress(char *str, size_t len = 16);

  private:
    bool running;
    HANDLE hThread;
    u_short port;

    WaldoState state;
    long deviceId;
    std::mutex lock;

    static DWORD WINAPI StartThreadStatic(LPVOID lpParam);
    DWORD StartThread();

    DWORD sendPing(SOCKET s, struct sockaddr *addr);

    bool parseMessage(char *buffer, WaldoState *outState);
    void parseState(const char *name, char **grps, size_t grpc, WaldoState *outState);
    bool shouldKeepMessage(WaldoState *newState);

    void calculateStatistics(DWORD lastPingSentTime, long lastPingTime, long *pelapsedTimerMessageCount,
                             long *pElapsedTimeMessageLossCount, DWORD *pdwEndTime);
};