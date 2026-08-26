// MobileGL - MobileGL/MG_State/GLState/FramebufferState/FramebufferObject.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "FramebufferObject.h"
#include "MG_Util/Types.h"
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
#include <MG_Util/Texture/PZF13NullTextureProducer.h>
#endif
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
#include <MG_Util/Debug/Log.h>
#include <MG_Util/Texture/PZF14ClearToDetachState.h>
#endif
#ifdef MOBILEPZ_V1_CANDIDATE
#include <MG_Util/PZV1/PZV1QuadTargetTracker.h>
#endif

namespace MobileGL::MG_State::GLState {
    // FramebufferAttachmentObject
    FramebufferAttachmentObject::FramebufferAttachmentObject(
        const SharedPtr<MG_State::GLState::ITextureObject>& texture, TextureUploadTarget textureUploadTarget, Int level,
        Int layer, Bool layered)
        : m_texture(texture), m_textureUploadTarget(textureUploadTarget), m_textureLevel(level),
          m_textureLayer(layer), m_layered(layered) {}
    FramebufferAttachmentObject::FramebufferAttachmentObject(const SharedPtr<RenderbufferObject>& renderbuffer)
        : m_renderbuffer(renderbuffer) {}
    FramebufferAttachmentObject::FramebufferAttachmentObject(Bool IsValid)
        : m_texture(nullptr), m_renderbuffer(nullptr) {
        m_isValid = IsValid;
    }

    Bool FramebufferAttachmentObject::IsTexture() const {
        return m_texture != nullptr;
    }

    Bool FramebufferAttachmentObject::IsRenderbuffer() const {
        return m_renderbuffer != nullptr;
    }

    Bool FramebufferAttachmentObject::IsEmpty() const {
        return m_texture == nullptr && m_renderbuffer == nullptr;
    }

    const SharedPtr<MG_State::GLState::ITextureObject>& FramebufferAttachmentObject::GetTexture() const {
        return m_texture;
    }

    const SharedPtr<RenderbufferObject>& FramebufferAttachmentObject::GetRenderbuffer() const {
        return m_renderbuffer;
    }

    Int FramebufferAttachmentObject::GetTextureLevel() const {
        return m_textureLevel;
    }

    Int FramebufferAttachmentObject::GetTextureLayer() const {
        return m_textureLayer;
    }

    Bool FramebufferAttachmentObject::IsLayered() const {
        return m_layered;
    }

    TextureUploadTarget FramebufferAttachmentObject::GetTextureUploadTarget() const {
        return m_textureUploadTarget;
    }

    Bool FramebufferAttachmentObject::IsComplete() const {
        if (IsTexture()) {
            Bool complete = m_texture->IsComplete();
            return complete;
        } else if (IsRenderbuffer()) {
            Bool complete = m_renderbuffer->IsAllocated();
            return complete;
        }

        return false;
    }

    IntVec3 FramebufferAttachmentObject::GetSize() const {
        if (IsTexture()) {
            MOBILEGL_ASSERT(nullptr != static_cast<MG_State::GLState::TextureObjectMipmap*>(m_texture.get()),
                            "Texture object here should always be an object with mipmap");
            auto textureMipmapObject = static_cast<MG_State::GLState::TextureObjectMipmap*>(m_texture.get());
            TextureUploadTarget resolvedTarget = m_textureUploadTarget;
            if (resolvedTarget == TextureUploadTarget::Unknown) {
                const auto& uploadTargets = m_texture->GetUploadTargets();
                MOBILEGL_ASSERT(!uploadTargets.empty(),
                                "FramebufferAttachmentObject::GetSize: textureId=%u exposes no upload targets",
                                m_texture->GetExternalIndex());
                resolvedTarget = uploadTargets[0];
            }
            return textureMipmapObject->GetMipmapTexelSize(resolvedTarget, m_textureLevel);
        } else if (IsRenderbuffer()) {
            return {m_renderbuffer->GetWidth(), m_renderbuffer->GetHeight(), 1};
        }
        return {0, 0, 0};
    }

    Bool FramebufferAttachmentObject::IsValid() const {
        return m_isValid;
    }

