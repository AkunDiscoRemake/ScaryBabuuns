#include "gfx.h"
#include <android/log.h>
#include <stdlib.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BabuunHollow", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "BabuunHollow", __VA_ARGS__)

static const char *VS = R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec2 aUv; layout(location=3) in vec4 aCol;
uniform mat4 uMVP; uniform mat4 uModel; uniform int uFlags; uniform float uTime;
out vec3 vW; out vec3 vN; out vec2 vUv; out vec4 vC;
void main(){
  vec4 w = uModel * vec4(aPos,1.0);
  vec3 p = aPos;
  if ((uFlags & 32) != 0) { // vento nas folhas
    float s = sin(uTime*1.7 + w.x*0.35 + w.z*0.27) * 0.06 + sin(uTime*3.1 + w.x*1.3) * 0.02;
    p += vec3(s, 0.0, s*0.7) * clamp(aPos.y + 0.5, 0.0, 2.0);
  }
  vW = (uModel * vec4(p,1.0)).xyz; vN = mat3(uModel) * aNrm; vUv = aUv; vC = aCol;
  gl_Position = uMVP * vec4(p,1.0);
})";

static const char *FS = R"(#version 300 es
precision mediump float;
in vec3 vW; in vec3 vN; in vec2 vUv; in vec4 vC;
uniform sampler2D uTex; uniform vec4 uColor; uniform int uFlags; uniform float uAlpha;
uniform vec3 uSky, uGround, uSunDir, uSunCol, uFogCol, uCam; uniform float uFogDens;
uniform vec4 uLPos[4]; uniform vec3 uLCol[4];
out vec4 o;
void main(){
  vec4 t = texture(uTex, vUv);
  if ((uFlags & 4) != 0 && t.a < 0.5) discard;
  vec3 base = t.rgb * uColor.rgb * vC.rgb;
  vec3 c;
  if ((uFlags & 2) != 0) { c = t.rgb * uColor.rgb; }
  else {
    vec3 n = normalize(vN);
    if (!gl_FrontFacing) n = -n;
    float h = n.y*0.5+0.5;
    vec3 L = mix(uGround, uSky, h) + uSunCol * max(dot(n, uSunDir), 0.0);
    for (int i=0;i<4;i++){
      vec3 d = uLPos[i].xyz - vW; float dl = length(d);
      float a = clamp(1.0 - dl/uLPos[i].w, 0.0, 1.0);
      L += uLCol[i] * a*a * (0.35 + 0.65*max(dot(n, d/max(dl,0.001)), 0.0));
    }
    c = base * L;
  }
  float dist = length(vW - uCam);
  float f = 1.0 - exp(-uFogDens*uFogDens*dist*dist);
  if ((uFlags & 2) != 0) f *= 0.6;
  o = vec4(mix(c, uFogCol, f), uAlpha * uColor.a);
})";

static const char *VSF = R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=2) in vec2 aUv;
uniform mat4 uMVP; out vec2 vUv;
void main(){ vUv = aUv; gl_Position = uMVP*vec4(aPos,1.0); })";
static const char *FSF = R"(#version 300 es
precision mediump float; in vec2 vUv; uniform sampler2D uTex; uniform vec4 uColor; out vec4 o;
void main(){ vec4 t = texture(uTex, vUv); o = t*uColor; if (o.a < 0.01) discard; })";

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log); LOGE("shader: %s", log); }
    return s;
}
static GLuint link(const char *vs, const char *fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p, 2048, nullptr, log); LOGE("link: %s", log); }
    return p;
}

std::string utf8ToLatin1(const char *s) {
    std::string o;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if (*p < 0x80) o += (char)*p++;
        else if ((*p & 0xE0) == 0xC0 && p[1]) { o += (char)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F)); p += 2; }
        else { // caracteres fora do Latin-1: pula a sequência
            int n = (*p & 0xF0) == 0xE0 ? 3 : ((*p & 0xF8) == 0xF0 ? 4 : 1);
            for (int i = 0; i < n && *p; i++) p++;
            o += '?';
        }
    }
    return o;
}

