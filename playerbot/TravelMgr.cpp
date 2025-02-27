#pragma once

#include "TransportMgr.h"

#include "TravelMgr.h"
#include "TravelNode.h"
#include "PlayerbotAI.h"

#include "ObjectMgr.h"
#include <numeric>
#include <iomanip>

#include "PathFinder.h"
#include "PlayerbotAI.h"
#include "VMapFactory.h"
#include "MoveMap.h"
#include "Transports.h"
#include <playerbot/strategy/StrategyContext.h>

#include "strategy/values/SharedValueContext.h"

#include "Grids/CellImpl.h"
#include <playerbot/ServerFacade.h>

using namespace ai;
using namespace MaNGOS;

WorldPosition::WorldPosition(ObjectGuid guid)
{
    switch (guid.GetHigh())
    {
    case HIGHGUID_PLAYER:
    {
        Player* player = sObjectAccessor.FindPlayer(guid);
        if (player)
            wLoc = WorldLocation(player->GetMapId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation());
        break;
    }
    case HIGHGUID_GAMEOBJECT:    
    {
        GameObjectDataPair const* gpair = sObjectMgr.GetGODataPair(guid.GetCounter());
        if (gpair)
            wLoc = WorldLocation(gpair->second.mapid, gpair->second.posX, gpair->second.posY, gpair->second.posZ, gpair->second.orientation);
        break;
    }
    case HIGHGUID_UNIT:
    {
        CreatureDataPair const* cpair = sObjectMgr.GetCreatureDataPair(guid.GetCounter());
        if (cpair)
            wLoc = WorldLocation(cpair->second.mapid, cpair->second.posX, cpair->second.posY, cpair->second.posZ, cpair->second.orientation);
        break;
    }
    case HIGHGUID_TRANSPORT:
    case HIGHGUID_MO_TRANSPORT:
    case HIGHGUID_ITEM:
    case HIGHGUID_PET:
    case HIGHGUID_DYNAMICOBJECT:
    case HIGHGUID_CORPSE:
        return;
    }
}

WorldPosition::WorldPosition(GuidPosition gpos)
{
    wLoc = gpos.getPosition().wLoc;
}

WorldPosition::WorldPosition(vector<WorldPosition*> list, WorldPositionConst conType)
{
    uint32 size = list.size();
    if (size == 0)
        return;
    else if (size == 1)
        wLoc = list.front()->wLoc;
    else if (conType == WP_RANDOM)
        wLoc = list[urand(0, size - 1)]->wLoc;
    else if (conType == WP_CENTROID)
        wLoc = std::accumulate(list.begin(), list.end(), WorldLocation(list[0]->getMapId(), 0, 0, 0, 0), [size](WorldLocation i, WorldPosition* j) {i.coord_x += j->getX() / size; i.coord_y += j->getY() / size; i.coord_z += j->getZ() / size; i.orientation += j->getO() / size; return i; });
    else if (conType == WP_MEAN_CENTROID)
    {
        WorldPosition pos = WorldPosition(list, WP_CENTROID);
        wLoc = pos.closestSq(list)->wLoc;
    }
}

WorldPosition::WorldPosition(vector<WorldPosition> list, WorldPositionConst conType)
{    
    uint32 size = list.size();
    if (size == 0)
        return;
    else if (size == 1)
        wLoc = list.front().wLoc;
    else if (conType == WP_RANDOM)
        wLoc = list[urand(0, size - 1)].wLoc;
    else if (conType == WP_CENTROID)
        wLoc = std::accumulate(list.begin(), list.end(), WorldLocation(list[0].getMapId(), 0, 0, 0, 0), [size](WorldLocation i, WorldPosition j) {i.coord_x += j.getX() / size; i.coord_y += j.getY() / size; i.coord_z += j.getZ() / size; i.orientation += j.getO() / size; return i; });
    else if (conType == WP_MEAN_CENTROID)
    {
        WorldPosition pos = WorldPosition(list, WP_CENTROID);
        wLoc = pos.closestSq(list).wLoc;
    }    
}


float WorldPosition::distance(WorldPosition* center)
{
    if(wLoc.mapid == center->getMapId())
        return relPoint(center).size(); 

    //this -> mapTransfer | mapTransfer -> center
    return sTravelMgr.mapTransDistance(*this, *center);
};

float WorldPosition::fDist(WorldPosition* center)
{
    if (wLoc.mapid == center->getMapId())
        return sqrt(sqDistance2d(center));

    //this -> mapTransfer | mapTransfer -> center
    return sTravelMgr.fastMapTransDistance(*this, *center);
};

//When moving from this along list return last point that falls within range.
//Distance is move distance along path.
WorldPosition WorldPosition::lastInRange(vector<WorldPosition> list, float minDist, float maxDist)
{
    WorldPosition rPoint;

    float startDist = 0.0f;

    //Enter the path at the closest point.
    for (auto& p : list)
    {
        float curDist = distance(p);
        if (startDist < curDist || p == list.front())
            startDist = curDist + 0.1f;
    }

    float totalDist = 0.0f;

    //Follow the path from the last nearest point
    //Return last point in range.
    for (auto& p : list)
    {
        float curDist = distance(p);

        if (totalDist > 0) //We have started the path. Keep counting.
            totalDist += p.distance(std::prev(&p, 1));

        if (curDist == startDist) //Start the path here.
            totalDist = startDist;

        if (minDist > 0 && totalDist < minDist)
            continue;

        if (maxDist > 0 && totalDist > maxDist)
            continue; //We do not break here because the path may loop back and have a second startDist point.

        rPoint = p;
    }

    return rPoint;
};

//Todo: remove or adjust to above standard.
WorldPosition WorldPosition::firstOutRange(vector<WorldPosition> list, float minDist, float maxDist)
{
    WorldPosition rPoint;

    for (auto& p : list)
    {
        if (minDist > 0 && distance(p) < minDist)
            return p;

        if (maxDist > 0 && distance(p) > maxDist)
            return p;

        rPoint = p;
    }

    return rPoint;
}

