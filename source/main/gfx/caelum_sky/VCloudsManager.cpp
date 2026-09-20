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

#include "VCloudsManager.h"
#include "CaelumSystem.h"

#include "SkyX.h"

namespace Caelum
{
	VCloudsManager::VCloudsManager(CaelumSystem* caelumSys)
		: mCaelumSys(caelumSys)
		, mVClouds(0)
		, mCreated(false)
		, mCurrentTimeSinceLastFrame(0)
	{
		mVClouds = new VClouds::VClouds(mCaelumSys->getSceneMgr());
        VClouds::VClouds::RenderQueueGroups rqGroups(
            CAELUM_RENDER_QUEUE_VCLOUDS,
            CAELUM_RENDER_QUEUE_LIGHTNING_FROM_ABOVE,
            CAELUM_RENDER_QUEUE_LIGHTNING_FROM_BELOW
        );
		mVClouds->setRenderQueueGroups(rqGroups);

        this->create();
	}

	VCloudsManager::~VCloudsManager()
	{
		remove();

		delete mVClouds;
	}

	void VCloudsManager::create()
	{
		if (mCreated)
		{
			return;
		}

		_setLightParameters();
		mVClouds->create();

		mCreated = true;
	}

	void VCloudsManager::update(const Ogre::Real& timeSinceLastFrame)
	{
		if (!mCreated)
		{
			return;
		}

		mCurrentTimeSinceLastFrame = timeSinceLastFrame;

		_setLightParameters();

		mVClouds->update(timeSinceLastFrame);
	}

	void VCloudsManager::notifyCameraRender(Ogre::Camera* c)
	{
		if (!mCreated)
		{
			return;
		}

		mVClouds->notifyCameraRender(c, mCurrentTimeSinceLastFrame);
	}

	void VCloudsManager::remove()
	{
		if (!mCreated)
		{
			return;
		}

		mVClouds->remove();

		mCreated = false;
	}

	void VCloudsManager::_setLightParameters()
	{
		Ogre::Vector3 sunDir = mCaelumSys->getSun()->getLightDirection();
        sunDir.y *= -1.0f;
		mVClouds->setSunDirection(sunDir);

		Ogre::ColourValue sunColor = mCaelumSys->getSun()->getBodyColour();
		mVClouds->setCurrentSunColor(Ogre::Vector3(sunColor.r, sunColor.g, sunColor.b));

        // Note: actual ambient light is too dim for this.
        Ogre::ColourValue ambColor = mCaelumSys->getSun()->getLightColour();
        mVClouds->setCurrentAmbientColor(Ogre::Vector3(ambColor.r, ambColor.g, ambColor.b));
	}

}
