/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2019 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

/// @file   CacheSystem.h
/// @author Thomas Fischer, 21th of May 2008
/// @author Petr Ohlidal, 2018

#include "CacheSystem.h"

#include "ActorSpawner.h"
#include "Application.h"
#include "SimData.h"
#include "ContentManager.h"
#include "ErrorUtils.h"
#include "GUI_LoadingWindow.h"
#include "GUI_GameMainMenu.h"
#include "GUIManager.h"
#include "GfxActor.h"
#include "GfxScene.h"
#include "Language.h"
#include "PlatformUtils.h"
#include "RigDef_Parser.h"
#include "SkinFileFormat.h"
#include "Terrain.h"
#include "Terrn2FileFormat.h"
#include "Utils.h"

#include <OgreException.h>
#include <OgreFileSystem.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <fstream>

using namespace Ogre;
using namespace RoR;

CacheSystem::CacheSystem()
{
    // register the extensions
    m_known_extensions.push_back("machine");
    m_known_extensions.push_back("fixed");
    m_known_extensions.push_back("terrn2");
    m_known_extensions.push_back("truck");
    m_known_extensions.push_back("car");
    m_known_extensions.push_back("boat");
    m_known_extensions.push_back("airplane");
    m_known_extensions.push_back("trailer");
    m_known_extensions.push_back("load");
    m_known_extensions.push_back("train");
    m_known_extensions.push_back("skin");
}

void CacheSystem::LoadModCache(CacheValidity validity)
{
    m_resource_paths.clear();
    m_update_time = getTimeStamp();

    if (validity != CacheValidity::VALID)
    {
        if (validity == CacheValidity::NEEDS_REBUILD)
        {
            RoR::Log("[RoR|ModCache] Performing rebuild ...");
            this->ClearCache();
        }
        else
        {
            RoR::Log("[RoR|ModCache] Performing update ...");
            this->ClearResourceGroups();
            this->PruneCache();
        }
        const bool orig_echo = App::diag_log_console_echo->getBool();
        App::diag_log_console_echo->setVal(false);
        this->ParseZipArchives(RGN_CONTENT);
        this->ParseKnownFiles(RGN_CONTENT);
        App::diag_log_console_echo->setVal(orig_echo);
        this->DetectDuplicates();
        this->WriteCacheFileJson();

        this->LoadCacheFileJson();
    }

    RoR::Log("[RoR|ModCache] Cache loaded");
}

CacheEntry* CacheSystem::FindEntryByFilename(LoaderType type, bool partial, std::string filename)
{
    StringUtil::toLowerCase(filename);
    size_t partial_match_length = std::numeric_limits<size_t>::max();
    CacheEntry* partial_match = nullptr;
    for (CacheEntry& entry : m_entries)
    {
        if ((type == LT_Terrain) != (entry.fext == "terrn2") ||
            (type == LT_AllBeam && entry.fext == "skin"))
            continue;

        String fname = entry.fname;
        String fname_without_uid = entry.fname_without_uid;
        StringUtil::toLowerCase(fname);
        StringUtil::toLowerCase(fname_without_uid);
        if (fname == filename || fname_without_uid == filename)
            return &entry;

        if (partial &&
            fname.length() < partial_match_length &&
            fname.find(filename) != std::string::npos)
        {
            partial_match = &entry;
            partial_match_length = fname.length();
        }
    }

    return (partial) ? partial_match : nullptr;
}

CacheValidity CacheSystem::EvaluateCacheValidity()
{
    this->GenerateHashFromFilenames();

    // Load cache file
    CacheValidity validity = this->LoadCacheFileJson();

    if (validity != CacheValidity::VALID)
    {
        RoR::Log("[RoR|ModCache] Cannot load cache file: wrong version, corrupted or missing.");
        return validity;
    }

    // Compare stored hash with generated hash
    if (m_filenames_hash_loaded != m_filenames_hash_generated)
    {
        RoR::Log("[RoR|ModCache] Cache file out of date");
        return CacheValidity::NEEDS_UPDATE;
    }

    for (auto& entry : m_entries)
    {
        std::string fn = entry.resource_bundle_path;
        if (entry.resource_bundle_type == "FileSystem")
        {
            fn = PathCombine(fn, entry.fname);
        }

        if ((entry.filetime != RoR::GetFileLastModifiedTime(fn)))
        {
            return CacheValidity::NEEDS_UPDATE;
        }
    }

    RoR::Log("[RoR|ModCache] Cache valid");
    return CacheValidity::VALID;
}

