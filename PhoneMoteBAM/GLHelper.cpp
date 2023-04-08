#include "pch.h"

#include "GLHelper.h"

#include <SOIL2/SOIL2.h>

GLuint glHelperGenFont(HDC hDC, GLsizei start, GLsizei size, LPGLYPHMETRICSFLOAT pGMF) {
    int height = 64; // Note: This height doesn't effect fonts generated with wglUseFontOutlines

    HFONT hFont = CreateFontA(-(height), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 0, 0, 0, 0, "Arial");
    HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);

    glPushAttrib(GL_POLYGON_BIT);
    GLuint base = glGenLists(size);
    wglUseFontOutlines(hDC, start, size, base, 0.0f, (float)0.1, WGL_FONT_POLYGONS, pGMF);
    glPopAttrib();

    SelectObject(hDC, hOldFont);
    DeleteObject(hFont);
    return base;
}

void glHelperDeleteFont(GLuint list, GLsizei size) {
    glDeleteLists(list, size);
}

void glHelperDrawText(GLuint list, GLsizei start, GLsizei size, LPGLYPHMETRICSFLOAT pGMF, float x, float y,
                      const char *text, float scale, GLfloat *color, bool center) {

    // color
    GLfloat defaultColor[3] = {1.0, 1.0, 1.0};
    if (color == NULL) {
        color = defaultColor;
    }
    glColor3fv(color);

    // font aspect ratio
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2];
    int height = viewport[3];
    float aspect = (float)width / (float)height;

    float lineHeight = pGMF['!' - start].gmfBlackBoxY * scale;

    char seps[] = "\n";
    char *next_token;

    char *copy = _strdup(text);
    if (copy == NULL) {
        return;
    }

    char *s = strtok_s(copy, seps, &next_token);
    while (s != NULL) {
        // calculate length
        float len = 0;
        for (const char *pch = s; *pch; pch++) {
            len += pGMF[*pch - start].gmfCellIncX;
        }
        len *= scale;

        glLoadIdentity();

        if (center) {
            glTranslatef(y, -len / 2, 0.0f);
        } else {
            glTranslatef(y, x, 0.0f);
        }
        glRotatef(90.0, 0, 0, 1);
        glScalef(scale, scale / aspect, scale);

        glPushAttrib(GL_POLYGON_BIT); // save as wglUseFontOutlines can change from glFrontFace(GL_CCW)
        glPushAttrib(GL_LIST_BIT);

        glListBase(list - start);
        glCallLists(strlen(s), GL_UNSIGNED_BYTE, s);

        glPopAttrib();
        glPopAttrib();

        // find max line height
        y += lineHeight;
        s = strtok_s(NULL, seps, &next_token);
    }
    free(copy);
}

void glHelperDrawImage(GLuint texture, float scale, float aspectRatio) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    glRotatef(90.0, 0, 0, 1);
    glScalef(scale, scale * aspectRatio, 1);

    glBegin(GL_QUADS);
    glColor3f(255, 255, 255);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 1.0f);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

GLuint glHelperLoadImageFromResource(HMODULE hModule, LPTSTR lpName, LPTSTR lpType, int *pWidth, int *pHeight) {
    GLuint textId = 0;

    // Load image data
    HRSRC hRsrc = FindResource(hModule, lpName, lpType);
    if (hRsrc) {
        DWORD size = SizeofResource(hModule, hRsrc);
        HGLOBAL hRsrcData = LoadResource(hModule, hRsrc);
        LPVOID data = LockResource(hRsrcData);

        int width, height, channels;
        unsigned char *img =
            SOIL_load_image_from_memory((unsigned char *)data, size, &width, &height, &channels, SOIL_LOAD_AUTO);
        if (img) {
            if (pWidth) {
                *pWidth = width;
            }
            if (pHeight) {
                *pHeight = height;
            }

            textId = SOIL_create_OGL_texture(img, &width, &height, channels, SOIL_CREATE_NEW_ID,
                                             SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y | SOIL_FLAG_COMPRESS_TO_DXT);
            SOIL_free_image_data(img);
        }
    }
    return textId;
}