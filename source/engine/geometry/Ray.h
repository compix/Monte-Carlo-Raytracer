#pragma once
#include <glm/glm.hpp>
#include "OBBox.h"

class BBox;
class Transform;
class Mesh;

struct Ray
{
    Ray() { }

    Ray(const glm::vec3& origin, const glm::vec3& direction)
        : origin(origin), direction(direction) { }

    bool intersects(const OBBox& obb, float& t) const;
    /**
    * The output t is a vector with the first component being tNear(first hit) and the second tFar(second hit)
    * If there is no intersection then tNear > tFar
    * Note: If the AABB is hit in the opposite direction of the ray from the origin then tNear < 0
    */
    bool intersects(const BBox& aabb, glm::vec2& t) const;

    bool intersectsCapsule(const glm::vec3& p0, const glm::vec3& p1, float radius, float& t);

    bool intersectsSphere(const glm::vec3& pos, float radius, float& t);

    /**
    * Cylinder is specified by points p, q and radius r
    */
    bool intersectsCylinder(const glm::vec3& p, const glm::vec3& q, float r, float& t, float maxDistance);

    /**
    * Performs an intersection test between this ray and a given triangle defined by
    * points (p0, p1, p2). If there is no intersection false is returned and (uv, t) will be undefined
    * otherwise true is returned and (uv, t) set to the correct values.
    */
    bool intersectsTriangle(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, glm::vec2& uv, float& t) const;

    bool intersects(const Mesh& mesh, const Transform& transform, glm::vec3& normal, float& t) const = delete;

    bool intersectsSphere(const glm::vec3& pos, const float& radius, float& t) const;

    glm::vec3 origin;
    glm::vec3 direction;
};