    // FramebufferObject
    FramebufferObject::FramebufferObject(Uint externalIndex)
        : m_externalIndex(externalIndex), m_attachmentVersions{}, m_drawBuffers{} {
        m_attachmentObjects.fill(FramebufferAttachmentObject(false));
        m_drawBuffers.fill(FramebufferAttachmentType::None);
        const FramebufferAttachmentType defaultColorBuffer =
            (externalIndex == 0) ? FramebufferAttachmentType::BackLeft : FramebufferAttachmentType::Color0;
        m_drawBuffers[0] = defaultColorBuffer;
        m_readBuffer = defaultColorBuffer;
        m_attachmentVersions.fill(0);
    }

    FramebufferObject::~FramebufferObject() {
#ifdef MOBILEPZ_V1_CANDIDATE
        // Deletion/name reuse must not inherit a predecessor's target window.
        ::MobilePZ::V1::DeactivateFramebuffer(m_externalIndex);
#endif
    }

    void FramebufferObject::AttachTexture(FramebufferAttachmentType type, const SharedPtr<ITextureObject>& texture,
                                          TextureUploadTarget textureUploadTarget, int level, int layer, Bool layered) {
#ifdef MOBILEPZ_V1_CANDIDATE
        const auto& pzV1Previous = m_attachmentObjects[static_cast<SizeT>(type)];
        if (pzV1Previous.IsTexture() && pzV1Previous.GetTexture()) {
            ::MobilePZ::V1::DeactivateFramebuffer(
                m_externalIndex, pzV1Previous.GetTexture()->GetLifetimeId(), static_cast<Int>(type));
        }
#endif
        m_attachmentObjects[static_cast<SizeT>(type)] =
            FramebufferAttachmentObject(texture, textureUploadTarget, level, layer, layered);
        BumpAttachmentVersion(type);
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
        if (texture) {
            ::MobilePZ::PZF13::RecordAttachment(texture->GetLifetimeId(), texture->GetExternalIndex(),
                                                 m_externalIndex, static_cast<Int>(type));
        }
#endif
    }

    void FramebufferObject::AttachRenderbuffer(FramebufferAttachmentType type,
                                               const SharedPtr<RenderbufferObject>& renderbuffer) {
#ifdef MOBILEPZ_V1_CANDIDATE
        const auto& pzV1Previous = m_attachmentObjects[static_cast<SizeT>(type)];
        if (pzV1Previous.IsTexture() && pzV1Previous.GetTexture()) {
            ::MobilePZ::V1::DeactivateFramebuffer(
                m_externalIndex, pzV1Previous.GetTexture()->GetLifetimeId(), static_cast<Int>(type));
        }
#endif
        m_attachmentObjects[static_cast<SizeT>(type)] = FramebufferAttachmentObject(renderbuffer);
        BumpAttachmentVersion(type);
    }