bool Gfx::init() {
    prog = link(VS, FS);
    uMVP = glGetUniformLocation(prog, "uMVP"); uModel = glGetUniformLocation(prog, "uModel");
    uTex = glGetUniformLocation(prog, "uTex"); uColor = glGetUniformLocation(prog, "uColor");
    uFlags = glGetUniformLocation(prog, "uFlags"); uSky = glGetUniformLocation(prog, "uSky");
    uGround = glGetUniformLocation(prog, "uGround"); uSunDir = glGetUniformLocation(prog, "uSunDir");
    uSunCol = glGetUniformLocation(prog, "uSunCol"); uFogCol = glGetUniformLocation(prog, "uFogCol");
    uFogDens = glGetUniformLocation(prog, "uFogDens"); uCam = glGetUniformLocation(prog, "uCam");
    uLPos = glGetUniformLocation(prog, "uLPos"); uLCol = glGetUniformLocation(prog, "uLCol");
    uTime = glGetUniformLocation(prog, "uTime"); uAlpha = glGetUniformLocation(prog, "uAlpha");
    progFlat = link(VSF, FSF);
    fMVP = glGetUniformLocation(progFlat, "uMVP"); fTex = glGetUniformLocation(progFlat, "uTex"); fColor = glGetUniformLocation(progFlat, "uColor");

    // quad unitário (-0.5..0.5 em XY, olhando +Z)
    float q[] = {-.5f, -.5f, 0, 0, 0, 1, 0, 1, .5f, -.5f, 0, 0, 0, 1, 1, 1, .5f, .5f, 0, 0, 0, 1, 1, 0,
                 -.5f, -.5f, 0, 0, 0, 1, 0, 1, .5f, .5f, 0, 0, 0, 1, 1, 0, -.5f, .5f, 0, 0, 0, 1, 0, 0};
    glGenVertexArrays(1, &quadVao); glBindVertexArray(quadVao);
    glGenBuffers(1, &quadVbo); glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void *)0);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void *)24);
    glGenVertexArrays(1, &textVao); glBindVertexArray(textVao);
    glGenBuffers(1, &textVbo); glBindBuffer(GL_ARRAY_BUFFER, textVbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, (void *)0);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 20, (void *)12);
    glBindVertexArray(0);
    std::vector<unsigned char> fw;
    if (readAsset("font.bin", fw) && fw.size() >= 256) memcpy(fontW, fw.data(), 256);
    else memset(fontW, 40, 256);
    return true;
}

Texture *Gfx::tex(const std::string &name) {
    auto it = texs.find(name);
    if (it != texs.end()) return it->second;
    Texture *t = new Texture();
    std::vector<unsigned char> d;
    if (readAsset(("tex/" + name + ".tex").c_str(), d) && d.size() > 8 && !memcmp(d.data(), "BTEX", 4)) {
        t->w = d[4] | (d[5] << 8); t->h = d[6] | (d[7] << 8);
        glGenTextures(1, &t->id); glBindTexture(GL_TEXTURE_2D, t->id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d.data() + 8);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        bool font = name == "font";
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, font ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, font ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    } else {
        LOGE("textura ausente: %s", name.c_str());
        unsigned char px[4] = {255, 0, 255, 255};
        glGenTextures(1, &t->id); glBindTexture(GL_TEXTURE_2D, t->id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        t->w = t->h = 1;
    }
    texs[name] = t;
    return t;
}

Model *Gfx::model(const std::string &name) {
    auto it = models.find(name);
    if (it != models.end()) return it->second;
    Model *m = new Model();
    models[name] = m;
    std::vector<unsigned char> d;
    if (!readAsset(("models/" + name + ".bm").c_str(), d) || d.size() < 12 || memcmp(d.data(), "BMDL", 4)) {
        LOGE("modelo ausente: %s", name.c_str());
        return m;
    }
    size_t o = 8;
    auto u32 = [&]() { unsigned v; memcpy(&v, &d[o], 4); o += 4; return v; };
    unsigned n = u32();
    m->nodes.resize(n);
    for (unsigned i = 0; i < n; i++) {
        Node &nd = m->nodes[i];
        nd.name = std::string((const char *)&d[o], strnlen((const char *)&d[o], 32)); o += 32;
        int par; memcpy(&par, &d[o], 4); o += 4; nd.parent = par;
        memcpy(nd.local.m, &d[o], 64); o += 64;
        unsigned ns = u32();
        for (unsigned s = 0; s < ns; s++) {
            Section sec;
            std::string tn((const char *)&d[o], strnlen((const char *)&d[o], 32)); o += 32;
            memcpy(sec.color, &d[o], 16); o += 16;
            sec.flags = u32();
            unsigned vc = u32(), ic = u32();
            const unsigned char *vb = &d[o]; o += vc * 36;
            const unsigned char *ib = &d[o]; o += ic * 4;
            sec.tex = tex(tn);
            sec.count = ic;
            // esfera envolvente
            V3 lo(1e9, 1e9, 1e9), hi(-1e9, -1e9, -1e9);
            for (unsigned v = 0; v < vc; v++) {
                float p[3]; memcpy(p, vb + v * 36, 12);
                lo = V3(fminf(lo.x, p[0]), fminf(lo.y, p[1]), fminf(lo.z, p[2]));
                hi = V3(fmaxf(hi.x, p[0]), fmaxf(hi.y, p[1]), fmaxf(hi.z, p[2]));
            }
            sec.bcenter = (lo + hi) * 0.5f; sec.bradius = len(hi - lo) * 0.5f;
            glGenVertexArrays(1, &sec.vao); glBindVertexArray(sec.vao);
            glGenBuffers(1, &sec.vbo); glBindBuffer(GL_ARRAY_BUFFER, sec.vbo);
            glBufferData(GL_ARRAY_BUFFER, vc * 36, vb, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 36, (void *)0);
            glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 36, (void *)12);
            glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 36, (void *)24);
            glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, (void *)32);
            glGenBuffers(1, &sec.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sec.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, ic * 4, ib, GL_STATIC_DRAW);
            glBindVertexArray(0);
            nd.secs.push_back(sec);
        }
    }
    LOGI("modelo %s: %u nós", name.c_str(), n);
    return m;
}

