/*
 *  Geometry.mm
 *  ShapeFinder
 *
 *  Created by Stephen Gifford on 10/28/09.
 *  Copyright 2009 Qyoo. All rights reserved.
 *
 */

#include "Geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>

/**
 * Compute the proper arctangent for a given point in Cartesian coordinates.
 *
 * This function determines the angle (in degrees) of the vector (gx, gy) relative to the positive x-axis.
 * It handles the different quadrants (I-IV) of the Cartesian plane to return a correct result.
 * The returned angle is always between 0 and 360 degrees.
 *
 * @param gx The x-component of the vector (can be negative or zero).
 * @param gy The y-component of the vector (can be negative or zero).
 * @return The angle in degrees relative to the positive x-axis, in the range [0, 360).
 */
float properAtan(int gx, int gy)
{
    // Convert gx and gy to absolute values for calculation
    float fgx = fabs(gx);
    float fgy = fabs(gy);

    // Determine the angle based on the quadrant
    if (gx > 0)
    {
        if (gy > 0)
        {
            // Quadrant I: both gx and gy are positive
            return atan(fgy / fgx) * 180.0 / M_PI;
        }
        else
        {
            // Quadrant IV: gx > 0, gy < 0
            return atan(fgx / fgy) * 180.0 / M_PI + 270.0;
        }
    }
    else if (gx == 0)
    {
        // Special case: the point lies on the y-axis
        if (gy < 0)
        {
            // Negative y-axis direction
            return 270.0;
        }
        else
        {
            // Positive y-axis direction
            return 90.0;
        }
    }
    else // gx < 0
    {
        if (gy > 0)
        {
            // Quadrant II: gx < 0, gy > 0
            return atan(fgx / fgy) * 180.0 / M_PI + 90.0;
        }
        else
        {
            // Quadrant III: both gx and gy are negative
            return atan(fgy / fgx) * 180.0 / M_PI + 180.0;
        }
    }
}

ProjectiveTransform::ProjectiveTransform()
    : ProjectiveTransform(1.0, 0.0, 0.0,
                          0.0, 1.0, 0.0,
                          0.0, 0.0, 1.0)
{
}

ProjectiveTransform::ProjectiveTransform(double m00, double m01, double m02,
                                         double m10, double m11, double m12,
                                         double m20, double m21, double m22)
{
    values[0][0] = m00; values[0][1] = m01; values[0][2] = m02;
    values[1][0] = m10; values[1][1] = m11; values[1][2] = m12;
    values[2][0] = m20; values[2][1] = m21; values[2][2] = m22;
}

static bool solveEight(double matrix[8][8], double right[8], double solution[8], double tolerance)
{
    for (int column = 0; column < 8; column++)
    {
        int pivotRow = column;
        double pivotMagnitude = fabs(matrix[column][column]);
        for (int row = column + 1; row < 8; row++)
        {
            double magnitude = fabs(matrix[row][column]);
            if (magnitude > pivotMagnitude)
            {
                pivotMagnitude = magnitude;
                pivotRow = row;
            }
        }
        if (!std::isfinite(pivotMagnitude) || pivotMagnitude <= tolerance)
            return false;
        if (pivotRow != column)
        {
            for (int index = column; index < 8; index++)
                std::swap(matrix[column][index], matrix[pivotRow][index]);
            std::swap(right[column], right[pivotRow]);
        }
        double pivot = matrix[column][column];
        for (int row = column + 1; row < 8; row++)
        {
            double factor = matrix[row][column] / pivot;
            matrix[row][column] = 0.0;
            for (int index = column + 1; index < 8; index++)
                matrix[row][index] -= factor * matrix[column][index];
            right[row] -= factor * right[column];
        }
    }

    for (int row = 7; row >= 0; row--)
    {
        double value = right[row];
        for (int column = row + 1; column < 8; column++)
            value -= matrix[row][column] * solution[column];
        double divisor = matrix[row][row];
        if (!std::isfinite(divisor) || fabs(divisor) <= tolerance)
            return false;
        solution[row] = value / divisor;
        if (!std::isfinite(solution[row]))
            return false;
    }
    return true;
}

struct PointNormalization
{
    double scale;
    double centerX;
    double centerY;
};

static bool normalizePoints(const std::vector<ProjectivePoint> &points,
                            std::vector<ProjectivePoint> &normalized,
                            PointNormalization &normalization,
                            double tolerance)
{
    normalization.centerX = 0.0;
    normalization.centerY = 0.0;
    for (const ProjectivePoint &point : points)
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            return false;
        normalization.centerX += point.x;
        normalization.centerY += point.y;
    }
    normalization.centerX /= points.size();
    normalization.centerY /= points.size();
    double meanDistance = 0.0;
    for (const ProjectivePoint &point : points)
        meanDistance += hypot(point.x - normalization.centerX, point.y - normalization.centerY);
    meanDistance /= points.size();
    if (!std::isfinite(meanDistance) || meanDistance <= tolerance)
        return false;
    normalization.scale = sqrt(2.0) / meanDistance;
    normalized.clear();
    normalized.reserve(points.size());
    for (const ProjectivePoint &point : points)
        normalized.push_back(ProjectivePoint(
            normalization.scale * (point.x - normalization.centerX),
            normalization.scale * (point.y - normalization.centerY)));
    return true;
}

