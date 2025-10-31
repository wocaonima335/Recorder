#ifndef FFGLRENDERER_H
#define FFGLRENDERER_H

#include "ffglitem.h"

#include <QOpenGLFunctions>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLShaderProgram>

class FFGLRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions_4_5_Core
{
public:
    FFGLRenderer(FFGLItem *item);
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
    void render() override;
    void synchronize(QQuickFramebufferObject *item) override;

private:
    void ensureProgram();
    void ensureBuffers();
    void ensureTextures();

private:
    FFGLItem *m_item = nullptr;
    QSize m_fboSize;
    QOpenGLShaderProgram *program = nullptr;
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint yTex = 0, uTex = 0, vTex = 0;
};

#endif // FFGLRENDERER_H
