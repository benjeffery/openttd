#include "region_tile_store.h"
#include "../../stdafx.h"
#include "../../tile_map.h"
#include "../../openttd.h"

#include <vector>
using std::vector;

void CRegionTileStore::Init(uint size_x, uint size_y, TileIndex middle)
{
	this->Clear();
	/*Work out our bounds*/
	m_left_x = TileX(middle) - (size_x/2);
	if (m_left_x > TileX(middle)) m_left_x = size_x/2;
	m_size_x = size_x;
	m_bottom_y = TileY(middle) - (size_y/2);
	if (m_bottom_y > TileY(middle)) m_bottom_y = size_y/2;
	m_size_y = size_y;

	/*Reset the storage to the right size*/
	m_storage.resize(size_y,vector<bool>(size_x));
}

bool CRegionTileStore::Has(TileIndex tile) const
{
	if (this->Contains(tile))
		return this->GetStorage(tile);
	else
		return false;
}

void CRegionTileStore::Add(TileIndex tile)
{
	if (!this->Contains(tile))
		this->ResizeToContain(tile);
	this->SetStorage(tile,true);
}

void CRegionTileStore::Remove(TileIndex tile)
{
	assert(this->Contains(tile));
	this->SetStorage(tile,false);
}

vector<TileIndex> CRegionTileStore::GetTiles() const
{
	vector<TileIndex> tiles;
	/*Loop over storage adding if we find one*/
	for (uint16 y = 0; y < m_size_y; ++y)
		for (uint16 x = 0; x < m_size_x; ++x)
			if (m_storage[y][x])
				tiles.push_back(TileXY(x+m_left_x,y+m_bottom_y));

	return tiles;
}

void CRegionTileStore::Clear()
{
	m_size_x = 0;
	m_size_y = 0;
	m_storage.clear();
}

uint16 CRegionTileStore::NumberOfTiles() const
{
	uint16 count = 0;
	/*Loop over storage adding if we find one*/
	for (uint16 y = 0; y < m_size_y; ++y)
		for (uint16 x = 0; x < m_size_x; ++x)
			count += m_storage[y][x];
	return count;
}

void CRegionTileStore::ResizeToContain(TileIndex tile)
{
	if (this->Contains(tile)) return;
	/*Y resizing*/
	/*Tile's y is above us*/
	if (TileY(tile) >= m_bottom_y+m_size_y){
		m_size_y += TileY(tile) - (m_bottom_y+m_size_y) + 1;
		m_storage.resize(m_size_y,vector<bool>(m_size_x));
	}
	/*Tile's y is below us*/
	if (TileY(tile) < m_bottom_y){
		uint16 extra = m_bottom_y - TileY(tile);
		m_storage.insert(m_storage.begin(),extra,vector<bool>(m_size_x));
		m_size_y += extra;
		m_bottom_y = TileY(tile);
	}
	/*X resizing*/
	/*Tile's x is right of us*/
	if (TileX(tile) >= m_left_x+m_size_x){
		m_size_x += TileX(tile) - (m_left_x+m_size_x) + 1;
		for (uint16 i=0;i<m_size_y;++i)
			m_storage[i].resize(m_size_x,false);
	}
	/*Tile's x is left of us*/
	if (TileX(tile) < m_left_x){
		uint16 extra = m_left_x - TileX(tile);
		for (uint16 i=0;i<m_size_y;++i)
			m_storage[i].insert(m_storage[i].begin(),extra,false);
		m_size_x += extra;
		m_left_x= TileX(tile);
	}
	assert(this->Contains(tile));	
}

bool CRegionTileStore::Contains(TileIndex tile) const
{
	return (TileX(tile) < m_left_x+m_size_x && TileX(tile) >= m_left_x && TileY(tile) < m_bottom_y + m_size_y && TileY(tile) >= m_bottom_y);
}

void CRegionTileStore::SetStorage(TileIndex tile,bool value)
{
	assert(this->Contains(tile));
	m_storage[TileY(tile)-m_bottom_y][TileX(tile)-m_left_x] = value;
}

bool CRegionTileStore::GetStorage(TileIndex tile) const
{
	assert(this->Contains(tile));
	return m_storage[TileY(tile)-m_bottom_y][TileX(tile)-m_left_x];
}

uint CRegionTileStore::WidthX() const
{
	vector<TileIndex> tiles = this->GetTiles();
	if (tiles.empty()) return 0;

	uint lowest_x = TileX(tiles[0]);
	uint highest_x = TileX(tiles[0]);

	for (uint16 i = 0; i < tiles.size(); ++i)
	{
		lowest_x = (TileX(tiles[i]) < lowest_x ? TileX(tiles[i]) : lowest_x);
		highest_x = (TileX(tiles[i]) > highest_x ? TileX(tiles[i]) : highest_x);
	}
	return highest_x - lowest_x + 1;
}

uint CRegionTileStore::HeightY() const
{
	vector<TileIndex> tiles = this->GetTiles();
	if (tiles.empty()) return 0;

	uint lowest_y = TileY(tiles[0]);
	uint highest_y = TileY(tiles[0]);

	for (uint16 i = 0; i < tiles.size(); ++i)
	{
		lowest_y = (TileY(tiles[i]) < lowest_y ? TileY(tiles[i]) : lowest_y);
		highest_y = (TileY(tiles[i]) > highest_y ? TileY(tiles[i]) : highest_y);
	}
	return highest_y - lowest_y + 1;
}
