#include <iostream>
#include <team.h>
#include "logger.h"
#include "select.h"
#include "netdispatcher.h"
#include "netlink.h"
#include "teamsync.h"

using namespace std;
using namespace swss;

int main(int argc, char **argv)
{
    swss::Logger::linkToDbNative("teamsyncd");
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("fast-reboot. Starting");
    DBConnector db(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    Select s;
    TeamSync sync(&db, &s);

    NetDispatcher::getInstance().registerMessageHandler(RTM_NEWLINK, &sync);
    NetDispatcher::getInstance().registerMessageHandler(RTM_DELLINK, &sync);

    while (1)
    {
        try
        {
            NetLink netlink;

            netlink.registerGroup(RTNLGRP_LINK);
            cout << "Listens to teamd events..." << endl;
            netlink.dumpRequest(RTM_GETLINK);

            s.addSelectable(&netlink);
            SWSS_LOG_NOTICE("fast-reboot. main loop");
            while (true)
            {
                Selectable *temps;
                int tempfd;
                s.select(&temps, &tempfd);
            }
        }
        catch (const std::exception& e)
        {
            cout << "Exception \"" << e.what() << "\" had been thrown in deamon" << endl;
            return 0;
        }
    }

    return 1;
}
