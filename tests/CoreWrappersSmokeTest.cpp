#include <VarjoToolkit/Core/VarjoFrameInfo.hpp>
#include <VarjoToolkit/Core/VarjoSession.hpp>

#include <iostream>
#include <utility>

namespace {

int fail(const char* message)
{
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

} // namespace

int main()
{
    std::cout << "VarjoToolkit core wrapper smoke test\n";

    if (!VarjoSession::runtimeAvailable()) {
        return fail("Varjo runtime is not available. Start Varjo Base and connect a Varjo HMD.");
    }

    VarjoSession session;
    if (!session.valid()) {
        std::cerr << "lastError=" << session.lastError() << "\n";
        return fail("VarjoSession failed to initialize");
    }

    if (!session.get()) {
        return fail("VarjoSession::get returned null");
    }

    const auto shared_session = session.shared();
    if (!shared_session || shared_session.get() != session.get()) {
        return fail("VarjoSession::shared returned an unexpected pointer");
    }

    const int32_t view_count = session.viewCount();
    std::cout << "viewCount=" << view_count << "\n";
    if (view_count <= 0) {
        return fail("varjo_GetViewCount returned no views");
    }

    const varjo_Nanoseconds now = session.currentTime();
    std::cout << "currentTime=" << now << "\n";
    if (now <= 0) {
        return fail("VarjoSession::currentTime returned non-positive timestamp");
    }

    VarjoFrameInfo frame_info(session);
    if (!frame_info.valid()) {
        return fail("VarjoFrameInfo failed to create varjo_FrameInfo");
    }

    if (frame_info.viewCount() != view_count) {
        return fail("VarjoFrameInfo::viewCount does not match VarjoSession::viewCount");
    }

    if (!frame_info.waitSync()) {
        return fail("VarjoFrameInfo::waitSync failed");
    }

    const auto snapshot = frame_info.snapshot();
    std::cout << "frameNumber=" << snapshot.frameNumber
              << " displayTime=" << snapshot.displayTime
              << " snapshotViews=" << snapshot.views.size() << "\n";

    if (!snapshot.valid) {
        return fail("VarjoFrameInfo::snapshot returned invalid snapshot");
    }
    if (snapshot.views.size() != static_cast<size_t>(view_count)) {
        return fail("VarjoFrameInfoSnapshot view count mismatch");
    }
    if (snapshot.displayTime <= 0) {
        return fail("VarjoFrameInfoSnapshot displayTime is non-positive");
    }

    const auto& first_view = frame_info.view(0);
    if (first_view.preferredWidth <= 0 || first_view.preferredHeight <= 0) {
        return fail("first view preferred size is invalid");
    }

    VarjoFrameInfo moved_frame_info(std::move(frame_info));
    if (!moved_frame_info.valid()) {
        return fail("VarjoFrameInfo move constructor did not preserve ownership");
    }
    if (frame_info.valid()) {
        return fail("moved-from VarjoFrameInfo still reports valid ownership");
    }

    const auto moved_snapshot = moved_frame_info.snapshot();
    if (!moved_snapshot.valid || moved_snapshot.views.size() != static_cast<size_t>(view_count)) {
        return fail("moved VarjoFrameInfo snapshot is invalid");
    }

    std::cout << "[PASS] VarjoSession and VarjoFrameInfo smoke test passed\n";
    return 0;
}
