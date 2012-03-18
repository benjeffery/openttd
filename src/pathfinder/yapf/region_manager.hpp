/** @file region_manager.hpp */

#ifndef  REGION_MANAGER_HPP
#define  REGION_MANAGER_HPP

#include "../../tile_map.h"
//#include "../../gfx.h"
#include "../../landscape.h"
//#include "../../direction.h"
#include "../../water_map.h"
#include "../../openttd.h"
#include "yapf.hpp"

#include <vector>
#include <set>
#include <stack>
#include <map>
#include <math.h>

using std::vector;
using std::set;
using std::stack;
using std::multimap;
using std::pair;

template<class TRegion>
class CRegionManager
{
	private:
	static CRegionManager<TRegion> m_manager;
	set<TRegion*> m_regions;

	public:
	static inline CRegionManager<TRegion> *GetManager()
	{
		return & m_manager;
	}

	set<TRegion*> Regions()
	{
		return m_regions;
	}
	
	void AddRegion(TRegion *reg)
	{
		m_regions.insert(reg);
	}
	void AddNewTile(TileIndex tile)
	{
		/*return if this is not a water tile, or if another region has claimed it*/
		if((!TRegion::TRD::IsRoutable(tile)) || TRegion::TRD::GetRegion(tile) != NULL) return;

		/*Get a set of possible region-valid neighbours*/
		uint x = TileX(tile);
		uint y = TileY(tile);
		TileIndex neighbour_tiles[4];
		neighbour_tiles[0] = TileXY(x+1,y);
		neighbour_tiles[1] = TileXY(x,y+1);
		neighbour_tiles[2] = TileXY(x-1,y);
		neighbour_tiles[3] = TileXY(x,y-1);
		vector<TRegion*> good_neighbours;
		for (short i = 0;i < 4;++i){
			if (TRegion::TRD::IsRoutable(neighbour_tiles[i])
				&& TRegion::TRD::IsPassable(tile,neighbour_tiles[i])
				&& TRegion::TRD::GetRegion(neighbour_tiles[i]) != NULL)
				good_neighbours.push_back(TRegion::TRD::GetRegion(neighbour_tiles[i]));
		}
		
		if (good_neighbours.size() > 0)
		{
			/*Add the tile to the region with the nearest center and link in the rest*/
			uint nearest = DistanceSquare(tile,good_neighbours[0]->GetCenter());
			TRegion *nearest_reg = good_neighbours[0];
			for (uint i = 1;i<good_neighbours.size();++i)
			{
				if (DistanceSquare(tile,good_neighbours[i]->GetCenter()) < nearest){
					nearest = DistanceSquare(tile,good_neighbours[i]->GetCenter());
					nearest_reg = good_neighbours[i];
				}
			}
			nearest_reg->AddTile(tile);
			/*Link to the other neighbours*/
			for (uint i = 0;i<good_neighbours.size();++i){
				nearest_reg->Neighbours()->Add(good_neighbours[i]);
			}
			/*Did it grow too big?*/
			if (nearest_reg->NumTiles() > TRegion::TRD::MAX_TILES_PER_REGION+TRegion::TRD::MIN_REGION_SIZE){
				this->SplitRegion(nearest_reg,true);
			}
			else{
				/*Check it hasn't been too streched*/
				set<TRegion*> new_regions;
				new_regions.insert(nearest_reg);
				new_regions = this->RefindElongatedRegions(new_regions);
				/*Check they are not concave*/
				new_regions = this->SplitConcaveRegions(new_regions);
				/*And not too small*/
				new_regions = this->RemoveSmallRegions(new_regions);
			}
		}
		else
		this->m_regions.insert(new TRegion(tile,TRegion::TRD::MAX_TILES_PER_REGION));
	}

