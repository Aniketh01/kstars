/*  INDI Server Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <libindi/basedevice.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>

#include <QRadioButton>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QTreeWidget>
#include <QIcon>
#include <QDialog>
#include <QTcpServer>

#include <KMenu>
#include <KMessageBox>
#include <KPushButton>
#include <KLineEdit>
#include <KProcess>
#include <KAction>
#include <KActionCollection>
#include <KIconLoader>

#include <kstandarddirs.h>

#include "oal/log.h"
#include "oal/scope.h"

#include "ui_indihostconf.h"
#include "servermanager.h"
#include "guimanager.h"
#include "Options.h"
#include "drivermanager.h"
#include "driverinfo.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "driverinfo.h"
#include "indilistener.h"

#include <config-kstars.h>

#define INDI_MAX_TRIES  3
#define  ERRMSG_SIZE 1024

DriverManagerUI::DriverManagerUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);

    localTreeWidget->setSortingEnabled(false);
    localTreeWidget->setRootIsDecorated(true);

    clientTreeWidget->setSortingEnabled(false);

    runningPix = KIcon( "system-run" );
    stopPix    = KIcon( "dialog-cancel" );
    localMode  = KIcon( "computer" );
    serverMode = KIcon( "network-server" );

    connected           = KIcon( "network-connect" );
    disconnected        = KIcon( "network-disconnect" );

    connect(localTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(makePortEditable(QTreeWidgetItem*,int)));
}

void DriverManagerUI::makePortEditable(QTreeWidgetItem* selectedItem, int column)
{
    // If it's the port column, then make it user-editable
    if (column == ::DriverManager::LOCAL_PORT_COLUMN)
        selectedItem->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEditable|Qt::ItemIsEnabled);

    localTreeWidget->editItem(selectedItem, ::DriverManager::LOCAL_PORT_COLUMN);
}

DriverManager * DriverManager::_DriverManager = NULL;

DriverManager * DriverManager::Instance()
{
    if (_DriverManager == NULL)
        _DriverManager = new DriverManager();

    return _DriverManager;
}

DriverManager::DriverManager()
        : KDialog( KStars::Instance() )
{

    currentPort = Options::serverPortStart().toInt()-1;
    lastGroup = NULL;
    lastDevice = NULL;

    connectionMode = SERVER_CLIENT;

    ui = new DriverManagerUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Device Manager" ) );
    setButtons( KDialog::Close );

    lastGroup = NULL;

    QObject::connect(ui->addB, SIGNAL(clicked()), this, SLOT(addINDIHost()));
    QObject::connect(ui->modifyB, SIGNAL(clicked()), this, SLOT(modifyINDIHost()));
    QObject::connect(ui->removeB, SIGNAL(clicked()), this, SLOT(removeINDIHost()));

    QObject::connect(ui->connectHostB, SIGNAL(clicked()), this, SLOT(activateHostConnection()));
    QObject::connect(ui->disconnectHostB, SIGNAL(clicked()), this, SLOT(activateHostDisconnection()));
    QObject::connect(ui->runServiceB, SIGNAL(clicked()), this, SLOT(activateRunService()));
    QObject::connect(ui->stopServiceB, SIGNAL(clicked()), this, SLOT(activateStopService()));
    QObject::connect(ui->localTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(updateLocalTab()));
    QObject::connect(ui->clientTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(updateClientTab()));
    QObject::connect(ui->localTreeWidget, SIGNAL(expanded(const QModelIndex &)), this, SLOT(resizeDeviceColumn()));

    readXMLDrivers();

    readINDIHosts();

    updateCustomDrivers();


}

void DriverManager::processDeviceStatus(DriverInfo *dv)
{

   if (dv == NULL)
        return;

   QString currentDriver;
   ServerMode   mode = connectionMode;
   ServerManager *manager = dv->getServerManager();
   bool dState=false;
   bool cState=false;

   if (dv->getDriverSource() != HOST_SOURCE)
   {
       if (ui->localTreeWidget->currentItem())
            currentDriver = ui->localTreeWidget->currentItem()->text(LOCAL_NAME_COLUMN);

        foreach (QTreeWidgetItem *item, ui->localTreeWidget->findItems(dv->getTreeLabel(), Qt::MatchExactly |  Qt::MatchRecursive))
        {

                 item->setText(LOCAL_VERSION_COLUMN, dv->getVersion());

                 if (manager)
                     mode = manager->getMode();


                     dState = dv->getServerState();
                     cState = dv->getClientState() && dState;


                 switch (mode)
                 {
                    case SERVER_ONLY:
                     ui->runServiceB->setEnabled(!dState);
                     ui->stopServiceB->setEnabled(dState);
                     item->setIcon(LOCAL_STATUS_COLUMN, dState ? ui->runningPix : ui->stopPix);
                     if (dState)
                     {
                         item->setIcon(LOCAL_MODE_COLUMN, ui->serverMode);
                         if (manager)
                            item->setText(LOCAL_PORT_COLUMN, QString(manager->getPort()));
                     }
                     else
                     {
                         item->setIcon(LOCAL_MODE_COLUMN, QIcon());
                         item->setText(LOCAL_PORT_COLUMN, dv->getUserPort());
                     }

                     break;

                    case SERVER_CLIENT:
                     ui->runServiceB->setEnabled(!cState);
                     ui->stopServiceB->setEnabled(cState);
                     item->setIcon(LOCAL_STATUS_COLUMN, cState ? ui->runningPix : ui->stopPix);
                     if (cState)
                     {
                         item->setIcon(LOCAL_MODE_COLUMN, ui->localMode);

                         if (manager)
                         item->setText(LOCAL_PORT_COLUMN, QString(manager->getPort()));
                     }
                     else
                     {
                         item->setIcon(LOCAL_MODE_COLUMN, QIcon());
                         item->setText(LOCAL_PORT_COLUMN, dv->getUserPort());
                     }

                     break;
                 }

                 // Only update the log if the current driver is selected
                 if (currentDriver == dv->getTreeLabel())
                 {
                    ui->serverLogText->clear();
                    ui->serverLogText->append(dv->getServerBuffer());
                 }

        }
   }
   else
   {
       foreach (QTreeWidgetItem *item, ui->clientTreeWidget->findItems(dv->getName(), Qt::MatchExactly, HOST_NAME_COLUMN))
       {
           if (dv->getClientState())
           {
               item->setIcon(HOST_STATUS_COLUMN, ui->connected);
               ui->connectHostB->setEnabled(false);
               ui->disconnectHostB->setEnabled(true);
           }
           else
           {
               item->setIcon(HOST_STATUS_COLUMN, ui->disconnected);
               ui->connectHostB->setEnabled(true);
               ui->disconnectHostB->setEnabled(false);
           }
        }

  }

}

void DriverManager::getUniqueHosts(QList<DriverInfo*> & dList, QList < QList<DriverInfo *> > & uHosts)
{
    bool found=false;

    foreach(DriverInfo *dv, dList)
    {
       QList<DriverInfo *> uList;

        foreach(DriverInfo *idv, dList)
            if (dv->getHost() == idv->getHost() && dv->getPort() == idv->getPort())
            {

                // Check if running already
                if (dv->getClientState() || dv->getServerState())
                {
                  int ans = KMessageBox::warningContinueCancel(0, i18n("Driver %1 is already running, do you want to restart it?").arg(dv->getTreeLabel()));
                  if (ans == KMessageBox::Cancel)
                      continue;
                  else
                  {
                      QList<DriverInfo *> stopList;
                      stopList.append(dv);
                      stopDevices(stopList);
                  }
                }

                found = false;

                foreach(QList < DriverInfo *> qdi, uHosts)
                {
                    foreach(DriverInfo *di, qdi)
                    {
                        if (di == idv)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                if (found == false)
                    uList.append(idv);

            }

        if (uList.empty() == false)
            uHosts.append(uList);
    }

}

bool DriverManager::startDevices(QList<DriverInfo*> & dList)
{
    ServerManager *serverManager=NULL;
    ClientManager *clientManager=NULL;
    int port=-1;

    QList < QList<DriverInfo*> > uHosts;

    bool connectionToServer=false;

    getUniqueHosts(dList, uHosts);

    foreach(QList <DriverInfo *> qdv, uHosts)
    {

        if (qdv.empty())
            continue;

       //port = dv->getPort().toInt();
        port = qdv.at(0)->getPort().toInt();

       //qDebug() << "For host " << qdv.at(0)->getHost() << " which is equal to conversion values of " << port << endl;
        // Select random port within range is none specified.
        if (port == -1)
            port = getINDIPort(port);

         if (port <= 0)
         {
                 KMessageBox::error(0, i18n("Cannot start INDI server: port error."));
                 return false;
         }

        serverManager = new ServerManager(qdv.at(0)->getHost(), ((uint) port));

        if (serverManager == NULL)
        {
               kWarning() << "Warning: device manager has not been established properly";
               return false;
        }

        serverManager->setMode(connectionMode);

       connect(serverManager, SIGNAL(newServerLog()), this, SLOT(updateLocalTab()));
       //connect(serverManager, SIGNAL(serverFailure(ServerManager*)), this, SLOT(processServerTermination(ServerManager*)));



         if (serverManager->start())
           servers.append(serverManager);
         else
         {
             delete serverManager;
             return false;
         }


         foreach(DriverInfo *dv, qdv)
         {
              if (serverManager->startDriver(dv) == false)
              {
                  servers.removeOne(serverManager);
                  serverManager->stop();
                  delete serverManager;
                  return false;
              }
         }

         // Nothing to do more if SERVER ONLY
         if (connectionMode == SERVER_ONLY)
            continue;

         clientManager = new ClientManager();

         foreach(DriverInfo *dv, qdv)
            clientManager->appendManagedDriver(dv);


         connect(clientManager, SIGNAL(connectionFailure(ClientManager*)), this, SLOT(processClientTermination(ClientManager*)));
         //connect(clientManager, SIGNAL(newINDIDevice(DriverInfo*)), this, SIGNAL(newINDIDevice(DriverInfo*)));

         clientManager->setServer(qdv.at(0)->getHost().toLatin1().constData(), ((uint) port));

         for (int i=0; i < INDI_MAX_TRIES; i++)
         {
             connectionToServer= clientManager->connectServer();

             if (connectionToServer)
                 break;

              usleep(100000);
         }

         if (connectionToServer)
         {

             clients.append(clientManager);
             GUIManager::Instance()->addClient(clientManager);
             INDIListener::Instance()->addClient(clientManager);
             updateMenuActions();

         }
         else
         {
             QString errMsg = QString("Connection to INDI server locally on port %1 failed.").arg(port);
             KMessageBox::error(NULL, errMsg);
             foreach (DriverInfo *dv, qdv)
                 processDeviceStatus(dv);

             return false;

         }

    }


    return true;

}

void DriverManager::stopDevices(const QList<DriverInfo*> & dList)
{

     // #2 stop server
    foreach(DriverInfo *dv, dList)
    {
        ClientManager *cm = dv->getClientManager();

        cm->removeManagedDriver(dv);

        if (cm->size() == 0)
        {
              GUIManager::Instance()->removeClient(cm);
              INDIListener::Instance()->removeClient(cm);
              cm->disconnectServer();
              clients.removeOne(cm);
              delete cm;
              cm = NULL;
        }

      ServerManager *sm = dv->getServerManager();

      if (sm != NULL)
      {
          //qDebug() << "Asking to stop driver " << dv->getUniqueLabel() << endl;
          sm->stopDriver(dv);

          if (sm->size() == 0)
          {
                sm->stop();
                servers.removeOne(sm);
                delete sm;
                sm = NULL;
          }
      }

    }

    updateMenuActions();

}

void DriverManager::clearServers()
{
    foreach(ServerManager *serverManager, servers)
        serverManager->stop();

    qDeleteAll(servers);
}

void DriverManager::activateRunService()
  {
    processLocalTree(true);
  }

void DriverManager::activateStopService()
  {
    processLocalTree(false);
  }

void DriverManager::activateHostConnection()
  {
    processRemoteTree(true);
  }

void DriverManager::activateHostDisconnection()
  {
    processRemoteTree(false);
  }

ClientManager *DriverManager::getClientManager(DriverInfo *dv)
{
    return dv->getClientManager();
}

void DriverManager::updateLocalTab()
{
    if (ui->localTreeWidget->currentItem() == NULL)
          return;

    QString currentDriver = ui->localTreeWidget->currentItem()->text(LOCAL_NAME_COLUMN);

    foreach (DriverInfo *device, driversList)
    {
        if (currentDriver == device->getTreeLabel())
        {
            processDeviceStatus(device);
            break;
        }
    }
}

void DriverManager::updateClientTab()
{

     if (ui->clientTreeWidget->currentItem() == NULL)
        return;

    foreach (DriverInfo *dv, driversList)
    {
        if (ui->clientTreeWidget->currentItem()->text(HOST_NAME_COLUMN) == dv->getName() &&
                ui->clientTreeWidget->currentItem()->text(HOST_PORT_COLUMN) == dv->getPort())
        {
            processDeviceStatus(dv);
            break;
        }
    }

}

void DriverManager::processLocalTree(bool dState)
{
   QList<DriverInfo *> processed_devices;

   int port=-1;
   bool portOK=false;

   connectionMode = ui->localR->isChecked() ? SERVER_CLIENT : SERVER_ONLY;

   foreach(QTreeWidgetItem *item, ui->localTreeWidget->selectedItems())
   {
      foreach (DriverInfo *device, driversList)
      {
          port = -1;

                //device->state = (dev_request == DriverInfo::DEV_TERMINATE) ? DriverInfo::DEV_START : DriverInfo::DEV_TERMINATE;
                if (item->text(LOCAL_NAME_COLUMN) == device->getTreeLabel() && device->getServerState() != dState)
                {
                        processed_devices.append(device);

                        // N.B. If multipe devices are selected to run under one device manager
                        // then we select the port for the first device that has a valid port
                        // entry, the rest are ignored.
                        if (port == -1 && item->text(LOCAL_PORT_COLUMN).isEmpty() == false)
                        {
                            port = item->text(LOCAL_PORT_COLUMN).toInt(&portOK);
                            // If we encounter conversion error, we abort
                            if (portOK == false)
                            {
                                KMessageBox::error(0, i18n("Invalid port entry: %1", item->text(LOCAL_PORT_COLUMN)));
                                return;
                            }
                        }

                        device->setHostParameters("localhost", QString("%1").arg(port));
                }
       }
   }

   if (processed_devices.empty()) return;

   if (dState)
       startDevices(processed_devices);
   else
       stopDevices(processed_devices);


}

void DriverManager::processClientTermination(ClientManager *client)
{
    if (client == NULL)
        return;

    ServerManager *manager = client->getServerManager();
    QString errMsg = QString("Connection to INDI host at %1 on port %2 lost. Server disconnected.").arg(client->getHost()).arg(client->getPort());
    KMessageBox::error(NULL, errMsg);

    if (manager)
    {
        servers.removeOne(manager);
        delete manager;
    }

    GUIManager::Instance()->removeClient(client);
    INDIListener::Instance()->removeClient(client);

    clients.removeOne(client);
    delete client;

    updateMenuActions();
    updateLocalTab();


}

void DriverManager::processServerTermination(ServerManager* server)
{
    if (server == NULL)
        return;

    foreach(DriverInfo *dv, driversList)
        if (dv->getServerManager() == server)
        {
            dv->setServerState(false);
            dv->clear();
        }

    if (server->getMode() == SERVER_ONLY)
    {
        QString errMsg = QString("Connection to INDI host at %1 on port %2 encountered an error: %3.").arg(server->getHost()).arg(server->getPort()).arg(server->errorString());
        KMessageBox::error(NULL, errMsg);
    }

    servers.removeOne(server);
    delete server;

    updateLocalTab();
}


void DriverManager::processRemoteTree(bool dState)
{
      bool connectionToServer=false;
      bool hostPortOk = false;
      ClientManager *clientManager = NULL;
      QTreeWidgetItem *currentItem = ui->clientTreeWidget->currentItem();
      if (!currentItem) return;

    foreach (DriverInfo *dv, driversList)
     {
        if (dv->getDriverSource() != HOST_SOURCE)
            continue;

       //qDebug() << "Current item port " << currentItem->text(HOST_PORT_COLUMN) << " current dev " << dv->getName() << " -- port " << dv->getPort() << endl;
       //qDebug() << "dState is : " << (dState ? "True" : "False") << endl;

        if (currentItem->text(HOST_NAME_COLUMN) == dv->getName() && currentItem->text(HOST_PORT_COLUMN) == dv->getPort())
        {
            // Nothing changed, return
            if (dv->getClientState() == dState)
                return;

            // connect to host
            if (dState)
            {
                dv->getPort().toInt(&hostPortOk);

                if (hostPortOk == false)
                {
                    QString errMsg = QString("Invalid host port %1").arg(dv->getPort());
                    KMessageBox::error(NULL, errMsg);
                    continue;
                }

                clientManager = new ClientManager();

                clientManager->appendManagedDriver(dv);

                connect(clientManager, SIGNAL(connectionFailure(ClientManager*)), this, SLOT(processClientTermination(ClientManager*)));

                clientManager->setServer(dv->getHost().toLatin1().constData(), (uint) (dv->getPort().toInt()));

                GUIManager::Instance()->addClient(clientManager);
                INDIListener::Instance()->addClient(clientManager);

                for (int i=0; i < INDI_MAX_TRIES; i++)
                {
                    connectionToServer= clientManager->connectServer();

                    if (connectionToServer)
                        break;

                     usleep(100000);
                }

                if (connectionToServer)
                {
                    clients.append(clientManager);
                    updateMenuActions();
                }
                else
                {
                    GUIManager::Instance()->removeClient(clientManager);
                    INDIListener::Instance()->removeClient(clientManager);
                    QString errMsg = QString("Connection to INDI server at host %1 with port %2 failed.").arg(dv->getHost()).arg(dv->getPort());
                    KMessageBox::error(NULL, errMsg);
                    processDeviceStatus(dv);
                }

                return;
            }
            // Disconnect form host
            else
            {
                clientManager = dv->getClientManager();

                if (clientManager)
                {
                    clientManager->removeManagedDriver(dv);
                    clientManager->disconnectServer();
                    GUIManager::Instance()->removeClient(clientManager);
                    INDIListener::Instance()->removeClient(clientManager);
                    clients.removeOne(clientManager);
                    delete clientManager;
                    updateMenuActions();

                }

                return;
            }

        }

    }
}

void DriverManager::resizeDeviceColumn()
{
  ui->localTreeWidget->resizeColumnToContents(0);
}

void DriverManager::updateMenuActions()
{
    // We iterate over devices, we enable INDI Control Panel if we have any active device
    // We enable capture image sequence if we have any imaging device

    QAction *tmpAction = NULL;
    bool activeDevice = false;

    if (clients.size() > 0)
        activeDevice = true;

    tmpAction = KStars::Instance()->actionCollection()->action("indi_cpl");
    if (tmpAction != NULL)
    {
        //qDebug() << "indi_cpl action set to active" << endl;
       tmpAction->setEnabled(activeDevice);
    }
}

int DriverManager::getINDIPort(int customPort)
{

    int lastPort  = Options::serverPortEnd().toInt();;
    bool success = false;
    currentPort++;

    // recycle
    if (currentPort > lastPort) currentPort = Options::serverPortStart().toInt();

    QTcpServer temp_server;

    if (customPort != -1)
    {
        success = temp_server.listen(QHostAddress::LocalHost, customPort);
        if (success)
        {
            temp_server.close();
            return customPort;
        }
        else
            return -1;
    }

    for(; currentPort <= lastPort; currentPort++)
    {
            success = temp_server.listen(QHostAddress::LocalHost, currentPort);
        if(success)
        {
            temp_server.close();
            return currentPort;
        }
    }
    return -1;
}


bool DriverManager::readINDIHosts()
{
    QString indiFile("indihosts.xml");
    //QFile localeFile;
    QFile file;
    char errmsg[1024];
    char c;
    LilXML *xmlParser = newLilXML();
    XMLEle *root = NULL;
    XMLAtt *ap;
    QString hName, hHost, hPort;
    lastGroup = NULL;


    file.setFileName( KStandardDirs::locate( "appdata", indiFile ) );
    if ( file.fileName().isEmpty() || !file.open( QIODevice::ReadOnly ) )
        return false;

    while ( file.getChar( &c ) )
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            // Get host name
            ap = findXMLAtt(root, "name");
            if (!ap)
            {
                delLilXML(xmlParser);
                return false;
            }

            hName = QString(valuXMLAtt(ap));

            // Get host name
            ap = findXMLAtt(root, "hostname");

            if (!ap)
            {
                delLilXML(xmlParser);
                return false;
            }

            hHost = QString(valuXMLAtt(ap));

            ap = findXMLAtt(root, "port");

            if (!ap)
            {
                delLilXML(xmlParser);
                return false;
            }

            hPort = QString(valuXMLAtt(ap));

            DriverInfo *dv = new DriverInfo(hName);
            dv->setHostParameters(hHost, hPort);
            dv->setDriverSource(HOST_SOURCE);

            connect(dv, SIGNAL(deviceStateChanged(DriverInfo*)), this, SLOT(processDeviceStatus(DriverInfo*)));

            driversList.append(dv);

            QTreeWidgetItem *item = new QTreeWidgetItem(ui->clientTreeWidget, lastGroup);
            lastGroup = item;
            item->setIcon(HOST_STATUS_COLUMN, ui->disconnected);
            item->setText(HOST_NAME_COLUMN, hName);
            item->setText(HOST_PORT_COLUMN, hPort);


            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            kDebug() << errmsg;
            delLilXML(xmlParser);
            return false;
        }
    }

    delLilXML(xmlParser);

    return true;




}


bool DriverManager::readXMLDrivers()
{
    QDir indiDir;
    QString driverName;

    if (indiDir.cd(Options::indiDriversDir()) == false)
    {
        KMessageBox::error(0, i18n("Unable to find INDI Drivers directory: %1\nPlease make sure to set the correct path in KStars configuration", Options::indiDriversDir()));
          return false;
     }

    indiDir.setNameFilters(QStringList("*.xml"));
    indiDir.setFilter(QDir::Files | QDir::NoSymLinks);
    QFileInfoList list = indiDir.entryInfoList();

    foreach (QFileInfo fileInfo, list)
    {
        // libindi 0.7.1: Skip skeleton files
        if (fileInfo.fileName().endsWith("_sk.xml"))
            continue;

        if (fileInfo.fileName() == "drivers.xml")
        {
            // Let first attempt to load the local version of drivers.xml
            driverName = KStandardDirs::locateLocal( "appdata", "drivers.xml");

            // If found, we continue, otherwise, we load the system file
            if (driverName.isEmpty() == false && QFile(driverName).exists())
            {
                processXMLDriver(driverName);
                continue;
            }
        }

                driverName = QString("%1/%2").arg(Options::indiDriversDir()).arg(fileInfo.fileName());
                processXMLDriver(driverName);

    }

return true;

}

void DriverManager::processXMLDriver(QString & driverName)
{
    QFile file(driverName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        KMessageBox::error(0, i18n("Failed to open INDI Driver file: %1", driverName));
         return;
    }

    char errmsg[ERRMSG_SIZE];
    char c;
    LilXML *xmlParser = newLilXML();
    XMLEle *root = NULL;

    if (driverName.endsWith("drivers.xml"))
        driverSource = PRIMARY_XML;
    else
        driverSource = THIRD_PARTY_XML;

    while ( file.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            if (!buildDeviceGroup(root, errmsg))
                prXMLEle(stderr, root, 0);

            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            kDebug() << QString(errmsg) << endl;
            delLilXML(xmlParser);
            return;
        }
    }

    delLilXML(xmlParser);

}

bool DriverManager::buildDeviceGroup(XMLEle *root, char errmsg[])
{

    XMLAtt *ap;
    XMLEle *ep;
    QString groupName;
    QTreeWidgetItem *group;
    DeviceFamily groupType = KSTARS_TELESCOPE;

    // avoid overflow
    if (strlen(tagXMLEle(root)) > 1024)
        return false;

    // Get device grouping name
    ap = findXMLAtt(root, "group");

    if (!ap)
    {
        snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a group attribute", tagXMLEle(root));
        return false;
    }

    groupName = valuXMLAtt(ap);

    if (groupName.indexOf("Telescopes") != -1)
        groupType = KSTARS_TELESCOPE;
    else if (groupName.indexOf("CCDs") != -1)
        groupType = KSTARS_CCD;
    else if (groupName.indexOf("Filter") != -1)
        groupType = KSTARS_FILTER;
    else if (groupName.indexOf("Video") != -1)
        groupType = KSTARS_VIDEO;
    else if (groupName.indexOf("Focusers") != -1)
        groupType = KSTARS_FOCUSER;
    else if (groupName.indexOf("Domes") != -1)
        groupType = KSTARS_DOME;
    else if (groupName.indexOf("Receivers") != -1)
        groupType = KSTARS_RECEIVERS;
    else if (groupName.indexOf("GPS") != -1)
        groupType = KSTARS_GPS;
    else
        groupType = KSTARS_UNKNOWN;

#ifndef HAVE_CFITSIO_H
    // We do not create these groups if we don't have CFITSIO support
    if (groupType == KSTARS_CCD || groupType == KSTARS_VIDEO)
        return true;
#endif

    // Find if the group already exists
    QList<QTreeWidgetItem *> treeList = ui->localTreeWidget->findItems(groupName, Qt::MatchExactly);
    if (!treeList.isEmpty())
        group = treeList[0];
    else
        group = new QTreeWidgetItem(ui->localTreeWidget, lastGroup);

    group->setText(0, groupName);
    lastGroup = group;

    for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
        if (!buildDriverElement(ep, group, groupType, errmsg))
            return false;

    return true;
}


bool DriverManager::buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, DeviceFamily groupType, char errmsg[])
{
    XMLAtt *ap;
    XMLEle *el;
    DriverInfo *dv;
    QString label;
    QString driver;
    QString version;
    QString name;
    QString port;
    //double focal_length (-1), aperture (-1);


        ap = findXMLAtt(root, "label");
        if (!ap)
        {
            snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a label attribute", tagXMLEle(root));
            return false;
        }

        label = valuXMLAtt(ap);

        // Search for optional port attribute
        ap = findXMLAtt(root, "port");
        if (ap)
            port = valuXMLAtt(ap);

        // Let's look for telescope-specific attributes: focal length and aperture
        /*ap = findXMLAtt(root, "focal_length");
        if (ap)
            focal_length = QString(valuXMLAtt(ap)).toDouble();

        ap = findXMLAtt(root, "aperture");
        if (ap)
            aperture = QString(valuXMLAtt(ap)).toDouble();*/

        el = findXMLEle(root, "driver");

        if (!el)
            return false;

        driver = pcdataXMLEle(el);

        ap = findXMLAtt(el, "name");
        if (!ap)
        {
            snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a name attribute", tagXMLEle(el));
            return false;
        }

        name = valuXMLAtt(ap);

        el = findXMLEle(root, "version");

        if (!el)
            return false;

        version = pcdataXMLEle(el);

        QTreeWidgetItem *device = new QTreeWidgetItem(DGroup);
        //, lastDevice);

    device->setText(LOCAL_NAME_COLUMN, label);
    device->setIcon(LOCAL_STATUS_COLUMN, ui->stopPix);
    device->setText(LOCAL_VERSION_COLUMN, version);
    device->setText(LOCAL_PORT_COLUMN, port);

    lastDevice = device;

    if ((driverSource == PRIMARY_XML) && driversStringList.contains(driver) == false)
        driversStringList.append(driver);

    dv = new DriverInfo(name);

    dv->setTreeLabel(label);
    dv->setVersion(version);
    dv->setDriver(driver);

    dv->setType(groupType);

    dv->setDriverSource(driverSource);

    dv->setUserPort(port);

    connect(dv, SIGNAL(deviceStateChanged(DriverInfo*)), this, SLOT(processDeviceStatus(DriverInfo*)));

    driversList.append(dv);

    // SLOTS/SIGNAL, pop menu, indi server logic
    return true;
}

