/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2023 Petr Ohlidal

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

#include "TuneupFileFormat.h"

#include "Actor.h"
#include "Application.h"
#include "CacheSystem.h"
#include "Console.h"
#include "Utils.h"

#include <OgreEntity.h>
#include <OgreMaterialManager.h>
#include <OgrePass.h>
#include <OgreSubEntity.h>
#include <OgreTechnique.h>

using namespace RoR;

TuneupDefPtr TuneupDef::clone()
{
    TuneupDefPtr ret = new TuneupDef();
    
    // General info
    ret->name               =     this->name              ; //std::string           
    ret->guid               =     this->guid              ; //std::string           
    ret->thumbnail          =     this->thumbnail         ; //std::string           
    ret->description        =     this->description       ; //std::string           
    ret->author_name        =     this->author_name       ; //std::string           
    ret->author_id          =     this->author_id         ; //int                   
    ret->category_id        =     this->category_id       ; //CacheCategoryId   

    // Modding attributes
    ret->use_addonparts       =   this->use_addonparts    ; //std::set<std::string> 
    ret->unwanted_props       =   this->unwanted_props      ; //std::set<std::string> 
    ret->unwanted_flexbodies  =   this->unwanted_flexbodies ; //std::set<std::string> 
    ret->protected_props      =   this->protected_props      ; //std::set<std::string> 
    ret->protected_flexbodies =   this->protected_flexbodies ; //std::set<std::string> 

    return ret;
}

    // Tweaking helpers

float RoR::TuneupUtil::getTweakedWheelTireRadius(CacheEntryPtr& tuneup_entry, int wheel_id, float orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->wheel_tweaks.find(wheel_id);
    auto endi = tuneup_entry->tuneup_def->wheel_tweaks.end();
    return (itor != endi && itor->second.twt_tire_radius > 0) 
        ? itor->second.twt_tire_radius : orig_val;
}

float RoR::TuneupUtil::getTweakedWheelRimRadius(CacheEntryPtr& tuneup_entry, int wheel_id, float orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->wheel_tweaks.find(wheel_id);
    auto endi = tuneup_entry->tuneup_def->wheel_tweaks.end();
    return (itor != endi && itor->second.twt_rim_radius > 0)
        ? itor->second.twt_rim_radius : orig_val;
}

std::string RoR::TuneupUtil::getTweakedWheelMedia(CacheEntryPtr& tuneup_entry, int wheel_id, int media_idx, const std::string& orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->wheel_tweaks.find(wheel_id);
    auto endi = tuneup_entry->tuneup_def->wheel_tweaks.end();
    return (itor != endi && itor->second.twt_media[media_idx] != "")
        ? itor->second.twt_media[media_idx] : orig_val;
}

std::string RoR::TuneupUtil::getTweakedWheelMediaRG(ActorPtr& actor, int wheel_id, int media_idx)
{
    // Check there's a tuneup at all
    ROR_ASSERT(actor);
    if (!actor->getUsedTuneupEntry())
        return actor->GetGfxActor()->GetResourceGroup();

    // Check there's a tweak
    TuneupDefPtr& doc = actor->getUsedTuneupEntry()->tuneup_def;
    ROR_ASSERT(doc);
    auto itor = doc->wheel_tweaks.find(wheel_id);
    auto endi = doc->wheel_tweaks.end();
    if (itor == endi || itor->second.twt_media[media_idx] == "")
        return actor->GetGfxActor()->GetResourceGroup();

    // Find the tweak addonpart
    CacheEntryPtr addonpart_entry = App::GetCacheSystem()->FindEntryByFilename(LT_AddonPart, /*partial:*/false, itor->second.twt_origin);
    if (addonpart_entry)
        return addonpart_entry->resource_group;
    else
    {
        LOG(fmt::format("[RoR|Tuneup] WARN Addonpart '{}' not found in modcache!", itor->second.twt_origin));
        return actor->GetGfxActor()->GetResourceGroup();
    }
}

WheelSide RoR::TuneupUtil::getTweakedWheelSide(CacheEntryPtr& tuneup_entry, int wheel_id, WheelSide orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);

    // First query the UI overrides
    {
        auto itor = tuneup_entry->tuneup_def->force_wheel_sides.find(wheel_id);
        auto endi = tuneup_entry->tuneup_def->force_wheel_sides.end();
        if (itor != endi)
            return itor->second;
    }
    
    // Then query tweaks
    {
        auto itor = tuneup_entry->tuneup_def->wheel_tweaks.find(wheel_id);
        auto endi = tuneup_entry->tuneup_def->wheel_tweaks.end();
        if (itor != endi && itor->second.twt_side != WheelSide::INVALID)
            return itor->second.twt_side;
    }

    return orig_val;
}