//Returns true if (on the x-y plane) the position is inside the three points.
bool WorldPosition::isInside(WorldPosition* p1, WorldPosition* p2, WorldPosition* p3)
{
    if (getMapId() != p1->getMapId() != p2->getMapId() != p3->getMapId())
        return false;

    float d1, d2, d3;
    bool has_neg, has_pos;

    d1 = mSign(p1, p2);
    d2 = mSign(p2, p3);
    d3 = mSign(p3, p1);

    has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

G3D::Vector3 WorldPosition::getVector3() 
{
    return G3D::Vector3(wLoc.coord_x, wLoc.coord_y, wLoc.coord_z); 
}

string WorldPosition::print()
{
    ostringstream out;
    out << wLoc.mapid << std::fixed << std::setprecision(2);
    out << ';'<< wLoc.coord_x;
    out << ';' << wLoc.coord_y;
    out << ';' << wLoc.coord_z;
    out << ';' << wLoc.orientation;

    return out.str();
}

void WorldPosition::printWKT(vector<WorldPosition> points, ostringstream& out, uint32 dim, bool loop)
{
    switch (dim) {
    case 0:
        if(points.size() == 1)
            out << "\"POINT(";
        else
            out << "\"MULTIPOINT(";
        break;
    case 1:
        out << "\"LINESTRING(";
        break;
    case 2:
        out << "\"POLYGON((";
    }

    for (auto& p : points)
        out << p.getDisplayX() << " " << p.getDisplayY() << (!loop && &p == &points.back() ? "" : ",");

    if (loop)
        out << points.front().getDisplayX() << " " << points.front().getDisplayY();

    out << (dim == 2 ? "))\"," : ")\",");
}

WorldPosition WorldPosition::getDisplayLocation() 
{ 
    return offset(&sTravelNodeMap.getMapOffset(getMapId())); 
};

AreaTableEntry const* WorldPosition::getArea()
{
    uint16 areaFlag = getAreaFlag();

    if(!areaFlag)
        return NULL;

    return GetAreaEntryByAreaFlagAndMap(areaFlag, getMapId());
}

string WorldPosition::getAreaName(bool fullName, bool zoneName)
{    
    if (!isOverworld())
    {
        MapEntry const* map = sMapStore.LookupEntry(getMapId());
        if (map)
            return map->name[0];
    }

    AreaTableEntry const* area = getArea();

    if (!area)
        return "";

    string areaName = area->area_name[0];

    if (fullName)
    {
        uint16 zoneId = area->zone;

        while (zoneId > 0)
        {
            AreaTableEntry const* parentArea = GetAreaEntryByAreaID(zoneId);

            if (!parentArea)
                break;

            string subAreaName = parentArea->area_name[0];

            if (zoneName)
                areaName = subAreaName;
            else
                areaName = subAreaName + " " + areaName;

            zoneId = parentArea->zone;
        }
    }

    return areaName;
}

std::set<Transport*> WorldPosition::getTransports(uint32 entry)
{
    /*
    if(!entry)
        return getMap()->m_transports;
    else
    {
    */
        std::set<Transport*> transports;
        /*
        for (auto transport : getMap()->m_transports)
            if(transport->GetEntry() == entry)
                transports.insert(transport);

        return transports;
    }
    */
    return transports;
}

std::vector<GridPair> WorldPosition::getGridPairs(WorldPosition secondPos)
{
    std::vector<GridPair> retVec;

    int lx = std::min(getGridPair().x_coord, secondPos.getGridPair().x_coord);
    int ly = std::min(getGridPair().y_coord, secondPos.getGridPair().y_coord);
    int ux = std::max(getGridPair().x_coord, secondPos.getGridPair().x_coord);
    int uy = std::max(getGridPair().y_coord, secondPos.getGridPair().y_coord);
    int border = 1;

    lx = std::min(std::max(border, lx), MAX_NUMBER_OF_GRIDS - border);
    ly = std::min(std::max(border, ly), MAX_NUMBER_OF_GRIDS - border);
    ux = std::min(std::max(border, ux), MAX_NUMBER_OF_GRIDS - border);
    uy = std::min(std::max(border, uy), MAX_NUMBER_OF_GRIDS - border);

    for (int x = lx - border; x <= ux + border; x++)
    {
        for (int y = ly - border; y <= uy + border; y++)
        {
            retVec.push_back(GridPair(x, y));
        }
    }

    return retVec;
}

vector<WorldPosition> WorldPosition::fromGridPair(GridPair gridPair)
{
    vector<WorldPosition> retVec;
    GridPair g;

    for (uint32 d = 0; d < 4; d++)
    {
        g = gridPair;

        if (d == 1 || d == 2)
            g >> 1;
        if (d == 2 || d == 3)
            g += 1;

        retVec.push_back(WorldPosition(getMapId(), g));
    }
   
    return retVec;
}

vector<WorldPosition> WorldPosition::fromCellPair(CellPair cellPair)
{
    vector<WorldPosition> retVec;
    CellPair p;

    for (uint32 d = 0; d < 4; d++)
    {
        p = cellPair;

        if (d == 1 || d == 2)
            p >> 1;
        if (d == 2 || d == 3)
            p += 1;

        retVec.push_back(WorldPosition(getMapId(), p));
    }
    return retVec;
}

vector<WorldPosition> WorldPosition::gridFromCellPair(CellPair cellPair)
{    
    Cell c(cellPair);

    return fromGridPair(GridPair(c.GridX(), c.GridY()));
}

vector<pair<int,int>> WorldPosition::getmGridPairs(WorldPosition secondPos)
{
    std::vector<mGridPair> retVec;

    int lx = std::min(getmGridPair().first, secondPos.getmGridPair().first);
    int ly = std::min(getmGridPair().second, secondPos.getmGridPair().second);
    int ux = std::max(getmGridPair().first, secondPos.getmGridPair().first);
    int uy = std::max(getmGridPair().second, secondPos.getmGridPair().second);
    int border = 1;

    //lx = std::min(std::max(border, lx), MAX_NUMBER_OF_GRIDS - border);
    //ly = std::min(std::max(border, ly), MAX_NUMBER_OF_GRIDS - border);
    //ux = std::min(std::max(border, ux), MAX_NUMBER_OF_GRIDS - border);
    //uy = std::min(std::max(border, uy), MAX_NUMBER_OF_GRIDS - border);

    for (int x = lx - border; x <= ux + border; x++)
    {
        for (int y = ly - border; y <= uy + border; y++)
        {
            retVec.push_back(make_pair(x, y));
        }
    }

    return retVec;
}

vector<WorldPosition> WorldPosition::frommGridPair(mGridPair gridPair)
{
    vector<WorldPosition> retVec;
    mGridPair g;

    for (uint32 d = 0; d < 4; d++)
    {
        g = gridPair;

        if (d == 1 || d == 2)
            g.second++;
        if (d == 2 || d == 3)
            g.first++;

        retVec.push_back(WorldPosition(getMapId(), g));
    }

    return retVec;
}

void WorldPosition::loadMapAndVMap(uint32 mapId, int x, int y)
{
    string fileName = "load_map_grid.csv";

    if (isOverworld() && false || false)
    {
        if (!MMAP::MMapFactory::createOrGetMMapManager()->IsMMapIsLoaded(mapId, x, y))
            if (sPlayerbotAIConfig.hasLog(fileName))
            {
                ostringstream out;
                out << sPlayerbotAIConfig.GetTimestampStr();
                out << "+00,\"mmap\", " << x << "," << y << "," << (sTravelMgr.isBadMmap(mapId, x, y) ? "0" : "1") << ",";
                printWKT(fromGridPair(GridPair(x, y)), out, 1, true);
                sPlayerbotAIConfig.log(fileName, out.str().c_str());
            }

        int px = (float)(32 - x) * SIZE_OF_GRIDS;
        int py = (float)(32 - y) * SIZE_OF_GRIDS;

        if (!MMAP::MMapFactory::createOrGetMMapManager()->IsMMapIsLoaded(mapId, x, y))
            if(getTerrain())
                getTerrain()->GetTerrainType(px, py);

    }
    else
    {

        //This needs to be disabled or maps will not load.
        //Needs more testing to check for impact on movement.
        if (false)
            if (!VMAP::VMapFactory::createOrGetVMapManager()->IsTileLoaded(mapId, x, y) && !sTravelMgr.isBadVmap(mapId, x, y))
            {
                // load VMAPs for current map/grid...
                const MapEntry* i_mapEntry = sMapStore.LookupEntry(mapId);
                const char* mapName = i_mapEntry ? i_mapEntry->name[sWorld.GetDefaultDbcLocale()] : "UNNAMEDMAP\x0";

                int vmapLoadResult = VMAP::VMapFactory::createOrGetVMapManager()->loadMap((sWorld.GetDataPath() + "vmaps").c_str(), mapId, x, y);
                switch (vmapLoadResult)
                {
                case VMAP::VMAP_LOAD_RESULT_OK:
                    //sLog.outError("VMAP loaded name:%s, id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)", mapName, mapId, x, y, x, y);
                    break;
                case VMAP::VMAP_LOAD_RESULT_ERROR:
                    //sLog.outError("Could not load VMAP name:%s, id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)", mapName, mapId, x, y, x, y);
                    sTravelMgr.addBadVmap(mapId, x, y);
                    break;
                case VMAP::VMAP_LOAD_RESULT_IGNORED:
                    sTravelMgr.addBadVmap(mapId, x, y);
                    //sLog.outError("Ignored VMAP name:%s, id:%d, x:%d, y:%d (vmap rep.: x:%d, y:%d)", mapName, mapId, x, y, x, y);
                    break;
                }

                if (sPlayerbotAIConfig.hasLog(fileName))
                {
                    ostringstream out;
                    out << sPlayerbotAIConfig.GetTimestampStr();
                    out << "+00,\"vmap\", " << x << "," << y << ", " << (sTravelMgr.isBadVmap(mapId, x, y) ? "0" : "1") << ",";
                    printWKT(fromGridPair(GridPair(x, y)), out, 1, true);
                    sPlayerbotAIConfig.log(fileName, out.str().c_str());
                }
            }

        if (!MMAP::MMapFactory::createOrGetMMapManager()->IsMMapIsLoaded(mapId, x, y) && !sTravelMgr.isBadMmap(mapId, x, y))
        {
            // load navmesh
            if (!MMAP::MMapFactory::createOrGetMMapManager()->loadMap(mapId, x, y))
                sTravelMgr.addBadMmap(mapId, x, y);

            if (sPlayerbotAIConfig.hasLog(fileName))
            {
                ostringstream out;
                out << sPlayerbotAIConfig.GetTimestampStr();
                out << "+00,\"mmap\", " << x << "," << y << "," << (sTravelMgr.isBadMmap(mapId, x, y) ? "0" : "1") << ",";
                printWKT(frommGridPair(mGridPair(x, y)), out, 1, true);
                sPlayerbotAIConfig.log(fileName, out.str().c_str());
            }
        }
    }
}

void WorldPosition::loadMapAndVMaps(WorldPosition secondPos)
{
    for (auto& grid : getmGridPairs(secondPos))
    {
        loadMapAndVMap(getMapId(), grid.first, grid.second);
    }
}

vector<WorldPosition> WorldPosition::fromPointsArray(std::vector<G3D::Vector3> path)
{
    vector<WorldPosition> retVec;
    for (auto p : path)
        retVec.push_back(WorldPosition(getMapId(), p.x, p.y, p.z, getO()));

    return retVec;
}

//A single pathfinding attempt from one position to another. Returns pathfinding status and path.
vector<WorldPosition> WorldPosition::getPathStepFrom(WorldPosition startPos, Unit* bot)
{
    //Load mmaps and vmaps between the two points.
    loadMapAndVMaps(startPos);

    PointsArray points;
    PathType type;

    if (bot)
    {
        PathFinder path(bot);

#ifdef IKE_PATHFINDER
        path.setAreaCost(8, 10.0f);  //Water
        path.setAreaCost(11, 5.0f);  //Mob proximity
        path.setAreaCost(12, 20.0f); //Mob agro
#endif
        path.calculate(startPos.getVector3(), getVector3(), false);

        points = path.getPath();
        type = path.getPathType();

    }
    else
#ifdef IKE_PATHFINDER
    {
        std::hash<std::thread::id> hasher;
        PathFinder path(getMapId(), hasher(std::this_thread::get_id()));

        path.setAreaCost(8, 10.0f);
        path.setAreaCost(11, 5.0f);
        path.setAreaCost(12, 20.0f);

        path.calculate(startPos.getVector3(), getVector3(), false);

        points = path.getPath();
        type = path.getPathType();
    }
#else
        return PATHFIND_NOPATH;
#endif

    if (sPlayerbotAIConfig.hasLog("pathfind_attempt_point.csv"))
    {
        ostringstream out;
        out << std::fixed << std::setprecision(1);
        printWKT({ startPos, *this }, out);
        sPlayerbotAIConfig.log("pathfind_attempt_point.csv", out.str().c_str());
    }

    if (sPlayerbotAIConfig.hasLog("pathfind_attempt.csv") && (type == PATHFIND_INCOMPLETE || type == PATHFIND_NORMAL))
    {
        ostringstream out;
        out << sPlayerbotAIConfig.GetTimestampStr() << "+00,";
        out << std::fixed << std::setprecision(1) << type << ",";
        printWKT(fromPointsArray(points), out, 1);
        sPlayerbotAIConfig.log("pathfind_attempt.csv", out.str().c_str());
    }    

    if (type == PATHFIND_INCOMPLETE || type == PATHFIND_NORMAL)
        return fromPointsArray(points);
    else
        return {};

}

//A sequential series of pathfinding attempts. Returns the complete path and if the patfinder eventually found a way to the destination.
vector<WorldPosition> WorldPosition::getPathFromPath(vector<WorldPosition> startPath, Unit* bot)
{
    //We start at the end of the last path.
    WorldPosition currentPos = startPath.back();

    //No pathfinding across maps.
    if (getMapId() != currentPos.getMapId())
        return { };

    vector<WorldPosition> subPath, fullPath = startPath;

    //Limit the pathfinding attempts to 40
    for (uint32 i = 0; i < 40; i++)
    {
        //Try to pathfind to this position.
        subPath = getPathStepFrom(currentPos, bot);

        //If we could not find a path return what we have now.
        if (subPath.empty() || currentPos.distance(&subPath.back()) < sPlayerbotAIConfig.targetPosRecalcDistance)
            break;
        
        //Append the path excluding the start (this should be the same as the end of the startPath)
        fullPath.insert(fullPath.end(), std::next(subPath.begin(),1), subPath.end());

        //Are we there yet?
        if (isPathTo(subPath))
            break;

        //Continue pathfinding.
        currentPos = subPath.back();
    }

    return fullPath;
}

uint32 WorldPosition::getUnitsNear(list<ObjectGuid>& units, float radius)
{
    units.remove_if([this, radius](ObjectGuid guid) {return this->sqDistance(WorldPosition(guid)) > radius * radius; });

    return units.size();
};

uint32 WorldPosition::getUnitsAggro(list<ObjectGuid>& units, Player* bot)
{
    units.remove_if([this, bot](ObjectGuid guid) {Unit* unit = GuidPosition(guid).getUnit(); if (!unit) return true; return this->sqDistance(WorldPosition(guid)) > unit->GetAttackDistance(bot) * unit->GetAttackDistance(bot); });

    return units.size();
};



bool FindPointCreatureData::operator()(CreatureDataPair const& dataPair)
{
    if (!entry || dataPair.second.id == entry)
        if ((!point || dataPair.second.mapid == point.getMapId()) && (!radius || point.sqDistance(WorldPosition(dataPair.second.mapid, dataPair.second.posX, dataPair.second.posY, dataPair.second.posZ)) < radius * radius))
        {
            data.push_back(&dataPair);
        }

    return false;
}

bool FindPointGameObjectData::operator()(GameObjectDataPair const& dataPair)
{
    if (!entry || dataPair.second.id == entry)
        if ((!point || dataPair.second.mapid == point.getMapId()) && (!radius || point.sqDistance(WorldPosition(dataPair.second.mapid, dataPair.second.posX, dataPair.second.posY, dataPair.second.posZ)) < radius * radius))
        {
            data.push_back(&dataPair);
        }

    return false;
}

vector<CreatureDataPair const*> WorldPosition::getCreaturesNear(float radius, uint32 entry)
{
    FindPointCreatureData worker(*this, radius, entry);
    sObjectMgr.DoCreatureData(worker);
    return worker.GetResult();
}

vector<GameObjectDataPair const*> WorldPosition::getGameObjectsNear(float radius, uint32 entry)
{
    FindPointGameObjectData worker(*this, radius, entry);
    sObjectMgr.DoGOData(worker);
    return worker.GetResult();
}

Unit* GuidPosition::getUnit()
{
    if (!*this)
        return nullptr;

    if (IsPlayer())
        return sObjectAccessor.FindPlayer(*this);

    return point.getMap()->GetAnyTypeCreature(*this);
}

GameObject* GuidPosition::getGameObject()
{
    if (!*this)
        return nullptr;

    return point.getMap()->GetGameObject(*this);
}


bool GuidPosition::isDead()
{
    if (!point.getMap())
        return false;

    if (!point.getMap()->IsLoaded(point.getX(), point.getY()))
        return false;

    if (IsUnit() && getUnit() && getUnit()->IsInWorld() && getUnit()->IsAlive())
        return false;

    if (IsGameObject() && getGameObject() && getGameObject()->IsInWorld())
        return false;

    return true;
}

vector<WorldPosition*> TravelDestination::getPoints(bool ignoreFull) {
    if (ignoreFull)
        return points;

    uint32 max = maxVisitorsPerPoint;

    if (max == 0)
        return points;

    vector<WorldPosition*> retVec;
    std::copy_if(points.begin(), points.end(), std::back_inserter(retVec), [max](WorldPosition* p) { return p->getVisitors() < max; });
    return retVec;
}

WorldPosition* TravelDestination::nearestPoint(WorldPosition* pos) {
    return *std::min_element(points.begin(), points.end(), [pos](WorldPosition* i, WorldPosition* j) {return i->distance(pos) < j->distance(pos); });
}

vector<WorldPosition*> TravelDestination::touchingPoints(WorldPosition* pos) {
    vector<WorldPosition*> ret_points;
    for (auto& point : points)
    {
        float dist = pos->distance(point);
        if (dist == 0)
            continue;

        if (dist > radiusMax * 2)
            continue;

        ret_points.push_back(point);
    }

    return ret_points;
};

vector<WorldPosition*> TravelDestination::sortedPoints(WorldPosition* pos) {
    vector<WorldPosition*> ret_points = points;

    std::sort(ret_points.begin(), ret_points.end(), [pos](WorldPosition* i, WorldPosition* j) {return i->distance(pos) < j->distance(pos); });

    return ret_points;
};

vector <WorldPosition*> TravelDestination::nextPoint(WorldPosition* pos, bool ignoreFull) {
    return sTravelMgr.getNextPoint(pos, ignoreFull ? points : getPoints());
}

bool TravelDestination::isFull(bool ignoreFull) {
    if (!ignoreFull && maxVisitors > 0 && visitors >= maxVisitors)
        return true;

    if (maxVisitorsPerPoint > 0)
        if (getPoints(ignoreFull).empty())
            return true;

    return false;
}

string QuestTravelDestination::getTitle() {
    return ChatHelper::formatQuest(questTemplate);
}

bool QuestRelationTravelDestination::isActive(Player* bot) {
    if (relation == 0)
    {
        if (questTemplate->GetQuestLevel() >= bot->GetLevel() + (uint32)5)
            return false;
        //if (questTemplate->XPValue(bot) == 0)
        //    return false;
        if (!bot->GetMap()->IsContinent() || !bot->CanTakeQuest(questTemplate, false))
            return false;

        PlayerbotAI* ai = bot->GetPlayerbotAI();
        AiObjectContext* context = ai->GetAiObjectContext();

        if (AI_VALUE(bool, "can fight equal"))
        {
            if (sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_AVAILABLE)
                return false;
    }
        else
        {
            #ifndef MANGOSBOT_TWO
            if (sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_CHAT)
            #else
            if (sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_LOW_LEVEL_AVAILABLE)
            #endif
                return false;
        }
    }
    else
    {
        if (!bot->IsActiveQuest(questId))
            return false;

        if (!bot->CanRewardQuest(questTemplate, false))
            return false;

#ifdef MANGOSBOT_ZERO
        if (sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_REWARD2 && sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_REWARD_REP)
#else
        if (sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_REWARD2 && sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_REWARD && sTravelMgr.getDialogStatus(bot, entry, questTemplate) != DIALOG_STATUS_REWARD_REP)
#endif
            return false;
    }

    return true;
}

string QuestRelationTravelDestination::getTitle() {
    ostringstream out;

    if (relation == 0)
        out << "questgiver";
    else
        out << "questtaker";

    out << " " << ChatHelper::formatWorldEntry(entry);
    return out.str();
}

bool QuestObjectiveTravelDestination::isActive(Player* bot) {
    if (questTemplate->GetQuestLevel() > bot->GetLevel() + (uint32)1)
        return false;

    PlayerbotAI* ai = bot->GetPlayerbotAI();
    AiObjectContext* context = ai->GetAiObjectContext();
    if (questTemplate->GetQuestLevel() + 5 > (int)bot->GetLevel() && !AI_VALUE(bool, "can fight equal"))
        return false;

    //Check mob level
    if (getEntry() > 0)
    {
        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(getEntry());

        if (cInfo && (int)cInfo->MaxLevel - (int)bot->GetLevel() > 4)
            return false;
    }

    if (questTemplate->GetType() == QUEST_TYPE_ELITE && !AI_VALUE(bool, "can fight boss"))
        return false;

    if (!sTravelMgr.getObjectiveStatus(bot, questTemplate, objective))
        return false;

    WorldPosition botPos(bot);

    if (getEntry() > 0 && !isOut(&botPos))
    {
        list<ObjectGuid> targets = AI_VALUE(list<ObjectGuid>, "possible targets");

        for (auto& target : targets)
            if (target.GetEntry() == getEntry() && target.IsCreature() && ai->GetCreature(target) && ai->GetCreature(target)->IsAlive())
                return true;
        
        return false;
    }

    return true;
}

string QuestObjectiveTravelDestination::getTitle() {
    ostringstream out;

    out << "objective " << objective;

    if (itemId)
        out << " loot " << ChatHelper::formatItem(sObjectMgr.GetItemPrototype(itemId), 0, 0) << " from";
    else
        out << " to kill";

    out << " " << ChatHelper::formatWorldEntry(entry);
    return out.str();
}

bool RpgTravelDestination::isActive(Player* bot)
{
    PlayerbotAI* ai = bot->GetPlayerbotAI();
    AiObjectContext* context = ai->GetAiObjectContext();

    CreatureInfo const* cInfo = this->getCreatureInfo();

    if (!cInfo)
        return false;

    bool isUsefull = false;

    if (cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR)
        if (AI_VALUE2(bool, "group or", "should sell,can sell,following party"))
            isUsefull = true;

    if (cInfo->NpcFlags & UNIT_NPC_FLAG_REPAIR)
        if (AI_VALUE2(bool, "group or", "should repair,can repair,following party"))
            isUsefull = true;

    if (!isUsefull)
        return false;

    //Once the target rpged with it is added to the ignore list. We can now move on.
    set<ObjectGuid>& ignoreList = bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<set<ObjectGuid>&>("ignore rpg target")->Get();

    for (auto& i : ignoreList)
    {
        if (i.GetEntry() == getEntry())
        {
            return false;
        }
    }

    FactionTemplateEntry const* factionEntry = sFactionTemplateStore.LookupEntry(cInfo->Faction);
    ReputationRank reaction = ai->getReaction(factionEntry);

    return reaction > REP_NEUTRAL;
}

string RpgTravelDestination::getTitle() {
    ostringstream out;

    out << "rpg npc ";

    out << " " << ChatHelper::formatWorldEntry(entry);

    return out.str();
}

bool ExploreTravelDestination::isActive(Player* bot)
{
    AreaTableEntry const* area = GetAreaEntryByAreaID(areaId);

    if (area->area_level && (uint32)area->area_level > bot->GetLevel() && bot->GetLevel() < DEFAULT_MAX_LEVEL)
        return false;

    if (area->exploreFlag == 0xffff)
        return false;
    int offset = area->exploreFlag / 32;

    uint32 val = (uint32)(1 << (area->exploreFlag % 32));
    uint32 currFields = bot->GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);

    return !(currFields & val);    
}

