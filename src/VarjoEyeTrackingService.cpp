﻿#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <VarjoServices/EyeTracking/VarjoEyeTrackingService.hpp>

#include <Windows.h>

#include <array>
#include <algorithm>
#include <ostream>
#include <chrono>
#include <cmath>
#include <limits>

namespace {
	struct Vec4 {
		double x;
		double y;
		double z;
		double w;
	};

	Vec4 mulMat4Vec4(const double mat[16], const Vec4& v);

	struct Vec3 {
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
	};


	Vec3 addVec3(const Vec3& a, const Vec3& b) { return Vec3{ a.x + b.x, a.y + b.y, a.z + b.z }; }
	Vec3 subVec3(const Vec3& a, const Vec3& b) { return Vec3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
	Vec3 mulVec3(const Vec3& v, double s) { return Vec3{ v.x * s, v.y * s, v.z * s }; }
	double dotVec3(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	Vec3 crossVec3(const Vec3& a, const Vec3& b) {
		return Vec3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
	}
	double lengthVec3(const Vec3& v) { return std::sqrt(dotVec3(v, v)); }
	Vec3 normalizeVec3(const Vec3& v, const Vec3& fallback) {
		const double len = lengthVec3(v);
		if (len <= 1.0e-9) return fallback;
		return mulVec3(v, 1.0 / len);
	}

	Vec3 toVec3(const varjo_Vector3D& v) { return Vec3{ v.x, v.y, v.z }; }
	varjo_Vector3D toVarjoVector3D(const Vec3& v) { return varjo_Vector3D{ v.x, v.y, v.z }; }

	Vec3 viewMatrixEyeOriginWorld(const double view[16]) {
		// view is world -> eye. For Varjo view matrices this is rigid, so inverse
		// origin is -R^T t. This avoids using current varjo_FrameGetPose().
		const double tx = view[12];
		const double ty = view[13];
		const double tz = view[14];
		return Vec3{
			-(view[0] * tx + view[1] * ty + view[2] * tz),
			-(view[4] * tx + view[5] * ty + view[6] * tz),
			-(view[8] * tx + view[9] * ty + view[10] * tz)
		};
	}

	Vec3 transformViewVectorToWorld(const double view[16], const Vec3& v) {
		// Inverse rotation of the world->eye view matrix: R^T * v.
		return Vec3{
			view[0] * v.x + view[1] * v.y + view[2] * v.z,
			view[4] * v.x + view[5] * v.y + view[6] * v.z,
			view[8] * v.x + view[9] * v.y + view[10] * v.z
		};
	}

	bool reconstructHeadBasisFromFrameInfoViews(
		const std::vector<varjo_ViewInfo>& views,
		Vec3& center_world,
		Vec3& right_world,
		Vec3& up_world,
		Vec3& forward_world,
		Vec3& positive_z_world)
	{
		if (views.size() < 2) return false;

		const Vec3 leftEye = viewMatrixEyeOriginWorld(views[0].viewMatrix);
		const Vec3 rightEye = viewMatrixEyeOriginWorld(views[1].viewMatrix);
		center_world = mulVec3(addVec3(leftEye, rightEye), 0.5);

		right_world = normalizeVec3(addVec3(
			transformViewVectorToWorld(views[0].viewMatrix, Vec3{ 1.0, 0.0, 0.0 }),
			transformViewVectorToWorld(views[1].viewMatrix, Vec3{ 1.0, 0.0, 0.0 })), Vec3{ 1.0, 0.0, 0.0 });

		up_world = normalizeVec3(addVec3(
			transformViewVectorToWorld(views[0].viewMatrix, Vec3{ 0.0, 1.0, 0.0 }),
			transformViewVectorToWorld(views[1].viewMatrix, Vec3{ 0.0, 1.0, 0.0 })), Vec3{ 0.0, 1.0, 0.0 });

		forward_world = normalizeVec3(addVec3(
			transformViewVectorToWorld(views[0].viewMatrix, Vec3{ 0.0, 0.0, -1.0 }),
			transformViewVectorToWorld(views[1].viewMatrix, Vec3{ 0.0, 0.0, -1.0 })), Vec3{ 0.0, 0.0, -1.0 });

		// Re-orthogonalize to keep the basis stable.
		right_world = normalizeVec3(right_world, normalizeVec3(crossVec3(forward_world, up_world), Vec3{ 1.0, 0.0, 0.0 }));
		up_world = normalizeVec3(crossVec3(right_world, forward_world), up_world);
		positive_z_world = normalizeVec3(mulVec3(forward_world, -1.0), Vec3{ 0.0, 0.0, 1.0 });
		return true;
	}

	varjo_Vector3D transformHeadPointToWorldUsingFrameInfo(
		const std::vector<varjo_ViewInfo>& views,
		const varjo_Vector3D& pointHeadRH)
	{
		Vec3 center{}, right{}, up{}, forward{}, positiveZ{};
		if (!reconstructHeadBasisFromFrameInfoViews(views, center, right, up, forward, positiveZ)) {
			return pointHeadRH;
		}

		const Vec3 p = toVec3(pointHeadRH);
		const Vec3 world = addVec3(addVec3(addVec3(center, mulVec3(right, p.x)), mulVec3(up, p.y)), mulVec3(positiveZ, p.z));
		return toVarjoVector3D(world);
	}

void setCurrentThreadLowPriorityForWaitSync() {
		// This worker calls varjo_WaitSync only to collect frameInfo for gaze
		// projection. It must not compete with the main rendering/camera path.
		// Lower only this worker thread, not the process.
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
	}

	void copyViewInfo(const varjo_ViewInfo& src, varjo_ViewInfo& dst) {
		std::copy(std::begin(src.projectionMatrix), std::end(src.projectionMatrix), std::begin(dst.projectionMatrix));
		std::copy(std::begin(src.viewMatrix), std::end(src.viewMatrix), std::begin(dst.viewMatrix));
		dst.preferredWidth = src.preferredWidth;
		dst.preferredHeight = src.preferredHeight;
		dst.enabled = src.enabled;
		dst.reserved = src.reserved;
	}

	void copyFrameInfo(const FrameInfo& src, FrameInfo& dst) {
		dst.views.resize(src.views.size());
		for (size_t i = 0; i < src.views.size(); ++i) {
			copyViewInfo(src.views[i], dst.views[i]);
		}
		dst.displayTime = src.displayTime;
		dst.frameNumber = src.frameNumber;
	}

	FrameInfo makeEmptyFrameInfo(size_t viewCount) {
		FrameInfo frameInfo{};
		frameInfo.views.resize(viewCount);
		frameInfo.displayTime = 0;
		frameInfo.frameNumber = 0;
		return frameInfo;
	}

	template <typename FrameInfoContainer>
	std::optional<FrameInfo> findFrameInfoAtOrBeforeOrClosestBoundary(
		const FrameInfoContainer& frameInfos,
		varjo_Nanoseconds target)
	{
		if (frameInfos.empty()) {
			return std::nullopt;
		}

		// upper_bound returns the first FrameInfo whose displayTime is greater than target.
		// Therefore the previous item is the newest FrameInfo at or before target.
		auto it = std::upper_bound(
			frameInfos.begin(),
			frameInfos.end(),
			target,
			[] (varjo_Nanoseconds targetTime, const FrameInfo& frameInfo) {
				return targetTime < frameInfo.displayTime;
			}
		);

		if (it == frameInfos.begin()) {
			// target is older than the oldest buffered FrameInfo. Use the oldest one
			// instead of dereferencing begin() - 1.
			FrameInfo out{};
			copyFrameInfo(*it, out);
			return out;
		}

		--it;
		FrameInfo out{};
		copyFrameInfo(*it, out);
		return out;
	}

	Vec4 mulMat4Vec4(const double mat[16], const Vec4& v)
	{
		// Varjo matrix is column-major.
		// result = M * v
		return {
			mat[0] * v.x + mat[4] * v.y + mat[8] * v.z + mat[12] * v.w,
			mat[1] * v.x + mat[5] * v.y + mat[9] * v.z + mat[13] * v.w,
			mat[2] * v.x + mat[6] * v.y + mat[10] * v.z + mat[14] * v.w,
			mat[3] * v.x + mat[7] * v.y + mat[11] * v.z + mat[15] * v.w
		};
	}

	varjo_Vector3D toRightHandedPointFromGaze(const double p[3])
	{
		return {
			p[0],
			p[1],
			-p[2]
		};
	}

	varjo_Vector3D toRightHandedDirectionFromGaze(const double d[3])
	{
		return {
			d[0],
			d[1],
			-d[2]
		};
	}

	varjo_Vector3D makeGazePointInHeadSpaceRH(
		const varjo_Ray& ray,
		double distance
	) {
		varjo_Vector3D origin = toRightHandedPointFromGaze(ray.origin);
		varjo_Vector3D forward = toRightHandedDirectionFromGaze(ray.forward);

		return {
			origin.x + forward.x * distance,
			origin.y + forward.y * distance,
			origin.z + forward.z * distance
		};
	}

	varjo_Vector3D transformPoint(const double mat[16], const varjo_Vector3D& p)
	{
		Vec4 r = mulMat4Vec4(mat, {p.x, p.y, p.z, 1.0});

		if (std::abs(r.w) > 1e-12) {
			return {
				r.x / r.w,
				r.y / r.w,
				r.z / r.w
			};
		}

		return {r.x, r.y, r.z};
	}

	std::string varjo_RayToString(const varjo_Ray& ray) {
		std::string out = "";
		out += (std::to_string(ray.origin[0]) + "," + std::to_string(ray.origin[1]) + "," + std::to_string(ray.origin[2]) + ",");
		out += (std::to_string(ray.forward[0]) + "," + std::to_string(ray.forward[1]) + "," + std::to_string(ray.forward[2]));

		return out;
	}

bool isFinite2(const varjo_Vector2Df& p)
	{
		return std::isfinite(p.x) && std::isfinite(p.y);
	}

	varjo_Vector2Df ndcToDisplayUv01(const varjo_Vector2Df& ndc)
	{
		if (!isFinite2(ndc)) {
			const float nan = std::numeric_limits<float>::quiet_NaN();
			return varjo_Vector2Df{ nan, nan };
		}

		return varjo_Vector2Df{
			ndc.x * 0.5f + 0.5f,
			0.5f - ndc.y * 0.5f
		};
	}

}

VarjoEyeTrackingProvider::VarjoEyeTrackingProvider(const std::shared_ptr<varjo_Session>& session)
	: session_(session)
	, frameInfos_(boost::circular_buffer<FrameInfo>(512))
	, viewCount_(varjo_GetViewCount(session.get()))
{}

VarjoEyeTrackingProvider::~VarjoEyeTrackingProvider() {
	this->shutdown();
}

void VarjoEyeTrackingProvider::initialize(OutputFilterType outputFilterType, OutputFrequency outputFrequency) {
	
	if (this->getFrameInfoWorker_.joinable()) {
		return;
	}

	// frameInfo取得開始(満杯になるまで待機)
	this->getFrameInfoWorker_ = std::thread(&VarjoEyeTrackingProvider::getFrameInfoWorkerFunction, this);
	// The worker itself also lowers its priority, but set it here as soon as
	// the native handle exists to avoid competing with rendering during startup.
	SetThreadPriority(static_cast<HANDLE>(this->getFrameInfoWorker_.native_handle()), THREAD_PRIORITY_LOWEST);
	while (!this->frameInfos_.full()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	// アイトラッカの設定 / 開始
	varjo_GazeParameters params[2];
	params[0].key = varjo_GazeParametersKey_OutputFilterType;
	switch (outputFilterType) {
	case OutputFilterType::NONE: 
		params[0].value = varjo_GazeParametersValue_OutputFilterNone;
		break;
	case OutputFilterType::STANDARD:
	default:
		params[0].value = varjo_GazeParametersValue_OutputFilterStandard;
		break;
	}

	params[1].key = varjo_GazeParametersKey_OutputFrequency;
	switch (outputFrequency) {
	case OutputFrequency::_100HZ:
		params[1].value = varjo_GazeParametersValue_OutputFrequency100Hz;
		break;
	case OutputFrequency::_200HZ:
		params[1].value = varjo_GazeParametersValue_OutputFrequency200Hz;
		break;
	case OutputFrequency::MAXIMUM:
	default:
		params[1].value = varjo_GazeParametersValue_OutputFrequencyMaximumSupported;
		break;
	}

	varjo_GazeInitWithParameters(this->session_.get(), params, static_cast<int32_t>(std::size(params)));
}

void VarjoEyeTrackingProvider::shutdown()
{
	this->workerStopSignal_.store(true);
	if (this->getFrameInfoWorker_.joinable()) {
		this->getFrameInfoWorker_.join();
	}
}

VarjoEyeTrackingProvider::Status VarjoEyeTrackingProvider::getStatus() const
{
	varjo_SyncProperties(this->session_.get());

	if (!varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_GazeAllowed)) {
		return Status::NOT_AVAILABLE;
	}

	if (!varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_HMDConnected)) {
		return Status::NOT_CONNECTED;
	}

	if (varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_GazeCalibrating)) {
		return Status::CALIBRATING;
	}

