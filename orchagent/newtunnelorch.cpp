#include <string.h>
#include "newtunnelorch.h"
#include "logger.h"
#include "swssnet.h"
#include "tokenize.h"

void VRouterOrch::doTask(Consumer& consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;

        auto key = kfvKey(t);
        auto op = kfvOp(t);

        auto key_elements = tokenize(key, ':');

        if (key_elements.size() == 1)
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. No vrf_id", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }
        if (key_elements.size() > 2)
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. Invalid vrf_id. ':' is not allowed in vrf_id", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto vrf_id = key_elements[1];

        if (op == SET_COMMAND)
        {
            if(!isExist(vrf_id))
            {
                m_vrfs[vrf_id] = 0;
                // REMOVE ME
                SWSS_LOG_ERROR("Doing action: add vrf: %s", vrf_id.c_str());
                // REMOVE ME
            }
            else
            {
                SWSS_LOG_ERROR("Cannot create vrf with vrf_id:'%s'. It exists already", vrf_id.c_str());
            }
        }
        else if (op == DEL_COMMAND)
        {
            if(isExist(vrf_id))
            {
                if (m_vrfs[vrf_id] == 0)
                {
                    m_vrfs.erase(vrf_id);
                    // REMOVE ME
                    SWSS_LOG_ERROR("Doing action: remove vrf: %s", vrf_id.c_str());
                    // REMOVE ME
                }
                else
                {
                    SWSS_LOG_ERROR("Cannot remove vrf with vrf_id:'%s'. There're %d references to it", vrf_id.c_str(), m_vrfs[vrf_id]);
                }
            }
            else
            {
                SWSS_LOG_ERROR("Cannot remove vrf with vrf_id:'%s'. It doesn't exist", vrf_id.c_str());
            }
        }

        it = consumer.m_toSync.erase(it);
    }
}

bool VRouterOrch::isExist(const string& vrf_id) const
{
    return m_vrfs.find(vrf_id) != m_vrfs.end();
}

void VRouterOrch::incrRefCounter(const string& vrf_id)
{
    if (isExist(vrf_id))
    {
        m_vrfs[vrf_id]++;
    }
}

void VRouterOrch::decrRefCounter(const string& vrf_id)
{
    if (isExist(vrf_id))
    {
        m_vrfs[vrf_id]--;
    }
}

void TunnelOrch::doTask(Consumer& consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;

        auto key = kfvKey(t);
        auto op = kfvOp(t);

        auto key_elements = tokenize(key, ':');
        if (key_elements.size() < 3)
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. It must contain encapsulation direction and tunnel type", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto direction = key_elements[1];
        if (direction != "encapsulation" && direction != "decapsulation")
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. Tunnel_table direction must be either 'encapsulation' or 'decapsulation'", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto type = key_elements[2];
        if (type != "vxlan")
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. Tunnel_table type must be 'vxlan'", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        if (direction == "encapsulation" && key_elements.size() < 4)
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. vxlan_id is required", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto vxlan_id_str = key_elements[3];
        // FIXME: check that vxlan_id is a number
        auto vxlan_id = stoi(vxlan_id_str);

        if (op == SET_COMMAND)
        {
            for (auto i : kfvFieldsValues(t))
            {
                if (fvField(i) == "vrf_id" && direction == "encapsulation")
                {
                    auto vrf_id = fvValue(i);
                    if (!m_vrouter_orch->isExist(vrf_id))
                    {
                        SWSS_LOG_ERROR("vrf_id '%s' doesn't exist.", vrf_id.c_str());
                        break;
                    }

                    if (!isExist(vxlan_id))
                    {
                        m_vxlan_vrf_mapping[vxlan_id] = vrf_id;
                        m_vrouter_orch->incrRefCounter(vrf_id);
                        // REMOVE ME
                        SWSS_LOG_ERROR("Doing action: set map between vxlan_id and vrf_id: %d - %s", vxlan_id, vrf_id.c_str());
                        // REMOVE ME
                    }
                    else
                    {
                        SWSS_LOG_ERROR("vxlan_id '%d' exists already", vxlan_id);
                        break;
                    }
                    break;
                }
                else if (fvField(i) == "local_termination_ip" && direction == "decapsulation")
                {
                    IpAddress local_termination_ip;
                    auto local_termination_ip_str = fvValue(i);

                    try
                    {
                        local_termination_ip = IpAddress(local_termination_ip);
                    }
                    catch (invalid_argument& err)
                    {
                        SWSS_LOG_ERROR("vxlan_id '%d' exists already", vxlan_id);
                        break;
                    }

                    if (!m_decapsulation_is_set)
                    {
                        // FIXME: do the action
                        // REMOVE ME
                        SWSS_LOG_ERROR("Doing action: set local local_termination_ip");
                        // REMOVE ME
                        m_decapsulation_is_set = true;
                    }
                    else
                    {
                        SWSS_LOG_ERROR("decapsulation parameters is already set for vxlan");
                        break;
                    }
                }
                else
                {
                    SWSS_LOG_ERROR("Unsupported attribute: '%s'", fvField(i).c_str());
                    break;
                }
            }
        }
        else if (op == DEL_COMMAND)
        {
            if (direction == "encapsulation")
            {
                if (isExist(vxlan_id))
                {
                    m_vxlan_vrf_mapping.erase(vxlan_id);
                    m_vrouter_orch->decrRefCounter(m_vxlan_vrf_mapping[vxlan_id]);
                    // REMOVE ME
                    SWSS_LOG_ERROR("Doing action: remove map between vxlan_id and vrf_id: %d - %s", vxlan_id, m_vxlan_vrf_mapping[vxlan_id].c_str());
                    // REMOVE ME
                }
                else
                {
                    SWSS_LOG_ERROR("encapsulation wasn't set for vxlan_id: '%d'", vxlan_id);
                    break;
                }
            }
            else if (direction == "decapsulation")
            {
                if (m_decapsulation_is_set)
                {
                    // FIXME: remove the entry
                    // REMOVE ME
                    SWSS_LOG_ERROR("Doing action: remove local local_termination_ip");
                    // REMOVE ME
                    m_decapsulation_is_set = false;
                }
                else
                {
                    SWSS_LOG_ERROR("decapsulation parameters wasn't set for vxlan");
                    break;
                }
            }
       }

        it = consumer.m_toSync.erase(it);
    }
}

