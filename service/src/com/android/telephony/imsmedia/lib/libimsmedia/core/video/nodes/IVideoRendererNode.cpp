/**
 * Copyright (C) 2022 The Android Open Source Project
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

#include <IVideoRendererNode.h>
#include <ImsMediaVideoRenderer.h>
#include <ImsMediaTrace.h>
#include <ImsMediaTimer.h>
#include <ImsMediaBitReader.h>
#include <VideoConfig.h>
#include <ImsMediaVideoUtil.h>
#include <VideoJitterBuffer.h>
#include <string.h>

using namespace android::telephony::imsmedia;

#define DEFAULT_UNDEFINED (-1)

IVideoRendererNode::IVideoRendererNode(BaseSessionCallback* callback) :
        JitterBufferControlNode(callback, IMS_MEDIA_VIDEO)
{
    std::unique_ptr<ImsMediaVideoRenderer> renderer(new ImsMediaVideoRenderer());
    mVideoRenderer = std::move(renderer);
    mVideoRenderer->SetSessionCallback(mCallback);

    if (mJitterBuffer)
    {
        mJitterBuffer->SetSessionCallback(mCallback);
    }

    mWindow = nullptr;
    mCondition.reset();
    mCodecType = DEFAULT_UNDEFINED;
    mWidth = 0;
    mHeight = 0;
    mSamplingRate = 0;
    mCvoValue = 0;
    memset(mConfigBuffer, 0, MAX_CONFIG_INDEX * MAX_CONFIG_LEN * sizeof(uint8_t));
    memset(mConfigLen, 0, MAX_CONFIG_INDEX * sizeof(uint32_t));
    mDeviceOrientation = 0;
    mFirstFrame = false;
    mSubtype = MEDIASUBTYPE_UNDEFINED;
    mFramerate = 0;
    mWaitIntraFrame = 0;
    mLossDuration = 0;
    mLossRateThreshold = 0;
}

IVideoRendererNode::~IVideoRendererNode() {}

kBaseNodeId IVideoRendererNode::GetNodeId()
{
    return kNodeIdVideoRenderer;
}

ImsMediaResult IVideoRendererNode::Start()
{
    IMLOGD1("[Start] codec[%d]", mCodecType);
    if (mJitterBuffer)
    {
        VideoJitterBuffer* jitter = reinterpret_cast<VideoJitterBuffer*>(mJitterBuffer);
        jitter->SetCodecType(mCodecType);
        jitter->SetFramerate(mFramerate);
        jitter->SetJitterBufferSize(15, 15, 25);
        jitter->StartTimer(mLossDuration / 1000, mLossRateThreshold);
    }

    Reset();
    std::lock_guard<std::mutex> guard(mMutex);

    if (mVideoRenderer)
    {
        mVideoRenderer->SetCodec(mCodecType);
        mVideoRenderer->SetResolution(mWidth, mHeight);
        mVideoRenderer->SetDeviceOrientation(mDeviceOrientation);
        mVideoRenderer->SetSurface(mWindow);

        if (!mVideoRenderer->Start())
        {
            return RESULT_NOT_READY;
        }
    }

    mFirstFrame = false;
    mNodeState = kNodeStateRunning;
    return RESULT_SUCCESS;
}

void IVideoRendererNode::Stop()
{
    IMLOGD0("[Stop]");
    std::lock_guard<std::mutex> guard(mMutex);

    if (mVideoRenderer)
    {
        mVideoRenderer->Stop();
    }

    if (mJitterBuffer != nullptr)
    {
        VideoJitterBuffer* jitter = reinterpret_cast<VideoJitterBuffer*>(mJitterBuffer);
        jitter->StopTimer();
    }

    mNodeState = kNodeStateStopped;
}

bool IVideoRendererNode::IsRunTime()
{
    return false;
}

bool IVideoRendererNode::IsSourceNode()
{
    return false;
}

void IVideoRendererNode::SetConfig(void* config)
{
    if (config == nullptr)
    {
        return;
    }

    VideoConfig* pConfig = reinterpret_cast<VideoConfig*>(config);
    mCodecType = ImsMediaVideoUtil::ConvertCodecType(pConfig->getCodecType());
    mSamplingRate = pConfig->getSamplingRateKHz();
    mWidth = pConfig->getResolutionWidth();
    mHeight = pConfig->getResolutionHeight();
    mCvoValue = pConfig->getCvoValue();
    mDeviceOrientation = pConfig->getDeviceOrientationDegree();
    mFramerate = pConfig->getFramerate();
}

bool IVideoRendererNode::IsSameConfig(void* config)
{
    if (config == nullptr)
    {
        return true;
    }

    VideoConfig* pConfig = reinterpret_cast<VideoConfig*>(config);
    return (mCodecType == ImsMediaVideoUtil::ConvertCodecType(pConfig->getCodecType()) &&
            mWidth == pConfig->getResolutionWidth() && mHeight == pConfig->getResolutionHeight() &&
            mCvoValue == pConfig->getCvoValue() &&
            mDeviceOrientation == pConfig->getDeviceOrientationDegree() &&
            mSamplingRate == pConfig->getSamplingRateKHz());
}

void IVideoRendererNode::ProcessData()
{
    std::lock_guard<std::mutex> guard(mMutex);
    uint8_t* pData = nullptr;
    uint32_t nDataSize = 0;
    uint32_t nTimeStamp = 0;
    bool bMark = false;
    uint32_t nSeqNum = 0;
    uint32_t timestamp = 0;
    uint32_t nBitstreamSize = 0;
    ImsMediaSubType subtype = MEDIASUBTYPE_UNDEFINED;
    uint32_t nInitialSeqNum = 0;
    uint32_t nBufferOffset = 0;
    ImsMediaSubType dataType;

    while (GetData(&subtype, &pData, &nDataSize, &nTimeStamp, &bMark, &nSeqNum, &dataType))
    {
        IMLOGD_PACKET4(IM_PACKET_LOG_VIDEO,
                "[ProcessData] subtype[%d], Size[%d], TimeStamp[%d] nBitstreamSize[%d]", subtype,
                nDataSize, nTimeStamp, nBitstreamSize);

        if (timestamp == 0)
        {
            timestamp = nTimeStamp;
        }
        else if (timestamp != nTimeStamp)
        {
            break;
        }

        if (nDataSize >= MAX_RTP_PAYLOAD_BUFFER_SIZE)
        {
            IMLOGE1("[ProcessData] exceed buffer size[%d]", nDataSize);
            return;
        }

        memcpy(mBuffer + nBitstreamSize, pData, nDataSize);
        nBitstreamSize += nDataSize;

        if (nInitialSeqNum == 0)
        {
            nInitialSeqNum = nSeqNum;
        }

        DeleteData();

        if (bMark)
        {
            break;
        }
    }

    if (nBitstreamSize == 0)
    {
        return;
    }

    // remove AUD nal unit
    uint32_t nDatabufferSize = nBitstreamSize;
    uint8_t* pDataBuff = mBuffer;
    RemoveAUDNalUnit(mBuffer, nBitstreamSize, &pDataBuff, &nDatabufferSize);

    // check Config String for updating config frame
    nBufferOffset = 0;
    if ((mCodecType == kVideoCodecHevc || mCodecType == kVideoCodecAvc) &&
            IsConfigFrame(pDataBuff, nDatabufferSize, &nBufferOffset))
    {
        SaveConfigFrame(pDataBuff + nBufferOffset, nDatabufferSize - nBufferOffset, kConfigSps);
        SaveConfigFrame(pDataBuff + nBufferOffset, nDatabufferSize - nBufferOffset, kConfigPps);

        if (mCodecType == kVideoCodecHevc)
        {
            SaveConfigFrame(pDataBuff + nBufferOffset, nDatabufferSize - nBufferOffset, kConfigVps);
        }

        if (IsSps(pDataBuff, nDatabufferSize, &nBufferOffset))
        {
            IMLOGD_PACKET1(
                    IM_PACKET_LOG_VIDEO, "[ProcessData] parse SPS - nOffset[%d]", nBufferOffset);
            tCodecConfig codecConfig;

            if (mCodecType == kVideoCodecAvc)
            {
                if (ImsMediaVideoUtil::ParseAvcSps(pDataBuff + nBufferOffset,
                            nDatabufferSize - nBufferOffset, &codecConfig))
                {
                    CheckResolution(codecConfig.nWidth, codecConfig.nHeight);
                }
            }
            else if (mCodecType == kVideoCodecHevc)
            {
                if (ImsMediaVideoUtil::ParseHevcSps(pDataBuff + nBufferOffset,
                            nDatabufferSize - nBufferOffset, &codecConfig))
                {
                    CheckResolution(codecConfig.nWidth, codecConfig.nHeight);
                }
            }
        }

        return;
    }

    IMLOGD_PACKET2(IM_PACKET_LOG_VIDEO, "[ProcessData] nBitstreamSize[%d] nDatabufferSize[%d]",
            nBitstreamSize, nDatabufferSize);

    bool isIntraFrame = IsIntraFrame(pDataBuff, nDatabufferSize);

    // drop non-idr frame when idr frame is not received
    if (mWaitIntraFrame > 0 && nDatabufferSize > 0)
    {
        if (isIntraFrame)
        {
            mWaitIntraFrame = 0;
        }
        else
        {
            // Send FIR when I-frame wasn't received
            if ((mWaitIntraFrame % mFramerate) == 0)  // every 1 sec
            {
                // TODO: send PLI event
                IMLOGD0("[ProcessData] request Send PLI");
            }

            mWaitIntraFrame--;
            nDatabufferSize = 0;  // drop non-DIR frame

            IMLOGD1("[ProcessData] wait intra frame[%d]", mWaitIntraFrame);
        }
    }

    if (!mFirstFrame)
    {
        IMLOGD0("[ProcessData] notify first frame");
        mFirstFrame = true;

        if (mCallback != nullptr)
        {
            mCallback->SendEvent(kImsMediaEventFirstPacketReceived);
        }
    }

    // cvo
    if (mCvoValue > 0)
    {
        if (mSubtype == MEDIASUBTYPE_UNDEFINED && subtype == MEDIASUBTYPE_UNDEFINED)
        {
            subtype = MEDIASUBTYPE_ROT0;
        }

        // rotation changed
        if (mSubtype != subtype && (subtype >= MEDIASUBTYPE_ROT0 && subtype <= MEDIASUBTYPE_ROT270))
        {
            mSubtype = subtype;
            int degree = 0;

            switch (mSubtype)
            {
                default:
                case MEDIASUBTYPE_ROT0:
                    degree = 0;
                    break;
                case MEDIASUBTYPE_ROT90:
                    degree = 90;
                    break;
                case MEDIASUBTYPE_ROT180:
                    degree = 180;
                    break;
                case MEDIASUBTYPE_ROT270:
                    degree = 270;
                    break;
            }

            mVideoRenderer->UpdatePeerOrientation(degree);
            NotifyPeerDimensionChanged();
        }
    }

    // send sps/pps before send I frame
    if (isIntraFrame)
    {
        QueueConfigFrame(timestamp);
    }

    mVideoRenderer->OnDataFrame(pDataBuff, nDatabufferSize, timestamp, false);
}

void IVideoRendererNode::UpdateSurface(ANativeWindow* window)
{
    IMLOGD1("[UpdateSurface] surface[%p]", window);
    mWindow = window;
}

void IVideoRendererNode::UpdateRoundTripTimeDelay(int32_t delay)
{
    IMLOGD1("[UpdateRoundTripTimeDelay] delay[%d]", delay);

    if (mJitterBuffer != nullptr)
    {
        VideoJitterBuffer* jitter = reinterpret_cast<VideoJitterBuffer*>(mJitterBuffer);

        // calculate Response wait time : RWT = RTTD (mm) + 2 * frame duration
        jitter->SetResponseWaitTime((delay / DEMON_NTP2MSEC) + 2 * (1000 / mFramerate));
    }
}

void IVideoRendererNode::SetPacketLossParam(uint32_t time, uint32_t rate)
{
    IMLOGD2("[SetPacketLossParam] time[%d], rate[%d]", time, rate);

    mLossDuration = time;
    mLossRateThreshold = rate;
}

bool IVideoRendererNode::IsIntraFrame(uint8_t* pbBuffer, uint32_t nBufferSize)
{
    bool bIntraFrame = false;

    if (nBufferSize <= 4)
    {
        return false;
    }

    IMLOGD_PACKET2(IM_PACKET_LOG_VIDEO, "[IsIntraFrame] size[%d], data[%s]", nBufferSize,
            ImsMediaTrace::IMTrace_Bin2String(
                    reinterpret_cast<const char*>(pbBuffer), nBufferSize > 16 ? 16 : nBufferSize));

    switch (mCodecType)
    {
        case kVideoCodecAvc:
        {
            uint32_t nCurrSize = nBufferSize;
            uint8_t* nCurrBuff = pbBuffer;

            while (nCurrSize >= 5)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01 && (nCurrBuff[4] & 0x1F) == 5)
                {
                    bIntraFrame = true;
                    break;
                }
                nCurrBuff++;
                nCurrSize--;
            }

            break;
        }
        case kVideoCodecHevc:
        {
            uint32_t nCurrSize = nBufferSize;
            uint8_t* nCurrBuff = pbBuffer;
            while (nCurrSize >= 5)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01 &&
                        (((nCurrBuff[4] >> 1) & 0x3F) == 19 || ((nCurrBuff[4] >> 1) & 0x3F) == 20))
                {
                    bIntraFrame = true;
                    break;
                }
                nCurrBuff++;
                nCurrSize--;
            }
            break;
        }
        default:
            IMLOGE1("[IsIntraFrame] Invalid video codec type %d", mCodecType);
            return true;
    }

    return bIntraFrame;
}

bool IVideoRendererNode::IsConfigFrame(
        uint8_t* pbBuffer, uint32_t nBufferSize, uint32_t* nBufferOffset)
{
    bool bConfigFrame = false;

    if (nBufferSize <= 4)
        return false;

    IMLOGD_PACKET2(IM_PACKET_LOG_VIDEO, "[IsConfigFrame] size[%d], data[%s]", nBufferSize,
            ImsMediaTrace::IMTrace_Bin2String(
                    reinterpret_cast<const char*>(pbBuffer), nBufferSize > 16 ? 16 : nBufferSize));

    switch (mCodecType)
    {
        case kVideoCodecAvc:
        {
            uint32_t nOffset = 0;
            uint32_t nCurrSize = nBufferSize;
            uint8_t* nCurrBuff = pbBuffer;

            while (nCurrSize >= 5)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01 &&
                        ((nCurrBuff[4] & 0x1F) == 7 || ((nCurrBuff[4] & 0x1F) == 8)))
                {
                    bConfigFrame = true;

                    if (nBufferOffset)
                    {
                        *nBufferOffset = nOffset;
                    }
                    break;
                }

                nOffset++;
                nCurrBuff++;
                nCurrSize--;
            }
            break;
        }
        case kVideoCodecHevc:
        {
            uint32_t nOffset = 0;
            uint32_t nCurrSize = nBufferSize;
            uint8_t* nCurrBuff = pbBuffer;

            while (nCurrSize >= 5)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01 &&
                        (((nCurrBuff[4] >> 1) & 0x3F) == 32 || ((nCurrBuff[4] >> 1) & 0x3F) == 33 ||
                                ((nCurrBuff[4] >> 1) & 0x3F) == 34))
                {
                    bConfigFrame = true;
                    if (nBufferOffset)
                    {
                        *nBufferOffset = nOffset;
                    }
                    break;
                }
                nOffset++;
                nCurrBuff++;
                nCurrSize--;
            }
            break;
        }
        default:
            return false;
    }

    return bConfigFrame;
}

bool IVideoRendererNode::IsSps(uint8_t* pbBuffer, uint32_t nBufferSize, uint32_t* nBufferOffset)
{
    bool bSPS = false;
    if (nBufferSize <= 4)
    {
        return false;
    }

    IMLOGD_PACKET2(IM_PACKET_LOG_VIDEO, "[IsSps] size[%d], data[%s]", nBufferSize,
            ImsMediaTrace::IMTrace_Bin2String(
                    reinterpret_cast<const char*>(pbBuffer), nBufferSize > 16 ? 16 : nBufferSize));

    switch (mCodecType)
    {
        case kVideoCodecAvc:
        {
            uint32_t nOffset = 0;
            uint32_t nCurrSize = nBufferSize;
            uint8_t* nCurrBuff = pbBuffer;

            while (nCurrSize >= 5)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01 && (nCurrBuff[4] & 0x1F) == 7)
                {
                    bSPS = true;

                    if (nBufferOffset)
                    {
                        *nBufferOffset = nOffset;
                    }

                    break;
                }

                nOffset++;
                nCurrBuff++;
                nCurrSize--;
            }

            break;
        }
        case kVideoCodecHevc:
        {
            uint32_t nOffset = 0;
            uint32_t nCurrSize = nBufferSize;
            uint8_t* nCurrBuff = pbBuffer;

            while (nCurrSize >= 5)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01 && ((nCurrBuff[4] >> 1) & 0x3F) == 33)
                {
                    bSPS = true;

                    if (nBufferOffset)
                    {
                        *nBufferOffset = nOffset;
                    }

                    break;
                }

                nOffset++;
                nCurrBuff++;
                nCurrSize--;
            }
            break;
        }
        default:
            return false;
    }

    return bSPS;
}

void IVideoRendererNode::SaveConfigFrame(uint8_t* pbBuffer, uint32_t nBufferSize, uint32_t eMode)
{
    bool bSPSString = false;
    bool bPPSString = false;

    if (nBufferSize <= 4)
    {
        return;
    }

    IMLOGD_PACKET3(IM_PACKET_LOG_VIDEO, "[SaveConfigFrame] mode[%d], size[%d], data[%s]", eMode,
            nBufferSize,
            ImsMediaTrace::IMTrace_Bin2String(
                    reinterpret_cast<const char*>(pbBuffer), nBufferSize > 52 ? 52 : nBufferSize));

    switch (mCodecType)
    {
        case kVideoCodecAvc:
        {
            uint32_t nCurrSize = 0;
            uint32_t nOffset = 0;
            uint32_t nConfigSize = 0;
            uint8_t* nCurrBuff = pbBuffer;

            while (nCurrSize <= nBufferSize)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01)
                {
                    if (eMode == kConfigSps && !bSPSString && ((nCurrBuff[4] & 0x1F) == 7))
                    {
                        nOffset = nCurrSize;
                        bSPSString = true;
                    }
                    else if (eMode == kConfigPps && !bPPSString && ((nCurrBuff[4] & 0x1F) == 8))
                    {
                        nOffset = nCurrSize;
                        bPPSString = true;
                    }
                    else if (bSPSString || bPPSString)
                    {
                        nConfigSize = nCurrSize - nOffset;
                        break;
                    }
                }

                nCurrBuff++;
                nCurrSize++;
            }

            if ((bSPSString || bPPSString) && nConfigSize == 0)
            {
                nConfigSize = nBufferSize - nOffset;
            }

            IMLOGD_PACKET3(IM_PACKET_LOG_VIDEO,
                    "[SaveConfigFrame] AVC Codec - bSps[%d], bPps[%d], size[%d]", bSPSString,
                    bPPSString, nConfigSize);

            // save
            if (bSPSString || bPPSString)
            {
                uint8_t* pConfigData = nullptr;
                uint32_t nConfigIndex = 0;

                if (eMode == kConfigSps)
                {
                    nConfigIndex = 0;
                }
                else if (eMode == kConfigPps)
                {
                    nConfigIndex = 1;
                }
                else
                {
                    return;
                }

                pConfigData = mConfigBuffer[nConfigIndex];

                if (0 != memcmp(pConfigData, pbBuffer + nOffset, nConfigSize))
                {
                    memcpy(pConfigData, pbBuffer + nOffset, nConfigSize);
                    mConfigLen[nConfigIndex] = nConfigSize;
                }
            }
            break;
        }

        case kVideoCodecHevc:
        {
            uint32_t nCurrSize = 0;
            uint32_t nOffset = 0;
            uint32_t nConfigSize = 0;
            uint8_t* nCurrBuff = pbBuffer;
            bool bVPSString = false;

            while (nCurrSize <= nBufferSize)
            {
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01)
                {
                    if (eMode == kConfigVps && !bVPSString && (((nCurrBuff[4] >> 1) & 0x3F) == 32))
                    {
                        nOffset = nCurrSize;
                        bVPSString = true;
                        break;
                    }
                    else if (eMode == kConfigSps && !bSPSString &&
                            (((nCurrBuff[4] >> 1) & 0x3F) == 33))
                    {
                        nOffset = nCurrSize;
                        bSPSString = true;
                        break;
                    }
                    else if (eMode == kConfigPps && !bPPSString &&
                            (((nCurrBuff[4] >> 1) & 0x3F) == 34))
                    {
                        nOffset = nCurrSize;
                        bPPSString = true;
                        break;
                    }
                }

                nCurrBuff++;
                nCurrSize++;
            }

            if (bVPSString || bSPSString || bPPSString)
            {
                if ((nBufferSize - nOffset) > 0)
                {
                    nConfigSize = nBufferSize - nOffset;
                }
            }

            IMLOGD_PACKET4(IM_PACKET_LOG_VIDEO,
                    "[SaveConfigFrame - H265] bVPS[%d], bSPS[%d], bPPS[%d], nConfigSize[%d]",
                    bVPSString, bSPSString, bPPSString, nConfigSize);

            // save
            if (bVPSString || bSPSString || bPPSString)
            {
                uint8_t* pConfigData = nullptr;
                uint32_t nConfigIndex = 0;

                if (eMode == kConfigVps)
                {
                    nConfigIndex = 0;
                }
                else if (eMode == kConfigSps)
                {
                    nConfigIndex = 1;
                }
                else if (eMode == kConfigPps)
                {
                    nConfigIndex = 2;
                }
                else
                {
                    return;
                }

                pConfigData = mConfigBuffer[nConfigIndex];

                if (0 != memcmp(pConfigData, pbBuffer + nOffset, nConfigSize))
                {
                    memcpy(pConfigData, pbBuffer + nOffset, nConfigSize);
                    mConfigLen[nConfigIndex] = nConfigSize;
                }
            }
            break;
        }
        default:
            return;
    }
}

bool IVideoRendererNode::RemoveAUDNalUnit(
        uint8_t* pInBuffer, uint32_t nInBufferSize, uint8_t** ppOutBuffer, uint32_t* pOutBufferSize)
{
    bool bAUDUnit = false;
    *ppOutBuffer = pInBuffer;
    *pOutBufferSize = nInBufferSize;

    if (nInBufferSize <= 4)
    {
        return false;
    }

    switch (mCodecType)
    {
        case kVideoCodecAvc:
        {
            uint32_t nCurrSize = nInBufferSize;
            uint8_t* nCurrBuff = pInBuffer;
            uint32_t nCnt = 0;

            while (nCurrSize >= 5 && nCnt <= 12)
            {
                if (bAUDUnit &&
                        (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                                nCurrBuff[3] == 0x01))
                {
                    *ppOutBuffer = nCurrBuff;
                    *pOutBufferSize = nCurrSize;
                    break;
                }
                if (nCurrBuff[0] == 0x00 && nCurrBuff[1] == 0x00 && nCurrBuff[2] == 0x00 &&
                        nCurrBuff[3] == 0x01 && nCurrBuff[4] == 0x09)
                {
                    bAUDUnit = true;
                }

                nCurrBuff++;
                nCurrSize--;
                nCnt++;
            }
        }
        break;
        case kVideoCodecHevc:
        default:
            return false;
    }

    return bAUDUnit;
}

void IVideoRendererNode::CheckResolution(uint32_t nWidth, uint32_t nHeight)
{
    if ((nWidth != 0 && nWidth != mWidth) || (nHeight != 0 && nHeight != mHeight))
    {
        IMLOGD4("[CheckResolution] resolution change[%dx%d] to [%dx%d]", mWidth, mHeight, nWidth,
                nHeight);
        mWidth = nWidth;
        mHeight = nHeight;

        NotifyPeerDimensionChanged();
    }
}

void IVideoRendererNode::QueueConfigFrame(uint32_t timestamp)
{
    uint32_t nNumOfConfigString = 0;
    if (mCodecType == kVideoCodecAvc)
    {
        nNumOfConfigString = 2;
    }
    else if (mCodecType == kVideoCodecHevc)
    {
        nNumOfConfigString = 3;
    }

    for (int32_t i = 0; i < nNumOfConfigString; i++)
    {
        uint8_t* pConfigData = nullptr;
        uint32_t nConfigLen = mConfigLen[i];
        pConfigData = mConfigBuffer[i];

        if (nConfigLen == 0 || mVideoRenderer == nullptr)
        {
            continue;
        }

        mVideoRenderer->OnDataFrame(pConfigData, nConfigLen, timestamp, true);
    }
}

void IVideoRendererNode::NotifyPeerDimensionChanged()
{
    if (mCallback == nullptr)
    {
        return;
    }

    // assume the device is portrait
    if (mWidth > mHeight)  // landscape
    {
        // local rotation
        if (mDeviceOrientation == 0 || mDeviceOrientation == 180)
        {
            // peer rotation
            if (mSubtype == MEDIASUBTYPE_ROT0 || mSubtype == MEDIASUBTYPE_ROT180)
            {
                mCallback->SendEvent(kImsMediaEventResolutionChanged, mWidth, mHeight);
            }
            else if (mSubtype == MEDIASUBTYPE_ROT90 || mSubtype == MEDIASUBTYPE_ROT270)
            {
                mCallback->SendEvent(kImsMediaEventResolutionChanged, mHeight, mWidth);
            }
        }
        else
        {
            // peer rotation
            if (mSubtype == MEDIASUBTYPE_ROT0 || mSubtype == MEDIASUBTYPE_ROT180)
            {
                mCallback->SendEvent(kImsMediaEventResolutionChanged, mHeight, mWidth);
            }
            else if (mSubtype == MEDIASUBTYPE_ROT90 || mSubtype == MEDIASUBTYPE_ROT270)
            {
                mCallback->SendEvent(kImsMediaEventResolutionChanged, mWidth, mHeight);
            }
        }
    }
    else  // portrait
    {
        // peer rotation
        if (mSubtype == MEDIASUBTYPE_ROT0 || mSubtype == MEDIASUBTYPE_ROT180)
        {
            mCallback->SendEvent(kImsMediaEventResolutionChanged, mWidth, mHeight);
        }
        else if (mSubtype == MEDIASUBTYPE_ROT90 || mSubtype == MEDIASUBTYPE_ROT270)
        {
            mCallback->SendEvent(kImsMediaEventResolutionChanged, mHeight, mWidth);
        }
    }
}