	if (varjo_GetPropertyBool(this->session_.get(), varjo_PropertyKey_GazeCalibrated)) {
		return Status::CALIBRATED;
	}

	return Status::NOT_CALIBRATED;
}

std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>> VarjoEyeTrackingProvider::getGazeDataWithEyeMeasurements() const
{
	// Number of items to grow the output buffer in each iteration
	constexpr size_t c_growStep = 16;
	std::array<varjo_Gaze, c_growStep> gazeArray;
	std::array<varjo_EyeMeasurements, c_growStep> eyeMeasurementsArray;

	std::vector<std::pair<varjo_Gaze, varjo_EyeMeasurements>> output;
	int32_t newItems = 0;
	do {
		// Get more items from Varjo
		newItems = varjo_GetGazeDataArray(this->session_.get(), gazeArray.data(), eyeMeasurementsArray.data(), c_growStep);

		// Copy new items to output
		output.reserve(output.size() + newItems);
		for (int32_t i = 0; i < newItems; ++i) {
			output.push_back({gazeArray[i], eyeMeasurementsArray[i]});
		}
	} while (newItems == c_growStep);

	return output;
}

std::optional<double> VarjoEyeTrackingProvider::getUserIPD() const
{
	varjo_SyncProperties(this->session_.get());

	const double estimate = varjo_GetPropertyDouble(this->session_.get(), varjo_PropertyKey_GazeIPDEstimate);
	return (estimate <= 0.0) ? std::nullopt : std::make_optional(estimate);
}