void CacheSystem::ImportEntryFromJson(rapidjson::Value& j_entry, CacheEntry & out_entry)
{
    // Common details
    out_entry.usagecounter =           j_entry["usagecounter"].GetInt();
    out_entry.addtimestamp =           j_entry["addtimestamp"].GetInt();
    out_entry.resource_bundle_type =   j_entry["resource_bundle_type"].GetString();
    out_entry.resource_bundle_path =   j_entry["resource_bundle_path"].GetString();
    out_entry.fpath =                  j_entry["fpath"].GetString();
    out_entry.fname =                  j_entry["fname"].GetString();
    out_entry.fname_without_uid =      j_entry["fname_without_uid"].GetString();
    out_entry.fext =                   j_entry["fext"].GetString();
    out_entry.filetime =               j_entry["filetime"].GetInt();
    out_entry.dname =                  j_entry["dname"].GetString();
    out_entry.uniqueid =               j_entry["uniqueid"].GetString();
    out_entry.version =                j_entry["version"].GetInt();
    out_entry.filecachename =          j_entry["filecachename"].GetString();
    out_entry.default_skin =           j_entry["default_skin"].GetString();

    out_entry.guid = j_entry["guid"].GetString();
    Ogre::StringUtil::trim(out_entry.guid);

    // Category
    int category_id = j_entry["categoryid"].GetInt();
    auto category_itor = m_categories.find(category_id);
    if (category_itor == m_categories.end() || category_id >= CID_Max)
    {
        category_itor = m_categories.find(CID_Unsorted);
    }
    out_entry.categoryname = category_itor->second;
    out_entry.categoryid = category_itor->first;

     // Common - Authors
    for (rapidjson::Value& j_author: j_entry["authors"].GetArray())
    {
        AuthorInfo author;

        author.type  =  j_author["type"].GetString();
        author.name  =  j_author["name"].GetString();
        author.email =  j_author["email"].GetString();
        author.id    =  j_author["id"].GetInt();

        out_entry.authors.push_back(author);
    }

    // Vehicle configurations
    for (rapidjson::Value& j_config : j_entry["sectionconfigs"].GetArray())
    {
        CacheActorConfigInfo out_config;

        // Attributes
        out_config.config_name       = j_config["config_name"].GetString();
        out_config.truckmass         = j_config["truckmass"].GetFloat();
        out_config.loadmass          = j_config["loadmass"].GetFloat();
        out_config.customtach        = j_config["customtach"].GetBool();
        out_config.custom_particles  = j_config["custom_particles"].GetBool();
        out_config.forwardcommands   = j_config["forwardcommands"].GetBool();
        out_config.importcommands    = j_config["importcommands"].GetBool();
        out_config.rescuer           = j_config["rescuer"].GetBool();

        // Element counts
        out_config.nodecount         = j_config["nodecount"].GetInt();
        out_config.beamcount         = j_config["beamcount"].GetInt();
        out_config.shockcount        = j_config["shockcount"].GetInt();
        out_config.fixescount        = j_config["fixescount"].GetInt();
        out_config.hydroscount       = j_config["hydroscount"].GetInt();
        out_config.tiecount          = j_config["tiecount"].GetInt();
        out_config.ropecount         = j_config["ropecount"].GetInt();
        out_config.wheelcount        = j_config["wheelcount"].GetInt();
        out_config.propwheelcount    = j_config["propwheelcount"].GetInt();
        out_config.commandscount     = j_config["commandscount"].GetInt();
        out_config.flarescount       = j_config["flarescount"].GetInt();
        out_config.propscount        = j_config["propscount"].GetInt();
        out_config.wingscount        = j_config["wingscount"].GetInt();
        out_config.turbopropscount   = j_config["turbopropscount"].GetInt();
        out_config.turbojetcount     = j_config["turbojetcount"].GetInt();
        out_config.rotatorscount     = j_config["rotatorscount"].GetInt();
        out_config.exhaustscount     = j_config["exhaustscount"].GetInt();
        out_config.flexbodiescount   = j_config["flexbodiescount"].GetInt();
        out_config.soundsourcescount = j_config["soundsourcescount"].GetInt();
        out_config.airbrakescount    = j_config["airbrakescount"].GetInt();
        out_config.rotatorscount     = j_config["rotatorscount"].GetInt();
        out_config.submeshescount    = j_config["submeshescount"].GetInt();
        
        // Engine
        out_config.numgears          = j_config["numgears"].GetInt();
        out_config.enginetype        = static_cast<char>(j_config["enginetype"].GetInt());
        out_config.minrpm            = j_config["minrpm"].GetFloat();
        out_config.maxrpm            = j_config["maxrpm"].GetFloat();
        out_config.torque            = j_config["torque"].GetFloat();

        out_entry.sectionconfigs.push_back(out_config);
    }
}

CacheValidity CacheSystem::LoadCacheFileJson()
{
    // Clear existing entries
    m_entries.clear();

    rapidjson::Document j_doc;
    if (!App::GetContentManager()->LoadAndParseJson(CACHE_FILE, RGN_CACHE, j_doc) ||
        !j_doc.IsObject() || !j_doc.HasMember("entries") || !j_doc["entries"].IsArray())
    {
        RoR::Log("[RoR|ModCache] Error, cache file still invalid after check/update, content selector will be empty.");
        return CacheValidity::NEEDS_REBUILD;
    }

    if (j_doc["format_version"].GetInt() != CACHE_FILE_FORMAT)
    {
        RoR::Log("[RoR|ModCache] Invalid cache file format");
        return CacheValidity::NEEDS_REBUILD;
    }

    for (rapidjson::Value& j_entry: j_doc["entries"].GetArray())
    {
        CacheEntry entry;
        this->ImportEntryFromJson(j_entry, entry);
        entry.number = static_cast<int>(m_entries.size() + 1); // Let's number mods from 1
        m_entries.push_back(entry);
    }

    m_filenames_hash_loaded = j_doc["global_hash"].GetString();

    return CacheValidity::VALID;
}

void CacheSystem::PruneCache()
{
    this->LoadCacheFileJson();

    std::vector<String> paths;
    for (auto& entry : m_entries)
    {
        std::string fn = entry.resource_bundle_path;
        if (entry.resource_bundle_type == "FileSystem")
        {
            fn = PathCombine(fn, entry.fname);
        }

        if (!RoR::FileExists(fn.c_str()) || (entry.filetime != RoR::GetFileLastModifiedTime(fn)))
        {
            if (!entry.deleted)
            {
                if (std::find(paths.begin(), paths.end(), fn) == paths.end())
                {
                    RoR::LogFormat("[RoR|ModCache] Removing '%s'", fn.c_str());
                    paths.push_back(fn);
                }
                this->RemoveFileCache(entry);
            }
            entry.deleted = true;
        }
        else
        {
            m_resource_paths.insert(fn);
        }
    }
}

void CacheSystem::ClearResourceGroups()
{
    for (auto& entry : m_entries)
    {
        String group = entry.resource_group;
        if (!group.empty())
        {
            if (ResourceGroupManager::getSingleton().resourceGroupExists(group))
                ResourceGroupManager::getSingleton().destroyResourceGroup(group);
        }
    }
}

