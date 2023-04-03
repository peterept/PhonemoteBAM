#pragma once

#include "BAM.h"
#include "ImageCalibration.h"
#include "Waldo.h"

class PhonemoteBAM {
  public:
    PhonemoteBAM(HMODULE hModule, HMODULE bam_module, int pluginId);
    ~PhonemoteBAM();
    void createMenu(BAM::BAM_OnButtonPress_func *calibrateCallback);
    void OnPluginStart();
    void OnPluginStop();
    void onSwapBuffers(HDC hDC);
    void calibrate();

  private:
    enum TrackingType {
        Face = 0,
        Phone = 1,
    };
    enum FaceTrackingType {
        Center = 0,
        LeftEye = 1,
        RightEye = 2,
    };
    struct Cfg {
        int trackingOption;
        int faceTrackingOption;
        double deviceId;
        double port;
    } cfg = {0, 0, 0.0, 3010.0};

    HMODULE hModule;
    HANDLE hThread;
    BOOL running;

    Waldo waldo;
    ImageCalibration calibration;

    // statistics
    double X = 0.0, Y = 0.0, Z = 0.0;
    double FPS = 0.0;
    double LATENCY = 0.0;

    static DWORD WINAPI StaticThreadStart(LPVOID instance);
    DWORD WINAPI ThreadStart();
    void pushXYZ(double x, double y, double z, double wts);
};