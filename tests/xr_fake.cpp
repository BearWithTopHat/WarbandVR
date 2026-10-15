// SPDX-License-Identifier: GPL-3.0-only
// OpenXR action/space fake for headless MotionInput tests.
#include "xr_fake.h"
#include <openxr/openxr.h>
#include <cstring>
#include <map>
#include <vector>
#include <sstream>

// The winshim deterministic clock (linked into the motion-input test
// binary): tests set it via WinShimSetTickCount64, and the fake stamps
// action change times from it so runtime-timestamp behavior is testable.
extern "C" uint64_t GetTickCount64();

namespace
{
    struct FloatState { float v = 0; bool active = true; XrTime changeTime = 0; };
    struct BoolState { bool v = false; bool active = true; XrTime changeTime = 0; };
    struct Vec2State { float x = 0, y = 0; bool active = true; XrTime changeTime = 0; };
    struct PoseState { float p[3] = {}; bool valid = false; };
    struct VelState { float v[3] = {}; bool valid = false; };

    struct ActionRec
    {
        XrAction handle = 0;
        std::string name;
        XrActionType type = XR_ACTION_TYPE_BOOLEAN_INPUT;
        XrPath sub[2] = { XR_NULL_PATH, XR_NULL_PATH };
    };

    std::map<std::string, XrPath> g_paths;
    std::map<XrPath, std::string> g_pathNames;
    XrPath g_nextPath = 100;
    XrActionSet g_nextSet = 1000;
    XrAction g_nextAction = 2000;
    XrSpace g_nextSpace = 3000;
    std::vector<ActionRec> g_actions;
    std::map<XrSpace, int> g_spaces; // hand index

    std::map<std::string, FloatState> g_floats;
    std::map<std::string, BoolState> g_bools;
    std::map<std::string, Vec2State> g_vec2;
    PoseState g_pose[2];
    VelState g_vel[2];

    XrResult g_syncResult = XR_SUCCESS;
    XrResult g_locateResult = XR_SUCCESS;
    int g_syncCount = 0;
    int g_attachedSets = 0;
    int g_suggestedBindings = 0;
    int g_destroyedSpaces = 0;
    int g_destroyedSets = 0;

    std::string Key(XrAction action, XrPath sub)
    {
        for (const ActionRec& a : g_actions)
            if (a.handle == action)
                return a.name + "|" + g_pathNames[sub];
        return "";
    }

    XrPath PathFor(const char* s)
    {
        std::string str = s ? s : "";
        auto it = g_paths.find(str);
        if (it != g_paths.end()) return it->second;
        XrPath p = g_nextPath++;
        g_paths[str] = p;
        g_pathNames[p] = str;
        return p;
    }

    int HandFromSub(XrPath p)
    {
        const std::string& s = g_pathNames[p];
        if (s.find("/user/hand/left") == 0) return 0;
        if (s.find("/user/hand/right") == 0) return 1;
        return -1;
    }
}

namespace XrFake
{
    void Reset()
    {
        g_paths.clear(); g_pathNames.clear();
        g_nextPath = 100; g_nextSet = 1000; g_nextAction = 2000; g_nextSpace = 3000;
        g_actions.clear(); g_spaces.clear();
        g_floats.clear(); g_bools.clear(); g_vec2.clear();
        g_pose[0] = PoseState{}; g_pose[1] = PoseState{};
        g_vel[0] = VelState{}; g_vel[1] = VelState{};
        g_syncResult = XR_SUCCESS; g_locateResult = XR_SUCCESS;
        g_syncCount = 0; g_attachedSets = 0; g_suggestedBindings = 0;
        g_destroyedSpaces = 0; g_destroyedSets = 0;
    }

