/**
 * @file    waveform_trigger_test.cpp
 * @version 0.2.11
 * @authors Anton Chernov
 * @date    2026-09-07
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
#include <cstddef>
#include <iostream>

#include "waveform_trigger.h"

/********************************* Definitions ********************************/

namespace {

using oscilloscope::core::ETriggerSlope;
using oscilloscope::core::findEdgeTrigger;

/* A single low-to-high step: samples cross the level 128 between index 3
 * and index 4. */
static const uint8_t kRisingStep[8] =
    {10U, 20U, 30U, 40U, 200U, 210U, 220U, 230U};

/* A single high-to-low step: samples cross the level 128 between index 3
 * and index 4. */
static const uint8_t kFallingStep[8] =
    {230U, 220U, 210U, 200U, 40U, 30U, 20U, 10U};

/* Two rising crossings of level 128: between 1..2 and between 5..6. */
static const uint8_t kTwoRising[8] =
    {10U, 100U, 200U, 210U, 90U, 100U, 200U, 210U};

static const uint8_t kTriggerLevel = 128U;

/***************************** Private prototypes *****************************/

static bool expect(bool condition, const char *message);
static bool testRisingEdge();
static bool testFallingEdge();
static bool testNoCrossing();
static bool testStartIndexSkipsFirstEdge();
static bool testGuards();

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

/** @fn testRisingEdge */
static bool testRisingEdge() {
    bool passed = true;
    size_t triggerIndex = 0U;
    const bool found = findEdgeTrigger(
        kRisingStep, 8U, kTriggerLevel, ETriggerSlope::eRising, 0U,
        &triggerIndex
    );

    passed = expect(found, "Rising step must trigger") && passed;
    passed =
        expect(triggerIndex == 4U, "Rising trigger must land on the crossing")
        && passed;

    return passed;
}
/*----------------------------------------------------------------------------*/

/** @fn testFallingEdge */
static bool testFallingEdge() {
    bool passed = true;
    size_t triggerIndex = 0U;
    const bool found = findEdgeTrigger(
        kFallingStep, 8U, kTriggerLevel, ETriggerSlope::eFalling, 0U,
        &triggerIndex
    );

    passed = expect(found, "Falling step must trigger") && passed;
    passed =
        expect(triggerIndex == 4U, "Falling trigger must land on the crossing")
        && passed;

    return passed;
}
/*----------------------------------------------------------------------------*/

/** @fn testNoCrossing */
static bool testNoCrossing() {
    bool passed = true;
    size_t triggerIndex = 42U;
    const bool found = findEdgeTrigger(
        kRisingStep, 8U, kTriggerLevel, ETriggerSlope::eFalling, 0U,
        &triggerIndex
    );

    passed =
        expect(!found, "A rising-only buffer must not fire a falling trigger")
        && passed;
    passed =
        expect(triggerIndex == 42U, "Trigger index must be untouched on miss")
        && passed;

    return passed;
}
/*----------------------------------------------------------------------------*/

/** @fn testStartIndexSkipsFirstEdge */
static bool testStartIndexSkipsFirstEdge() {
    bool passed = true;
    size_t triggerIndex = 0U;
    const bool found = findEdgeTrigger(
        kTwoRising, 8U, kTriggerLevel, ETriggerSlope::eRising, 3U,
        &triggerIndex
    );

    passed = expect(found, "Second rising edge must be found") && passed;
    passed =
        expect(
            triggerIndex == 6U,
            "Start index must skip the first edge and find the second"
        ) && passed;

    return passed;
}
/*----------------------------------------------------------------------------*/

/** @fn testGuards */
static bool testGuards() {
    bool passed = true;
    size_t triggerIndex = 0U;

    passed =
        expect(
            !findEdgeTrigger(
                NULL, 8U, kTriggerLevel, ETriggerSlope::eRising, 0U,
                &triggerIndex
            ),
            "Null samples must not trigger"
        ) && passed;
    passed =
        expect(
            !findEdgeTrigger(
                kRisingStep, 1U, kTriggerLevel, ETriggerSlope::eRising, 0U,
                &triggerIndex
            ),
            "A single-sample buffer has no edge"
        ) && passed;
    passed =
        expect(
            !findEdgeTrigger(
                kRisingStep, 8U, kTriggerLevel, ETriggerSlope::eRising, 0U,
                NULL
            ),
            "Null output pointer must not trigger"
        ) && passed;

    return passed;
}

} // namespace

/********************* Application Programming Interface *********************/

/** @fn main */
int main() {
    bool passed = true;
    int result = 1;

    passed = testRisingEdge() && passed;
    passed = testFallingEdge() && passed;
    passed = testNoCrossing() && passed;
    passed = testStartIndexSkipsFirstEdge() && passed;
    passed = testGuards() && passed;
    if (passed) {
        result = 0;
    }

    return result;
}
