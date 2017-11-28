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
        return tie(mac, vlan) < tie(other.mac, other.vlan);
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
    FdbOrch(DBConnector *appDb, string tableName, DBConnector *stateDb, PortsOrch *port) :
        Orch(appDb, tableName),
        m_portsOrch(port),
        m_appFdbtable(Table(appDb, tableName)),
        m_stateVlanMemberTable(stateDb, STATE_VLAN_MEMBER_TABLE_NAME, CONFIGDB_TABLE_NAME_SEPARATOR)
    {
    }

    void update(sai_fdb_event_t, const sai_fdb_entry_t *, sai_object_id_t);
    bool getPort(const MacAddress&, uint16_t, Port&);

private:
    PortsOrch *m_portsOrch;
    set<FdbEntry> m_entries;
    Table m_appFdbtable;
    Table m_stateVlanMemberTable;

    void doTask(Consumer& consumer);

    bool addFdbEntry(const FdbEntry&, const string&, const string&);
    bool removeFdbEntry(const FdbEntry&);
    bool isVlanMemberReady(int, const string&);
};

#endif /* SWSS_FDBORCH_H */