void CacheSystem::DetectDuplicates()
{
    RoR::Log("[RoR|ModCache] Searching for duplicates ...");
    std::map<String, String> possible_duplicates;
    for (int i=0; i<m_entries.size(); i++) 
    {
        if (m_entries[i].deleted)
            continue;

        String dnameA = m_entries[i].dname;
        StringUtil::toLowerCase(dnameA);
        StringUtil::trim(dnameA);
        String dirA = m_entries[i].resource_bundle_path;
        StringUtil::toLowerCase(dirA);
        String basenameA, basepathA;
        StringUtil::splitFilename(dirA, basenameA, basepathA);
        String filenameWUIDA = m_entries[i].fname_without_uid;
        StringUtil::toLowerCase(filenameWUIDA);

        for (int j=i+1; j<m_entries.size(); j++) 
        {
            if (m_entries[j].deleted)
                continue;

            String filenameWUIDB = m_entries[j].fname_without_uid;
            StringUtil::toLowerCase(filenameWUIDB);
            if (filenameWUIDA != filenameWUIDB)
                continue;

            String dnameB = m_entries[j].dname;
            StringUtil::toLowerCase(dnameB);
            StringUtil::trim(dnameB);
            if (dnameA != dnameB)
                continue;

            String dirB = m_entries[j].resource_bundle_path;
            StringUtil::toLowerCase(dirB);
            String basenameB, basepathB;
            StringUtil::splitFilename(dirB, basenameB, basepathB);
            basenameA = Ogre::StringUtil::replaceAll(basenameA, " ", "_");
            basenameA = Ogre::StringUtil::replaceAll(basenameA, "-", "_");
            basenameB = Ogre::StringUtil::replaceAll(basenameB, " ", "_");
            basenameB = Ogre::StringUtil::replaceAll(basenameB, "-", "_");
            if (StripSHA1fromString(basenameA) != StripSHA1fromString(basenameB))
                continue;

            if (m_entries[i].resource_bundle_path == m_entries[j].resource_bundle_path)
            {
                LOG("- duplicate: " + m_entries[i].fpath + m_entries[i].fname
                             + " <--> " + m_entries[j].fpath + m_entries[j].fname);
                LOG("  - " + m_entries[j].resource_bundle_path);
                int idx = m_entries[i].fpath.size() < m_entries[j].fpath.size() ? i : j;
                m_entries[idx].deleted = true;
            }
            else
            {
                possible_duplicates[m_entries[i].resource_bundle_path] = m_entries[j].resource_bundle_path;
            }
        }
    }
    for (auto duplicate : possible_duplicates)
    {
        LOG("- possible duplicate: ");
        LOG("  - " + duplicate.first);
        LOG("  - " + duplicate.second);
    }
}

CacheEntry* CacheSystem::GetEntry(int modid)
{
    for (std::vector<CacheEntry>::iterator it = m_entries.begin(); it != m_entries.end(); it++)
    {
        if (modid == it->number)
            return &(*it);
    }
    return 0;
}

String CacheSystem::GetPrettyName(String fname)
{
    for (std::vector<CacheEntry>::iterator it = m_entries.begin(); it != m_entries.end(); it++)
    {
        if (fname == it->fname)
            return it->dname;
    }
    return "";
}

std::string CacheSystem::ActorTypeToName(ActorType driveable)
{
    switch (driveable)
    {
    case ActorType::NOT_DRIVEABLE: return _LC("MainSelector", "Non-Driveable");
    case ActorType::TRUCK:         return _LC("MainSelector", "Truck");
    case ActorType::AIRPLANE:      return _LC("MainSelector", "Airplane");
    case ActorType::BOAT:          return _LC("MainSelector", "Boat");
    case ActorType::MACHINE:       return _LC("MainSelector", "Machine");
    case ActorType::AI:            return _LC("MainSelector", "A.I.");
    default:                       return "";
    };
}