Ogre::Vector3 RoR::TuneupUtil::getTweakedNodePosition(CacheEntryPtr& tuneup_entry, NodeNum_t nodenum, Ogre::Vector3 orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->node_tweaks.find(nodenum);
    if (itor == tuneup_entry->tuneup_def->node_tweaks.end())
        return orig_val;

    Ogre::Vector3 retval = itor->second.tnt_pos;
    ROR_ASSERT(!isnan(retval.x));
    ROR_ASSERT(!isnan(retval.y));
    ROR_ASSERT(!isnan(retval.z));
    return retval;
}


// > prop
bool RoR::TuneupUtil::isPropAnyhowRemoved(ActorPtr& actor, PropID_t prop_id)
{
    return actor->getUsedTuneupEntry()
        && actor->getUsedTuneupEntry()->tuneup_def
        && (actor->getUsedTuneupEntry()->tuneup_def->isPropUnwanted(prop_id) || actor->getUsedTuneupEntry()->tuneup_def->isPropForceRemoved(prop_id));
}

Ogre::Vector3 RoR::TuneupUtil::getTweakedPropOffset(CacheEntryPtr& tuneup_entry, PropID_t prop_id, Ogre::Vector3 orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->prop_tweaks.find(prop_id);
    if (itor == tuneup_entry->tuneup_def->prop_tweaks.end())
        return orig_val;

    Ogre::Vector3 retval = itor->second.tpt_offset;
    ROR_ASSERT(!isnan(retval.x));
    ROR_ASSERT(!isnan(retval.y));
    ROR_ASSERT(!isnan(retval.z));
    return retval;
}

Ogre::Vector3 RoR::TuneupUtil::getTweakedPropRotation(CacheEntryPtr& tuneup_entry, PropID_t prop_id, Ogre::Vector3 orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->prop_tweaks.find(prop_id);
    if (itor == tuneup_entry->tuneup_def->prop_tweaks.end())
        return orig_val;

    Ogre::Vector3 retval = itor->second.tpt_rotation;
    ROR_ASSERT(!isnan(retval.x));
    ROR_ASSERT(!isnan(retval.y));
    ROR_ASSERT(!isnan(retval.z));
    return retval;
}

std::string RoR::TuneupUtil::getTweakedPropMedia(CacheEntryPtr& tuneup_entry, PropID_t prop_id, int media_idx, const std::string& orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->prop_tweaks.find(prop_id);
    auto endi = tuneup_entry->tuneup_def->prop_tweaks.end();
    return (itor != endi && itor->second.tpt_media[media_idx] != "")
        ? itor->second.tpt_media[media_idx] : orig_val;
}

std::string RoR::TuneupUtil::getTweakedPropMediaRG(ActorPtr& actor, PropID_t prop_id, int media_idx, const std::string& orig_val)
{
    // Check there's a tuneup at all
    ROR_ASSERT(actor);
    if (!actor->getUsedTuneupEntry())
        return orig_val;

    // Check there's a tweak
    TuneupDefPtr& doc = actor->getUsedTuneupEntry()->tuneup_def;
    ROR_ASSERT(doc);
    auto itor = doc->prop_tweaks.find(prop_id);
    auto endi = doc->prop_tweaks.end();
    if (itor == endi || itor->second.tpt_media[media_idx] == "")
        return orig_val;

    // Find the tweak addonpart
    CacheEntryPtr addonpart_entry = App::GetCacheSystem()->FindEntryByFilename(LT_AddonPart, /*partial:*/false, itor->second.tpt_origin);
    if (addonpart_entry)
        return addonpart_entry->resource_group;
    else
    {
        LOG(fmt::format("[RoR|Tuneup] WARN Addonpart '{}' not found in modcache!", itor->second.tpt_origin));
        return orig_val;
    }
}


// > flexbody
bool RoR::TuneupUtil::isFlexbodyAnyhowRemoved(ActorPtr& actor, FlexbodyID_t flexbody_id)
{
    return actor->getUsedTuneupEntry()
        && actor->getUsedTuneupEntry()->tuneup_def
        && (actor->getUsedTuneupEntry()->tuneup_def->isFlexbodyUnwanted(flexbody_id) || actor->getUsedTuneupEntry()->tuneup_def->isFlexbodyForceRemoved(flexbody_id));
}

Ogre::Vector3 RoR::TuneupUtil::getTweakedFlexbodyOffset(CacheEntryPtr& tuneup_entry, FlexbodyID_t flexbody_id, Ogre::Vector3 orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->flexbody_tweaks.find(flexbody_id);
    if (itor == tuneup_entry->tuneup_def->flexbody_tweaks.end())
        return orig_val;

    Ogre::Vector3 retval = itor->second.tft_offset;
    ROR_ASSERT(!isnan(retval.x));
    ROR_ASSERT(!isnan(retval.y));
    ROR_ASSERT(!isnan(retval.z));
    return retval;
}

