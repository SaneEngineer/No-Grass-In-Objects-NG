// Adapted from https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/source/raycast.cpp
#include "GrassControl/RaycastHelper.h"

#include "CasualLibrary.hpp"

Raycast::RayCollector::RayCollector() = default;
Raycast::CdBodyPairCollector::CdBodyPairCollector() = default;

void Raycast::HandleErrorMessage()
{
	if (RaycastErrorCount < 10) {
		logger::error("Exception occurred while attempting raycasting. Unless repeated within the same cell this unlikely to be a serious issue.");
		RaycastErrorCount++;
	} else if (!shownError) {
		RE::DebugMessageBox("NGIO has encountered more than 10 errors while attempting raycasting in this cell. There is likely an issue with your game, that could result in a crash. It is recommend to save your game. If you do not experience crashes, this warning can be ignored and disabled in GrassControl.ini using the setting Ray-cast-error-message.");
		logger::error("Raycast error count for this cell exceeded 10. There is likely an issue with your game, that could result in crashes. If you do not experience crashes, this warning can be ignored and disabled in GrassControl.ini using the setting Ray-cast-error-message.");
		shownError = true;
	}
};

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

	const uint64_t m = 1ULL << flags.filter;
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

void Raycast::CdBodyPairCollector::addCdBodyPair(const RE::hkpCdBody& bodyA, const RE::hkpCdBody& bodyB)
{
	HitResult hit{};

	const RE::hkpCdBody* obj = &bodyA;

	if (obj == phantom->GetCollidable()) {
		obj = &bodyB;
	}

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

	const uint64_t m = 1ULL << flags.filter;
	constexpr uint64_t filter = 0x40122716;  //@TODO
	if ((m & filter) != 0) {
		if (!objectFilter.empty()) {
			for (const auto filteredObj : objectFilter) {
				if (hit.getAVObject() == filteredObj)
					return;
			}
		}

		hits.push_back(hit);
	}
}

const std::vector<Raycast::RayCollector::HitResult>& Raycast::RayCollector::GetHits()
{
	return hits;
}

const std::vector<Raycast::CdBodyPairCollector::HitResult>& Raycast::CdBodyPairCollector::GetHits()
{
	return hits;
}

void Raycast::RayCollector::Reset()
{
	earlyOutHitFraction = 1.0f;
	hits.clear();
	objectFilter.clear();
}

void Raycast::CdBodyPairCollector::Reset()
{
	hits.clear();
	objectFilter.clear();
}

RE::NiAVObject* Raycast::RayCollector::HitResult::getAVObject()
{
	typedef RE::NiAVObject* (*_GetUserData)(const RE::hkpCdBody*);
	static auto getAVObject = REL::Relocation<_GetUserData>(RELOCATION_ID(76160, 77988));
	return body ? getAVObject(body) : nullptr;
}

RE::NiAVObject* Raycast::CdBodyPairCollector::HitResult::getAVObject()
{
	typedef RE::NiAVObject* (*_GetUserData)(const RE::hkpCdBody*);
	static auto getAVObject = REL::Relocation<_GetUserData>(RELOCATION_ID(76160, 77988));
	return body ? getAVObject(body) : nullptr;
}

RE::NiAVObject* Raycast::getAVObject(const RE::hkpCdBody* body)
{
	typedef RE::NiAVObject* (*_GetUserData)(const RE::hkpCdBody*);
	static auto getAVObject = REL::Relocation<_GetUserData>(RELOCATION_ID(76160, 77988));
	return body ? getAVObject(body) : nullptr;
}