bool TunnelOrch::isExist(const int vxlan_id) const
{
    return m_vxlan_vrf_mapping.find(vxlan_id) != m_vxlan_vrf_mapping.end();
}

void VRouterRoutesOrch::doTask(Consumer& consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;

        string key = kfvKey(t);
        string op = kfvOp(t);

        auto key_elements = tokenize(key, ':');
        if (key_elements.size() < 3)
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. vrf_id and ip_prefix should be presented", key.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto vrf_id = key_elements[1];
        if (m_vrouter_orch->isExist(vrf_id))
        {
            SWSS_LOG_ERROR("vrf_id '%s' is not presented", vrf_id.c_str());
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto ip_prefix_str = key_elements[2];
        IpPrefix ip_prefix;
        try
        {
            ip_prefix = IpPrefix(ip_prefix_str);
        }
        catch (invalid_argument& err)
        {
            SWSS_LOG_ERROR("Invalid ip prefix format");
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto vroute = VRouterRoute(vrf_id, ip_prefix);

        string nexthop_type;
        bool nexthop_type_defined = false;
        IpAddress nexthop;
        bool nexthop_defined = false;
        unsigned int vxlan_id = 0;
        bool vxlan_id_defined = false;

        if (op == SET_COMMAND)
        {
            for (auto i : kfvFieldsValues(t))
            {
                if (fvField(i) == "nexthop_type")
                {
                    nexthop_type = fvValue(i);
                    if (nexthop_type != "vxlan")
                    {
                        SWSS_LOG_ERROR("Invalid nexthop_type: '%s'. nexthop_type should be vxlan", nexthop_type.c_str());
                        break;
                    }
                    nexthop_type_defined = true;
                }
                else if (fvField(i) == "nexthop")
                {
                    string nexthop_str = fvValue(i);
                    try
                    {
                        nexthop = IpAddress(nexthop_str);
                    }
                    catch(invalid_argument& err)
                    {
                        SWSS_LOG_ERROR("Invalid ip address in 'nexthop' field: '%s'", nexthop_str.c_str());
                        break;
                    }
                    nexthop_defined = true;
                }
                else if (fvField(i) == "vxlan_id")
                {
                    string vxlan_id_str = fvValue(i);
                    // FIXME: check that value is an interger
                    vxlan_id = stoi(vxlan_id_str.c_str());
                    vxlan_id_defined = true;
                }
                else
                {
                    SWSS_LOG_ERROR("Wrong attribute: '%s'", fvField(i).c_str());
                    break;
                }
            }
            bool completed = nexthop_type_defined && nexthop_defined && vxlan_id_defined;
            if (completed)
            {
                if (!isExist(vroute)) // FIXME: support for update the route?
                {
                    m_routing_table[vroute] = VxlanNexthop(vxlan_id, nexthop);
                    m_vrouter_orch->incrRefCounter(vrf_id);
                    // FIXME: install route to SAI
                    // REMOVE ME
                    SWSS_LOG_ERROR("Doing action: add routing (%s,%s) -> (%d,%s)", vrf_id.c_str(), ip_prefix_str.c_str(), vxlan_id, nexthop.to_string().c_str());
                    // REMOVE ME
                }
                else
                {
                    SWSS_LOG_ERROR("Route (%d,%s) already exists", vxlan_id, nexthop.to_string().c_str());
                    break;
                }
            }
            else
            {
                SWSS_LOG_ERROR("Not complete");
            }
        }
        else if (op == DEL_COMMAND)
        {
            if (isExist(vroute))
            {
                m_routing_table.erase(vroute);
                m_vrouter_orch->decrRefCounter(vrf_id);
                //FIXME: Remove it from the SAI
                // REMOVE ME
                SWSS_LOG_ERROR("Doing action: remove routing (%s,%s)", vrf_id.c_str(), ip_prefix_str.c_str());
                // REMOVE ME
            }
            else
            {
                SWSS_LOG_ERROR("Virtual route: (%s: %s) doesn't exist", vrf_id.c_str(), nexthop.to_string().c_str());
            }
        }

        it = consumer.m_toSync.erase(it);
    }
}

bool VRouterRoutesOrch::isExist(const VRouterRoute& route) const
{
    return m_routing_table.find(route) != m_routing_table.end();
}
