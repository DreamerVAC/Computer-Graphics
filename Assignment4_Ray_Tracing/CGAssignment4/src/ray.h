#ifndef RAY_H
#define RAY_H
#include "vec3.h"

class ray {
public:
	ray() {}
	ray(const point3& origin, const vec3& direction, double time = 0.0)
		:orig(origin), dir(direction),tm(time)
	{}
	point3 origin() const { return orig; }
	vec3 direction() const { return dir; }
	double time() const { return tm; }
	point3 at(double t) const {
		return orig + t * dir;
	}
public:
	point3 orig;
	vec3 dir;
	double tm;
};

double hit_sphere(const point3& center, double radius, const ray& r) {
	vec3 oc = r.origin() - center;
	auto a = r.direction().length_squared();
	auto half_b = dot(oc, r.direction());
	auto c = dot(oc, oc) - radius * radius;
	auto discriminant = half_b * half_b - a * c;
	return discriminant < 0 ? -1.0 : (-half_b - sqrt(discriminant)) / a;
}




#endif // !RAY_H

