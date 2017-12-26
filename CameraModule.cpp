#include "sc2api/sc2_api.h"
#include "CameraModule.h"

int TILE_SIZE = 32;

CameraModule::CameraModule(sc2::Agent & bot):
	m_bot(bot), 
	cameraMoveTime(150),
	cameraMoveTimeMin(50),
	watchScoutWorkerUntil(7500),
	lastMoved(0),
	lastMovedPriority(0),
	lastMovedPosition(sc2::Point2D(0, 0)),
	cameraFocusPosition(sc2::Point2D(0, 0)),
	cameraFocusUnit(nullptr),
	followUnit(false)
{
}

void CameraModule::onStart(int screenWidth, int screenHeight)
{
	myStartLocation = getPlayerStartLocation();
	cameraFocusPosition = myStartLocation;
	currentCameraPosition = myStartLocation;
	scrWidth = screenWidth;
	scrHeight = screenHeight;
}

void CameraModule::onFrame()
{
	moveCameraFallingNuke();
	//moveCameraNukeDetect();
	moveCameraIsUnderAttack();
	moveCameraIsAttacking();
	if (m_bot.Observation()->GetGameLoop() <= watchScoutWorkerUntil)
	{
		moveCameraScoutWorker();
	}
	moveCameraDrop();
	moveCameraArmy();

	updateCameraPosition();
}

void CameraModule::moveCameraFallingNuke()
{
	int prio = 5;
	if (!shouldMoveCamera(prio))
	{
		return;
	}

	for(auto & effects: m_bot.Observation()->GetEffects())
	{
		if (effects.effect_id==uint32_t(7)) //7 = NukePersistent NOT TESTED YET
		{
			moveCamera(effects.positions.front(), prio);
			return;
		}
	}
}

//Not yet implemented
void CameraModule::moveCameraNukeDetect(const sc2::Point2D target)
{
	int prio = 4;
	if (!shouldMoveCamera(prio))
	{
		return;
	}
	else
	{
		moveCamera(target, prio);
	}
}

void CameraModule::moveCameraIsUnderAttack()
{
	const int prio = 3;
	if (!shouldMoveCamera(prio))
	{
		return;
	}

	for (auto unit : m_bot.Observation()->GetUnits())
	{
		if (isUnderAttack(unit))
		{
			moveCamera(unit, prio);
		}
	}
}


void CameraModule::moveCameraIsAttacking()
{
	int prio = 3;
	if (!shouldMoveCamera(prio))
	{
		return;
	}

	for (auto unit : m_bot.Observation()->GetUnits())
	{
		if (isAttacking(unit))
		{
			moveCamera(unit, prio);
		}
	}
}

void CameraModule::moveCameraScoutWorker()
{
	int highPrio = 2;
	int lowPrio = 0;
	if (!shouldMoveCamera(lowPrio))
	{
		return;
	}

	for (auto & unit : m_bot.Observation()->GetUnits())
	{
		if (!IsWorkerType(unit->unit_type))
		{
			continue;
		}
		//In the BW code here seems to be a bug. Why use high prio if we are near our base?
		if (isNearEnemyStartLocation(unit->pos))
		{
			moveCamera(unit, highPrio);
		}
		else if (!isNearOwnStartLocation(unit->pos))
		{
			moveCamera(unit, lowPrio);
		}
	}
}

void CameraModule::moveCameraDrop() {
	int prio = 2;
	if (!shouldMoveCamera(prio))
	{
		return;
	}
	for (auto & unit : m_bot.Observation()->GetUnits())
	{
		if ((unit->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_OVERLORDTRANSPORT || unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC || unit->unit_type.ToType() == sc2::UNIT_TYPEID::PROTOSS_WARPPRISM)
			&& isNearEnemyStartLocation(unit->pos) && unit->cargo_space_taken > 0)
		{
			moveCamera(unit, prio);
		}
	}
}

