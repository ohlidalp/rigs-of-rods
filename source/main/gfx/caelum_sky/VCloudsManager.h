/*
--------------------------------------------------------------------------------
This source file is part of SkyX.
Visit http://www.paradise-studios.net/products/skyx/

Copyright (C) 2009-2012 Xavier Verguín González <xavyiy@gmail.com>

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
--------------------------------------------------------------------------------
*/

#ifndef _SkyX_VCloudsManager_H_
#define _SkyX_VCloudsManager_H_

#include "CaelumPrerequisites.h"

#include "VClouds/VClouds.h"
#include "VClouds/VColorGradient.h"

namespace Caelum
{
    class VCloudsManager 
	{
	public:
	    /** Constructor
		    @param s Parent SkyX pointer
		 */
		VCloudsManager(CaelumSystem* caelumSys);

		/** Destructor 
		 */
		~VCloudsManager();

		/** Create all resources
		 */
		void create();

		/** Update
		    @param timeSinceLastFrame Time since last frame
		 */
		void update(const Ogre::Real& timeSinceLastFrame);

		/** Notify camera render, to be invoked per-camera and per-frame
			@param c Rendering camera
			@remarks The VClouds system needs the CAMERA time since last frame, so here we assume that all render targets
			         are being updated one time per frame(in other words, all render targets are being updated at the same rate)
         */
        void notifyCameraRender(Ogre::Camera* c);

		/** Remove all resources
		 */
		void remove();

		void setWindSpeed(float WindSpeed)
		{
            mVClouds->setWindSpeed(WindSpeed);
		}

		float getWindSpeed() const
		{
			return mVClouds->getWindSpeed();
		}

		/** Set wind direction
		    @param WindDirection Wind direction
		 */
		inline void setWindDirection(Ogre::Radian WindDirection)
		{
            mVClouds->setWindDirection(WindDirection);
		}

		/** Get wind direction
		    @return Wind direction
		 */
		inline Ogre::Radian getWindDirection() const
		{
			return mVClouds->getWindDirection();
		}

        /// @name Config parser proxies - vclouds
        /// @{
        void setAmbientColor(const Ogre::Vector3& ambientColor)
        {
            mVClouds->setAmbientColor(ambientColor);
        }
        const Ogre::Vector3 getAmbientColor() const
        {
            return mVClouds->getAmbientColor();
        }
        void setLightResponse(const Ogre::Vector4& lightResponse)
        {
            mVClouds->setLightResponse(lightResponse);
        }
        const Ogre::Vector4 getLightResponse() const
        {
            return mVClouds->getLightResponse();
        }
        void setAmbientFactors(const Ogre::Vector4& ambientFactors)
        {
            mVClouds->setAmbientFactors(ambientFactors);
        }
        const Ogre::Vector4 getAmbientFactors() const
        {
            return mVClouds->getAmbientFactors();
        }
        void setCloudiness(const Ogre::Vector2& wheater)
        {
            mVClouds->setWheater(wheater.x, wheater.y, /*delayedResponse=*/false);
        }
        const Ogre::Vector2 getCloudiness() const
        {
            return mVClouds->getWheater();
        }
        
		/** 'vertical_bounds' in the config.
		    @param Height x = Cloud field y-coord start, y = Field height (both in world coordinates)
			@remarks Calling this does not update existing geometry.
		 */
		inline void setHeight(const Ogre::Vector2& Height)
		{
			mVClouds->setHeight(Height);
		}

		/** 'vertical_bounds' in the config.
		    @return Height: x = Cloud field y-coord start, y = Field height (both in world coordinates)
		 */
		inline const Ogre::Vector2 getHeight() const
		{
			return mVClouds->getHeight();
		}
        /// @}

        /// @name Config parser proxies - lightnings
        void setEnableLightnings(bool val)
        {
            mVClouds->getLightningManager()->setEnabled(val);
        }
        bool getEnableLightnings() const
        {
            return mVClouds->getLightningManager()->isEnabled();
        }
        void setAverageLightningAppartitionTime(float val)
        {
            mVClouds->getLightningManager()->setAverageLightningApparitionTime(val);
        }
        float getAverageLightningAppartitionTime() const
        {
            return mVClouds->getLightningManager()->getAverageLightningApparitionTime();
        }
        void setLightningTimeMultiplier(float val)
        {
            mVClouds->getLightningManager()->setLightningTimeMultiplier(val);
        }
        float getLightningTimeMultiplier() const
        {
            return mVClouds->getLightningManager()->getLightningTimeMultiplier();
        }
        void setLightningColor(const Ogre::Vector3& val)
        {
            mVClouds->getLightningManager()->setLightningColor(val);
        }
        const Ogre::Vector3 getLightningColor() const
        {
            return mVClouds->getLightningManager()->getLightningColor();
        }
        void setRadius(float val)
        {
            mVClouds->setRadius(val);
        }
        float getRadius() const
        {
            return mVClouds->getRadius();
        }
        /// @}

		/** Get VClouds
		 */
		inline VClouds::VClouds* getVClouds()
		{
			return mVClouds;
		}

		/** Is moon manager created?
		    @return true if yes, false if not
		 */
		inline const bool& isCreated() const
		{
			return mCreated;
		}

	private:
		/** Set light parameters
		 */
		void _setLightParameters();
        
		/// Ambient and Sun color gradients
		VClouds::ColorGradient mAmbientGradient;
		VClouds::ColorGradient mSunGradient;

		/// VClouds pointer
		VClouds::VClouds* mVClouds;

		/// Is vclouds manager created?
		bool mCreated;

		/// Current time since last frame
		Ogre::Real mCurrentTimeSinceLastFrame;
        
        CaelumSystem* mCaelumSys = nullptr;

	};
}

#endif