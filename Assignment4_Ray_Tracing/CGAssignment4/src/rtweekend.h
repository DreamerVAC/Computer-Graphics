#ifndef RTWEEKEND_H
#define RTWEEKEND_H
#include <cmath>
#include <limits>
#include <memory>
#include <cstdlib>

using std::shared_ptr;
using std::make_shared;
using std::sqrt;

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.14115926535897932385;

inline double degrees_to_radians(double degrees) {
	return degrees * pi / 180.0;
}
inline double clamp(double x, double min, double max) {
	return x<min ? min : x>max ? max : x;
}

inline double random_double() {
	return rand() / (RAND_MAX + 1.0);
}
inline double random_double(double min, double max) {
	return min + (max - min) * random_double();
}

inline int random_int(int min, int max) {
	// Returns a random integer in [min,max].
	return static_cast<int>(random_double(min, max + 1));
}
//inline double random_double() {
//	static std::uniform_real_distribution<double> distribution(0.0, 1.0);
//	static std::mt19937 generator;
//	return distribution(generator);
//}

//#include "ray.h"
//#include "vec3.h"

#endif