string ExploreTravelDestination::getTitle()
{
    return points[0]->getAreaName();
};

bool GrindTravelDestination::isActive(Player* bot)
{
    return true;
}

string GrindTravelDestination::getTitle() {
    ostringstream out;

    out << "grind mob ";

    out << " " << ChatHelper::formatWorldEntry(entry);

    return out.str();
}

bool BossTravelDestination::isActive(Player* bot)
{
    PlayerbotAI* ai = bot->GetPlayerbotAI();
    AiObjectContext* context = ai->GetAiObjectContext();

    if (!AI_VALUE(bool, "can fight boss"))
        return false;

    CreatureInfo const* cInfo = this->getCreatureInfo();

    int32 botLevel = bot->GetLevel();

    uint8 botPowerLevel = AI_VALUE(uint8, "durability");
    float levelMod = botPowerLevel / 500.0f; //(0-0.2f)
    float levelBoost = botPowerLevel / 50.0f; //(0-2.0f)

    int32 maxLevel = botLevel + 3.0;

    if ((int32)cInfo->MaxLevel > maxLevel) //@lvl5 max = 3, @lvl60 max = 57
        return false;

    int32 minLevel = botLevel - 10;

    if ((int32)cInfo->MaxLevel < minLevel) //@lvl5 min = 3, @lvl60 max = 50
        return false;

    FactionTemplateEntry const* factionEntry = sFactionTemplateStore.LookupEntry(cInfo->Faction);
    ReputationRank reaction = ai->getReaction(factionEntry);

    if (reaction >= REP_NEUTRAL)
        return false;

    WorldPosition botPos(bot);

    if (!isOut(&botPos))
    {
        list<ObjectGuid> targets = AI_VALUE(list<ObjectGuid>, "possible targets");

        for (auto& target : targets)
            if (target.GetEntry() == getEntry() && target.IsCreature() && ai->GetCreature(target) && ai->GetCreature(target)->IsAlive())
                return true;

        return false;
    }

    return true;
}

string BossTravelDestination::getTitle() {
    ostringstream out;

    out << "boss mob ";

    out << " " << ChatHelper::formatWorldEntry(entry);

    return out.str();
}

TravelTarget::~TravelTarget() {
    if (!tDestination)
        return;

    releaseVisitors();
    //sTravelMgr.botTargets.erase(std::remove(sTravelMgr.botTargets.begin(), sTravelMgr.botTargets.end(), this), sTravelMgr.botTargets.end());
}

void TravelTarget::setTarget(TravelDestination* tDestination1, WorldPosition* wPosition1, bool groupCopy1) {
    releaseVisitors();

    wPosition = wPosition1;
    tDestination = tDestination1;
    groupCopy = groupCopy1;
    forced = false;
    radius = 0;

    addVisitors();

    setStatus(TRAVEL_STATUS_TRAVEL);
}

void TravelTarget::copyTarget(TravelTarget* target) {
    setTarget(target->tDestination, target->wPosition);
    groupCopy = target->isGroupCopy();
    forced = target->forced;
    extendRetryCount = target->extendRetryCount;
}

void TravelTarget::addVisitors() {
    if (!visitor)
    {
        wPosition->addVisitor();
        tDestination->addVisitor();
    }

    visitor = true;
}

void TravelTarget::releaseVisitors() {
    if (visitor)
    {
        if (tDestination)
            tDestination->remVisitor();
        if (wPosition)
            wPosition->remVisitor();
    }

    visitor = false;
}

void TravelTarget::setStatus(TravelStatus status) {
    m_status = status;
    startTime = WorldTimer::getMSTime();

    switch (m_status) {
    case TRAVEL_STATUS_NONE:
    case TRAVEL_STATUS_PREPARE:
    case TRAVEL_STATUS_EXPIRED:
        statusTime = 1;
        break;
    case TRAVEL_STATUS_TRAVEL:
        statusTime = getMaxTravelTime() * 2 + sPlayerbotAIConfig.maxWaitForMove;
        break;
    case TRAVEL_STATUS_WORK:
        statusTime = tDestination->getExpireDelay();
        break;
    case TRAVEL_STATUS_COOLDOWN:
        statusTime = tDestination->getCooldownDelay();
    }
}

bool TravelTarget::isActive() {
    if (m_status == TRAVEL_STATUS_NONE || m_status == TRAVEL_STATUS_EXPIRED || m_status == TRAVEL_STATUS_PREPARE)
        return false;

    if (forced && isTraveling())
        return true;

    if ((statusTime > 0 && startTime + statusTime < WorldTimer::getMSTime()))
    {
        setStatus(TRAVEL_STATUS_EXPIRED);
        return false;
    }

    if (isTraveling())
        return true;

    if (isWorking())
        return true;
    
    if (m_status == TRAVEL_STATUS_COOLDOWN)
        return true;

    if (!tDestination->isActive(bot)) //Target has become invalid. Stop.
    {
        setStatus(TRAVEL_STATUS_COOLDOWN);
        return true;
    }

    return true;
};

bool TravelTarget::isTraveling() {
    if (m_status != TRAVEL_STATUS_TRAVEL)
        return false;

    if (!tDestination->isActive(bot) && !forced) //Target has become invalid. Stop.
    {
        setStatus(TRAVEL_STATUS_COOLDOWN);
        return false;
    }

    WorldPosition pos(bot);

    bool HasArrived = tDestination->isIn(&pos, radius);

    if (HasArrived)
    {
        setStatus(TRAVEL_STATUS_WORK);
        return false;
    }

    return true;
}

bool TravelTarget::isWorking() {
    if (m_status != TRAVEL_STATUS_WORK)
        return false;

    if (!tDestination->isActive(bot)) //Target has become invalid. Stop.
    {
        setStatus(TRAVEL_STATUS_COOLDOWN);
        return false;
    }

    WorldPosition pos(bot);

    /*
    bool HasLeft = tDestination->isOut(&pos);

    if (HasLeft)
    {
        setStatus(TRAVEL_STATUS_TRAVEL);
        return false;
    }
    */

    return true;
}

bool TravelTarget::isPreparing() {
    if (m_status != TRAVEL_STATUS_PREPARE)
        return false;

    return true;
}

TravelState TravelTarget::getTravelState() {
    if (!tDestination || tDestination->getName() == "NullTravelDestination")
        return TRAVEL_STATE_IDLE;

    if (tDestination->getName() == "QuestRelationTravelDestination")
    {
        if (((QuestRelationTravelDestination*)tDestination)->getRelation() == 0)
        {
            if (isTraveling() || isPreparing())
                return TRAVEL_STATE_TRAVEL_PICK_UP_QUEST;
            if (isWorking())
                return TRAVEL_STATE_WORK_PICK_UP_QUEST;
        }
        else
        {
            if (isTraveling() || isPreparing())
                return TRAVEL_STATE_TRAVEL_HAND_IN_QUEST;
            if (isWorking())
                return TRAVEL_STATE_WORK_HAND_IN_QUEST;
        }
    }
    else if (tDestination->getName() == "QuestObjectiveTravelDestination")
    {
        if (isTraveling() || isPreparing())
            return TRAVEL_STATE_TRAVEL_DO_QUEST;
        if (isWorking())
            return TRAVEL_STATE_WORK_DO_QUEST;
    }
    else if (tDestination->getName() == "RpgTravelDestination")
    {
        return TRAVEL_STATE_TRAVEL_RPG;
    }
    else if (tDestination->getName() == "ExploreTravelDestination")
    {
        return TRAVEL_STATE_TRAVEL_EXPLORE;
    }

    return TRAVEL_STATE_IDLE;
}

void TravelMgr::Clear()
{
#ifdef MANGOS
    sObjectAccessor.DoForAllPlayers([this](Player* plr) { TravelMgr::setNullTravelTarget(plr); });
#endif
#ifdef CMANGOS
#ifndef MANGOSBOT_ZERO
    sObjectAccessor.ExecuteOnAllPlayers([this](Player* plr) { TravelMgr::setNullTravelTarget(plr); });
#else
    HashMapHolder<Player>::ReadGuard g(HashMapHolder<Player>::GetLock());
    HashMapHolder<Player>::MapType& m = sObjectAccessor.GetPlayers();
    for (HashMapHolder<Player>::MapType::iterator itr = m.begin(); itr != m.end(); ++itr)
        TravelMgr::setNullTravelTarget(itr->second);
#endif
#endif

    for (auto& quest : quests)
    {
        for (auto& dest : quest.second->questGivers)
        {
            delete dest;
        }

        for (auto& dest : quest.second->questTakers)
        {
            delete dest;
        }

        for (auto& dest : quest.second->questObjectives)
        {
            delete dest;
        }
    }

    questGivers.clear();
    quests.clear();
    pointsMap.clear();
}

void TravelMgr::logQuestError(uint32 errorNr, Quest* quest, uint32 objective, uint32 unitId, uint32 itemId)
{
    bool logQuestErrors = false; //For debugging.

    if (!logQuestErrors)
        return;

    if (errorNr == 1)
    {
        string unitName = "<unknown>";
        CreatureInfo const* cInfo = NULL;
        GameObjectInfo const* gInfo = NULL;

        if (unitId > 0)
            cInfo = ObjectMgr::GetCreatureTemplate(unitId);
        else
            gInfo = ObjectMgr::GetGameObjectInfo(unitId * -1);

        if (cInfo)
            unitName = cInfo->Name;
        else if (gInfo)
            unitName = gInfo->name;

        sLog.outString("Quest %s [%d] has %s %s [%d] but none is found in the world.", quest->GetTitle().c_str(), quest->GetQuestId(), objective == 0 ? "quest giver" : "quest taker", unitName.c_str(), unitId);
    }
    else if (errorNr == 2)
    {
        string unitName = "<unknown>";
        CreatureInfo const* cInfo = NULL;
        GameObjectInfo const* gInfo = NULL;

        if (unitId > 0)
            cInfo = ObjectMgr::GetCreatureTemplate(unitId);
        else
            gInfo = ObjectMgr::GetGameObjectInfo(unitId * -1);

        if (cInfo)
            unitName = cInfo->Name;
        else if (gInfo)
            unitName = gInfo->name;

        sLog.outErrorDb("Quest %s [%d] needs %s [%d] for objective %d but none is found in the world.", quest->GetTitle().c_str(), quest->GetQuestId(), unitName.c_str(), unitId, objective);
    }
    else if (errorNr == 3)
    {
        sLog.outErrorDb("Quest %s [%d] needs itemId %d but no such item exists.", quest->GetTitle().c_str(), quest->GetQuestId(), itemId);
    }
    else if (errorNr == 4)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);

        string unitName = "<unknown>";
        CreatureInfo const* cInfo = NULL;
        GameObjectInfo const* gInfo = NULL;

        if (unitId > 0)
            cInfo = ObjectMgr::GetCreatureTemplate(unitId);
        else
            gInfo = ObjectMgr::GetGameObjectInfo(unitId * -1);

        if (cInfo)
            unitName = cInfo->Name;
        else if (gInfo)
            unitName = gInfo->name;

        sLog.outString("Quest %s [%d] needs %s [%d] for loot of item %s [%d] for objective %d but none is found in the world.", quest->GetTitle().c_str(), quest->GetQuestId(), unitName.c_str(), unitId, proto->Name1, itemId, objective);
    }
    else if (errorNr == 5)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);

        sLog.outString("Quest %s [%d] needs item %s [%d] for objective %d but none is found in the world.", quest->GetTitle().c_str(), quest->GetQuestId(), proto->Name1, itemId, objective);
    }
    else if (errorNr == 6)
    {
        sLog.outErrorDb("Quest %s [%d] has no quest giver.", quest->GetTitle().c_str(), quest->GetQuestId());
    }
    else if (errorNr == 7)
    {
        sLog.outErrorDb("Quest %s [%d] has no quest taker.", quest->GetTitle().c_str(), quest->GetQuestId());
    }
    else if (errorNr == 8)
    {
        sLog.outErrorDb("Quest %s [%d] has no quest viable quest objective.", quest->GetTitle().c_str(), quest->GetQuestId());
    }
}

