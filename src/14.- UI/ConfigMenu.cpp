// Menú de configuración -- ver ConfigMenu.h. ImGui vía ImGuiMCP (DLL del framework); textos en inglés.

#include "14.- UI/ConfigMenu.h"

#include "1.- CORE/Settings.h"

// Avisos del header del framework (código de terceros).
#pragma warning(push)
#pragma warning(disable: 4996 4099 5054)
#include "13.- EXTERNAL/SKSEMenuFramework/SKSEMenuFramework.h"
#pragma warning(pop)

#include <atomic>
#include <string>

namespace UI::ConfigMenu
{
	namespace
	{
		namespace ImGui = ImGuiMCP;

		constexpr const char* kSectionName = "Throwable Kyne's Thunder";

		// Cambios aplicados pero sin guardar en el INI.
		std::atomic<bool> g_unsaved{ false };

		// Resultado del último guardado, mostrado bajo los botones.
		std::string g_statusMessage;

		// Captura de tecla activa: la siguiente pulsación pasa a ser la tecla (se consume).
		std::atomic<bool> g_capturingKey{ false };

		// Nombre de la tecla según el motor, o el código numérico.
		std::string GetButtonName(const Settings::ActionBinding& a_binding)
		{
			RE::BSFixedString name;
			auto*             devices = RE::BSInputDeviceManager::GetSingleton();
			if (devices && devices->GetButtonNameFromID(a_binding.device, static_cast<std::int32_t>(a_binding.keyCode), name) && !name.empty()) {
				return std::string(name.c_str());
			}
			return std::to_string(a_binding.keyCode);
		}

		void MarkChanged()
		{
			g_unsaved = true;
			g_statusMessage.clear();
		}

		bool __stdcall OnInputEvent(RE::InputEvent* a_event)
		{
			if (!g_capturingKey || !a_event) {
				return false;
			}

			const auto* button = a_event->AsButtonEvent();
			if (!button || !button->IsDown()) {
				// Mientras se captura se consumen todos los botones.
				return button != nullptr;
			}

			const auto device = button->GetDevice();
			const auto keyCode = button->GetIDCode();

			// Escape cancela la captura sin cambiar nada.
			if (device == RE::INPUT_DEVICE::kKeyboard && keyCode == RE::BSKeyboardDevice::Keys::kEscape) {
				g_capturingKey = false;
				return true;
			}

			// Solo los tres dispositivos que entiende Settings/InputManager.
			if (device != RE::INPUT_DEVICE::kKeyboard && device != RE::INPUT_DEVICE::kMouse && device != RE::INPUT_DEVICE::kGamepad) {
				return false;
			}

			Settings::SetActionBinding({ device, keyCode });
			g_capturingKey = false;
			// Solo el atómico: g_statusMessage pertenece al hilo de render.
			g_unsaved = true;
			logs::info("UI::ConfigMenu: nueva tecla capturada -- {} {}.", Settings::DeviceToString(device), keyCode);
			return true;
		}

		void HelpMarker(const char* a_text)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			ImGui::SetItemTooltip("%s", a_text);
		}

		// Controles ligados a un ajuste de Settings: muestran su valor, lo aplican al momento al cambiarlo y marcan
		// cambios sin guardar. Con a_help, "(?)" con esa ayuda al lado.
		void SettingCheckbox(const char* a_label, bool (*a_get)(), void (*a_set)(bool), const char* a_help = nullptr)
		{
			bool value = a_get();
			if (ImGui::Checkbox(a_label, &value)) {
				a_set(value);
				MarkChanged();
			}
			if (a_help) {
				HelpMarker(a_help);
			}
		}

		void SettingSlider(const char* a_label, float (*a_get)(), void (*a_set)(float), float a_min, float a_max, const char* a_format, const char* a_help)
		{
			float value = a_get();
			if (ImGui::SliderFloat(a_label, &value, a_min, a_max, a_format, ImGui::ImGuiSliderFlags_AlwaysClamp)) {
				a_set(value);
				MarkChanged();
			}
			HelpMarker(a_help);
		}

