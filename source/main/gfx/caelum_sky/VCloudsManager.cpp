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

        using namespace VClouds;

		mAmbientGradient = ColorGradient();
		mAmbientGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(1,1,1)*0.9f, 1.0f));
		mAmbientGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.7,0.7,0.65), 0.625f)); 
		mAmbientGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.6,0.55,0.4)*0.5, 0.5625f));
		mAmbientGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.6,0.55,0.4)*0.25, 0.475f));
		mAmbientGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.6,0.45,0.3)*0.2, 0.4f));
		mAmbientGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.2,0.2,0.3)*0.2, 0.325f));
		mAmbientGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.2,0.2,0.3)*0.15, 0));

		mSunGradient = ColorGradient();
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(1,1,1)*0.9f, 1.0f));
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(1,1,1)*0.8, 0.75f));
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.8,0.75,0.55)*1.3, 0.5625f));
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.6,0.5,0.2)*1.5, 0.5f));
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.6,0.5,0.2)*0.6, 0.4725f));
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.6,0.5,0.2)*0.4, 0.45f));
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.0,0.0,0.0), 0.4125f)); // Sun-Moon threshold
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.25,0.25,0.25), 0.25f));
		mSunGradient.addCFrame(ColorGradient::ColorFrame(Ogre::Vector3(0.4,0.4,0.4), 0.0f));

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

		float point = (sunDir.y + 1.0f) / 2.0f;

		mVClouds->setAmbientColor(mAmbientGradient.getColor(point));
		mVClouds->setSunColor(mSunGradient.getColor(point));
	}

}