void CacheSystem::ExportEntryToJson(rapidjson::Value& j_entries, rapidjson::Document& j_doc, CacheEntry const & entry)
{
    rapidjson::Value j_entry(rapidjson::kObjectType);

    // Common details
    j_entry.AddMember("usagecounter",         entry.usagecounter,                                          j_doc.GetAllocator());
    j_entry.AddMember("addtimestamp",         static_cast<int64_t>(entry.addtimestamp),                    j_doc.GetAllocator());
    j_entry.AddMember("resource_bundle_type", rapidjson::StringRef(entry.resource_bundle_type.c_str()),    j_doc.GetAllocator());
    j_entry.AddMember("resource_bundle_path", rapidjson::StringRef(entry.resource_bundle_path.c_str()),    j_doc.GetAllocator());
    j_entry.AddMember("fpath",                rapidjson::StringRef(entry.fpath.c_str()),                   j_doc.GetAllocator());
    j_entry.AddMember("fname",                rapidjson::StringRef(entry.fname.c_str()),                   j_doc.GetAllocator());
    j_entry.AddMember("fname_without_uid",    rapidjson::StringRef(entry.fname_without_uid.c_str()),       j_doc.GetAllocator());
    j_entry.AddMember("fext",                 rapidjson::StringRef(entry.fext.c_str()),                    j_doc.GetAllocator());
    j_entry.AddMember("filetime",             static_cast<int64_t>(entry.filetime),                        j_doc.GetAllocator()); 
    j_entry.AddMember("dname",                rapidjson::StringRef(entry.dname.c_str()),                   j_doc.GetAllocator());
    j_entry.AddMember("categoryid",           entry.categoryid,                                            j_doc.GetAllocator());
    j_entry.AddMember("uniqueid",             rapidjson::StringRef(entry.uniqueid.c_str()),                j_doc.GetAllocator());
    j_entry.AddMember("guid",                 rapidjson::StringRef(entry.guid.c_str()),                    j_doc.GetAllocator());
    j_entry.AddMember("version",              entry.version,                                               j_doc.GetAllocator());
    j_entry.AddMember("filecachename",        rapidjson::StringRef(entry.filecachename.c_str()),           j_doc.GetAllocator());

    // Common - Authors
    rapidjson::Value j_authors(rapidjson::kArrayType);
    for (AuthorInfo const& author: entry.authors)
    {
        rapidjson::Value j_author(rapidjson::kObjectType);

        j_author.AddMember("type",   rapidjson::StringRef(author.type.c_str()),   j_doc.GetAllocator());
        j_author.AddMember("name",   rapidjson::StringRef(author.name.c_str()),   j_doc.GetAllocator());
        j_author.AddMember("email",  rapidjson::StringRef(author.email.c_str()),  j_doc.GetAllocator());
        j_author.AddMember("id",     author.id,                                   j_doc.GetAllocator());

        j_authors.PushBack(j_author, j_doc.GetAllocator());
    }
    j_entry.AddMember("authors", j_authors, j_doc.GetAllocator());

    // Vehicle configurations
    j_entry.AddMember("default_skin",        rapidjson::StringRef(entry.default_skin.c_str()),      j_doc.GetAllocator());
    rapidjson::Value j_sectionconfigs(rapidjson::kArrayType);
    for (CacheActorConfigInfo const& config : entry.sectionconfigs)
    {
        rapidjson::Value j_config(rapidjson::kObjectType);

        // Attributes
        j_config.AddMember("config_name",         rapidjson::StringRef(config.config_name.c_str()), j_doc.GetAllocator());
        j_config.AddMember("truckmass",           config.truckmass,         j_doc.GetAllocator());
        j_config.AddMember("loadmass",            config.loadmass,          j_doc.GetAllocator());
        j_config.AddMember("customtach",          config.customtach,        j_doc.GetAllocator());
        j_config.AddMember("custom_particles",    config.custom_particles,  j_doc.GetAllocator());
        j_config.AddMember("forwardcommands",     config.forwardcommands,   j_doc.GetAllocator());
        j_config.AddMember("importcommands",      config.importcommands,    j_doc.GetAllocator());
        j_config.AddMember("rescuer",             config.rescuer,           j_doc.GetAllocator());
        j_config.AddMember("driveable",           config.driveable,         j_doc.GetAllocator());

        // Element counts
        j_config.AddMember("nodecount",           config.nodecount,         j_doc.GetAllocator());
        j_config.AddMember("beamcount",           config.beamcount,         j_doc.GetAllocator());
        j_config.AddMember("shockcount",          config.shockcount,        j_doc.GetAllocator());
        j_config.AddMember("fixescount",          config.fixescount,        j_doc.GetAllocator());
        j_config.AddMember("hydroscount",         config.hydroscount,       j_doc.GetAllocator());
        j_config.AddMember("tiecount",            config.tiecount,          j_doc.GetAllocator());
        j_config.AddMember("ropecount",           config.ropecount,         j_doc.GetAllocator());
        j_config.AddMember("wheelcount",          config.wheelcount,        j_doc.GetAllocator());
        j_config.AddMember("propwheelcount",      config.propwheelcount,    j_doc.GetAllocator());
        j_config.AddMember("commandscount",       config.commandscount,     j_doc.GetAllocator());
        j_config.AddMember("flarescount",         config.flarescount,       j_doc.GetAllocator());
        j_config.AddMember("propscount",          config.propscount,        j_doc.GetAllocator());
        j_config.AddMember("wingscount",          config.wingscount,        j_doc.GetAllocator());
        j_config.AddMember("turbopropscount",     config.turbopropscount,   j_doc.GetAllocator());
        j_config.AddMember("turbojetcount",       config.turbojetcount,     j_doc.GetAllocator());
        j_config.AddMember("rotatorscount",       config.rotatorscount,     j_doc.GetAllocator());
        j_config.AddMember("exhaustscount",       config.exhaustscount,     j_doc.GetAllocator());
        j_config.AddMember("flexbodiescount",     config.flexbodiescount,   j_doc.GetAllocator());
        j_config.AddMember("soundsourcescount",   config.soundsourcescount, j_doc.GetAllocator());
        j_config.AddMember("airbrakescount",      config.airbrakescount,    j_doc.GetAllocator());
        j_config.AddMember("rotatorscount",       config.rotatorscount,     j_doc.GetAllocator());
        j_config.AddMember("submeshescount",      config.submeshescount,    j_doc.GetAllocator());
        
        // Engine
        j_config.AddMember("numgears",            config.numgears,          j_doc.GetAllocator());
        j_config.AddMember("enginetype",          config.enginetype,        j_doc.GetAllocator());
        j_config.AddMember("minrpm",              config.minrpm,            j_doc.GetAllocator());
        j_config.AddMember("maxrpm",              config.maxrpm,            j_doc.GetAllocator());
        j_config.AddMember("torque",              config.torque,            j_doc.GetAllocator());

        j_sectionconfigs.PushBack(j_config, j_doc.GetAllocator());
    }

    j_entry.AddMember("sectionconfigs", j_sectionconfigs, j_doc.GetAllocator());

    // Add entry to list
    j_entries.PushBack(j_entry, j_doc.GetAllocator());
}

void CacheSystem::WriteCacheFileJson()
{
    // Basic file structure
    rapidjson::Document j_doc;
    j_doc.SetObject();
    j_doc.AddMember("format_version", CACHE_FILE_FORMAT, j_doc.GetAllocator());
    j_doc.AddMember("global_hash", rapidjson::StringRef(m_filenames_hash_generated.c_str()), j_doc.GetAllocator());

    // Entries
    rapidjson::Value j_entries(rapidjson::kArrayType);
    for (CacheEntry const& entry : m_entries)
    {
        if (!entry.deleted)
        {
            this->ExportEntryToJson(j_entries, j_doc, entry);
        }
    }
    j_doc.AddMember("entries", j_entries, j_doc.GetAllocator());

    // Write to file
    if (App::GetContentManager()->SerializeAndWriteJson(CACHE_FILE, RGN_CACHE, j_doc)) // Logs errors
    {
        RoR::LogFormat("[RoR|ModCache] File '%s' written OK", CACHE_FILE);
    }
}