		// Multiplicador mostrado como porcentaje (0.75 -> 75%).
		void SettingPercentSlider(const char* a_label, float (*a_get)(), void (*a_set)(float), float a_min, float a_max, const char* a_help)
		{
			float percent = a_get() * 100.0f;
			if (ImGui::SliderFloat(a_label, &percent, a_min * 100.0f, a_max * 100.0f, "%.0f%%", ImGui::ImGuiSliderFlags_AlwaysClamp)) {
				a_set(percent / 100.0f);
				MarkChanged();
			}
			HelpMarker(a_help);
		}

		// Desplegable de un ajuste enumerado; a_items va en el orden de los valores de E.
		template <class E, std::size_t N>
		void SettingCombo(const char* a_label, E (*a_get)(), void (*a_set)(E), const char* const (&a_items)[N], const char* a_help)
		{
			int value = static_cast<int>(a_get());
			if (ImGui::Combo(a_label, &value, a_items, static_cast<int>(N))) {
				a_set(static_cast<E>(value));
				MarkChanged();
			}
			HelpMarker(a_help);
		}

		// Guardar / restaurar, común a todas las páginas.
		void RenderFooter()
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Save")) {
				if (Settings::Save()) {
					g_unsaved = false;
					g_statusMessage = "Settings saved.";
				} else {
					g_statusMessage = "Could not write the INI file (see the plugin log).";
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Restore defaults")) {
				Settings::ResetToDefaults();
				g_capturingKey = false;
				MarkChanged();
			}
			HelpMarker("Restores every setting of this mod (all pages) to its default value. Press Save to keep them.");