    void FramebufferObject::Detach(FramebufferAttachmentType type) {
#ifdef MOBILEPZ_V1_CANDIDATE
        const auto& pzV1Previous = m_attachmentObjects[static_cast<SizeT>(type)];
        if (pzV1Previous.IsTexture() && pzV1Previous.GetTexture()) {
            ::MobilePZ::V1::PublishAndDeactivateFramebuffer(
                m_externalIndex, pzV1Previous.GetTexture()->GetLifetimeId(),
                static_cast<Int>(type));
        }
#endif
#ifdef MOBILEPZ_PZF13_NULL_TEXTURE_PRODUCER_LINEAGE
        const auto& previous = m_attachmentObjects[static_cast<SizeT>(type)];
        if (previous.IsTexture() && previous.GetTexture()) {
            const auto& texture = previous.GetTexture();
#ifdef MOBILEPZ_PZF14_CLEAR_TO_DETACH_DRAW_ROUTE
            const auto result = ::MobilePZ::PZF14::RecordDetachment(
                texture->GetLifetimeId(), m_externalIndex, static_cast<Int>(type));
            if (result.observed && result.shouldLog) {
                const auto& window = result.window;
                MGLOG_I("MOBILEPZ_PZF14_SESSION generation=%u classification=%s "
                        "texture_lifetime=%llu front_texture=%u front_fbo=%u attachment=%d "
                        "client_sequence=%llu open_sequence=%llu close_sequence=%llu "
                        "clear_writes=%llu non_draw_writes=%llu raw_draws_expected_fbo=%llu "
                        "raw_draws_other_fbo=%llu driver_state_queries=%llu "
                        "backend_fbo_mismatches=%llu backend_fbo_repairs=%llu "
                        "program_mismatches=%llu program_repairs=%llu last_front_fbo=%u "
                        "last_expected_backend_fbo=%u last_driver_fbo_before=%d "
                        "last_driver_fbo_after=%d last_front_program=%u "
                        "last_expected_backend_program=%u last_driver_program_before=%d "
                        "last_driver_program_after=%d last_thread_hash=%llu "
                        "rendering_output_mutation=%d additional_gl_calls=0 error_drain=0",
                        window.generation, ::MobilePZ::PZF14::ClassificationName(window),
                        static_cast<unsigned long long>(window.lifetime), window.frontTexture,
                        window.frontFbo, window.attachment,
                        static_cast<unsigned long long>(window.clientSequence),
                        static_cast<unsigned long long>(window.openSequence),
                        static_cast<unsigned long long>(window.closeSequence),
                        static_cast<unsigned long long>(window.clearWrites),
                        static_cast<unsigned long long>(window.nonDrawWrites),
                        static_cast<unsigned long long>(window.rawDrawsExpectedFbo),
                        static_cast<unsigned long long>(window.rawDrawsOtherFbo),
                        static_cast<unsigned long long>(window.driverStateQueries),
                        static_cast<unsigned long long>(window.backendFboMismatches),
                        static_cast<unsigned long long>(window.backendFboRepairs),
                        static_cast<unsigned long long>(window.programMismatches),
                        static_cast<unsigned long long>(window.programRepairs),
                        window.lastFrontFbo, window.lastExpectedBackendFbo,
                        window.lastDriverFboBefore, window.lastDriverFboAfter,
                        window.lastFrontProgram, window.lastExpectedBackendProgram,
                        window.lastDriverProgramBefore, window.lastDriverProgramAfter,
                        static_cast<unsigned long long>(window.lastThreadHash),
                        (window.backendFboRepairs != 0 || window.programRepairs != 0) ? 1 : 0);
#ifdef MOBILEPZ_PZF15_TEXTURE_COMBINER_SUBMISSION_TRACE
                MGLOG_I("MOBILEPZ_PZF15_SESSION generation=%u classification=%s "
                        "texture_lifetime=%llu front_texture=%u front_fbo=%u attachment=%d "
                        "client_sequence=%llu open_sequence=%llu close_sequence=%llu "
                        "submission_events=%llu push_attrib=%llu pop_attrib=%llu begin=%llu "
                        "vertices=%llu end=%llu empty_end=%llu immediate_attempts=%llu "
                        "immediate_resource_failures=%llu immediate_dispatches=%llu "
                        "frontend_draw_calls=%llu client_array_dispatches=%llu color=%llu "
                        "texcoord=%llu texenv=%llu first_submission_sequence=%llu "
                        "last_submission_sequence=%llu last_event=%s last_event_front_fbo=%u "
                        "last_mode=0x%x last_count=%d last_thread_hash=%llu "
                        "rendering_output_mutation=0 additional_gl_calls=0 error_drain=0",
                        window.generation, ::MobilePZ::PZF14::SubmissionClassificationName(window),
                        static_cast<unsigned long long>(window.lifetime), window.frontTexture,
                        window.frontFbo, window.attachment,
                        static_cast<unsigned long long>(window.clientSequence),
                        static_cast<unsigned long long>(window.openSequence),
                        static_cast<unsigned long long>(window.closeSequence),
                        static_cast<unsigned long long>(window.submissionEvents),
                        static_cast<unsigned long long>(window.legacyPushAttribCalls),
                        static_cast<unsigned long long>(window.legacyPopAttribCalls),
                        static_cast<unsigned long long>(window.legacyBeginCalls),
                        static_cast<unsigned long long>(window.legacyVertexCalls),
                        static_cast<unsigned long long>(window.legacyEndCalls),
                        static_cast<unsigned long long>(window.legacyEmptyEnds),
                        static_cast<unsigned long long>(window.immediateAttempts),
                        static_cast<unsigned long long>(window.immediateResourceFailures),
                        static_cast<unsigned long long>(window.immediateDispatches),
                        static_cast<unsigned long long>(window.frontendDrawCalls),
                        static_cast<unsigned long long>(window.clientArrayDispatches),
                        static_cast<unsigned long long>(window.legacyColorCalls),
                        static_cast<unsigned long long>(window.legacyTexCoordCalls),
                        static_cast<unsigned long long>(window.legacyTexEnvCalls),
                        static_cast<unsigned long long>(window.firstSubmissionSequence),
                        static_cast<unsigned long long>(window.lastSubmissionSequence),
                        window.submissionEvents == 0 ? "NONE" :
                            ::MobilePZ::PZF14::SubmissionEventName(
                                static_cast<::MobilePZ::PZF14::SubmissionEvent>(window.lastSubmissionEvent)),
                        window.lastSubmissionFrontFbo, window.lastSubmissionMode,
                        window.lastSubmissionCount,
                        static_cast<unsigned long long>(window.lastSubmissionThreadHash));
#endif
            }
#endif
            ::MobilePZ::PZF13::RecordDetachment(texture->GetLifetimeId(), texture->GetExternalIndex(),
                                                 m_externalIndex, static_cast<Int>(type));
        }
#endif
        m_attachmentObjects[static_cast<SizeT>(type)] = FramebufferAttachmentObject(false);
        BumpAttachmentVersion(type);
    }

