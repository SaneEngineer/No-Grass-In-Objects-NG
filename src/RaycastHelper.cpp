// Adapted from https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/raycast.cpp
#include "GrassControl/RaycastHelper.h"

Raycast::RayCollector::RayCollector() = default;

void Raycast::RayCollector::AddRayHit(const RE::hkpCdBody& body, const RE::hkpShapeRayCastCollectorOutput& hitInfo)
{
	HitResult hit{};
	hit.hitFraction = hitInfo.hitFraction;
	hit.normal = {
		hitInfo.normal.quad.m128_f32[0],
		hitInfo.normal.quad.m128_f32[1],
		hitInfo.normal.quad.m128_f32[2]
	};

	const RE::hkpCdBody* obj = &body;
	while (obj) {
		if (!obj->parent)
			break;
		obj = obj->parent;
	}

	hit.body = obj;
	if (!hit.body)
		return;

	const auto collisionObj = static_cast<const RE::hkpCollidable*>(hit.body);
	const auto flags = collisionObj->broadPhaseHandle.collisionFilterInfo;

	const uint64_t m = 1ULL << static_cast<uint64_t>(flags);
	constexpr uint64_t filter = 0x40122716;  //@TODO
	if ((m & filter) != 0) {
		if (!objectFilter.empty()) {
			for (const auto filteredObj : objectFilter) {
				if (hit.getAVObject() == filteredObj)
					return;
			}
		}

		earlyOutHitFraction = hit.hitFraction;
		hits.push_back(hit);
	}
}

const std::vector<Raycast::RayCollector::HitResult>& Raycast::RayCollector::GetHits()
{
	return hits;
}

void Raycast::RayCollector::Reset()
{
	earlyOutHitFraction = 1.0f;
	hits.clear();
	objectFilter.clear();
}

RE::NiAVObject* Raycast::RayCollector::HitResult::getAVObject()
{
	typedef RE::NiAVObject* (*_GetUserData)(const RE::hkpCdBody*);
	static auto getAVObject = REL::Relocation<_GetUserData>(RELOCATION_ID(76160, 77988));
	return body ? getAVObject(body) : nullptr;
}

Raycast::RayCollector* getCastCollector() noexcept
{
	auto collector = new Raycast::RayCollector();
	return collector;
}

Raycast::RayResult Raycast::hkpCastRay(const glm::vec4& start, const glm::vec4& end) noexcept
{
#ifndef NDEBUG
	if (!mmath::IsValid(start) || !mmath::IsValid(end)) {
		__debugbreak();
		return {};
	}
#endif
	constexpr auto hkpScale = 0.0142875f;
	const glm::vec4 dif = end - start;

	constexpr auto one = 1.0f;
	const auto from = start * hkpScale;
	const auto to = dif * hkpScale;

	RE::hkpWorldRayCastInput pickRayInput{};
	pickRayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
	pickRayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);

	auto collector = getCastCollector();
	collector->Reset();

	RE::bhkPickData pickData{};
	pickData.rayInput = pickRayInput;
	pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
	pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(collector);

	const auto ply = RE::PlayerCharacter::GetSingleton();
	if (!ply->parentCell)
		return {};

	if (ply->loadedData && ply->loadedData->data3D)
		collector->AddFilter(ply->loadedData->data3D.get());

	RayCollector::HitResult best{};
	best.hitFraction = 1.0f;
	glm::vec4 bestPos = {};

	for (auto& hit : collector->GetHits()) {
		const auto pos = (dif * hit.hitFraction) + start;
		if (best.body == nullptr) {
			best = hit;
			bestPos = pos;
			continue;
		}

		if (hit.hitFraction < best.hitFraction) {
			best = hit;
			bestPos = pos;
		}
	}

	RayResult result;

	auto physicsWorld = ply->parentCell->GetbhkWorld();
	if (physicsWorld) {
		//physicsWorld->PickObject(pickData);
		if (physicsWorld->PickObject(pickData); pickData.rayOutput.HasHit()) {
			result.CollisionLayer = static_cast<RE::COL_LAYER>(pickData.rayOutput.rootCollidable->broadPhaseHandle.collisionFilterInfo & 0x7F);
		}
	}

	result.hitArray = collector->GetHits();

	//result.hitPos = bestPos;
	result.rayLength = glm::length(bestPos - start);

	if (!best.body)
		return result;
	auto av = best.getAVObject();
	result.hit = av != nullptr;

	if (result.hit) {
		auto ref = av->GetUserData();
		result.hitObject = av;
	}

	return result;
}

