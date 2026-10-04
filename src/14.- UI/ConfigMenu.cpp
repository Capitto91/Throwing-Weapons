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

			float speed = Settings::GetThrowSpeed();
			if (ImGui::SliderFloat("Speed", &speed, Settings::kThrowSpeedMin, Settings::kThrowSpeedMax, "%.0f units/s", ImGui::ImGuiSliderFlags_AlwaysClamp)) {
				Settings::SetThrowSpeed(speed);
				MarkChanged();
			}
			HelpMarker("Initial speed of the thrown weapon. Default: 5000.");

			float gravity = Settings::GetThrowGravityMult();
			if (ImGui::SliderFloat("Gravity", &gravity, Settings::kThrowGravityMultMin, Settings::kThrowGravityMultMax, "%.2f", ImGui::ImGuiSliderFlags_AlwaysClamp)) {
				Settings::SetThrowGravityMult(gravity);
				MarkChanged();
			}
			HelpMarker("How strongly gravity pulls the weapon down, as a fraction of the world's gravity. 0.35 = same as vanilla arrows (default). 0 = no drop at all.");

			RenderFooter();
		}

		// Multiplicador mostrado como porcentaje (0.75 -> 75%).
		bool PercentSlider(const char* a_label, float a_mult, float& a_out)
		{
			float percent = a_mult * 100.0f;
			if (ImGui::SliderFloat(a_label, &percent, Settings::kHitMultMin * 100.0f, Settings::kHitMultMax * 100.0f, "%.0f%%", ImGui::ImGuiSliderFlags_AlwaysClamp)) {
				a_out = percent / 100.0f;
				return true;
			}
			return false;
		}

		void __stdcall RenderDamage()
		{
			ImGui::SeparatorText("Hit damage");

			float value = 0.0f;
			if (PercentSlider("Throw hit", Settings::GetThrowHitMult(), value)) {
				Settings::SetThrowHitMult(value);
				MarkChanged();
			}
			HelpMarker("Damage of the initial hit, as a percentage of the weapon's real melee damage (perks, target armor, sneak and criticals included). Default: 75%.");

			if (PercentSlider("Return hit", Settings::GetReturnHitMult(), value)) {
				Settings::SetReturnHitMult(value);
				MarkChanged();
			}
			HelpMarker("Damage of each hit while the weapon flies back to your hand, as a percentage of the weapon's real melee damage. Default: 25%.");

			ImGui::SeparatorText("Effects");

			bool stagger = Settings::GetReturnStagger();
			if (ImGui::Checkbox("Return hits stagger", &stagger)) {
				Settings::SetReturnStagger(stagger);
				MarkChanged();
			}
			HelpMarker("Enemies hit by the weapon on its way back are staggered.");

			bool hazardActor = Settings::GetHazardOnActor();
			if (ImGui::Checkbox("Electric discharge on enemies", &hazardActor)) {
				Settings::SetHazardOnActor(hazardActor);
				MarkChanged();
			}
			HelpMarker("Leaves an electric discharge when the weapon sticks into an enemy.");

			bool hazardSurface = Settings::GetHazardOnSurface();
			if (ImGui::Checkbox("Electric discharge on surfaces", &hazardSurface)) {
				Settings::SetHazardOnSurface(hazardSurface);
				MarkChanged();
			}
			HelpMarker("Leaves an electric discharge when the weapon sticks into a wall, the ground, etc.");

			bool explosion = Settings::GetImpactExplosion();
			if (ImGui::Checkbox("Impact explosion", &explosion)) {
				Settings::SetImpactExplosion(explosion);
				MarkChanged();
			}
			HelpMarker("Plays the lightning explosion on every impact of the throw, against enemies or surfaces.");

			RenderFooter();
		}
	}

	void __stdcall RenderVfx()
	{
		ImGui::SeparatorText("Visual effects");
		ImGui::TextWrapped("Purely visual.");
		ImGui::Spacing();

		bool trail = Settings::GetTrail();
		if (ImGui::Checkbox("Trail", &trail)) {
			Settings::SetTrail(trail);
			MarkChanged();
		}
		HelpMarker("Lightning trail behind the weapon while it flies.");

		bool particles = Settings::GetParticles();
		if (ImGui::Checkbox("Particles", &particles)) {
			Settings::SetParticles(particles);
			MarkChanged();
		}
		HelpMarker("Sparks around the weapon while it moves (throw, flight, recall).");

		bool weaponLight = Settings::GetWeaponLight();
		if (ImGui::Checkbox("Weapon light", &weaponLight)) {
			Settings::SetWeaponLight(weaponLight);
			MarkChanged();
		}
		HelpMarker("Glow around the hammer head");

		bool handEffect = Settings::GetHandEffect();
		if (ImGui::Checkbox("Hand effect", &handEffect)) {
			Settings::SetHandEffect(handEffect);
			MarkChanged();
		}
		HelpMarker("Brief glow on your hands when you throw.");

		bool powerAttack = Settings::GetPowerAttackEffects();
		if (ImGui::Checkbox("Power attack VFX", &powerAttack)) {
			Settings::SetPowerAttackEffects(powerAttack);
			MarkChanged();
		}
		HelpMarker("Sparks, hammer glow and light during every power attack with the weapon in hand. Independent from the throw effects above.");

		// Glow de la textura del martillo (GlowMapControl), aplicado en vivo.
		ImGui::SeparatorText("Weapon glow");

		static constexpr const char* kGlowModeItems[] = { "Off", "Constant", "Pulse" };
		int glowMode = static_cast<int>(Settings::GetGlowMode());
		if (ImGui::Combo("Glow", &glowMode, kGlowModeItems, 3)) {
			Settings::SetGlowMode(static_cast<Settings::GlowMode>(glowMode));
			MarkChanged();
		}
		HelpMarker("Glow of the hammer's own texture.");

		const bool glowOn = Settings::GetGlowMode() != Settings::GlowMode::kOff;
		ImGui::BeginDisabled(!glowOn);

		static constexpr const char* kGlowConditionItems[] = { "Always", "Near creatures" };
		int glowCondition = static_cast<int>(Settings::GetGlowCondition());
		if (ImGui::Combo("When", &glowCondition, kGlowConditionItems, 2)) {
			Settings::SetGlowCondition(static_cast<Settings::GlowCondition>(glowCondition));
			MarkChanged();
		}
		HelpMarker("Always, or only while a living creature of the checked types is within the radius. The glow fades in and out.");

		if (Settings::GetGlowCondition() == Settings::GlowCondition::kNearCreatures) {
			ImGui::Indent();

			bool dragons = Settings::GetGlowNearDragons();
			if (ImGui::Checkbox("Dragons", &dragons)) {
				Settings::SetGlowNearDragons(dragons);
				MarkChanged();
			}

			bool undead = Settings::GetGlowNearUndead();
			if (ImGui::Checkbox("Undead", &undead)) {
				Settings::SetGlowNearUndead(undead);
				MarkChanged();
			}

			bool daedra = Settings::GetGlowNearDaedra();
			if (ImGui::Checkbox("Daedra", &daedra)) {
				Settings::SetGlowNearDaedra(daedra);
				MarkChanged();
			}

			float radius = Settings::GetGlowRadius();
			if (ImGui::SliderFloat("Radius", &radius, Settings::kGlowRadiusMin, Settings::kGlowRadiusMax, "%.0f units", ImGui::ImGuiSliderFlags_AlwaysClamp)) {
				Settings::SetGlowRadius(radius);
				MarkChanged();
			}
			HelpMarker("Detection distance. Default: 2000 (about 28 meters).");

			ImGui::Unindent();
		}

		float intensity = Settings::GetGlowIntensity() * 100.0f;
		if (ImGui::SliderFloat("Intensity", &intensity, Settings::kGlowIntensityMin * 100.0f, Settings::kGlowIntensityMax * 100.0f, "%.0f%%", ImGui::ImGuiSliderFlags_AlwaysClamp)) {
			Settings::SetGlowIntensity(intensity / 100.0f);
			MarkChanged();
		}
		HelpMarker("Brightness relative to the original mesh. Default: 100%. In Pulse mode, this is the peak.");

		if (Settings::GetGlowMode() == Settings::GlowMode::kPulse) {
			float pulseSpeed = Settings::GetGlowPulseSpeed();
			if (ImGui::SliderFloat("Pulse speed", &pulseSpeed, Settings::kGlowPulseSpeedMin, Settings::kGlowPulseSpeedMax, "%.1f per second", ImGui::ImGuiSliderFlags_AlwaysClamp)) {
				Settings::SetGlowPulseSpeed(pulseSpeed);
				MarkChanged();
			}
			HelpMarker("Pulses per second. Default: 1.");
		}

		ImGui::EndDisabled();

		RenderFooter();
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

		// Vive toda la sesión: no se libera.
		(void)SKSEMenuFramework::AddInputEvent(OnInputEvent);

		logs::info("UI::ConfigMenu: sección \"{}\" registrada en SKSE Menu Framework {:.2f}.", kSectionName, SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