std::optional<double> VarjoEyeTrackingProvider::getHMDIPD() const
{
	varjo_SyncProperties(this->session_.get());

	const double positionInMM = varjo_GetPropertyDouble(this->session_.get(), varjo_PropertyKey_IPDPosition);
	return (positionInMM <= 0.0) ? std::nullopt : std::make_optional(positionInMM);
}

std::string VarjoEyeTrackingProvider::getIPDAdjustmentMode() const
{
	varjo_SyncProperties(this->session_.get());

	const uint32_t strSizeWithNullTerm = varjo_GetPropertyStringSize(this->session_.get(), varjo_PropertyKey_IPDAdjustmentMode);

	if (strSizeWithNullTerm <= 1) {
		return {};  // property is empty or does not exist
	}

	std::vector<char> buffer(strSizeWithNullTerm);
	varjo_GetPropertyString(this->session_.get(), varjo_PropertyKey_IPDAdjustmentMode, buffer.data(), static_cast<uint32_t>(buffer.size()));
	return std::string(buffer.data());
}

std::optional<varjo_Gaze> VarjoEyeTrackingProvider::getRenderingGaze() const
{
	varjo_Gaze renderingGaze;
	auto ret = varjo_GetRenderingGaze(this->session_.get(), &renderingGaze);
	if (!ret) {
		return std::nullopt;
	}
	return renderingGaze;
}