void DriverManager::updateCustomDrivers()
{

    QString label;
    QString driver;
    QString version;
    QString name;
    QTreeWidgetItem *group, *widgetDev;
    //double focal_length (-1), aperture (-1);

    // Find if the group already exists
    QList<QTreeWidgetItem *> treeList = ui->localTreeWidget->findItems("Telescopes", Qt::MatchExactly);
    if (!treeList.isEmpty())
        group = treeList[0];
    else return;


    // Find custom telescope to ADD
    foreach(OAL::Scope *s, *(KStarsData::Instance()->logObject()->scopeList()))
        {
            name = label = s->name();

            if (findDriverByLabel(label) || s->driver() == i18n("None"))
                continue;

            driver = s->driver();
            version = QString("1.0");

            QTreeWidgetItem *device = new QTreeWidgetItem(group);
            device->setText(LOCAL_NAME_COLUMN, QString(label));
            device->setIcon(LOCAL_STATUS_COLUMN, ui->stopPix);
            device->setText(LOCAL_VERSION_COLUMN, QString(version));

            DriverInfo *dv = new DriverInfo(name);

            dv->setTreeLabel(label);
            dv->setDriver(driver);
            dv->setVersion(version);
            dv->setType(KSTARS_TELESCOPE);
            dv->setDriverSource(EM_XML);

            connect(dv, SIGNAL(deviceStateChanged(DriverInfo*)), this, SLOT(processDeviceStatus(DriverInfo*)));
            driversList.append(dv);
        }

        // Find custom telescope to REMOVE
        foreach(DriverInfo *dev, driversList)
        {
            // If it's from primary xml file or it is in a running state, continue.
            if (dev->getDriverSource() != EM_XML || dev->getClientState())
                continue;

            if (KStarsData::Instance()->logObject()->findScopeByName(dev->getName()))
                continue;

            // Find if the group already exists
            QList<QTreeWidgetItem *> devList = ui->localTreeWidget->findItems(dev->getTreeLabel(), Qt::MatchExactly  | Qt::MatchRecursive);
            if (!devList.isEmpty())
            {
                widgetDev = devList[0];
                group->removeChild(widgetDev);
            }
            else return;

            driversList.removeOne(dev);
            delete (dev);
        }

}

