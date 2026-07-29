// Implementación del sistema de logs.
// Gestiona la escritura de información de depuración y errores.

#include "1.- CORE/Logger.h"

namespace Logger
{
	void Init()
	{
		logs::init();
		logs::info("ThorMjolnir: logger inicializado.");
	}
}