void CameraModule::moveCameraArmy()
{
	int prio = 1;
	if (!shouldMoveCamera(prio))
	{
		return;
	}
	// Double loop, check if army units are close to each other
	int radius = 50;

	sc2::Point2D bestPos;
	const sc2::Unit * bestPosUnit = nullptr;
	int mostUnitsNearby = 0;

	for (auto & unit: m_bot.Observation()->GetUnits())
	{
		if (!isArmyUnitType(unit->unit_type.ToType()) || unit->display_type!=sc2::Unit::DisplayType::Visible || unit->alliance==sc2::Unit::Alliance::Neutral)
		{
			continue;
		}
		sc2::Point2D uPos = unit->pos;

		int nrUnitsNearby = 0;
		for (auto & nearbyUnit : m_bot.Observation()->GetUnits())
		{
			if (!isArmyUnitType(nearbyUnit->unit_type.ToType()) || unit->display_type != sc2::Unit::DisplayType::Visible || unit->alliance == sc2::Unit::Alliance::Neutral)
			{
				continue;
			}
			nrUnitsNearby++;
		}

		if (nrUnitsNearby > mostUnitsNearby) {
			mostUnitsNearby = nrUnitsNearby;
			bestPos = uPos;
			bestPosUnit = unit;
		}
	}

	if (mostUnitsNearby > 1) {
		moveCamera(bestPosUnit, prio);
	}
}

void CameraModule::moveCameraUnitCreated(const sc2::Unit * unit)
{
	int prio = 1;
	if (!shouldMoveCamera(prio))
	{
		return;
	}
	else if (unit->alliance==sc2::Unit::Alliance::Self && !IsWorkerType(unit->unit_type))
	{
		moveCamera(unit, prio);
	}
}

const bool CameraModule::shouldMoveCamera(int priority) const
{
	const int elapsedFrames = m_bot.Observation()->GetGameLoop() - lastMoved;
	const bool isTimeToMove = elapsedFrames >= cameraMoveTime;
	const bool isTimeToMoveIfHigherPrio = elapsedFrames >= cameraMoveTimeMin;
	const bool isHigherPrio = lastMovedPriority < priority;
	// camera should move IF: enough time has passed OR (minimum time has passed AND new prio is higher)
	return isTimeToMove || (isHigherPrio && isTimeToMoveIfHigherPrio);
}

void CameraModule::moveCamera(sc2::Point2D pos, int priority)
{
	if (!shouldMoveCamera(priority))
	{
		return;
	}
	if (followUnit == false && cameraFocusPosition == pos)
	{
		// don't register a camera move if the position is the same
		return;
	}

	cameraFocusPosition = pos;
	lastMovedPosition = cameraFocusPosition;
	lastMoved = m_bot.Observation()->GetGameLoop();
	lastMovedPriority = priority;
	followUnit = false;
}

void CameraModule::moveCamera(const sc2::Unit * unit, int priority)
{
	if (!shouldMoveCamera(priority))
	{
		return;
	}
	if (followUnit == true && cameraFocusUnit == unit) {
		// don't register a camera move if we follow the same unit
		return;
	}

	cameraFocusUnit = unit;
	lastMovedPosition = cameraFocusUnit->pos;
	lastMoved = m_bot.Observation()->GetGameLoop();
	lastMovedPriority = priority;
	followUnit = true;
}




void CameraModule::updateCameraPosition()
{
	float moveFactor = 0.1;
	if (followUnit && isValidPos(cameraFocusUnit->pos))
	{
		cameraFocusPosition = cameraFocusUnit->pos;
	}
	currentCameraPosition = currentCameraPosition + sc2::Point2D(
		moveFactor*(cameraFocusPosition.x - currentCameraPosition.x),
		moveFactor*(cameraFocusPosition.y - currentCameraPosition.y));
	sc2::Point2D currentMovedPosition = currentCameraPosition;// -sc2::Point2D(scrWidth / 2, scrHeight / 2 - 40); // -40 to account for HUD

	if (isValidPos(currentCameraPosition))
	{
		
		const sc2::Point2DI minimapPos = ConvertWorldToMinimap(currentMovedPosition);
		m_bot.ActionsFeatureLayer()->CameraMove(minimapPos);
	}
}

//Utility

//At the moment there is no flag for being under attack
const bool CameraModule::isUnderAttack(const sc2::Unit * unit) const
{
	return false;
}

