#pragma once

#include <GrassControl/Config.h>
#include <GrassControl/exceptionhelper.h>
#include <GrassControl/Util.h>
#include "GrassControl/mmath.h"
#include <string>
#include <vector>
#include <cstdint>

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
		std::vector<const RE::NiAVObject*> objectFilter;
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
		RE::NiAVObject* hitObject;
		RE::COL_LAYER CollisionLayer;
		// The length of the ray from start to hitPos
		float rayLength = 0.0f;
		// A vector of hits to be iterated in original code
		std::vector<Raycast::RayCollector::HitResult> hitArray;

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

namespace RayCastUtil
{
	/*
	//
	// Summary:
	//     Contains results from raycasting.
	class RayCastResult final
	{
	private:
		float _fraction = 0;

		std::vector<float> _normal;

		std::vector<float> _pos;

	public:
		std::intptr_t _obj;

		std::intptr_t _hkObj;

		//
		// Summary:
		//     Gets the havok object.
		//
		// Value:
		//     The havok object.
		intptr_t getHavokObject() const;

		//
		// Summary:
		//     Gets the object of the collision.
		//
		// Value:
		//     The object.
		RE::NiAVObject* getObject() const;

		//
		// Summary:
		//     Gets the position of the collision.
		//
		// Value:
		//     The position.
		std::vector<float> getPosition() const;

	public:
		void setPosition(const std::vector<float>& value);

		//
		// Summary:
		//     Gets the normal of the collision.
		//
		// Value:
		//     The normal.
		std::vector<float> getNormal() const;

	public:
		void setNormal(const std::vector<float>& value);

		//
		// Summary:
		//     Gets the fraction.
		//
		// Value:
		//     The fraction.
		float getFraction() const;

	public:
		void setFraction(float value);

		RayCastResult();

		static RayCastResult* GetClosestResult(std::vector<float>& source, std::vector<RayCastResult*>& results, std::vector<RE::NiAVObject>& ignore);
	};
	*/
	class RayCastParameters
	{
	private:
		glm::vec4 _begin{};

		glm::vec4 _end{};

		RE::TESObjectCELL* _cell = nullptr;

		//
		// Summary:
		//     Gets or sets the cell. This must be set.
		//
		// Value:
		//     The cell.
	public:
		virtual ~RayCastParameters()
		{
			delete _cell;
		}

		RE::TESObjectCELL* getCell() const;
		void setCell(RE::TESObjectCELL* value);

		//
		// Summary:
		//     Gets or sets the end coordinate.
		//
		// Value:
		//     The end.
		glm::vec4 getEnd() const;
		void setEnd(glm::vec4& value);

		//
		// Summary:
		//     Gets or sets the begin coordinate.
		//
		// Value:
		//     The begin.
		glm::vec4 getBegin() const;
		void setBegin(glm::vec4& value);

		//
		// Summary:
		//     Initializes a new instance of the NetScriptFramework.SkyrimSE.RayCastParameters
		//     class.
		RayCastParameters();
	};

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

		RaycastHelper(int version, float rayHeight, float rayDepth, const std::string &layers, Util::CachedFormList *ignored);

		const int Version = 0;

		const float RayHeight = 0.0f;

		const float RayDepth = 0.0f;

		unsigned long long RaycastMask = 0;

		Util::CachedFormList *const Ignore = nullptr;

		bool CanPlaceGrass(RE::TESObjectLAND *land, const float x, const float y,const float z);

	private:

		bool IsIgnoredObject(Raycast::RayResult r) const;
	};

}