void TravelMgr::LoadQuestTravelTable()
{
    if (!sTravelMgr.quests.empty())
        return;
    // Clearing store (for reloading case)
    Clear();

    struct unit { uint64 guidx; uint32 guid; uint32 type; uint32 entry; uint32 map; float  x; float  y; float  z;  float  o; uint32 c; } t_unit;
    vector<unit> units;

    ObjectMgr::QuestMap const& questMap = sObjectMgr.GetQuestTemplates();
    vector<uint32> questIds;

    unordered_map <uint32, uint32> entryCount;

    for (auto& quest : questMap)
        questIds.push_back(quest.first);

    sort(questIds.begin(), questIds.end());

    sLog.outErrorDb("Loading units locations.");
    for (auto& creaturePair : WorldPosition().getCreaturesNear())
    {
        t_unit.type = 0;
        t_unit.guidx = ObjectGuid(HIGHGUID_UNIT, creaturePair->second.id, creaturePair->first).GetRawValue();
        t_unit.guid = creaturePair->first;
        t_unit.entry = creaturePair->second.id;
        t_unit.map = creaturePair->second.mapid;
        t_unit.x = creaturePair->second.posX;
        t_unit.y = creaturePair->second.posY;
        t_unit.z = creaturePair->second.posZ;
        t_unit.o = creaturePair->second.orientation;

        entryCount[creaturePair->second.id]++;

        units.push_back(t_unit);
    }

    for (auto& unit : units)
    {
        unit.c = entryCount[unit.entry];
    }

    sLog.outErrorDb("Loading game object locations.");
    for (auto& goPair : WorldPosition().getGameObjectsNear())
    {
        t_unit.type = 1;
        t_unit.guidx = ObjectGuid(HIGHGUID_GAMEOBJECT, goPair->second.id, goPair->first).GetRawValue();
        t_unit.entry = goPair->second.id;
        t_unit.map = goPair->second.mapid;
        t_unit.x = goPair->second.posX;
        t_unit.y = goPair->second.posY;
        t_unit.z = goPair->second.posZ;
        t_unit.o = goPair->second.orientation;
        t_unit.c = 1;

        units.push_back(t_unit);
    }  

    sLog.outErrorDb("Loading quest data.");

    bool loadQuestData = false;

    if (loadQuestData)
    {
        questGuidpMap questMap = GAI_VALUE(questGuidpMap, "quest guidp map");

        for (auto& q : questMap)
        {
            uint32 questId = q.first;

            QuestContainer* container = new QuestContainer;

            for (auto& r : q.second)
            {
                uint32 flag = r.first;

                for (auto& e : r.second)
                {
                    int32 entry = e.first;

                    QuestTravelDestination* loc;
                    vector<QuestTravelDestination*> locs;

                    if (flag & (uint32)QuestRelationFlag::questGiver)
                    {
                        loc = new QuestRelationTravelDestination(questId, entry, 0, sPlayerbotAIConfig.tooCloseDistance, sPlayerbotAIConfig.sightDistance);
                        loc->setExpireDelay(5 * 60 * 1000);
                        loc->setMaxVisitors(15, 0);
                        container->questGivers.push_back(loc);
                        locs.push_back(loc);
                    }
                    if (flag & (uint32)QuestRelationFlag::questTaker)
                    {
                        loc = new QuestRelationTravelDestination(questId, entry, 1, sPlayerbotAIConfig.tooCloseDistance, sPlayerbotAIConfig.sightDistance);
                        loc->setExpireDelay(5 * 60 * 1000);
                        loc->setMaxVisitors(15, 0);
                        container->questTakers.push_back(loc);
                        locs.push_back(loc);
                    }
                    else
                    {
                        uint32 objective = 0;
                        if (flag & (uint32)QuestRelationFlag::objective1)
                            objective = 0;
                        else if (flag & (uint32)QuestRelationFlag::objective2)
                            objective = 1;
                        else if (flag & (uint32)QuestRelationFlag::objective3)
                            objective = 2;
                        else if (flag & (uint32)QuestRelationFlag::objective4)
                            objective = 3;

                        loc = new QuestObjectiveTravelDestination(questId, entry, objective, sPlayerbotAIConfig.tooCloseDistance, sPlayerbotAIConfig.sightDistance);
                        loc->setExpireDelay(1 * 60 * 1000);
                        loc->setMaxVisitors(100, 1);
                        container->questObjectives.push_back(loc);
                        locs.push_back(loc);
                    }

                    for (auto& guidP : e.second)
                    {
                        WorldPosition point = guidP.getPosition();
                        pointsMap.insert(make_pair(guidP.GetRawValue(), point));

                        for (auto tLoc : locs)
                        {
                            tLoc->addPoint(&pointsMap.find(guidP.GetRawValue())->second);
                        }
                    }
                }
            }

            if (!container->questTakers.empty())
            {
                quests.insert(make_pair(questId, container));

                for (auto loc : container->questGivers)
                    questGivers.push_back(loc);
            }
        }
    }  

    sLog.outErrorDb("Loading Rpg, Grind and Boss locations.");

    WorldPosition point;

    vector<uint32> allowedNpcFlags;

    allowedNpcFlags.push_back(UNIT_NPC_FLAG_INNKEEPER);
    allowedNpcFlags.push_back(UNIT_NPC_FLAG_VENDOR);
    allowedNpcFlags.push_back(UNIT_NPC_FLAG_REPAIR);

    //Rpg locations
    for (auto& u : units)
    {
        RpgTravelDestination* rLoc;
        GrindTravelDestination* gLoc;
        BossTravelDestination* bLoc;

        if (u.type != 0)
            continue;

        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(u.entry);

        if (!cInfo)
            continue;

        point = WorldPosition(u.map, u.x, u.y, u.z, u.o);         

        if (!point.getMapEntry()->IsContinent())
            continue;

        for (vector<uint32>::iterator i = allowedNpcFlags.begin(); i != allowedNpcFlags.end(); ++i)
        {
            if ((cInfo->NpcFlags & *i) != 0)
            {
                rLoc = new RpgTravelDestination(u.entry, sPlayerbotAIConfig.tooCloseDistance, sPlayerbotAIConfig.sightDistance);
                rLoc->setExpireDelay(60 * 1000);
                rLoc->setMaxVisitors(15, 0);
                rLoc->guid = ObjectGuid(u.guidx);

                pointsMap.insert_or_assign(u.guidx, point);
                rLoc->addPoint(&pointsMap.find(u.guidx)->second);
                rpgNpcs.push_back(rLoc);
                break;
            }
        }

        gLoc = new GrindTravelDestination(u.entry, sPlayerbotAIConfig.tooCloseDistance, sPlayerbotAIConfig.sightDistance);



        gLoc->setExpireDelay(60 * 1000);
        gLoc->setMaxVisitors(100, 0);

        point = WorldPosition(u.map, u.x, u.y, u.z, u.o);

        if (point.isUnderWater() || point.isInWater())
            continue;

        if (point.getMapEntry()->IsDungeon())
            continue;

        pointsMap.insert_or_assign(u.guidx, point);
        gLoc->addPoint(&pointsMap.find(u.guidx)->second);
        grindMobs.push_back(gLoc);        

        if (cInfo->Rank == 3 || (cInfo->Rank == 1 && !point.isOverworld() && u.c == 1))
        {
            string nodeName = cInfo->Name;

            bLoc = new BossTravelDestination(u.entry, sPlayerbotAIConfig.tooCloseDistance, sPlayerbotAIConfig.sightDistance);
            bLoc->setExpireDelay(60 * 1000);
            bLoc->setMaxVisitors(0, 0);

            pointsMap.insert_or_assign(u.guidx, point);
            bLoc->addPoint(&pointsMap.find(u.guidx)->second);
            bossMobs.push_back(bLoc);
        }
    }

    sLog.outErrorDb("Loading Explore locations.");

    //Explore points
    for (auto& u : units)
    {
        ExploreTravelDestination* loc;

        WorldPosition point = WorldPosition(u.map, u.x, u.y, u.z, u.o);
        AreaTableEntry const* area = point.getArea();

        if (!area)
            continue;

        if (!area->exploreFlag)
            continue;

        if (u.type == 1) 
            continue;

        auto iloc = exploreLocs.find(area->ID);

        int32 guid = u.type == 0 ? u.guidx : u.guidx * -1;

        pointsMap.insert_or_assign(guid, point);

        if (iloc == exploreLocs.end())
        {
            loc = new ExploreTravelDestination(area->ID, sPlayerbotAIConfig.tooCloseDistance, sPlayerbotAIConfig.sightDistance);
            loc->setMaxVisitors(1000, 0);
            loc->setCooldownDelay(1000);
            loc->setExpireDelay(1000);
            exploreLocs.insert_or_assign(area->ID, loc);
        }
        else
        {
            loc = iloc->second;
        }

        loc->addPoint(&pointsMap.find(guid)->second);
    }     


#ifdef IKE_PATHFINDER
    bool mmapAvoidMobMod = true;

    if (mmapAvoidMobMod)
    {
        sLog.outString("Loading mob avoidance maps");

        //Mob avoidance
        PathFinder path;
        WorldPosition emptyPoint;
        FactionTemplateEntry const* humanFaction = sFactionTemplateStore.LookupEntry(1);
        FactionTemplateEntry const* orcFaction = sFactionTemplateStore.LookupEntry(2);        

        for (auto& creaturePair : emptyPoint.getCreaturesNear())
        {
            CreatureData const cData = creaturePair->second;
            CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(cData.id);

            if (!cInfo)
                continue;

            WorldPosition point = WorldPosition(cData.mapid, cData.posX, cData.posY, cData.posZ, cData.orientation);

            if (cInfo->NpcFlags > 0)
                continue;

            FactionTemplateEntry const* factionEntry = sFactionTemplateStore.LookupEntry(cInfo->Faction);
            ReputationRank reactionHum = PlayerbotAI::GetFactionReaction(humanFaction, factionEntry);
            ReputationRank reactionOrc = PlayerbotAI::GetFactionReaction(orcFaction, factionEntry);

            if (reactionHum >= REP_NEUTRAL || reactionOrc >= REP_NEUTRAL)
                continue;

            if (!point.getTerrain())
                continue;

            point.loadMapAndVMap();

            path.setArea(point.getMapId(), point.getX(), point.getY(), point.getZ(), 11, 50.0f);
            path.setArea(point.getMapId(), point.getX(), point.getY(), point.getZ(), 12, 20.0f);
        }
    }
#endif

    //unittest
    if(false)
    {
        //arrange
        TravelNode* centernode = sTravelNodeMap.addNode(WorldPosition(0, 0, 0), "center", true);
        TravelNode* nearnode = sTravelNodeMap.addNode(WorldPosition(0, 10, 0), "near", true);
        TravelNode* farnode = sTravelNodeMap.addNode(WorldPosition(0, 100, 0), "far", true);

        centernode->setPathTo(nearnode, TravelNodePath(10));
        nearnode->setPathTo(centernode, TravelNodePath(10));
        
        centernode->setPathTo(farnode, TravelNodePath(100));
        farnode->setPathTo(centernode,  TravelNodePath(100));
        
        nearnode->setPathTo(farnode, TravelNodePath(90));
        farnode->setPathTo(nearnode, TravelNodePath(90));

        //act
        sTravelNodeMap.removeUselessPaths();

        //assert
        MANGOS_ASSERT(centernode->hasPathTo(farnode));

        //teardown
        sTravelNodeMap.removeNode(centernode);
        sTravelNodeMap.removeNode(nearnode);
        sTravelNodeMap.removeNode(farnode);
    }

    //unittest
    if (false)
    {
        //arrange
        TravelNode* centernode = sTravelNodeMap.addNode(WorldPosition(0, 0, 0), "center", true);
        TravelNode* nearnode = sTravelNodeMap.addNode(WorldPosition(0, 10, 0), "near", true);
        TravelNode* subnode = sTravelNodeMap.addNode(WorldPosition(0, 20, 0), "near", true);
        TravelNode* farnode = sTravelNodeMap.addNode(WorldPosition(0, 100, 0), "far", true);

        centernode->setPathTo(nearnode, TravelNodePath(10));
        nearnode->setPathTo(centernode, TravelNodePath(10));

        centernode->setPathTo(farnode, TravelNodePath(100));
        farnode->setPathTo(centernode, TravelNodePath(100));

        nearnode->setPathTo(subnode, TravelNodePath(10));
        subnode->setPathTo(nearnode, TravelNodePath(10));

        subnode->setPathTo(farnode, TravelNodePath(80));
        farnode->setPathTo(subnode, TravelNodePath(80));

        //act
        sTravelNodeMap.removeUselessPaths();

        //assert
        MANGOS_ASSERT(centernode->hasPathTo(farnode));

        //teardown
        sTravelNodeMap.removeNode(centernode);
        sTravelNodeMap.removeNode(nearnode);
        sTravelNodeMap.removeNode(subnode);
        sTravelNodeMap.removeNode(farnode);
    }

    //Clear these logs files
    sPlayerbotAIConfig.openLog("zones.csv", "w");
    sPlayerbotAIConfig.openLog("creatures.csv", "w");
    sPlayerbotAIConfig.openLog("gos.csv", "w");
    sPlayerbotAIConfig.openLog("bot_movement.csv", "w");
    sPlayerbotAIConfig.openLog("bot_pathfinding.csv", "w");
    sPlayerbotAIConfig.openLog("pathfind_attempt.csv", "w");
    sPlayerbotAIConfig.openLog("pathfind_attempt_point.csv", "w");
    sPlayerbotAIConfig.openLog("pathfind_result.csv", "w");
    sPlayerbotAIConfig.openLog("load_map_grid.csv", "w");
    sPlayerbotAIConfig.openLog("strategy.csv", "w");

    sPlayerbotAIConfig.openLog("unload_grid.csv", "w");
    sPlayerbotAIConfig.openLog("unload_obj.csv", "w");

    sTravelNodeMap.loadNodeStore();

    sTravelNodeMap.generateAll();


    /*
    bool fullNavPointReload = false;
    bool storeNavPointReload = true;

    if(!fullNavPointReload && true)
        TravelNodeStore::loadNodes();

    //sTravelNodeMap.loadNodeStore();

    for (auto node : sTravelNodeMap.getNodes())
    {
        node->setLinked(true);
    }

    bool reloadNavigationPoints = false || fullNavPointReload || storeNavPointReload;

    if (reloadNavigationPoints)
    {
        sLog.outString("Loading navigation points");

        //Npc nodes

        WorldPosition pos;

        for (auto& u : units)
        {
            if (u.type != 0)
                continue;

            CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(u.entry);

            if (!cInfo)
                continue;

            vector<uint32> allowedNpcFlags;

            allowedNpcFlags.push_back(UNIT_NPC_FLAG_INNKEEPER);
            allowedNpcFlags.push_back(UNIT_NPC_FLAG_FLIGHTMASTER);
            //allowedNpcFlags.push_back(UNIT_NPC_FLAG_QUESTGIVER);

            for (vector<uint32>::iterator i = allowedNpcFlags.begin(); i != allowedNpcFlags.end(); ++i)
            {
                if ((cInfo->NpcFlags & *i) != 0)
                {

                    pos = WorldPosition(u.map, u.x, u.y, u.z, u.o);

                    string nodeName = pos.getAreaName(false);
                    if ((cInfo->NpcFlags & UNIT_NPC_FLAG_INNKEEPER) != 0)
                        nodeName += " innkeeper";
                    else
                        nodeName += " flightMaster";

                    TravelNode* node = sTravelNodeMap.addNode(&pos, nodeName, true, true);

                    break;
                }
            }
        }

        //Build flight paths

        for (uint32 i = 0; i < sTaxiPathStore.GetNumRows(); ++i)
        {
            TaxiPathEntry const* taxiPath = sTaxiPathStore.LookupEntry(i);

            if (!taxiPath)
                continue;

            TaxiNodesEntry const* startTaxiNode = sTaxiNodesStore.LookupEntry(taxiPath->from);

            if (!startTaxiNode)
                continue;

            TaxiNodesEntry const* endTaxiNode = sTaxiNodesStore.LookupEntry(taxiPath->to);

            if (!endTaxiNode)
                continue;

            TaxiPathNodeList const& nodes = sTaxiPathNodesByPath[taxiPath->ID];

            if (nodes.empty())
                continue;

            WorldPosition startPos(startTaxiNode->map_id, startTaxiNode->x, startTaxiNode->y, startTaxiNode->z);
            WorldPosition endPos(endTaxiNode->map_id, endTaxiNode->x, endTaxiNode->y, endTaxiNode->z);

            TravelNode* startNode = sTravelNodeMap.getNode(&startPos, nullptr, 15.0f);
            TravelNode* endNode = sTravelNodeMap.getNode(&endPos, nullptr, 15.0f);

            if (!startNode || !endNode)
                continue;

            vector<WorldPosition> ppath;

            for (auto& n : nodes)
                ppath.push_back(WorldPosition(n->mapid, n->x, n->y, n->z, 0.0));

            float totalTime = startPos.getPathLength(ppath) / (450 * 8.0f);

            TravelNodePath travelPath(0.1f, totalTime, (uint8)TravelNodePathType::flightPath, i, true);
            travelPath.setPath(ppath);

            startNode->setPathTo(endNode, travelPath);
        }
    
        //Unique bosses
        for (auto& u : units)
        {
            if (u.type != 0)
                continue;

            CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(u.entry);

            if (!cInfo)
                continue;

            pos = WorldPosition(u.map, u.x, u.y, u.z, u.o);

            if (cInfo->Rank == 3 || (cInfo->Rank == 1 && !pos.isOverworld() && u.c == 1))
            {
                string nodeName = cInfo->Name;

                sTravelNodeMap.addNode(&pos, nodeName, true, true);
            }
        }

        map<uint8, string> startNames;
        startNames[RACE_HUMAN] = "Human";
        startNames[RACE_ORC] = "Orc and Troll";
        startNames[RACE_DWARF] = "Dwarf and Gnome";
        startNames[RACE_NIGHTELF] = "Night Elf";
        startNames[RACE_UNDEAD] = "Undead";
        startNames[RACE_TAUREN] = "Tauren";
        startNames[RACE_GNOME] = "Dwarf and Gnome";
        startNames[RACE_TROLL] = "Orc and Troll";
        startNames[RACE_GOBLIN] = "Goblin";

        for (uint32 i = 0; i < MAX_RACES; i++)
        {
            for (uint32 j = 0; j < MAX_CLASSES; j++)
            {
                PlayerInfo const* info = sObjectMgr.GetPlayerInfo(i, j);

                if (!info)
                    continue;

                pos = WorldPosition(info->mapId, info->positionX, info->positionY, info->positionZ, info->orientation);

                string nodeName = startNames[i] + " start";

                sTravelNodeMap.addNode(&pos, nodeName, true, true);
            }
        }

        //Entrance nodes

        for (int i = 0; i < 6000; i++)
        {
            AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(i);
            if (!atEntry)
                continue;

            AreaTrigger const* at = sObjectMgr.GetAreaTrigger(i);
            if (!at)
                continue;

            WorldPosition inPos = WorldPosition(atEntry->mapid, atEntry->x, atEntry->y, atEntry->z - 4.0f, 0);

            WorldPosition outPos = WorldPosition(at->target_mapId, at->target_X, at->target_Y, at->target_Z, at->target_Orientation);

            string nodeName;

            if (!outPos.isOverworld())
                nodeName = outPos.getAreaName(false) + " entrance";
            else if (!inPos.isOverworld())
                nodeName = inPos.getAreaName(false) + " exit";
            else
                nodeName = inPos.getAreaName(false) + " portal";

            sTravelNodeMap.addNode(&inPos, nodeName, true, true);
        }

        //Exit nodes

        for (int i = 0; i < 6000; i++)
        {
            AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(i);
            if (!atEntry)
                continue;

            AreaTrigger const* at = sObjectMgr.GetAreaTrigger(i);
            if (!at)
                continue;

            WorldPosition inPos = WorldPosition(atEntry->mapid, atEntry->x, atEntry->y, atEntry->z - 4.0f, 0);

            WorldPosition outPos = WorldPosition(at->target_mapId, at->target_X, at->target_Y, at->target_Z, at->target_Orientation);

            string nodeName;

            if (!outPos.isOverworld())
                nodeName = outPos.getAreaName(false) + " entrance";
            else if (!inPos.isOverworld())
                nodeName = inPos.getAreaName(false) + " exit";
            else
                nodeName = inPos.getAreaName(false) + " portal";

            TravelNode* entryNode = sTravelNodeMap.getNode(&outPos, NULL, 20.0f); //Entry side, portal exit.

            TravelNode* outNode = sTravelNodeMap.addNode(&outPos, nodeName, true, true); //Exit size, portal exit.

            TravelNode* inNode = sTravelNodeMap.getNode(&inPos, NULL, 5.0f); //Entry side, portal center.

            //Portal link from area trigger to area trigger destination.
            if (outNode && inNode)
            {                
                TravelNodePath travelPath(0.1f, 3.0f, (uint8)TravelNodePathType::portal, i, true);
                travelPath.setPath({ *inNode->getPosition(), *outNode->getPosition() });
                inNode->setPathTo(outNode, travelPath);
            }

        }

        //Transports
        for (uint32 entry = 1; entry <= sGOStorage.GetMaxEntry(); ++entry)
        {
            auto data = sGOStorage.LookupEntry<GameObjectInfo>(entry);
            if (data && (data->type == GAMEOBJECT_TYPE_TRANSPORT || data->type == GAMEOBJECT_TYPE_MO_TRANSPORT))
            {
                TransportAnimation const* animation = sTransportMgr.GetTransportAnimInfo(entry);

                uint32 pathId = data->moTransport.taxiPathId;
                float moveSpeed = data->moTransport.moveSpeed;
                if (pathId >= sTaxiPathNodesByPath.size())
                    continue;

                TaxiPathNodeList const& path = sTaxiPathNodesByPath[pathId];

                vector<WorldPosition> ppath;
                TravelNode* prevNode = nullptr;

                //Elevators/Trams
                if (path.empty())
                {
                    if (animation)
                    {
                        TransportPathContainer aPath = animation->Path;
                        float timeStart;

                        for (auto& u : units)
                        {
                            if (u.type != 1)
                                continue;

                            if (u.entry != entry)
                                continue;

                            prevNode = nullptr;
                            WorldPosition lPos = WorldPosition(u.map, 0, 0, 0, 0);

                            for (auto& p : aPath)
                            {
#ifndef MANGOSBOT_TWO
                                float dx = cos(u.o) * p.second->X - sin(u.o) * p.second->Y;
                                float dy = sin(u.o) * p.second->X + cos(u.o) * p.second->Y;
#else
                                float dx = -1 * p.second->X;
                                float dy = -1 * p.second->Y;
#endif
                                WorldPosition pos = WorldPosition(u.map, u.x + dx, u.y + dy, u.z + p.second->Z, u.o);

                                if (prevNode)
                                {
                                    ppath.push_back(pos);
                                }

                                if (pos.distance(&lPos) == 0)
                                {
                                    TravelNode* node = sTravelNodeMap.addNode(&pos, data->name, true, true, true, entry);
                        
                                    if (!prevNode)
                                    {
                                        ppath.push_back(pos);
                                        timeStart = p.second->TimeSeg;
                                    }
                                    else
                                    {
                                        float totalTime = (p.second->TimeSeg - timeStart) / 1000.0f;

                                        TravelNodePath travelPath(0.1f, totalTime, (uint8)TravelNodePathType::transport, entry, true);
                                        node->setPathTo(prevNode, travelPath);
                                        ppath.clear();
                                        ppath.push_back(pos);
                                        timeStart = p.second->TimeSeg;
                                    }

                                    prevNode = node;
                                }

                                lPos = pos;
                            }

                            if (prevNode)
                            {
                                for (auto& p : aPath)
                                {
#ifndef MANGOSBOT_TWO
                                    float dx = cos(u.o) * p.second->X - sin(u.o) * p.second->Y;
                                    float dy = sin(u.o) * p.second->X + cos(u.o) * p.second->Y;
#else
                                    float dx = -1 * p.second->X;
                                    float dy = -1 * p.second->Y;
#endif
                                    WorldPosition pos = WorldPosition(u.map, u.x + dx, u.y + dy, u.z + p.second->Z, u.o);

                                    ppath.push_back(pos);

                                    if (pos.distance(&lPos) == 0)
                                    {
                                        TravelNode* node = sTravelNodeMap.addNode(&pos, data->name, true, true, true, entry);
                                        if (node != prevNode) {
                                            float totalTime = (p.second->TimeSeg - timeStart) / 1000.0f;

                                            TravelNodePath travelPath(0.1f, totalTime, (uint8)TravelNodePathType::transport, entry, true);
                                            travelPath.setPath(ppath);
                                            node->setPathTo(prevNode, travelPath);
                                            ppath.clear();
                                            ppath.push_back(pos);
                                            timeStart = p.second->TimeSeg;
                                        }
                                    }

                                    lPos = pos;
                                }
                            }

                            ppath.clear();
                        }
                    }
                }
                else //Boats/Zepelins
                {
                    //Loop over the path and connect stop locations.
                    for (auto& p : path)
                    {
                        WorldPosition pos = WorldPosition(p->mapid, p->x, p->y, p->z, 0);

                        //if (data->displayId == 3015) 
                        //    pos.setZ(pos.getZ() + 6.0f);
                        //else if(data->displayId == 3031)
                       //     pos.setZ(pos.getZ() - 17.0f);

                        if (prevNode)
                        {
                            ppath.push_back(pos);
                        }
                        
                        if (p->delay > 0)
                        {
                            TravelNode* node = sTravelNodeMap.addNode(&pos, data->name, true, true, true, entry);

                            if (!prevNode)
                            {
                                ppath.push_back(pos);
                            }
                            else
                            {
                                TravelNodePath travelPath(0.1f, 0.0, (uint8)TravelNodePathType::transport, entry, true);
                                travelPath.setPathAndCost(ppath, moveSpeed);
                                node->setPathTo(prevNode, travelPath);
                                ppath.clear();
                                ppath.push_back(pos);
                            }

                            prevNode = node;
                        }
                    }

                    if (prevNode)
                    {
                        //Continue from start until first stop and connect to end.
                        for (auto& p : path)
                        {
                            WorldPosition pos = WorldPosition(p->mapid, p->x, p->y, p->z, 0);

                            //if (data->displayId == 3015)
                            //    pos.setZ(pos.getZ() + 6.0f);
                            //else if (data->displayId == 3031)
                            //    pos.setZ(pos.getZ() - 17.0f);

                            ppath.push_back(pos);

                            if (p->delay > 0)
                            {
                                TravelNode* node = sTravelNodeMap.getNode(&pos, NULL, 5.0f);

                                if (node != prevNode) {
                                    TravelNodePath travelPath(0.1f, 0.0, (uint8)TravelNodePathType::transport, entry, true);
                                    travelPath.setPathAndCost(ppath, moveSpeed);

                                    node->setPathTo(prevNode, travelPath);
                                }
                            }
                        }
                    }

                    ppath.clear();
                }
            }
        }

        BarGoLink bar(exploreLocs.size());

        //Zone means   
        for (auto& loc : exploreLocs)
        {
            bar.step();
            vector<WorldPosition*> points;

            for (auto p : loc.second->getPoints(true))
                if (!p->isUnderWater())
                    points.push_back(p);

            if (points.empty())
                points = loc.second->getPoints(true);

            WorldPosition  pos = WorldPosition(points, WP_MEAN_CENTROID);

            TravelNode* node = sTravelNodeMap.addNode(&pos, pos.getAreaName(), true, true, false);
        }

        sLog.outString(">> Loaded " SIZEFMTD " navigation points.", sTravelNodeMap.getNodes().size());
    }

    TravelNodeStore::loadUserNodes();

    sTravelNodeMap.calcMapOffset();
    loadMapTransfers();
    */

    
    /*
    bool preloadNodePaths = false || fullNavPointReload || storeNavPointReload;             //Calculate paths using pathfinder.
    bool preloadReLinkFullyLinked = false || fullNavPointReload || storeNavPointReload;      //Retry nodes that are fully linked.
    bool preloadUnlinkedPaths = false || fullNavPointReload;        //Try to connect points currently unlinked.
    bool preloadWorldPaths = true;            //Try to load paths in overworld.
    bool preloadInstancePaths = true;         //Try to load paths in instances.
    bool preloadSubPrint = false;              //Print output every 2%.

    if (preloadNodePaths)
    {
        std::unordered_map<uint32, Map*> instances;       

        //Pathfinder
        BarGoLink bar(sTravelNodeMap.getNodes().size());
        vector<WorldPosition> ppath;

        uint32 cur = 0, max = sTravelNodeMap.getNodes().size();

        for (auto& startNode : sTravelNodeMap.getNodes())
        {
            if (!preloadReLinkFullyLinked && startNode->isLinked())
                continue;

            for (auto& endNode : sTravelNodeMap.getNodes())
            {
                if (startNode == endNode)
                    continue;

                if (startNode->getPosition()->isOverworld() && !preloadWorldPaths)
                    continue;

                if (!startNode->getPosition()->isOverworld() && !preloadInstancePaths)
                    continue;

                if (startNode->hasCompletePathTo(endNode))
                    continue;

                if (!preloadUnlinkedPaths && !startNode->hasLinkTo(endNode))
                    continue;

                if (startNode->getMapId() != endNode->getMapId())
                    continue;

                //if (preloadUnlinkedPaths && !startNode->hasLinkTo(endNode) && startNode->isUselessLink(endNode))
                //    continue;

                startNode->buildPath(endNode, NULL, false);

                //if (startNode->hasLinkTo(endNode) && !startNode->getPathTo(endNode)->getComplete())
                //    startNode->removeLinkTo(endNode);
            }

            startNode->setLinked(true);

            cur++;

            if (preloadSubPrint && (cur * 50) / max > ((cur - 1) * 50) / max)
            {
                sTravelNodeMap.printMap();
                sTravelNodeMap.printNodeStore();
            }

            bar.step();
        }

        if (!preloadSubPrint)
        {
            sTravelNodeMap.printNodeStore();
            sTravelNodeMap.printMap();
        }

        sLog.outString(">> Loaded paths for " SIZEFMTD " nodes.", sTravelNodeMap.getNodes().size());
    }

    bool removeLowLinkNodes = false || fullNavPointReload || storeNavPointReload;

    if (removeLowLinkNodes)
    {
        BarGoLink bar(sTravelNodeMap.getNodes().size());
        
        vector<TravelNode*> goodNodes;
        vector<TravelNode*> remNodes;
        for (auto& node : sTravelNodeMap.getNodes())
        {
            bar.step();

            if (!node->getPosition()->isOverworld())
                continue;

            if (std::find(goodNodes.begin(), goodNodes.end(), node) != goodNodes.end())
                continue;

            if (std::find(remNodes.begin(), remNodes.end(), node) != remNodes.end())
                continue;

            vector<TravelNode*> nodes = node->getNodeMap(true);

            if (nodes.size() < 5)
                remNodes.insert(remNodes.end(), nodes.begin(), nodes.end());
            else
                goodNodes.insert(goodNodes.end(), nodes.begin(), nodes.end());
        }

        for (auto& node : remNodes)
            sTravelNodeMap.removeNode(node);

        sLog.outString(">> Checked " SIZEFMTD " nodes.", sTravelNodeMap.getNodes().size());
    }
  
    bool cleanUpNodeLinks = false || fullNavPointReload || storeNavPointReload;
    bool cleanUpSubPrint = false;              //Print output every 2%.

    if (cleanUpNodeLinks)
    {
        //Routes
        BarGoLink bar(sTravelNodeMap.getNodes().size());

        uint32 cur = 0, max = sTravelNodeMap.getNodes().size();
        
        //Clean up node links
        for (auto& startNode : sTravelNodeMap.getNodes())
        {
             startNode->cropUselessLinks();            

             cur++;
             if (cleanUpSubPrint && (cur * 10) / max > ((cur - 1) * 10) / max)
             {
                 sTravelNodeMap.printMap();
                 sTravelNodeMap.printNodeStore();
             }

             bar.step();
        }

        sLog.outString(">> Cleaned paths for " SIZEFMTD " nodes.", sTravelNodeMap.getNodes().size());
    }

    bool reCalculateCost = false || fullNavPointReload || storeNavPointReload;
    bool forceReCalculate = false;

    if (reCalculateCost)
    {
        BarGoLink bar(sTravelNodeMap.getNodes().size());

        for (auto& startNode : sTravelNodeMap.getNodes())
        {
            for (auto& path : *startNode->getLinks())
            {
                TravelNodePath* nodePath = path.second;

                if (path.second->getPathType() != TravelNodePathType::walk)
                    continue;

                if (nodePath->getCalculated() && !forceReCalculate)
                    continue;

                nodePath->calculateCost();
            }

            bar.step();
        }

        sLog.outString(">> Calculated pathcost for " SIZEFMTD " nodes.", sTravelNodeMap.getNodes().size());
    }

    bool mirrorMissingPaths = true || fullNavPointReload || storeNavPointReload;

    if (mirrorMissingPaths)
    {
        BarGoLink bar(sTravelNodeMap.getNodes().size());

        for (auto& startNode : sTravelNodeMap.getNodes())
        {
            for (auto& path : *startNode->getLinks())
            {
                TravelNode* endNode = path.first;

                if (endNode->hasLinkTo(startNode))
                    continue;

                if (path.second->getPathType() != TravelNodePathType::walk)
                    continue;

                TravelNodePath nodePath = *path.second;

                vector<WorldPosition> pPath = nodePath.getPath();
                std::reverse(pPath.begin(), pPath.end());

                nodePath.setPath(pPath);

                endNode->setPathTo(startNode, nodePath, true);
            }

            bar.step();
        }

        sLog.outString(">> Reversed missing paths for " SIZEFMTD " nodes.", sTravelNodeMap.getNodes().size());
    }
    */

    sTravelNodeMap.printMap();
    sTravelNodeMap.printNodeStore();
    sTravelNodeMap.saveNodeStore();

    //Creature/gos/zone export.
    if (sPlayerbotAIConfig.hasLog("creatures.csv"))
    {
        for (auto& creaturePair : WorldPosition().getCreaturesNear())
        {
            CreatureData const cData = creaturePair->second;
            CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(cData.id);

            if (!cInfo)
                continue;

            WorldPosition point = WorldPosition(cData.mapid, cData.posX, cData.posY, cData.posZ, cData.orientation);

            string name = cInfo->Name;
            name.erase(remove(name.begin(), name.end(), ','), name.end());
            name.erase(remove(name.begin(), name.end(), '\"'), name.end());

            ostringstream out;
            out << name << ",";
            point.printWKT(out);
            out << cInfo->MaxLevel << ",";
            out << cInfo->Rank << ",";
            out << cInfo->Faction << ",";
            out << cInfo->NpcFlags << ",";
            out << point.getAreaName() << ",";
            out << std::fixed;

            sPlayerbotAIConfig.log("creatures.csv", out.str().c_str());
        }
    }

    if (sPlayerbotAIConfig.hasLog("gos.csv"))
    {
        for (auto& gameObjectPair : WorldPosition().getGameObjectsNear())
        {
            GameObjectData const gData = gameObjectPair->second;
            auto data = sGOStorage.LookupEntry<GameObjectInfo>(gData.id);

            if (!data)
                continue;

            WorldPosition point = WorldPosition(gData.mapid, gData.posX, gData.posY, gData.posZ, gData.orientation);

            string name = data->name;
            name.erase(remove(name.begin(), name.end(), ','), name.end());
            name.erase(remove(name.begin(), name.end(), '\"'), name.end());

            ostringstream out;
            out << name << ",";
            point.printWKT(out);
            out << data->type << ",";
            out << point.getAreaName() << ",";
            out << std::fixed;

            sPlayerbotAIConfig.log("gos.csv", out.str().c_str());
        }
    }

    if (sPlayerbotAIConfig.hasLog("zones.csv"))
    {
        std::unordered_map<string, vector<WorldPosition>> zoneLocs;

        vector<WorldPosition> Locs = {};
        
        for (auto& u : units)
        {
            WorldPosition point = WorldPosition(u.map, u.x, u.y, u.z, u.o);
            string name = to_string(u.map) + point.getAreaName();

            if (zoneLocs.find(name) == zoneLocs.end())
                zoneLocs.insert_or_assign(name, Locs);

            zoneLocs.find(name)->second.push_back(point);            
        }        

        for (auto& loc : zoneLocs)
        {
            if (loc.second.empty())
                continue;

            if (!sTravelNodeMap.getMapOffset(loc.second.front().getMapId()) && loc.second.front().getMapId() != 0)
                continue;

            vector<WorldPosition> points = loc.second;;
           
            ostringstream out; 

            WorldPosition pos = WorldPosition(points, WP_MEAN_CENTROID);

            out << "\"center\"" << ",";
            out << points.begin()->getMapId() << ",";
            out << points.begin()->getAreaName() << ",";
            out << points.begin()->getAreaName(true, true) << ",";

            pos.printWKT(out);

            out << "\n";
            
            out << "\"area\"" << ",";
            out << points.begin()->getMapId() << ",";
            out << points.begin()->getAreaName() << ",";
            out << points.begin()->getAreaName(true, true) << ",";

            point.printWKT(points, out, 0);

            sPlayerbotAIConfig.log("zones.csv", out.str().c_str());
        }
    }


    bool printStrategyMap = false;

    if (printStrategyMap && sPlayerbotAIConfig.hasLog("strategy.csv"))
    {
        static map<uint8, string> classes;
        static map<uint8, map<uint8, string> > specs;
        classes[CLASS_DRUID] = "druid";
        specs[CLASS_DRUID][0] = "balance";
        specs[CLASS_DRUID][1] = "feral combat";
        specs[CLASS_DRUID][2] = "restoration";

        classes[CLASS_HUNTER] = "hunter";
        specs[CLASS_HUNTER][0] = "beast mastery";
        specs[CLASS_HUNTER][1] = "marksmanship";
        specs[CLASS_HUNTER][2] = "survival";

        classes[CLASS_MAGE] = "mage";
        specs[CLASS_MAGE][0] = "arcane";
        specs[CLASS_MAGE][1] = "fire";
        specs[CLASS_MAGE][2] = "frost";

        classes[CLASS_PALADIN] = "paladin";
        specs[CLASS_PALADIN][0] = "holy";
        specs[CLASS_PALADIN][1] = "protection";
        specs[CLASS_PALADIN][2] = "retribution";

        classes[CLASS_PRIEST] = "priest";
        specs[CLASS_PRIEST][0] = "discipline";
        specs[CLASS_PRIEST][1] = "holy";
        specs[CLASS_PRIEST][2] = "shadow";

        classes[CLASS_ROGUE] = "rogue";
        specs[CLASS_ROGUE][0] = "assasination";
        specs[CLASS_ROGUE][1] = "combat";
        specs[CLASS_ROGUE][2] = "subtlety";

        classes[CLASS_SHAMAN] = "shaman";
        specs[CLASS_SHAMAN][0] = "elemental";
        specs[CLASS_SHAMAN][1] = "enhancement";
        specs[CLASS_SHAMAN][2] = "restoration";

        classes[CLASS_WARLOCK] = "warlock";
        specs[CLASS_WARLOCK][0] = "affliction";
        specs[CLASS_WARLOCK][1] = "demonology";
        specs[CLASS_WARLOCK][2] = "destruction";

        classes[CLASS_WARRIOR] = "warrior";
        specs[CLASS_WARRIOR][0] = "arms";
        specs[CLASS_WARRIOR][1] = "fury";
        specs[CLASS_WARRIOR][2] = "protection";

#ifdef MANGOSBOT_TWO
        classes[CLASS_DEATH_KNIGHT] = "dk";
        specs[CLASS_DEATH_KNIGHT][0] = "blood";
        specs[CLASS_DEATH_KNIGHT][1] = "frost";
        specs[CLASS_DEATH_KNIGHT][2] = "unholy";
#endif

        //Use randombot 0.
        ostringstream cout; cout << sPlayerbotAIConfig.randomBotAccountPrefix << 0;
        string accountName = cout.str();

        QueryResult* results = LoginDatabase.PQuery("SELECT id FROM account where username = '%s'", accountName.c_str());
        if (results)
        {

            Field* fields = results->Fetch();
            uint32 accountId = fields[0].GetUInt32();

            WorldSession* session = new WorldSession(accountId, NULL, SEC_PLAYER,
#ifndef MANGOSBOT_ZERO
                2,
#endif
                0, LOCALE_enUS, "", 0, 0, false);

            session->SetNoAnticheat();

            vector <pair<pair<uint32, uint32>, uint32>> classSpecLevel;

            std::unordered_map<string, vector<pair<pair<uint32, uint32>, uint32>>> actions;

            ostringstream out;

            for (uint8 race = RACE_HUMAN; race < MAX_RACES; race++)
            {
                for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
                {
#ifdef MANGOSBOT_TWO
                    if (cls != 10)
#else
                    if (cls != 10 && cls != 6)
#endif
                    {
                        Player* player = new Player(session);

                        if (player->Create(sObjectMgr.GeneratePlayerLowGuid(), "dummy",
                            race, //race
                            cls, //class
                            1, //gender
                            1, // skinColor,
                            1,
                            1,
                            1, // hairColor,
                            1, 0))
                        {

                            for (uint8 tab = 0; tab < 3; tab++)
                            {
                                TalentSpec newSpec;
                                if (tab == 0)
                                    newSpec = TalentSpec(player, "1-0-0");
                                else if (tab == 1)
                                    newSpec = TalentSpec(player, "0-1-0");
                                else
                                    newSpec = TalentSpec(player, "0-0-1");

                                for (uint32 lvl = 1; lvl < MAX_LEVEL; lvl++)
                                {
                                    player->SetLevel(lvl);

                                    ostringstream tout;
                                    newSpec.ApplyTalents(player, &tout);

                                    PlayerbotAI* ai = new PlayerbotAI(player);

                                    ai->ResetStrategies(false);

                                    AiObjectContext* con = ai->GetAiObjectContext();

                                    list<string> tstrats;
                                    set<string> strategies, sstrats;

                                    tstrats = ai->GetStrategies(BOT_STATE_COMBAT);
                                    sstrats = con->GetSupportedStrategies();
                                    if (!sstrats.empty())
                                        strategies.insert(tstrats.begin(), tstrats.end());

                                    tstrats = ai->GetStrategies(BOT_STATE_NON_COMBAT);
                                    if (!tstrats.empty())
                                        strategies.insert(tstrats.begin(), tstrats.end());

                                    tstrats = ai->GetStrategies(BOT_STATE_DEAD);
                                    if (!tstrats.empty())
                                        strategies.insert(tstrats.begin(), tstrats.end());

                                    sstrats = con->GetSupportedStrategies();
                                    if(!sstrats.empty())
                                        strategies.insert(sstrats.begin(), sstrats.end());

                                    for (auto& stratName : strategies)
                                    {
                                        Strategy* strat = con->GetStrategy(stratName);

                                        if (strat->getDefaultActions())
                                            for (int32 i = 0; i < NextAction::size(strat->getDefaultActions()); i++)
                                            {
                                                NextAction* nextAction = strat->getDefaultActions()[i];

                                                ostringstream aout;

                                                aout << nextAction->getRelevance() << "," << nextAction->getName() << ",,S:" << stratName;

                                                if (actions.find(aout.str().c_str()) != actions.end())
                                                    classSpecLevel = actions.find(aout.str().c_str())->second;
                                                else
                                                    classSpecLevel.clear();

                                                classSpecLevel.push_back(make_pair(make_pair(cls, tab), lvl));

                                                actions.insert_or_assign(aout.str().c_str(), classSpecLevel);
                                            }

                                        std::list<TriggerNode*> triggers;
                                        strat->InitTriggers(triggers);
                                        for (auto& triggerNode : triggers)
                                        {
                                            //out << " TN:" << triggerNode->getName();

                                            Trigger* trigger = con->GetTrigger(triggerNode->getName());

                                            if (trigger)
                                            {

                                                triggerNode->setTrigger(trigger);

                                                NextAction** nextActions = triggerNode->getHandlers();

                                                for (int32 i = 0; i < NextAction::size(nextActions); i++)
                                                {
                                                    NextAction* nextAction = nextActions[i];
                                                    //out << " A:" << nextAction->getName() << "(" << nextAction->getRelevance() << ")";

                                                    ostringstream aout;

                                                    aout << nextAction->getRelevance() << "," << nextAction->getName() << "," << triggerNode->getName() << "," << stratName;

                                                    if (actions.find(aout.str().c_str()) != actions.end())
                                                        classSpecLevel = actions.find(aout.str().c_str())->second;
                                                    else
                                                        classSpecLevel.clear();

                                                    classSpecLevel.push_back(make_pair(make_pair(cls, tab), lvl));

                                                    actions.insert_or_assign(aout.str().c_str(), classSpecLevel);
                                                }
                                            }
                                        }
                                    }

                                    delete ai;
                                }
                            }                            
                        }
                        delete player;
                    }
                }
            }

            vector< string> actionKeys;

            for (auto& action : actions)
                actionKeys.push_back(action.first);

            std::sort(actionKeys.begin(), actionKeys.end(), [](string i, string j)
                {stringstream is(i); stringstream js(j); float iref, jref; string iact, jact, itrig, jtrig, istrat, jstrat;
            is >> iref >> iact >> itrig >> istrat;
            js >> jref >> jact >> jtrig >> jstrat;
            if (iref > jref)
                return true;
            if (iref == jref && istrat < jstrat)
                return true;
            if (iref == jref && !(istrat > jstrat) && iact < jact)
                return true;
            if (iref == jref && !(istrat > jstrat) && !(iact > jact) && itrig < jtrig)
                return true;
            return false;
             });

            sPlayerbotAIConfig.log("strategy.csv", "relevance, action, trigger, strategy, classes");

            for (auto& actionkey : actionKeys)
            {
                if (actions.find(actionkey)->second.size() != (MAX_LEVEL - 1) * (MAX_CLASSES - 1))
                {
                    classSpecLevel = actions.find(actionkey)->second;
                    
                    vector<pair<pair<uint32, uint32>,pair<uint32, uint32>>> classs;

                    for (auto cl : classSpecLevel)
                    {
                        uint32 minLevel = MAX_LEVEL; uint32 maxLevel = 0;

                        uint32 cls = cl.first.first;
                        uint32 tb = cl.first.second;

                        if (std::find_if(classs.begin(), classs.end(), [cls,tb](pair<pair<uint32, uint32>, pair<uint32, uint32>> i){return i.first.first ==cls && i.first.second == tb;}) == classs.end())
                        {
                            for (auto cll : classSpecLevel)
                            {
                                if (cll.first.first == cl.first.first && cll.first.second == cl.first.second)
                                {
                                    minLevel = std::min(minLevel, cll.second);
                                    maxLevel = std::max(maxLevel, cll.second);
                                }
                            }

                            classs.push_back(make_pair(cl.first, make_pair(minLevel, maxLevel)));
                        }
                    }

                    out << actionkey;

                    if (classs.size() != 9 * 3)
                    {
                        out << ",";

                        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
                        {
                            bool a[3] = { false,false,false };
                            uint32 min[3] = { 0,0,0 };
                            uint32 max[3] = { 0,0,0 };

                            if (std::find_if(classs.begin(), classs.end(), [cls](pair<pair<uint32, uint32>, pair<uint32, uint32>> i) {return i.first.first == cls; }) == classs.end())
                                continue;

                            for (uint32 tb = 0; tb < 3; tb++)
                            {
                                auto tcl = std::find_if(classs.begin(), classs.end(), [cls, tb](pair<pair<uint32, uint32>, pair<uint32, uint32>> i) {return i.first.first == cls && i.first.second == tb; });
                                if (tcl == classs.end())
                                    continue;

                                a[tb] = true;
                                min[tb] = tcl->second.first;
                                max[tb] = tcl->second.second;
                            }

                            if (a[0] && a[1] && a[2] && min[0] == min[1] == min[2] && max[0] == max[1] == max[2])
                            {
                                if (min[0] != 1 || max[0] != MAX_LEVEL - 1)
                                    out << classes[cls] << "(" << min[0] << "-" << max[0] << ")";
                                else
                                    out << classes[cls];

                                if (cls != classs.back().first.first)
                                    out << ";";
                            }
                            else
                            {
                                for (uint32 tb = 0; tb < 3; tb++)
                                {
                                    if (!a[tb])
                                        continue;

                                    if (min[tb] != 1 || max[tb] != MAX_LEVEL - 1)
                                        out << specs[cls][tb] << " " << classes[cls] << "(" << min[tb] << "-" << max[tb] << ")";
                                    else
                                        out << specs[cls][tb] << " " << classes[cls];

                                    if (cls != classs.back().first.first || tb != classs.back().first.second)
                                        out << ";";
                                }
                            }
                        }                       
                    }
                    else
                        "all";

                    out << "\n";
                }
                else
                    out << actionkey << "\n";
            }

            sPlayerbotAIConfig.log("strategy.csv", out.str().c_str());
        }
    }
    /*

    sPlayerbotAIConfig.openLog(7, "w");

    //Zone area map REMOVE!
    uint32 k = 0;
    for (auto& node : sTravelNodeMap.getNodes())
    {
        WorldPosition* pos = node->getPosition();
        //map area
        for (uint32 x = 0; x < 2000; x++)
        {
            for (uint32 y = 0; y < 2000; y++)
            {
                if (!pos->getMap())
                    continue;

                float nx = pos->getX() + (x*5)-5000.0f;
                float ny = pos->getY() + (y*5)-5000.0f;
                float nz = pos->getZ() + 100.0f;

                //pos->getMap()->GetHitPosition(nx, ny, nz + 200.0f, nx, ny, nz, -0.5f);

                if (!pos->getMap()->GetHeightInRange(nx, ny, nz, 5000.0f)) // GetHeight can fail
                    continue;

                WorldPosition  npos = WorldPosition(pos->getMapId(), nx, ny, nz, 0.0);
                uint32 area = path.getArea(npos.getMapId(), npos.getX(), npos.getY(), npos.getZ());

                ostringstream out;
                out << std::fixed << area << "," << npos.getDisplayX() << "," << npos.getDisplayY();
                sPlayerbotAIConfig.log(7, out.str().c_str());
            }
        }
        k++;

        if (k > 0)
            break;
    }

    //Explore map output (REMOVE!)

    sPlayerbotAIConfig.openLog(5, "w");
    for (auto i : exploreLocs)
    {
        for (auto j : i.second->getPoints())
        {
            ostringstream out;
            string name = i.second->getTitle();
            name.erase(remove(name.begin(), name.end(), '\"'), name.end());
            out << std::fixed << std::setprecision(2) << name.c_str() << "," << i.first << "," << j->getDisplayX() << "," << j->getDisplayY() << "," << j->getX() << "," << j->getY() << "," << j->getZ();
            sPlayerbotAIConfig.log(5, out.str().c_str());
        }
    }

    */    
}

