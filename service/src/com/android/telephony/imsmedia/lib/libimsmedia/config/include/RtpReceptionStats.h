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

#ifndef IMSMEDIA_RTP_RECEPTION_STATISTICS_H
#define IMSMEDIA_RTP_RECEPTION_STATISTICS_H

#include <binder/Parcel.h>
#include <binder/Parcelable.h>
#include <binder/Status.h>
#include <stdint.h>

namespace android
{

namespace telephony
{

namespace imsmedia
{

/* Native representation of android.telephony.imsmedia.RtpReceptionStats */

class RtpReceptionStats : public Parcelable
{
public:
    RtpReceptionStats();
    RtpReceptionStats(const int32_t rtpTimestamp, const int32_t rtpSeq, const int32_t timeDuration,
            const int32_t jitterBuffer, const int32_t roundTripTime);
    RtpReceptionStats(const RtpReceptionStats& params);
    virtual ~RtpReceptionStats();
    RtpReceptionStats& operator=(const RtpReceptionStats& params);
    bool operator==(const RtpReceptionStats& params) const;
    bool operator!=(const RtpReceptionStats& params) const;
    status_t writeToParcel(Parcel* out) const;
    status_t readFromParcel(const Parcel* in);
    int32_t getRtpTimestamp();
    void setRtpTimestamp(const int32_t rtpTimestamp);
    int32_t getRtpSequenceNumber();
    void setRtpSequenceNumber(const int32_t rtpSequenceNumber);
    int32_t getTimeDurationMs();
    void setTimeDurationMs(const int32_t timeDurationMs);
    int32_t getJitterBufferMs();
    void setJitterBufferMs(const int32_t jitterBufferMs);
    int32_t getRoundTripTimeMs();
    void setRoundTripTimeMs(const int32_t roundTripTimeMs);
    void setDefaultConfig();
    char* printDebugString();

private:
    /* The timestamp reflects the sampling instant of the latest RTP packet received */
    int32_t mRtpTimestamp;
    /* The sequence number of latest RTP packet received */
    int32_t mRtpSequenceNumber;
    /* The clock time in millisecond of latest RTP packet received */
    int32_t mTimeDurationMs;
    /** The mean jitter buffer delay of a media stream from received to playback, measured in
     * milliseconds, within the reporting interval */
    int32_t mJitterBufferMs;
    /* The round trip time delay in millisecond when latest RTP packet received */
    int32_t mRoundTripTimeMs;
};

}  // namespace imsmedia

}  // namespace telephony

}  // namespace android

#endif  // IMSMEDIA_RTP_RECEPTION_STATISTICS_H