void DriverManager::addINDIHost()
{
    QDialog hostConfDialog;
    Ui::INDIHostConf hostConf;
    hostConf.setupUi(&hostConfDialog);
    hostConfDialog.setWindowTitle(i18n("Add Host"));
    bool portOk = false;

    if (hostConfDialog.exec() == QDialog::Accepted)
    {
        DriverInfo *hostItem      = new DriverInfo(hostConf.nameIN->text());

        hostConf.portnumber->text().toInt(&portOk);

        if (portOk == false)
        {
            KMessageBox::error(0, i18n("Error: the port number is invalid."));
            delete hostItem;
            return;
        }

        hostItem->setHostParameters(hostConf.hostname->text(), hostConf.portnumber->text());

        //search for duplicates
        //for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
        foreach (DriverInfo * host, driversList)
        if (hostItem->getName()   == host->getName() &&  hostItem->getPort() == host->getPort())
        {
            KMessageBox::error(0, i18n("Host: %1 Port: %2 already exists.", hostItem->getName(), hostItem->getPort()));
            delete hostItem;
            return;
        }

        hostItem->setDriverSource(HOST_SOURCE);

        driversList.append(hostItem);

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->clientTreeWidget);
        item->setIcon(HOST_STATUS_COLUMN, ui->disconnected);
        item->setText(HOST_NAME_COLUMN, hostConf.nameIN->text());
        item->setText(HOST_PORT_COLUMN, hostConf.portnumber->text());

    }

    saveHosts();
}