std::vector<VarjoEyeTrackingData> VarjoEyeTrackingProvider::getEyeTrackingData()
{
	auto gaze_and_measurements = this->getGazeDataWithEyeMeasurements();

	// 返却するデータが無ければ則終了
	if (gaze_and_measurements.empty()) {
		return std::vector<VarjoEyeTrackingData>();
	}

	auto userIPD = this->getUserIPD();
	auto hmdIPD = this->getHMDIPD();
	auto ipdAdjustmentMode = this->getIPDAdjustmentMode();
	auto renderingGaze = this->getRenderingGaze();
	
	// 視線位置を計算
	auto viewCount = varjo_GetViewCount(this->session_.get());
	std::vector<FrameInfo> copy_frameInfos{};					// 視線位置の座標計算のために必要なframeInfoをコピーする配列
	std::optional<FrameInfo> copy_frameInfo_forRenderingGaze = std::nullopt;

	{
		// 今回取り出したvarjo_GazeのcaptureTimeの最小値と最大値を取り出す
		auto capTime_start = gaze_and_measurements.front().first.captureTime;
		auto capTime_end = gaze_and_measurements.back().first.captureTime;

		// frameInfos_をロック
		std::lock_guard lock(this->frameInfoMtx_);

		if (!this->frameInfos_.empty()) {
			// 必要な範囲のインデックスを取得する。
			// start は capTime_start 以下の最新 frameInfo、end は capTime_end 以下の最新 frameInfo。
			// 範囲外の場合は最古/最新に丸める。
			auto start_frameInfo_it = std::upper_bound(
				this->frameInfos_.begin(),
				this->frameInfos_.end(),
				capTime_start,
				[] (varjo_Nanoseconds target, const FrameInfo& frameInfo) {
					return target < frameInfo.displayTime;
				}
			);

			auto end_frameInfo_it = std::upper_bound(
				this->frameInfos_.begin(),
				this->frameInfos_.end(),
				capTime_end,
				[] (varjo_Nanoseconds target, const FrameInfo& frameInfo) {
					return target < frameInfo.displayTime;
				}
			);

			int start_frameInfo_idx = 0;
			int end_frameInfo_idx = 0;

			if (start_frameInfo_it == this->frameInfos_.begin()) {
				start_frameInfo_idx = 0;
			} else {
				--start_frameInfo_it;
				start_frameInfo_idx = static_cast<int>(std::distance(this->frameInfos_.begin(), start_frameInfo_it));
			}

			if (end_frameInfo_it == this->frameInfos_.begin()) {
				end_frameInfo_idx = 0;
			} else {
				--end_frameInfo_it;
				end_frameInfo_idx = static_cast<int>(std::distance(this->frameInfos_.begin(), end_frameInfo_it));
			}

			if (end_frameInfo_idx < start_frameInfo_idx) {
				end_frameInfo_idx = start_frameInfo_idx;
			}

			// コピー。ここでは必ず i 番目の frameInfo をコピーする。
			// 以前のコードは start_frameInfo_idx を使い続けていたため、
			// copy_frameInfos 全体が同じ FrameInfo になる可能性があった。
			copy_frameInfos.resize(end_frameInfo_idx - start_frameInfo_idx + 1);
			for (int i = start_frameInfo_idx; i <= end_frameInfo_idx; ++i) {
				copyFrameInfo(this->frameInfos_[i], copy_frameInfos[i - start_frameInfo_idx]);
			}

			// renderingGazeに対応するframeInfoを検索。
			// lower_bound/end の dereference を避けるため、範囲外は最古/最新へ丸める。
			if (renderingGaze.has_value()) {
				copy_frameInfo_forRenderingGaze = findFrameInfoAtOrBeforeOrClosestBoundary(
					this->frameInfos_,
					renderingGaze.value().captureTime
				);
			}
		}
	}


	// ---------- 視線位置の座標計算
	// gazePos_toVideo is now derived from the same principled view/projection
	// calculation as gazePos_toVarjoDisplay. The old empirical fixed coefficient
	// path has been removed.
	std::optional<VarjoProjectedGazePosition> renderingGazePos_toVideo = std::nullopt;
	std::vector<VarjoProjectedGazePosition> gazePos_toVideo(gaze_and_measurements.size());

	// ---------- 視線位置のVarjoディスプレイへの座標計算

	// renderingGaze
	std::optional<VarjoProjectedGazePosition> renderingGazePos_toVarjoDisplay = std::nullopt;
	if (renderingGaze.has_value() && copy_frameInfo_forRenderingGaze.has_value()) {
		renderingGazePos_toVarjoDisplay = this->calcProjectedGazePositionToVarjoDisplay(
			renderingGaze.value(),
			copy_frameInfo_forRenderingGaze.value().views
		);
		renderingGazePos_toVideo = this->calcProjectedGazePositionToVideo(
			renderingGaze.value(),
			copy_frameInfo_forRenderingGaze.value().views
		);
	}

	// 通常gaze
	std::vector<std::pair<VarjoProjectedGazePosition, FrameInfo>> gazePos_toVarjoDisplay_and_frameInfo(gaze_and_measurements.size());
	for (auto i = 0; i < gaze_and_measurements.size(); ++i) {
		// 対応するframeInfoを見つける。
		// begin()-1 を避け、範囲外の場合は最古/最新に丸める。
		FrameInfo paired_frameInfo = makeEmptyFrameInfo(static_cast<size_t>(viewCount));
		auto paired_frameInfo_opt = findFrameInfoAtOrBeforeOrClosestBoundary(
			copy_frameInfos,
			gaze_and_measurements[i].first.captureTime
		);

		if (paired_frameInfo_opt.has_value()) {
			paired_frameInfo = paired_frameInfo_opt.value();
		}

		gazePos_toVideo[i] = this->calcProjectedGazePositionToVideo(
			gaze_and_measurements[i].first,
			paired_frameInfo.views
		);

		gazePos_toVarjoDisplay_and_frameInfo[i] = std::pair<VarjoProjectedGazePosition, FrameInfo>(
			this->calcProjectedGazePositionToVarjoDisplay(gaze_and_measurements[i].first, paired_frameInfo.views),
			paired_frameInfo
		);
	}

	std::vector<VarjoEyeTrackingData> datas(gaze_and_measurements.size());
	for (auto i = 0; i < gaze_and_measurements.size(); ++i) {
		datas[i] = VarjoEyeTrackingData{
			.gaze = gaze_and_measurements[i].first,
			.measurements = gaze_and_measurements[i].second,
			.userIPD = userIPD,
			.hmdIPD = hmdIPD,
			.ipdAdjustmentMode = ipdAdjustmentMode,
			.renderingGaze = renderingGaze,
			.gazePos_toVideo = gazePos_toVideo[i], 
			.renderingGazePos_toVideo = renderingGazePos_toVideo,
			.gazePos_toVarjoDisplay = gazePos_toVarjoDisplay_and_frameInfo[i].first,
			.renderingGazePos_toVarjoDisplay = renderingGazePos_toVarjoDisplay,
			.frameInfo = gazePos_toVarjoDisplay_and_frameInfo[i].second,
			.renderingGazeFrameInfo = copy_frameInfo_forRenderingGaze
		};
	}

	return datas;
}

