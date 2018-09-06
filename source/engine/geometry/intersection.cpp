#include "intersection.h"
#include <algorithm>
#include "BBox.h"
#include <engine/util/math.h>

// Algorithm from Real-Time Collision Detection by Christer Ericson
bool intersection::segmentCylinder(const glm::vec3& sa, const glm::vec3& sb, 
    const glm::vec3& p, const glm::vec3& q, float r, float& t)
{
    glm::vec3 d = q - p;
    glm::vec3 m = sa - p;
    glm::vec3 n = sb - sa;
    float md = glm::dot(m, d);
    float nd = glm::dot(n, d);
    float dd = glm::dot(d, d);

    // Test if segment fully outside of either endcap of cylinder
    if (md < 0.0f && md + nd < 0.0f)
        return false; // Segment outside p side of cylinder

    if (md > dd && md + nd > dd)
        return false; // Segment outside q side of cylinder

    float nn = glm::dot(n, n);
    float mn = glm::dot(m, n);
    float a = dd * nn - nd * nd;
    float k = glm::dot(m, m) - r * r;
    float c = dd * k - md * md;

    if (std::abs(a) < math::EPSILON)
    {
        // Segment runs parallel to cylinder axis
        if (c > 0.0f)
            return false; // a and thus the segment lie outside the cylinder

        // Now known that segment intersects cylinder; figure out how it intersects
        if (md < 0.0f)
            t = -mn / nn; // Intersect segment against p endcap
        else if (md > dd)
            t = (nd - mn) / nn; // Intersect segment against q endcap
        else t = 0.0f; // a lies inside cylinder

        return true;
    }

    float b = dd * mn - nd * md;
    float discr = b * b - a * c;

    if (discr < 0.0f)
        return false; // No real roots -> no intersection

    t = (-b - std::sqrtf(discr)) / a;

    if (md + t * nd < 0.0f)
    {
        // Intersection outside cylinder on p side
        if (nd <= 0.0f)
            return false; // Segment pointing away from endcap

        t = -md / nd;
        // Keep intersection if dot(S(t) - p, S(t) - p) <= r^2
        return k + t * (2.0f * mn + t * nn) <= 0.0f;
    }

    if (md + t * nd > dd)
    {
        // Intersection outside cylinder on q side
        if (nd >= 0.0f)
            return false; // Segment pointing away from endcap

        t = (dd - md) / nd;
        // Keep intersection if dot(S(t) - q, S(t) - q) <= r^2
        return k + dd - 2.0f * md + t * (2.0f * (mn - nd) + t * nn) <= 0.0f;
    }

    // Segment intersects cylinder between the endcaps if t lies within segment
    return t >= 0.0f && t <= 1.0f;
}

// Algorithm based on Real-Time Collision Detection by Christer Ericson
bool intersection::segmentCapsule(const glm::vec3& sa, const glm::vec3& sb, const glm::vec3& p, const glm::vec3& q, float r, float& t)
{
    glm::vec3 d = q - p;
    glm::vec3 m = sa - p;
    glm::vec3 n = sb - sa;
    float md = glm::dot(m, d);
    float nd = glm::dot(n, d);
    float dd = glm::dot(d, d);

    // Test if segment fully outside of either endcap of capsule
    if (md < 0.0f && md + nd < 0.0f)
        return false; // Segment outside p side of capsule

    if (md > dd && md + nd > dd)
        return false; // Segment outside q side of capsule

    float nn = glm::dot(n, n);
    float mn = glm::dot(m, n);
    float a = dd * nn - nd * nd;
    float k = glm::dot(m, m) - r * r;
    float c = dd * k - md * md;

    if (std::abs(a) < math::EPSILON)
    {
        // Segment runs parallel to capsule axis
        if (c > 0.0f)
            return false; // a and thus the segment lie outside the capsule

        // Now known that segment intersects capsule; figure out how it intersects
        if (md < 0.0f)
            t = (-mn - std::sqrtf(mn * mn - nn * k)) / nn; // Intersect segment against p endcap
        else if (md > dd)
            t = (nd - mn - std::sqrtf(mn * mn - nn * k)) / nn; // Intersect segment against q endcap
        else t = 0.0f; // a lies inside capsule

        return true;
    }

    float b = dd * mn - nd * md;
    float discr = b * b - a * c;

    if (discr < 0.0f)
        return false; // No real roots -> no intersection

    t = (-b - std::sqrtf(discr)) / a;

    // dot(S(t) - P, d) < 0
    if (md + t * nd < 0.0f)
    {
        // Sphere/Segment intersection at endcap P
        float sdiscr = mn * mn - nn * k;

        if (sdiscr > 0.0f)
        {
            t = (-mn - std::sqrtf(sdiscr)) / nn;
            return t >= 0.0f && t <= 1.0f;
        }

        return false;
    }

    // dot(S(t) - P, d) > dd
    if (md + t * nd > dd)
    {
        // Sphere/Segment intersection at endcap Q
        glm::vec3 sm = sa - q;
        float smn = glm::dot(sm, n);
        float sk = glm::dot(sm, sm) - r * r;
        float sdiscr = smn * smn - nn * sk;

        if (sdiscr > 0.0f)
        {
            t = (-smn - std::sqrtf(sdiscr)) / nn;
            return t >= 0.0f && t <= 1.0f;
        }

        return false;
    }

    // Segment intersects capsule between the endcaps if t lies within segment
    return t >= 0.0f && t <= 1.0f;
}