void DriverManager::modifyINDIHost()
{

    QDialog hostConfDialog;
    Ui::INDIHostConf hostConf;
    hostConf.setupUi(&hostConfDialog);
    hostConfDialog.setWindowTitle(i18n("Modify Host"));

    QTreeWidgetItem *currentItem = ui->clientTreeWidget->currentItem();

    if (currentItem == NULL)
        return;

    foreach (DriverInfo * host, driversList)
    {
        if (currentItem->text(HOST_NAME_COLUMN) == host->getName() && currentItem->text(HOST_PORT_COLUMN) == host->getPort())
        {
            hostConf.nameIN->setText(host->getName());
            hostConf.hostname->setText(host->getHost());
            hostConf.portnumber->setText(host->getPort());

            if (hostConfDialog.exec() == QDialog::Accepted)
            {
                //INDIHostsInfo *hostItem = new INDIHostsInfo;
                host->setName(hostConf.nameIN->text());
                host->setHostParameters(hostConf.hostname->text(), hostConf.portnumber->text());

                currentItem->setText(HOST_NAME_COLUMN, hostConf.nameIN->text());
                currentItem->setText(HOST_PORT_COLUMN, hostConf.portnumber->text());

                //ksw->data()->INDIHostsList.replace(i, hostItem);

                saveHosts();
                return;
            }
        }
    }

}