			if (g_unsaved) {
				ImGui::TextColored(ImGui::ImVec4{ 1.0f, 0.8f, 0.3f, 1.0f }, "Changes are already active in game, but not saved yet.");
			} else if (!g_statusMessage.empty()) {
				ImGui::TextUnformatted(g_statusMessage.c_str());
			}
		}

		void __stdcall RenderControls()
		{
			const auto binding = Settings::GetActionBinding();

			ImGui::SeparatorText("Throw / recall");
			ImGui::Text("Current: %s - %s", Settings::DeviceToString(binding.device), GetButtonName(binding).c_str());

			if (g_capturingKey) {
				ImGui::TextColored(ImGui::ImVec4{ 0.4f, 0.8f, 1.0f, 1.0f }, "Press any key, mouse button or gamepad button... (Esc to cancel)");
				if (ImGui::Button("Cancel")) {
					g_capturingKey = false;
				}
			} else if (ImGui::Button("Set key...")) {
				g_capturingKey = true;
			}
			HelpMarker("A single tap throws the weapon, or recalls it while it is thrown or stuck.");

			RenderFooter();
		}

		void __stdcall RenderThrow()
		{
			ImGui::SeparatorText("Trajectory");

			SettingSlider("Speed", Settings::GetThrowSpeed, Settings::SetThrowSpeed, Settings::kThrowSpeedMin, Settings::kThrowSpeedMax, "%.0f units/s",
				"Initial speed of the thrown weapon. Default: 5000.");
			SettingSlider("Gravity", Settings::GetThrowGravityMult, Settings::SetThrowGravityMult, Settings::kThrowGravityMultMin, Settings::kThrowGravityMultMax, "%.2f",
				"How strongly gravity pulls the weapon down, as a fraction of the world's gravity. 0.35 = same as vanilla arrows (default). 0 = no drop at all.");

			RenderFooter();
		}

		void __stdcall RenderDamage()
		{
			ImGui::SeparatorText("Hit damage");

			SettingPercentSlider("Throw hit", Settings::GetThrowHitMult, Settings::SetThrowHitMult, Settings::kHitMultMin, Settings::kHitMultMax,
				"Damage of the initial hit, as a percentage of the weapon's real melee damage (perks, target armor, sneak and criticals included). Default: 75%.");
			SettingPercentSlider("Return hit", Settings::GetReturnHitMult, Settings::SetReturnHitMult, Settings::kHitMultMin, Settings::kHitMultMax,
				"Damage of each hit while the weapon flies back to your hand, as a percentage of the weapon's real melee damage. Default: 25%.");

			ImGui::SeparatorText("Effects");

			SettingCheckbox("Return hits stagger", Settings::GetReturnStagger, Settings::SetReturnStagger,
				"Enemies hit by the weapon on its way back are staggered.");
			SettingCheckbox("Electric discharge on enemies", Settings::GetHazardOnActor, Settings::SetHazardOnActor,
				"Leaves an electric discharge when the weapon sticks into an enemy.");
			SettingCheckbox("Electric discharge on surfaces", Settings::GetHazardOnSurface, Settings::SetHazardOnSurface,
				"Leaves an electric discharge when the weapon sticks into a wall, the ground, etc.");
			SettingCheckbox("Impact explosion", Settings::GetImpactExplosion, Settings::SetImpactExplosion,
				"Plays the lightning explosion on every impact of the throw, against enemies or surfaces.");

			RenderFooter();
		}

		void __stdcall RenderVfx()
		{
			ImGui::SeparatorText("Visual effects");
			ImGui::TextWrapped("Purely visual.");
			ImGui::Spacing();

			SettingCheckbox("Trail", Settings::GetTrail, Settings::SetTrail,
				"Lightning trail behind the weapon while it flies.");
			SettingCheckbox("Particles", Settings::GetParticles, Settings::SetParticles,
				"Sparks around the weapon while it moves (throw, flight, recall).");
			SettingCheckbox("Weapon light", Settings::GetWeaponLight, Settings::SetWeaponLight,
				"Glow around the hammer head");
			SettingCheckbox("Hand effect", Settings::GetHandEffect, Settings::SetHandEffect,
				"Brief glow on your hands when you throw.");
			SettingCheckbox("Power attack VFX", Settings::GetPowerAttackEffects, Settings::SetPowerAttackEffects,
				"Sparks, hammer glow and light during every power attack with the weapon in hand. Independent from the throw effects above.");

			// Chispas (WeaponVFX), aplicado en vivo.
			ImGui::SeparatorText("Sparks");

			SettingPercentSlider("Amount", Settings::GetParticleAmount, Settings::SetParticleAmount, Settings::kParticleAmountMin, Settings::kParticleAmountMax,
				"Sparks emitted per second, relative to the original mesh. Default: 100%. Also applies to power attack sparks. Combined with a long lifetime, very high values are capped by the mesh's particle limit.");
			SettingPercentSlider("Lifetime", Settings::GetParticleLifetime, Settings::SetParticleLifetime, Settings::kParticleLifetimeMin, Settings::kParticleLifetimeMax,
				"How long each spark lasts, relative to the original mesh. Default: 100%. Longer sparks leave a longer trail behind the weapon.");

			// Glow de la textura del martillo (GlowMapControl), aplicado en vivo.
			ImGui::SeparatorText("Weapon glow");

			static constexpr const char* kGlowModeItems[] = { "Off", "Constant", "Pulse" };
			SettingCombo("Glow", Settings::GetGlowMode, Settings::SetGlowMode, kGlowModeItems,
				"Glow of the hammer's own texture.");

			const bool glowOn = Settings::GetGlowMode() != Settings::GlowMode::kOff;
			ImGui::BeginDisabled(!glowOn);

			static constexpr const char* kGlowConditionItems[] = { "Always", "Near creatures" };
			SettingCombo("When", Settings::GetGlowCondition, Settings::SetGlowCondition, kGlowConditionItems,
				"Always, or only while a living creature of the checked types is within the radius. The glow fades in and out.");

			if (Settings::GetGlowCondition() == Settings::GlowCondition::kNearCreatures) {
				ImGui::Indent();

				SettingCheckbox("Dragons", Settings::GetGlowNearDragons, Settings::SetGlowNearDragons);
				SettingCheckbox("Undead", Settings::GetGlowNearUndead, Settings::SetGlowNearUndead);
				SettingCheckbox("Daedra", Settings::GetGlowNearDaedra, Settings::SetGlowNearDaedra);
				SettingSlider("Radius", Settings::GetGlowRadius, Settings::SetGlowRadius, Settings::kGlowRadiusMin, Settings::kGlowRadiusMax, "%.0f units",
					"Detection distance. Default: 2000 (about 28 meters).");

				ImGui::Unindent();
			}

			SettingPercentSlider("Intensity", Settings::GetGlowIntensity, Settings::SetGlowIntensity, Settings::kGlowIntensityMin, Settings::kGlowIntensityMax,
				"Brightness relative to the original mesh. Default: 100%. In Pulse mode, this is the peak.");

			if (Settings::GetGlowMode() == Settings::GlowMode::kPulse) {
				SettingSlider("Pulse speed", Settings::GetGlowPulseSpeed, Settings::SetGlowPulseSpeed, Settings::kGlowPulseSpeedMin, Settings::kGlowPulseSpeedMax, "%.1f per second",
					"Pulses per second. Default: 1.");
			}

			ImGui::EndDisabled();

			// Golpe de cámara al atrapar (CameraKick), leído en cada atrape.
			ImGui::SeparatorText("Camera");

			SettingCheckbox("Camera shake on catch", Settings::GetCameraShake, Settings::SetCameraShake,
				"When the weapon lands back in your hand, the camera snaps upwards like a blow to the forehead, then springs back and forth, each time less, until it settles.");

			ImGui::BeginDisabled(!Settings::GetCameraShake());

			SettingSlider("Strength", Settings::GetCameraShakeAngle, Settings::SetCameraShakeAngle, Settings::kCameraShakeAngleMin, Settings::kCameraShakeAngleMax, "%.1f deg",
				"Upward angle of the initial jolt. The bounces after it are smaller. Default: 6 deg.");
			SettingSlider("Duration", Settings::GetCameraShakeDuration, Settings::SetCameraShakeDuration, Settings::kCameraShakeDurationMin, Settings::kCameraShakeDurationMax, "%.2f s",
				"Time until the bounces die out. Default: 1.00 s.");
			SettingSlider("Bounce speed", Settings::GetCameraShakeFrequency, Settings::SetCameraShakeFrequency, Settings::kCameraShakeFrequencyMin, Settings::kCameraShakeFrequencyMax, "%.1f per second",
				"Back-and-forth movements per second after the jolt. Higher values give quicker, tighter bounces. Default: 3.");

			ImGui::EndDisabled();

			RenderFooter();
		}

		void __stdcall RenderDebug()
		{
			ImGui::SeparatorText("Diagnostics");
			ImGui::Spacing();

			SettingCheckbox("Performance log", Settings::GetPerformanceLog, Settings::SetPerformanceLog,
				"Writes a line to ThorMjolnir_OAR.log every 10 seconds: time the mod spends per frame (average and peak), frame time, and active loops and timers. For diagnostics only; leave it off for normal play.");

			RenderFooter();
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logs::info("UI::ConfigMenu: SKSE Menu Framework no está instalado, sin menú en el juego (configuración solo por INI).");
			return;
		}

		SKSEMenuFramework::SetSection(kSectionName);
		SKSEMenuFramework::AddSectionItem("Controls", RenderControls);
		SKSEMenuFramework::AddSectionItem("Throw", RenderThrow);
		SKSEMenuFramework::AddSectionItem("Damage", RenderDamage);
		SKSEMenuFramework::AddSectionItem("VFX", RenderVfx);
		SKSEMenuFramework::AddSectionItem("Debug", RenderDebug);

		// Vive toda la sesión: no se libera.
		(void)SKSEMenuFramework::AddInputEvent(OnInputEvent);

		logs::info("UI::ConfigMenu: sección \"{}\" registrada en SKSE Menu Framework {:.2f}.", kSectionName, SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