const bool CameraModule::isAttacking(const sc2::Unit * unit) const
{
	//Option A
	return unit->orders.size()>0 && unit->orders.front().ability_id.ToType() == sc2::ABILITY_ID::ATTACK_ATTACK;
	//Option B
	//return unit->weapon_cooldown > 0.0f;
}

const bool CameraModule::IsWorkerType(const sc2::UNIT_TYPEID type) const
{
	switch (type)
	{
	case sc2::UNIT_TYPEID::TERRAN_SCV: return true;
	case sc2::UNIT_TYPEID::TERRAN_MULE: return true;
	case sc2::UNIT_TYPEID::PROTOSS_PROBE: return true;
	case sc2::UNIT_TYPEID::ZERG_DRONE: return true;
	case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED: return true;
	default: return false;
	}
}
bool CameraModule::isNearEnemyBuilding(const sc2::Unit * unit, sc2::Units enemyUnits) const
{
	for (auto & enemy:enemyUnits) {
		if (isBuilding(enemy->unit_type.ToType())
			&& Dist(unit,enemy) <= TILE_SIZE*20 
			&& unit->alliance != enemy->alliance)
		{
			return true;
		}
	}

	return false;
}

bool CameraModule::isNearEnemyStartLocation(sc2::Point2D pos) {
	int distance = 100;

	std::vector<sc2::Point2D> startLocations = m_bot.Observation()->GetGameInfo().enemy_start_locations;

	for (auto & startLocation : startLocations)
	{
		// if the start position is not our own home, and the start position is closer than distance
		if (Dist(pos,startLocation) <= distance)
		{
			return true;
		}
	}

	return false;
}

const bool CameraModule::isNearOwnStartLocation(const sc2::Point2D pos) const
{
	int distance = 10 * TILE_SIZE; // 10*32
	return Dist(myStartLocation,pos) <= distance;
}

const bool CameraModule::isArmyUnitType(sc2::UNIT_TYPEID type) const
{
	if (IsWorkerType(type)) { return false; }
	if (type == sc2::UNIT_TYPEID::ZERG_OVERLORD) { return false; } //Excluded here the overlord transport etc to count them as army unit
	if (isBuilding(type)) { return false; }
	if (type == sc2::UNIT_TYPEID::ZERG_EGG) { return false; }
	if (type == sc2::UNIT_TYPEID::ZERG_LARVA) { return false; }

	return true;
}

const bool CameraModule::isBuilding(sc2::UNIT_TYPEID type) const
{
	switch(type)
	{
		//Terran
	case sc2::UNIT_TYPEID::TERRAN_ARMORY:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
	case sc2::UNIT_TYPEID::TERRAN_BUNKER:
	case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER:
	case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
	case sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY:
	case sc2::UNIT_TYPEID::TERRAN_FACTORY:
	case sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING:
	case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
	case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
	case sc2::UNIT_TYPEID::TERRAN_FUSIONCORE:
	case sc2::UNIT_TYPEID::TERRAN_GHOSTACADEMY:
	case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET:
	case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
	case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
	case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
	case sc2::UNIT_TYPEID::TERRAN_REFINERY:
	case sc2::UNIT_TYPEID::TERRAN_SENSORTOWER:
	case sc2::UNIT_TYPEID::TERRAN_STARPORT:
	case sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING:
	case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
	case sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
	case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT:
	case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
	case sc2::UNIT_TYPEID::TERRAN_REACTOR:
	case sc2::UNIT_TYPEID::TERRAN_TECHLAB:

		// Zerg
	case sc2::UNIT_TYPEID::ZERG_BANELINGNEST:
	case sc2::UNIT_TYPEID::ZERG_CREEPTUMOR:
	case sc2::UNIT_TYPEID::ZERG_CREEPTUMORBURROWED:
	case sc2::UNIT_TYPEID::ZERG_CREEPTUMORQUEEN:
	case sc2::UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER:
	case sc2::UNIT_TYPEID::ZERG_EXTRACTOR:
	case sc2::UNIT_TYPEID::ZERG_GREATERSPIRE:
	case sc2::UNIT_TYPEID::ZERG_HATCHERY:
	case sc2::UNIT_TYPEID::ZERG_HIVE:
	case sc2::UNIT_TYPEID::ZERG_HYDRALISKDEN:
	case sc2::UNIT_TYPEID::ZERG_INFESTATIONPIT:
	case sc2::UNIT_TYPEID::ZERG_LAIR:
	case sc2::UNIT_TYPEID::ZERG_LURKERDENMP:
	case sc2::UNIT_TYPEID::ZERG_NYDUSCANAL:
	case sc2::UNIT_TYPEID::ZERG_NYDUSNETWORK:
	case sc2::UNIT_TYPEID::ZERG_SPAWNINGPOOL:
	case sc2::UNIT_TYPEID::ZERG_SPINECRAWLER:
	case sc2::UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED:
	case sc2::UNIT_TYPEID::ZERG_SPIRE:
	case sc2::UNIT_TYPEID::ZERG_SPORECRAWLER:
	case sc2::UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED:
	case sc2::UNIT_TYPEID::ZERG_ULTRALISKCAVERN:

		// Protoss
	case sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR:
	case sc2::UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
	case sc2::UNIT_TYPEID::PROTOSS_DARKSHRINE:
	case sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON:
	case sc2::UNIT_TYPEID::PROTOSS_FORGE:
	case sc2::UNIT_TYPEID::PROTOSS_GATEWAY:
	case sc2::UNIT_TYPEID::PROTOSS_NEXUS:
	case sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON:
	case sc2::UNIT_TYPEID::PROTOSS_PYLON:
	case sc2::UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED:
	case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
	case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
	case sc2::UNIT_TYPEID::PROTOSS_STARGATE:
	case sc2::UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
	case sc2::UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
	case sc2::UNIT_TYPEID::PROTOSS_WARPGATE:
		return true;
	}
	return false;
}

