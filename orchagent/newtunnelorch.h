#ifndef SWSS_NEWTUNNELORCH_H
#define SWSS_NEWTUNNELORCH_H

#include <arpa/inet.h>
#include <unordered_set>

#include "orch.h"
#include "sai.h"
#include "ipaddress.h"
#include "ipaddresses.h"

class VRouterOrch : public Orch
{
public:
    VRouterOrch(DBConnector *db, string tableName) : Orch(db, tableName) {};

    bool isExist(const string& vrf_id) const;

private:
    void doTask(Consumer& consumer);

    unordered_set<string> m_vrfs;
};

class TunnelOrch : public Orch
{
public:
    TunnelOrch(DBConnector *db, string tableName, VRouterOrch* vrouter_orch) : Orch(db, tableName), m_vrouter_orch(vrouter_orch) {};

private:
    void doTask(Consumer& consumer);

    unordered_map<unsigned int, string> m_vxlan_vrf_mapping;
    VRouterOrch* m_vrouter_orch;
};

struct VxlanNexthop
{
    VxlanNexthop(const int _vxlan_id, const IpAddress& _ip) : vxlan_id(_vxlan_id), ip(_ip) {}

    IpAddress ip;
    unsigned int vxlan_id;
private:
    VxlanNexthop() {};
};

struct VRouterRoute
{
    VRouterRoute(const string& _vrf_id, const IpPrefix& _prefix) : vrf_id(_vrf_id), prefix(_prefix) {}

    string   vrf_id;
    IpPrefix prefix;

    bool operator<(const VRouterRoute& other) const
    {
        return tie(vrf_id, prefix) < tie(other.vrf_id, other.prefix);
    }
private:
    VRouterRoute() {};
}

class VRouterRoutesOrch : public Orch
{
public:
    TunnelOrch(DBConnector *db, string tableName, VRouterOrch* vrouter_orch) : Orch(db, tableName), m_vrouter_orch(vrouter_orch) {};

private:
    void doTask(Consumer& consumer);

    unordered_map<VRouterRoute, VxlanNexthop> m_routing_table;
    VRouterOrch* m_vrouter_orch;
};

#endif //SWSS_NEWTUNNELORCH_H