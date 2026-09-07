/**
 * @file    instrument_scaling_profile_test.cpp
 * @version 0.2.10
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

#include "instrument_scaling_profile.h"

/********************************* Definitions ********************************/

namespace {

using oscilloscope::core::EInstrumentModel;
using oscilloscope::core::findInstrtScalingProfile;
using oscilloscope::core::SInstrumentScalingProfile;

static const double kEpsilon = 1.0e-9;

/* Expected values must match the DSO-2250 entry in
 * core/src/instrument_scaling_profile.cpp; update both together. */
static const double kExpectedHorizontalDivisions = 10.0;
static const double kExpectedVerticalDivisions = 8.0;
static const uint8_t kExpectedAdcCenterValue = 128U;     /**< Raw sample=0V */
static const double kExpectedAdcCountsPerDivision = 32.0; /**< 256/8 divs */
static const size_t kExpectedTimebaseStepCount = 10U;
static const size_t kExpectedVoltageStepCount = 8U;
static const double kExpectedFastestTimebaseStep = 4.0e-9; /**< 4 ns/div */
static const double kExpectedSlowestTimebaseStep = 1.0;    /**< 1 s/div */
static const double kExpectedSmallestVoltageStep = 0.02;   /**< 20 mV/div */
static const double kExpectedLargestVoltageStep = 5.0;     /**< 5 V/div */

/***************************** Private prototypes *****************************/

static bool expect(bool condition, const char *message);
static bool nearlyEqual(double actual, double expected);
static bool testUnknownModelHasNoProfile();
static bool testHantekDso2250Profile();

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

/** @fn testUnknownModelHasNoProfile */
static bool testUnknownModelHasNoProfile() {
    return expect(
        findInstrtScalingProfile(EInstrumentModel::eUnknown) == NULL,
        "Unknown model must have no scaling profile"
    );
}
/*----------------------------------------------------------------------------*/

/** @fn testHantekDso2250Profile */
static bool testHantekDso2250Profile() {
    bool passed = true;
    const SInstrumentScalingProfile *profile =
        findInstrtScalingProfile(EInstrumentModel::eHantekDso2250);

    passed =
        expect(profile != NULL, "DSO-2250 must have a scaling profile") &&
        passed;
    if (profile != NULL) {
        passed =
            expect(
                nearlyEqual(
                    profile->horizontalDivisions,
                    kExpectedHorizontalDivisions
                ),
                "DSO-2250 must have 10 horizontal divisions"
            ) && passed;
        passed =
            expect(
                nearlyEqual(
                    profile->verticalDivisions,
                    kExpectedVerticalDivisions
                ),
                "DSO-2250 must have 8 vertical divisions"
            ) && passed;
        passed =
            expect(
                profile->adcCenterValue == kExpectedAdcCenterValue,
                "DSO-2250 ADC center value must be 128"
            ) && passed;
        passed =
            expect(
                nearlyEqual(
                    profile->adcCountsPerDivision,
                    kExpectedAdcCountsPerDivision
                ),
                "DSO-2250 must have 32 ADC counts per division"
            ) && passed;
        passed =
            expect(
                profile->timebaseStepCount == kExpectedTimebaseStepCount,
                "DSO-2250 must have 10 timebase steps"
            ) && passed;
        passed =
            expect(
                profile->voltageStepCount == kExpectedVoltageStepCount,
                "DSO-2250 must have 8 voltage-scale steps"
            ) && passed;
        passed =
            expect(
                nearlyEqual(
                    profile->timebaseSteps[0].valuePerDivision,
                    kExpectedFastestTimebaseStep
                ),
                "Fastest DSO-2250 timebase step must be 4 ns/div"
            ) && passed;
        passed =
            expect(
                nearlyEqual(
                    profile->timebaseSteps[
                        profile->timebaseStepCount - 1U
                    ].valuePerDivision,
                    kExpectedSlowestTimebaseStep
                ),
                "Slowest DSO-2250 timebase step must be 1 s/div"
            ) && passed;
        passed =
            expect(
                nearlyEqual(
                    profile->voltageSteps[0].valuePerDivision,
                    kExpectedSmallestVoltageStep
                ),
                "Smallest DSO-2250 voltage step must be 20 mV/div"
            ) && passed;
        passed =
            expect(
                nearlyEqual(
                    profile->voltageSteps[
                        profile->voltageStepCount - 1U
                    ].valuePerDivision,
                    kExpectedLargestVoltageStep
                ),
                "Largest DSO-2250 voltage step must be 5 V/div"
            ) && passed;
    }

    return passed;
}

} // namespace

/********************* Application Programming Interface *********************/

/** @fn main */
int main() {
    bool passed = true;
    int result = 1;

    passed = testUnknownModelHasNoProfile() && passed;
    passed = testHantekDso2250Profile() && passed;
    if (passed) {
        result = 0;
    }

    return result;
}
/******************************************************************************/
