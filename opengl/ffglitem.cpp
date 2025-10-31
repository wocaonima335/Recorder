#include "ffglitem.h"
#include "ffglrenderer.h"

QQuickFramebufferObject::Renderer *FFGLItem::createRenderer() const
{
    return new FFGLRenderer(const_cast<FFGLItem *>(this));
}
bool FFGLItem::keepRatio() const
{
    return m_keepRatio;
}

void FFGLItem::setYUVData(const QByteArray &y, const QByteArray &u, const QByteArray &v, int w, int h)
{
    QMutexLocker lk(&mtx);
    yData = y;
    uData = u;
    vData = v;
    width = w;
    height = h;
    update();
}

void FFGLItem::setFrame(AVFrame *frame)
{
    if (!frame)
        return;
    // 将 AVFrame 拆成 Y/U/V 三路并复制到 QByteArray
    QMutexLocker lk(&mtx);
    width = frame->width;
    height = frame->height;
    yData = QByteArray(reinterpret_cast<const char *>(frame->data[0]), width * height);
    uData = QByteArray(reinterpret_cast<const char *>(frame->data[1]), width * height / 4);
    vData = QByteArray(reinterpret_cast<const char *>(frame->data[2]), width * height / 4);
    update();
    av_frame_unref(frame);
    av_frame_free(&frame);
}