Raycast::RayResult Raycast::hkpCastRay(const glm::vec4& start, const glm::vec4& end) noexcept
{
	constexpr auto hkpScale = 0.0142875f;
	const glm::vec4 dif = end - start;

	constexpr auto one = 1.0f;
	const auto from = start * hkpScale;
	const auto to = dif * hkpScale;

	RE::hkpWorldRayCastInput pickRayInput{};
	pickRayInput.from = RE::hkVector4(from.x, from.y, from.z, one);
	pickRayInput.to = RE::hkVector4(0.0, 0.0, 0.0, 0.0);

	auto collector = RayCollector();
	collector.Reset();

	RE::bhkPickData pickData{};
	pickData.rayInput = pickRayInput;
	pickData.ray = RE::hkVector4(to.x, to.y, to.z, one);
	pickData.rayHitCollectorA8 = reinterpret_cast<RE::hkpClosestRayHitCollector*>(&collector);

	const auto ply = RE::PlayerCharacter::GetSingleton();
	auto cell = ply->GetParentCell();
	if (!cell)
		return {};

	if (ply->loadedData && ply->loadedData->data3D)
		collector.AddFilter(ply->loadedData->data3D.get());

	RayCollector::HitResult best{};
	best.hitFraction = 1.0f;
	glm::vec4 bestPos = start;

	RayResult result;

	if (!lastCell) {
		lastCell = cell;
	} else if (lastCell->formID != cell->formID) {
		lastCell = cell;
		RaycastErrorCount = 0;
		shownError = false;
	}

	try {
		auto physicsWorld = cell->GetbhkWorld();
		if (physicsWorld) {
			physicsWorld->PickObject(pickData);
		}
	} catch (...) {
		HandleErrorMessage();
		return result;
	}

	for (auto& hit : collector.GetHits()) {
		const auto pos = dif * hit.hitFraction + start;
		if (best.body == nullptr) {
			best = hit;
			bestPos = pos;
			continue;
		}

		if (hit.hitFraction < best.hitFraction) {
			best = hit;
		}

		if (pos.z > bestPos.z) {
			bestPos = pos;
		}
	}

	result.hitArray = collector.GetHits();
	result.hitPos = bestPos;

	if (!best.body)
		return result;
	auto av = best.getAVObject();
	result.hit = av != nullptr;

	if (result.hit) {
		result.hitObject = av->GetUserData();
	}

	return result;
}

