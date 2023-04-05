#include "pch.h"

#include "PhonemoteBAM.h"

#include "Version.h"

PhonemoteBAM::PhonemoteBAM(HMODULE hModule, HMODULE bam_module, int pluginId)
    : hModule(hModule), hThread(NULL), running(false), calibration(hModule) {
    BAM::Init(pluginId, bam_module);
}

PhonemoteBAM::~PhonemoteBAM() {
    OnPluginStop();
}

void PhonemoteBAM::createMenu(BAM::BAM_OnButtonPress_func *calibrateCallback) {
    Version version(hModule);

    char info[256];
    sprintf_s(info,
              "Phonemote BAM Plugin\n"
              "#c777#phonemote.com\n"
              "Copyright (c) 2023 Peter Koch\n"
              "v%s",
              version.productVersionString().c_str());

    BAM::menu::create("Phonemote", info);
    BAM::menu::info("#-Head Tracking using the Phonemote mobile app" DEFIW);
    BAM::menu::info("#-Download it from the iOS app store" DEFIW);
    BAM::menu::info("");

    BAM::menu::info("#c777##-Tracking" DEFIW);
    BAM::menu::info3D("Position: " DEFPW "%.2f,  %.2f,  %.2f", &X, &Y, &Z);
    BAM::menu::info3D("FPS: " DEFPW "%.0f", &FPS, &FPS, &FPS);
    BAM::menu::info3D("Latency: " DEFPW "%.0f ms", &LATENCY, &LATENCY, &LATENCY);
    BAM::menu::command("Calibrate" DEFIW, calibrateCallback);
    BAM::menu::info("#cfa0##-Note: Also set the physical display size" DEFIW);
    BAM::menu::info("#cfa0##- in the \"Config -> Reality\" menu below" DEFIW);
    BAM::menu::info("");

    BAM::menu::info("#c777##-Configuration" DEFIW);
    BAM::menu::paramValue("ID: " DEFPW "%.0f", &cfg.deviceId, 1.0, 1.0, "");
    char addr[128];
    sprintf_s(addr, "Host IP Address: " DEFPW);
    Waldo::getLocalIPAddress(&addr[strlen(addr)]);
    BAM::menu::info(addr);
    BAM::menu::info3D("Port: " DEFPW "%.0f", &cfg.port, &cfg.port, &cfg.port);
    static const char *trackingOptions[] = {"FACE", "PHONE"};
    BAM::menu::paramSwtich("Track: " DEFPW "#cfff# %s", &cfg.trackingOption, trackingOptions,
                           ARRAY_ENTRIES(trackingOptions), "Select tracking method.");
    static const char *faceTrackingOptions[] = {"CENTER", "LEFT EYE", "RIGHT EYE"};
    BAM::menu::paramSwtich("Face Eye Point: " DEFPW "#cfff# %s", &cfg.faceTrackingOption, faceTrackingOptions,
                           ARRAY_ENTRIES(faceTrackingOptions), "Select face tracking options.");

    BAM::menu::info("");
    BAM::menu::info("#c777##-Standard options" DEFIW);
    BAM::menu::defaults::translateAndLighting();
    BAM::menu::defaults::reality();
}

void PhonemoteBAM::OnPluginStart() {
    BAM::helpers::load("PhonemoteBAM", &cfg, sizeof(cfg));
    waldo.start((long)cfg.deviceId);

    running = true;
    hThread = CreateThread(NULL, 0, StaticThreadStart, (void *)this, 0, 0);
}

void PhonemoteBAM::OnPluginStop() {
    if (running) {
        running = false;
        waldo.stop();
        BAM::helpers::save("PhonemoteBAM", &cfg, sizeof(cfg));

        WaitForSingleObject(hThread, 50);
        hThread = NULL;
    }
}

void PhonemoteBAM::onSwapBuffers(HDC hDC) {
    calibration.render(hDC);
}

void PhonemoteBAM::calibrate() {
    calibration.start();
}

void PhonemoteBAM::pushXYZ(double x, double y, double z, double wts) {
    X = x;
    Y = y;
    Z = z;
    BAM::push::rawXYZ_wts(x, y, z, wts);
}

void PhonemoteBAM::update() {
    WaldoState state;
    waldo.setDeviceId((long)cfg.deviceId);
    waldo.getState(&state);

    // Handle calibration
    calibration.setIsReceiving(waldo.isReceiving());
    if (state.trackedImage.valid) {
        glm::mat4 imgTransform = glm::make_mat4(state.trackedImage.transformMatrix);
        calibration.scanned(imgTransform);
    }

    // Handle Tracking: Phone position
    if (cfg.trackingOption == (int)TrackingType::Phone) {
        if (state.ar.valid) {
            pushXYZ(state.ar.x(), -state.ar.y(), state.ar.z(), (double)state.local.time);
        }
    }
    // Handle Tracking: Face position
    else {
        if (state.face.valid) {
            glm::mat4 faceTransform = glm::make_mat4(state.face.transformFace);
            glm::mat4 eyeOffset(1.0f);

            if (cfg.faceTrackingOption == (int)FaceTrackingType::LeftEye) {
                eyeOffset[3] = glm::vec4(state.face.leftEyeX(), state.face.leftEyeY(), state.face.leftEyeZ(), 1.0f);
            } else if (cfg.faceTrackingOption == (int)FaceTrackingType::RightEye) {
                eyeOffset[3] = glm::vec4(state.face.rightEyeX(), state.face.rightEyeY(), state.face.rightEyeZ(), 1.0f);
            }
            faceTransform = faceTransform * eyeOffset;

            glm::vec3 pos = glm::vec3(faceTransform[3]);
            pushXYZ(pos.x, -pos.y, pos.z, (double)state.local.time);
        }
    }

    // Update statistics
    FPS = state.local.statistics.fps;
    LATENCY = (double)state.local.statistics.latency;
}

DWORD WINAPI PhonemoteBAM::StaticThreadStart(LPVOID instance) {
    PhonemoteBAM *phonemoteBAM = (PhonemoteBAM *)instance;
    return phonemoteBAM->ThreadStart();
}

DWORD PhonemoteBAM::ThreadStart() {
    int UpdatesPerSecond = 100; // Max Update FPS
    DWORD sleepMs = 1000 / UpdatesPerSecond;

    while (running) {
        update();
        Sleep(sleepMs);
    }
    return S_OK;
}
