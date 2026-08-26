// OPT-LAB persistent GLES program-binary cache. Internal backend cache only;
// it does not change the frontend glGetProgramBinary/glProgramBinary contract.
#pragma once

#include <Includes.h>

namespace MobileGL::MG_External {
    struct GLESCapabilities;
}
namespace MobileGL::MG_State::GLState {
    class ProgramObject;
}

namespace MobileGL::MG_Backend::DirectGLES::PZProgramBinaryCache {
    Uint64 ComputeKey(const MG_State::GLState::ProgramObject& program,
                      const MG_External::GLESCapabilities& capabilities,
                      Uint32 snormClampMask,
                      Uint32 unormClampMask,
                      Uint fragColorBroadcastCount,
                      Uint esslVersion);
    Bool TryLoad(Uint backendProgram, Uint64 key);
    void PrepareForSourceLink(Uint backendProgram);
    void Store(Uint backendProgram, Uint64 key);

    // Final ESSL cache: still lets the driver compile/link normally, but skips the
    // SPIR-V rewrite + SPIRV-Cross + source-rewrite pipeline on later launches.
    Bool TryLoadShaderSource(Uint64 programKey, Uint stageIndex,
                             GLenum shaderType, String& source);
    void StoreShaderSource(Uint64 programKey, Uint stageIndex,
                           GLenum shaderType, const String& source);
} // namespace MobileGL::MG_Backend::DirectGLES::PZProgramBinaryCache
