#pragma once

namespace MobileGL::MG_Util::Debug::LwjglThreadBridge {
    // Observe a JNI entry made through MobileGL. On the Project Zomboid Java
    // thread this captures LWJGL's populated OpenGL capability table; on
    // ZomDroid's Android render thread it installs a private copy of the JNI
    // dispatch table that points at those same capabilities.
    void ObserveCurrentThread(const char* origin) noexcept;
}