	void RebuildRegionsFromTiles()
	{
#ifndef NO_DEBUG_MESSAGES
		CPerformanceTimer perf;
		perf.Start();
#endif /* !NO_DEBUG_MESSAGES */		
		/*Remove old regions (without them wiping their tiles)*/
		set<TRegion*> temp = this->m_regions;
		this->m_regions.clear();
		for (typename set<TRegion*>::iterator i = temp.begin();i != temp.end(); ++i){
			(*i)->ClearTiles();
			delete *i;
		}

		/*Activate updates from landscape changes*/
		TRegion::TRD::updates_active = true;

		/*Loop over tiles, putting them into groups with a region id*/
		multimap<uint16,TileIndex> tiles;
		for (uint y = 0; y != MapSizeY(); y++)
			for (uint x = 0; x != MapSizeX(); x++)
				tiles.insert(pair<uint16,TileIndex>(TRegion::TRD::GetRegionID(TileXY(x,y)), TileXY(x,y)));
		/*Loop over regions creating them as we go*/
		while (!tiles.empty()){
			vector<TileIndex> region_tiles;
			/*Get the all the tiles that match the first region id*/
			uint16 id = (*tiles.begin()).first;
			if (id != 0){
				for (multimap<uint16,TileIndex>::iterator i = tiles.equal_range(id).first; i != tiles.equal_range(id).second; ++i)
					region_tiles.push_back((*i).second);
				/*Make a region with those tiles and id*/
				this->AddRegion(new TRegion(id,region_tiles));
			}
			/*Erase all tiles with that id from the list*/
			tiles.erase(id);
		}

		/*Rebuild the connection network*/
		for (typename set<TRegion*>::iterator i = m_regions.begin();i != m_regions.end(); ++i){
			(*i)->RecheckConnections();
		}
#ifndef NO_DEBUG_MESSAGES
		perf.Stop();
		if (_debug_yapf_level >= 3) {
			DEBUG(yapf, 3, "[Region] %d Rebuilt in %d us --", (int)this->m_regions.size(),perf.Get(1000000));
		}
#endif /* !NO_DEBUG_MESSAGES */
	}

	void FindRegionsFromScratch()
	{
#ifndef NO_DEBUG_MESSAGES
		CPerformanceTimer perf;
		perf.Start();
#endif /* !NO_DEBUG_MESSAGES */		

		/*Remove old regions*/
		set<TRegion*> temp = this->m_regions;
		this->m_regions.clear();
		for (typename set<TRegion*>::iterator i = temp.begin();i != temp.end(); ++i){
			delete *i;
		}

		/*Activate updates from landscape changes*/
		TRegion::TRD::updates_active = true;

		/*Clear the tiles region id*/
		for (uint y = 0; y != MapSizeY(); y++)
			for (uint x = 0; x != MapSizeX(); x++)
				TRegion::TRD::SetRegion(TileXY(x,y),NULL);

		/*Loop over tiles seeding from valid ones not already in regions*/
		for (uint y = 1; y != MapSizeY()-1; y++){
			for (uint x = 1; x != MapSizeX()-1; x++){
				TileIndex tile = TileXY(x,y);
				if (TRegion::TRD::GetRegion(tile) == NULL
				&& TRegion::TRD::IsRoutable(tile)){
					/*Region is found on construction*/
					this->m_regions.insert(new TRegion(tile,TRegion::TRD::MAX_TILES_PER_REGION));
				}
			}
		}

		this->SplitConcaveRegions(m_regions);
		this->RemoveSmallRegions(m_regions);

#ifndef NO_DEBUG_MESSAGES
		perf.Stop();
		if (_debug_yapf_level >= 3) {
			DEBUG(yapf, 3, "[Region] %d Found in %d us --", (int)this->m_regions.size(),perf.Get(1000000));
		}
#endif /* !NO_DEBUG_MESSAGES */

	/*Check that all routable tiles are assigned to a region
	uint unassigned = 0;
	for (uint y = 1; y != MapSizeY()-1; y++){
			for (uint x = 1; x != MapSizeX()-1; x++){
				TileIndex tile = TileXY(x,y);
				if (TRegion::TRD::GetRegion(tile) == NULL
				&& TRegion::TRD::IsRoutable(tile)){
					++unassigned;
				}
				
			}
		}
	assert(unassigned == 0);*/
	}