VarjoProjectedGazePosition VarjoEyeTrackingProvider::calcProjectedGazePositionToVarjoDisplay(
	const varjo_Gaze& gaze, 
	const std::vector<varjo_ViewInfo>& viewInfo) const
{
	VarjoProjectedGazePosition out{};

	if (viewInfo.size() < 2) {
		return out;
	}

	const int leftView_index = 0;
	const int rightView_index = 1;

	out.leftEye = calcProjectedGazePositionToVarjoDisplayOneRay(
		gaze.leftEye,
		gaze.focusDistance,
		viewInfo,
		leftView_index
	);

	out.rightEye = calcProjectedGazePositionToVarjoDisplayOneRay(
		gaze.rightEye,
		gaze.focusDistance,
		viewInfo,
		rightView_index
	);

	out.combinedEyeToLeft = calcProjectedGazePositionToVarjoDisplayOneRay(
		gaze.gaze,
		gaze.focusDistance,
		viewInfo,
		leftView_index
	);

	out.combinedEyeToRight = calcProjectedGazePositionToVarjoDisplayOneRay(
		gaze.gaze,
		gaze.focusDistance,
		viewInfo,
		rightView_index
	);

	return out;
}

varjo_Vector2Df VarjoEyeTrackingProvider::calcProjectedGazePositionToVarjoDisplayOneRay(
	const varjo_Ray& gazeRay,
	double focusDistance,
	const std::vector<varjo_ViewInfo>& viewInfo,
	size_t targetViewIndex) const
{
	double distance = focusDistance;

	if (!(distance > 0.01 && distance <= 2.0)) {
		distance = 1.0;
	}

	if (viewInfo.size() <= targetViewIndex || viewInfo.size() < 2) {
		return { 0.0f, 0.0f };
	}

	const varjo_Vector3D pointHeadRH = makeGazePointInHeadSpaceRH(gazeRay, distance);

	// Use the same FrameInfo/view matrices paired to this gaze sample. Do not use
	// varjo_FrameGetPose() here because that would be the current pose, not
	// necessarily the pose of the gaze sample being converted.
	const varjo_Vector3D pointWorld = transformHeadPointToWorldUsingFrameInfo(viewInfo, pointHeadRH);

	Vec4 pWorld{
		pointWorld.x,
		pointWorld.y,
		pointWorld.z,
		1.0
	};

	const varjo_ViewInfo& targetView = viewInfo[targetViewIndex];
	Vec4 pEye = mulMat4Vec4(targetView.viewMatrix, pWorld);
	Vec4 pClip = mulMat4Vec4(targetView.projectionMatrix, pEye);

	if (std::abs(pClip.w) < 1e-12) {
		return { 0.0f, 0.0f };
	}

	varjo_Vector2Df out{
		.x = static_cast<float>(pClip.x / pClip.w),
		.y = static_cast<float>(pClip.y / pClip.w)
	};

	if (std::isfinite(out.x) && std::isfinite(out.y)) {
		return out;
	}

	return { 0.0f, 0.0f };
}

VarjoProjectedGazePosition VarjoEyeTrackingProvider::calcProjectedGazePositionToVideo(
	const varjo_Gaze& gaze,
	const std::vector<varjo_ViewInfo>& viewInfo) const
{
	// No empirical coefficients or per-eye hand-tuned offsets are applied here.
	// First project the gaze ray through the actual HMD pose, view matrix, and
	// projection matrix to get NDC [-1, 1]. Then convert NDC to normalized video /
	// image UV [0, 1]. This keeps gazePos_toVideo on the same principled geometry
	// path as gazePos_toVarjoDisplay.
	const VarjoProjectedGazePosition ndc = this->calcProjectedGazePositionToVarjoDisplay(gaze, viewInfo);

	VarjoProjectedGazePosition result{};
	result.leftEye = ndcToDisplayUv01(ndc.leftEye);
	result.rightEye = ndcToDisplayUv01(ndc.rightEye);
	result.combinedEyeToLeft = ndcToDisplayUv01(ndc.combinedEyeToLeft);
	result.combinedEyeToRight = ndcToDisplayUv01(ndc.combinedEyeToRight);
	return result;
}

FrameInfo VarjoEyeTrackingProvider::requestFrameInfo()
{
	varjo_FrameInfo* vFrameInfo = varjo_CreateFrameInfo(this->session_.get());
	varjo_WaitSync(this->session_.get(), vFrameInfo);

	FrameInfo frameInfo;
	const auto viewCount = varjo_GetViewCount(this->session_.get());
	frameInfo.views.resize(viewCount);

	for (auto i = 0; i < viewCount; ++i) {
		frameInfo.views[i] = vFrameInfo->views[i];
	}
	frameInfo.displayTime = vFrameInfo->displayTime;
	frameInfo.frameNumber = vFrameInfo->frameNumber;

	varjo_FreeFrameInfo(vFrameInfo);

	return frameInfo;
}

void VarjoEyeTrackingProvider::getFrameInfoWorkerFunction()
{
	setCurrentThreadLowPriorityForWaitSync();

	while (!this->workerStopSignal_) {

		// FrameInfoを取得
		auto frameInfo = this->requestFrameInfo();

		// frameInfoを更新
		{
			std::lock_guard lock(this->frameInfoMtx_);

			this->frameInfos_.push_back(frameInfo);
		}
	}
}