uint32 TravelMgr::getDialogStatus(Player* pPlayer, int32 questgiver, Quest const* pQuest)
{
    uint32 dialogStatus = DIALOG_STATUS_NONE;

    QuestRelationsMapBounds rbounds;                        // QuestRelations (quest-giver)
    QuestRelationsMapBounds irbounds;                       // InvolvedRelations (quest-finisher)

    uint32 questId = pQuest->GetQuestId();

    if (questgiver > 0)
    {
        rbounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(questgiver);
        irbounds = sObjectMgr.GetCreatureQuestInvolvedRelationsMapBounds(questgiver);
    }
    else
    {
        rbounds = sObjectMgr.GetGOQuestRelationsMapBounds(questgiver * -1);
        irbounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(questgiver * -1);
    }

    // Check markings for quest-finisher
    for (QuestRelationsMap::const_iterator itr = irbounds.first; itr != irbounds.second; ++itr)
    {
        if (itr->second != questId)
            continue;

        uint32 dialogStatusNew = DIALOG_STATUS_NONE;

        if (!pQuest || !pQuest->IsActive())
        {
            continue;
        }

        QuestStatus status = pPlayer->GetQuestStatus(questId);

        if ((status == QUEST_STATUS_COMPLETE && !pPlayer->GetQuestRewardStatus(questId)) ||
            (pQuest->IsAutoComplete() && pPlayer->CanTakeQuest(pQuest, false)))
        {
            if (pQuest->IsAutoComplete() && pQuest->IsRepeatable())
            {
                dialogStatusNew = DIALOG_STATUS_REWARD_REP;
            }
            else
            {
                dialogStatusNew = DIALOG_STATUS_REWARD2;
            }
        }
        else if (status == QUEST_STATUS_INCOMPLETE)
        {
            dialogStatusNew = DIALOG_STATUS_INCOMPLETE;
        }

        if (dialogStatusNew > dialogStatus)
        {
            dialogStatus = dialogStatusNew;
        }
    }

    // check markings for quest-giver
    for (QuestRelationsMap::const_iterator itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        if (itr->second != questId)
            continue;

        uint32 dialogStatusNew = DIALOG_STATUS_NONE;

        if (!pQuest || !pQuest->IsActive())
        {
            continue;
        }

        QuestStatus status = pPlayer->GetQuestStatus(questId);

        if (status == QUEST_STATUS_NONE)                    // For all other cases the mark is handled either at some place else, or with involved-relations already
        {
            if (pPlayer->CanSeeStartQuest(pQuest))
            {
                if (pPlayer->SatisfyQuestLevel(pQuest, false))
                {
                    int32 lowLevelDiff = sWorld.getConfig(CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF);
                    if (pQuest->IsAutoComplete() || (pQuest->IsRepeatable() && pPlayer->getQuestStatusMap()[questId].m_rewarded))
                    {
                        dialogStatusNew = DIALOG_STATUS_REWARD_REP;
                    }
                    else if (lowLevelDiff < 0 || pPlayer->GetLevel() <= pPlayer->GetQuestLevelForPlayer(pQuest) + uint32(lowLevelDiff))
                    {
                        dialogStatusNew = DIALOG_STATUS_AVAILABLE;
                    }
                    else
                    {
#ifndef MANGOSBOT_TWO
                        dialogStatusNew = DIALOG_STATUS_CHAT;
#else
                        dialogStatusNew = DIALOG_STATUS_LOW_LEVEL_AVAILABLE;
#endif
                    }
                }
                else
                {
                    dialogStatusNew = DIALOG_STATUS_UNAVAILABLE;
                }
            }
        }

        if (dialogStatusNew > dialogStatus)
        {
            dialogStatus = dialogStatusNew;
        }
    }

    return dialogStatus;
}

