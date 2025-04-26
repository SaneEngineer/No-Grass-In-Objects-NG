#pragma once

#include "GrassControl/mmath.h"
#include <GrassControl/Config.h>
#include <GrassControl/Util.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Raycast
{

	struct hkpGenericShapeData
	{
		intptr_t* unk;
		uint32_t shapeType;
	};

	struct rayHitShapeInfo
	{
		hkpGenericShapeData* hitShape;
		uint32_t unk;
	};

	class RayCollector
	{
	public:
		struct HitResult
		{
			glm::vec3 normal;
			float hitFraction;
			const RE::hkpCdBody* body;

			RE::NiAVObject* getAVObject();
		};

		RayCollector();
		~RayCollector() = default;

		virtual void AddRayHit(const RE::hkpCdBody& body, const RE::hkpShapeRayCastCollectorOutput& hitInfo);

		void AddFilter(const RE::NiAVObject* obj) noexcept
		{
			objectFilter.push_back(obj);
		}

		const std::vector<HitResult>& GetHits();

		void Reset();

	private:
		float earlyOutHitFraction{ 1.0f };  // 08
		std::uint32_t pad0C{};
		RE::hkpWorldRayCastOutput rayHit;  // 10

		std::vector<HitResult> hits{};
		std::vector<const RE::NiAVObject*> objectFilter{};
	};

#pragma warning(push)
#pragma warning(disable: 26495)

	typedef __declspec(align(16)) struct bhkRayResult
	{
		union
		{
			uint32_t data[16];
			struct
			{
				// Might be surface normal
				glm::vec4 rayUnkPos;
				// The normal vector of the ray
				glm::vec4 rayNormal;

				rayHitShapeInfo colA;
				rayHitShapeInfo colB;
			} s;
		} u;

		// Custom utility data, not part of the game

		// True if the trace hit something before reaching it's end position
		bool hit = false;
		// If the ray hits a havok object, this will point to its reference
		RE::TESObjectREFR* hitObject = nullptr;

		// The position the ray hit, in world space
		glm::vec4 hitPos;
		// A vector of hits to be iterated in original code
		std::vector<RayCollector::HitResult> hitArray{};

	} RayResult;
	static_assert(sizeof(RayResult) == 128);

#pragma warning(pop)

	RE::NiAVObject* getAVObject(const RE::hkpCdBody* body);

	// Cast a ray from 'start' to 'end', returning the first thing it hits
	// This variant collides with pretty much any solid geometry
	//	Params:
	//		glm::vec4 start:     Starting position for the trace in world space
	//		glm::vec4 end:       End position for the trace in world space
	//
	// Returns:
	//	RayResult:
	//		A structure holding the results of the ray cast.
	//		If the ray hit something, result.hit will be true.
	RayResult hkpCastRay(const glm::vec4& start, const glm::vec4& end) noexcept;

}

namespace GrassControl
{
	// not actually cached anything here at the moment
	class RaycastHelper
	{
	public:
		virtual ~RaycastHelper()
		{
			delete Ignore;
		}

		RaycastHelper(int version, float rayHeight, float rayDepth, const std::string& layers, Util::CachedFormList* ignored, Util::CachedFormList* textures, Util::CachedFormList* cliffs);

		const int Version = 0;

		const float RayHeight = 0.0f;

		const float RayDepth = 0.0f;

		unsigned long long RaycastMask = 0;

		Util::CachedFormList* const Ignore = nullptr;

		Util::CachedFormList* const Textures = nullptr;

		Util::CachedFormList* const Cliffs = nullptr;

		bool CanPlaceGrass(RE::TESObjectLAND* land, float x, float y, float z) const;
		float CreateGrassCliff(float x, float y, float z, glm::vec3& Normal, RE::GrassParam* param) const;

	private:
		/// @brief Iterate the Raycast Hit object and provide the first TESForm*
		/// @param r The Rayresult to iterate
		/// @return True if the predicate function returns true
		static RE::TESForm* GetRaycastHitBaseForm(const Raycast::RayResult& r);
		static RE::TESForm* GetRaycastHitBaseForm(const RE::hkpCdBody* body);

		bool IsCliffObject(const Raycast::RayResult& r) const;
	};

}