Raycast::RayResult Raycast::hkpPhantomCast(glm::vec4& start, const glm::vec4& end, RE::TESObjectCELL* cell, RE::GrassParam* param)
{
	if (!cell)
		return {};

	constexpr auto hkpScale = 0.0142875f;

	constexpr auto one = 1.0f;
	const auto from = start * hkpScale;

	const glm::vec4 dif = end - start;

	auto vecA = RE::hkVector4(from.x, from.y, from.z, one);

	auto bhkWorld = cell->GetbhkWorld();
	if (!bhkWorld)
		return {};

	auto hkWorld = bhkWorld->GetWorld1();
	if (!hkWorld)
		return {};

	RE::hkTransform transform;
	transform.translation = RE::hkVector4(0.0f, 0.0f, 0.0f, 1.0f);

	auto vecBottom = RE::hkVector4(0.0f, 0.0f, 0.0f, 1.0f);
	auto vecTop = RE::hkVector4(0.0f, 0.0f, dif.z * hkpScale, 1.0f);

	float radius = 20.0f;
	float widthX = 20.0f;
	float widthY = 20.0f;
	int shapeType = 0;

	if (param) {
		auto grassForm = RE::TESForm::LookupByID<RE::TESGrass>(param->grassFormID);
		if (grassForm) {
			if (grassForm->boundData.boundMax.x != 0 && grassForm->boundData.boundMin.x != 0) {
				widthX = std::abs(static_cast<float>(grassForm->boundData.boundMax.x - grassForm->boundData.boundMin.x) * 0.5f);
				widthY = std::abs(static_cast<float>(grassForm->boundData.boundMax.y - grassForm->boundData.boundMin.y) * 0.5f);
				radius = std::max(widthX, widthY) * 0.5f;
			}

			radius *= GrassControl::Config::RayCastWidthMult;
		}
	}

	if (GrassControl::Config::RayCastWidth > 0.0f) {
		widthX = GrassControl::Config::RayCastWidth * 0.5f;
		widthY = GrassControl::Config::RayCastWidth * 0.5f;
	}

	bool newShape = false;
	newShape = std::abs(oldRadius - radius) > 1.0f;
	if (newShape) {
		if (GrassControl::Config::RayCastMode == 1 && currentShape) {
			Memory::Internal::write<RE::hkVector4>(reinterpret_cast<uintptr_t>(currentShape) + 0x28, radius * hkpScale);  // Set Radius
			Memory::Internal::write<RE::hkVector4>(reinterpret_cast<uintptr_t>(currentShape) + 0x40, vecTop);             // Set Top Vertex
		} else if (currentShape) {
			auto halfExtents = RE::hkVector4(widthX, widthY, dif.z * hkpScale / 2.0f, 0.0f);
			Memory::Internal::write<RE::hkVector4>(reinterpret_cast<uintptr_t>(currentShape) + 0x30, halfExtents);  // Set halfExtent
		}

		oldRadius = radius;
	}

	if (!currentShape) {
		currentShape = RE::malloc<RE::hkpShape>(0x70);
		if (!currentShape)
			return {};
	}

	if (!createdShape) {
		if (GrassControl::Config::RayCastMode == 1) {
			shapeType = Memory::Internal::read<int>(RELOCATION_ID(511265, 385180).address());

			using createCylinderShape_t = RE::hkpShape* (*)(RE::hkpShape*, RE::hkVector4&, RE::hkVector4&, float, int);
			REL::Relocation<createCylinderShape_t> createCylinderShape{ RELOCATION_ID(59971, 60720) };

			currentShape = createCylinderShape(currentShape, vecBottom, vecTop, radius * hkpScale, shapeType);
		} else {
			shapeType = Memory::Internal::read<int>(RELOCATION_ID(525125, 411600).address());

			auto halfExtents = RE::hkVector4(widthX, widthY, dif.z * hkpScale / 2.0f, 0.0f);

			using createBoxShape_t = RE::hkpShape* (*)(RE::hkpShape*, RE::hkVector4&, float);
			REL::Relocation<createBoxShape_t> createBoxShape{ RELOCATION_ID(59603, 60287) };

			currentShape = createBoxShape(currentShape, halfExtents, shapeType);
		}
		createdShape = true;
	}

	if (!phantom) {
		using createSimpleShapePhantom_t = RE::hkpShapePhantom* (*)(RE::hkpShapePhantom*, RE::hkpShape*, const RE::hkTransform&, uint32_t);
		REL::Relocation<createSimpleShapePhantom_t> createSimpleShapePhantom{ RELOCATION_ID(60675, 61535) };
		phantom = RE::malloc<RE::hkpShapePhantom>(0x1C0);
		if (!phantom)
			return {};
		phantom = createSimpleShapePhantom(phantom, currentShape, transform, 0);

		RE::hkpPhantom* returnPhantom = nullptr;
		if (phantom->GetShape()) {
			bhkWorld->worldLock.LockForWrite();
			returnPhantom = hkWorld->AddPhantom(phantom);
			bhkWorld->worldLock.UnlockForWrite();
		}

		if (!returnPhantom)
			return {};
	}

	if (phantom->world != hkWorld) {
		bhkWorld->worldLock.LockForWrite();
		phantom->world->RemovePhantom(phantom);
		hkWorld->AddPhantom(phantom);
		bhkWorld->worldLock.UnlockForWrite();
	}

	bhkWorld->worldLock.LockForWrite();

	RayResult result;

	auto collector = CdBodyPairCollector();
	collector.Reset();

	if (!phantom->GetShape()) {
		bhkWorld->worldLock.UnlockForWrite();
		return result;
	}

	if (!lastCell) {
		lastCell = cell;
	} else if (lastCell != cell) {
		lastCell = cell;
		RaycastErrorCount = 0;
		shownError = false;
	}

	try {
		using SetPosition_t = void (*)(RE::hkpShapePhantom*, RE::hkVector4);
		REL::Relocation<SetPosition_t> SetPosition{ RELOCATION_ID(60791, 61653) };
		SetPosition(phantom, vecA);

		using GetPenetrations_t = void (*)(RE::hkpShapePhantom*, RE::hkpCdBodyPairCollector*, RE::hkpCollisionInput*);
		REL::Relocation<GetPenetrations_t> GetPenetrations{ RELOCATION_ID(60682, 61543) };

		GetPenetrations(phantom, reinterpret_cast<RE::hkpCdBodyPairCollector*>(&collector), nullptr);
	} catch (...) {
		HandleErrorMessage();
	}

	bhkWorld->worldLock.UnlockForWrite();

	result.cdBodyHitArray = collector.GetHits();

	return result;
}