//Selects a random WorldPosition from a list. Use a distance weighted distribution.
vector<WorldPosition*> TravelMgr::getNextPoint(WorldPosition* center, vector<WorldPosition*> points, uint32 amount) {
    vector<WorldPosition*> retVec;

    if (points.size() == 1)
    {
        retVec.push_back(points[0]);
        return retVec;
    }

    //List of weights based on distance (Gausian curve that starts at 100 and lower to 1 at 1000 distance)
    vector<uint32> weights;

    std::transform(points.begin(), points.end(), std::back_inserter(weights), [center](WorldPosition* point) { return 1 + 1000 * exp(-1 * pow(point->distance(center) / 400.0, 2)); });

    //Total sum of all those weights.
    uint32 sum = std::accumulate(weights.begin(), weights.end(), 0);

    //Pick a random number in that range.
    uint32 rnd = urand(0, sum);

    //Pick a random point based on weights.
    for (uint32 nr = 0; nr < amount; nr++)
    {
        for (unsigned i = 0; i < points.size(); ++i)
            if (rnd < weights[i] && (retVec.empty() || std::find(retVec.begin(), retVec.end(), points[i]) == retVec.end()))
            {
                retVec.push_back(points[i]);
                break;
            }
            else
                rnd -= weights[i];
    }

    if (!retVec.empty())
        return retVec;

    assert(!"No valid point found.");

    return retVec;
}

