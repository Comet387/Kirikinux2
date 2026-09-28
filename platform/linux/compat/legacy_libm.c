/* Cocos 3.17's prebuilt Chipmunk archive was compiled with -ffast-math against
 * glibc's former unversioned finite-math entry points. Modern glibc removed
 * those entry points. Delegate to the public functions with the same ABI.
 */
#include <math.h>
float __powf_finite(float x, float y) { return powf(x, y); }
float __expf_finite(float x) { return expf(x); }
