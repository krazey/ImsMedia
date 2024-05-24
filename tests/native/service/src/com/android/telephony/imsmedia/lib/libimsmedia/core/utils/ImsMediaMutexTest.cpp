#include <gtest/gtest.h>
#include <ImsMediaMutex.h>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

const std::chrono::duration<int32_t, std::milli> kTimeout = 100ms;

class ImsMediaMutexTest : public ::testing::Test
{
public:
    bool mCallbackCalled;
    ImsMediaMutex mMutex;

    virtual void SetUp() override
    {
        mCallbackCalled = false;
        mMutex.setCallback(errorCallback, this);
        mMutex.setTimeout(kTimeout);
    }
    virtual void TearDown() override {}

    static void errorCallback(void* user)
    {
        ImsMediaMutexTest* test = reinterpret_cast<ImsMediaMutexTest*>(user);
        test->mCallbackCalled = true;
    }

    void job()
    {
        mMutex.lock();
        std::this_thread::sleep_for(150ms);
        mMutex.unlock();
    }
};

TEST_F(ImsMediaMutexTest, testLockTimeout)
{
    for (int32_t i = 0; i < 2; ++i)
    {
        std::thread testThread(&ImsMediaMutexTest::job, this);
        testThread.detach();
        std::this_thread::sleep_for(10ms);
    }

    std::this_thread::sleep_for(300ms);
    EXPECT_TRUE(mCallbackCalled);
}
