// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Test control surface for the OpenXR action/space fake.

#include <string>
#include <vector>

namespace XrFake
{
    enum Hand { Left = 0, Right = 1 };

    void Reset();

    void SetFloat(const char* action, Hand hand, float value, bool active = true);
    void SetBool(const char* action, Hand hand, bool value, bool active = true);
    void SetVec2(const char* action, Hand hand, float x, float y, bool active = true);
    void SetPose(Hand hand, float x, float y, float z, bool valid = true);
    void SetVelocity(Hand hand, float x, float y, float z, bool valid = true);

    void SetSyncResult(int xrResult);       // default XR_SUCCESS
    void SetLocateResult(int xrResult);     // default XR_SUCCESS

    int SyncCount();
    int AttachedSets();
    int SuggestedBindings();
    int DestroyedSpaces();
    int DestroyedActionSets();
    bool HasAction(const char* name);
    std::string ActionsCreated();
    int HapticCount();
    float LastHapticAmplitude();
    long long LastHapticDuration();
    int LastHapticHand();
}