namespace GrassControl
{
	RaycastHelper::RaycastHelper(int version, float rayHeight, float rayDepth, const std::string& layers, std::unique_ptr<Util::CachedFormList> ignored, std::unique_ptr<Util::CachedFormList> textures, std::unique_ptr<Util::CachedFormList> cliffs, std::unique_ptr<Util::CachedFormList> grassTypes) :
		Version(version), RayHeight(rayHeight), RayDepth(rayDepth), Ignore(std::move(ignored)), Textures(std::move(textures)), Cliffs(std::move(cliffs)), Grasses(std::move(grassTypes))
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

	bool RaycastHelper::CanPlaceGrass(RE::TESObjectLAND* land, const float x, const float y, const float z, RE::GrassParam* param) const
	{
		auto cell = land->GetSaveParentCell();

		if (cell == nullptr) {
			return true;
		}

		// Currently not dealing with this.
		if (cell->IsInteriorCell() || !cell->IsAttached()) {
			return true;
		}

		if (param) {
			if (this->Grasses != nullptr && this->Grasses->Contains(param->grassFormID))
				return true;
		}

		if (this->Textures != nullptr) {
			const auto width = static_cast<float>(Config::RayCastTextureWidth);
			auto tes = RE::TES::GetSingleton();

			if (width > 0.0f) {
				RE::TESLandTexture* txts[5];
				std::array pts = { RE::NiPoint3{ x, y, z }, RE::NiPoint3{ x + width, y, z }, RE::NiPoint3{ x - width, y, z }, RE::NiPoint3{ x, y + width, z }, RE::NiPoint3{ x, y - width, z } };

				for (int i = 0; i < 5; i++) {
					txts[i] = tes->GetLandTexture(pts[i]);
				}

				for (auto& txt : txts) {
					if (txt && this->Textures->Contains(txt->GetFormID())) {
						logger::debug("Detected hit with landscape texture in {} with 0x{:x}",
							cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName(), txt->GetFormID());
						return false;
					}
				}
			} else {
				if (auto txt = tes->GetLandTexture(RE::NiPoint3{ x, y, z })) {
					if (this->Textures->Contains(txt->GetFormID())) {
						logger::debug("Detected hit with landscape texture in {} with 0x{:x}", cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName(), txt->GetFormID());
						return false;
					}
				}
			}
		}

		auto begin = glm::vec4(x, y, z - this->RayDepth, 0.0f);
		auto end = glm::vec4(x, y, z + this->RayHeight, 0.0f);

		Raycast::RayResult rs;

		if (Config::RayCastMode >= 1) {
			rs = Raycast::hkpPhantomCast(begin, end, cell, param);

			for (auto& [body] : rs.cdBodyHitArray) {
				if (body == nullptr) {
					continue;
				}

				const auto collisionObj = static_cast<const RE::hkpCollidable*>(body);
				const auto flags = collisionObj->GetCollisionLayer();
				unsigned long long mask = static_cast<unsigned long long>(1) << static_cast<int>(flags);
				if (!(this->RaycastMask & mask)) {
					continue;
				}

				auto rsTESForm = GetRaycastHitBaseForm(body);
				if (this->Ignore != nullptr && this->Ignore->Contains(rsTESForm)) {
					if (Config::DebugLogEnable && rsTESForm)
						logger::debug("Ignored 0x{:x} in {}", rsTESForm->formID, cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName());
					continue;
				}

				if (Config::DebugLogEnable) {
					auto sTemplate = fmt::runtime("{} {}(0x{:x})");
					auto cellName = format(sTemplate, "cell", cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName(), cell->formID);
					auto hitObjectName = rsTESForm ? format(sTemplate, "with", rsTESForm->GetFormEditorID() ? rsTESForm->GetFormEditorID() : rsTESForm->GetName(), rsTESForm->formID) : "";
					logger::debug("{}({},{},{}) detected hit {}", cellName, x, y, z, hitObjectName);
				}

				return false;
			}
		} else {
			rs = Raycast::hkpCastRay(begin, end);

			for (auto& [normal, hitFraction, body] : rs.hitArray) {
				if (body == nullptr) {
					continue;
				}

				const auto collisionObj = static_cast<const RE::hkpCollidable*>(body);
				const auto flags = collisionObj->GetCollisionLayer();
				unsigned long long mask = static_cast<unsigned long long>(1) << static_cast<int>(flags);
				if (!(this->RaycastMask & mask)) {
					continue;
				}

				auto rsTESForm = GetRaycastHitBaseForm(body);
				if (this->Ignore != nullptr && this->Ignore->Contains(rsTESForm)) {
					if (Config::DebugLogEnable && rsTESForm)
						logger::debug("Ignored 0x{:x} in {}", rsTESForm->formID, cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName());
					continue;
				}

				if (Config::DebugLogEnable) {
					auto sTemplate = fmt::runtime("{} {}(0x{:x})");
					auto cellName = format(sTemplate, "cell", cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName(), cell->formID);
					auto hitObjectName = rsTESForm ? format(sTemplate, "with", rsTESForm->GetFormEditorID() ? rsTESForm->GetFormEditorID() : rsTESForm->GetName(), rsTESForm->formID) : "";
					logger::debug("{}({},{},{}) detected hit {}", cellName, x, y, z, hitObjectName);
				}

				return false;
			}
		}

		return true;
	}

