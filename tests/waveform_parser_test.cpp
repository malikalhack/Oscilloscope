/**
 * @file    waveform_parser_test.cpp
 * @version 0.2.6
 * @authors Anton Chernov
 * @date    2026-09-05
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
#include <iostream>

#include "waveform_parser.h"

/********************************* Definitions ********************************/

namespace {

using oscilloscope::capture::parseCaptureStateResponse;
using oscilloscope::capture::parseInterleavedWaveformSamples;
using oscilloscope::capture::SCaptureStateResponse;
using oscilloscope::capture::SWaveformSamples;

/***************************** Private prototypes *****************************/

static bool expect(bool condition, const char *message);
static bool testCaptureStateResponse();
static bool testInterleavedSamples();
static bool testInvalidSampleBuffers();

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

/** @fn testCaptureStateResponse */
static bool testCaptureStateResponse() {
    const uint8_t packet[] = {2U, 0U, 3U, 0U};
    SCaptureStateResponse response = {0U, 0U};
    bool result = true;

    result = expect(
        parseCaptureStateResponse(packet, sizeof(packet), &response),
        "parse capture state"
    ) && result;
    result = expect(response.captureState == 2U, "capture state value") &&
        result;
    result = expect(response.triggerPoint == 2U, "legacy trigger transform") &&
        result;
    result = expect(
        !parseCaptureStateResponse(packet, 3U, &response),
        "reject short capture state"
    ) && result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testInterleavedSamples */
static bool testInterleavedSamples() {
    const uint8_t packet[] = { 20U, 10U, 21U, 11U, 22U, 12U };
    SWaveformSamples samples = {};
    bool result = true;

    result = expect(
        parseInterleavedWaveformSamples(
            packet, sizeof(packet), 3U, 2U, true, &samples
        ),
        "parse interleaved samples"
    ) && result;
    result = expect(samples.sampleCount == 3U, "sample count") && result;
    result = expect(
        (samples.channelOne[0] == 10U) &&
        (samples.channelOne[1] == 11U) &&
        (samples.channelOne[2] == 12U),
        "channel one order"
    ) && result;
    result = expect(
        (samples.channelTwo[0] == 20U) &&
        (samples.channelTwo[1] == 21U) &&
        (samples.channelTwo[2] == 22U),
        "channel two order"
    ) && result;

    return result;
}

/*----------------------------------------------------------------------------*/

/** @fn testInvalidSampleBuffers */
static bool testInvalidSampleBuffers() {
    const uint8_t packet[] = {20U, 10U, 21U, 11U};
    SWaveformSamples samples = {};
    bool result = true;

    samples.sampleCount = 99U;
    result = expect(
        !parseInterleavedWaveformSamples(
            packet, sizeof(packet) - 1U, 2U, 2U, true, &samples
        ),
        "reject short sample buffer"
    ) && result;
    result = expect(samples.sampleCount == 99U, "preserve rejected samples") &&
        result;
    result = expect(
        !parseInterleavedWaveformSamples(
            packet, sizeof(packet), 32769U, 2U, true, &samples
        ),
        "reject oversized sample count"
    ) && result;

    return result;
}

} // namespace

/********************* Application Programming Interface *********************/

/** @fn main */
int main() {
    bool passed = true;
    int result = 1;

    passed = testCaptureStateResponse() && passed;
    passed = testInterleavedSamples() && passed;
    passed = testInvalidSampleBuffers() && passed;
    if (passed) {
        result = 0;
    }

    return result;
}
/******************************************************************************/