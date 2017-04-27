#include <assert.h>
#include <iostream>
#include <tuple>
#include <vector>

#include "logger.h"
#include "fdborch.h"

extern sai_fdb_api_t *sai_fdb_api;

FdbOrch::FdbOrch(DBConnector *db, string tableName, PortsOrch *port) :
        Orch(db, tableName),
        m_portsOrch(port)
{
}

void FdbOrch::update(sai_fdb_event_t type, const sai_fdb_entry_t* entry, sai_object_id_t portOid)
{
    SWSS_LOG_ENTER();

    FdbUpdate update;
    update.entry.mac = entry->mac_address;
    update.entry.vlan = entry->vlan_id;

    switch (type)
    {
    case SAI_FDB_EVENT_LEARNED:
        if (!m_portsOrch->getPort(portOid, update.port))
        {
            SWSS_LOG_ERROR("Failed to get port for %lu OID\n", portOid);
            return;
        }

        update.add = true;

        (void)m_entries.insert(update.entry);
        break;
    case SAI_FDB_EVENT_AGED:
    case SAI_FDB_EVENT_FLUSHED:

        update.add = false;

        (void)m_entries.erase(update.entry);
        break;
    }

    for (auto observer: m_observers)
    {
        observer->update(SUBJECT_TYPE_FDB_CHANGE, static_cast<void *>(&update));
    }
}

bool FdbOrch::getPort(const MacAddress& mac, uint16_t vlan, Port& port)
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t entry;
    memcpy(entry.mac_address, mac.getMac(), sizeof(sai_mac_t));
    entry.vlan_id = vlan;

    sai_attribute_t attr;
    attr.id = SAI_FDB_ENTRY_ATTR_PORT_ID;

    sai_status_t status = sai_fdb_api->get_fdb_entry_attribute(&entry, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_INFO("Failed to get port for FDB entry OID\n");
        return false;
    }

    if (!m_portsOrch->getPort(attr.value.oid, port))
    {
        SWSS_LOG_ERROR("Failed to get port for %lu OID\n", attr.value.oid);
        return false;
    }

    return true;
}

void FdbOrch::doTask(Consumer& consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;

        string key = kfvKey(t);
        string op = kfvOp(t);

        FdbEntry entry;
        if (!splitKey(key, entry)) // remove this entry from app_db. it's invalid
        {
            it = consumer.m_toSync.erase(it);
            continue;
        }

        if (op == SET_COMMAND)
        {
            string port;
            string type;

            for (auto i : kfvFieldsValues(t))
            {
                if (fvField(i) == "port")
                    port = fvValue(i);

                if (fvField(i) == "type")
                    type = fvValue(i);
            }

            // check that type either static or dynamic
            if (type != "static" || type != "dynamic")
            {
                it = consumer.m_toSync.erase(it);
                continue;
            }

            // FIXME: check that port is available port

            if (addFdbEntry(entry, port, type))
                it = consumer.m_toSync.erase(it);
            else
                it++;
        }
        else if (op == DEL_COMMAND)
        {
            if (removeFdbEntry(entry))
                it = consumer.m_toSync.erase(it);
            else
                it++;

        }
        else
        {
            SWSS_LOG_ERROR("Unknown operation type %s\n", op.c_str());
            it = consumer.m_toSync.erase(it);
        }
    }
}

bool FdbOrch::addFdbEntry(const FdbEntry& entry, const string& port_name, const string& type)
{
    SWSS_LOG_ENTER();

    if (m_entries.count(entry) != 0) // we already have such entries
    {
        // FIXME: should we check that the entry are moving to another port?
        // FIXME: should we check that the entry are changing its type
        SWSS_LOG_ERROR("FDB entry already exists. mac=%s vlan=%s\n", mac_address.to_string(), vlan);
        return true;
    }

    sai_status_t status;

    sai_fdb_entry_t fdb_entry;
    memcpy(fdb_entry.mac_address, mac_address.getMac(), sizeof(sai_mac_t));
    fdb_entry.vlan_id = vlan;

    // Get port id
    Port port;
    if (!m_portsOrch->getPort(port_name, port))
    {
        SWSS_LOG_ERROR("Failed to get port for %s\n", port_name);
        return false;
    }

    sai_attribute_t attr;
    vector<sai_attribute_t> attrs;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value. = (type == 'dynamic') ? SAI_FDB_ENTRY_DYNAMIC : SAI_FDB_ENTRY_STATIC;
    attrs.push_back(attr);

    attr.id = SAI_FDB_ENTRY_ATTR_PORT_ID;
    attr.value. = port.m_port_id;
    attrs.push_back(attr);

    attr.id = SAI_FDB_ENTRY_ATTR_PACKET_ACTION;
    attr.value. = SAI_PACKET_ACTION_FORWARD; // FIXME: what to use here?
    attrs.push_back(attr);

    status = sai_fdb_api->create_fdb_entry(&fdb_entry, attrs.size(), attrs.data());
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_INFO("Failed to remove FDB entry\n");
        return false; // retry it
    }

    (void)m_entries.insert(new_entry);

    return true;
}

bool FdbOrch::removedbEntry(const FdbEntry& entry)
{
    SWSS_LOG_ENTER();

    if (m_entries.count(entry) == 0)
    {
        SWSS_LOG_ERROR("FDB entry doesn't found. mac=%s vlan=%s\n", mac_address.to_string(), vlan);
        return true;
    }

    // FIXME: What if the entry was created by ASIC learning process Should we remove it?

    sai_status_t status;
    sai_fdb_entry_t fdb_entry;
    memcpy(fdb_entry.mac_address, mac_address.getMac(), sizeof(sai_mac_t));
    fdb_entry.vlan_id = vlan;

    status = sai_fdb_api->remove_fdb_entry(&fdb_entry);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_INFO("Failed to remove FDB entry\n");
        return false; // retry it
    }

    m_entries.erase(entry);

    return true;
}

bool FdbOrch::splitKey(const string& key, FdbEntry& entry)
{
    SWSS_LOG_ENTER();

    string mac_address_str;

    size_t found = key.rfind(':');
    if (found == string::npos)
    {
        SWSS_LOG_ERROR("Failed to parse task key: %s\n", key.c_str());
        return false;
    }

    mac_address_str = key.substr(0, found);
    vlan = key.substr(found + 1, string::npos); // FIXME: what if we have "macaddress:"

    // check that mac_address is valid
    if (mac_address.parseMacString(mac_address_str))
    {
        SWSS_LOG_ERROR("Failed to parse mac address: %s\n", mac_address_str.c_str());
        return false;
    }

    // check that vlan is available
    Port port;
    if (!m_portsOrch->getPort(vlan, port))
    {
        SWSS_LOG_ERROR("Failed to get port for %s\n", vlan);
        return false;
    }

    entry.mac = mac_address;
    entry.vlan = stoi(vlan.substr(4)); // FIXME: create swss-common function to

    return true;
}