namespace GrassControl
{
	RaycastHelper::RaycastHelper(int version, float rayHeight, float rayDepth, const std::string& layers, Util::CachedFormList* ignored) :
		Version(version), RayHeight(rayHeight), RayDepth(rayDepth), Ignore(ignored)
	{
		auto spl = Util::StringHelpers::Split_at_any(layers, { ' ', ',', '\t', '+' }, true);
		unsigned long long mask = 0;
		for (const auto& x : spl) {
			int y = std::stoi(x);
			if (y >= 0 && y < 64) {
				mask |= static_cast<unsigned long long>(1) << y;
			}
		}
		this->RaycastMask = mask;
	}

	bool RaycastHelper::CanPlaceGrass(RE::TESObjectLAND* land, const float x, const float y, const float z) const
	{
		RE::TESObjectCELL* cell = land->GetSaveParentCell();

		if (cell == nullptr) {
			return true;
		}

		// Currently not dealing with this.
		if (cell->IsInteriorCell() || !cell->IsAttached()) {
			return true;
		}

		auto begin = glm::vec4(x, y, z + this->RayHeight, 0.0f);
		auto end = glm::vec4(x, y, z - this->RayDepth, 0.0f);
		auto rs = Raycast::hkpCastRay(begin, end);

		for (auto& [normal, hitFraction, body] : rs.hitArray) {
			if (hitFraction >= 1.0f || body == nullptr) {
				continue;
			}

			const auto collisionObj = static_cast<const RE::hkpCollidable*>(body);
			const auto flags = collisionObj->GetCollisionLayer();
			unsigned long long mask = static_cast<unsigned long long>(1) << static_cast<int>(flags);
			if (!(this->RaycastMask & mask)) {
				//if (rs.CollisionLayer == RE::COL_LAYER::kTerrain) {
				continue;
			}

			if (this->Ignore != nullptr && this->IsIgnoredObject(rs)) {
				continue;
			}
			auto sTemplate = fmt::runtime("{} {}(0x{:x})");
			auto landName = fmt::format(sTemplate, "land", land->GetFormEditorID() ? land->GetFormEditorID() : land->GetName(), land->formID);
			auto cellName = fmt::format(sTemplate, "cell", cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName(), cell->formID);
			auto rsTESForm = GetRaycastHitBaseForm(rs);
			auto hitObjectName = rsTESForm ?
			                         fmt::format(sTemplate, "with", rsTESForm->GetFormEditorID() ? rsTESForm->GetFormEditorID() : rsTESForm->GetName(), rsTESForm->formID) :
			                         "";
			logger::debug("{}:{}({},{},{}) detected hit {}", landName, cellName, x, y, z, hitObjectName);
			return false;
		}
		return true;
	}

	RE::TESForm* RaycastHelper::GetRaycastHitBaseForm(Raycast::RayResult r) const
	{
		RE::TESForm* result = nullptr;
		try {
			auto o = r.hitObject;
			int tries = 0;
			while (o != nullptr && tries++ < 10) {
				const auto userdata = o->GetUserData();
				if (userdata != nullptr) {
					auto baseForm = userdata->GetOwner();
					if (baseForm != nullptr) {
						{
							result = baseForm;
							return result;
						}
						break;
					}
				}
				o = o->parent;
			}
		} catch (...) {
		}
		return result;
	}

	bool RaycastHelper::IsRaycastHitTest(Raycast::RayResult r, std::function<bool(RE::FormID)> func) const
	{
		bool result = false;
		try {
			auto o = r.hitObject;
			int tries = 0;
			while (o != nullptr && tries++ < 10) {
				const auto userdata = o->GetUserData();
				if (userdata != nullptr) {
					auto baseForm = userdata->GetOwner();
					if (baseForm != nullptr) {
						{
							if (func(baseForm->formID))
								result = true;
						}
						break;
					}
				}
				o = o->parent;
			}
		} catch (...) {
		}
		return result;
	}

	bool RaycastHelper::IsIgnoredObject(Raycast::RayResult r) const
	{
		/*if (this.Ignore.Contains(obj.FormId))
					        result = true;
					    else*/
		return IsRaycastHitTest(r, [this](RE::FormID formid) { return this->Ignore->Contains(formid); });
	}
}
