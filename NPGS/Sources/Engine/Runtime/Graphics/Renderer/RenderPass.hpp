#pragma once

namespace Npgs
{
    class IRenderPass
    {
    public:
        IRenderPass()                   = default;
        IRenderPass(const IRenderPass&) = delete;
        IRenderPass(IRenderPass&&)      = delete;
        virtual ~IRenderPass()          = default;

        IRenderPass& operator=(const IRenderPass&) = delete;
        IRenderPass& operator=(IRenderPass&&)      = delete;

        void Setup();

    protected:
        virtual void LoadShaders()        = 0;
        virtual void SetupPipeline()      = 0;
        virtual void BindDescriptors()    = 0;
        virtual void DeclareAttachments() = 0;
    };
} // namespace Npgs
