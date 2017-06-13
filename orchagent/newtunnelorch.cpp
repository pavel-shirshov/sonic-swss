#include <string.h>
#include "newtunnelorch.h"
#include "logger.h"
#include "swssnet.h"

void VRouterOrch::doTask(Consumer& consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;

        string key = kfvKey(t);
        string op = kfvOp(t);

        auto key_elements = tokenize(key);
        //FIXME: check number of elements
        auto vrf_id = key_elements[1];

        if (op == SET_COMMAND)
        {
            if(!isExist(vrf_id))
            {
                m_vrfs.insert(vrf_id);
            }
            else
            {
                SWSS_LOG_ERROR("Cannot create vrf with vrf_id:'%s'. It exists already", vrf_id);
            }
        }
        else if (op == DEL_COMMAND)
        {
            if(isExist(vrf_id))
            {
                m_vrfs.erase(vrf_id);
            }
            else
            {
                SWSS_LOG_ERROR("Cannot remove vrf with vrf_id:'%s'. It doesn't exist", vrf_id);
            }
        }

        it = consumer.m_toSync.erase(it);
    }
}

bool VRouterOrch::isExist(const string& vrf_id) const
{
    return m_vrfs.find(vrf_id) != m_vrfs.end();
}

void TunnelOrch::doTask(Consumer& consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;

        string key = kfvKey(t);
        string op = kfvOp(t);

        auto key_elements = tokenize(key);
        //FIXME: check number of elements

        auto direction = key_elements[1];
        if (direction != "encapsulation" || direction != "decapsulation")
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. Tunnel_table direction could be either 'encapsulation' or 'decapsulation'", key);
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto type = key_elements[2];
        if (type != "vxlan")
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. Tunnel_table type could be 'vxlan' only", key);
            it = consumer.m_toSync.erase(it);
            continue;
        }

        if (direction == "encapsulation" && key_elements.size < 3)
        {
            SWSS_LOG_ERROR("Invalid key: '%s'. vxlan_id is required", key);
            it = consumer.m_toSync.erase(it);
            continue;
        }

        auto vxlan_id_str = key_elements[3];
        // FIXME: check that vxlan_id is a number
        auto vxlan_id = atoi(vxlan_id_str.c_str());

        string vrf_id;
        string local_termination_ip;
        if (op == SET_COMMAND)
        {
            for (auto i : kfvFieldsValues(t))
            {
                if (fvField(i) == "vrf_id" && direction == "encapsulation")
                {
                    vrf_id = fvValue(i);
                    // FIXME: check that vrf_id is exist in vrouter_table
                    if(!isExist(vxlan_id))
                    {
                        m_vxlan_vrf_mapping[vxlan_id] = vrf_id;
                    }
                    else
                    {
                        // FIXME:
                        SWSS_LOG_ERROR("Cannot create vrf with vrf_id:'%s'. It exists already", vrf_id);
                    }
                    break;
                }
                else if (fvField(i) == "local_termination_ip" && direction == "decapsulation")
                {
                    local_termination_ip = fvValue(i);
                    // FIXME: check that local_termination_ip is an ip address
                    // FIXME: do the action if it wasn't done before
                    break;
                }
                else
                {
                    // FIXME:
                    SWSS_LOG_ERROR("Cannot create vrf with vrf_id:'%s'. It exists already", vrf_id);
                }
            }
        }
        else if (op == DEL_COMMAND)
        {
            if(direction == "encapsulation" && isExist(vxlan_id))
            {
                m_vxlan_vrf_mapping.erase(vxlan_id);
            }
            else if (direction == "decapsulation")
            {
                // FIXME: do something
            }
            else
            {
                // FIXME:
                SWSS_LOG_ERROR("Cannot create vrf with vrf_id:'%s'. It exists already", vrf_id);
            }
        }

        it = consumer.m_toSync.erase(it);
    }
}

bool TunnelOrch::isExist(const int vxlan_id) const
{
    return m_vxlan_vrf_mapping.find(vxlan_id) != m_vxlan_vrf_mapping.end();
}

void VRouterOrch::doTask(Consumer& consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;

        string key = kfvKey(t);
        string op = kfvOp(t);

        auto key_elements = tokenize(key);
        // FIXME: check the number of arguments
        auto vrf_id = key_elements[1];
        // FIXME: check that vrf_id exists
        auto ip_prefix = key_elements[2];
        // FIXME: check that ip_prefix is ok

        string nexthop_type;
        bool nexthop_type_defined = false;
        IpAddress nexthop;
        bool nexthop_defined = false;
        unsigned int vxlan_id;
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
                        SWSS_LOG_ERROR("Invalid nexthop_type: '%s'. nexthop_type should be vxlan", nexthop_type);
                        break;
                    }
                    nexthop_type_defined = true;
                }
                else if (fvField(i) == "nexthop")
                {
                    string nexthop_str = fvValue(i);
                    // FIXME: check that value is an ipaddress
                    nexthop = IpAddress(nexthop_str);
                    nexthop_defined = true;
                }
                else if (fvField(i) == "vxlan_id")
                {
                    string vxlan_id_str = fvValue(i);
                    // FIXME: check that value is an interger
                    vxlan_id = atoi(vxlan_id_str.c_str());
                    vxlan_id_defined = true;
                }
                else
                {
                    SWSS_LOG_ERROR("Wrong attribute: '%s'", fvField(i));
                    break;
                }
            }
            bool completed = nexthop_defined && nexthop_defined && vxlan_id_defined;
            if (completed)
            {
                m_routing_table[VRouterRoute(vrf_id, ip_prefix)] = VxlanNexthop(vxlan_id, nexthop);
                // FIXME: install route to SAI
                // FIXME: what if the route is existed
            }
            else
            {
                SWSS_LOG_ERROR("Not complete");
            }
        }
        else if (op == DEL_COMMAND)
        {
            if (isExist(VRouterRoute(vrf_id, ip_prefix)))
            {
                m_routing_table.erase(VRouterRoute(vrf_id, ip_prefix));
                //FIXME: Remove it from the SAI
            }
            else
            {
                SWSS_LOG_ERROR("Virtual route: (%s: %s) doesn't exist", vrf_id.c_str(), )
            }
        }

        it = consumer.m_toSync.erase(it);
    }
}

VRouterRoutesOrch::isExist(const VRouterRoute& route) const
{
    return m_routing_table.find(route) != m_routing_table.end();
}
