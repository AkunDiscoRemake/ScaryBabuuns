// Renderização GLES3: shaders, texturas, modelos .bm
#pragma once
#include <GLES3/gl3.h>
#include <string>
#include <vector>
#include <map>
#include "mathx.h"

enum MatFlags { F_TINT = 1, F_UNLIT = 2, F_ALPHA = 4, F_SCREEN = 8, F_DOUBLE = 16, F_WIND = 32 };

struct Texture { GLuint id = 0; int w = 0, h = 0; };

struct Section {
    GLuint vao = 0, vbo = 0, ibo = 0;
    int count = 0;
    Texture *tex = nullptr;
    float color[4] = {1, 1, 1, 1};
    unsigned flags = 0;
    V3 bcenter; float bradius = 0;  // esfera envolvente (espaço do nó)
};

struct Node {
    std::string name;
    int parent = -1;
    M4 local;
    std::vector<Section> secs;
};

struct Model {
    std::vector<Node> nodes;
    int find(const char *n) const {
        for (size_t i = 0; i < nodes.size(); i++) if (nodes[i].name == n) return (int)i;
        return -1;
    }
};

// Pose: rotações extras por nó (aplicadas após a transformação local)
struct Pose {
    std::vector<Q> rot;
    std::vector<V3> off;
    void init(const Model *m) { rot.assign(m->nodes.size(), Q()); off.assign(m->nodes.size(), V3()); }
};

struct Light { V3 pos; float radius; V3 color; };

struct Scene {
    // parâmetros de iluminação globais
    V3 skyAmb = V3(0.20f, 0.22f, 0.32f), groundAmb = V3(0.08f, 0.07f, 0.06f);
    V3 sunDir = norm(V3(0.3f, 0.8f, 0.4f)), sunCol = V3(0.35f, 0.4f, 0.55f);
    V3 fogCol = V3(0.03f, 0.04f, 0.07f); float fogDens = 0.045f;
    V3 clear = V3(0.03f, 0.04f, 0.07f);
    Light lights[4]; int nLights = 0;
    float time = 0;
};

struct DrawCtx {
    M4 view, proj, vp;
    V3 camPos, camFwd;
    float cullDist = 80;
};

class Gfx {
public:
    bool init();
    Texture *tex(const std::string &name);
    Model *model(const std::string &name);
    void begin(const DrawCtx &ctx, const Scene &sc);
    // desenha um modelo; tint = cor para materiais F_TINT; tint2 = segunda cor (acessórios)
    void draw(const Model *m, const M4 &world, const V3 &tint = V3(1, 1, 1), const Pose *pose = nullptr,
              GLuint screenTex = 0, float alpha = 1.f, const V3 *tint2 = nullptr);
    void drawNode(const Model *m, int node, const M4 &world, const V3 &tint = V3(1, 1, 1));
    // quad colorido (overlay) em espaço de mundo
    void quad(const M4 &world, const float rgba[4], GLuint texId = 0, bool additive = false);
    GLuint whiteTex() { return tex("white")->id; }

    // texto
    void text(const M4 &world, const char *utf8, float size, const float rgba[4], int align = 0, float maxW = 0);
    float textWidth(const char *utf8, float size);

    DrawCtx ctx;
    Scene sc;
private:
    GLuint prog = 0, progFlat = 0;
    GLint uMVP, uModel, uTex, uColor, uFlags, uSky, uGround, uSunDir, uSunCol, uFogCol, uFogDens, uCam, uLPos, uLCol, uTime, uAlpha;
    GLint fMVP, fTex, fColor;
    GLuint quadVao = 0, quadVbo = 0, textVao = 0, textVbo = 0;
    std::map<std::string, Texture *> texs;
    std::map<std::string, Model *> models;
    unsigned char fontW[256];
    void drawSection(const Section &s, const M4 &w, const V3 &tint, GLuint screenTex, float alpha, const V3 *tint2);
};

// leitura de assets (implementado em main.cpp)
bool readAsset(const char *path, std::vector<unsigned char> &out);
std::string utf8ToLatin1(const char *s);