namespace {
	std::string gazeEyeStatusToString(varjo_GazeEyeStatus status) {
		switch (status) {
		case varjo_GazeEyeStatus_Invalid: 
			return "Invalid";
		case varjo_GazeEyeStatus_Visible:
			return "Visible";
		case varjo_GazeEyeStatus_Compensated:
			return "Compensated";
		case varjo_GazeEyeStatus_Tracked:
			return "Tracked";
		}
	}

	std::string varjo_ViewInfoToCsvString(const varjo_ViewInfo& view) {
		const auto matrix_size = 16;

		std::string out = "";
		for (auto i = 0; i < matrix_size; ++i) {
			out += std::to_string(view.projectionMatrix[i]) + ",";
		}

		for (auto i = 0; i < matrix_size; ++i) {
			out += std::to_string(view.viewMatrix[i]) + ",";
		}

		out += std::to_string(view.preferredWidth) + ",";
		out += std::to_string(view.preferredHeight) + ",";
		out += (view.enabled == varjo_True ? "enable" : "unenable");

		return out;
	}
}

VarjoEyeTrackingDataLogger::VarjoEyeTrackingDataLogger(const std::string& filepath, std::shared_ptr<varjo_Session> session)
	: session_(session)
	, filepath_(filepath)
	, viewCount_(varjo_GetViewCount(session.get()))
{}

VarjoEyeTrackingDataLogger::~VarjoEyeTrackingDataLogger()
{
	this->close();
}

bool VarjoEyeTrackingDataLogger::open()
{
	if (this->logfile_.is_open()) {
		return false;
	}

	this->logfile_.open(this->filepath_.string());

	if (this->logfile_.is_open()) {
		this->logfile_ << this->getHeaderCsvString();
	}

	return this->logfile_.is_open();
}

void VarjoEyeTrackingDataLogger::close()
{
	if (!this->logfile_.is_open()) {
		return;
	}

	this->logfile_.close();
}

void VarjoEyeTrackingDataLogger::write(const VarjoEyeTrackingData& data) 
{
	auto csvString = this->varjoEyeTrackingDataToCsvString(data);

	this->logfile_ << csvString;
}

