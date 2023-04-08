#include "pch.h"

#include "ImageCalibration.h"

#include "BAM.h"
#include "GLHelper.h"
#include "Waldo.h"
#include "resource.h"

#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

void ImageCalibration::start() {
    if (getCalibrationSequence() == CalibrationSequenceNotRunning) {
        setCalibrationSequence(CalibrationSequenceShowCheckerboard);
    }
}

void ImageCalibration::stop() {
    if (getCalibrationSequence() != CalibrationSequenceNotRunning) {
        setCalibrationSequence(CalibrationSequenceShutdown);
    }
}

void ImageCalibration::scanned(glm::mat4 transform) {
    if (getCalibrationSequence() == CalibrationSequenceWaitForScan) {
        scanTransform = transform;
        setCalibrationSequence(CalibrationSequenceScanned);
    }
}

void ImageCalibration::setIsReceiving(bool is) {
    isReceiving = is;
}

void ImageCalibration::render(HDC hDC) {

    CalibrationSequence seq = getCalibrationSequence();
    if (seq == CalibrationSequenceNotRunning) {
        return;
    }

    // clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    switch (seq) {
    case CalibrationSequenceShowCheckerboard:
        calibrationTimeoutEndsMs = GetTickCount() + calibrationTimeout;
        BAM::menu::showCheckerboard(1);
        // wait for next frame before using the calibration points
        setCalibrationSequence(CalibrationSequenceRenderStart);
        break;

    case CalibrationSequenceRenderStart:
        BAM::menu::showCheckerboard(0); // hide checkerboard
        double *points;
        points = BAM::menu::getChecerboardPoints();
        if (!points) {
            break;
        }
        for (int i = 0; i < 6; i++) {
            cpoints[i] = points[i];
        }
        renderStart(hDC);
        setCalibrationSequence(CalibrationSequenceWaitForScan);

        /* fall through */

    case CalibrationSequenceWaitForScan:
        renderWaitForScan();
        break;

    case CalibrationSequenceScanned:
        double pointsXYZ[9];
        calculate3Points(screenWidth, screenHeight, cpoints, scanTransform, pointsXYZ);
        BAM::push::cherkerboard_3points(pointsXYZ);

        /* fall through */

    case CalibrationSequenceShutdown:
        // clean up
        BAM::menu::showCheckerboard(0); // make sure to close the checkerboard
        renderDestroy();
        setCalibrationSequence(CalibrationSequenceNotRunning);
        return;
    }

    // abort on timeout
    if (GetTickCount() > calibrationTimeoutEndsMs) {
        setCalibrationSequence(CalibrationSequenceShutdown);
    }

    // hack: Hides the BAM screen and/or checkerboard
    glViewport(0, 0, 0, 0);
}

void ImageCalibration::renderStart(HDC hDC) {

    // get screen size (assume the screen is in portrait mode)
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    screenWidth = viewport[2];
    screenHeight = viewport[3];
    if (screenWidth > screenHeight) {
        screenWidth = viewport[3];
        screenHeight = viewport[2];
    }

    // Load calibration image
    tex_2d = glHelperLoadImageFromResource(hModule, MAKEINTRESOURCE(IDR_CALIBRATION), RT_RCDATA, &texWidth, &texHeight);
    texPhysicalWidthMeters = 0.206f;

    // Setup font
    if (!fontList) {
        fontList = glHelperGenFont(hDC, 32, 96, fontGMF);
    }
}

void ImageCalibration::renderDestroy() {

    if (tex_2d) {
        glDeleteTextures(1, &tex_2d);
        tex_2d = 0;
    }

    if (fontList) {
        glHelperDeleteFont(fontList, 96);
        fontList = 0;
    }
}

void ImageCalibration::renderWaitForScan() {

    glPushAttrib(GL_LIGHTING);
    glDisable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // draw calibration image - we have the information to render it at correct real-world size
    float screenPhysicalWidthMeters = (float)calculateScreenWidthInMeters(cpoints);
    float imageScale = texPhysicalWidthMeters / screenPhysicalWidthMeters;

    glHelperDrawImage(tex_2d, imageScale, (float)texWidth / (float)texHeight);

    GLfloat textColor[3] = {0.6f, 0.6f, 0.6f};
    glHelperDrawText(fontList, 32, 96, fontGMF, 0, 0.5f,
                     "To calibrate Phonemote scan this image\nusing the rear facing camera", 0.1f, textColor, true);

    if (isReceiving) {
        GLfloat goodColor[3] = {0.1f, 0.8f, 0.1f};
        glHelperDrawText(fontList, 32, 96, fontGMF, 0, 0.7f, "Great News: Phonemote is currently connected!", 0.075f,
                         goodColor, true);
    } else {
        GLfloat badColor[3] = {0.8f, 0.1f, 0.1f};
        glHelperDrawText(
            fontList, 32, 96, fontGMF, 0, 0.7f,
            "Warning: Phonemote is not detected!\nMake sure it is running or download from the iOS AppStore", 0.07f,
            badColor, true);

        char addr[128];
        sprintf_s(addr, "Note: Your Host IP address is ");
        Waldo::getLocalIPAddress(&addr[strlen(addr)]);
        glHelperDrawText(fontList, 32, 96, fontGMF, 0, 0.85f, addr, 0.05f, textColor, true);
    }

    // timeout
    char strTimeout[32];
    GLfloat textColor2[3] = {1.0f, 1.0f, 0.2f};
    sprintf_s(strTimeout, "%d", (calibrationTimeoutEndsMs - GetTickCount()) / 1000);
    glHelperDrawText(fontList, 32, 96, fontGMF, -1.0f, 0.95f, strTimeout, 0.1f, textColor2, true);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();
}

float ImageCalibration::calculateScreenWidthInMeters(double *pointsXY) {
    if (!pointsXY) {
        return 0.0;
    }

    float leftPointInMeters = (float)pointsXY[0];
    float rightPointInMeters = (float)pointsXY[2];
    return rightPointInMeters + leftPointInMeters;
}

bool ImageCalibration::calculate3Points(int screenWidth, int screenHeight, double *pointsXY, glm::mat4 imgTransform,
                                        double *outPointsXYZ) {
    if (!pointsXY || !outPointsXYZ) {
        return false;
    }

    // Screen dimensions in Meters
    float aspect = (float)screenWidth / (float)screenHeight;
    float w = calculateScreenWidthInMeters(pointsXY);
    float h = w / aspect;

    // Center position in Meters
    float cx = w / 2.0f;
    float cy = h / 2.0f;

    // Convert each screen X,Y (meters( point to real world X,Y,Z (meters)
    for (int i = 0; i < 3; i++) {
        float sx = (float)pointsXY[i * 2 + 0];
        float sy = (float)pointsXY[i * 2 + 1];

        // marker image is in the center of the screen, so we can calculate the real world positions
        glm::mat4 mblTranslation(1.0f); // identity matrix
        mblTranslation[3] = glm::vec4(glm::vec3(sx - cx, 0.0f, -(sy - cy)), 1.0f);
        glm::mat4 blTransform = imgTransform * mblTranslation;

        outPointsXYZ[i * 3 + 0] = blTransform[3].x;
        outPointsXYZ[i * 3 + 1] = -blTransform[3].y;
        outPointsXYZ[i * 3 + 2] = blTransform[3].z;
    }

    return true;
}