Ogre::Vector3 RoR::TuneupUtil::getTweakedFlexbodyRotation(CacheEntryPtr& tuneup_entry, FlexbodyID_t flexbody_id, Ogre::Vector3 orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->flexbody_tweaks.find(flexbody_id);
    if (itor == tuneup_entry->tuneup_def->flexbody_tweaks.end())
        return orig_val;

    Ogre::Vector3 retval = itor->second.tft_rotation;
    ROR_ASSERT(!isnan(retval.x));
    ROR_ASSERT(!isnan(retval.y));
    ROR_ASSERT(!isnan(retval.z));
    return retval;
}

std::string RoR::TuneupUtil::getTweakedFlexbodyMedia(CacheEntryPtr& tuneup_entry, FlexbodyID_t flexbody_id, int media_idx, const std::string& orig_val)
{
    if (!tuneup_entry)
        return orig_val;

    ROR_ASSERT(tuneup_entry->tuneup_def);
    auto itor = tuneup_entry->tuneup_def->flexbody_tweaks.find(flexbody_id);
    auto endi = tuneup_entry->tuneup_def->flexbody_tweaks.end();
    return (itor != endi && itor->second.tft_media != "")
        ? itor->second.tft_media : orig_val;
}

std::string RoR::TuneupUtil::getTweakedFlexbodyMediaRG(ActorPtr& actor, FlexbodyID_t flexbody_id, int media_idx, const std::string& orig_val)
{
    // Check there's a tuneup at all
    ROR_ASSERT(actor);
    if (!actor->getUsedTuneupEntry())
        return orig_val;

    // Check there's a tweak
    TuneupDefPtr& doc = actor->getUsedTuneupEntry()->tuneup_def;
    ROR_ASSERT(doc);
    auto itor = doc->flexbody_tweaks.find(flexbody_id);
    auto endi = doc->flexbody_tweaks.end();
    if (itor == endi || itor->second.tft_media == "")
        return orig_val;

    // Find the tweak addonpart
    CacheEntryPtr addonpart_entry = App::GetCacheSystem()->FindEntryByFilename(LT_AddonPart, /*partial:*/false, itor->second.tft_origin);
    if (addonpart_entry)
        return addonpart_entry->resource_group;
    else
    {
        LOG(fmt::format("[RoR|Tuneup] WARN Addonpart '{}' not found in modcache!", itor->second.tft_origin));
        return orig_val;
    }
}


std::vector<TuneupDefPtr> RoR::TuneupUtil::ParseTuneups(Ogre::DataStreamPtr& stream)
{
    std::vector<TuneupDefPtr> result;
    TuneupDefPtr curr_tuneup;
    try
    {
        while(!stream->eof())
        {
            std::string line = SanitizeUtf8String(stream->getLine());

            // Ignore blanks & comments
            if (!line.length() || line.substr(0, 2) == "//")
            {
                continue;
            }

            if (!curr_tuneup)
            {
                // No current tuneup -- So first valid data should be tuneup name
                Ogre::StringUtil::trim(line);
                curr_tuneup = new TuneupDef();
                curr_tuneup->name = line;
                stream->skipLine("{");
            }
            else
            {
                // Already in tuneup
                if (line == "}")
                {
                    result.push_back(curr_tuneup); // Finished
                    curr_tuneup = nullptr;
                }
                else
                {
                    RoR::TuneupUtil::ParseTuneupAttribute(line, curr_tuneup);
                }
            }
        }

        if (curr_tuneup)
        {
            App::GetConsole()->putMessage(
                Console::CONSOLE_MSGTYPE_ACTOR, Console::CONSOLE_SYSTEM_WARNING,
                fmt::format("Tuneup '{}' in file '{}' not properly closed with '}}'",
                    curr_tuneup->name, stream->getName()));
            result.push_back(curr_tuneup); // Submit anyway
            curr_tuneup = nullptr;
        }
    }
    catch (Ogre::Exception& e)
    {
        App::GetConsole()->putMessage(
            Console::CONSOLE_MSGTYPE_ACTOR, Console::CONSOLE_SYSTEM_WARNING,
            fmt::format("Error parsing tuneup file '{}', message: {}",
                stream->getName(), e.getFullDescription()));
    }
    return result;
}