vector<WorldPosition> TravelMgr::getNextPoint(WorldPosition center, vector<WorldPosition> points, uint32 amount) {
    vector<WorldPosition> retVec;

    if (points.size() < 2)
    {
        if (points.size() == 1)
            retVec.push_back(points[0]);
        return retVec;
    }

    retVec = points;

    
    vector<uint32> weights;

    //List of weights based on distance (Gausian curve that starts at 100 and lower to 1 at 1000 distance)
    //std::transform(retVec.begin(), retVec.end(), std::back_inserter(weights), [center](WorldPosition point) { return 1 + 1000 * exp(-1 * pow(point.distance(center) / 400.0, 2)); });

    //List of weights based on distance (Twice the distance = half the weight). Caps out at 200.0000 range.
    std::transform(retVec.begin(), retVec.end(), std::back_inserter(weights), [center](WorldPosition point) { return 200000/(1+point.distance(center)); });

    std::mt19937 gen(time(0));

    weighted_shuffle(retVec.begin(), retVec.end(), weights.begin(), weights.end(), gen);

    vector<float> dists;

    //Total sum of all those weights.
    /*
    uint32 sum = std::accumulate(weights.begin(), weights.end(), 0);

    //Pick a random point based on weights.
    for (uint32 nr = 0; nr < amount; nr++)
    {
        //Pick a random number in that range.
        uint32 rnd = urand(0, sum);

        for (unsigned i = 0; i < points.size(); ++i)
            if (rnd < weights[i] && (retVec.empty() || std::find(retVec.begin(), retVec.end(), points[i]) == retVec.end()))
            {
                retVec.push_back(points[i]);
                break;
            }
            else
                rnd -= weights[i];
    }
    */

    return retVec;
}

