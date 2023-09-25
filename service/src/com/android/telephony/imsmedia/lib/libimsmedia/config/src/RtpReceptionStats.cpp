/**
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <RtpReceptionStats.h>
#include <string.h>

namespace android
{

namespace telephony
{

namespace imsmedia
{

RtpReceptionStats::RtpReceptionStats()
{
    mRtpTimestamp = 0;
    mRtpSequenceNumber = 0;
    mTimeDurationMs = 0;
    mJitterBufferMs = 0;
    mRoundTripTimeMs = 0;
}

RtpReceptionStats::RtpReceptionStats(const int32_t rtpTimestamp, const int32_t rtpSeq,
        const int32_t timeDuration, const int32_t jitterBuffer, const int32_t roundTripTime)
{
    mRtpTimestamp = rtpTimestamp;
    mRtpSequenceNumber = rtpSeq;
    mTimeDurationMs = timeDuration;
    mJitterBufferMs = jitterBuffer;
    mRoundTripTimeMs = roundTripTime;
}

RtpReceptionStats::RtpReceptionStats(const RtpReceptionStats& stats)
{
    mRtpTimestamp = stats.mRtpTimestamp;
    mRtpSequenceNumber = stats.mRtpSequenceNumber;
    mTimeDurationMs = stats.mTimeDurationMs;
    mJitterBufferMs = stats.mJitterBufferMs;
    mRoundTripTimeMs = stats.mRoundTripTimeMs;
}

RtpReceptionStats::~RtpReceptionStats() {}

RtpReceptionStats& RtpReceptionStats::operator=(const RtpReceptionStats& stats)
{
    if (this != &stats)
    {
        mRtpTimestamp = stats.mRtpTimestamp;
        mRtpSequenceNumber = stats.mRtpSequenceNumber;
        mTimeDurationMs = stats.mTimeDurationMs;
        mJitterBufferMs = stats.mJitterBufferMs;
        mRoundTripTimeMs = stats.mRoundTripTimeMs;
    }
    return *this;
}

bool RtpReceptionStats::operator==(const RtpReceptionStats& stats) const
{
    return (mRtpTimestamp == stats.mRtpTimestamp &&
            mRtpSequenceNumber == stats.mRtpSequenceNumber &&
            mTimeDurationMs == stats.mTimeDurationMs && mJitterBufferMs == stats.mJitterBufferMs &&
            mRoundTripTimeMs == stats.mRoundTripTimeMs);
}

bool RtpReceptionStats::operator!=(const RtpReceptionStats& stats) const
{
    return (mRtpTimestamp != stats.mRtpTimestamp ||
            mRtpSequenceNumber != stats.mRtpSequenceNumber ||
            mTimeDurationMs != stats.mTimeDurationMs || mJitterBufferMs != stats.mJitterBufferMs ||
            mRoundTripTimeMs != stats.mRoundTripTimeMs);
}

status_t RtpReceptionStats::writeToParcel(Parcel* out) const
{
    status_t err;

    if (out == nullptr)
    {
        return BAD_VALUE;
    }

    err = out->writeInt32(mRtpTimestamp);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mRtpSequenceNumber);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mTimeDurationMs);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mJitterBufferMs);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = out->writeInt32(mRoundTripTimeMs);

    if (err != NO_ERROR)
    {
        return err;
    }

    return NO_ERROR;
}

status_t RtpReceptionStats::readFromParcel(const Parcel* in)
{
    status_t err;

    if (in == nullptr)
    {
        return BAD_VALUE;
    }

    err = in->readInt32(&mRtpTimestamp);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mRtpSequenceNumber);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mTimeDurationMs);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mJitterBufferMs);

    if (err != NO_ERROR)
    {
        return err;
    }

    err = in->readInt32(&mRoundTripTimeMs);

    if (err != NO_ERROR)
    {
        return err;
    }

    return NO_ERROR;
}

int32_t RtpReceptionStats::getRtpTimestamp()
{
    return mRtpTimestamp;
}

void RtpReceptionStats::setRtpTimestamp(const int32_t rtpTimestamp)
{
    mRtpTimestamp = rtpTimestamp;
}

int32_t RtpReceptionStats::getRtpSequenceNumber()
{
    return mRtpSequenceNumber;
}

void RtpReceptionStats::setRtpSequenceNumber(const int32_t rtpSequenceNumber)
{
    mRtpSequenceNumber = rtpSequenceNumber;
}

int32_t RtpReceptionStats::getTimeDurationMs()
{
    return mTimeDurationMs;
}

void RtpReceptionStats::setTimeDurationMs(const int32_t timeDurationMs)
{
    mTimeDurationMs = timeDurationMs;
}

int32_t RtpReceptionStats::getJitterBufferMs()
{
    return mJitterBufferMs;
}

void RtpReceptionStats::setJitterBufferMs(const int32_t jitterBufferMs)
{
    mJitterBufferMs = jitterBufferMs;
}

int32_t RtpReceptionStats::getRoundTripTimeMs()
{
    return mRoundTripTimeMs;
}

void RtpReceptionStats::setRoundTripTimeMs(const int32_t roundTripTimeMs)
{
    mRoundTripTimeMs = roundTripTimeMs;
}

void RtpReceptionStats::setDefaultConfig()
{
    mRtpTimestamp = 0;
    mRtpSequenceNumber = 0;
    mTimeDurationMs = 0;
    mJitterBufferMs = 0;
    mRoundTripTimeMs = 0;
}

char* RtpReceptionStats::printDebugString()
{
    static char debugString[128] = {'\0'};
    sprintf(debugString,
            "mRtpTimestamp=%d, mRtpSequenceNumber=%d, mTimeDurationMs=%d, mJitterBufferMs=%d, "
            "mRoundTripTimeMs=%d",
            mRtpTimestamp, mRtpSequenceNumber, mTimeDurationMs, mJitterBufferMs, mRoundTripTimeMs);
    return debugString;
}

}  // namespace imsmedia

}  // namespace telephony

}  // namespace android