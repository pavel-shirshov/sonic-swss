#ifndef SWSS_NEWTUNNELORCH_H
#define SWSS_NEWTUNNELORCH_H

#include <arpa/inet.h>
#include <set>
#include <map>

#include "intfsorch.h"
#include "orch.h"
#include "sai.h"
#include "ipaddress.h"
#include "ipprefix.h"
#include "ipaddresses.h"

class IntfsOrch;

class VRouterOrch : public Orch
{
public:
    VRouterOrch(DBConnector *db, string tableName) : Orch(db, tableName) {};

    bool isExist(const string& vrf_id) const;
    void incrRefCounter(const string& vrf_id);
    void decrRefCounter(const string& vrf_id);

private:
    void doTask(Consumer& consumer);

    map<string, unsigned int> m_vrfs;
};

class TunnelOrch : public Orch
{
public:
    TunnelOrch(DBConnector *db, string tableName, VRouterOrch* vrouter_orch) : Orch(db, tableName),
                                                                               m_vrouter_orch(vrouter_orch),
                                                                               m_decapsulation_is_set(false) {};

    bool isExist(const unsigned int vxlan_id) const;
    bool hasPair(const unsigned int vxlan_id, const string& vrf_id) const;

    void incrRefCounter(const unsigned int vxlan_id);
    void decrRefCounter(const unsigned int vxlan_id);
private:
    void doTask(Consumer& consumer);

    map<unsigned int, string> m_vxlan_vrf_mapping;
    map<unsigned int, unsigned int> m_refcounters;
    VRouterOrch* m_vrouter_orch;
    bool m_decapsulation_is_set;
};

struct VxlanNexthop
{
    VxlanNexthop(const int _vxlan_id, const IpAddress& _ip) : vxlan_id(_vxlan_id), ip(_ip) {}

    IpAddress ip;
    unsigned int vxlan_id;
//private:
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
};

class VRouterRoutesOrch : public Orch
{
public:
    VRouterRoutesOrch(DBConnector *db, string tableName, VRouterOrch* vrouter_orch, IntfsOrch *intfs_orch, TunnelOrch* tunnel_orch) : Orch(db, tableName),
                                                                                                             m_vrouter_orch(vrouter_orch),
                                                                                                             m_intfs_orch(intfs_orch),
                                                                                                             m_tunnel_orch(tunnel_orch) {};
    bool isExist(const VRouterRoute& route) const;
    bool addVRoute(const VRouterRoute& route, const VxlanNexthop& vnexthop);
    bool removeVRoute(const VRouterRoute& route);

private:
    void doTask(Consumer& consumer);

    map<VRouterRoute, VxlanNexthop> m_routing_table;
    VRouterOrch* m_vrouter_orch;
    IntfsOrch *m_intfs_orch;
    TunnelOrch* m_tunnel_orch;
};

#endif //SWSS_NEWTUNNELORCH_H
