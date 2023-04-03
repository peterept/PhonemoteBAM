#pragma once

#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mutex>

class ImageCalibration {
  public:
    ImageCalibration(HMODULE hModule)
        : hModule(hModule), calibrationSequence(CalibrationSequenceNotRunning), calibrationTimeout(18000), tex_2d(0),
          fontList(0), isReceiving(false) {
    }

    void start();
    void stop();
    void scanned(glm::mat4 transform);
    void setIsReceiving(bool is);
    void render(HDC hDC);

  private:
    enum CalibrationSequence {
        CalibrationSequenceNotRunning = 0,
        CalibrationSequenceShowCheckerboard,
        CalibrationSequenceRenderStart,
        CalibrationSequenceWaitForScan,
        CalibrationSequenceScanned,
        CalibrationSequenceShutdown,
    } calibrationSequence;
    void setCalibrationSequence(CalibrationSequence seq) {
        lock.lock();
        calibrationSequence = seq;
        lock.unlock();
    }
    CalibrationSequence getCalibrationSequence() {
        lock.lock();
        CalibrationSequence seq = calibrationSequence;
        lock.unlock();
        return seq;
    }
    std::mutex lock;

    HMODULE hModule;
    bool isReceiving;
    glm::mat4 scanTransform;

    DWORD calibrationTimeout;
    DWORD calibrationTimeoutEndsMs;

    // screen size
    int screenWidth;
    int screenHeight;

    // calibration image texture
    GLuint tex_2d;
    int texWidth;
    int texHeight;
    float texPhysicalWidthMeters;

    // font
    GLuint fontList;
    GLYPHMETRICSFLOAT fontGMF[96];

    // checkerboard points
    double cpoints[9];

    // render handlers
    void renderStart(HDC hDC);
    void renderDestroy();
    void renderWaitForScan();

    // checkerboard calculators
    static float calculateScreenWidthInMeters(double *pointsXY);
    static bool calculate3Points(int screenWidth, int screenHeight, double *pointsXY, glm::mat4 imgTransform,
                                 double *outPointsXYZ);
};
