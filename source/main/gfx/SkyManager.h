/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2017-2018 Petr Ohlidal

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

#pragma once

#include "Application.h"

#include "CaelumPrerequisites.h"

#include <Caelum.h>

namespace RoR {

/// @addtogroup Gfx
/// @{

class SkyManager
{
public:

    SkyManager();
    ~SkyManager();

    void           SetupCaelumFog(int fogStart, int fogEnd);
    void           LoadCaelumScript(const std::string& script, const std::string& rg);
    Ogre::Light*   GetSkyMainLight();
    std::string    GetPrettyTime();                 //!< prints the current time of the simulation in the format of HH:MM:SS
    double         GetTime()                    { return m_caelum_system->getJulianDay(); };
    void           SetTime(double time)         {  m_caelum_system->setJulianDay(time); };
    void           UpdateSky(float dt_sim);
    void           NotifySkyCameraChanged(Ogre::Camera* cam);
    Caelum::CaelumSystem* GetCaelumSys()        { return m_caelum_system; }

    /// Set a Caelum parameter by section and property name;<br> See file '/resources/caelum_sky/RoRSkies.os' for list of available sections and parameters.
    bool           SetCaelumParameter(const std::string& section, const std::string& name, 
                                      float arg1 = 0.f, float arg2 = 0.f, float arg3 = 0.f, float arg4 = 0.f);

    /// Get a Caelum parameter by section and property name;<br> See file '/resources/caelum_sky/RoRSkies.os' for list of available sections and parameters.
    bool           GetCaelumParameter(const std::string& section, const std::string& name,
                                      float& arg1, float& arg2, float& arg3, float& arg4);

private:
    /// Helper to get the target object and type descriptor for a given section name
    bool           GetDescriptorAndTarget(const std::string& section, 
                                          const Caelum::TypeDescriptor*& outDescriptor, 
                                          void*& outTarget);

    Caelum::LongReal      m_last_clock;
    Caelum::CaelumSystem* m_caelum_system;
};

/// @} // addtogroup Gfx

} // namespace RoR


