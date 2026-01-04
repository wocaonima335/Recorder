#ifndef FFGLITEM_H
#define FFGLITEM_H

#include <QByteArray>
#include <QMutex>
#include <QQuickFramebufferObject>

extern "C" {
#include <libavformat/avformat.h>
}

class FFGLItem : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(bool keepRatio READ keepRatio WRITE setKeepRatio NOTIFY keepRatioChanged)
    Q_PROPERTY(float aspect READ aspect WRITE setAspect NOTIFY aspectChanged)
public:
    FFGLItem() = default;
    Renderer *createRenderer() const override;

    bool keepRatio() const;
    void setKeepRatio(bool v)
    {
        if (m_keepRatio != v) {
            m_keepRatio = v;
            emit keepRatioChanged();
            update();
        }
    }
    float aspect() const { return m_aspect; }
    void setAspect(float v)
    {
        if (m_aspect != v) {
            m_aspect = v;
            emit aspectChanged();
            update();
        }
    }

    Q_INVOKABLE void setYUVData(
        const QByteArray &y, const QByteArray &u, const QByteArray &v, int w, int h);
    Q_INVOKABLE void setFrame(AVFrame *frame); // 与现有 FFGLRenderWidget 接口一致

signals:
    void keepRatioChanged();
    void aspectChanged();

public:
    mutable QMutex mtx;
    QByteArray yData, uData, vData;
    int width = 0, height = 0;
    bool m_keepRatio = false;
    float m_aspect = 1.60f / 9.0f;
};

class PreviewBridge : public QObject
{
public:
    static PreviewBridge &instance()
    {
        static PreviewBridge b;
        return b;
    }
    void setPreviewItem(FFGLItem *item) { m_item = item; }
    FFGLItem *get() const { return m_item; }

private:
    QPointer<FFGLItem> m_item;
};

#endif // FFGLITEM_H