const bool CameraModule::isValidPos(const sc2::Point2D pos) const
{
	//Maybe playable width/height?
	return pos.x >= 0 && pos.y >= 0 && pos.x < m_bot.Observation()->GetGameInfo().width && pos.y < m_bot.Observation()->GetGameInfo().height;
}

const float CameraModule::Dist(const sc2::Unit * A, const sc2::Unit * B) const
{
	return std::sqrt(std::pow(A->pos.x - B->pos.x, 2) + std::pow(A->pos.y - B->pos.y, 2));
}

const float CameraModule::Dist(const sc2::Point2D A, const sc2::Point2D B) const
{
	return std::sqrt(std::pow(A.x - B.x, 2) + std::pow(A.y - B.y, 2));
}

const sc2::Point2DI CameraModule::ConvertWorldToMinimap(const sc2::Point2D& world) const
{
	const sc2::GameInfo game_info = m_bot.Observation()->GetGameInfo();
	const int image_width = game_info.options.feature_layer.minimap_resolution_x;
	const int image_height = game_info.options.feature_layer.minimap_resolution_y;
	const float map_width = (float)game_info.width;
	const float map_height = (float)game_info.height;

	// Pixels always cover a square amount of world space. The scale is determined
	// by the largest axis of the map.
	const float pixel_size = std::max(map_width / image_width, map_height / image_height);

	// Origin of world space is bottom left. Origin of image space is top left.
	// Upper left corner of the map corresponds to the upper left corner of the upper 
	// left pixel of the feature layer.
	const float image_origin_x = 0;
	const float image_origin_y = map_height;
	const float image_relative_x = world.x - image_origin_x;
	const float image_relative_y = image_origin_y - world.y;

	const int image_x = static_cast<int>((image_relative_x / pixel_size));
	const int image_y = static_cast<int>((image_relative_y / pixel_size));

	return sc2::Point2DI(image_x, image_y);
}

const sc2::Point2D CameraModule::getPlayerStartLocation() const
{
	for (auto & unit : m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER,sc2::UNIT_TYPEID::ZERG_HATCHERY,sc2::UNIT_TYPEID::PROTOSS_NEXUS })))
	{
		for (auto & startLocation : m_bot.Observation()->GetGameInfo().start_locations)
		{
			if (Dist(unit->pos, startLocation) < 20.0f)
			{
				return startLocation;
			}
		}
	}
	return sc2::Point2D(0.0f, 0.0f);
}