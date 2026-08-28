#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "src/Geometry.h"

static int failures = 0;

static void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        failures++;
    }
}

static void requireNear(double actual, double expected, const std::string &message, double tolerance = 1e-7)
{
    require(std::isfinite(actual) && fabs(actual - expected) <= tolerance,
            message + " expected=" + std::to_string(expected) + " actual=" + std::to_string(actual));
}

static void proveMapping(const std::string &name, const ProjectiveTransform &expected)
{
    const ProjectivePoint source[4] = {
        ProjectivePoint(0.0, 0.0), ProjectivePoint(2.0, 0.0),
        ProjectivePoint(2.0, 3.0), ProjectivePoint(0.0, 3.0)
    };
    ProjectivePoint destination[4];
    for (int index = 0; index < 4; index++)
        require(expected.map(source[index], destination[index]), name + " expected corner maps");
    ProjectiveTransform fitted;
    require(ProjectiveTransform::fromFourPointCorrespondences(source, destination, fitted), name + " fit succeeds");
    for (int index = 0; index < 4; index++)
    {
        ProjectivePoint actual;
        require(fitted.map(source[index], actual), name + " fitted corner maps");
        requireNear(actual.x, destination[index].x, name + " corner x");
        requireNear(actual.y, destination[index].y, name + " corner y");
    }
    ProjectivePoint interior(0.73, 1.81), expectedInterior, actualInterior;
    require(expected.map(interior, expectedInterior), name + " expected interior maps");
    require(fitted.map(interior, actualInterior), name + " fitted interior maps");
    requireNear(actualInterior.x, expectedInterior.x, name + " interior x", 1e-6);
    requireNear(actualInterior.y, expectedInterior.y, name + " interior y", 1e-6);
}

int main()
{
    proveMapping("identity", ProjectiveTransform());
    proveMapping("translation", ProjectiveTransform(1, 0, 4, 0, 1, -3, 0, 0, 1));
    proveMapping("scale", ProjectiveTransform(2, 0, 0, 0, 3, 0, 0, 0, 1));
    double angle = 0.61;
    proveMapping("rotation", ProjectiveTransform(cos(angle), -sin(angle), 0,
                                                  sin(angle), cos(angle), 0,
                                                  0, 0, 1));
    proveMapping("shear", ProjectiveTransform(1, 0.35, 0, -0.2, 1, 0, 0, 0, 1));
    proveMapping("trapezoid", ProjectiveTransform(1.1, 0.08, 2.0,
                                                   0.03, 0.95, -1.0,
                                                   0.12, 0.0, 1.0));
    proveMapping("asymmetric perspective", ProjectiveTransform(0.81, -0.14, 19.0,
                                                                0.17, 1.12, -7.0,
                                                                0.002, -0.003, 1.0));

    ProjectiveTransform transform(0.81, -0.14, 19.0,
                                  0.17, 1.12, -7.0,
                                  0.002, -0.003, 1.0);
    ProjectiveTransform inverse;
    require(transform.inverse(inverse), "asymmetric inverse succeeds");
    const std::vector<ProjectivePoint> roundTripPoints = {
        ProjectivePoint(0, 0), ProjectivePoint(1, 0), ProjectivePoint(1, 1),
        ProjectivePoint(0, 1), ProjectivePoint(0.37, 0.82)
    };
    for (const ProjectivePoint &point : roundTripPoints)
    {
        ProjectivePoint mapped, restored;
        require(transform.map(point, mapped), "round-trip forward maps");
        require(inverse.map(mapped, restored), "round-trip inverse maps");
        requireNear(restored.x, point.x, "round-trip x", 1e-8);
        requireNear(restored.y, point.y, "round-trip y", 1e-8);
    }

    const ProjectivePoint degenerateSource[4] = {
        ProjectivePoint(0, 0), ProjectivePoint(1, 0),
        ProjectivePoint(2, 0), ProjectivePoint(3, 0)
    };
    const ProjectivePoint square[4] = {
        ProjectivePoint(0, 0), ProjectivePoint(1, 0),
        ProjectivePoint(1, 1), ProjectivePoint(0, 1)
    };
    ProjectiveTransform rejected;
    require(!ProjectiveTransform::fromFourPointCorrespondences(degenerateSource, square, rejected),
            "collinear source is rejected");
    require(!ProjectiveTransform::fromFourPointCorrespondences(square, degenerateSource, rejected),
            "collinear destination is rejected");
    ProjectiveTransform singular(1, 0, 0, 0, 0, 0, 0, 0, 1);
    require(!singular.inverse(rejected), "singular inverse is rejected");
    ProjectiveTransform mapsToInfinity(1, 0, 0, 0, 1, 0, 1, 0, 0);
    ProjectivePoint output;
    require(!mapsToInfinity.map(ProjectivePoint(0, 1), output), "zero homogeneous denominator is rejected");

    if (failures != 0)
        return 1;
    std::cout << "PASS: identity, affine families, projective families, round-trip and degeneracy" << std::endl;
    return 0;
}