void CacheSystem::ClearCache()
{
    App::GetContentManager()->DeleteDiskFile(CACHE_FILE, RGN_CACHE);
    for (auto& entry : m_entries)
    {
        String group = entry.resource_group;
        if (!group.empty())
        {
            if (ResourceGroupManager::getSingleton().resourceGroupExists(group))
                ResourceGroupManager::getSingleton().destroyResourceGroup(group);
        }
        this->RemoveFileCache(entry);
    }
    m_entries.clear();
}

Ogre::String CacheSystem::StripUIDfromString(Ogre::String uidstr)
{
    size_t pos = uidstr.find("-");
    if (pos != String::npos && pos >= 3 && uidstr.substr(pos - 3, 3) == "UID")
        return uidstr.substr(pos + 1, uidstr.length() - pos);
    return uidstr;
}

Ogre::String CacheSystem::StripSHA1fromString(Ogre::String sha1str)
{
    size_t pos = sha1str.find_first_of("-_");
    if (pos != String::npos && pos >= 20)
        return sha1str.substr(pos + 1, sha1str.length() - pos);
    return sha1str;
}

void CacheSystem::AddFile(String group, Ogre::FileInfo f, String ext)
{
    String type = f.archive ? f.archive->getType() : "FileSystem";
    String path = f.archive ? f.archive->getName() : "";

    if (std::find_if(m_entries.begin(), m_entries.end(), [&](CacheEntry& e)
                { return !e.deleted && e.fname == f.filename && e.resource_bundle_path == path; }) != m_entries.end())
        return;

    RoR::LogFormat("[RoR|CacheSystem] Preparing to add file '%f'", f.filename.c_str());

    try
    {
        DataStreamPtr ds = ResourceGroupManager::getSingleton().openResource(f.filename, group);
        // ds closes automatically, so do _not_ close it explicitly below

        std::vector<CacheEntry> new_entries;
        if (ext == "terrn2")
        {
            new_entries.resize(1);
            FillTerrainDetailInfo(new_entries.back(), ds, f.filename);
        }
        else if (ext == "skin")
        {
            auto new_skins = RoR::SkinParser::ParseSkins(ds);
            for (auto skin_def: new_skins)
            {
                CacheEntry entry;
                if (!skin_def->author_name.empty())
                {
                    AuthorInfo a;
                    a.id = skin_def->author_id;
                    a.name = skin_def->author_name;
                    entry.authors.push_back(a);
                }

                entry.dname       = skin_def->name;
                entry.guid        = skin_def->guid;
                entry.description = skin_def->description;
                entry.categoryid  = -1;
                entry.skin_def    = skin_def; // Needed to generate preview image

                new_entries.push_back(entry);
            }
        }
        else
        {
            new_entries.resize(1);
            FillTruckDetailInfo(new_entries.back(), ds, f.filename, group);
        }

        for (auto& entry: new_entries)
        {
            Ogre::StringUtil::toLowerCase(entry.guid); // Important for comparsion
            entry.fpath = f.path;
            entry.fname = f.filename;
            entry.fname_without_uid = StripUIDfromString(f.filename);
            entry.fext = ext;
            if (type == "Zip")
            {
                entry.filetime = RoR::GetFileLastModifiedTime(path);
            }
            else
            {
                entry.filetime = RoR::GetFileLastModifiedTime(PathCombine(path, f.filename));
            }
            entry.resource_bundle_type = type;
            entry.resource_bundle_path = path;
            entry.number = static_cast<int>(m_entries.size() + 1); // Let's number mods from 1
            entry.addtimestamp = m_update_time;
            this->GenerateFileCache(entry, group);
            m_entries.push_back(entry);
        }
    }
    catch (Ogre::Exception& e)
    {
        RoR::LogFormat("[RoR|CacheSystem] Error processing file '%s', message :%s",
            f.filename.c_str(), e.getFullDescription().c_str());
    }
}

void CacheSystem::FillTruckDetailInfo(CacheEntry& entry, Ogre::DataStreamPtr stream, String file_name, String group)
{
    // Load the document
    RigDef::Parser parser;
    parser.Prepare();
    parser.ProcessOgreStream(stream.getPointer(), group);
    parser.Finalize();
    RigDef::DocumentPtr document = parser.GetFile();

    // Fill info
    entry.dname = document->name;
    std::vector<Ogre::String>::iterator desc_itor = document->description.begin();
    for (; desc_itor != document->description.end(); desc_itor++)
    {
        entry.description += *desc_itor + "\n";
    }
    if (document->guid.size() > 0)
    {
        entry.guid = document->guid[document->guid.size() - 1].guid;
    }

    // Fill authors
    std::vector<RigDef::Author>::iterator author_itor = document->author.begin();
    for (; author_itor != document->author.end(); author_itor++)
    {
        AuthorInfo author;
        author.email = author_itor->email;
        author.id = (author_itor->_has_forum_account) ? static_cast<int>(author_itor->forum_account_id) : -1;
        author.name = author_itor->name;
        author.type = author_itor->type;

        entry.authors.push_back(author);
    }

    /* Default skin */
    if (document->default_skin.size() > 0)
    {
        entry.default_skin = document->default_skin.back().skin_name;
    }

    // File info
    if (document->fileinfo.size() > 0)
    {
        RigDef::Fileinfo& data = document->fileinfo[document->fileinfo.size() - 1];

        entry.uniqueid = data.unique_id;
        entry.categoryid = static_cast<int>(data.category_id);
        entry.version = static_cast<int>(data.file_version);
    }

    // Fill per-configuration data
    ActorSpawner spawner;
    if (document->sectionconfig.size() == 0)
    {
        CacheActorConfigInfo config_info;
        spawner.FillCacheConfigInfo(document, "", config_info);
        entry.sectionconfigs.push_back(config_info);
    }
    else
    {
        for (RigDef::SectionConfig& config : document->sectionconfig)
        {
            CacheActorConfigInfo config_info;
            spawner.FillCacheConfigInfo(document, config.name, config_info);
            entry.sectionconfigs.push_back(config_info);
        }
    }
}

