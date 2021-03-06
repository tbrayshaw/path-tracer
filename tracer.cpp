#include <stdio.h>
#include <tchar.h>
#include <math.h>
#include <stdlib.h>

double M_PI = 3.1415926535;
double M_1_PI = 1.0 / M_PI;

double erand48(unsigned short xsubi[3]) {
	return (double)rand() / (double)RAND_MAX;
}

struct Vector {
	double x, y, z; // position or colour (r,g,b)

	Vector(double x_ = 0, double y_ = 0, double z_ = 0) { x = x_; y = y_; z = z_; }

	Vector operator+(const Vector &b) const { 
		return Vector(x + b.x, y + b.y, z + b.z); 
	}
	
	Vector operator-(const Vector &b) const { 
		return Vector(x - b.x, y - b.y, z - b.z); 
	}

	Vector operator*(double b) const { 
		return Vector(x*b, y*b, z*b); 
	}

	Vector operator%(Vector &b) { 
		return Vector(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x); // cross product - normal of two vectors
	}

	Vector mult(const Vector &b) const { 
		return Vector(x*b.x, y*b.y, z*b.z); 
	}

	Vector& norm() { 
		return *this = *this * (1 / sqrt(x * x + y * y + z * z)); // divide vector by its own length
	}

	double dot(const Vector &b) const { 
		return x * b.x + y * b.y + z * b.z; // angle between two vectors
	}
};

struct Ray {
	Vector origin, direction; // origin vector, direction vector

	Ray(Vector origin_, Vector direction_) : origin(origin_), direction(direction_) {}
};

enum Material { DIFFUSE, SPECULAR, REFRACTIVE };

struct Sphere {
	double radius; 
	Vector position, emission, colour; // position, emission, colour
	Material material; // determines the type of reflection

	Sphere(double radius_, Vector position_, Vector emission_, Vector colour_, Material material_) : radius(radius_), position(position_), emission(emission_), colour(colour_), material(material_) {}

	// returns distance, 0 if no intersection of ray and sphere
	double intersect(const Ray &ray) const {
		// sphere equation in vector form = (P - C) . (P - C) - r^2 = 0
		// sub P for Ray point equation = (O + tD - C) . (O + tD - C) - r^2 = 0
		// = (D . D)t^2 + 2D . (O - C)t + (O - C) . (O - C) - r^2 = 0

		Vector op = position - ray.origin; // sphere center (position) minus the ray origin vector
		double t, eps = 1e-4;
		double b = op.dot(ray.direction);
		double det = b * b - op.dot(op) + radius * radius;

		if (det < 0) return 0; // no intersection
		else det = sqrt(det);

		return (t = b - det) > eps ? t : ((t = b + det) > eps ? t : 0);
	}
};

Sphere spheres[] = {
	Sphere(1e5, Vector(1e5 + 1,40.8,81.6), Vector(), Vector(.75,.25,.25), DIFFUSE),
	Sphere(1e5, Vector(-1e5 + 99,40.8,81.6), Vector(), Vector(.25,.25,.75), DIFFUSE), 
	Sphere(1e5, Vector(50,40.8, 1e5), Vector(), Vector(.75,.75,.75), DIFFUSE), 
	Sphere(1e5, Vector(50,40.8,-1e5 + 170), Vector(), Vector(), DIFFUSE), 
	Sphere(1e5, Vector(50, 1e5, 81.6), Vector(), Vector(.75,.75,.75), DIFFUSE), 
	Sphere(1e5, Vector(50,-1e5 + 81.6,81.6), Vector(), Vector(.75,.75,.75), DIFFUSE), 
	Sphere(16.5, Vector(27,16.5,47), Vector(), Vector(1,1,1)*.999, SPECULAR), 
	Sphere(11, Vector(55,11,95), Vector(), Vector(1,1,1)*.999, SPECULAR), 
	Sphere(20, Vector(73,16.5,55), Vector(), Vector(1,1,1)*.999, REFRACTIVE), 
	Sphere(1.5, Vector(50,81.6 - 16.5,81.6),Vector(4,4,4) * 100,  Vector(), DIFFUSE)
};

int numSpheres = sizeof(spheres) / sizeof(Sphere);

