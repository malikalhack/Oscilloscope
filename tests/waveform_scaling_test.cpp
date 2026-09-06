/**
 * @file    waveform_scaling_test.cpp
 * @version 0.2.8
 * @authors Anton Chernov
 * @date    2026-09-06
 * @date    @showdate "%Y-%m-%d"
 * @par
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/******************************* Included files *******************************/
#include <cmath>
#include <iostream>

#include "waveform_scaling.h"

/********************************* Definitions ********************************/

namespace {

using oscilloscope::core::sampleIndexToSeconds;
using oscilloscope::core::sampleToVolts;

static const double kEpsilon = 1.0e-9;

/***************************** Private prototypes *****************************/

static bool expect(bool condition, const char *message);
static bool nearlyEqual(double actual, double expected);
static bool testSampleToVolts();
static bool testSampleIndexToSeconds();

/****************************** Private functions *****************************/

/** @fn expect */
static bool expect(bool condition, const char *message) {
    bool result = condition;

    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }

    return result;
}
/*----------------------------------------------------------------------------*/

/** @fn nearlyEqual */
static bool nearlyEqual(double actual, double expected) {
    return std::fabs(actual - expected) < kEpsilon;
}
/*----------------------------------------------------------------------------*/

/** @fn testSampleToVolts */
static bool testSampleToVolts() {
    bool passed = true;

    passed =
        expect(
            nearlyEqual(sampleToVolts(128U, 1.0), 0.0),
            "Midpoint sample must map to zero volts"
        ) && passed;
    passed =
        expect(
            nearlyEqual(sampleToVolts(0U, 1.0), -4.0),
            "Minimum sample must map to -4 divisions worth of volts"
        ) && passed;
    passed =
        expect(
            nearlyEqual(sampleToVolts(255U, 1.0), 3.96875),
            "Maximum sample must map to the topmost division fraction"
        ) && passed;
    passed =
        expect(
            nearlyEqual(sampleToVolts(160U, 0.5), 0.5),
            "One division above center must scale with volts/division"
        ) && passed;
    passed =
        expect(
            nearlyEqual(sampleToVolts(96U, 0.5), -0.5),
            "One division below center must scale with volts/division"
        ) && passed;

    return passed;
}
/*----------------------------------------------------------------------------*/

/** @fn testSampleIndexToSeconds */
static bool testSampleIndexToSeconds() {
    bool passed = true;

    passed =
        expect(
            nearlyEqual(sampleIndexToSeconds(0U, 1000U, 1.0e-3), 0.0),
            "First sample must be at time zero"
        ) && passed;
    passed =
        expect(
            nearlyEqual(
                sampleIndexToSeconds(500U, 1000U, 1.0e-3),
                5.0e-3
            ),
            "Midpoint sample must be at half the total capture time"
        ) && passed;
    passed =
        expect(
            nearlyEqual(
                sampleIndexToSeconds(1000U, 1000U, 1.0e-3),
                10.0e-3
            ),
            "Sample index at sampleCount must reach the full capture time"
        ) && passed;
    passed =
        expect(
            nearlyEqual(sampleIndexToSeconds(5U, 0U, 1.0e-3), 0.0),
            "Zero sampleCount must not divide by zero"
        ) && passed;

    return passed;
}

} // namespace

/********************* Application Programming Interface *********************/

/** @fn main */
int main() {
    bool passed = true;
    int result = 1;

    passed = testSampleToVolts() && passed;
    passed = testSampleIndexToSeconds() && passed;
    if (passed) {
        result = 0;
    }

    return result;
}
/******************************************************************************/
