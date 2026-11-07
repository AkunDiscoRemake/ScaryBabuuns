// Matemática 3D mínima (Y para cima, frente = -Z, matrizes coluna-major como OpenGL)
#pragma once
#include <math.h>
#include <string.h>

static const float PI = 3.14159265f;

struct V3 {
    float x = 0, y = 0, z = 0;
    V3() {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    V3 operator+(const V3 &o) const { return V3(x + o.x, y + o.y, z + o.z); }
    V3 operator-(const V3 &o) const { return V3(x - o.x, y - o.y, z - o.z); }
    V3 operator*(float s) const { return V3(x * s, y * s, z * s); }
    V3 operator/(float s) const { return V3(x / s, y / s, z / s); }
    V3 operator-() const { return V3(-x, -y, -z); }
    V3 &operator+=(const V3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3 &operator-=(const V3 &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3 &operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};
inline float dot(const V3 &a, const V3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3 &a, const V3 &b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float len(const V3 &a) { return sqrtf(dot(a, a)); }
inline float len2(const V3 &a) { return dot(a, a); }
inline V3 norm(const V3 &a) { float l = len(a); return l > 1e-8f ? a / l : V3(0, 0, 0); }
inline V3 lerp(const V3 &a, const V3 &b, float t) { return a + (b - a) * t; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float smoothstep(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
inline float hlen(const V3 &a) { return sqrtf(a.x * a.x + a.z * a.z); }
inline float wrapAngle(float a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }

struct Q {
    float x = 0, y = 0, z = 0, w = 1;
    Q() {}
    Q(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
    Q operator*(const Q &o) const {
        return Q(w * o.x + x * o.w + y * o.z - z * o.y, w * o.y - x * o.z + y * o.w + z * o.x,
                 w * o.z + x * o.y - y * o.x + z * o.w, w * o.w - x * o.x - y * o.y - z * o.z);
    }
    V3 rot(const V3 &v) const {
        V3 u(x, y, z);
        V3 t = cross(u, v) * 2.f;
        return v + t * w + cross(u, t);
    }
    Q conj() const { return Q(-x, -y, -z, w); }
};
inline Q qAxis(const V3 &axis, float ang) { V3 a = norm(axis); float s = sinf(ang * .5f); return Q(a.x * s, a.y * s, a.z * s, cosf(ang * .5f)); }
inline Q qYaw(float a) { return qAxis(V3(0, 1, 0), a); }
inline Q qNorm(const Q &q) { float l = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w); return Q(q.x / l, q.y / l, q.z / l, q.w / l); }
inline Q qSlerp(Q a, Q b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0) { b = Q(-b.x, -b.y, -b.z, -b.w); d = -d; }
    if (d > 0.9995f) return qNorm(Q(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t), lerpf(a.w, b.w, t)));
    float th = acosf(d), s = sinf(th);
    float wa = sinf((1 - t) * th) / s, wb = sinf(t * th) / s;
    return Q(a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb);
}
// yaw (rotação em torno de Y) de uma orientação, a partir do vetor frente
inline float qGetYaw(const Q &q) { V3 f = q.rot(V3(0, 0, -1)); return atan2f(-f.x, -f.z); }
// rotação mínima que leva a para b
inline Q qFromTo(const V3 &a, const V3 &b) {
    V3 c = cross(a, b); float d = dot(a, b);
    if (d < -0.9999f) { V3 ax = fabsf(a.x) < 0.9f ? cross(a, V3(1, 0, 0)) : cross(a, V3(0, 1, 0)); return qAxis(ax, PI); }
    return qNorm(Q(c.x, c.y, c.z, 1 + d));
}

struct M4 {
    float m[16];  // coluna-major: m[col*4+row]
    M4() { memset(m, 0, sizeof(m)); m[0] = m[5] = m[10] = m[15] = 1; }
    float &at(int r, int c) { return m[c * 4 + r]; }
    float at(int r, int c) const { return m[c * 4 + r]; }
    M4 operator*(const M4 &o) const {
        M4 r;
        for (int c = 0; c < 4; c++)
            for (int rr = 0; rr < 4; rr++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += at(rr, k) * o.at(k, c);
                r.at(rr, c) = s;
            }
        return r;
    }
    V3 pt(const V3 &v) const { return V3(at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z + at(0, 3), at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z + at(1, 3), at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z + at(2, 3)); }
    V3 dir(const V3 &v) const { return V3(at(0, 0) * v.x + at(0, 1) * v.y + at(0, 2) * v.z, at(1, 0) * v.x + at(1, 1) * v.y + at(1, 2) * v.z, at(2, 0) * v.x + at(2, 1) * v.y + at(2, 2) * v.z); }
    V3 pos() const { return V3(m[12], m[13], m[14]); }
};
inline M4 mTrans(const V3 &t) { M4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
inline M4 mScale(const V3 &s) { M4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
inline M4 mRot(const Q &q) {
    M4 r;
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    r.at(0, 0) = 1 - 2 * (yy + zz); r.at(0, 1) = 2 * (xy - wz); r.at(0, 2) = 2 * (xz + wy);
    r.at(1, 0) = 2 * (xy + wz); r.at(1, 1) = 1 - 2 * (xx + zz); r.at(1, 2) = 2 * (yz - wx);
    r.at(2, 0) = 2 * (xz - wy); r.at(2, 1) = 2 * (yz + wx); r.at(2, 2) = 1 - 2 * (xx + yy);
    return r;
}
inline M4 mTRS(const V3 &t, const Q &q, const V3 &s = V3(1, 1, 1)) { return mTrans(t) * mRot(q) * mScale(s); }
// inversa de transformação rígida (rotação + translação)
inline M4 mInvRigid(const M4 &a) {
    M4 r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r.at(i, j) = a.at(j, i);
    V3 t = a.pos();
    r.at(0, 3) = -(r.at(0, 0) * t.x + r.at(0, 1) * t.y + r.at(0, 2) * t.z);
    r.at(1, 3) = -(r.at(1, 0) * t.x + r.at(1, 1) * t.y + r.at(1, 2) * t.z);
    r.at(2, 3) = -(r.at(2, 0) * t.x + r.at(2, 1) * t.y + r.at(2, 2) * t.z);
    return r;
}
inline M4 mPersp(float fovY, float aspect, float n, float f) {
    M4 r; float t = 1.f / tanf(fovY * .5f);
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f); r.m[15] = 0;
    return r;
}
// matriz de visão olhando de 'eye' com orientação q
inline M4 mView(const V3 &eye, const Q &q) { return mInvRigid(mTRS(eye, q)); }

// RNG simples
struct Rng {
    unsigned s;
    explicit Rng(unsigned seed = 1234) : s(seed) {}
    unsigned next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (next() & 0xFFFFFF) / float(0x1000000); }
    float r(float a, float b) { return a + f() * (b - a); }
};