	float RaycastHelper::CreateGrassCliff(const float x, const float y, const float z, glm::vec3& Normal, RE::GrassParam* param) const
	{
		RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
		auto cell = player->GetParentCell();

		if (cell == nullptr) {
			return z;
		}

		// Currently not dealing with this.
		if (cell->IsInteriorCell() || !cell->IsAttached()) {
			return z;
		}

		RE::TESGrass* grassForm = nullptr;

		if (param) {
			grassForm = RE::TESForm::LookupByID<RE::TESGrass>(param->grassFormID);
			if (grassForm) {
				if (grassForm->GetUnderwaterState() == RE::TESGrass::GRASS_WATER_STATE::kBelowOnlyAtLeast || grassForm->GetUnderwaterState() == RE::TESGrass::GRASS_WATER_STATE::kBelowOnlyAtMost) {
					return z;
				}
			}
		}

		auto begin = glm::vec4{ x, y, z, 0.0f };
		auto end = glm::vec4{ x, y, z + 300.0f, 0.0f };

		auto rs = Raycast::hkpCastRay(begin, end);
		float retn = z;

		if (!IsCliffObject(rs))
			return z;

		glm::vec4 bestPos = begin;

		for (auto& [normal, hitFraction, body] : rs.hitArray) {
			if (hitFraction >= 1.0f || body == nullptr) {
				continue;
			}

			const auto pos = (end - begin) * hitFraction + begin;

			if (this->Cliffs == nullptr) {
				return z;
			}

			if (this->Cliffs->Contains(GetRaycastHitBaseForm(body)) && pos.z > bestPos.z) {
				normal *= -1;

				if (grassForm) {
					if (std::acosf(normal.z) > grassForm->GetMaxSlope() || std::acosf(normal.z) < grassForm->GetMinSlope())
						return z;
				}

				Normal = normal;

				retn = pos.z;
				bestPos = pos;
			}
		}

		if (param)
			param->fitsToSlope = true;

		std::vector<glm::vec4> ptsBegin;
		std::vector<glm::vec4> ptsEnd;

		if (grassForm) {
			if (grassForm->boundData.boundMax.x != 0 && grassForm->boundData.boundMin.x != 0) {
				auto widthX = std::abs(static_cast<float>(grassForm->boundData.boundMax.x - grassForm->boundData.boundMin.x) / 2);
				auto widthY = std::abs(static_cast<float>(grassForm->boundData.boundMax.y - grassForm->boundData.boundMin.y) / 2);
				auto width = std::max(widthX, widthY) + 40.0f;
				ptsBegin = { glm::vec4{ x + width, y, retn - 30.0f, 0.0f }, glm::vec4{ x - width, y, retn - 30.0f, 0.0f }, glm::vec4{ x, y + width, retn - 30.0f, 0.0f }, glm::vec4{ x, y - width, retn - 30.0f, 0.0f } };
				ptsEnd = { glm::vec4{ x + width, y, retn + 30.0f, 0.0f }, glm::vec4{ x - width, y, retn + 30.0f, 0.0f }, glm::vec4{ x, y + width, retn + 30.0f, 0.0f }, glm::vec4{ x, y - width, retn + 30.0f, 0.0f } };
			}
		}

		if (ptsBegin.empty() && ptsEnd.empty()) {
			ptsBegin = { glm::vec4{ x + 80.0f, y, retn - 30.0f, 0.0f }, glm::vec4{ x - 80.0f, y, retn - 30.0f, 0.0f }, glm::vec4{ x, y + 80.0f, retn - 30.0f, 0.0f }, glm::vec4{ x, y - 80.0f, retn - 30.0f, 0.0f } };
			ptsEnd = { glm::vec4{ x + 80.0f, y, retn + 30.0f, 0.0f }, glm::vec4{ x - 80.0f, y, retn + 30.0f, 0.0f }, glm::vec4{ x, y + 80.0f, retn + 30.0f, 0.0f }, glm::vec4{ x, y - 80.0f, retn + 30.0f, 0.0f } };
		}

		for (int i = 0; i < ptsBegin.size(); i++) {
			rs = Raycast::hkpCastRay(ptsBegin[i], ptsEnd[i]);

			if (!this->Cliffs->Contains(GetRaycastHitBaseForm(rs))) {
				return z;
			}
		}

		if (retn != z) {
			auto sTemplate = fmt::runtime("{} {}(0x{:x})");
			auto cellName = fmt::format(sTemplate, "cell", cell->GetFormEditorID() ? cell->GetFormEditorID() : cell->GetName(), cell->formID);
			auto rsTESForm = GetRaycastHitBaseForm(rs);
			auto hitObjectName = rsTESForm ? fmt::format(sTemplate, "onto", rsTESForm->GetFormEditorID() ? rsTESForm->GetFormEditorID() : rsTESForm->GetName(), rsTESForm->formID) : "";
			logger::debug("{}({},{},{}) moved grass patch {}. New height is {} and new normal (up) direction is ({},{},{})", cellName, x, y, z, hitObjectName, retn, Normal.x, Normal.y, Normal.z);
		}

		return retn;
	}