    const FramebufferAttachmentObject& FramebufferObject::GetAttachment(FramebufferAttachmentType type) const {
        return m_attachmentObjects[static_cast<SizeT>(type)];
    }

    const FramebufferObject::FramebufferAttachmentObjectArray& FramebufferObject::GetAllAttachmentObjects() const {
        return m_attachmentObjects;
    }

    Bool FramebufferObject::CheckCompleteness() const {
        if (m_attachmentObjects.empty()) {
            return false;
        }

        Int width = -1, height = -1;
        Int validAttachmentCount = 0;
        for (const auto& attachmentObject : m_attachmentObjects) {
            if (!attachmentObject.IsValid()) continue;

            ++validAttachmentCount;
            const auto& attachment = attachmentObject;
            auto attachmentSize = attachment.GetSize();
            Int w = attachmentSize.x();
            Int h = attachmentSize.y();

            if (width == -1) {
                width = w;
                height = h;
            } else if (width != w || height != h) {
                return false;
            }

            if (!attachment.IsComplete()) {
                return false;
            }
        }

        if (validAttachmentCount == 0) return false;
        return true;
    }

    void FramebufferObject::SetDrawBuffer(Uint index, FramebufferAttachmentType buffer) {
        if (m_drawBuffers[index] == buffer) return;
#ifdef MOBILEPZ_V1_CANDIDATE
        // The next population write will re-arm only a qualifying attachment.
        ::MobilePZ::V1::DeactivateFramebuffer(m_externalIndex);
#endif
        m_drawBuffers[index] = buffer;
        BumpAttachmentVersion(buffer);
    }

    const FramebufferObject::FramebufferAttachmentArray& FramebufferObject::GetDrawBuffers() const {
        return m_drawBuffers;
    }

    void FramebufferObject::SetReadBuffer(FramebufferAttachmentType buf) {
        if (m_readBuffer == buf) return;
        m_readBuffer = buf;
        ++m_objectVersion;
    }

    Uint FramebufferObject::GetExternalIndex() const {
        return m_externalIndex;
    }

#define MOBILEGL_DEFINE_FRAMEBUFFER_DEFAULT_SETTER(name, member, type)                                                \
    void FramebufferObject::Set##name(type value) {                                                                    \
        if (member == value) return;                                                                                    \
        member = value;                                                                                                 \
        ++m_objectVersion;                                                                                              \
    }

    MOBILEGL_DEFINE_FRAMEBUFFER_DEFAULT_SETTER(DefaultWidth, m_defaultWidth, Int)
    MOBILEGL_DEFINE_FRAMEBUFFER_DEFAULT_SETTER(DefaultHeight, m_defaultHeight, Int)
    MOBILEGL_DEFINE_FRAMEBUFFER_DEFAULT_SETTER(DefaultLayers, m_defaultLayers, Int)
    MOBILEGL_DEFINE_FRAMEBUFFER_DEFAULT_SETTER(DefaultSamples, m_defaultSamples, Int)
    MOBILEGL_DEFINE_FRAMEBUFFER_DEFAULT_SETTER(DefaultFixedSampleLocations, m_defaultFixedSampleLocations, Bool)
#undef MOBILEGL_DEFINE_FRAMEBUFFER_DEFAULT_SETTER

    void FramebufferObject::BumpAttachmentVersion(FramebufferAttachmentType type) {
        ++m_attachmentVersions[static_cast<SizeT>(type)];
        ++m_objectVersion;
    }
} // namespace MobileGL::MG_State::GLState
