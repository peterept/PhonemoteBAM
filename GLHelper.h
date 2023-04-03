#pragma once

#include <GL/gl.h>

GLuint glHelperGenFont(HDC hDC, GLsizei start, GLsizei size, LPGLYPHMETRICSFLOAT pGMF);
void glHelperDeleteFont(GLuint list, GLsizei size);
void glHelperDrawText(GLuint list, GLsizei start, GLsizei size, LPGLYPHMETRICSFLOAT pGMF, float x, float y,
                      const char *text, float scale = 0.1, GLfloat *color = NULL, bool center = true);
void glHelperDrawImage(GLuint texture, float scale, float aspectRatio = 1.0);
GLuint glHelperLoadImageFromResource(HMODULE hModule, LPTSTR lpName, LPTSTR lpType, int *pWidth = NULL,
                                     int *pHeight = NULL);