Ogre::String detectMiniType(String filename, String group)
{
    if (ResourceGroupManager::getSingleton().resourceExists(group, filename + "dds"))
        return "dds";

    if (ResourceGroupManager::getSingleton().resourceExists(group, filename + "png"))
        return "png";

    if (ResourceGroupManager::getSingleton().resourceExists(group, filename + "jpg"))
        return "jpg";

    return "";
}

void CacheSystem::RemoveFileCache(CacheEntry& entry)
{
    if (!entry.filecachename.empty())
    {
        App::GetContentManager()->DeleteDiskFile(entry.filecachename, RGN_CACHE);
    }
}

void CacheSystem::GenerateFileCache(CacheEntry& entry, String group)
{
    if (entry.fname.empty())
        return;

    String bundle_basename, bundle_path;
    StringUtil::splitFilename(entry.resource_bundle_path, bundle_basename, bundle_path);

    String src_path;
    String dst_path;
    if (entry.fext == "skin")
    {
        if (entry.skin_def->thumbnail.empty())
            return;
        src_path = entry.skin_def->thumbnail;
        String mini_fbase, minitype;
        StringUtil::splitBaseFilename(entry.skin_def->thumbnail, mini_fbase, minitype);
        dst_path = bundle_basename + "_" + mini_fbase + ".mini." + minitype;
    }
    else
    {
        String fbase, fext;
        StringUtil::splitBaseFilename(entry.fname, fbase, fext);
        String minifn = fbase + "-mini.";
        String minitype = detectMiniType(minifn, group);
        if (minitype.empty())
            return;
        src_path = minifn + minitype;
        dst_path = bundle_basename + "_" + entry.fname + ".mini." + minitype;
    }

    try
    {
        DataStreamPtr src_ds = ResourceGroupManager::getSingleton().openResource(src_path, group);
        DataStreamPtr dst_ds = ResourceGroupManager::getSingleton().createResource(dst_path, RGN_CACHE, true);
        std::vector<char> buf(src_ds->size());
        size_t read = src_ds->read(buf.data(), src_ds->size());
        if (read > 0)
        {
            dst_ds->write(buf.data(), read); 
            entry.filecachename = dst_path;
        }
    }
    catch (Ogre::Exception& e)
    {
        LOG("error while generating file cache: " + e.getFullDescription());
    }

    LOG("done generating file cache!");
}

void CacheSystem::ParseZipArchives(String group)
{
    auto files = ResourceGroupManager::getSingleton().findResourceFileInfo(group, "*.zip");
    auto skinzips = ResourceGroupManager::getSingleton().findResourceFileInfo(group, "*.skinzip");
    for (const auto& skinzip : *skinzips)
        files->push_back(skinzip);

    int i = 0, count = static_cast<int>(files->size());
    for (const auto& file : *files)
    {
        int progress = ((float)i++ / (float)count) * 100;
        std::string text = fmt::format("{}{}\n{}\n{}/{}",
            _L("Loading zips in group "), group, file.filename, i, count);
        RoR::App::GetGuiManager()->LoadingWindow.SetProgress(progress, text);

        String path = PathCombine(file.archive->getName(), file.filename);
        this->ParseSingleZip(path);
    }

    RoR::App::GetGuiManager()->LoadingWindow.SetVisible(false);
    App::GetGuiManager()->GameMainMenu.CacheUpdatedNotice();
}

void CacheSystem::ParseSingleZip(String path)
{
    if (std::find(m_resource_paths.begin(), m_resource_paths.end(), path) == m_resource_paths.end())
    {
        RoR::LogFormat("[RoR|ModCache] Adding archive '%s'", path.c_str());
        ResourceGroupManager::getSingleton().createResourceGroup(RGN_TEMP, false);
        try
        {
            ResourceGroupManager::getSingleton().addResourceLocation(path, "Zip", RGN_TEMP);
            if (ParseKnownFiles(RGN_TEMP))
            {
                LOG("No usable content in: '" + path + "'");
            }
        }
        catch (Ogre::Exception& e)
        {
            LOG("Error while opening archive: '" + path + "': " + e.getFullDescription());
        }
        ResourceGroupManager::getSingleton().destroyResourceGroup(RGN_TEMP);
        m_resource_paths.insert(path);
    }
}

bool CacheSystem::ParseKnownFiles(Ogre::String group)
{
    bool empty = true;
    for (auto ext : m_known_extensions)
    {
        auto files = ResourceGroupManager::getSingleton().findResourceFileInfo(group, "*." + ext);
        for (const auto& file : *files)
        {
            this->AddFile(group, file, ext);
            empty = false;
        }
    }
    return empty;
}

void CacheSystem::GenerateHashFromFilenames()
{
    std::string filenames = App::GetContentManager()->ListAllUserContent();
    m_filenames_hash_generated = HashData(filenames.c_str(), static_cast<int>(filenames.size()));
}

void CacheSystem::FillTerrainDetailInfo(CacheEntry& entry, Ogre::DataStreamPtr ds, Ogre::String fname)
{
    Terrn2Def def;
    Terrn2Parser parser;
    parser.LoadTerrn2(def, ds);

    for (Terrn2Author& author : def.authors)
    {
        AuthorInfo a;
        a.id = -1;
        a.name = author.name;
        a.type = author.type;
        entry.authors.push_back(a);
    }

    entry.dname      = def.name;
    entry.categoryid = def.category_id;
    entry.uniqueid   = def.guid;
    entry.version    = def.version;
}

bool CacheSystem::CheckResourceLoaded(Ogre::String & filename)
{
    Ogre::String group = "";
    return CheckResourceLoaded(filename, group);
}