void Gfx::begin(const DrawCtx &c, const Scene &s) {
    ctx = c; sc = s;
    ctx.vp = ctx.proj * ctx.view;
    glUseProgram(prog);
    glUniform3f(uSky, s.skyAmb.x, s.skyAmb.y, s.skyAmb.z);
    glUniform3f(uGround, s.groundAmb.x, s.groundAmb.y, s.groundAmb.z);
    glUniform3f(uSunDir, s.sunDir.x, s.sunDir.y, s.sunDir.z);
    glUniform3f(uSunCol, s.sunCol.x, s.sunCol.y, s.sunCol.z);
    glUniform3f(uFogCol, s.fogCol.x, s.fogCol.y, s.fogCol.z);
    glUniform1f(uFogDens, s.fogDens);
    glUniform3f(uCam, c.camPos.x, c.camPos.y, c.camPos.z);
    glUniform1f(uTime, s.time);
    float lp[16] = {0}, lc[12] = {0};
    for (int i = 0; i < s.nLights && i < 4; i++) {
        lp[i * 4] = s.lights[i].pos.x; lp[i * 4 + 1] = s.lights[i].pos.y; lp[i * 4 + 2] = s.lights[i].pos.z; lp[i * 4 + 3] = s.lights[i].radius;
        lc[i * 3] = s.lights[i].color.x; lc[i * 3 + 1] = s.lights[i].color.y; lc[i * 3 + 2] = s.lights[i].color.z;
    }
    for (int i = s.nLights; i < 4; i++) lp[i * 4 + 3] = 0.001f;
    glUniform4fv(uLPos, 4, lp);
    glUniform3fv(uLCol, 4, lc);
    glUniform1i(uTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
}

void Gfx::drawSection(const Section &s, const M4 &w, const V3 &tint, GLuint screenTex, float alpha, const V3 *tint2) {
    // culling por distância/direção
    V3 c = w.pt(s.bcenter);
    float sx = len(w.dir(V3(1, 0, 0)));
    float r = s.bradius * sx;
    V3 d = c - ctx.camPos;
    float dl = len(d);
    if (dl - r > ctx.cullDist) return;
    if (dot(d, ctx.camFwd) < -r - 0.5f) return;
    M4 mvp = ctx.vp * w;
    glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(uModel, 1, GL_FALSE, w.m);
    float col[4] = {s.color[0], s.color[1], s.color[2], s.color[3]};
    if (s.flags & F_TINT) {
        const V3 &t = tint2 ? *tint2 : tint;
        col[0] *= t.x; col[1] *= t.y; col[2] *= t.z;
    }
    glUniform4fv(uColor, 1, col);
    glUniform1i(uFlags, (int)s.flags);
    glUniform1f(uAlpha, alpha);
    glBindTexture(GL_TEXTURE_2D, (s.flags & F_SCREEN) && screenTex ? screenTex : (s.tex ? s.tex->id : 0));
    if (s.flags & F_DOUBLE) glDisable(GL_CULL_FACE);
    glBindVertexArray(s.vao);
    glDrawElements(GL_TRIANGLES, s.count, GL_UNSIGNED_INT, 0);
    if (s.flags & F_DOUBLE) glEnable(GL_CULL_FACE);
}

void Gfx::draw(const Model *m, const M4 &world, const V3 &tint, const Pose *pose, GLuint screenTex, float alpha, const V3 *tint2) {
    if (!m || m->nodes.empty()) return;
    size_t n = m->nodes.size();
    M4 stackW[64];
    M4 *ws = n <= 64 ? stackW : new M4[n];
    if (alpha < 1) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    for (size_t i = 0; i < n; i++) {
        const Node &nd = m->nodes[i];
        M4 local = nd.local;
        if (pose && i < pose->rot.size()) local = local * mTrans(pose->off[i]) * mRot(pose->rot[i]);
        ws[i] = nd.parent >= 0 ? ws[nd.parent] * local : world * local;
        for (auto &s : nd.secs) drawSection(s, ws[i], tint, screenTex, alpha, tint2);
    }
    if (alpha < 1) glDisable(GL_BLEND);
    if (ws != stackW) delete[] ws;
}

void Gfx::drawNode(const Model *m, int node, const M4 &world, const V3 &tint) {
    if (!m || node < 0) return;
    for (auto &s : m->nodes[node].secs) drawSection(s, world, tint, 0, 1, nullptr);
}

void Gfx::quad(const M4 &world, const float rgba[4], GLuint texId, bool additive) {
    glUseProgram(progFlat);
    M4 mvp = ctx.vp * world;
    glUniformMatrix4fv(fMVP, 1, GL_FALSE, mvp.m);
    glUniform4fv(fColor, 1, rgba);
    glUniform1i(fTex, 0);
    glBindTexture(GL_TEXTURE_2D, texId ? texId : whiteTex());
    glEnable(GL_BLEND);
    if (additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE); else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(prog);
}

float Gfx::textWidth(const char *utf8, float size) {
    std::string s = utf8ToLatin1(utf8);
    float w = 0, best = 0;
    for (unsigned char c : s) {
        if (c == '\n') { best = fmaxf(best, w); w = 0; continue; }
        w += fontW[c] / 64.f * size * 1.05f;
    }
    return fmaxf(best, w);
}

void Gfx::text(const M4 &world, const char *utf8, float size, const float rgba[4], int align, float maxW) {
    std::string s = utf8ToLatin1(utf8);
    // quebra de linha automática
    if (maxW > 0) {
        std::string out, word; float lw = 0;
        auto cw = [&](const std::string &w) { float x = 0; for (unsigned char c : w) x += fontW[c] / 64.f * size * 1.05f; return x; };
        for (size_t i = 0; i <= s.size(); i++) {
            char c = i < s.size() ? s[i] : ' ';
            if (c == ' ' || c == '\n') {
                float ww = cw(word);
                if (lw + ww > maxW && lw > 0) { out += '\n'; lw = 0; }
                out += word; lw += ww;
                if (c == '\n') { out += '\n'; lw = 0; }
                else if (i < s.size()) { out += ' '; lw += cw(" "); }
                word.clear();
            } else word += c;
        }
        s = out;
    }
    std::vector<float> v;
    v.reserve(s.size() * 30);
    float y = 0;
    size_t lineStart = 0;
    auto lineW = [&](size_t from) { float w = 0; for (size_t i = from; i < s.size() && s[i] != '\n'; i++) w += fontW[(unsigned char)s[i]] / 64.f * size * 1.05f; return w; };
    float x = align == 1 ? -lineW(0) / 2 : (align == 2 ? -lineW(0) : 0);
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = s[i];
        if (c == '\n') {
            y -= size * 1.25f; lineStart = i + 1;
            float lw = lineW(lineStart);
            x = align == 1 ? -lw / 2 : (align == 2 ? -lw : 0);
            continue;
        }
        float u0 = (c % 16) / 16.f, v0 = (c / 16) / 16.f, du = 1 / 16.f;
        float q = size * 64.f / 52.f;  // célula inteira
        float x0 = x - size * 0.08f, x1 = x0 + q, y1 = y + size * 0.95f, y0 = y1 - q;
        float quad[] = {x0, y0, 0, u0, v0 + du, x1, y0, 0, u0 + du, v0 + du, x1, y1, 0, u0 + du, v0,
                        x0, y0, 0, u0, v0 + du, x1, y1, 0, u0 + du, v0, x0, y1, 0, u0, v0};
        v.insert(v.end(), quad, quad + 30);
        x += fontW[c] / 64.f * size * 1.05f;
    }
    if (v.empty()) return;
    glUseProgram(progFlat);
    M4 mvp = ctx.vp * world;
    glUniformMatrix4fv(fMVP, 1, GL_FALSE, mvp.m);
    glUniform4fv(fColor, 1, rgba);
    glBindTexture(GL_TEXTURE_2D, tex("font")->id);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(textVao);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 5));
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(prog);
}
