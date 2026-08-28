#include <cmath>
#include <iostream>
#include <string>

#include "src/Feature.h"
#include "src/QyooModel.h"

bool verbose = false;
void logVerbose(const std::string &) { }

static int failures = 0;

static void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << std::endl;
        failures++;
    }
}

static ProjectivePoint modelCircle(double angle)
{
    return ProjectivePoint(0.5 + 0.5 * cos(angle), 0.5 + 0.5 * sin(angle));
}

static void appendMapped(Feature &feature, const ProjectiveTransform &transform,
                         const ProjectivePoint &point)
{
    ProjectivePoint mapped;
    require(transform.map(point, mapped), "synthetic contour point maps");
    feature.origPoints.push_back(Feature::Point(static_cast<int>(mapped.x + 0.5),
                                                static_cast<int>(mapped.y + 0.5)));
}

static Feature syntheticFeature(const ProjectiveTransform &truth)
{
    Feature feature;
    const int edgeSamples = 120;
    for (int index = 0; index <= edgeSamples; index++)
        appendMapped(feature, truth, ProjectivePoint(0.5 * index / edgeSamples, 0.0));
    const int arcSamples = 420;
    for (int index = 0; index <= arcSamples; index++)
        appendMapped(feature, truth, modelCircle(-M_PI / 2.0 + 3.0 * M_PI / 2.0 * index / arcSamples));
    for (int index = edgeSamples; index >= 0; index--)
        appendMapped(feature, truth, ProjectivePoint(0.0, 0.5 * index / edgeSamples));

    ProjectivePoint corner, xJoin, yJoin;
    truth.map(ProjectivePoint(0.0, 0.0), corner);
    truth.map(ProjectivePoint(0.5, 0.0), xJoin);
    truth.map(ProjectivePoint(0.0, 0.5), yJoin);
    feature.mat = QyooMatrix(
        2.0 * (xJoin.x - corner.x), 2.0 * (yJoin.x - corner.x), corner.x,
        2.0 * (xJoin.y - corner.y), 2.0 * (yJoin.y - corner.y), corner.y,
        0.0, 0.0, 1.0);
    return feature;
}

static void proveEstimate(const std::string &name, const ProjectiveTransform &truth)
{
    Feature feature = syntheticFeature(truth);
    require(feature.estimateProjectiveTransform(), name + " estimate succeeds");
    require(feature.projectiveValid, name + " estimate is valid");
    require(feature.projectiveCorrespondenceCount == static_cast<int>(feature.origPoints.size()),
            name + " uses the retained contour");
    const ProjectivePoint checks[] = {
        ProjectivePoint(0.0, 0.0), ProjectivePoint(0.5, 0.0),
        ProjectivePoint(1.0, 0.5), ProjectivePoint(0.5, 1.0),
        ProjectivePoint(0.0, 0.5), ProjectivePoint(0.37, 0.74)
    };
    for (const ProjectivePoint &point : checks)
    {
        ProjectivePoint expected, actual;
        require(truth.map(point, expected), name + " truth check maps");
        require(feature.projectiveMat.map(point, actual), name + " fitted check maps");
        require(hypot(actual.x - expected.x, actual.y - expected.y) < 1.2,
                name + " fitted point is within one quantized contour pixel");
    }

    require(feature.estimateCarrierProjectiveClass(),
            name + " line/conic carrier class succeeds");
    require(feature.carrierCirclePointCount > 100,
            name + " retains substantial curved-boundary evidence");
    require(feature.carrierFirstEdgePointCount > 20 &&
            feature.carrierSecondEdgePointCount > 20,
            name + " retains both straight carrier edges");
    double bestGridRms = 1e100;
    for (int index = 0; index <= 2000; index++)
    {
        double amount = -0.5 + index / 2000.0;
        ProjectiveTransform candidate = feature.carrierProjectiveMat *
            QyooModel::carrierAmbiguityTransform(amount);
        double squared = 0.0;
        int count = 0;
        QyooModel *model = QyooModel::getQyooModel();
        for (int row = 0; row < model->numRows(); row++)
            for (int position = 0; position < model->numPos(); position++)
            {
                SimplePoint2D location = model->dotLocation(position, row);
                ProjectivePoint expected, actual;
                require(truth.map(ProjectivePoint(location.x, location.y), expected),
                        name + " truth grid maps");
                require(candidate.map(ProjectivePoint(location.x, location.y), actual),
                        name + " carrier grid maps");
                double dx = expected.x - actual.x;
                double dy = expected.y - actual.y;
                squared += dx * dx + dy * dy;
                count++;
            }
        bestGridRms = std::min(bestGridRms, sqrt(squared / count));
    }
    require(bestGridRms < 1.2,
            name + " carrier class contains the quantized ground-truth grid");
}

int main()
{
    proveEstimate("affine control", ProjectiveTransform(-256, 0, 384,
                                                         0, -256, 384,
                                                         0, 0, 1));
    proveEstimate("trapezoidal perspective", ProjectiveTransform(-221, -19, 397,
                                                                  13, -247, 341,
                                                                  0.0009, -0.0007, 1));
    proveEstimate("asymmetric perspective", ProjectiveTransform(-188, 37, 374,
                                                                 -21, -215, 299,
                                                                 -0.0011, 0.0016, 1));

    Feature tooSmall;
    tooSmall.origPoints.push_back(Feature::Point(0, 0));
    require(!tooSmall.estimateProjectiveTransform(), "insufficient contour is rejected");

    if (failures != 0)
        return 1;
    std::cout << "PASS: accepted-contour projective estimates and insufficient geometry rejection" << std::endl;
    return 0;
}
