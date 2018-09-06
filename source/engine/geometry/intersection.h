#pragma once
#include <glm/glm.hpp>
#include "BBox.h"

class BBox;

namespace intersection
{
    /**
    * Segment is given by points sa and sb.
    * Cylinder is specified by points p, q and radius r.
    */
    bool segmentCylinder(const glm::vec3& sa, const glm::vec3& sb, 
        const glm::vec3& p, const glm::vec3& q, float r, float& t);

    /**
    * Segment is given by points sa and sb.
    * Capsule is specified by points p, q and radius r
    */
    bool segmentCapsule(const glm::vec3& sa, const glm::vec3& sb, 
        const glm::vec3& p, const glm::vec3& q, float r, float& t);

    /**
    * Segment is given by points sa and sb.
    * Capsule is specified by points p, q and radius r
    */
    bool segmentSphere(const glm::vec3& sa, const glm::vec3& sb, const glm::vec3& pos, float r, float& t);

    /**
    * Segment is given by points sa and sb.
    */
    bool segmentAABB(const glm::vec3& sa, const glm::vec3& sb, const BBox& aabb, float& t);

    /**
    * Ray is given by origin o and direction d.
    */
    bool rayAABB(const glm::vec3& o, const glm::vec3& d, const BBox& aabb, float& t);

    /**
    * Ray is given by origin o and direction d.
    * p is the computed intersection point.
    */
    bool rayAABB(const glm::vec3& o, const glm::vec3& d, const BBox& aabb, float& t, glm::vec3& p);

    /**
    * Sphere is given by points center c, radius r and moves in direction d.
    */
    bool movingSphereAABB(const glm::vec3& c, float r, const glm::vec3& d, const BBox& b, float& t);

    /**
    * @brief   BBox/Triangle overlap test. Algorithm from "Fast Parallel Surface and Solid Voxelization on GPUs" by Michael Schwarz and Hans-Peter Seidel.
    */
    bool bboxTriangle(const BBox& b, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

    bool intersection(const glm::vec3& c0, float r0, const glm::vec3& c1, float r1);

    class TriangleBBox
    {
    public:
        void setTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
        void setBBoxScale(const glm::vec3& scale);
        bool test(const BBox& b);

        glm::vec3 v0, v1, v2;
        glm::vec3 n;
        BBox triangleBBox;
        glm::vec3 dp;

        glm::vec2 n_yz_e0;
        glm::vec2 n_yz_e1;
        glm::vec2 n_yz_e2;

        glm::vec2 n_zx_e0;
        glm::vec2 n_zx_e1;
        glm::vec2 n_zx_e2;

        glm::vec2 n_xy_e0;
        glm::vec2 n_xy_e1;
        glm::vec2 n_xy_e2;

        float d1;
        float d2;

        float d_xy_e0;
        float d_xy_e1;
        float d_xy_e2;

        float d_zx_e0;
        float d_zx_e1;
        float d_zx_e2;

        float d_yz_e0;
        float d_yz_e1;
        float d_yz_e2;
    };
}
