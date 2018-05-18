#include "view/flyTo.h"

#include "util/mapProjection.h"
#include "view/view.h"
#include <cmath>
#include "log.h"

namespace Tangram {

float getMinimumEnclosingZoom(double aLng, double aLat, double bLng, double bLat, const View& view, float buffer) {
    const MapProjection& projection = view.getMapProjection();
    glm::dvec2 aMeters = projection.LonLatToMeters(glm::dvec2(aLng, aLat));
    glm::dvec2 bMeters = projection.LonLatToMeters(glm::dvec2(bLng, bLat));
    double distance = glm::distance(aMeters, bMeters) * (1. + buffer);
    double focusScale = distance / (2. * MapProjection::HALF_CIRCUMFERENCE);
    double viewScale = view.getWidth() / projection.TileSize();
    double zoom = -log2(focusScale / viewScale);
    return zoom;
}

std::function<glm::dvec3(float)> getFlyToFunction(const View& view, glm::dvec3 start, glm::dvec3 end, double& _duration) {

    // User preference for zoom/move curve sqrt(2)
    const double rho = 1.414;

    const double scale = std::pow(2.0, end.z - start.z);

    // Current view bounds in Mercator Meters
    auto rect = view.getBoundsRect();
    auto width = std::abs(rect[0][0] - rect[1][0]);
    auto height = std::abs(rect[0][1] - rect[1][1]);

    const double w0 = std::max(width, height);
    const double w1 = w0 / scale;

    const glm::dvec2 c0{start.x, start.y};
    const glm::dvec2 c1{end.x, end.y};

    const double u1 = glm::distance(c0, c1);

    auto b = [=](int i) {
                 double n = std::pow(w1, 2.0) - std::pow(w0, 2.0) +
                     (i ? -1 : 1) * std::pow(rho, 4.0) * std::pow(u1, 2.0);

                 double d = 2.0 * (i ? w1 : w0) * std::pow(rho, 2.0) * u1;
                 return n / d;
             };

    auto r = [](double b) { return std::log(-b + std::sqrt(std::pow(b, 2.0) + 1.0)); };

    const double r0 = r(b(0));
    const double r1 = r(b(1));
    const double S = (r1 - r0) / rho;

    _duration = std::isnan(S) ? std::abs(start.z - end.z) : S;

    auto u = [=](double s) {
                 double a = w0 / std::pow(rho, 2);
                 double b = a * std::cosh(r0) * std::tanh(rho * s + r0);
                 double c = a * std::sinh(r0);
                 return b - c;
             };

    auto w = [=](double s) { return std::cosh(r0) / std::cosh(rho * s + r0); };

    // Check if movement is large enough to derive the fly-to curve
    bool move = u1 > std::numeric_limits<double>::epsilon();

    return [=](float t) {

                 if (t >= 1.0) {
                     return end;
                 } else if (move) {
                     double s = S * t;
                     glm::dvec2 pos = glm::mix(c0, c1, u(s) / u1);
                     double zoom = start.z - std::log2(w(s));

                     return glm::dvec3(pos.x, pos.y, zoom);
                 } else {
                     return glm::mix(start, end, t);
                 }
           };
}

} // namespace Tangram
