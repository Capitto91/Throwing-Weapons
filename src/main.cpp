// Punto de entrada principal del plugin SKSE.
// Inicializa la DLL, registra el plugin en Skyrim y ejecuta la inicialización
// de los diferentes sistemas internos del mod.


SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);



    return true;
}