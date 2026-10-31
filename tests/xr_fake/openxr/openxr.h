// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Minimal OpenXR API surface for headless MotionInput tests.
// NOT used by production builds; production/zig builds use the real SDK header.

#include <stdint.h>
#include <windows.h>

typedef uint64_t XrInstance;
typedef uint64_t XrSystemId;
typedef uint64_t XrSession;
typedef uint64_t XrActionSet;
typedef uint64_t XrAction;
typedef uint64_t XrSpace;
typedef uint64_t XrSwapchain;
typedef uint64_t XrPath;
typedef int64_t  XrTime;
typedef int32_t  XrResult;
typedef uint32_t XrBool32;
typedef uint64_t XrFlags64;

#define XR_NULL_HANDLE 0
#define XR_NULL_PATH 0
#define XR_NULL_SYSTEM_ID 0
#define XR_FALSE 0
#define XR_TRUE 1
#define XR_SUCCESS 0
#define XR_ERROR_INSTANCE_LOST  (-6)
#define XR_ERROR_SESSION_LOST   (-37)
#define XR_SUCCEEDED(r) ((r) >= 0)
#define XR_FAILED(r) ((r) < 0)
#define XR_CURRENT_API_VERSION 1
#define XR_MAX_RESULT_STRING_SIZE 64
#define XR_INFINITE_DURATION 0x7fffffffffffffffLL
#define XR_KHR_D3D11_ENABLE_EXTENSION_NAME "XR_KHR_D3D11_enable"
#define XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT 0x00000001
#define XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT     0x00000008
#define XR_VIEW_STATE_ORIENTATION_VALID_BIT     0x00000001

typedef enum XrStructureType {
    XR_TYPE_UNKNOWN = 0,
    XR_TYPE_ACTION_SET_CREATE_INFO = 1,
    XR_TYPE_ACTION_CREATE_INFO = 2,
    XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING = 3,
    XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO = 4,
    XR_TYPE_ACTION_SPACE_CREATE_INFO = 5,
    XR_TYPE_ACTION_STATE_GET_INFO = 6,
    XR_TYPE_ACTION_STATE_FLOAT = 7,
    XR_TYPE_ACTION_STATE_BOOLEAN = 8,
    XR_TYPE_ACTION_STATE_VECTOR2F = 9,
    XR_TYPE_ACTIONS_SYNC_INFO = 10,
    XR_TYPE_SPACE_LOCATION = 11,
    XR_TYPE_SPACE_VELOCITY = 12,
    XR_TYPE_INSTANCE_CREATE_INFO = 20,
    XR_TYPE_SYSTEM_GET_INFO = 21,
    XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR = 22,
    XR_TYPE_GRAPHICS_BINDING_D3D11_KHR = 23,
    XR_TYPE_SESSION_CREATE_INFO = 24,
    XR_TYPE_REFERENCE_SPACE_CREATE_INFO = 25,
    XR_TYPE_SWAPCHAIN_CREATE_INFO = 26,
    XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR = 27,
    XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO = 28,
    XR_TYPE_EVENT_DATA_BUFFER = 29,
    XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED = 30,
    XR_TYPE_SESSION_BEGIN_INFO = 31,
    XR_TYPE_FRAME_WAIT_INFO = 32,
    XR_TYPE_FRAME_STATE = 33,
    XR_TYPE_FRAME_BEGIN_INFO = 34,
    XR_TYPE_VIEW_LOCATE_INFO = 35,
    XR_TYPE_VIEW_STATE = 36,
    XR_TYPE_VIEW = 37,
    XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW = 38,
    XR_TYPE_COMPOSITION_LAYER_PROJECTION = 39,
    XR_TYPE_COMPOSITION_LAYER_QUAD = 40,
    XR_TYPE_FRAME_END_INFO = 41,
    XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING = 42,
    XR_TYPE_HAPTIC_VIBRATION = 43,
    XR_TYPE_HAPTIC_ACTION_INFO = 44
} XrStructureType;

#define XR_FREQUENCY_UNSPECIFIED 0

typedef struct XrHapticBaseHeader { XrStructureType type; const void* next; } XrHapticBaseHeader;
typedef struct XrHapticVibration {
    XrStructureType type; const void* next;
    int64_t duration; float amplitude; float frequency;
} XrHapticVibration;
typedef struct XrHapticActionInfo {
    XrStructureType type; const void* next;
    XrAction action; XrPath subactionPath;
} XrHapticActionInfo;

