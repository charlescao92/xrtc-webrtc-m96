#include "YUVOpenGLWidget.h"
#include <QOpenGLTexture>
#include <QOpenGLBuffer>

YUVOpenGLWidget::YUVOpenGLWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    textureUniformY = 0;
    textureUniformU = 0;
    textureUniformV = 0;
    id_y = 0;
    id_u = 0;
    id_v = 0;
    m_pVSHader = NULL;
    m_pFSHader = NULL;
    m_pShaderProgram = NULL;
    m_pTextureY = NULL;
    m_pTextureU = NULL;
    m_pTextureV = NULL;
}

YUVOpenGLWidget::~YUVOpenGLWidget()
{
    releaseMemory();
}

void YUVOpenGLWidget::updateFrame(MediaFrameSharedPointer frame)
{
#if 1
    int src_width = frame->fmt.sub_fmt.video_fmt.width;
    int src_height = frame->fmt.sub_fmt.video_fmt.height;
    int stridey = frame->stride[0];
    int strideu = frame->stride[1];
    int stridev = frame->stride[2];
    int size = stridey * src_height + (strideu + stridev) * ((src_height + 1) / 2);

    m_videoFrame.reset(new xrtc::MediaFrame(size));
    m_videoFrame->fmt.sub_fmt.video_fmt.type = xrtc::SubMediaType::kSubTypeI420;
    m_videoFrame->fmt.sub_fmt.video_fmt.width = src_width;
    m_videoFrame->fmt.sub_fmt.video_fmt.height = src_height;
    m_videoFrame->stride[0] = stridey;
    m_videoFrame->stride[1] = strideu;
    m_videoFrame->stride[2] = stridev;
    m_videoFrame->data_len[0] = stridey * src_height;
    m_videoFrame->data_len[1] = strideu * ((src_height + 1) / 2);
    m_videoFrame->data_len[2] = stridev * ((src_height + 1) / 2);
    m_videoFrame->data[1] = m_videoFrame->data[0] + m_videoFrame->data_len[0];
    m_videoFrame->data[2] = m_videoFrame->data[1] + m_videoFrame->data_len[1];
    memcpy(m_videoFrame->data[0], frame->data[0], frame->data_len[0]);
    memcpy(m_videoFrame->data[1], frame->data[1], frame->data_len[1]);
    memcpy(m_videoFrame->data[2], frame->data[2], frame->data_len[2]);

    update();

#else
    static FILE* fp = fopen("f://save_preview.yuv", "wb+");
    if (fp != NULL) {
        int width = frame->fmt.sub_fmt.video_fmt.width;
        int height = frame->fmt.sub_fmt.video_fmt.height;

        fwrite(frame->data[0], 1, width * height, fp);
        fwrite(frame->data[1], 1, width * height / 4, fp);
        fwrite(frame->data[2], 1, width * height / 4, fp);
        fflush(fp);
    }
#endif

}

void YUVOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    // opengl渲染管线依赖着色器来处理传入的数据
    // 着色器：就是使用openGL着色语言(OpenGL Shading Language,GLSL)编写的一个小函数,GLSL是构成所有OpenGL着色器的语言
    // 初始化顶点着色器、对象
    m_pVSHader = new QOpenGLShader(QOpenGLShader::Vertex, this);

    // 顶点着色器源码
    const char* vsrc = "attribute vec4 vertexIn; \
    attribute vec2 textureIn; \
    varying vec2 textureOut;  \
    void main(void)           \
    {                         \
        gl_Position = vertexIn; \
        textureOut = textureIn; \
    }";

    // 编译顶点着色器程序
    m_pVSHader->compileSourceCode(vsrc);

    // 初始化片段着色器功能gpu中yuv转换成rgb
    m_pFSHader = new QOpenGLShader(QOpenGLShader::Fragment, this);

    // 片段着色器源码
    const char* fsrc = "varying vec2 textureOut; \
    uniform sampler2D tex_y; \
    uniform sampler2D tex_u; \
    uniform sampler2D tex_v; \
    void main(void) \
    { \
        vec3 yuv; \
        vec3 rgb; \
        yuv.x = texture2D(tex_y, textureOut).r; \
        yuv.y = texture2D(tex_u, textureOut).r - 0.5; \
        yuv.z = texture2D(tex_v, textureOut).r - 0.5; \
        rgb = mat3( 1,       1,         1, \
                    0,       -0.39465,  2.03211, \
                    1.13983, -0.58060,  0) * yuv; \
        gl_FragColor = vec4(rgb, 1); \
    }";

    // 将glsl源码送入编译器编译着色器程序
    m_pFSHader->compileSourceCode(fsrc);

    // 创建着色器程序容器
    m_pShaderProgram = new QOpenGLShaderProgram;
    // 将片段着色器添加到程序容器
    m_pShaderProgram->addShader(m_pFSHader);
    // 将顶点着色器添加到程序容器
    m_pShaderProgram->addShader(m_pVSHader);
    // 绑定属性vertexIn到指定位置ATTRIB_VERTEX,该属性在顶点着色源码其中有声明
    m_pShaderProgram->bindAttributeLocation("vertexIn", ATTRIB_VERTEX);
    // 绑定属性textureIn到指定位置ATTRIB_TEXTURE,该属性在顶点着色源码其中有声明
    m_pShaderProgram->bindAttributeLocation("textureIn", ATTRIB_TEXTURE);
    // 链接所有所有添入到的着色器程序
    m_pShaderProgram->link();
    // 激活所有链接
    m_pShaderProgram->bind();

    // 读取着色器中的数据变量tex_y, tex_u, tex_v的位置,这些变量的声明可以在
    // 片段着色器源码中可以看到
    textureUniformY = m_pShaderProgram->uniformLocation("tex_y");
    textureUniformU = m_pShaderProgram->uniformLocation("tex_u");
    textureUniformV = m_pShaderProgram->uniformLocation("tex_v");

    // 顶点矩阵
    static const GLfloat vertexVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         -1.0f, 1.0f,
         1.0f, 1.0f,
    };

    // 纹理矩阵
    static const GLfloat textureVertices[] = {
        0.0f,  1.0f,
        1.0f,  1.0f,
        0.0f,  0.0f,
        1.0f,  0.0f,
    };

    // 设置属性ATTRIB_VERTEX的顶点矩阵值以及格式
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertexVertices);
    // 设置属性ATTRIB_TEXTURE的纹理矩阵值以及格式
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
    // 启用ATTRIB_VERTEX属性的数据,默认是关闭的
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    // 启用ATTRIB_TEXTURE属性的数据,默认是关闭的
    glEnableVertexAttribArray(ATTRIB_TEXTURE);

    // 分别创建y,u,v纹理对象
    m_pTextureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    m_pTextureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    m_pTextureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    m_pTextureY->create();
    m_pTextureU->create();
    m_pTextureV->create();

    // 获取返回y分量的纹理索引值
    id_y = m_pTextureY->textureId();
    // 获取返回u分量的纹理索引值
    id_u = m_pTextureU->textureId();
    // 获取返回v分量的纹理索引值
    id_v = m_pTextureV->textureId();

    // 设置背景色
    glClearColor(0.3, 0.3, 0.3, 0.0);

}
void YUVOpenGLWidget::resizeGL(int w, int h)
{
    if (h == 0) // 防止被零除
    {
        h = 1;  // 将高设为1
    }

    if (w == 0) {
        w = 1;
    }

    glViewport(0, 0, w, h);
}

void YUVOpenGLWidget::paintGL()
{
    if ( nullptr == m_videoFrame) {
        return;
    }

    int width = m_videoFrame->fmt.sub_fmt.video_fmt.width;
    int height = m_videoFrame->fmt.sub_fmt.video_fmt.height;

    // 加载y数据纹理
    // 激活纹理单元GL_TEXTURE0
    glActiveTexture(GL_TEXTURE0);
    // 使用来自y数据生成纹理
    glBindTexture(GL_TEXTURE_2D, id_y);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED,
        GL_UNSIGNED_BYTE, m_videoFrame->data[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 加载u数据纹理
    // 激活纹理单元GL_TEXTURE1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, id_u);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED,
        GL_UNSIGNED_BYTE, m_videoFrame->data[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 加载v数据纹理
    // 激活纹理单元GL_TEXTURE2
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, id_v);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED,
        GL_UNSIGNED_BYTE, m_videoFrame->data[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 指定y纹理要使用新值 只能用0,1,2等表示纹理单元的索引，这是opengl不人性化的地方
    // 0对应纹理单元GL_TEXTURE0 1对应纹理单元GL_TEXTURE1 2对应纹理的单元
    glUniform1i(textureUniformY, 0);
    // 指定u纹理要使用新值
    glUniform1i(textureUniformU, 1);
    // 指定v纹理要使用新值
    glUniform1i(textureUniformV, 2);
    // 使用顶点数组方式绘制图形
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

}

void YUVOpenGLWidget::releaseMemory()
{

}

