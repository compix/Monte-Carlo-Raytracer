# Monte-Carlo-Raytracer with Radeon Rays and OpenCL 1.2

![San-Miguel](/screenshots/San-Miguel.png)

# Features

* Regular Path Tracer
* Bidirectional Path Tracer
* Light Sources
  * Directional Light
  * Point Light
  * Area Lights
* GUI with editable Scene
* Uber Material
  * Glossy Reflection and Transmission: Torrance-Sparrow Microfacet BRDF and BTDF with a Trowbridge-Reitz distribution for dielectrics
  * Specular Transmission
  * Specular Reflection
  * Lambertian Reflection
  * Normal Mapping
* Currently a random sampler is used however it can be changed to Sobol via #define in assets/kernels/samplers.cl

# Build
* git clone --recursive https://github.com/compix/Monte-Carlo-Raytracer.git
* CMake - Currently only Windows is supported, minor modifications need to be made to support Linux.
* Tested on Windows 10, compiled with Visual Studio 2017 (which has built in CMake support)

# Relevant Sources
* Pharr, Matt, Wenzel Jakob und Greg Humphreys: Physically based rendering: From theory to implementation. Morgan Kaufmann, 2016.
* Veach, Eric: Robust monte carlo methods for light transport simulation. Nummer 1610. Stanford University PhD thesis, 1997.
* Torrance, Kenneth E und Ephraim M Sparrow: Theory for off-specular reflection from roughened surfaces. Josa, 57(9):1105-1114, 1967.
* Trowbridge, TS und Karl P Reitz: Average irregularity representation of a rough surface for ray reflection.
* Munshi, Aaftab: The OpenCL specification version: 1.2 document revision: 19, 2012. https://www.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf
* Radeon Rays: https://www.amd.com/de/technologies/radeon-rays
* Sobol’, Il’ya Meerovich: On the distribution of points in a cube and the approximate evaluation of integrals.
* Morgan McGuire, Computer Graphics Archive, July 2017 (https://casual-effects.com/data)