bool CacheSystem::CheckResourceLoaded(Ogre::String & filename, Ogre::String& group)
{
    try
    {
        // check if we already loaded it via ogre ...
        if (ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(filename))
        {
            group = ResourceGroupManager::getSingleton().findGroupContainingResource(filename);
            return true;
        }

        for (auto& entry : m_entries)
        {
            // case insensitive comparison
            String fname = entry.fname;
            String fname_without_uid = entry.fname_without_uid;
            StringUtil::toLowerCase(fname);
            StringUtil::toLowerCase(filename);
            StringUtil::toLowerCase(fname_without_uid);
            if (fname == filename || fname_without_uid == filename)
            {
                // we found the file, load it
                LoadResource(entry);
                filename = entry.fname;
                group = entry.resource_group;
                return !group.empty() && ResourceGroupManager::getSingleton().resourceExists(group, filename);
            }
        }
    }
    catch (Ogre::Exception) {} // Already logged by OGRE

    return false;
}

void CacheSystem::LoadResource(CacheEntry& t)
{
    // Check if already loaded for this entry.
    if (t.resource_group != "")
    {
        return;
    }

    Ogre::String group = "bundle " + t.resource_bundle_path; // Compose group name from full path.

    // Load now.
    try
    {
        if (t.fext == "terrn2")
        {
            // PagedGeometry is hardcoded to use `Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME`
            ResourceGroupManager::getSingleton().createResourceGroup(group, /*inGlobalPool=*/true);
            ResourceGroupManager::getSingleton().addResourceLocation(t.resource_bundle_path, t.resource_bundle_type, group);
        }
        else if (t.fext == "skin")
        {
            // This is a SkinZip bundle - use `inGlobalPool=false` to prevent resource name conflicts.
            // Note: this code won't execute for .skin files in vehicle-bundles because in such case the bundle is already loaded by the vehicle's CacheEntry.
            ResourceGroupManager::getSingleton().createResourceGroup(group, /*inGlobalPool=*/false);
            ResourceGroupManager::getSingleton().addResourceLocation(t.resource_bundle_path, t.resource_bundle_type, group);
            App::GetContentManager()->InitManagedMaterials(group);
        }
        else
        {
            // A vehicle bundle - use `inGlobalPool=false` to prevent resource name conflicts.
            // See bottom 'note' at https://ogrecave.github.io/ogre/api/latest/_resource-_management.html#Resource-Groups
            ResourceGroupManager::getSingleton().createResourceGroup(group, /*inGlobalPool=*/false);
            ResourceGroupManager::getSingleton().addResourceLocation(t.resource_bundle_path, t.resource_bundle_type, group);

            App::GetContentManager()->InitManagedMaterials(group);
            App::GetContentManager()->AddResourcePack(ContentManager::ResourcePack::TEXTURES, group);
            App::GetContentManager()->AddResourcePack(ContentManager::ResourcePack::MATERIALS, group);
            App::GetContentManager()->AddResourcePack(ContentManager::ResourcePack::MESHES, group);
        }

        ResourceGroupManager::getSingleton().initialiseResourceGroup(group);

        t.resource_group = group;

        // Inform other entries sharing this bundle (i.e. '.skin' entries in vehicle bundles)
        for (CacheEntry& i_entry: m_entries)
        {
            if (i_entry.resource_bundle_path == t.resource_bundle_path)
            {
                i_entry.resource_group = group; // Mark as loaded
            }
        }
    }
    catch (Ogre::Exception& e)
    {
        RoR::LogFormat("[RoR] Error while loading '%s', message: %s",
            t.resource_bundle_path.c_str(), e.getFullDescription().c_str());
        if (ResourceGroupManager::getSingleton().resourceGroupExists(group))
        {
            ResourceGroupManager::getSingleton().destroyResourceGroup(group);
        }
    }
}

void CacheSystem::ReLoadResource(CacheEntry& t)
{
    if (t.resource_group == "")
    {
        return; // Not loaded - nothing to do
    }

    // IMPORTANT! No actors must use the bundle while reloading, use RoR::MsgType::MSG_EDI_RELOAD_BUNDLE_REQUESTED

    this->UnLoadResource(t);
    this->LoadResource(t); // Will create the same resource group again
}

void CacheSystem::UnLoadResource(CacheEntry& t)
{
    if (t.resource_group == "")
    {
        return; // Not loaded - nothing to do
    }

    // IMPORTANT! No actors must use the bundle after reloading, use RoR::MsgType::MSG_EDI_RELOAD_BUNDLE_REQUESTED

    std::string resource_group = t.resource_group; // Keep local copy, the CacheEntry will be blanked!
    for (CacheEntry& i_entry: m_entries)
    {
        if (i_entry.resource_group == resource_group)
        {
            i_entry.actor_def = nullptr; // Delete cached truck file - force reload from disk
            i_entry.resource_group = ""; // Mark as unloaded
        }
    }

    Ogre::ResourceGroupManager::getSingleton().destroyResourceGroup(resource_group);
}

CacheEntry* CacheSystem::FetchSkinByName(std::string const & skin_name)
{
    for (CacheEntry & entry: m_entries)
    {
        if (entry.dname == skin_name && entry.fext == "skin")
        {
            return &entry;
        }
    }
    return nullptr;
}

std::shared_ptr<SkinDef> CacheSystem::FetchSkinDef(CacheEntry* cache_entry)
{
    if (cache_entry->skin_def != nullptr) // If already parsed, re-use
    {
        return cache_entry->skin_def;
    }

    try
    {
        App::GetCacheSystem()->LoadResource(*cache_entry); // Load if not already
        Ogre::DataStreamPtr ds = Ogre::ResourceGroupManager::getSingleton()
            .openResource(cache_entry->fname, cache_entry->resource_group);

        auto new_skins = RoR::SkinParser::ParseSkins(ds); // Load the '.skin' file
        for (auto def: new_skins)
        {
            for (CacheEntry& e: m_entries)
            {
                if (e.resource_bundle_path == cache_entry->resource_bundle_path
                    && e.resource_bundle_type == cache_entry->resource_bundle_type
                    && e.fname == cache_entry->fname
                    && e.dname == def->name)
                {
                    e.skin_def = def;
                    e.resource_group = cache_entry->resource_group;
                }
            }
        }

        if (cache_entry->skin_def == nullptr)
        {
            RoR::LogFormat("Definition of skin '%s' was not found in file '%s'",
               cache_entry->dname.c_str(), cache_entry->fname.c_str());
        }
        return cache_entry->skin_def;
    }
    catch (Ogre::Exception& oex)
    {
        RoR::LogFormat("[RoR] Error loading skin file '%s', message: %s",
            cache_entry->fname.c_str(), oex.getFullDescription().c_str());
        return nullptr;
    }
}

