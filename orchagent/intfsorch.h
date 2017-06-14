#ifndef SWSS_INTFSORCH_H
#define SWSS_INTFSORCH_H

#include "orch.h"
#include "portsorch.h"

#include "ipaddresses.h"
#include "ipprefix.h"
#include "macaddress.h"

#include <map>
#include <set>

extern sai_object_id_t gVirtualRouterId;
extern MacAddress gMacAddress;

struct IntfsEntry
{
    std::set<IpPrefix>  ip_addresses;
    int                 ref_count;
};

typedef map<string, IntfsEntry> IntfsTable;

class IntfsOrch : public Orch
{
public:
    IntfsOrch(DBConnector *db, string tableName, VRouterOrch *vrouter_orch, TunnelOrch *tunnel_orch);

    sai_object_id_t getRouterIntfsId(const string&);

    void increaseRouterIntfsRefCount(const string&);
    void decreaseRouterIntfsRefCount(const string&);
    bool hasVlanIdbyVrf(const string& vrf_id) const;
    unsigned int getVlanIdbyVrf(const string& vrf_id) const;
private:
    IntfsTable m_syncdIntfses;
    map<vrf_id, unsigned short> m_vrf2vlan;
    map<unsigned short, vrf_id> m_vlan2vrf;
    VRouterOrch *m_vrouter_orch;
    TunnelOrch *m_tunnel_orch;

    void doTask(Consumer &consumer);

    int getRouterIntfsRefCount(const string&);

    bool addRouterIntfs(Port &port);
    bool removeRouterIntfs(Port &port);

    void addSubnetRoute(const Port &port, const IpPrefix &ip_prefix);
    void removeSubnetRoute(const Port &port, const IpPrefix &ip_prefix);

    void addIp2MeRoute(const IpPrefix &ip_prefix);
    void removeIp2MeRoute(const IpPrefix &ip_prefix);
};

#endif /* SWSS_INTFSORCH_H */