std::string VarjoEyeTrackingDataLogger::getHeaderCsvString()
{
	return "gaze.leftEye.origin[x],gaze.leftEye.origin[y],gaze.leftEye.origin[z],gaze.leftEye.forward[x],gaze.leftEye.forward[y],gaze.leftEye.forward[z],gaze.rightEye.origin[x],gaze.rightEye.origin[y],gaze.rightEye.origin[z],gaze.rightEye.forward[x],gaze.rightEye.forward[y],gaze.rightEye.forward[z],gaze.gaze.origin[x],gaze.gaze.origin[y],gaze.gaze.origin[z],gaze.gaze.forward[x],gaze.gaze.forward[y],gaze.gaze.forward[z],gaze.focusDistance,gaze.stability,gaze.captureTime,gaze.leftStatus,gaze.rightStatus,gaze.status,gaze.frameNumber,"
		"measurements.frameNumber,measurements.captureTime,measurements.interPupillaryDistanceInMM,measurements.leftPupilIrisDiameterRatio,measurements.rightPupilIrisDiameterRatio,measurements.leftPupilDiameterInMM,measurements.rightPupilDiameterInMM,measurements.leftIrisDiameterInMM,measurements.rightIrisDiameterInMM,measurements.leftEyeOpenness,measurements.rightEyeOpenness,"
		"userIPD,hmdIPD,ipdAdjustmentMode,"
		"renderingGaze.leftEye.origin[x],renderingGaze.leftEye.origin[y],renderingGaze.leftEye.origin[z],renderingGaze.leftEye.forward[x],renderingGaze.leftEye.forward[y],renderingGaze.leftEye.forward[z],renderingGaze.rightEye.origin[x],renderingGaze.rightEye.origin[y],renderingGaze.rightEye.origin[z],renderingGaze.rightEye.forward[x],renderingGaze.rightEye.forward[y],renderingGaze.rightEye.forward[z],renderingGaze.gaze.origin[x],renderingGaze.gaze.origin[y],renderingGaze.gaze.origin[z],renderingGaze.gaze.forward[x],renderingGaze.gaze.forward[y],renderingGaze.gaze.forward[z],renderingGaze.focusDistance,renderingGaze.stability,renderingGaze.captureTime,renderingGaze.leftStatus,renderingGaze.rightStatus,renderingGaze.status,renderingGaze.frameNumber,"
		"gazePos_toVideo.leftEye.x,gazePos_toVideo.leftEye.y,gazePos_toVideo.rightEye.x,gazePos_toVideo.rightEye.y,gazePos_toVideo.combinedEyeToLeftEye.x,gazePos_toVideo.combinedEyeToLeft.y,gazePos_toVideo.combinedEyeToRight.x,gazePos_toVideo.combinedEyeToRight.y,"
		"renderingGazePos_toVideo.leftEye.x,renderingGazePos_toVideo.leftEye.y,renderingGazePos_toVideo.rightEye.x,renderingGazePos_toVideo.rightEye.y,renderingGazePos_toVideo.combinedEyeToLeftEye.x,renderingGazePos_toVideo.combinedEyeToLeft.y,renderingGazePos_toVideo.combinedEyeToRight.x,renderingGazePos_toVideo.combinedEyeToRight.y,"
		"gazePos_toVarjoDisplay.leftEye.x,gazePos_toVarjoDisplay.leftEye.y,gazePos_toVarjoDisplay.rightEye.x,gazePos_toVarjoDisplay.rightEye.y,gazePos_toVarjoDisplay.combinedEyeToLeftEye.x,gazePos_toVarjoDisplay.combinedEyeToLeft.y,gazePos_toVarjoDisplay.combinedEyeToRight.x,gazePos_toVarjoDisplay.combinedEyeToRight.y,"
		"renderingGazePos_toVarjoDisplay.leftEye.x,renderingGazePos_toVarjoDisplay.leftEye.y,renderingGazePos_toVarjoDisplay.rightEye.x,renderingGazePos_toVarjoDisplay.rightEye.y,renderingGazePos_toVarjoDisplay.combinedEyeToLeftEye.x,renderingGazePos_toVarjoDisplay.combinedEyeToLeft.y,renderingGazePos_toVarjoDisplay.combinedEyeToRight.x,renderingGazePos_toVarjoDisplay.combinedEyeToRight.y,"
		"frameInfo.views[left].projectionMatrix[0],frameInfo.views[left].projectionMatrix[1],frameInfo.views[left].projectionMatrix[2], frameInfo.views[left].projectionMatrix[3], frameInfo.views[left].projectionMatrix[4], frameInfo.views[left].projectionMatrix[5], frameInfo.views[left].projectionMatrix[6], frameInfo.views[left].projectionMatrix[7], frameInfo.views[left].projectionMatrix[8], frameInfo.views[left].projectionMatrix[9], frameInfo.views[left].projectionMatrix[10], frameInfo.views[left].projectionMatrix[11], frameInfo.views[left].projectionMatrix[12], frameInfo.views[left].projectionMatrix[13], frameInfo.views[left].projectionMatrix[14], frameInfo.views[left].projectionMatrix[15], frameInfo.views[left].viewMatrix[0], frameInfo.views[left].viewMatrix[1], frameInfo.views[left].viewMatrix[2], frameInfo.views[left].viewMatrix[3], frameInfo.views[left].viewMatrix[4], frameInfo.views[left].viewMatrix[5], frameInfo.views[left].viewMatrix[6], frameInfo.views[left].viewMatrix[7], frameInfo.views[left].viewMatrix[8], frameInfo.views[left].viewMatrix[9], frameInfo.views[left].viewMatrix[10], frameInfo.views[left].viewMatrix[11], frameInfo.views[left].viewMatrix[12], frameInfo.views[left].viewMatrix[13], frameInfo.views[left].viewMatrix[14], frameInfo.views[left].viewMatrix[15], frameInfo.views[left].preferredWidth, frameInfo.views[left].preferredHeight, frameInfo.views[left].enabled, frameInfo.views[right].projectionMatrix[0], frameInfo.views[right].projectionMatrix[1], frameInfo.views[right].projectionMatrix[2], frameInfo.views[right].projectionMatrix[3], frameInfo.views[right].projectionMatrix[4], frameInfo.views[right].projectionMatrix[5], frameInfo.views[right].projectionMatrix[6], frameInfo.views[right].projectionMatrix[7], frameInfo.views[right].projectionMatrix[8], frameInfo.views[right].projectionMatrix[9], frameInfo.views[right].projectionMatrix[10], frameInfo.views[right].projectionMatrix[11], frameInfo.views[right].projectionMatrix[12], frameInfo.views[right].projectionMatrix[13], frameInfo.views[right].projectionMatrix[14], frameInfo.views[right].projectionMatrix[15], frameInfo.views[right].viewMatrix[0], frameInfo.views[right].viewMatrix[1], frameInfo.views[right].viewMatrix[2], frameInfo.views[right].viewMatrix[3], frameInfo.views[right].viewMatrix[4], frameInfo.views[right].viewMatrix[5], frameInfo.views[right].viewMatrix[6], frameInfo.views[right].viewMatrix[7], frameInfo.views[right].viewMatrix[8], frameInfo.views[right].viewMatrix[9], frameInfo.views[right].viewMatrix[10], frameInfo.views[right].viewMatrix[11], frameInfo.views[right].viewMatrix[12], frameInfo.views[right].viewMatrix[13], frameInfo.views[right].viewMatrix[14], frameInfo.views[right].viewMatrix[15], frameInfo.views[right].preferredWidth, frameInfo.views[right].preferredHeight, frameInfo.views[right].enabled, frameInfo.displayTime, frameInfo.frameNumber\n";
}

std::string VarjoEyeTrackingDataLogger::varjoEyeTrackingDataToCsvString(const VarjoEyeTrackingData& data)
{
	std::string out = "";

	// gaze
	out += this->varjoGazeToCsvString(data.gaze) + ",";

	// measurements
	out += this->varjoEyeMeasurementsToCsvString(data.measurements) + ",";

	// IPD and adjustmentMode
	out += (data.userIPD.has_value() ? std::to_string(data.userIPD.value()) : "nullopt") + ",";
	out += (data.hmdIPD.has_value() ? std::to_string(data.hmdIPD.value()) : "nullopt") + ",";
	out += data.ipdAdjustmentMode + ",";

	// renderingGaze
	if (data.renderingGaze.has_value()) {
		out += this->varjoGazeToCsvString(data.renderingGaze.value()) + ",";
	} else {
		// varjo_Gazeの要素数と同じ数だけnulloptを入れる
		auto elements_count = 25;
		for (auto i = 0; i < elements_count; ++i) {
			out += "nullopt,";
		}
	}

	// gazePos_toVideo
	out += this->varjoProjectedGazePositionToCsvString(data.gazePos_toVideo) + ",";
	if (data.renderingGazePos_toVideo.has_value()) {
		out += this->varjoProjectedGazePositionToCsvString(data.renderingGazePos_toVideo.value());
	} else {
		auto elements_count = 8;
		for (auto i = 0; i < elements_count; ++i) {
			out += "nullopt,";
		}
	}

	// gazePos_toVarjoDisplay
	out += this->varjoProjectedGazePositionToCsvString(data.gazePos_toVarjoDisplay) + ",";
	if (data.renderingGazePos_toVarjoDisplay.has_value()) {
		out += this->varjoProjectedGazePositionToCsvString(data.renderingGazePos_toVarjoDisplay.value()) + ",";
	} else {
		auto elements_count = 8;
		for (auto i = 0; i < elements_count; ++i) {
			out += "nullopt,";
		}
	}

	// frameInfo
	out += this->frameInfoToCsvString(data.frameInfo) + "\n";

	return out;
}