bool intersection::segmentSphere(const glm::vec3& sa, const glm::vec3& sb, const glm::vec3& pos, float r, float& t)
{
    glm::vec3 m = sa - pos;
    glm::vec3 n = sb - sa;
    float a = glm::dot(n, n);
    float b = glm::dot(n, m);
    float c = glm::dot(m, m) - r * r;

    float d = b * b - a * c;

    if (d > 0.0f)
    {
        t = (-b - std::sqrtf(d)) / a;
        return t >= 0.0f && t <= 1.0f;
    }

    return false;
}

bool intersection::segmentAABB(const glm::vec3& sa, const glm::vec3& sb, const BBox& aabb, float& t)
{
    return rayAABB(sa, sb - sa, aabb, t) && t > 0.0f && t < 1.0f;
}

bool intersection::rayAABB(const glm::vec3& o, const glm::vec3& d, const BBox& aabb, float& t)
{
    glm::vec3 tMin = (aabb.min() - o) / d;
    glm::vec3 tMax = (aabb.max() - o) / d;
    glm::vec3 t1 = min(tMin, tMax);
    glm::vec3 t2 = max(tMin, tMax);
    t = glm::max(glm::max(t1.x, t1.y), t1.z); // near hit
    float farT = glm::min(glm::min(t2.x, t2.y), t2.z); // far hit

    return t <= farT;
}

bool intersection::rayAABB(const glm::vec3& o, const glm::vec3& d, const BBox& aabb, float& t, glm::vec3& p)
{
    if (rayAABB(o, d, aabb, t))
    {
        p = o + d * t;
        return true;
    }

    return false;
}

// Support function that returns the AABB vertex with index n
glm::vec3 Corner(const BBox& b, int n)
{
    glm::vec3 p;
    p.x = ((n & 1) ? b.max().x : b.min().x);
    p.y = ((n & 2) ? b.max().y : b.min().y);
    p.z = ((n & 4) ? b.max().z : b.min().z);
    return p;
}

// Algorithm from Real-Time Collision Detection by Christer Ericson
bool intersection::movingSphereAABB(const glm::vec3& c, float r, const glm::vec3& d, const BBox& b, float& t)
{
    // Compute the AABB resulting from expanding b by sphere radius r
    BBox e = b;
    e.expand(r);

    // Intersect ray against expanded AABB e. Exit with no intersection if ray
    // misses e, else get intersection point p and time t as result
    glm::vec3 p;
    if (!rayAABB(c, d, e, t, p) || t > 1.0f)
        return false;

    // Compute which min and max faces of b the intersection point p lies
    // outside of. Note, u and v cannot have the same bits set and
    // they must have at least one bit set among them
    int u = 0, v = 0;
    if (p.x < b.min().x) u |= 1;
    if (p.x > b.max().x) v |= 1;
    if (p.y < b.min().y) u |= 2;
    if (p.y > b.max().y) v |= 2;
    if (p.z < b.min().z) u |= 4;
    if (p.z > b.max().z) v |= 4;

    // Or all set bits together into a bit mask (note: here u + v == u | v)
    int m = u + v;

    // Define line segment [c, c+d] specified by the sphere movement
    glm::vec3 sa = c;
    glm::vec3 sb = c + d;

    // If all 3 bits set (m == 7) then p is in a vertex region
    if (m == 7) 
    {
        // Must now intersect segment [c, c+d] against the capsules of the three
        // edges meeting at the vertex and return the best time, if one or more hit
        float tmin = FLT_MAX;
        if (segmentCapsule(sa, sb, Corner(b, v), Corner(b, v ^ 1), r, t))
            tmin = std::min(t, tmin);
        if (segmentCapsule(sa, sb, Corner(b, v), Corner(b, v ^ 2), r, t))
            tmin = std::min(t, tmin);
        if (segmentCapsule(sa, sb, Corner(b, v), Corner(b, v ^ 4), r, t))
            tmin = std::min(t, tmin);

        if (tmin == FLT_MAX) 
            return false; // No intersection

        t = tmin;
        return true; // Intersection at time t == tmin
    }

    // If only one bit set in m, then p is in a face region
    if ((m & (m - 1)) == 0) 
    {
        // Do nothing. Time t from intersection with
        // expanded box is correct intersection time
        return true;
    }

    // p is in an edge region. Intersect against the capsule at the edge
    return segmentCapsule(sa, sb, Corner(b, u ^ 7), Corner(b, v), r, t);
}