bool ProjectiveTransform::fromCorrespondences(const std::vector<ProjectivePoint> &source,
                                              const std::vector<ProjectivePoint> &destination,
                                              ProjectiveTransform &result,
                                              double tolerance)
{
    if (source.size() != destination.size() || source.size() < 4 || tolerance <= 0.0)
        return false;
    std::vector<ProjectivePoint> normalizedSource, normalizedDestination;
    PointNormalization sourceNormalization, destinationNormalization;
    if (!normalizePoints(source, normalizedSource, sourceNormalization, tolerance) ||
        !normalizePoints(destination, normalizedDestination, destinationNormalization, tolerance))
        return false;

    double normal[8][8] = {{0.0}};
    double right[8] = {0.0};
    for (size_t index = 0; index < source.size(); index++)
    {
        double x = normalizedSource[index].x;
        double y = normalizedSource[index].y;
        double u = normalizedDestination[index].x;
        double v = normalizedDestination[index].y;
        double rows[2][8] = {
            {x, y, 1.0, 0.0, 0.0, 0.0, -u * x, -u * y},
            {0.0, 0.0, 0.0, x, y, 1.0, -v * x, -v * y}
        };
        double targets[2] = {u, v};
        for (int equation = 0; equation < 2; equation++)
        {
            for (int row = 0; row < 8; row++)
            {
                right[row] += rows[equation][row] * targets[equation];
                for (int column = 0; column < 8; column++)
                    normal[row][column] += rows[equation][row] * rows[equation][column];
            }
        }
    }
    double solution[8] = {0.0};
    if (!solveEight(normal, right, solution, tolerance))
        return false;

    ProjectiveTransform normalizedTransform(
        solution[0], solution[1], solution[2],
        solution[3], solution[4], solution[5],
        solution[6], solution[7], 1.0);
    ProjectiveTransform sourceNormalizationMatrix(
        sourceNormalization.scale, 0.0, -sourceNormalization.scale * sourceNormalization.centerX,
        0.0, sourceNormalization.scale, -sourceNormalization.scale * sourceNormalization.centerY,
        0.0, 0.0, 1.0);
    double inverseDestinationScale = 1.0 / destinationNormalization.scale;
    ProjectiveTransform destinationDenormalizationMatrix(
        inverseDestinationScale, 0.0, destinationNormalization.centerX,
        0.0, inverseDestinationScale, destinationNormalization.centerY,
        0.0, 0.0, 1.0);
    ProjectiveTransform candidate = destinationDenormalizationMatrix * normalizedTransform * sourceNormalizationMatrix;
    if (!candidate.isFinite())
        return false;
    ProjectiveTransform inverseCandidate;
    if (!candidate.inverse(inverseCandidate, tolerance))
        return false;
    result = candidate;
    return true;
}

bool ProjectiveTransform::fromFourPointCorrespondences(const ProjectivePoint source[4],
                                                       const ProjectivePoint destination[4],
                                                       ProjectiveTransform &result,
                                                       double tolerance)
{
    return fromCorrespondences(std::vector<ProjectivePoint>(source, source + 4),
                               std::vector<ProjectivePoint>(destination, destination + 4),
                               result, tolerance);
}

bool ProjectiveTransform::map(const ProjectivePoint &source, ProjectivePoint &destination,
                              double tolerance) const
{
    double mappedX = values[0][0] * source.x + values[0][1] * source.y + values[0][2];
    double mappedY = values[1][0] * source.x + values[1][1] * source.y + values[1][2];
    double mappedW = values[2][0] * source.x + values[2][1] * source.y + values[2][2];
    if (!std::isfinite(mappedX) || !std::isfinite(mappedY) || !std::isfinite(mappedW) ||
        fabs(mappedW) <= tolerance)
        return false;
    destination.x = mappedX / mappedW;
    destination.y = mappedY / mappedW;
    return std::isfinite(destination.x) && std::isfinite(destination.y);
}

bool ProjectiveTransform::inverse(ProjectiveTransform &result, double tolerance) const
{
    double a = values[0][0], b = values[0][1], c = values[0][2];
    double d = values[1][0], e = values[1][1], f = values[1][2];
    double g = values[2][0], h = values[2][1], i = values[2][2];
    double determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    double maximum = 0.0;
    for (int row = 0; row < 3; row++)
        for (int column = 0; column < 3; column++)
            maximum = std::max(maximum, fabs(values[row][column]));
    double determinantScale = std::max(1.0, maximum * maximum * maximum);
    if (!std::isfinite(determinant) || fabs(determinant) <= tolerance * determinantScale)
        return false;
    double factor = 1.0 / determinant;
    ProjectiveTransform candidate(
        (e * i - f * h) * factor, (c * h - b * i) * factor, (b * f - c * e) * factor,
        (f * g - d * i) * factor, (a * i - c * g) * factor, (c * d - a * f) * factor,
        (d * h - e * g) * factor, (b * g - a * h) * factor, (a * e - b * d) * factor);
    if (!candidate.isFinite())
        return false;
    result = candidate;
    return true;
}

bool ProjectiveTransform::isFinite() const
{
    for (int row = 0; row < 3; row++)
        for (int column = 0; column < 3; column++)
            if (!std::isfinite(values[row][column]))
                return false;
    return true;
}

ProjectiveTransform ProjectiveTransform::operator*(const ProjectiveTransform &that) const
{
    double product[3][3] = {{0.0}};
    for (int row = 0; row < 3; row++)
        for (int column = 0; column < 3; column++)
            for (int inner = 0; inner < 3; inner++)
                product[row][column] += values[row][inner] * that.values[inner][column];
    return ProjectiveTransform(
        product[0][0], product[0][1], product[0][2],
        product[1][0], product[1][1], product[1][2],
        product[2][0], product[2][1], product[2][2]);
}
