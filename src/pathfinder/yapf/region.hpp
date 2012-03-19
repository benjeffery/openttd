/** @file region.hpp */

#ifndef  REGION_HPP
#define  REGION_HPP

#include "../../tile_map.h"
//#include "../../gfx.h"
#include "../../landscape.h"
//#include "../../direction.h"
#include "../../water_map.h"
#include "../../openttd.h"
#include "../../misc/intsqrt.h"
#include "region_manager.hpp"
#include "region_tile_store.h"
#include "region_neighbours.hpp"
#include <queue>
#include <vector>
#include <set>
using std::set;
using std::queue;
using std::vector;

template<class TRegionDescription>
class CRegion
{
	public:
	typedef TRegionDescription TRD;

	private:

	CRegionTileStore        m_tile_store;
	TileIndex               m_seedtile;
	uint                    m_num_tiles;
	CRegionNeighbours<CRegion<TRD> >	m_neighbours;
	TileIndex               m_center_tile;
	uint16                  m_index_number;
	bool                    m_center_bad;

	public:
	static vector<CRegion<TRD>*>	m_region_index;
	static vector<bool>		m_used_region_indices;

	private:
	/*Should not be used*/
	CRegion<TRD>();
	CRegion<TRD>(CRegion<TRD>&);
	CRegion<TRD>& operator=(const CRegion<TRD>&);


	public:
	/*Find region from a seed tile*/
	CRegion<TRD>(TileIndex seed, uint numtiles)
	{
		assert(numtiles != 0);

		/*Get an ID for us*/
		this->m_index_number = FindAndReserveFreeID();
		m_region_index[this->m_index_number] = this;

		/*Prepare the tile store*/
		this->m_tile_store.Init(32,32,seed);

		/*Prepare the neighbour storage*/
		this->Neighbours()->Clear();
		this->Neighbours()->OwnerRegion() = this;

		/*Find the tiles that belong to us*/
		this->FindTiles(seed,numtiles);

	}
	/*Create region from a set of tiles*/
	CRegion<TRD>(vector<TileIndex> tiles)
	{
		assert(!tiles.empty());

		/*Get an ID for us*/
		this->m_index_number = FindAndReserveFreeID();
		m_region_index[this->m_index_number] = this;

		/*Prepare the tile store*/
		this->m_tile_store.Init(32,32,tiles[tiles.size() >> 1]);

		/*Prepare the neighbour storage*/
		this->Neighbours()->Clear();
		this->Neighbours()->OwnerRegion() = this;

		/*Add the tiles that were given to us*/
		for (uint i = 0; i < tiles.size(); ++i){
			++(this->m_num_tiles);
			this->m_tile_store.Add(tiles[i]);
			TRD::SetRegion(tiles[i],this);
		}

		/*Find our center and connecting regions*/
		this->RefindCentre();
		this->RecheckConnections();
	}

	CRegion<TRD>(uint16 id, vector<TileIndex> tiles)
	{
		/*Construct from tiles already allocated an id
		  We don't check connections as our neighbours don't exist yet!*/

		/*Get an ID for us*/
		ReserveID(id);
		this->m_index_number = id;
		m_region_index[this->m_index_number] = this;

		/*Prepare the tile store*/
		this->m_tile_store.Init(32,32,tiles[tiles.size() >> 1]);

		/*Prepare the neighbour storage*/
		this->Neighbours()->Clear();
		this->Neighbours()->OwnerRegion() = this;

		/*Add the tiles that were given to us*/
		for (uint i = 0; i < tiles.size(); ++i){
			++(this->m_num_tiles);
			this->m_tile_store.Add(tiles[i]);
			assert(TRD::GetRegion(tiles[i]) == this);
		}

		/*Find our center*/
		this->RefindCentre();
	}

	~CRegion<TRD>()
	{
		/*Clear the tiles that point to us*/
		vector<TileIndex> tiles = m_tile_store.GetTiles();
		for (uint i=0;i<tiles.size();++i)
			if (tiles[i] < MapSize())
				TRD::SetRegion(tiles[i],NULL);

		/*Clear all our connections*/
		this->Neighbours()->DestroyConnections();

		/*Release our id*/
		m_region_index[this->m_index_number] = 0;
		FreeID(this->m_index_number);
	}