	set<TRegion*> RemoveSmallRegions(set<TRegion*> regions)
	{
		set<TRegion*> surviving_regions;
		/*Find regions that are small and attach to thier neighbour (if there are any)*/
		for (typename set<TRegion*>::iterator loser = regions.begin();
		loser != regions.end();
		++loser){
			if ((*loser)->NumTiles() < TRegion::TRD::MIN_REGION_SIZE
			&& (!(*loser)->Neighbours()->Empty()))
			{
				/*Add this regions tiles to the 1st connected region - region will set tile's region id*/
				TRegion *reciever = (*(*loser)->Neighbours()->Get().begin());
				reciever->AddTiles((*loser)->GetTiles());
				(*loser)->ClearTiles();

				/*Replace all connections to the new region (the loser will remove its connections on delete)*/
				set<TRegion*> temp = (*loser)->Neighbours()->Get();
				for (typename set<TRegion*>::iterator neighbour = temp.begin();
				neighbour != temp.end();
				++neighbour){
					reciever->Neighbours()->Add(*neighbour);
				}
				delete (*loser);
				this->m_regions.erase(*loser);
			}
			else
				surviving_regions.insert(*loser);
		}
		return surviving_regions;
	}

	set<TRegion*> SplitRegion(TRegion* region, bool check_concave)
	{
		vector<TileIndex> oldtiles = region->GetTiles();
		/*Deletion resets the tile ids and removes connections*/
		delete region;
		m_regions.erase(region);

		set<TRegion*> to_check;
		/*Loop over the old tiles finding new regions*/
		for (uint i=0;i<oldtiles.size();++i){
			if (TRegion::TRD::GetRegion(oldtiles[i]) == NULL
			&& TRegion::TRD::IsRoutable(oldtiles[i])){
				/*Region is found on construction*/
				TRegion *new_region = new TRegion(oldtiles[i],oldtiles.size() >> 1);
				this->m_regions.insert(new_region);
				to_check.insert(new_region);
			}
		}

		/*Merge the small ones and check for concave*/
		return this->RemoveSmallRegions(check_concave ? this->SplitConcaveRegions(to_check) : to_check);
	}

	set<TRegion*> SplitConcaveRegions(set<TRegion*> regions)
	{
		set<TRegion*> new_regions;
		for (typename set<TRegion*>::iterator i = regions.begin();
		i != regions.end();
		++i){
			if ((*i)->BadCenter() && (*i)->NumTiles() > TRegion::TRD::MIN_REGION_SIZE){
				set<TRegion*> split_regions = SplitRegion(*i, false);
				new_regions.insert(split_regions.begin(),split_regions.end());
			}
			else
				new_regions.insert(*i);
		}
		return new_regions;
	}

	set<TRegion*> RefindRegion(TRegion* region, bool refind_neighbours)
	{
		set<TRegion*> new_regions;
		set<TRegion*> regions;
		if (refind_neighbours) regions = region->Neighbours()->Get();
		regions.insert(region);

		vector<TileIndex> tiles;
		for (typename set<TRegion*>::iterator i = regions.begin();i != regions.end(); ++i){
			vector<TileIndex> add = (*i)->GetTiles();
			tiles.insert(tiles.end(),add.begin(),add.end());
			delete (*i);
			m_regions.erase(region);
		}

		set<TRegion*> to_check;
		for (uint i=0;i<tiles.size();++i){
			if (TRegion::TRD::GetRegion(tiles[i]) == NULL
			&& TRegion::TRD::IsRoutable(tiles[i])){
				/*Region is found on construction*/
				TRegion* new_region = new TRegion(tiles[i],TRegion::TRD::MAX_TILES_PER_REGION);
				this->m_regions.insert(new_region);
				to_check.insert(new_region);
			}
		}
		return this->RemoveSmallRegions(this->SplitConcaveRegions(to_check));
	}

	set<TRegion*> RefindElongatedRegions(set<TRegion*> regions)
	{
		set<TRegion*> new_regions;
		for (typename set<TRegion*>::iterator i = regions.begin();
		i != regions.end();
		++i){
			/*Check that the region still exists - we might deleted it in the last iteration as we change neighbours*/
			if (m_regions.count(*i) > 0){
				uint aspect_ratio = ((*i)->WidthX() > (*i)->HeightY() ? (*i)->WidthX()/(*i)->HeightY() : (*i)->HeightY()/(*i)->WidthX());
				if (aspect_ratio > 2){
					set<TRegion*> brand_new_regions = RefindRegion(*i,true);
					new_regions.insert(brand_new_regions.begin(),brand_new_regions.end());
				}
			}
		}
		return new_regions;
	}
	};

/*Static instance of manager*/
/*GCC wants this here - it may be different for other compilers*/
template<class TRegion> CRegionManager<TRegion> CRegionManager<TRegion>::m_manager;

#endif

