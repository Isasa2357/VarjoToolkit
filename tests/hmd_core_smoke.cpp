#include "hmd_external_frame_sync.hpp"

#include <stdexcept>

namespace {

void runCoreSmokeTest()
{
    std::cout << "Checking Varjo runtime availability...\n";
    VTK_HMD_TEST_REQUIRE(VarjoSession::runtimeAvailable());

    VarjoSession session;
    VTK_HMD_TEST_REQUIRE(session.valid());
    VTK_HMD_TEST_REQUIRE(session.get() != nullptr);
    VTK_HMD_TEST_REQUIRE(static_cast<bool>(session.shared()));
    VTK_HMD_TEST_REQUIRE(session.lastError().empty());

    const auto currentTime = session.currentTime();
    const int32_t sessionViewCount = session.viewCount();
    std::cout << "Session currentTime=" << currentTime
              << " viewCount=" << sessionViewCount << '\n';
    VTK_HMD_TEST_REQUIRE(sessionViewCount > 0);

    VarjoFrameInfo frameInfo(session);
    VTK_HMD_TEST_REQUIRE(frameInfo.valid());
    VTK_HMD_TEST_REQUIRE(frameInfo.session() == session.get());
    VTK_HMD_TEST_REQUIRE(frameInfo.ownsSession());
    VTK_HMD_TEST_REQUIRE(static_cast<bool>(frameInfo.sharedSession()));

    VTK_HMD_TEST_REQUIRE(
        vtk_hmd_test::waitSyncExternally(session, frameInfo));
    VTK_HMD_TEST_REQUIRE(frameInfo.get() != nullptr);
    VTK_HMD_TEST_REQUIRE(frameInfo.viewCount() == sessionViewCount);
    VTK_HMD_TEST_REQUIRE(frameInfo.views() != nullptr);
    VTK_HMD_TEST_REQUIRE(frameInfo.frameNumber() >= 0);
    VTK_HMD_TEST_REQUIRE(frameInfo.displayTime() > 0);

    for (int32_t viewIndex = 0;
         viewIndex < frameInfo.viewCount();
         ++viewIndex) {
        const auto& view = frameInfo.view(viewIndex);
        std::cout << "View " << viewIndex
                  << " preferred=" << view.preferredWidth
                  << 'x' << view.preferredHeight << '\n';
        VTK_HMD_TEST_REQUIRE(view.preferredWidth > 0);
        VTK_HMD_TEST_REQUIRE(view.preferredHeight > 0);
        VTK_HMD_TEST_REQUIRE(
            vtk_hmd_test::isFiniteMatrix(view.projectionMatrix));
        VTK_HMD_TEST_REQUIRE(
            vtk_hmd_test::isFiniteMatrix(view.viewMatrix));
    }

    const auto snapshot = frameInfo.snapshot();
    VTK_HMD_TEST_REQUIRE(snapshot.valid);
    VTK_HMD_TEST_REQUIRE(snapshot.centerPoseValid);
    VTK_HMD_TEST_REQUIRE(snapshot.frameNumber == frameInfo.frameNumber());
    VTK_HMD_TEST_REQUIRE(snapshot.displayTime == frameInfo.displayTime());
    VTK_HMD_TEST_REQUIRE(
        snapshot.views.size() == static_cast<size_t>(frameInfo.viewCount()));

    bool outOfRangeThrew = false;
    try {
        (void)frameInfo.view(frameInfo.viewCount());
    } catch (const std::out_of_range&) {
        outOfRangeThrew = true;
    }
    VTK_HMD_TEST_REQUIRE(outOfRangeThrew);

    VarjoFrameInfo movedFrameInfo(std::move(frameInfo));
    VTK_HMD_TEST_REQUIRE(movedFrameInfo.valid());
    VTK_HMD_TEST_REQUIRE(!frameInfo.valid());
    VTK_HMD_TEST_REQUIRE(movedFrameInfo.session() == session.get());

    VarjoFrameInfo invalidFrameInfo(static_cast<varjo_Session*>(nullptr));
    VTK_HMD_TEST_REQUIRE(!invalidFrameInfo.valid());
    VTK_HMD_TEST_REQUIRE(!vtk_hmd_test::waitSyncExternally(
        session,
        invalidFrameInfo));
    VTK_HMD_TEST_REQUIRE(invalidFrameInfo.viewCount() == 0);
    VTK_HMD_TEST_REQUIRE(invalidFrameInfo.views() == nullptr);
    VTK_HMD_TEST_REQUIRE(!invalidFrameInfo.snapshot().valid);
}

} // namespace

int main()
{
    return vtk_hmd_test::runTest(
        "VarjoToolkitHmdCoreSmokeTest",
        runCoreSmokeTest);
}