bool intersection::bboxTriangle(const BBox& b, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
{
    // BBox/BBox test
    BBox triangleBBox;
    triangleBBox.unite({v0, v1, v2});

    if (!b.overlaps(triangleBBox))
        return false;

    // Triangle plane/BBox test
    glm::vec3 e0 = v1 - v0;
    glm::vec3 e1 = v2 - v1;
    glm::vec3 e2 = v0 - v2;

    glm::vec3 n = glm::normalize(glm::cross(e0, e1));
    glm::vec3 dp = b.scale();
    glm::vec3 c = glm::vec3(
        n.x > 0.0f ? dp.x : 0.0f,
        n.y > 0.0f ? dp.y : 0.0f,
        n.z > 0.0f ? dp.z : 0.0f);

    float d1 = glm::dot(n, c - v0);
    float d2 = glm::dot(n, (dp - c) - v0);

    if ((glm::dot(n, b.min()) + d1) * (glm::dot(n, b.min()) + d2) > 0.0f)
        return false;

    // 2D Projections of Triangle/BBox test

    // XY Plane
    glm::vec2 n_xy_e0(-e0.y, e0.x);
    glm::vec2 n_xy_e1(-e1.y, e1.x);
    glm::vec2 n_xy_e2(-e2.y, e2.x);

    if (n.z < 0.0f)
    {
        n_xy_e0 *= -1.0f;
        n_xy_e1 *= -1.0f;
        n_xy_e2 *= -1.0f;
    }

    float d_xy_e0 = -glm::dot(n_xy_e0, glm::vec2(v0.x, v0.y)) + std::max(0.0f, dp.x * n_xy_e0[0]) + std::max(0.0f, dp.y * n_xy_e0[1]);
    float d_xy_e1 = -glm::dot(n_xy_e1, glm::vec2(v1.x, v1.y)) + std::max(0.0f, dp.x * n_xy_e1[0]) + std::max(0.0f, dp.y * n_xy_e1[1]);
    float d_xy_e2 = -glm::dot(n_xy_e2, glm::vec2(v2.x, v2.y)) + std::max(0.0f, dp.x * n_xy_e2[0]) + std::max(0.0f, dp.y * n_xy_e2[1]);

    glm::vec2 p_xy(b.min().x, b.min().y);

    if ((glm::dot(n_xy_e0, p_xy) + d_xy_e0 < 0.0f) ||
        (glm::dot(n_xy_e1, p_xy) + d_xy_e1 < 0.0f) ||
        (glm::dot(n_xy_e2, p_xy) + d_xy_e2 < 0.0f))
        return false;

    // ZX Plane
    glm::vec2 n_zx_e0(-e0.x, e0.z);
    glm::vec2 n_zx_e1(-e1.x, e1.z);
    glm::vec2 n_zx_e2(-e2.x, e2.z);

    if (n.y < 0.0f)
    {
        n_zx_e0 *= -1.0f;
        n_zx_e1 *= -1.0f;
        n_zx_e2 *= -1.0f;
    }

    float d_zx_e0 = -glm::dot(n_zx_e0, glm::vec2(v0.z, v0.x)) + std::max(0.0f, dp.z * n_zx_e0[0]) + std::max(0.0f, dp.x * n_zx_e0[1]);
    float d_zx_e1 = -glm::dot(n_zx_e1, glm::vec2(v1.z, v1.x)) + std::max(0.0f, dp.z * n_zx_e1[0]) + std::max(0.0f, dp.x * n_zx_e1[1]);
    float d_zx_e2 = -glm::dot(n_zx_e2, glm::vec2(v2.z, v2.x)) + std::max(0.0f, dp.z * n_zx_e2[0]) + std::max(0.0f, dp.x * n_zx_e2[1]);

    glm::vec2 p_zx(b.min().z, b.min().x);

    if ((glm::dot(n_zx_e0, p_zx) + d_zx_e0 < 0.0f) ||
        (glm::dot(n_zx_e1, p_zx) + d_zx_e1 < 0.0f) ||
        (glm::dot(n_zx_e2, p_zx) + d_zx_e2 < 0.0f))
        return false;

    // YZ Plane
    glm::vec2 n_yz_e0(-e0.z, e0.y);
    glm::vec2 n_yz_e1(-e1.z, e1.y);
    glm::vec2 n_yz_e2(-e2.z, e2.y);

    if (n.x < 0.0f)
    {
        n_yz_e0 *= -1.0f;
        n_yz_e1 *= -1.0f;
        n_yz_e2 *= -1.0f;
    }

    float d_yz_e0 = -glm::dot(n_yz_e0, glm::vec2(v0.y, v0.z)) + std::max(0.0f, dp.y * n_yz_e0[0]) + std::max(0.0f, dp.z * n_yz_e0[1]);
    float d_yz_e1 = -glm::dot(n_yz_e1, glm::vec2(v1.y, v1.z)) + std::max(0.0f, dp.y * n_yz_e1[0]) + std::max(0.0f, dp.z * n_yz_e1[1]);
    float d_yz_e2 = -glm::dot(n_yz_e2, glm::vec2(v2.y, v2.z)) + std::max(0.0f, dp.y * n_yz_e2[0]) + std::max(0.0f, dp.z * n_yz_e2[1]);

    glm::vec2 p_yz(b.min().y, b.min().z);

    if ((glm::dot(n_yz_e0, p_yz) + d_yz_e0 < 0.0f) ||
        (glm::dot(n_yz_e1, p_yz) + d_yz_e1 < 0.0f) ||
        (glm::dot(n_yz_e2, p_yz) + d_yz_e2 < 0.0f))
        return false;

    return true;
}

bool intersection::intersection(const glm::vec3& c0, float r0, const glm::vec3& c1, float r1)
{
    return glm::length2(c0 - c1) <= (r0 + r1) * (r0 + r1);
}

void intersection::TriangleBBox::setTriangle(const glm::vec3& pv0, const glm::vec3& pv1, const glm::vec3& pv2)
{
    this->v0 = pv0;
    this->v1 = pv1;
    this->v2 = pv2;

    // BBox/BBox test
    triangleBBox = BBox();
    triangleBBox.unite({v0, v1, v2});

    // Triangle plane/BBox test
    glm::vec3 e0 = v1 - v0;
    glm::vec3 e1 = v2 - v1;
    glm::vec3 e2 = v0 - v2;

    n = glm::normalize(glm::cross(e0, e1));

    // 2D Projections of Triangle/BBox test

    // XY Plane
    n_xy_e0 = glm::vec2(-e0.y, e0.x);
    n_xy_e1 = glm::vec2(-e1.y, e1.x);
    n_xy_e2 = glm::vec2(-e2.y, e2.x);

    if (n.z < 0.0f)
    {
        n_xy_e0 *= -1.0f;
        n_xy_e1 *= -1.0f;
        n_xy_e2 *= -1.0f;
    }

    // ZX Plane
    n_zx_e0 = glm::vec2(-e0.x, e0.z);
    n_zx_e1 = glm::vec2(-e1.x, e1.z);
    n_zx_e2 = glm::vec2(-e2.x, e2.z);

    if (n.y < 0.0f)
    {
        n_zx_e0 *= -1.0f;
        n_zx_e1 *= -1.0f;
        n_zx_e2 *= -1.0f;
    }

    // YZ Plane
    n_yz_e0 = glm::vec2(-e0.z, e0.y);
    n_yz_e1 = glm::vec2(-e1.z, e1.y);
    n_yz_e2 = glm::vec2(-e2.z, e2.y);

    if (n.x < 0.0f)
    {
        n_yz_e0 *= -1.0f;
        n_yz_e1 *= -1.0f;
        n_yz_e2 *= -1.0f;
    }
}

void intersection::TriangleBBox::setBBoxScale(const glm::vec3& scale)
{
    dp = scale;
    glm::vec3 c = glm::vec3(
        n.x > 0.0f ? dp.x : 0.0f,
        n.y > 0.0f ? dp.y : 0.0f,
        n.z > 0.0f ? dp.z : 0.0f);

    d1 = glm::dot(n, c - v0);
    d2 = glm::dot(n, (dp - c) - v0);

    d_xy_e0 = -glm::dot(n_xy_e0, glm::vec2(v0.x, v0.y)) + std::max(0.0f, dp.x * n_xy_e0[0]) + std::max(0.0f, dp.y * n_xy_e0[1]);
    d_xy_e1 = -glm::dot(n_xy_e1, glm::vec2(v1.x, v1.y)) + std::max(0.0f, dp.x * n_xy_e1[0]) + std::max(0.0f, dp.y * n_xy_e1[1]);
    d_xy_e2 = -glm::dot(n_xy_e2, glm::vec2(v2.x, v2.y)) + std::max(0.0f, dp.x * n_xy_e2[0]) + std::max(0.0f, dp.y * n_xy_e2[1]);

    d_zx_e0 = -glm::dot(n_zx_e0, glm::vec2(v0.z, v0.x)) + std::max(0.0f, dp.z * n_zx_e0[0]) + std::max(0.0f, dp.x * n_zx_e0[1]);
    d_zx_e1 = -glm::dot(n_zx_e1, glm::vec2(v1.z, v1.x)) + std::max(0.0f, dp.z * n_zx_e1[0]) + std::max(0.0f, dp.x * n_zx_e1[1]);
    d_zx_e2 = -glm::dot(n_zx_e2, glm::vec2(v2.z, v2.x)) + std::max(0.0f, dp.z * n_zx_e2[0]) + std::max(0.0f, dp.x * n_zx_e2[1]);

    d_yz_e0 = -glm::dot(n_yz_e0, glm::vec2(v0.y, v0.z)) + std::max(0.0f, dp.y * n_yz_e0[0]) + std::max(0.0f, dp.z * n_yz_e0[1]);
    d_yz_e1 = -glm::dot(n_yz_e1, glm::vec2(v1.y, v1.z)) + std::max(0.0f, dp.y * n_yz_e1[0]) + std::max(0.0f, dp.z * n_yz_e1[1]);
    d_yz_e2 = -glm::dot(n_yz_e2, glm::vec2(v2.y, v2.z)) + std::max(0.0f, dp.y * n_yz_e2[0]) + std::max(0.0f, dp.z * n_yz_e2[1]);
}

bool intersection::TriangleBBox::test(const BBox& b)
{
    if (!b.overlaps(triangleBBox))
        return false;

    if ((glm::dot(n, b.min()) + d1) * (glm::dot(n, b.min()) + d2) > 0.0f)
        return false;

    glm::vec2 p_xy(b.min().x, b.min().y);

    if ((glm::dot(n_xy_e0, p_xy) + d_xy_e0 < 0.0f) ||
        (glm::dot(n_xy_e1, p_xy) + d_xy_e1 < 0.0f) ||
        (glm::dot(n_xy_e2, p_xy) + d_xy_e2 < 0.0f))
        return false;

    glm::vec2 p_zx(b.min().z, b.min().x);

    if ((glm::dot(n_zx_e0, p_zx) + d_zx_e0 < 0.0f) ||
        (glm::dot(n_zx_e1, p_zx) + d_zx_e1 < 0.0f) ||
        (glm::dot(n_zx_e2, p_zx) + d_zx_e2 < 0.0f))
        return false;

    glm::vec2 p_yz(b.min().y, b.min().z);

    if ((glm::dot(n_yz_e0, p_yz) + d_yz_e0 < 0.0f) ||
        (glm::dot(n_yz_e1, p_yz) + d_yz_e1 < 0.0f) ||
        (glm::dot(n_yz_e2, p_yz) + d_yz_e2 < 0.0f))
        return false;

    return true;
}