    static std::string StateKey(const char* action, Hand hand)
    {
        return std::string(action) + (hand == Left ? "|/user/hand/left" : "|/user/hand/right");
    }
    static XrTime NowNs() { return (XrTime)GetTickCount64() * 1000000LL; }
    void SetFloat(const char* a, Hand h, float v, bool active)
    {
        auto& s = g_floats[StateKey(a,h)];
        if (s.v != v) s.changeTime = NowNs();
        s.v = v; s.active = active;
    }
    void SetBool(const char* a, Hand h, bool v, bool active)
    {
        auto& s = g_bools[StateKey(a,h)];
        if (s.v != v) s.changeTime = NowNs();
        s.v = v; s.active = active;
    }
    void SetVec2(const char* a, Hand h, float x, float y, bool active)
    {
        auto& s = g_vec2[StateKey(a,h)];
        if (s.x != x || s.y != y) s.changeTime = NowNs();
        s.x = x; s.y = y; s.active = active;
    }
    void SetPose(Hand h, float x, float y, float z, bool valid)
    { g_pose[h].p[0]=x; g_pose[h].p[1]=y; g_pose[h].p[2]=z; g_pose[h].valid=valid; }
    void SetVelocity(Hand h, float x, float y, float z, bool valid)
    { g_vel[h].v[0]=x; g_vel[h].v[1]=y; g_vel[h].v[2]=z; g_vel[h].valid=valid; }

    void SetSyncResult(int r) { g_syncResult = (XrResult)r; }
    void SetLocateResult(int r) { g_locateResult = (XrResult)r; }
    int SyncCount() { return g_syncCount; }
    int AttachedSets() { return g_attachedSets; }
    int SuggestedBindings() { return g_suggestedBindings; }
    int DestroyedSpaces() { return g_destroyedSpaces; }
    int DestroyedActionSets() { return g_destroyedSets; }
    bool HasAction(const char* name)
    {
        for (const ActionRec& a : g_actions) if (a.name == name) return true;
        return false;
    }
    std::string ActionsCreated()
    {
        std::ostringstream ss;
        for (size_t i=0;i<g_actions.size();++i) { if (i) ss << ","; ss << g_actions[i].name; }
        return ss.str();
    }
}

extern "C" XrResult xrStringToPath(XrInstance, const char* pathString, XrPath* outPath)
{
    if (!pathString || !outPath) return -1;
    *outPath = PathFor(pathString);
    return XR_SUCCESS;
}

extern "C" XrResult xrCreateActionSet(XrInstance, const XrActionSetCreateInfo*, XrActionSet* actionSet)
{
    if (!actionSet) return -1;
    *actionSet = g_nextSet++;
    return XR_SUCCESS;
}

extern "C" XrResult xrCreateAction(XrActionSet set, const XrActionCreateInfo* ci, XrAction* action)
{
    if (!set || !ci || !action || !ci->subactionPaths || ci->countSubactionPaths != 2) return -1;
    ActionRec a;
    a.handle = g_nextAction++;
    a.name = ci->actionName;
    a.type = ci->actionType;
    a.sub[0] = ci->subactionPaths[0];
    a.sub[1] = ci->subactionPaths[1];
    g_actions.push_back(a);
    *action = a.handle;
    return XR_SUCCESS;
}

extern "C" XrResult xrSuggestInteractionProfileBindings(XrInstance, const XrInteractionProfileSuggestedBinding* sb)
{
    if (!sb) return -1;
    g_suggestedBindings += (int)sb->countSuggestedBindings;
    return XR_SUCCESS;
}

extern "C" XrResult xrAttachSessionActionSets(XrSession, const XrSessionActionSetsAttachInfo* ai)
{
    if (!ai || ai->countActionSets == 0) return -1;
    g_attachedSets += (int)ai->countActionSets;
    return XR_SUCCESS;
}

extern "C" XrResult xrCreateActionSpace(XrSession, const XrActionSpaceCreateInfo* ci, XrSpace* space)
{
    if (!ci || !space) return -1;
    int hand = HandFromSub(ci->subactionPath);
    if (hand < 0) return -2;
    *space = g_nextSpace++;
    g_spaces[*space] = hand;
    return XR_SUCCESS;
}

