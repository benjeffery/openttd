#ifndef  REGION_NEIGHBOURS_HPP
#define  REGION_NEIGHBOURS_HPP

#include <set>
using std::set;

template<class TRegion>
class CRegionNeighbours
{
	private:
	set<TRegion*>	m_neighbours;
	TRegion		*m_parent_region;

	public:
	inline TRegion*& OwnerRegion()
	{
		return m_parent_region;
	}

	inline TRegion *OwnerRegion() const
	{
		return m_parent_region;
	}

	inline const set<TRegion*>& Get() const
	{
		return this->m_neighbours;
	}

	inline void Add(TRegion *region)
	{
		this->m_neighbours.insert(region);
		region->Neighbours()->m_neighbours.insert(this->OwnerRegion());
	}

	inline void Remove(TRegion *region)
	{
		this->m_neighbours.erase(region);
		region->Neighbours()->m_neighbours.erase(this->OwnerRegion());
	}
	inline void DestroyConnections()
	{
		while(!m_neighbours.empty()){
			this->Remove(*m_neighbours.begin());
		}
	}
	inline void Clear()
	{
		this->m_neighbours.clear();
	}
	
	inline bool Has(TRegion *region) const
	{
		return this->m_neighbours.count(region) > 0;
	}

	inline bool Empty() const
	{
		return m_neighbours.empty();
	}
};

#endif

