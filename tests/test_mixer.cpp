#include <gtest/gtest.h>
#include "../include/core/AudioMixer.h"
#include "../include/core/PriorityChannel.h"
#include "../include/core/BackgroundChannel.h"

TEST(AudioMixerTest, MasterVolumeDefaultIsOne) {
    AudioMixer mixer;
    EXPECT_FLOAT_EQ(mixer.getMasterVolume(), 1.0f);
}

TEST(AudioMixerTest, SetMasterVolumeClamps) {
    AudioMixer mixer;
    mixer.setMasterVolume(1.5f);
    EXPECT_LE(mixer.getMasterVolume(), 1.0f);
    mixer.setMasterVolume(-0.5f);
    EXPECT_GE(mixer.getMasterVolume(), 0.0f);
}

TEST(ThreadSafeQueueTest, PushAndPop) {
    ThreadSafeQueue<int> q;
    q.push(42);
    auto val = q.tryPop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}

TEST(ThreadSafeQueueTest, EmptyReturnsNullopt) {
    ThreadSafeQueue<int> q;
    auto val = q.tryPop();
    EXPECT_FALSE(val.has_value());
}

TEST(DuckingTest, BackgroundChannelDucksVolume) {
    auto bg = std::make_shared<BackgroundChannel>("bg1", "Music");
    bg->setVolume(1.0f);
    bg->duck(0.2f, 0);
    EXPECT_FLOAT_EQ(bg->getDuckedVolume(), 0.2f);
}