extern "C" XrResult xrApplyHapticFeedback(XrSession session, const XrHapticActionInfo* hapticActionInfo,
                                          const XrHapticBaseHeader* hapticEvent);

typedef enum XrFormFactor { XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY = 1 } XrFormFactor;
typedef enum XrViewConfigurationType { XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO = 2 } XrViewConfigurationType;
typedef enum XrEnvironmentBlendMode { XR_ENVIRONMENT_BLEND_MODE_OPAQUE = 1 } XrEnvironmentBlendMode;
typedef enum XrSessionState {
    XR_SESSION_STATE_IDLE = 0, XR_SESSION_STATE_READY = 3, XR_SESSION_STATE_STOPPING = 5,
    XR_SESSION_STATE_LOSS_PENDING = 7, XR_SESSION_STATE_EXITING = 8
} XrSessionState;
typedef enum XrReferenceSpaceType { XR_REFERENCE_SPACE_TYPE_LOCAL = 2 } XrReferenceSpaceType;
typedef enum XrEyeVisibility { XR_EYE_VISIBILITY_BOTH = 3 } XrEyeVisibility;
typedef void (*PFN_xrVoidFunction)(void);

typedef enum XrActionType {
    XR_ACTION_TYPE_BOOLEAN_INPUT = 1,
    XR_ACTION_TYPE_FLOAT_INPUT = 2,
    XR_ACTION_TYPE_VECTOR2F_INPUT = 3,
    XR_ACTION_TYPE_POSE_INPUT = 100,
    XR_ACTION_TYPE_VIBRATION_OUTPUT = 101
} XrActionType;

typedef struct XrVector2f { float x; float y; } XrVector2f;
typedef struct XrVector3f { float x; float y; float z; } XrVector3f;
typedef struct XrQuaternionf { float x; float y; float z; float w; } XrQuaternionf;
typedef struct XrPosef { XrQuaternionf orientation; XrVector3f position; } XrPosef;

typedef struct XrActionSetCreateInfo {
    XrStructureType type; const void* next;
    char actionSetName[64]; char localizedActionSetName[128]; uint32_t priority;
} XrActionSetCreateInfo;

typedef struct XrActionCreateInfo {
    XrStructureType type; const void* next;
    char actionName[64]; char localizedActionName[128];
    XrActionType actionType; uint32_t countSubactionPaths; const XrPath* subactionPaths;
} XrActionCreateInfo;

typedef struct XrActionSuggestedBinding { XrAction action; XrPath binding; } XrActionSuggestedBinding;
typedef struct XrInteractionProfileSuggestedBinding {
    XrStructureType type; const void* next; XrPath interactionProfile;
    uint32_t countSuggestedBindings; const XrActionSuggestedBinding* suggestedBindings;
} XrInteractionProfileSuggestedBinding;

typedef struct XrSessionActionSetsAttachInfo {
    XrStructureType type; const void* next; uint32_t countActionSets; const XrActionSet* actionSets;
} XrSessionActionSetsAttachInfo;

typedef struct XrActionSpaceCreateInfo {
    XrStructureType type; const void* next; XrAction action; XrPath subactionPath; XrPosef poseInActionSpace;
} XrActionSpaceCreateInfo;

typedef struct XrActiveActionSet { XrActionSet actionSet; XrPath subactionPath; } XrActiveActionSet;
typedef struct XrActionsSyncInfo {
    XrStructureType type; const void* next; uint32_t countActiveActionSets; const XrActiveActionSet* activeActionSets;
} XrActionsSyncInfo;

typedef struct XrActionStateGetInfo {
    XrStructureType type; const void* next; XrAction action; XrPath subactionPath;
} XrActionStateGetInfo;

typedef struct XrActionStateFloat {
    XrStructureType type; void* next; float currentState; XrBool32 changedSinceLastSync; XrTime lastChangeTime; XrBool32 isActive;
} XrActionStateFloat;
typedef struct XrActionStateBoolean {
    XrStructureType type; void* next; XrBool32 currentState; XrBool32 changedSinceLastSync; XrTime lastChangeTime; XrBool32 isActive;
} XrActionStateBoolean;
typedef struct XrActionStateVector2f {
    XrStructureType type; void* next; XrVector2f currentState; XrBool32 changedSinceLastSync; XrTime lastChangeTime; XrBool32 isActive;
} XrActionStateVector2f;

