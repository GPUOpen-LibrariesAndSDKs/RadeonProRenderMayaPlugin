/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
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

        frw::Value GetValue(const Scope& scope) const override;

    private:
        static MObject m_radiusAttr;
        static MObject m_sideAttr;
        static MObject m_occludedColorAttr;
        static MObject m_unoccludedColorAttr;
        static MObject m_outputAttr;
		static MObject m_samplesAttr;
    };
}