	RE::TESForm* RaycastHelper::GetRaycastHitBaseForm(const Raycast::RayResult& r)
	{
		RE::TESForm* result = nullptr;
		try {
			auto ref = r.hitObject;
			if (ref != nullptr) {
				auto bound = ref->GetBaseObject();
				if (bound != nullptr) {
					auto baseform = bound->As<RE::TESForm>();
					if (baseform != nullptr) {
						result = baseform;
						return result;
					}
				}
			}
		} catch (...) {
		}
		return result;
	}

	RE::TESForm* RaycastHelper::GetRaycastHitBaseForm(const RE::hkpCdBody* body)
	{
		RE::TESForm* result = nullptr;
		try {
			RE::TESObjectREFR* ref = nullptr;

			auto av = Raycast::getAVObject(body);
			if (av) {
				ref = av->GetUserData();
			}
			if (ref != nullptr) {
				auto bound = ref->GetBaseObject();
				if (bound != nullptr) {
					auto baseform = bound->As<RE::TESForm>();
					if (baseform != nullptr) {
						result = baseform;
						return result;
					}
				}
			}
		} catch (...) {
		}
		return result;
	}

	bool RaycastHelper::IsCliffObject(const Raycast::RayResult& r) const
	{
		return this->Cliffs->Contains(GetRaycastHitBaseForm(r));
	}
}
