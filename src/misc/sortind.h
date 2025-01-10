#ifndef  MC_CORE_SORTIND
#define  MC_CORE_SORTIND

struct SSortableIndex
{
	SSortableIndex() {};
	SSortableIndex( int i, float v ) { idx = i; val = v; }
	int idx;
	float val;
	bool operator<( const SSortableIndex &oth) const
	{
		return val < oth.val;
	}
};

#endif
