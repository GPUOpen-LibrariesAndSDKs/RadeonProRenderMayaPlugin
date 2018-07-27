#pragma once
#include "FireMaya.h"

namespace FireMaya
{
    enum AOMapSideType
    {
        AOMapSideTypeFront = 0,
        AOMapSideTypeBack = 1
    };

    class FireRenderAO : public ValueNode
    {
    public:
        // initialize and type info
        static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderAONode; }
        static void* creator();
        static MStatus initialize();

        frw::Value GetValue(Scope& scope) override;

    private:
        static MObject m_radiusAttr;
        static MObject m_sideAttr;
        static MObject m_occludedColorAttr;
        static MObject m_unoccludedColorAttr;
        static MObject m_outputAttr;
    };
}

