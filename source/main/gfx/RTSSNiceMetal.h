/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

/// @file
/// @brief RTSS sub render state reproducing the legacy 'nicemetal.cg' vehicle shading.

#pragma once

#include <OgreShaderSubRenderState.h>

namespace RoR {

/// @addtogroup Gfx
/// @{

/// Vehicle surface shading: vertex-colour driven damage/wetness blending plus a
/// specular-map-masked cubemap reflection.
///
/// The specular map is a reflectivity mask, not a gloss map: it selects between
/// the lit diffuse colour and a mirror highlight. Vertex colour carries per-node
/// simulation flags rather than a colour (alpha = damaged, blue = wet), so it is
/// deliberately kept out of OGRE's vertex colour tracking and sampled directly.
class NiceMetalSubRenderState : public Ogre::RTShader::SubRenderState
{
public:

    static const Ogre::String Type;

    const Ogre::String& getType() const override { return Type; }
    int getExecutionOrder() const override;
    void copyFrom(const SubRenderState& rhs) override;
    bool setParameter(const Ogre::String& name, const Ogre::String& value) override;
    bool preAddToRenderState(const Ogre::RTShader::RenderState* renderState, Ogre::Pass* srcPass, Ogre::Pass* dstPass) override;
    bool createCpuSubPrograms(Ogre::RTShader::ProgramSet* programSet) override;

private:

    Ogre::String m_env_map_name = "EnvironmentTexture";
    int m_diffuse_sampler = -1;
    int m_damage_sampler = -1;
    int m_specular_sampler = -1;
    int m_env_sampler = -1;
    bool m_transparent = false;
    /// Whether the mesh actually carries the per-node flags. Only flexbodies get a
    /// VES_DIFFUSE buffer; ordinary meshes have none, and reading a COLOR0 input that the
    /// vertex declaration lacks is a hard error on some render systems.
    bool m_is_flexbody = true;
    /// Produce only the blended surface colour, leaving lighting to another sub render
    /// state - used to feed the damage blend into the physically based pipeline.
    bool m_surface_only = false;
};

class NiceMetalSubRenderStateFactory : public Ogre::RTShader::SubRenderStateFactory
{
public:

    const Ogre::String& getType() const override { return NiceMetalSubRenderState::Type; }

protected:

    Ogre::RTShader::SubRenderState* createInstanceImpl() override;
};

/// @} // addtogroup Gfx

} // namespace RoR
