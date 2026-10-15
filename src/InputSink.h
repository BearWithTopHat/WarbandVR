// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// InputSink: abstraction over OS input injection (SendInput). Production
// installs SendInputSink (MotionInput.cpp); headless tests install a
// recording fake. MotionInput's layers only ever talk to this interface,
// so injection sequences are assertable without Windows.

struct IInputSink
{
    virtual ~IInputSink() {}
    virtual void Key(int vk, bool down) = 0;
    virtual void MouseButton(bool left, bool down) = 0;
    virtual void MouseMove(int dx, int dy) = 0;
};

namespace InputSink
{
    // Install/retrieve the active sink. Never null in production after
    // MotionInput init; Install(nullptr) restores the null (drop) sink.
    void Install(IInputSink* sink);
    IInputSink& Get();
}