void DriverManager::removeINDIHost()
{


    if (ui->clientTreeWidget->currentItem() == NULL)
        return;

    foreach (DriverInfo * host, driversList)
        if (ui->clientTreeWidget->currentItem()->text(HOST_NAME_COLUMN) == host->getName() &&
                ui->clientTreeWidget->currentItem()->text(HOST_PORT_COLUMN) == host->getPort())
        {
            if (host->getClientState())
            {
                KMessageBox::error( 0, i18n("You need to disconnect the client before removing it."));
                return;
            }

            if (KMessageBox::warningContinueCancel( 0, i18n("Are you sure you want to remove the %1 client?", ui->clientTreeWidget->currentItem()->text(HOST_NAME_COLUMN)), i18n("Delete Confirmation"),KStandardGuiItem::del())!=KMessageBox::Continue)
                return;

            driversList.removeOne(host);
            delete host;
            delete (ui->clientTreeWidget->currentItem());
            break;
        }

    saveHosts();
}

void DriverManager::saveHosts()
{
    QFile file;
    QString hostData;

    file.setFileName( KStandardDirs::locateLocal( "appdata", "indihosts.xml" ) ); //determine filename in local user KDE directory tree.

    if ( !file.open( QIODevice::WriteOnly))
    {
        QString message = i18n( "unable to write to file 'indihosts.xml'\nAny changes to INDI hosts configurations will not be saved." );
        KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
        return;
    }

    QTextStream outstream(&file);

    //for (uint i= 0; i < ksw->data()->INDIHostsList.count(); i++)
    foreach (DriverInfo * host, driversList)
    {

        if (host->getDriverSource() != HOST_SOURCE)
            continue;

        hostData  = "<INDIHost name='";
        hostData += host->getName();
        hostData += "' hostname='";
        hostData += host->getHost();
        hostData += "' port='";
        hostData += host->getPort();
        hostData += "' />\n";

        outstream << hostData;

    }

    file.close();
}

DriverInfo * DriverManager::findDriverByName(const QString &name)
{
    foreach(DriverInfo *dv, driversList)
    {
        if (dv->getName() == name)
            return dv;
    }

    return NULL;
}

DriverInfo * DriverManager::findDriverByLabel(const QString &label)
{
    foreach(DriverInfo *dv, driversList)
    {
        if (dv->getTreeLabel() == label)
            return dv;
    }

    return NULL;
}

QString DriverManager::getUniqueDeviceLabel(const QString &label)
{
    int nset=0;
    QString uniqueLabel = label;

    foreach(ClientManager *cm, clients)
        foreach(INDI::BaseDevice *dv, cm->getDevices())
        {
            if (label == QString(dv->getDeviceName()))
                nset++;
        }

  if (nset > 1)
      uniqueLabel = QString("%1 %2").arg(label).arg(nset);

  return uniqueLabel;
}

#include "drivermanager.moc"