#define XR_SPACE_LOCATION_POSITION_VALID_BIT 0x00000001
#define XR_SPACE_LOCATION_ORIENTATION_VALID_BIT 0x00000002
#define XR_SPACE_VELOCITY_LINEAR_VALID_BIT 0x00000001
#define XR_SPACE_VELOCITY_ANGULAR_VALID_BIT 0x00000002

typedef struct XrSpaceLocation {
    XrStructureType type; void* next; XrFlags64 locationFlags; XrPosef pose;
} XrSpaceLocation;
typedef struct XrSpaceVelocity {
    XrStructureType type; void* next; XrFlags64 velocityFlags; XrVector3f linearVelocity; XrVector3f angularVelocity;
} XrSpaceVelocity;

// --- instance/session/frame structs used by OpenXRBridge compile gate -------
typedef struct XrApplicationInfo {
    char applicationName[128]; uint32_t applicationVersion;
    char engineName[128]; uint32_t engineVersion; uint64_t apiVersion;
} XrApplicationInfo;
typedef struct XrInstanceCreateInfo {
    XrStructureType type; const void* next; uint64_t createFlags; XrApplicationInfo applicationInfo;
    uint32_t enabledApiLayerCount; const char* const* enabledApiLayerNames;
    uint32_t enabledExtensionCount; const char* const* enabledExtensionNames;
} XrInstanceCreateInfo;
typedef struct XrSystemGetInfo {
    XrStructureType type; const void* next; uint64_t createFlags; XrFormFactor formFactor;
} XrSystemGetInfo;

struct ID3D11Device;
struct ID3D11Texture2D;
typedef struct XrGraphicsRequirementsD3D11KHR {
    XrStructureType type; void* next; LUID adapterLuid; uint32_t minFeatureLevel;
} XrGraphicsRequirementsD3D11KHR;
typedef struct XrGraphicsBindingD3D11KHR {
    XrStructureType type; const void* next; ID3D11Device* device;
} XrGraphicsBindingD3D11KHR;
typedef struct XrSessionCreateInfo {
    XrStructureType type; const void* next; uint64_t createFlags; XrSystemId systemId;
} XrSessionCreateInfo;
typedef struct XrReferenceSpaceCreateInfo {
    XrStructureType type; const void* next; XrReferenceSpaceType referenceSpaceType; XrPosef poseInReferenceSpace;
} XrReferenceSpaceCreateInfo;
typedef struct XrSwapchainCreateInfo {
    XrStructureType type; const void* next; uint64_t createFlags; uint64_t usageFlags;
    int64_t format; uint32_t sampleCount; uint32_t width; uint32_t height;
    uint32_t faceCount; uint32_t arraySize; uint32_t mipCount;
} XrSwapchainCreateInfo;
typedef struct XrSwapchainImageBaseHeader { XrStructureType type; void* next; } XrSwapchainImageBaseHeader;
typedef struct XrSwapchainImageD3D11KHR {
    XrStructureType type; void* next; ID3D11Texture2D* texture;
} XrSwapchainImageD3D11KHR;
typedef struct XrSwapchainImageAcquireInfo { XrStructureType type; const void* next; } XrSwapchainImageAcquireInfo;
typedef struct XrSwapchainImageWaitInfo { XrStructureType type; const void* next; XrTime timeout; } XrSwapchainImageWaitInfo;
typedef struct XrSwapchainImageReleaseInfo { XrStructureType type; const void* next; } XrSwapchainImageReleaseInfo;

