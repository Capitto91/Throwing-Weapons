// Reproduce los sonidos del arma directamente desde su .wav.

#pragma once

namespace Audio
{
	// Reproduce a_filePath (ruta relativa a Data) una vez en a_position.
	void PlayFileOneShot(const RE::NiPoint3& a_position, const char* a_filePath, float a_volume);

	// Reproduce a volumen 0 los cuatro sonidos del arma, porque el primer uso no suena.
	// Lo llama EventManager en kDataLoaded.
	void WarmUpAll();
}