inline double clamp(double x) {
	return x < 0 ? 0 : x > 1 ? 1 : x;
}

// double to int to represent value in rgb
inline int toInt(double x) {
	return int(pow(clamp(x), 1 / 2.2) * 255 + 0.5);
}

inline bool intersect(const Ray &ray, double &t, int &id) {
	double n = sizeof(spheres) / sizeof(Sphere);
	double d;
	double inf = t = 1e20;

	// check each sphere and keep the closest intersection
	for (int i = int(n); i--;) {
		if ((d = spheres[i].intersect(ray)) && d < t) {
			t = d;
			id = i;
		}
	}

	return t < inf;
}

Vector radiance(const Ray &ray, int depth, unsigned short *seed, int emissive = 1) {
	double t; // distance to intersection
	int id = 0; // id of intersected object

	if (!intersect(ray, t, id)) return Vector(); // if no intersection, return black

	const Sphere &obj = spheres[id]; // the intersected object
	
	Vector intersection = ray.origin + ray.direction * t; // intersection point of ray
	Vector normal = (intersection - obj.position).norm(); // normal of object
	Vector oriented = normal.dot(ray.direction) < 0 ? normal : normal * -1; // oriented normal (entering or exiting glass?)
	Vector colour = obj.colour; // object colour

	// stop the recursion randomly based on surface reflectivity, use maximum RGB of the surface colour
	double p = colour.x > colour.y && colour.x > colour.z ? colour.x : colour.y > colour.z ? colour.y : colour.z;
	if (++depth > 5 || !p) if (erand48(seed) < p) colour = colour * (1 / p); else return obj.emission * emissive; // Russian Roulette termination after depth 5, unbiased termination
	if (depth > 100) return obj.emission; // prevents stack overflow from convergence

	// diffuse reflection
	if (obj.material == DIFFUSE) {
		double r1 = 2 * M_PI * erand48(seed); // random angle
		double r2 = erand48(seed), r2s = sqrt(r2); // random distance from center
		
		// u, v, w represents and orthonormal coordinate frame
		Vector w = oriented; // normal
		Vector u = ((fabs(w.x) > 0.1 ? Vector(0, 1) : Vector(1)) % w).norm(); // u is perpendicular to w
		Vector v = w % u; // v is perpendicular to u and w
		Vector d = (u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2)).norm(); // random reflection ray

		// loop lights
		Vector e;
		for (int i = 0; i < numSpheres; i++) {
			const Sphere &sphere = spheres[i];

			if (sphere.emission.x <= 0 && sphere.emission.y <= 0 && sphere.emission.z <= 0) continue; // skip non-lights
						
			// coordinate system (sw, su, sv)
			Vector sw = sphere.position - intersection;
			Vector su = ((fabs(sw.x) > 0.1 ? Vector(0, 1) : Vector(1)) % sw).norm();
			Vector sv = sw % su; 
			
			// create random direction towards sphere
			double cos_a_max = sqrt(1 - sphere.radius * sphere.radius / (intersection - sphere.position).dot(intersection - sphere.position)); // maximum angle

			// calculate random sample direction
			double eps1 = erand48(seed), eps2 = erand48(seed);
			double cos_a = 1 - eps1 + eps1 * cos_a_max;
			double sin_a = sqrt(1 - cos_a * cos_a);
			double phi = 2 * M_PI * eps2;
			Vector shadow = su * cos(phi) * sin_a + sv * sin(phi) * sin_a + sw * cos_a;
			shadow.norm();

			// shadow ray
			if (intersect(Ray(intersection, shadow), t, id) && id == i) { // occlusion check
				double omega = 2 * M_PI * (1 - cos_a_max);
				e = e + colour.mult(sphere.emission * shadow.dot(oriented) * omega) * M_1_PI; // calculate lighting and increment
			}
		}

		return obj.emission * emissive + e + colour.mult(radiance(Ray(intersection, d), depth, seed, 0));
	}

	// specular reflection
	else if (obj.material == SPECULAR) {
		return obj.emission + colour.mult(radiance(Ray(intersection, ray.direction - normal * 2 * normal.dot(ray.direction)), depth, seed));
	}

	// if neither diffuse or specular then the surface is glass
	Ray reflRay(intersection, ray.direction - normal * 2 * normal.dot(ray.direction)); // reflected ray (glass reflects and refracts)
	bool into = normal.dot(oriented) > 0; // direction of ray
	double nc = 1, nt = 1.5, nnt = into ? nc / nt : nt / nc, ddn = ray.direction.dot(oriented), cos2t; // Refraction Index for glass = 1.5

	if ((cos2t = 1 - nnt * nnt * (1 - ddn * ddn)) < 0) return obj.emission + colour.mult(radiance(reflRay, depth, seed)); // total internal reflection

	Vector refracted = (ray.direction * nnt - normal * ((into ? 1 : -1) * (ddn * nnt + sqrt(cos2t)))).norm(); // refracted ray (Fresnel term)

	double a = nt - nc, b = nt + nc, R0 = a * a / (b * b), c = 1 - (into ? -ddn : refracted.dot(normal));
	double Re = R0 + (1 - R0) * c * c * c * c * c, Tr = 1 - Re, P = 0.25 + 0.5 * Re, RP = Re / P, TP = Tr / (1 - P); // R0 = reflectance at normal incidence, Re = Fresnel reflectance
	
	// P = reflection probability
	// Russian Roulette termination, 2 recursive calls if depth <= 2
	return obj.emission + colour.mult(depth > 2 ? (erand48(seed) < P ?
		radiance(reflRay, depth, seed) * RP : radiance(Ray(intersection, refracted), depth, seed) * TP) :
		radiance(reflRay, depth, seed) * Re + radiance(Ray(intersection, refracted), depth, seed) * Tr);
}