std::string VarjoEyeTrackingDataLogger::varjoGazeToCsvString(const varjo_Gaze& gaze)
{
	std::string out = "";
	
	out += varjo_RayToString(gaze.leftEye) + ",";
	out += varjo_RayToString(gaze.rightEye) + ",";
	out += varjo_RayToString(gaze.gaze) + ",";
	out += std::to_string(gaze.focusDistance) + ",";
	out += std::to_string(gaze.stability) + ",";
	out += std::to_string(gaze.captureTime) + ",";
	out += gazeEyeStatusToString(gaze.leftStatus) + ",";
	out += gazeEyeStatusToString(gaze.rightStatus) + ",";
	out += gazeEyeStatusToString(gaze.status) + ",";
	out += std::to_string(gaze.frameNumber);

	return out;
}

std::string VarjoEyeTrackingDataLogger::varjoEyeMeasurementsToCsvString(const varjo_EyeMeasurements& measurements)
{
	std::string out = "";

	out += std::to_string(measurements.frameNumber) + ",";
	out += std::to_string(measurements.captureTime) + ",";
	out += std::to_string(measurements.interPupillaryDistanceInMM) + ",";
	out += std::to_string(measurements.leftPupilIrisDiameterRatio) + ",";
	out += std::to_string(measurements.rightPupilIrisDiameterRatio) + ",";
	out += std::to_string(measurements.leftPupilDiameterInMM) + ",";
	out += std::to_string(measurements.rightPupilDiameterInMM) + ",";
	out += std::to_string(measurements.leftIrisDiameterInMM) + ",";
	out += std::to_string(measurements.rightIrisDiameterInMM) + ",";
	out += std::to_string(measurements.leftEyeOpenness) + ",";
	out += std::to_string(measurements.rightEyeOpenness);

	return out;
}

std::string VarjoEyeTrackingDataLogger::varjoProjectedGazePositionToCsvString(const VarjoProjectedGazePosition& gazePos)
{
	std::string out = "";

	out += std::to_string(gazePos.leftEye.x) + "," + std::to_string(gazePos.leftEye.y) + ",";
	out += std::to_string(gazePos.rightEye.x) + "," + std::to_string(gazePos.rightEye.y) + ",";
	out += std::to_string(gazePos.combinedEyeToLeft.x) + "," + std::to_string(gazePos.combinedEyeToLeft.y) + ",";
	out += std::to_string(gazePos.combinedEyeToRight.x) + "," + std::to_string(gazePos.combinedEyeToRight.y);

	return out;
}

std::string VarjoEyeTrackingDataLogger::frameInfoToCsvString(const FrameInfo& frameInfo)
{
	std::string out = "";

	for (auto i = 0; i < 2; ++i) {
		if (static_cast<size_t>(i) < frameInfo.views.size()) {
			out += varjo_ViewInfoToCsvString(frameInfo.views[i]) + ",";
		} else {
			varjo_ViewInfo emptyView{};
			out += varjo_ViewInfoToCsvString(emptyView) + ",";
		}
	}

	out += std::to_string(frameInfo.displayTime) + ",";
	out += std::to_string(frameInfo.frameNumber);

	return out;
}

VarjoEyeTrackingService::VarjoEyeTrackingService(
	const std::shared_ptr<varjo_Session>& session,
	const VarjoEyeTrackingProvider::OutputFilterType outputFilterType,
	const VarjoEyeTrackingProvider::OutputFrequency outputFrequency,
	const std::string& filepath,
	const size_t queueSize,
	const int acquireFrequencyMs)
	: session_(session)
	, outputFilterType_(outputFilterType)
	, outputFrequency_(outputFrequency)
	, eyeTracker_(VarjoEyeTrackingProvider(session))
	, logger_(VarjoEyeTrackingDataLogger(filepath, session))
	, dataQueueMaxSize_(queueSize)
	, acquireFrequencyMs_(acquireFrequencyMs)
{}

bool VarjoEyeTrackingService::start()
{
	this->eyeTracker_.initialize(this->outputFilterType_, this->outputFrequency_);

	auto loggerOpenRet = this->logger_.open();

	if (!loggerOpenRet) {
		this->eyeTracker_.shutdown();
		this->logger_.close();

		return false;
	}


	this->threadEndSignal_.store(false);
	this->dataRequestThread_ = std::thread(&VarjoEyeTrackingService::dataRequestWorker, this);

	return true;
}

void VarjoEyeTrackingService::stop()
{
	this->threadEndSignal_.store(true);
	if (this->dataRequestThread_.joinable()) {
		this->dataRequestThread_.join();
	}

	this->eyeTracker_.shutdown();

	this->logger_.close();
}

std::deque<VarjoEyeTrackingData> VarjoEyeTrackingService::requestData()
{
	std::lock_guard<std::mutex> lock(this->dataQueueMutex_);
	return std::exchange(this->dataQueue_, std::deque<VarjoEyeTrackingData>());
}

void VarjoEyeTrackingService::dataRequestWorker()
{
	while (!this->threadEndSignal_.load()) {

		// データの取得
		auto datas = this->eyeTracker_.getEyeTrackingData();

		// データをdataQueueに提出
		{
			std::lock_guard<std::mutex> lock(this->dataQueueMutex_);

			for (auto& data : datas) {
				this->dataQueue_.push_back(data);
			}

			// queueSizeを調整
			while (this->dataQueue_.size() > this->dataQueueMaxSize_) {
				this->dataQueue_.pop_front();
			}
		}

		// ログへ書き込み
		for (auto& data : datas) {
			this->logger_.write(data);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(this->acquireFrequencyMs_));
	}
}