typedef struct XrEventDataBuffer { XrStructureType type; const void* next; char payload[4096]; } XrEventDataBuffer;
typedef struct XrEventDataSessionStateChanged {
    XrStructureType type; const void* next; XrSession session; XrSessionState state; XrTime time;
} XrEventDataSessionStateChanged;
typedef struct XrSessionBeginInfo {
    XrStructureType type; const void* next; XrViewConfigurationType primaryViewConfigurationType;
} XrSessionBeginInfo;
typedef struct XrFrameWaitInfo { XrStructureType type; const void* next; } XrFrameWaitInfo;
typedef struct XrFrameState {
    XrStructureType type; void* next; XrTime predictedDisplayTime; int64_t predictedDisplayPeriod; uint32_t shouldRender;
} XrFrameState;
typedef struct XrFrameBeginInfo { XrStructureType type; const void* next; } XrFrameBeginInfo;
typedef struct XrFovf { float angleLeft; float angleRight; float angleUp; float angleDown; } XrFovf;
typedef struct XrViewLocateInfo {
    XrStructureType type; const void* next; XrViewConfigurationType viewConfigurationType; XrTime displayTime; XrSpace space;
} XrViewLocateInfo;
typedef struct XrViewState { XrStructureType type; void* next; XrFlags64 viewStateFlags; } XrViewState;
typedef struct XrView {
    XrStructureType type; void* next; XrPosef pose; XrFovf fov;
} XrView;
typedef struct XrOffset2Di { int32_t x; int32_t y; } XrOffset2Di;
typedef struct XrExtent2Di { int32_t width; int32_t height; } XrExtent2Di;
typedef struct XrRect2Di { XrOffset2Di offset; XrExtent2Di extent; } XrRect2Di;
typedef struct XrSwapchainSubImage {
    XrSwapchain swapchain; XrRect2Di imageRect; uint32_t imageArrayIndex;
} XrSwapchainSubImage;
typedef struct XrCompositionLayerBaseHeader {
    XrStructureType type; const void* next; uint64_t layerFlags; XrSpace space;
} XrCompositionLayerBaseHeader;
typedef struct XrCompositionLayerProjectionView {
    XrStructureType type; const void* next; XrPosef pose; XrFovf fov; XrSwapchainSubImage subImage;
} XrCompositionLayerProjectionView;
typedef struct XrCompositionLayerProjection {
    XrStructureType type; const void* next; uint64_t layerFlags; XrSpace space;
    uint32_t viewCount; const XrCompositionLayerProjectionView* views;
} XrCompositionLayerProjection;
typedef struct XrExtent2Df { float width; float height; } XrExtent2Df;
typedef struct XrCompositionLayerQuad {
    XrStructureType type; const void* next; uint64_t layerFlags; XrSpace space;
    XrEyeVisibility eyeVisibility; XrSwapchainSubImage subImage; XrPosef pose; XrExtent2Df size;
} XrCompositionLayerQuad;
typedef struct XrFrameEndInfo {
    XrStructureType type; const void* next; XrTime displayTime; XrEnvironmentBlendMode environmentBlendMode;
    uint32_t layerCount; const XrCompositionLayerBaseHeader* const* layers;
} XrFrameEndInfo;

#ifdef __cplusplus
extern "C" {
#endif
XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* outPath);
XrResult xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet);
XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action);
XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings);
XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo);
XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space);
XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo);
XrResult xrGetActionStateFloat(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateFloat* state);
XrResult xrGetActionStateBoolean(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateBoolean* state);
XrResult xrGetActionStateVector2f(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStateVector2f* state);
XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location);
XrResult xrDestroySpace(XrSpace space);
XrResult xrDestroyActionSet(XrActionSet actionSet);
XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo, XrInstance* instance);
XrResult xrDestroyInstance(XrInstance instance);
XrResult xrResultToString(XrInstance instance, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE]);
XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId);
XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session);
XrResult xrDestroySession(XrSession session);
XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space);
XrResult xrEnumerateEnvironmentBlendModes(XrInstance instance, XrSystemId systemId, XrViewConfigurationType viewConfigurationType, uint32_t capacityInput, uint32_t* countOutput, XrEnvironmentBlendMode* modes);
XrResult xrCreateSwapchain(XrSession session, const XrSwapchainCreateInfo* createInfo, XrSwapchain* swapchain);
XrResult xrDestroySwapchain(XrSwapchain swapchain);
XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain, uint32_t capacityInput, uint32_t* countOutput, XrSwapchainImageBaseHeader* images);
XrResult xrAcquireSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageAcquireInfo* acquireInfo, uint32_t* index);
XrResult xrWaitSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageWaitInfo* waitInfo);
XrResult xrReleaseSwapchainImage(XrSwapchain swapchain, const XrSwapchainImageReleaseInfo* releaseInfo);
XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* eventData);
XrResult xrBeginSession(XrSession session, const XrSessionBeginInfo* beginInfo);
XrResult xrEndSession(XrSession session);
XrResult xrWaitFrame(XrSession session, const XrFrameWaitInfo* waitInfo, XrFrameState* state);
XrResult xrBeginFrame(XrSession session, const XrFrameBeginInfo* beginInfo);
XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* endInfo);
XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* locateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views);
typedef XrResult (*PFN_xrGetD3D11GraphicsRequirementsKHR)(XrInstance, XrSystemId, XrGraphicsRequirementsD3D11KHR*);
#ifdef __cplusplus
}
#endif