int main(int argc, char *argv[])
{
	int width = 512, height = 384; // image size
	int samples = argc == 2 ? atoi(argv[1]) / 4 : 1; // number of samples (default of 1)
	Vector colours; // colours
	Vector *image = new Vector[width * height]; // the image
	
	Ray camera(Vector(50, 52, 295.6), Vector(0, -0.042612, -1).norm()); // camera position, direction
	Vector cx = Vector(width * 0.5135 / height); // x increment (implies 0 for y, z)
	Vector cy = Vector(cx % camera.direction).norm() * 0.5135; // y increment (cross product)	

	#pragma omp parallel for schedule(dynamic, 1) private(r) // OpenMP: each loop will run in its own thread

	// loop over all pixels
	for (int y = 0; y < height; y++) { // row loop
		fprintf(stderr, "\rRendering at %d Samples Per Pixel... %5.2f%%", samples * 4, 100. * y / (height - 1));

		unsigned short Xi[3] = { 0, 0, y * y * y };

		for (unsigned short x = 0; x < width; x++) // loop columns
			// 2x2 subsamples per pixel, average colour taken
			for (int sy = 0, i = (height - y - 1) * width + x; sy < 2; sy++) //subsample rows
				for (int sx = 0; sx < 2; sx++, colours = Vector()) { //subsample columns
					// samples per subsample
					for (int s = 0; s < samples; s++) {
						double r1 = 2 * erand48(Xi), dx = r1 < 1 ? sqrt(r1) - 1 : 1 - sqrt(2 - r1);
						double r2 = 2 * erand48(Xi), dy = r2 < 1 ? sqrt(r2) - 1 : 1 - sqrt(2 - r2);

						Vector direction = cx * (((sx + 0.5 + dx) / 2 + x) / width - 0.5) + cy * (((sy + 0.5 + dy) / 2 + y) / height - 0.5) + camera.direction; // ray direction
						colours = colours + radiance(Ray(camera.origin + direction * 140, direction.norm()), 0, Xi) * (1. / samples); // ray radiance
					}

					image[i] = image[i] + Vector(clamp(colours.x), clamp(colours.y), clamp(colours.z)) * 0.25; // add subpixel colour estimate to image
				}
	}

	// write image to ppm format file
	FILE *f;
	errno_t err = fopen_s(&f, "image.ppm", "w"); 
	fprintf(f, "P3\n%d %d\n%d\n", width, height, 255);

	for (int i = 0; i < width * height; i++) {
		fprintf(f, "%d %d %d ", toInt(image[i].x), toInt(image[i].y), toInt(image[i].z));
	}
}