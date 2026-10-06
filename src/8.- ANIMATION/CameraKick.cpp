// Golpe de cámara al atrapar -- ver CameraKick.h.

#include "8.- ANIMATION/CameraKick.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/FrameHook.h"
#include "9.- MATH/RotationMath.h"

#include <atomic>
#include <cmath>
#include <mutex>
#include <numbers>

namespace Animation::CameraKick
{
	namespace
	{
		// Posición de TESCameraState::GetRotation en la vtable de SE/AE (TESCameraState.h).
		constexpr std::size_t kGetRotationVtableIndex = 0x04;

		std::atomic<bool> g_installed{ false };

		// Golpe en curso: inclinación(t) = amplitude · e^(-decayRate·t) · sin(angularFrequency·t), con t desde
		// startTime (FrameHook::Now). Lo escribe Start (cualquier hilo) y lo lee el hook (hilo principal).
		struct Kick
		{
			double startTime{ 0.0 };
			float  amplitude{ 0.0f };         // rad, escalada para que el primer pico valga el ángulo pedido
			float  decayRate{ 0.0f };         // 1/s
			float  angularFrequency{ 0.0f };  // rad/s
			float  peakTime{ 0.0f };          // s, primer pico (hacia arriba)
			float  endTime{ 0.0f };           // s, primer paso por cero tras la duración pedida
			bool   active{ false };
			bool   peakLogged{ false };
		};

		std::mutex g_mutex;
		Kick       g_kick;

		std::atomic<float> g_appliedPitch{ 0.0f };

		// Inclinación del golpe ahora mismo (0 sin golpe; al pasar endTime lo da por terminado).
		// a_firstPeak vale true una sola vez por golpe, al pasar el primer pico.
		float CurrentPitch(bool& a_firstPeak, bool& a_finished)
		{
			std::scoped_lock lock(g_mutex);
			if (!g_kick.active) {
				return 0.0f;
			}

			const float t = static_cast<float>(FrameHook::Now() - g_kick.startTime);
			if (t >= g_kick.endTime) {
				g_kick.active = false;
				a_finished = true;
				return 0.0f;
			}
			if (t <= 0.0f) {
				return 0.0f;
			}

			if (!g_kick.peakLogged && t >= g_kick.peakTime) {
				g_kick.peakLogged = true;
				a_firstPeak = true;
			}
			return g_kick.amplitude * std::exp(-g_kick.decayRate * t) * std::sin(g_kick.angularFrequency * t);
		}

		// Suma la inclinación a la rotación de la cámara: giro alrededor de su eje X local, del avance (Y) hacia arriba (Z).
		void ApplyKick(RE::NiQuaternion& a_rotation)
		{
			bool        firstPeak = false;
			bool        finished = false;
			const float pitch = CurrentPitch(firstPeak, finished);
			g_appliedPitch.store(pitch);

			if (finished) {
				logs::info("CameraKick: golpe terminado.");
			}
			if (pitch == 0.0f) {
				return;
			}

			const float   c = std::cos(pitch);
			const float   s = std::sin(pitch);
			RE::NiMatrix3 kick;
			kick.entry[1][1] = c;
			kick.entry[1][2] = -s;
			kick.entry[2][1] = s;
			kick.entry[2][2] = c;

			const auto base = a_rotation.ToRotation();
			const auto kicked = base * kick;
			a_rotation = RE::NiQuaternion(kicked);

			// Comprobación del sentido en el log: con el golpe hacia arriba, la Z del avance sube.
			if (firstPeak) {
				logs::info("CameraKick: primer pico {:.2f}°, Z del avance {:.3f} -> {:.3f}.",
					pitch * 180.0f / std::numbers::pi_v<float>, base.GetVectorY().z, kicked.GetVectorY().z);
			}
		}

		// Primero el original (el juego y los hooks de otros mods); después se suma el golpe a su resultado.
		template <class State>
		struct GetRotationHook
		{
			static void thunk(State* a_this, RE::NiQuaternion& a_rotation)
			{
				func(a_this, a_rotation);

				// Ninguna excepción debe cruzar al motor.
				try {
					ApplyKick(a_rotation);
				} catch (...) {
					logs::error("CameraKick: excepción al aplicar el golpe de cámara.");
				}
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	bool Install()
	{
		if (REL::Module::IsVR()) {
			logs::warn("CameraKick: VR sin soporte, el atrape usa el temblor de cámara vanilla.");
			return false;
		}

		REL::Relocation<std::uintptr_t> firstPersonVtbl{ RE::VTABLE_FirstPersonState[0] };
		GetRotationHook<RE::FirstPersonState>::func = firstPersonVtbl.write_vfunc(kGetRotationVtableIndex, GetRotationHook<RE::FirstPersonState>::thunk);

		REL::Relocation<std::uintptr_t> thirdPersonVtbl{ RE::VTABLE_ThirdPersonState[0] };
		GetRotationHook<RE::ThirdPersonState>::func = thirdPersonVtbl.write_vfunc(kGetRotationVtableIndex, GetRotationHook<RE::ThirdPersonState>::thunk);

		g_installed.store(true);
		logs::info("CameraKick: hooks de GetRotation de FirstPersonState y ThirdPersonState instalados.");
		return true;
	}

	bool Start(float a_angleDegrees, float a_durationSeconds, float a_bouncesPerSecond)
	{
		if (!g_installed.load() || !(a_durationSeconds > 0.0f) || !(a_bouncesPerSecond > 0.0f)) {
			return false;
		}

		// Envolvente que baja a kCameraKickEndAmplitudeFraction al acabar la duración; el golpe termina en el paso
		// por cero siguiente para no dejar un salto. La amplitud se escala para que el primer pico valga el ángulo.
		const float angularFrequency = 2.0f * std::numbers::pi_v<float> * a_bouncesPerSecond;
		const float decayRate = std::log(1.0f / Constants::kCameraKickEndAmplitudeFraction) / a_durationSeconds;
		const float peakTime = std::atan(angularFrequency / decayRate) / angularFrequency;
		const float peakShape = std::exp(-decayRate * peakTime) * std::sin(angularFrequency * peakTime);
		const float halfPeriod = std::numbers::pi_v<float> / angularFrequency;
		const float endTime = std::ceil(a_durationSeconds / halfPeriod) * halfPeriod;

		{
			std::scoped_lock lock(g_mutex);
			g_kick.startTime = FrameHook::Now();
			g_kick.amplitude = Math::DegreesToRadians(a_angleDegrees) / peakShape;
			g_kick.decayRate = decayRate;
			g_kick.angularFrequency = angularFrequency;
			g_kick.peakTime = peakTime;
			g_kick.endTime = endTime;
			g_kick.active = true;
			g_kick.peakLogged = false;
		}

		logs::info("CameraKick: golpe de {:.1f}° hacia arriba, {:.2f} s ({:.2f} s hasta el último paso por cero), {:.1f} rebotes/s, primer pico a los {:.3f} s.",
			a_angleDegrees, a_durationSeconds, endTime, a_bouncesPerSecond, peakTime);
		return true;
	}

	float GetAppliedPitch() noexcept
	{
		return g_appliedPitch.load();
	}
}