extern "C" XrResult xrSyncActions(XrSession, const XrActionsSyncInfo* si)
{
    if (!si || si->countActiveActionSets == 0) return -1;
    ++g_syncCount;
    return g_syncResult;
}

extern "C" XrResult xrGetActionStateFloat(XrSession, const XrActionStateGetInfo* gi, XrActionStateFloat* st)
{
    if (!gi || !st) return -1;
    auto it = g_floats.find(Key(gi->action, gi->subactionPath));
    if (it == g_floats.end()) { st->isActive = XR_FALSE; st->currentState = 0; st->lastChangeTime = 0; return XR_SUCCESS; }
    st->currentState = it->second.v;
    st->isActive = it->second.active ? XR_TRUE : XR_FALSE;
    st->lastChangeTime = it->second.changeTime;
    return XR_SUCCESS;
}

extern "C" XrResult xrGetActionStateBoolean(XrSession, const XrActionStateGetInfo* gi, XrActionStateBoolean* st)
{
    if (!gi || !st) return -1;
    auto it = g_bools.find(Key(gi->action, gi->subactionPath));
    if (it == g_bools.end()) { st->isActive = XR_FALSE; st->currentState = XR_FALSE; st->lastChangeTime = 0; return XR_SUCCESS; }
    st->currentState = it->second.v ? XR_TRUE : XR_FALSE;
    st->isActive = it->second.active ? XR_TRUE : XR_FALSE;
    st->lastChangeTime = it->second.changeTime;
    return XR_SUCCESS;
}

extern "C" XrResult xrGetActionStateVector2f(XrSession, const XrActionStateGetInfo* gi, XrActionStateVector2f* st)
{
    if (!gi || !st) return -1;
    auto it = g_vec2.find(Key(gi->action, gi->subactionPath));
    if (it == g_vec2.end()) { st->isActive = XR_FALSE; st->currentState = {0,0}; st->lastChangeTime = 0; return XR_SUCCESS; }
    st->currentState.x = it->second.x;
    st->currentState.y = it->second.y;
    st->isActive = it->second.active ? XR_TRUE : XR_FALSE;
    st->lastChangeTime = it->second.changeTime;
    return XR_SUCCESS;
}

extern "C" XrResult xrLocateSpace(XrSpace space, XrSpace, XrTime, XrSpaceLocation* loc)
{
    if (!loc) return -1;
    if (g_locateResult != XR_SUCCESS) return g_locateResult;
    auto it = g_spaces.find(space);
    if (it == g_spaces.end()) return -2;
    int h = it->second;
    loc->locationFlags = g_pose[h].valid ? XR_SPACE_LOCATION_POSITION_VALID_BIT : 0;
    loc->pose.orientation.w = 1.0f;
    loc->pose.position.x = g_pose[h].p[0];
    loc->pose.position.y = g_pose[h].p[1];
    loc->pose.position.z = g_pose[h].p[2];
    if (loc->next)
    {
        XrSpaceVelocity* v = (XrSpaceVelocity*)loc->next;
        if (v->type == XR_TYPE_SPACE_VELOCITY)
        {
            v->velocityFlags = g_vel[h].valid ? XR_SPACE_VELOCITY_LINEAR_VALID_BIT : 0;
            v->linearVelocity.x = g_vel[h].v[0];
            v->linearVelocity.y = g_vel[h].v[1];
            v->linearVelocity.z = g_vel[h].v[2];
        }
    }
    return XR_SUCCESS;
}

extern "C" XrResult xrDestroySpace(XrSpace space)
{
    if (g_spaces.erase(space)) ++g_destroyedSpaces;
    return XR_SUCCESS;
}

extern "C" XrResult xrDestroyActionSet(XrActionSet)
{
    ++g_destroyedSets;
    return XR_SUCCESS;
}
