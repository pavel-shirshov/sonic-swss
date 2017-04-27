#ifndef SWSS_FDBORCH_H
#define SWSS_FDBORCH_H

#include "orch.h"
#include "observer.h"
#include "portsorch.h"

struct FdbEntry
{
    MacAddress mac;
    sai_vlan_id_t vlan;
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
    bool addFdbEntry(string&, string&, string&, string&);
    bool removedbEntry(string&, string&);
    bool splitKey(string&, string&, string&);
};

#endif /* SWSS_FDBORCH_H */
