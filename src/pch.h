#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace logs = SKSE::log;

// Necesario para los headers vendorizados de la API de Open Animation
// Replacer (ver 13.- EXTERNAL/OpenAnimationReplacer), que usan literales
// "..."sv sin incluir su propio "using" -- asumen que el proyecto que los
// vendoriza ya lo tiene, patrón habitual en plantillas de commonlibsse-ng.
using namespace std::literals;