size_t CacheSystem::Query(CacheQuery& query)
{
    Ogre::StringUtil::toLowerCase(query.cqy_search_string);
    std::time_t cur_time = std::time(nullptr);
    for (CacheEntry& entry: m_entries)
    {
        // Filter by GUID
        if (!query.cqy_filter_guid.empty() && entry.guid != query.cqy_filter_guid)
        {
            continue;
        }

        // Filter by entry type
        bool add = false;
        if (entry.fext == "terrn2")
            add = (query.cqy_filter_type == LT_Terrain);
        if (entry.fext == "skin")
            add = (query.cqy_filter_type == LT_Skin);
        else if (entry.fext == "truck")
            add = (query.cqy_filter_type == LT_AllBeam || query.cqy_filter_type == LT_Vehicle || query.cqy_filter_type == LT_Truck);
        else if (entry.fext == "car")
            add = (query.cqy_filter_type == LT_AllBeam || query.cqy_filter_type == LT_Vehicle || query.cqy_filter_type == LT_Truck || query.cqy_filter_type == LT_Car);
        else if (entry.fext == "boat")
            add = (query.cqy_filter_type == LT_AllBeam || query.cqy_filter_type == LT_Boat);
        else if (entry.fext == "airplane")
            add = (query.cqy_filter_type == LT_AllBeam || query.cqy_filter_type == LT_Airplane);
        else if (entry.fext == "trailer")
            add = (query.cqy_filter_type == LT_AllBeam || query.cqy_filter_type == LT_Trailer || query.cqy_filter_type == LT_Extension);
        else if (entry.fext == "train")
            add = (query.cqy_filter_type == LT_AllBeam || query.cqy_filter_type == LT_Train);
        else if (entry.fext == "load")
            add = (query.cqy_filter_type == LT_AllBeam || query.cqy_filter_type == LT_Load || query.cqy_filter_type == LT_Extension);

        if (!add)
        {
            continue;
        }

        // Category usage stats
        query.cqy_res_category_usage[entry.categoryid]++;

        query.cqy_res_category_usage[CacheCategoryId::CID_All]++;

        const bool is_fresh = (cur_time - entry.addtimestamp) < CACHE_FILE_FRESHNESS;
        if (is_fresh)
            query.cqy_res_category_usage[CacheCategoryId::CID_Fresh]++;

        // Filter by category
        if ((query.cqy_filter_category_id <= CacheCategoryId::CID_Max && query.cqy_filter_category_id != entry.categoryid) ||
            (query.cqy_filter_category_id == CID_Fresh && !is_fresh))
        {
            continue;
        }

        // Search
        size_t score = 0;
        bool match = false;
        Str<100> wheels_str;
        switch (query.cqy_search_method)
        {
        case CacheSearchMethod::FULLTEXT:
            if (match = this->Match(score, entry.dname,       query.cqy_search_string, 0))   { break; }
            if (match = this->Match(score, entry.fname,       query.cqy_search_string, 100)) { break; }
            if (match = this->Match(score, entry.description, query.cqy_search_string, 200)) { break; }
            for (AuthorInfo const& author: entry.authors)
            {
                if (match = this->Match(score, author.name,  query.cqy_search_string, 300)) { break; }
                if (match = this->Match(score, author.email, query.cqy_search_string, 400)) { break; }
            }
            break;

        case CacheSearchMethod::GUID:
            match = this->Match(score, entry.guid, query.cqy_search_string, 0);
            break;

        case CacheSearchMethod::AUTHORS:
            for (AuthorInfo const& author: entry.authors)
            {
                if (match = this->Match(score, author.name,  query.cqy_search_string, 0)) { break; }
                if (match = this->Match(score, author.email, query.cqy_search_string, 0)) { break; }
            }
            break;

        case CacheSearchMethod::WHEELS:
            for (CacheActorConfigInfo& info : entry.sectionconfigs)
            {
                if (!match)
                {
                    wheels_str << info.wheelcount << "x" << info.propwheelcount;
                    match = match || this->Match(score, wheels_str.ToCStr(), query.cqy_search_string, 0);
                }
            }
            break;

        case CacheSearchMethod::FILENAME:
            match = this->Match(score, entry.fname, query.cqy_search_string, 100);
            break;

        default: // CacheSearchMethod::NONE
            match = true;
            break;
        };

        if (match)
        {
            query.cqy_results.emplace_back(&entry, score);
            query.cqy_res_last_update = std::max(query.cqy_res_last_update, entry.addtimestamp);
        }
    }

    std::sort(query.cqy_results.begin(), query.cqy_results.end());
    return query.cqy_results.size();
}

bool CacheSystem::Match(size_t& out_score, std::string data, std::string const& query, size_t score)
{
    Ogre::StringUtil::toLowerCase(data);
    size_t pos = data.find(query);
    if (pos != std::string::npos)
    {
        out_score = score + pos;
        return true;
    }
    else
    {
        return false;
    }
}

bool CacheQueryResult::operator<(CacheQueryResult const& other) const
{
    if (cqr_score == other.cqr_score)
    {
        Ogre::String first = this->cqr_entry->dname;
        Ogre::String second = other.cqr_entry->dname;
        Ogre::StringUtil::toLowerCase(first);
        Ogre::StringUtil::toLowerCase(second);
        return first < second;
    }

    return cqr_score < other.cqr_score;
}