QuestStatusData* TravelMgr::getQuestStatus(Player* bot, uint32 questId)
{
    return &bot->getQuestStatusMap()[questId];
}

bool TravelMgr::getObjectiveStatus(Player* bot, Quest const* pQuest, uint32 objective)
{
    uint32 questId = pQuest->GetQuestId();
    if (!bot->IsActiveQuest(questId))
        return false;

    if (bot->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
        return false;

    QuestStatusData* questStatus = sTravelMgr.getQuestStatus(bot, questId);

    uint32  reqCount = pQuest->ReqItemCount[objective];
    uint32  hasCount = questStatus->m_itemcount[objective];

    if (reqCount && hasCount < reqCount)
        return true;

    reqCount = pQuest->ReqCreatureOrGOCount[objective];
    hasCount = questStatus->m_creatureOrGOcount[objective];

    if (reqCount && hasCount < reqCount)
        return true;

    return false;
}

vector<TravelDestination*> TravelMgr::getQuestTravelDestinations(Player* bot, uint32 questId, bool ignoreFull, bool ignoreInactive, float maxDistance, bool ignoreObjectives)
{
    WorldPosition botLocation(bot);

    vector<TravelDestination*> retTravelLocations;

    if (questId == -1)
    {
        for (auto& dest : questGivers)
        {
            if (!ignoreInactive && !dest->isActive(bot))
                continue;

            if (dest->isFull(ignoreFull))
                continue;

            if (maxDistance > 0 && dest->distanceTo(&botLocation) > maxDistance)
                continue;

            retTravelLocations.push_back(dest);
        }
    }
    else
    {
        auto i = quests.find(questId);

        if (i != quests.end())
        {
            for (auto& dest : i->second->questTakers)
            {
                if (!ignoreInactive && !dest->isActive(bot))
                    continue;

                if (dest->isFull(ignoreFull))
                    continue;

                if (maxDistance > 0 && dest->distanceTo(&botLocation) > maxDistance)
                    continue;

                retTravelLocations.push_back(dest);
            }

            if (!ignoreObjectives)
                for (auto& dest : i->second->questObjectives)
                {
                    if (!ignoreInactive && !dest->isActive(bot))
                        continue;

                    if (dest->isFull(ignoreFull))
                        continue;

                    if (maxDistance > 0 && dest->distanceTo(&botLocation) > maxDistance)
                        continue;

                    retTravelLocations.push_back(dest);
                }
        }
    }

    return retTravelLocations;
}

vector<TravelDestination*> TravelMgr::getRpgTravelDestinations(Player* bot, bool ignoreFull, bool ignoreInactive)
{
    WorldPosition botLocation(bot);

    vector<TravelDestination*> retTravelLocations;

    for (auto& dest : rpgNpcs)
    {
        if (!ignoreInactive && !dest->isActive(bot))
            continue;
        
        if (dest->isFull(ignoreFull))
            continue;

        Unit* u = bot->GetPlayerbotAI()->GetUnit(dest->guid);

        if (u && u->IsInWorld())
        {
            retTravelLocations.push_back(dest);            
        }
    }

    return retTravelLocations;
}

vector<TravelDestination*> TravelMgr::getExploreTravelDestinations(Player* bot, bool ignoreFull, bool ignoreInactive)
{
    WorldPosition botLocation(bot);

    vector<TravelDestination*> retTravelLocations;

    for (auto& dest : exploreLocs)
    {
        if (!ignoreInactive && !dest.second->isActive(bot))
            continue;

        if (dest.second->isFull(ignoreFull))
            continue;

        retTravelLocations.push_back(dest.second);
    }

    return retTravelLocations;
}

vector<TravelDestination*> TravelMgr::getGrindTravelDestinations(Player* bot, bool ignoreFull, bool ignoreInactive)
{
    WorldPosition botLocation(bot);
        
    vector<TravelDestination*> retTravelLocations;

    bool onlySearchThisMap = urand(0, 10);
    uint32 botMap = bot->GetMap()->GetId();

    for (auto& dest : grindMobs)
    {
        if (!ignoreInactive && !dest->isActive(bot))
            continue;

        if (dest->isFull(ignoreFull))
            continue;

        if (!(onlySearchThisMap && dest->getPoints()[0]->getMapId() == botMap))
            continue;

        int level = bot->GetLevel();        
        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(dest->getEntry());      

        if (level >= cInfo->MinLevel && level <= cInfo->MaxLevel)
        {
            //ignore creatures that can only swim
            if (cInfo->InhabitType & INHABIT_GROUND == 0 && cInfo->InhabitType & INHABIT_WATER != 0)
                continue;           

            if (cInfo->ExtraFlags & CREATURE_EXTRA_FLAG_INVISIBLE != 0)
                continue;

            if (cInfo->Rank > 0 && !bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<bool>("can fight boss"))
                continue;

            FactionTemplateEntry const* factionEntry = sFactionTemplateStore.LookupEntry(cInfo->Faction);
            ReputationRank reaction = bot->GetPlayerbotAI()->getReaction(factionEntry);

            if (reaction > REP_NEUTRAL)
                continue;                      

            retTravelLocations.push_back(dest);
        }
    }

    return retTravelLocations;
}

vector<TravelDestination*> TravelMgr::getBossTravelDestinations(Player* bot, bool ignoreFull, bool ignoreInactive)
{
    WorldPosition botLocation(bot);

    vector<TravelDestination*> retTravelLocations;

    for (auto& dest : bossMobs)
    {
        if (!ignoreInactive && !dest->isActive(bot))
            continue;

        if (dest->isFull(ignoreFull))
            continue;

        retTravelLocations.push_back(dest);
    }

    return retTravelLocations;
}

void TravelMgr::setNullTravelTarget(Player* player)
{
    if (!player)
        return;

    if (!player->GetPlayerbotAI())
        return;

    TravelTarget* target = player->GetPlayerbotAI()->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();

    if (target)
        target->setTarget(sTravelMgr.nullTravelDestination, sTravelMgr.nullWorldPosition, true);
}

void TravelMgr::addMapTransfer(WorldPosition start, WorldPosition end, float portalDistance, bool makeShortcuts)
{
    uint32 sMap = start.getMapId();
    uint32 eMap = end.getMapId();

    if (sMap == eMap)
        return;
    
    //Calculate shortcuts.
    if(makeShortcuts)
        for (auto& mapTransfers : mapTransfersMap)
        {
            uint32 sMapt = mapTransfers.first.first;
            uint32 eMapt = mapTransfers.first.second;

            for (auto& mapTransfer : mapTransfers.second)
            {
                if (eMapt == sMap && sMapt != eMap) // [S1 >MT> E1 -> S2] >THIS> E2
                {
                    float newDistToEnd = mapTransDistance(*mapTransfer.getPointFrom(), start) + portalDistance;
                    if (mapTransDistance(*mapTransfer.getPointFrom(), end) > newDistToEnd)
                        addMapTransfer(*mapTransfer.getPointFrom(), end, newDistToEnd, false);
                }

                if (sMapt == eMap && eMapt != sMap) // S1 >THIS> [E1 -> S2 >MT> E2]
                {
                    float newDistToEnd = portalDistance + mapTransDistance(end, *mapTransfer.getPointTo());
                    if (mapTransDistance(start, *mapTransfer.getPointTo()) > newDistToEnd)
                        addMapTransfer(start, *mapTransfer.getPointTo(), newDistToEnd, false);
                }
            }
        }

    //Add actual transfer.
    auto mapTransfers = mapTransfersMap.find(make_pair(start.getMapId(), end.getMapId()));
    
    if (mapTransfers == mapTransfersMap.end())
        mapTransfersMap.insert({ { sMap, eMap }, {mapTransfer(start, end, portalDistance)} });
    else
        mapTransfers->second.push_back(mapTransfer(start, end, portalDistance));        
};

void TravelMgr::loadMapTransfers()
{
    for (auto& node : sTravelNodeMap.getNodes())
    {
        for (auto& link : *node->getLinks())
        {
            addMapTransfer(*node->getPosition(), *link.first->getPosition(), link.second->getDistance());
        }
    }
}

float TravelMgr::mapTransDistance(WorldPosition start, WorldPosition end)
{
    uint32 sMap = start.getMapId();
    uint32 eMap = end.getMapId();

    if (sMap == eMap)
        return start.distance(end);

    float minDist = 200000;

    auto mapTransfers = mapTransfersMap.find({ sMap, eMap });
    
    if (mapTransfers == mapTransfersMap.end())
        return minDist;

    for (auto& mapTrans : mapTransfers->second)
    {
        float dist = mapTrans.distance(start, end);

        if (dist < minDist)
            minDist = dist;
    }    

    return minDist;
}

float TravelMgr::fastMapTransDistance(WorldPosition start, WorldPosition end)
{
    uint32 sMap = start.getMapId();
    uint32 eMap = end.getMapId();

    if (sMap == eMap)
        return start.fDist(end);

    float minDist = 200000;

    auto mapTransfers = mapTransfersMap.find({ sMap, eMap });

    if (mapTransfers == mapTransfersMap.end())
        return minDist;

    for (auto& mapTrans : mapTransfers->second)
    {
        float dist = mapTrans.fDist(start, end);

        if (dist < minDist)
            minDist = dist;
    }

    return minDist;
}

void TravelMgr::printGrid(uint32 mapId, int x, int y, string type)
{
    string fileName = "unload_grid.csv";

    if (sPlayerbotAIConfig.hasLog(fileName))
    {
        WorldPosition p = WorldPosition(mapId, 0, 0, 0, 0);

        ostringstream out;
        out << sPlayerbotAIConfig.GetTimestampStr();
        out << "+00, " << 0 << 0 << x << "," << y << ", " << type << ",";
        p.printWKT(p.fromGridPair(GridPair(x, y)), out, 1, true);
        sPlayerbotAIConfig.log(fileName, out.str().c_str());
    }
}

void TravelMgr::printObj(WorldObject* obj, string type)
{
    string fileName = "unload_grid.csv";

    if (sPlayerbotAIConfig.hasLog(fileName))
    {
        WorldPosition p = WorldPosition(obj);

        Cell const& cell = obj->GetCurrentCell();

        vector<WorldPosition> vcell, vgrid;
        vcell = p.fromCellPair(p.getCellPair());
        vgrid = p.gridFromCellPair(p.getCellPair());

        {
            ostringstream out;
            out << sPlayerbotAIConfig.GetTimestampStr();
            out << "+00, " << obj->GetObjectGuid().GetEntry() << "," << obj->GetObjectGuid().GetCounter() << "," << cell.GridX() << "," << cell.GridY() << ", " << type << ",";

            p.printWKT(vcell, out, 1, true);
            sPlayerbotAIConfig.log(fileName, out.str().c_str());
        }

        {
            ostringstream out;
            out << sPlayerbotAIConfig.GetTimestampStr();
            out << "+00, " << obj->GetObjectGuid().GetEntry() << "," << obj->GetObjectGuid().GetCounter() << "," << cell.GridX() << "," << cell.GridY() << ", " << type << ",";

            p.printWKT(vgrid, out, 1, true);
            sPlayerbotAIConfig.log(fileName, out.str().c_str());
        }
    }

    fileName = "unload_obj.csv";

    if (sPlayerbotAIConfig.hasLog(fileName))
    {
        WorldPosition p = WorldPosition(obj);

        Cell const& cell = obj->GetCurrentCell();

        {
            ostringstream out;
            out << sPlayerbotAIConfig.GetTimestampStr();
            out << "+00, " << obj->GetObjectGuid().GetEntry() << "," << obj->GetObjectGuid().GetCounter() << "," << cell.GridX() << "," << cell.GridY() << ", " << type << ",";

            p.printWKT({ p }, out, 0);
            sPlayerbotAIConfig.log(fileName, out.str().c_str());
        }
    }
}