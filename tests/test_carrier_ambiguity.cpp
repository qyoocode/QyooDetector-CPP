#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "src/QyooModel.h"

static void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

static double distance(const ProjectivePoint &left, const ProjectivePoint &right)
{
    return std::hypot(left.x - right.x, left.y - right.y);
}

static void requireMaps(const ProjectiveTransform &transform,
                        const ProjectivePoint &source,
                        const ProjectivePoint &expected,
                        const std::string &message,
                        double tolerance = 1e-9)
{
    ProjectivePoint actual;
    require(transform.map(source, actual), message + " remains finite");
    require(distance(actual, expected) <= tolerance, message);
}

int main()
{
    ProjectiveTransform identity = QyooModel::carrierAmbiguityTransform(0.0);
    requireMaps(identity, ProjectivePoint(0.27, 0.63),
                ProjectivePoint(0.27, 0.63), "zero amount is identity");

    const double amount = 0.21;
    ProjectiveTransform forward = QyooModel::carrierAmbiguityTransform(amount);
    ProjectiveTransform reverse = QyooModel::carrierAmbiguityTransform(-amount);
    ProjectivePoint original(0.31, 0.72), moved, roundTrip;
    require(forward.map(original, moved), "forward interior map is finite");
    require(reverse.map(moved, roundTrip), "inverse interior map is finite");
    require(distance(original, roundTrip) <= 1e-9, "opposite amount is the inverse");

    // The family fixes the distinctive straight-edge corner and both line/conic
    // tangencies exactly. These facts prove why the outline alone leaves one
    // projective degree of freedom.
    requireMaps(forward, ProjectivePoint(0.0, 0.0),
                ProjectivePoint(0.0, 0.0), "orientation corner is fixed");
    requireMaps(forward, ProjectivePoint(0.5, 0.0),
                ProjectivePoint(0.5, 0.0), "horizontal tangency is fixed");
    requireMaps(forward, ProjectivePoint(0.0, 0.5),
                ProjectivePoint(0.0, 0.5), "vertical tangency is fixed");

    for (int index = 0; index <= 100; index++)
    {
        double angle = -M_PI / 2.0 + (3.0 * M_PI / 2.0) * index / 100.0;
        ProjectivePoint circle(0.5 + 0.5 * std::cos(angle),
                               0.5 + 0.5 * std::sin(angle));
        ProjectivePoint mapped;
        require(forward.map(circle, mapped), "circle sample remains finite");
        double radius = std::hypot(mapped.x - 0.5, mapped.y - 0.5);
        require(std::abs(radius - 0.5) <= 1e-9,
                "curved carrier boundary maps onto itself");
    }

    SimplePoint2D dot = QyooModel::getQyooModel()->dotLocation(0, 0);
    ProjectivePoint gridBefore(dot.x, dot.y);
    ProjectivePoint gridAfter;
    require(forward.map(gridBefore, gridAfter), "grid point remains finite");
    require(distance(gridBefore, gridAfter) > 0.01,
            "same exact carrier outline permits a materially different payload grid");

    std::cout << "PASS: carrier outline has a proven one-parameter projective ambiguity"
              << std::endl;
    return 0;
}