	inline uint16 GetIndex() const
	{
		return m_index_number;
	}

	inline uint DistanceTo(CRegion<TRD> *reg) const
	{
		/*we have to get around the fact we need an integer square root, so add a couple of
		decimal places to get better accuracy*/
		return lsqrt(100*(long)DistanceSquare(this->GetCenter(), reg->GetCenter()));
	}

	void RecheckConnections()
	{
		/*Clear the connections*/
		this->Neighbours()->DestroyConnections();

		/*Get the tiles, check the neighbours for other regions*/
		vector<TileIndex> tiles = this->GetTiles();
		for (uint i=0;i<tiles.size();++i){
			uint x = TileX(tiles[i]);
			uint y = TileY(tiles[i]);
			TileIndex newtiles[4];
			newtiles[0] = TileXY(x+1,y);
			newtiles[1] = TileXY(x,y+1);
			newtiles[2] = TileXY(x-1,y);
			newtiles[3] = TileXY(x,y-1);
			for (short inew = 0;inew < 4;++inew){
				if (newtiles[inew] < MapSize() &&
				TRD::IsRoutable(newtiles[inew]))
					if (TRD::IsPassable(tiles[i],newtiles[inew]) &&
					TRD::GetRegion(newtiles[inew]) != NULL &&
					TRD::GetRegion(newtiles[inew]) != this)
						this->Neighbours()->Add(TRD::GetRegion(newtiles[inew]));
			}
		}
	}

	inline CRegionNeighbours<CRegion<TRD> > *Neighbours()
	{
		return &this->m_neighbours;
	}

	inline const CRegionNeighbours<CRegion<TRD> > *Neighbours() const
	{
		return &this->m_neighbours;
	}

	inline uint NumTiles()
	{
		return this->m_num_tiles;
	}

	inline void AddTile(TileIndex tile)
	{
		++(this->m_num_tiles);
		this->m_tile_store.Add(tile);
		TRD::SetRegion(tile,this);
		assert(this->m_tile_store.Has(tile));
		this->RefindCentre();
	}

	inline void AddTiles(vector<TileIndex> tiles)
	{
		for (uint i=0;i<tiles.size();++i)
			this->AddTile(tiles[i]);
	}

	inline vector<TileIndex> GetTiles() const
	{
		return this->m_tile_store.GetTiles();
	}

	inline uint WidthX() const
	{
		return this->m_tile_store.WidthX();
	}

	inline uint HeightY() const
	{
		return this->m_tile_store.HeightY();
	}

	inline TileIndex GetCenter() const
	{
		return this->m_center_tile;
	}

	inline bool BadCenter()
	{
		return m_center_bad;
	}

	void RefindCentre()
	{
		if (this->NumTiles() != 0){
			vector<TileIndex> tiles = this->GetTiles();
			uint sum_x = 0;
			uint sum_y = 0;
			for (uint i = 0; i != tiles.size(); i++)
			{
				sum_x += TileX(tiles[i]);
				sum_y += TileY(tiles[i]);
			}
			TileIndex avg = TileXY(sum_x/tiles.size(),sum_y/tiles.size());
			if (TRD::GetRegion(avg) != this){
				this->m_center_tile = tiles[tiles.size() >> 1];
				m_center_bad = true;
			}
			else{
				this->m_center_tile = avg;
				m_center_bad = false;
			}
		}
	}

	inline void ClearTiles()
	{
		this->m_tile_store.Clear();
		this->m_num_tiles = 0;
	}

