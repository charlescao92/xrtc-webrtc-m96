#ifndef YUVOPENGLWIDGET_H
#define YUVOPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>

#include "defs.h"

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4

class YUVOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    YUVOpenGLWidget(QWidget* parent = nullptr);
    ~YUVOpenGLWidget();

public:
    void updateFrame(MediaFrameSharedPointer videoFrame);

private:
    void initializeGL() Q_DECL_OVERRIDE;
    void resizeGL(int w, int h) Q_DECL_OVERRIDE;
    void paintGL() Q_DECL_OVERRIDE;

    void releaseMemory();

private:
    /**
     * 纹理是一个2D图片，它可以用来添加物体的细节（贴图），纹理可以各种变形后
     * 贴到不同形状的区域内。这里直接用纹理显示视频帧
     */
    GLuint textureUniformY; // y纹理数据位置
    GLuint textureUniformU; // u纹理数据位置
    GLuint textureUniformV; // v纹理数据位置
    GLuint id_y; // y纹理对象ID
    GLuint id_u; // u纹理对象ID
    GLuint id_v; // v纹理对象ID
    QOpenGLTexture* m_pTextureY;  // y纹理对象
    QOpenGLTexture* m_pTextureU;  // u纹理对象
    QOpenGLTexture* m_pTextureV;  // v纹理对象
    /* 着色器：控制GPU进行绘制 */
    QOpenGLShader* m_pVSHader;  // 顶点着色器程序对象
    QOpenGLShader* m_pFSHader;  // 片段着色器对象
    QOpenGLShaderProgram* m_pShaderProgram; // 着色器程序容器

    MediaFrameSharedPointer m_videoFrame;
};

#endif // YUVOPENGLWIDGET_H

