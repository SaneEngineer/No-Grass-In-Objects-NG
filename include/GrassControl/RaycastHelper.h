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

	public:
		RayCollector();
		~RayCollector() = default;

		virtual void AddRayHit(const RE::hkpCdBody& body, const RE::hkpShapeRayCastCollectorOutput& hitInfo);

		inline void AddFilter(const RE::NiAVObject* obj) noexcept
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
			};
		};

		// Custom utility data, not part of the game

		// True if the trace hit something before reaching it's end position
		bool hit = false;
		// If the ray hit a character actor, this will point to it
		RE::Character* hitCharacter = nullptr;
		// If the ray hits a havok object, this will point to it
		RE::NiAVObject* hitObject = nullptr;
		RE::COL_LAYER CollisionLayer;
		// The length of the ray from start to hitPos
		float rayLength = 0.0f;
		// A vector of hits to be iterated in original code
		std::vector<RayCollector::HitResult> hitArray{};

		// pad to 128
		//uint64_t _pad{};
	} RayResult;
	static_assert(sizeof(RayResult) == 128);
#pragma warning(pop)

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

		RaycastHelper(int version, float rayHeight, float rayDepth, const std::string& layers, Util::CachedFormList* ignored);

		const int Version = 0;

		const float RayHeight = 0.0f;

		const float RayDepth = 0.0f;

		unsigned long long RaycastMask = 0;

		Util::CachedFormList* const Ignore = nullptr;

		bool CanPlaceGrass(RE::TESObjectLAND* land, const float x, const float y, const float z) const;

	private:
		/// @brief Iterate the Raycast Hit object and provide the first TESForm*
		/// @param r The Rayresult to iterate
		/// @return True if the predicate function returns true
		RE::TESForm* GetRaycastHitBaseForm(Raycast::RayResult r) const;

		/// @brief Iterate the Raycast Hit object and use provided test func
		/// @param r The Rayresult to iterate
		/// @param func The predicate function that tests based on formID
		/// @return True if the predicate function returns true
		bool IsRaycastHitTest(Raycast::RayResult r, std::function<bool(RE::FormID)> func) const;
		bool IsIgnoredObject(Raycast::RayResult r) const;
	};

}