	void FindTiles(TileIndex seed, uint numtiles)
	{
		this->m_seedtile=seed;
		this->m_num_tiles = 0;
		this->Neighbours()->Clear();
		assert(seed < MapSize() && TRD::GetRegion(seed) == NULL && TRD::IsRoutable(seed));

		/*Make a queue of tiles to add*/
		queue<TileIndex> Unexplored;
		/*And a way of remembering whats in the queue*/
		set<TileIndex> DoNotAdd;
		/*Add the seed point to the stack*/
		Unexplored.push(seed);
		DoNotAdd.insert(seed);
		++(this->m_num_tiles);

		/*To remember tiles for center calculation*/
		uint sum_x=0;
		uint sum_y=0;

		/*Loop till all tiles queued*/
		while (!Unexplored.empty()){
			TileIndex tile = Unexplored.front();
			Unexplored.pop();
			DoNotAdd.erase(tile);

			/*Add to region*/
			assert(TRD::IsRoutable(tile));
			this->m_tile_store.Add(tile);
			TRD::SetRegion(tile,this);
			
			/*Add to sum for center calc later*/
			uint x = TileX(tile);
			uint y = TileY(tile);
			sum_x += x;
			sum_y += y;
			
			/*Queue neighbours*/
			TileIndex newtiles[4];
			newtiles[0] = TileXY(x+1,y);
			newtiles[1] = TileXY(x,y+1);
			newtiles[2] = TileXY(x-1,y);
			newtiles[3] = TileXY(x,y-1);
			for (short inew = 0;inew < 4;++inew){
				if (newtiles[inew] < MapSize() &&
				TRD::IsRoutable(newtiles[inew]) &&
				DoNotAdd.count(newtiles[inew]) == 0 &&
				TRD::IsPassable(tile,newtiles[inew])){
					if (NULL == TRD::GetRegion(newtiles[inew])){
						if (m_num_tiles < numtiles){
							/*Add to the queue*/
							Unexplored.push(newtiles[inew]);
							DoNotAdd.insert(newtiles[inew]);
							++(this->m_num_tiles);
						}
					}
					else{
						if (TRD::GetRegion(newtiles[inew]) != this){
							/*This neighbouring tile is in another region. We found a link*/
							this->Neighbours()->Add(TRD::GetRegion(newtiles[inew]));
						}
					}
				}
			}
		}
		assert(this->m_num_tiles == this->m_tile_store.NumberOfTiles());

		/*We have all the tiles now work out the center one*/
		TileIndex avg = TileXY(sum_x/m_num_tiles,sum_y/m_num_tiles);
		if (TRD::GetRegion(avg) != this){
			this->m_center_tile = this->GetTiles()[this->NumTiles() >> 1];
			m_center_bad = true;
		}
		else{
			this->m_center_tile = avg;
			m_center_bad = false;
		}
	}

	private:
	static uint16 FindAndReserveFreeID()
	{
		/*Zero is reserved for NULL*/
		for (uint i = 1; i < m_used_region_indices.size(); ++i){
			if (!m_used_region_indices[i]){
				m_used_region_indices.resize(i+2 > m_used_region_indices.size() ? i+32 : m_used_region_indices.size());
				m_region_index.resize(i+2 > m_region_index.size() ? i+10 : m_region_index.size());
				m_used_region_indices[i] = true;
				assert(m_region_index.size() > i);
				assert(m_used_region_indices.size() > i);
				return i;
			}
		}
		/*There were no free id's*/
		error("Regions: Too many regions");
	}

	static void ReserveID(uint16 id_in)
	{
		uint id = id_in;
		m_used_region_indices.resize(id+2 > m_used_region_indices.size() ? id+32 : m_used_region_indices.size());
		m_region_index.resize(id+2 > m_region_index.size() ? id+10 : m_region_index.size());
		
		/*This ID was already taken*/
		assert(!m_used_region_indices[id]);
		assert(m_region_index.size() > id);
		m_used_region_indices[id] = true;
	}

	static void FreeID(uint16 id)
	{
		assert(m_used_region_indices.size() > id);
		assert(m_used_region_indices[id]);
		m_used_region_indices[id] = false;
	}
};

/*Static instance of index
  GCC wants this here - it may be different for other compilers*/
template<class TRegionDescription> vector<CRegion<TRegionDescription>*> CRegion<TRegionDescription>::m_region_index;
template<class TRegionDescription> vector<bool> CRegion<TRegionDescription>::m_used_region_indices = vector<bool>(100);

#endif