void RoR::TuneupUtil::ParseTuneupAttribute(const std::string& line, TuneupDefPtr& tuneup_def) // static
{
    Ogre::StringVector params = Ogre::StringUtil::split(line, "\t=,;\n");
    for (unsigned int i=0; i < params.size(); i++)
    {
        Ogre::StringUtil::trim(params[i]);
    }
    Ogre::String& attrib = params[0];
    Ogre::StringUtil::toLowerCase(attrib);

    // General info
    if (attrib == "preview"         && params.size() >= 2) { tuneup_def->thumbnail = params[1]; return; }
    if (attrib == "description"     && params.size() >= 2) { tuneup_def->description = params[1]; return; }
    if (attrib == "author_name"     && params.size() >= 2) { tuneup_def->author_name = params[1]; return; }
    if (attrib == "author_id"       && params.size() == 2) { tuneup_def->author_id = PARSEINT(params[1]); return; }
    if (attrib == "category_id"     && params.size() == 2) { tuneup_def->category_id = (CacheCategoryId)PARSEINT(params[1]); return; }
    if (attrib == "guid"            && params.size() >= 2) { tuneup_def->guid = params[1]; Ogre::StringUtil::trim(tuneup_def->guid); Ogre::StringUtil::toLowerCase(tuneup_def->guid); return; }
    if (attrib == "name"            && params.size() >= 2) { tuneup_def->name = params[1]; Ogre::StringUtil::trim(tuneup_def->name); return; }

    // Addonparts and extracted data
    if (attrib == "use_addonpart"   && params.size() == 2) { tuneup_def->use_addonparts.insert(params[1]); return; }
    if (attrib == "unwanted_prop"     && params.size() == 2) { tuneup_def->unwanted_props.insert(PARSEINT(params[1])); return; }
    if (attrib == "unwanted_flexbody" && params.size() == 2) { tuneup_def->unwanted_flexbodies.insert(PARSEINT(params[1])); return; }
    if (attrib == "protected_prop"     && params.size() == 2) { tuneup_def->protected_props.insert(PARSEINT(params[1])); return; }
    if (attrib == "protected_flexbody" && params.size() == 2) { tuneup_def->protected_flexbodies.insert(PARSEINT(params[1])); return; }

    // UI overrides
    if (attrib == "forced_wheel_side" && params.size() == 3) { tuneup_def->force_wheel_sides[PARSEINT(params[1])] = (WheelSide)PARSEINT(params[2]); return; }
    if (attrib == "force_remove_prop" && params.size() == 2) { tuneup_def->force_remove_props.insert(PARSEINT(params[1])); return; }
    if (attrib == "force_remove_flexbody" && params.size() == 2) { tuneup_def->force_remove_flexbodies.insert(PARSEINT(params[1])); return; }
}

void RoR::TuneupUtil::ExportTuneup(Ogre::DataStreamPtr& stream, TuneupDefPtr& tuneup)
{
    Str<2000> buf;
    buf << tuneup->name << "\n";
    buf << "{\n";

    // General info:
    buf << "\tpreview = "     << tuneup->thumbnail    << "\n";
    buf << "\tdescription = " << tuneup->description  << "\n";
    buf << "\tauthor_name = " << tuneup->author_name  << "\n";
    buf << "\tauthor_id = "   << tuneup->author_id    << "\n";
    buf << "\tcategory_id = " << (int)tuneup->category_id  << "\n";
    buf << "\tguid = "        << tuneup->guid         << "\n";
    buf << "\n";

    // Addonparts and extracted data:
    for (const std::string& addonpart: tuneup->use_addonparts)
    {
        buf << "\tuse_addonpart = " << addonpart << "\n";
    }
    for (PropID_t unwanted_prop: tuneup->unwanted_props)
    {
        buf << "\tunwanted_prop = " << (int)unwanted_prop << "\n";
    }
    for (FlexbodyID_t unwanted_flexbody: tuneup->unwanted_flexbodies)
    {
        buf << "\tunwanted_flexbody = " << (int)unwanted_flexbody << "\n";
    }
    for (PropID_t protected_prop: tuneup->protected_props)
    {
        buf << "\tprotected_prop = " << (int)protected_prop << "\n";
    }
    for (FlexbodyID_t protected_flexbody: tuneup->protected_flexbodies)
    {
        buf << "\tprotected_flexbody = " << (int)protected_flexbody << "\n";
    }

    // UI overrides:
    for (PropID_t prop: tuneup->force_remove_props)
    {
        buf << "\tforce_remove_prop = " << (int)prop << "\n";
    }
    for (FlexbodyID_t flexbody: tuneup->force_remove_flexbodies)
    {
        buf << "\tforce_remove_flexbody = " << (int)flexbody << "\n";
    }
    for (auto& pair: tuneup->force_wheel_sides)
    {
        buf << "\tforced_wheel_side = " << pair.first << ", " << (int)pair.second << "\n";
    }
    buf << "}\n\n";

    size_t written = stream->write(buf.GetBuffer(), buf.GetLength());
    if (written < buf.GetLength())
    {
        App::GetConsole()->putMessage(Console::CONSOLE_MSGTYPE_INFO, Console::CONSOLE_SYSTEM_WARNING,
            fmt::format("Error writing file '{}': only written {}/{} bytes", stream->getName(), written, buf.GetLength()));
    }
}

