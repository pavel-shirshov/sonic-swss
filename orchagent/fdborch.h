#ifndef SWSS_FDBORCH_H
#define SWSS_FDBORCH_H

#include "orch.h"
#include "observer.h"
#include "portsorch.h"

struct FdbEntry
{
    MacAddress mac;
    sai_vlan_id_t vlan;
    bool operator<(const FdbEntry& other) const
    {
        return (mac < other.mac) && (vlan < other.vlan);
    }
};

struct FdbUpdate
{
    FdbEntry entry;
    Port port;
    bool add;
};

class FdbOrch: public Orch, public Subject
{
public:
    FdbOrch(DBConnector *, string, PortsOrch *);

    void update(sai_fdb_event_t, const sai_fdb_entry_t *, sai_object_id_t);
    bool getPort(const MacAddress&, uint16_t, Port&);

private:
    PortsOrch *m_portsOrch;
    set<FdbEntry> m_entries;

    void doTask(Consumer& consumer);

    bool addFdbEntry(const FdbEntry&, const string&, const string&);
    bool removeFdbEntry(const FdbEntry&);
    bool splitKey(const string&, FdbEntry&);
};

#endif /* SWSS_FDBORCH_H */
