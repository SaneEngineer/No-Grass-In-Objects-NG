#pragma once


namespace mmath {
	constexpr const float half_pi = 1.57079632679485f;

	typedef struct {
		float data[4][4];
	} NiMatrix44;

	
	bool IsInf(const float f) noexcept;
	bool IsInf(const double f) noexcept;
	bool IsInf(const glm::vec3& v) noexcept;
	bool IsInf(const glm::vec4& v) noexcept;
	bool IsInf(const glm::vec<4, float, glm::packed_highp>& v) noexcept;

	bool IsNan(const float f) noexcept;
	bool IsNan(const double f) noexcept;
	bool IsNan(const glm::vec3& v) noexcept;
	bool IsNan(const glm::vec4& v) noexcept;
	bool IsNan(const glm::vec<4, float, glm::packed_highp>& v) noexcept;

	bool IsValid(const float f) noexcept;
	bool IsValid(const double f) noexcept;
	bool IsValid(const glm::vec3& v) noexcept;
	bool IsValid(const glm::vec4& v) noexcept;
	bool IsValid(const glm::vec<4, float, glm::packed_highp>& v) noexcept;
	

	// Return the forward view vector
	glm::vec3 GetViewVector(const glm::vec3& forwardRefer, float pitch, float yaw) noexcept;
	// Extracts pitch and yaw from a rotation matrix
	glm::vec3 NiMatrixToEuler(const RE::NiMatrix3& m) noexcept;
	// Decompose a position to 3 basis vectors and the coefficients, given an euler rotation
	void DecomposeToBasis(const glm::vec3& point, const glm::vec3& rotation,
		glm::vec3& forward, glm::vec3& right, glm::vec3& up, glm::vec3& coef) noexcept;
	// Construct a 3D perspective projection matrix that matches what is used by the game
	glm::mat4 Perspective(float fov, float aspect, const RE::NiFrustum& frustum) noexcept;
	// Construct a view matrix
	glm::mat4 LookAt(const glm::vec3& pos, const glm::vec3& at, const glm::vec3& up) noexcept;
	

	template<typename T, typename S>
	T Interpolate(const T from, const T to, const S scalar) noexcept {
		if (scalar > 1.0) return to;
		if (scalar < 0.0) return from;
		return from + (to - from) * scalar;
	};

	template<typename T>
	T Remap(T value, T inMin, T inMax, T outMin, T outMax) noexcept {
		return outMin + (((value - inMin) / (inMax - inMin)) * (outMax - outMin));
	};

	// Pitch/Yaw rotation
	struct Rotation {
		public:
			glm::quat quat = glm::identity<glm::quat>();
			glm::vec2 euler = { 0.0f, 0.0f };

			// Set euler angles, call UpdateQuaternion to refresh the quaternion part
			void SetEuler(float pitch, float yaw) noexcept;

			// Set with a quaternion and update euler angles
			void SetQuaternion(const glm::quat& q) noexcept;
			void SetQuaternion(const RE::NiQuaternion& q) noexcept;

			// Copy rotation from a TESObjectREFR
			void CopyFrom(const RE::TESObjectREFR* ref) noexcept;

			// Compute a quaternion after setting euler angles
			void UpdateQuaternion() noexcept;

			// Get a quaternion pointing in the opposite direction (p/y inverted)
			glm::quat InverseQuat() const noexcept;
			RE::NiQuaternion InverseNiQuat() const noexcept;

			RE::NiQuaternion ToNiQuat() const noexcept;
			RE::NiPoint2 ToNiPoint2() const noexcept;
			RE::NiPoint3 ToNiPoint3() const noexcept;

			// Get a 4x4 rotation matrix
			glm::mat4 ToRotationMatrix() noexcept;

		private:
			glm::mat4 mat = glm::identity<glm::mat4>();
			bool dirty = true;
	};

	// Smooth changes in scalar overloads
	enum class Local {
		No,
		Yes
	};
	
}
