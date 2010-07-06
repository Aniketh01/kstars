/***************************************************************************
                          observinglist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004 by Jeff Woods, Jason Harris
    email                : jcwoods@bellsouth.net, jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "observinglist.h"

#include <stdio.h>

#include <QFile>
#include <QDir>
#include <QFrame>
#include <QTextStream>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QDirIterator>

#include <kpushbutton.h>
#include <kstatusbar.h>
#include <ktextedit.h>
#include <kinputdialog.h>
#include <kicon.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <ktemporaryfile.h>
#include <klineedit.h>
#include <kplotobject.h>
#include <kplotaxis.h>
#include <kplotwidget.h>
#include <kio/copyjob.h>
#include <kstandarddirs.h>

#include "ksalmanac.h"
#include "obslistwizard.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skycomponents/skymapcomposite.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include "dialogs/finddialog.h"
#include "tools/altvstime.h"
#include "tools/wutdialog.h"
#include "Options.h"
#include "imageviewer.h"
#include "thumbnailpicker.h"
#include "obslistpopupmenu.h"
#include "oal/log.h"
#include "oal/oal.h"
#include "oal/execute.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indimenu.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/indidevice.h"
#include "indi/devicemanager.h"
#include "indi/indistd.h"
#endif

//
// ObservingListUI
// ---------------------------------
ObservingListUI::ObservingListUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

//
// ObservingList
// ---------------------------------
ObservingList::ObservingList( KStars *_ks )
        : KDialog( (QWidget*)_ks ),
        ks( _ks ), LogObject(0), m_CurrentObject(0),
        noNameStars(0), isModified(false), bIsLarge(true)
{
    ui = new ObservingListUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Observing List" ) );
    setButtons( KDialog::Close );
    dt = KStarsDateTime::currentDateTime();
    geo = ks->data()->geo();
    sessionView = false;
    nativeSave = true;
    FileName = "";
    pmenu = new ObsListPopupMenu();
    //Set up the Table Views
    m_Model = new QStandardItemModel( 0, 5, this );
    m_Session = new QStandardItemModel( 0, 5 );
    m_Model->setHorizontalHeaderLabels( QStringList() << i18n( "Name" ) 
        << i18nc( "Right Ascension", "RA" ) << i18nc( "Declination", "Dec" )
        << i18nc( "Magnitude", "Mag" ) << i18n( "Type" ) );
    m_Session->setHorizontalHeaderLabels( QStringList() << i18n( "Name" ) 
        << i18nc( "Right Ascension", "RA" ) << i18nc( "Declination", "Dec" )
        << i18nc( "Magnitude", "Mag" ) << i18n( "Type" ) 
        << i18n( "Time" ) << i18nc( "Altitude", "Alt" ) << i18nc( "Azimuth", "Az" ));
    m_SortModel = new QSortFilterProxyModel( this );
    m_SortModel->setSourceModel( m_Model );
    m_SortModel->setDynamicSortFilter( true );
    ui->TableView->setModel( m_SortModel );
    ui->TableView->horizontalHeader()->setStretchLastSection( true );
    ui->TableView->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );
    m_SortModelSession = new QSortFilterProxyModel;
    m_SortModelSession->setSourceModel( m_Session );
    m_SortModelSession->setDynamicSortFilter( true );
    ui->SessionView->setModel( m_SortModelSession );
    ui->SessionView->horizontalHeader()->setStretchLastSection( true );
    ui->SessionView->horizontalHeader()->setResizeMode( QHeaderView::ResizeToContents );
    ksal = new KSAlmanac;
    ksal->setLocation(geo);
    ui->View->setSunRiseSetTimes(ksal->getSunRise(),ksal->getSunSet());
    ui->View->setLimits( -12.0, 12.0, -90.0, 90.0 );
    ui->View->axis(KPlotWidget::BottomAxis)->setTickLabelFormat( 't' );
    ui->View->axis(KPlotWidget::BottomAxis)->setLabel( i18n( "Local Time" ) );
    ui->View->axis(KPlotWidget::TopAxis)->setTickLabelFormat( 't' );
    ui->View->axis(KPlotWidget::TopAxis)->setTickLabelsShown( true );
    ui->DateEdit->setDate(dt.date());
    ui->SetLocation->setText( geo -> fullName() );
    ui->ImagePreview->installEventFilter( this );
    ui->TableView->viewport()->installEventFilter( this );
    ui->SessionView->viewport()->installEventFilter( this );
    // setDefaultImage();
    //Connections
    connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
    connect( ui->TableView, SIGNAL( doubleClicked( const QModelIndex& ) ),
             this, SLOT( slotCenterObject() ) );
    connect( ui->TableView->selectionModel(), 
            SIGNAL( selectionChanged(const QItemSelection&, const QItemSelection&) ),
            this, SLOT( slotNewSelection() ) );
    connect( ui->SessionView->selectionModel(), 
            SIGNAL( selectionChanged(const QItemSelection&, const QItemSelection&) ),
            this, SLOT( slotNewSelection() ) );
    connect( ui->WUTButton, SIGNAL( clicked() ),
             this, SLOT( slotWUT() ) );
    connect( ui->FindButton, SIGNAL( clicked() ),
             this, SLOT( slotFind() ) );
    connect( ui->OpenButton, SIGNAL( clicked() ),
             this, SLOT( slotOpenList() ) );
    connect( ui->SaveButton, SIGNAL( clicked() ),
             this, SLOT( slotSaveSession() ) );
    connect( ui->SaveAsButton, SIGNAL( clicked() ),
             this, SLOT( slotSaveSessionAs() ) );
    connect( ui->WizardButton, SIGNAL( clicked() ),
             this, SLOT( slotWizard() ) );
    connect( ui->MiniButton, SIGNAL( clicked() ),
             this, SLOT( slotToggleSize() ) );
    connect( ui->SetLocation, SIGNAL( clicked() ),
             this, SLOT( slotLocation() ) );
    connect( ui->Update, SIGNAL( clicked() ),
             this, SLOT( slotUpdate() ) );
    connect( ui->SaveImage, SIGNAL( clicked() ),
             this, SLOT( slotSaveImage() ) );
    connect( ui->DeleteImage, SIGNAL( clicked() ),
             this, SLOT( slotDeleteCurrentImage() ) );
    connect( ui->GoogleImage, SIGNAL( clicked() ),
             this, SLOT( slotGoogleImage() ) );
    connect( ui->SetTime, SIGNAL( clicked() ),
             this, SLOT( slotSetTime() ) );
    connect( ui->tabWidget, SIGNAL( currentChanged(int) ),
             this, SLOT( slotChangeTab(int) ) );
    connect( ui->saveImages, SIGNAL( clicked() ),
             this, SLOT( slotSaveAllImages() ) );
    connect( ui->DeleteAllImages, SIGNAL( clicked() ),
             this, SLOT( slotDeleteAllImages() ) );
    connect( ui->OALExport, SIGNAL( clicked() ),
             this, SLOT( slotOALExport() ) );  
    //Add icons to Push Buttons
    ui->OpenButton->setIcon( KIcon("document-open") );
    ui->SaveButton->setIcon( KIcon("document-save") );
    ui->SaveAsButton->setIcon( KIcon("document-save-as") );
    ui->WizardButton->setIcon( KIcon("games-solve") ); //is there a better icon for this button?
    ui->MiniButton->setIcon( KIcon("view-restore") );
    noSelection = true;
    showScope = false;
    ui->NotesLabel->setEnabled( false );
    ui->NotesEdit->setEnabled( false );
    ui->SetTime->setEnabled( false );
    ui->TimeEdit->setEnabled( false );
    ui->GoogleImage->setEnabled( false );
    ui->saveImages->setEnabled( false );
    ui->SaveImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    ui->OALExport->setEnabled( false );

    slotLoadWishList(); //Load the wishlist from disk if present
    m_CurrentObject = 0;
    setSaveImagesButton();
    //Hide the MiniButton until I can figure out how to resize the Dialog!
//    ui->MiniButton->hide();
}

ObservingList::~ObservingList()
{
    delete ksal;
}

//SLOTS

void ObservingList::slotAddObject( SkyObject *obj, bool session, bool update ) {
    bool addToWishList=true;
    if( ! obj )
        obj = ks->map()->clickedObject();

    if( obj->name() == "star" ) {
        KMessageBox::sorry(0, i18n( "Unnamed stars are not supported in the observing lists"));
        return;
    }
        
    //First, make sure object is not already in the list
    if ( obsList().contains( obj ) ) {
        addToWishList = false;
        if( ! session ) {
            ks->statusBar()->changeItem( i18n( "%1 is already in your wishlist.", obj->name() ), 0 );
            return;
        }
    }

    if ( session && sessionList().contains( obj ) ) { 
        ks->statusBar()->changeItem( i18n( "%1 is already in the session plan.", obj->name() ), 0 );
        return;
    }
    
    QString smag = "--";
    if (  - 30.0 < obj->mag() && obj->mag() < 90.0 ) smag = QString::number( obj->mag(), 'g', 2 ); // The lower limit to avoid display of unrealistic comet magnitudes

    SkyPoint p = obj->recomputeCoords( dt, geo );

    //Insert object in the Wish List
    if( addToWishList  ) {
        m_ObservingList.append( obj );
        m_CurrentObject = obj;
        QList<QStandardItem*> itemList;
        if(obj->name() == "star" )
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( obj->ra0().toHMSString() ) 
                    << new QStandardItem( obj->dec0().toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() );
        else
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( p.ra().toHMSString() ) 
                    << new QStandardItem( p.dec().toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() );
        m_Model->appendRow( itemList );
        //Note addition in statusbar
        ks->statusBar()->changeItem( i18n( "Added %1 to observing list.", obj->name() ), 0 );
        ui->TableView->resizeColumnsToContents(); 
        if( ! update ) slotSaveList();
    }
    //Insert object in the Session List
    if( session ){
        m_SessionList.append(obj);
        dt.setTime( TimeHash.value( obj->name(), obj->transitTime( dt, geo ) ) );
        dms lst(geo->GSTtoLST( dt.gst() ));
        p.EquatorialToHorizontal( &lst, geo->lat() );
        QList<QStandardItem*> itemList;
        if(obj->name() == "star" )
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( obj->ra0().toHMSString() ) 
                    << new QStandardItem( obj->dec0().toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() )
                    << new QStandardItem( "--"  )
                    << new QStandardItem( "--"  )
                    << new QStandardItem( "--"  );
        else
            itemList << new QStandardItem( obj->translatedName() ) 
                    << new QStandardItem( p.ra().toHMSString() ) 
                    << new QStandardItem( p.dec().toDMSString() ) 
                    << new QStandardItem( smag )
                    << new QStandardItem( obj->typeName() )
                    << new QStandardItem( TimeHash.value( obj->name(), obj->transitTime( dt, geo ) ).toString( "HH:mm" ) )
                    << new QStandardItem( p.alt().toDMSString() )
                    << new QStandardItem( p.az().toDMSString() );
        m_Session->appendRow( itemList );
        //Adding an object should trigger the modified flag
        if ( ! isModified ) isModified = true;
        ui->SessionView->resizeColumnsToContents();
        //Note addition in statusbar
        ks->statusBar()->changeItem( i18n( "Added %1 to session list.", obj->name() ), 0 );
    }
    setSaveImagesButton();
}

void ObservingList::slotRemoveObject( SkyObject *o, bool session, bool update ) {
    if( ! update ) { 
        if ( ! o )
            o = ks->map()->clickedObject(); 
        else if( sessionView ) //else if is needed as clickedObject should not be removed from the session list.
            session = true;
    }
    if( ! session ) {
        int k = obsList().indexOf( o );
        if ( o == LogObject ) saveCurrentUserLog();
        //Remove row from the TableView model
        bool found(false);
        if ( o->name() == "star" ) {
            //Find object in table by RA and Dec
            for ( int irow = 0; irow < m_Model->rowCount(); ++irow ) {
                QString ra = m_Model->item(irow, 1)->text();
                QString dc = m_Model->item(irow, 2)->text();
                if ( o->ra0().toHMSString() == ra && o->dec0().toDMSString() == dc ) {
                    m_Model->removeRow(irow);
                    found = true;
                    break;
                }
            }
        } else { // name is not "star"
            //Find object in table by name
            for ( int irow = 0; irow < m_Model->rowCount(); ++irow ) {
                QString name = m_Model->item(irow, 0)->text();
                if ( o->translatedName() == name ) {
                    m_Model->removeRow(irow);
                    found = true;
                    break;
                }
            }
        }
        obsList().removeAt(k);
        ui->View->removeAllPlotObjects();
        ui->TableView->resizeColumnsToContents();
        if( ! update ) slotSaveList();
    } else {
        int k = sessionList().indexOf( o );
        if ( o == LogObject ) saveCurrentUserLog();
        //Remove row from the Session View model
        bool found(false);
        if ( o->name() == "star" ) {
            //Find object in table by RA and Dec
            for ( int irow = 0; irow < m_Model->rowCount(); ++irow ) {
                QString ra = m_Session->item(irow, 1)->text();
                QString dc = m_Session->item(irow, 2)->text();
                if ( o->ra0().toHMSString() == ra && o->dec0().toDMSString() == dc ) {
                    m_Session->removeRow(irow);
                    found = true;
                    break;
                }
            }
        } else { // name is not "star"
            //Find object in table by name
            for ( int irow = 0; irow < m_Session->rowCount(); ++irow ) {
                QString name = m_Session->item(irow, 0)->text();
                if ( o->translatedName() == name ) {
                    m_Session->removeRow(irow);
                    found = true;
                    break;
                }
            }
        }
        if( ! update )
            TimeHash.remove( o->name() );
        sessionList().removeAt(k);//Remove from the session list
        if ( ! isModified ) isModified = true;//Removing an object should trigger the modified flag
        ui->View->removeAllPlotObjects();
        ui->SessionView->resizeColumnsToContents();
    }
}

void ObservingList::slotRemoveSelectedObjects() {
    if( sessionView )
    {
        //Find each object by name in the session list, and remove it
        //Go backwards so item alignment doesn't get screwed up as rows are removed.
        for ( int irow = m_Session->rowCount()-1; irow >= 0; --irow ) {
            if ( ui->SessionView->selectionModel()->isRowSelected( irow, QModelIndex() ) ) {
                QModelIndex mSortIndex = m_SortModelSession->index( irow, 0 );
                QModelIndex mIndex = m_SortModelSession->mapToSource( mSortIndex );
                int irow = mIndex.row();
                QString ra = m_Session->item(irow, 1)->text();
                QString dc = m_Session->item(irow, 2)->text();
                foreach ( SkyObject *o, sessionList() ) {
                    //Stars named "star" must be matched by coordinates
                    if ( o->name() == "star" ) {
                        if ( o->ra0().toHMSString() == ra && o->dec0().toDMSString() == dc ) {
                            slotRemoveObject( o, true );
                            break;
                        }
    
                    } else if ( o->translatedName() == mIndex.data().toString() ) {
                        slotRemoveObject( o, true );
                        break;
                    }
                }
            }
        }
        //we've removed all selected objects, so clear the selection
        ui->SessionView->selectionModel()->clear();
        //Update the lists in the Execute window as well
        ks->getExecute()->init();
    } else {
         //Find each object by name in the observing list, and remove it
         //Go backwards so item alignment doesn't get screwed up as rows are removed.
         for ( int irow = m_Model->rowCount()-1; irow >= 0; --irow ) {
             if ( ui->TableView->selectionModel()->isRowSelected( irow, QModelIndex() ) ) {
                 QModelIndex mSortIndex = m_SortModel->index( irow, 0 );
                 QModelIndex mIndex = m_SortModel->mapToSource( mSortIndex );
                 int irow = mIndex.row();
                 QString ra = m_Model->item(irow, 1)->text();
                 QString dc = m_Model->item(irow, 2)->text(); 
                 foreach ( SkyObject *o, obsList() ) {
                     //Stars named "star" must be matched by coordinates
                     if ( o->name() == "star" ) {
                         if ( o->ra0().toHMSString() == ra && o->dec0().toDMSString() == dc ) {
                             slotRemoveObject( o );
                             break;
                         }
     
                     } else if ( o->translatedName() == mIndex.data().toString() ) {
                         slotRemoveObject( o );
                         break;
                     }
                 }
             }
         }
        //we've removed all selected objects, so clear the selection
        ui->TableView->selectionModel()->clear();
    }
    setSaveImagesButton();
    ui->ImagePreview->setCursor( Qt::ArrowCursor );
}

void ObservingList::slotNewSelection() {
    bool found = false;
    singleSelection = false;
    noSelection = false;
    showScope = false;
    ui->ImagePreview->clearPreview();
    ui->ImagePreview->setCursor( Qt::ArrowCursor );
    QModelIndexList selectedItems;
    QString newName;
    SkyObject *o;
    ui->SaveImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    if( sessionView ) {
        selectedItems = m_SortModelSession->mapSelectionToSource( ui->SessionView->selectionModel()->selection() ).indexes();
        //When one object is selected
        if ( selectedItems.size() == m_Session->columnCount() ) {
            newName = selectedItems[0].data().toString();
            singleSelection = true;
            //Find the selected object in the SessionList,
            //then break the loop.  Now SessionList.current()
            //points to the new selected object (until now it was the previous object)
            foreach ( o, sessionList() ) {
                if ( o->translatedName() == newName ) {
                    found = true;
                    break;
                }
            }
        }
    } else {
        selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();
        //When one object is selected
        if ( selectedItems.size() == m_Model->columnCount() ) {
            newName = selectedItems[0].data().toString();
            singleSelection = true;
            //Find the selected object in the obsList,
            //then break the loop.  Now obsList.current()
            //points to the new selected object (until now it was the previous object)
            foreach ( o, obsList() ) {
                if ( o->translatedName() == newName ) {
                    found = true;
                break;
                }
            }
        }
    }
    if( singleSelection ) {
        //Enable buttons
        ui->ImagePreview->setCursor( Qt::PointingHandCursor );
        #ifdef HAVE_INDI_H
            showScope = true;
        #endif
        setDefaultImage();
        if ( found ) {
            m_CurrentObject = o;
            QPoint pos(0,0);
            plot( o );
            //Change the CurrentImage, DSS/SDSS Url to correspond to the new object
            setCurrentImage( o );
            ui->GoogleImage->setEnabled( true );
            if ( newName != i18n( "star" ) ) {
                //Display the current object's user notes in the NotesEdit
                //First, save the last object's user log to disk, if necessary
                saveCurrentUserLog(); //uses LogObject, which is still the previous obj.
                //set LogObject to the new selected object
                LogObject = currentObject();
                ui->NotesLabel->setEnabled( true );
                ui->NotesEdit->setEnabled( true );
                ui->NotesLabel->setText( i18n( "observing notes for %1:", LogObject->translatedName() ) );
                if ( LogObject->userLog().isEmpty() ) {
                    ui->NotesEdit->setPlainText( i18n( "Record here observation logs and/or data on %1.", LogObject->translatedName() ) );
                } else {
                    ui->NotesEdit->setPlainText( LogObject->userLog() );
                }
                if( sessionView ) {
                    ui->TimeEdit->setEnabled( true );
                    ui->SetTime->setEnabled( true );
                    ui->TimeEdit->setTime( TimeHash.value( o->name(), o->transitTime( dt, geo ) ) );
                }   
            } else { //selected object is named "star"
                //clear the log text box
                saveCurrentUserLog();
                ui->NotesLabel->setText( i18n( "observing notes (disabled for unnamed star)" ) );
                ui->NotesLabel->setEnabled( false );
                ui->NotesEdit->clear();
                ui->NotesEdit->setEnabled( false );
                ui->GoogleImage->setEnabled( false );
            }
            if( QFile( KStandardDirs::locateLocal( "appdata", CurrentImage ) ).size() > 13000 ) {//If the image is present, show it!
                ui->ImagePreview->showPreview( KUrl( KStandardDirs::locateLocal( "appdata", CurrentImage ) ) );
                ui->ImagePreview->show();
                ui->SaveImage->setEnabled( false );
                ui->DeleteImage->setEnabled( true );
            } else if( QFile( KStandardDirs::locateLocal( "appdata", "Temp_" + CurrentImage ) ).size() > 13000 ) {
                ui->ImagePreview->showPreview( KUrl( KStandardDirs::locateLocal( "appdata","Temp_" + CurrentImage ) ) );
                ui->ImagePreview->show();
                ui->SaveImage->setEnabled( true );
                ui->DeleteImage->setEnabled( true );
            }
        } else {
            kDebug() << i18n( "Object %1 not found in list.", newName );
        } 
    } else if ( selectedItems.size() == 0 ) {//Nothing selected
        //Disable buttons
        noSelection = true;
        ui->NotesLabel->setText( i18n( "Select an object to record notes on it here:" ) );
        ui->NotesLabel->setEnabled( false );
        ui->NotesEdit->setEnabled( false );
        m_CurrentObject = 0;
        ui->TimeEdit->setEnabled( false );
        ui->SetTime->setEnabled( false );
        ui->GoogleImage->setEnabled( false );
        //Clear the user log text box.
        saveCurrentUserLog();
        ui->NotesEdit->setPlainText("");
        //Clear the plot in the AVTPlotwidget
        ui->View->removeAllPlotObjects();
    } else { //more than one object selected.
        ui->NotesLabel->setText( i18n( "Select a single object to record notes on it here:" ) );
        ui->NotesLabel->setEnabled( false );
        ui->NotesEdit->setEnabled( false );
        ui->TimeEdit->setEnabled( false );
        ui->SetTime->setEnabled( false );
        ui->GoogleImage->setEnabled( false );
        m_CurrentObject = 0;
        //Clear the plot in the AVTPlotwidget
        ui->View->removeAllPlotObjects();
        //Clear the user log text box.
        saveCurrentUserLog();
        ui->NotesEdit->setPlainText("");
    }
}

void ObservingList::slotCenterObject() {
    QModelIndexList selectedItems;
    if (sessionView) {
        selectedItems = ui->SessionView->selectionModel()->selectedRows();
    } else {
        selectedItems = ui->TableView->selectionModel()->selectedRows();
    }

    if (selectedItems.size() == 1) {
        ks->map()->setClickedObject( currentObject() );
        ks->map()->setClickedPoint( currentObject() );
        ks->map()->slotCenter();
    }
}

void ObservingList::slotSlewToObject()
{
#ifdef HAVE_INDI_H

    INDI_D *indidev(NULL);
    INDI_P *prop(NULL), *onset(NULL);
    INDI_E *RAEle(NULL), *DecEle(NULL), *AzEle(NULL), *AltEle(NULL), *ConnectEle(NULL), *nameEle(NULL);
    bool useJ2000( false);
    int selectedCoord(0);
    SkyPoint sp;

    // Find the first device with EQUATORIAL_EOD_COORD or EQUATORIAL_COORD and with SLEW element
    // i.e. the first telescope we find!
    INDIMenu *imenu = ks->indiMenu();

    for (int i=0; i < imenu->managers.size() ; i++)
    {
        for (int j=0; j < imenu->managers.at(i)->indi_dev.size(); j++)
        {
            indidev = imenu->managers.at(i)->indi_dev.at(j);
            indidev->stdDev->currentObject = NULL;
            prop = indidev->findProp("EQUATORIAL_EOD_COORD");
            if (prop == NULL)
            {
                prop = indidev->findProp("EQUATORIAL_COORD");
                if (prop == NULL)
                {
                    prop = indidev->findProp("HORIZONTAL_COORD");
                    if (prop == NULL)
                        continue;
                    else
                        selectedCoord = 1;      /* Select horizontal */
                }
                else
                    useJ2000 = true;
            }

            ConnectEle = indidev->findElem("CONNECT");
            if (!ConnectEle) continue;

            if (ConnectEle->state == PS_OFF)
            {
                KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", indidev->label));
                return;
            }

            switch (selectedCoord)
            {
                // Equatorial
            case 0:
                if (prop->perm == PP_RO) continue;
                RAEle  = prop->findElement("RA");
                if (!RAEle) continue;
                DecEle = prop->findElement("DEC");
                if (!DecEle) continue;
                break;

                // Horizontal
            case 1:
                if (prop->perm == PP_RO) continue;
                AzEle = prop->findElement("AZ");
                if (!AzEle) continue;
                AltEle = prop->findElement("ALT");
                if (!AltEle) continue;
                break;
            }

            onset = indidev->findProp("ON_COORD_SET");
            if (!onset) continue;

            onset->activateSwitch("SLEW");

            indidev->stdDev->currentObject = currentObject();

            /* Send object name if available */
            if (indidev->stdDev->currentObject)
            {
                nameEle = indidev->findElem("OBJECT_NAME");
                if (nameEle && nameEle->pp->perm != PP_RO)
                {
                    nameEle->write_w->setText(indidev->stdDev->currentObject->name());
                    nameEle->pp->newText();
                }
            }

            switch (selectedCoord)
            {
            case 0:
                if (indidev->stdDev->currentObject)
                    sp = *indidev->stdDev->currentObject;
                else
                    sp = *ks->map()->clickedPoint();

                if (useJ2000)
                    sp.apparentCoord(ks->data()->ut().djd(), (long double) J2000);

                RAEle->write_w->setText(QString("%1:%2:%3").arg(sp.ra().hour()).arg(sp.ra().minute()).arg(sp.ra().second()));
                DecEle->write_w->setText(QString("%1:%2:%3").arg(sp.dec().degree()).arg(sp.dec().arcmin()).arg(sp.dec().arcsec()));

                break;

            case 1:
                if (indidev->stdDev->currentObject)
                {
                    sp.setAz( indidev->stdDev->currentObject->az());
                    sp.setAlt(indidev->stdDev->currentObject->alt());
                }
                else
                {
                    sp.setAz( ks->map()->clickedPoint()->az());
                    sp.setAlt(ks->map()->clickedPoint()->alt());
                }

                AzEle->write_w->setText(QString("%1:%2:%3").arg(sp.az().degree()).arg(sp.az().arcmin()).arg(sp.az().arcsec()));
                AltEle->write_w->setText(QString("%1:%2:%3").arg(sp.alt().degree()).arg(sp.alt().arcmin()).arg(sp.alt().arcsec()));

                break;
            }

            prop->newText();

            return;
        }
    }

    // We didn't find any telescopes
    KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));

#endif
}

//FIXME: This will open multiple Detail windows for each object;
//Should have one window whose target object changes with selection
void ObservingList::slotDetails() {
    if ( currentObject() ) {
        QPointer<DetailDialog> dd = new DetailDialog( currentObject(), ks->data()->lt(), geo, ks );
        dd->exec();
    	delete dd;
    }
}

void ObservingList::slotWUT() {
    KStarsDateTime lt = dt;
    lt.setTime( QTime(8,0,0) );
    QPointer<WUTDialog> w = new WUTDialog( ks, sessionView, geo, lt );
    w->exec();
    delete w;
}

void ObservingList::slotAddToSession() {
    QModelIndexList selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();
    if ( selectedItems.size() ) {
        foreach ( const QModelIndex &i, selectedItems ) {
            foreach ( SkyObject *o, obsList() ) {
                if ( o->translatedName() == i.data().toString() )
                    slotAddObject( o, true );
                }
        }
    }
}

void ObservingList::slotFind() {
   QPointer<FindDialog> fd = new FindDialog( ks );    
   if ( fd->exec() == QDialog::Accepted ) {
       SkyObject *o = fd->selectedObject();
       if( o != 0 ) {
           slotAddObject( o, sessionView );  
       }
   }
   delete fd;
}

void ObservingList::slotAVT() {
    QModelIndexList selectedItems;
    // TODO: Think and see if there's a more effecient way to do this. I can't seem to think of any, but this code looks like it could be improved. - Akarsh
    if( sessionView ) {
        QPointer<AltVsTime> avt = new AltVsTime( ks );//FIXME KStars class is singleton, so why pass it?
        for ( int irow = m_Session->rowCount()-1; irow >= 0; --irow ) {
            if ( ui->SessionView->selectionModel()->isRowSelected( irow, QModelIndex() ) ) {
                QModelIndex mSortIndex = m_SortModelSession->index( irow, 0 );
                QModelIndex mIndex = m_SortModelSession->mapToSource( mSortIndex );
                int irow = mIndex.row();
                QString ra = m_Session->item(irow, 1)->text();
                QString dc = m_Session->item(irow, 2)->text();
                foreach ( SkyObject *o, sessionList() ) {
                    //Stars named "star" must be matched by coordinates
                    if ( o->name() == "star" ) {
                        if ( o->ra0().toHMSString() == ra && o->dec0().toDMSString() == dc ) {
                            avt->processObject( o );
                            break;
                        }
    
                    } else if ( o->translatedName() == mIndex.data().toString() ) {
                        avt->processObject( o );
                        break;
                    }
                }
            }
        }
        avt->exec();
	    delete avt;
    } else {
        selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();
        if ( selectedItems.size() ) {
            QPointer<AltVsTime> avt = new AltVsTime( ks );//FIXME KStars class is singleton, so why pass it?
            foreach ( const QModelIndex &i, selectedItems ) {
                foreach ( SkyObject *o, obsList() )
                    if ( o->translatedName() == i.data().toString() )
                        avt->processObject( o );
            }
            avt->exec();
	        delete avt;
        }
    }       
}

//FIXME: On close, we will need to close any open Details/AVT windows
void ObservingList::slotClose() {
    //Save the current User log text
    saveCurrentUserLog();
    ui->View->removeAllPlotObjects();
    slotNewSelection();
    saveCurrentList();
    hide();
}

void ObservingList::saveCurrentUserLog() {
    if ( ! ui->NotesEdit->toPlainText().isEmpty() &&
            ui->NotesEdit->toPlainText() !=
            i18n( "Record here observation logs and/or data on %1.", LogObject->translatedName() ) ) {
        LogObject->saveUserLog( ui->NotesEdit->toPlainText() );
        ui->NotesEdit->clear();
        ui->NotesLabel->setText( i18n( "Observing notes for object:" ) );
        LogObject = NULL;
    }
}

void ObservingList::slotOpenList() {
    KUrl fileURL = KFileDialog::getOpenUrl( QDir::homePath(), "*.obslist|KStars Observing List (*.obslist)" );
    QFile f;
    if ( fileURL.isValid() ) {
        if ( ! fileURL.isLocalFile() ) {
            //Save remote list to a temporary local file
            KTemporaryFile tmpfile;
            tmpfile.setAutoRemove(false);
            tmpfile.open();
            FileName = tmpfile.fileName();
            if( KIO::NetAccess::download( fileURL, FileName, this ) )
                f.setFileName( FileName );

        } else {
            FileName = fileURL.toLocalFile();
            f.setFileName( FileName );
        }

        if ( ! f.open( QIODevice::ReadOnly) ) {
            QString message = i18n( "Could not open file %1", f.fileName() );
            KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
            return;
        }
        saveCurrentList();//See if the current list needs to be saved before opening the new one
        ui->tabWidget->setCurrentIndex(1);
        slotChangeTab(1);
        sessionList().clear();
        TimeHash.clear();
        m_CurrentObject = 0;
        m_Session->removeRows( 0, m_Session->rowCount() );
        //First line is the name of the list. The rest of the file is
        //object names, one per line. With the TimeHash value if present
        QTextStream istream( &f );
        QString input;
        input = istream.readAll();
        OAL::Log logObject;
        logObject.readBegin( input );
        //Set the New TimeHash
        TimeHash = logObject.timeHash();
        geo = logObject.geoLocation();
        dt = logObject.dateTime();
        foreach( SkyObject *o, *( logObject.targetList() ) )
            slotAddObject( o, true );
        //Update the location and user set times from file
        slotUpdate();
        //Newly-opened list should not trigger isModified flag
        isModified = false;
        f.close();
    } else if ( ! fileURL.path().isEmpty() ) { 
        KMessageBox::sorry( 0 , i18n( "The specified file is invalid" ) );
    }
}

void ObservingList::saveCurrentList() {
    //Before loading a new list, do we need to save the current one?
    //Assume that if the list is empty, then there's no need to save
    if ( sessionList().size() ) {
        if ( isModified ) {
            QString message = i18n( "Do you want to save the current session?" );
            if ( KMessageBox::questionYesNo( this, message,
                                             i18n( "Save Current session?" ), KStandardGuiItem::save(), KStandardGuiItem::discard() ) == KMessageBox::Yes )
                slotSaveSession();
        }
    }
}

void ObservingList::slotSaveSessionAs() {
    KUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.obslist|KStars Observing List (*.obslist)" );
    if ( fileURL.isValid() ) {
        FileName = fileURL.path();
        slotSaveSession();
    }
}

void ObservingList::slotSaveList() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "wishlist.obslist" ) );   
    if ( ! f.open( QIODevice::WriteOnly ) ) {
        kDebug() << "Cannot write list to  file";
        return;
    }
    QTextStream ostream( &f );
    foreach ( SkyObject* o, obsList() ) {
        if ( o->name() == "star" ) {
            ostream << o->name() << "  " << o->ra0().Hours() << "  " << o->dec0().Degrees() << endl;
        } else if ( o->type() == SkyObject::STAR ) {
            StarObject *s = (StarObject*)o;
            if ( s->name() == s->gname() ) 
                ostream << s->name2() << endl;
            else  
                ostream << s->name() << endl;
        } else {
            ostream << o->name() << endl;
        }
    }
    f.close();
}

void ObservingList::slotLoadWishList() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "wishlist.obslist" ) );   
    if ( ! f.open( QIODevice::ReadOnly) ) {
       kDebug() << "No WishList Saved yet";
       return;
    }
    QTextStream istream( &f );
    QString line;
    while ( ! istream.atEnd() ) {
        line = istream.readLine();
        //If the object is named "star", add it by coordinates
        SkyObject *o;
        if ( line.startsWith( QLatin1String( "star" ) ) ) {
            QStringList fields = line.split( ' ', QString::SkipEmptyParts );
            dms ra = dms::fromString( fields[1], false ); //false = hours
            dms dc = dms::fromString( fields[2], true );  //true  = degrees
            SkyPoint p( ra, dc );
            double maxrad = 1000.0/Options::zoomFactor();
            o = ks->data()->skyComposite()->starNearest( &p, maxrad );
        } else {
            o = ks->data()->objectNamed( line );
        }
        //If we haven't identified the object, try interpreting the 
        //name as a star's genetive name (with ascii letters)
        if ( ! o ) o = ks->data()->skyComposite()->findStarByGenetiveName( line );
        if ( o ) slotAddObject( o, false, true );
    }
    f.close();
}

void ObservingList::slotSaveSession() {
    if ( FileName.isEmpty() ) {
        slotSaveSessionAs();
        return;
    }
    QFile f( FileName );
    if( ! f.open( QIODevice::WriteOnly ) ) {
        QString message = i18n( "Could not open file %1.  Try a different filename?", f.fileName() );
        if ( KMessageBox::warningYesNo( 0, message, i18n( "Could Not Open File" ), KGuiItem(i18n("Try Different")), KGuiItem(i18n("Do Not Try")) ) == KMessageBox::Yes ) {
            FileName.clear();
            slotSaveSessionAs();
        }
    return;
    }
    QTextStream ostream( &f );
    OAL::Log log;
    ostream<< log.writeLog( nativeSave );
    f.close();
    isModified = false;//We've saved the session, so reset the modified flag.
}

void ObservingList::slotWizard() {
    QPointer<ObsListWizard> wizard = new ObsListWizard( ks );
    if ( wizard->exec() == QDialog::Accepted ) {
        foreach ( SkyObject *o, wizard->obsList() ) {
            slotAddObject( o );
        }
    }
    delete wizard;
}

void ObservingList::plot( SkyObject *o ) {
    if( !o )
        return;
    float DayOffset = 0;
    if( TimeHash.value( o->name(), o->transitTime( dt, geo ) ).hour() > 12 )
        DayOffset = 1;
    KStarsDateTime ut = dt;
    ut.setTime(QTime());
    ut = geo->LTtoUT(ut);
    ut = ut.addSecs( ( 0.5 + DayOffset ) * 86400.0 );

    double h1 = geo->GSTtoLST( ut.gst() ).Hours();
    if ( h1 > 12.0 )
        h1 -= 24.0;

    ui->View->setSecondaryLimits( h1, h1 + 24.0, -90.0, 90.0 );
    ksal->setLocation(geo);
    ui->View->setSunRiseSetTimes( ksal->getSunRise(),ksal->getSunSet() );
    ui->View->update();
    KPlotObject *po = new KPlotObject( Qt::white, KPlotObject::Lines, 2.0 );
    for ( double h = -12.0; h <= 12.0; h += 0.5 ) {
        po->addPoint( h, findAltitude( o, ( h + DayOffset * 24.0 ) ) );
    }
    ui->View->removeAllPlotObjects();
    ui->View->addPlotObject( po );
}

double ObservingList::findAltitude( SkyPoint *p, double hour ) {
    KStarsDateTime ut = dt;
    ut.setTime( QTime() );
    ut = geo->LTtoUT( ut );
    ut= ut.addSecs( hour*3600.0 );
    dms LST = geo->GSTtoLST( ut.gst() );
    p->EquatorialToHorizontal( &LST, geo->lat() );
    return p->alt().Degrees();
}

void ObservingList::slotToggleSize() {
    if ( isLarge() ) {
        ui->MiniButton->setIcon( KIcon("view-fullscreen") );
        //Abbreviate text on each button
        ui->FindButton->setText( i18nc( "First letter in 'Find'", "F" ) );
        //Hide columns 1-5
        ui->TableView->hideColumn(1);
        ui->TableView->hideColumn(2);
        ui->TableView->hideColumn(3);
        ui->TableView->hideColumn(4);
        ui->TableView->hideColumn(5);
        //Hide the headers
        ui->TableView->horizontalHeader()->hide();
        ui->TableView->verticalHeader()->hide();
        //Hide Observing notes
        ui->NotesLabel->hide();
        ui->NotesEdit->hide();
        ui->View->hide();
        //Set the width of the Table to be the width of 5 toolbar buttons, 
        //or the width of column 1, whichever is larger
        int w = 5*ui->MiniButton->width();
        if ( ui->TableView->columnWidth(0) > w ) {
            w = ui->TableView->columnWidth(0);
        } else {
            ui->TableView->setColumnWidth(0, w);
        }
        int left, right, top, bottom;
        ui->layout()->getContentsMargins( &left, &top, &right, &bottom );
        resize( w + left + right, height() );
        bIsLarge = false;
    } else {
        ui->MiniButton->setIcon( KIcon( "view-restore" ) );
        //Show columns 1-5
        ui->TableView->showColumn(1);
        ui->TableView->showColumn(2);
        ui->TableView->showColumn(3);
        ui->TableView->showColumn(4);
        ui->TableView->showColumn(5);
        //Show the horizontal header
        ui->TableView->horizontalHeader()->show();
        //Expand text on each button
        ui->WUTButton->setText( i18n( "WUT") );
        ui->FindButton->setText( i18n( "Find &amp;Object") );
        //Show Observing notes
        ui->NotesLabel->show();
        ui->NotesEdit->show();
        ui->View->show();
        adjustSize();
        bIsLarge = true;
    }
}

void ObservingList::slotChangeTab(int index) {
    noSelection = true;
    saveCurrentUserLog();
    ui->NotesLabel->setText( i18n( "Select an object to record notes on it here:" ) );
    ui->NotesLabel->setEnabled( false );
    ui->NotesEdit->setEnabled( false );
    ui->TimeEdit->setEnabled( false );
    ui->SetTime->setEnabled( false );
    ui->GoogleImage->setEnabled( false );
    ui->SaveImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    m_CurrentObject = 0;
    if( index ) {
        sessionView = true;
    } else {
        sessionView = false;
    }
    setSaveImagesButton();
    ui->WizardButton->setEnabled( ! sessionView );//wizard adds only to the Wish List
    ui->OALExport->setEnabled( sessionView );
    //Clear the selection in the Tables
    ui->TableView->clearSelection();
    ui->SessionView->clearSelection();
    //Clear the user log text box.
    saveCurrentUserLog();
    ui->NotesEdit->setPlainText("");
    ui->View->removeAllPlotObjects();
}

void ObservingList::slotLocation() {
    QPointer<LocationDialog> ld = new LocationDialog( this );
    if ( ld->exec() == QDialog::Accepted ) {
        geo = ld->selectedCity();
        ui->SetLocation -> setText( geo -> fullName() );
    }
    delete ld;
}

void ObservingList::slotUpdate() {
    dt.setDate( ui->DateEdit->date() );
    ui->View->removeAllPlotObjects();
    //Creating a copy of the lists, we can't use the original lists as they'll keep getting modified as the loop iterates
    QList<SkyObject*> _obsList=m_ObservingList, _SessionList=m_SessionList;
    foreach ( SkyObject *o, _obsList ) {
        if( o->name() != "star" ) {
            slotRemoveObject( o, false, true );
            slotAddObject( o, false, true );
        }
    }
    foreach ( SkyObject *obj, _SessionList ) {
        if( obj->name() != "star" ) {  
            slotRemoveObject( obj, true, true );
            slotAddObject( obj, true, true );
        }
    }
}

void ObservingList::slotSetTime() {
    SkyObject *o = currentObject();
    slotRemoveObject( o, true );
    TimeHash [o->name()] = ui->TimeEdit->time();
    slotAddObject( o, true, true );
}

void ObservingList::slotGetImage( bool _dss ) {
    dss = _dss;
    ui->GoogleImage->setEnabled( false );
    ui->ImagePreview->clearPreview();
    if( ! QFile::exists( KStandardDirs::locateLocal( "appdata", CurrentImage ) ) ) 
        setCurrentImage( currentObject(), true );
    QFile::remove( KStandardDirs::locateLocal( "appdata", CurrentImage ) );
    KUrl url;
    if( dss ) {
        url.setUrl( DSSUrl );
    } else {
        url.setUrl( SDSSUrl );
    }
    downloadJob = KIO::copy ( url, KUrl( KStandardDirs::locateLocal( "appdata", CurrentImage ) ) );
    connect ( downloadJob, SIGNAL ( result (KJob *) ), SLOT ( downloadReady() ) );
}

void ObservingList::downloadReady() {
    // set downloadJob to 0, but don't delete it - the job will be deleted automatically
    downloadJob = 0;
    if( QFile( KStandardDirs::locateLocal( "appdata", CurrentImage ) ).size() > 13000 ) {//The default image is around 8689 bytes
        ui->ImagePreview->showPreview( KUrl( KStandardDirs::locateLocal( "appdata", CurrentImage ) ) );
        saveThumbImage();
        ui->ImagePreview->show();
        ui->ImagePreview->setCursor( Qt::PointingHandCursor );
        if( CurrentImage.contains( "Temp" ) ) {
            ImageList.append( CurrentImage );
            ui->SaveImage->setEnabled( true );
        }
        ui->DeleteImage->setEnabled( true );
    } 
    else if( ! dss )
        slotGetImage( true );
}  

void ObservingList::setCurrentImage( SkyObject *o, bool temp  ) {
    QString RAString, DecString, RA, Dec;
    RAString = RAString.sprintf( "%02d+%02d+%02d", o->ra0().hour(), o->ra0().minute(), o->ra0().second() );
    decsgn = '+';
    if ( o->dec0().Degrees() < 0.0 ) decsgn = '-';
    int dd = abs( o->dec0().degree() );
    int dm = abs( o->dec0().arcmin() );
    int ds = abs( o->dec0().arcsec() );
    DecString = DecString.sprintf( "%c%02d+%02d+%02d", decsgn, dd, dm, ds );
    RA = RA.sprintf( "ra=%f", o->ra0().Degrees() );
    Dec = Dec.sprintf( "&dec=%f", o->dec0().Degrees() );
    if( temp )
        CurrentImage = "Temp_Image_" +  o->name().remove(' ');
    else
        CurrentImage = "Image_" +  o->name().remove(' ');
    ThumbImage = "thumb-" + o->name().toLower().remove(' ') + ".png";
    if( o->name() == "star" ) {
        if( temp )
            CurrentImage = "Temp_Image" + RAString + DecString;
        else
            CurrentImage = "Image" + RAString + DecString;
        CurrentImage = CurrentImage.remove('+').remove('-') + decsgn;
    }
    CurrentImagePath = KStandardDirs::locateLocal( "appdata" , CurrentImage );
    CurrentTempPath = KStandardDirs::locateLocal( "appdata", "Temp_" + CurrentImage );
    QString UrlPrefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=1" );
    QString UrlSuffix( "&e=J2000&h=15.0&w=15.0&f=gif&c=none&fov=NONE" );
    DSSUrl = UrlPrefix + "&r=" + RAString + "&d=" + DecString + UrlSuffix;
    UrlPrefix = "http://casjobs.sdss.org/ImgCutoutDR6/getjpeg.aspx?";
    UrlSuffix = "&scale=1.0&width=600&height=600&opt=GST&query=SR(10,20)";
    SDSSUrl = UrlPrefix + RA + Dec + UrlSuffix;
}

void ObservingList::slotSaveAllImages() {
    ui->GoogleImage->setEnabled( false );
    ui->SaveImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    m_CurrentObject = 0;
    //Clear the selection in the Tables
    ui->TableView->clearSelection();
    ui->SessionView->clearSelection();

    if( sessionView ) {
        foreach( SkyObject *o, sessionList() ) {
            setCurrentImage( o );
            QString img( CurrentImagePath  );
            KUrl url( SDSSUrl );
            if( ! o->isSolarSystem() )//TODO find a way for adding support for solar system images
                saveImage( url, img );
        }
    } else {
        foreach( SkyObject *o, obsList() ) {
            setCurrentImage( o );
            QString img( CurrentImagePath  );
            KUrl url( SDSSUrl );
            if( ! o->isSolarSystem() )//TODO find a way for adding support for solar system images
                saveImage( url, img );
        }
    }
}

void ObservingList::saveImage( KUrl url, QString filename ) {
    if( ! QFile::exists( CurrentImagePath  ) && ! QFile::exists( CurrentTempPath ) ) {
        if(  KIO::NetAccess::download( url, filename, mainWidget() ) ) {
            if( QFile( CurrentImagePath ).size() < 13000 ) {//The default image is around 8689 bytes
                url = KUrl( DSSUrl );
                KIO::NetAccess::download( url, filename, mainWidget() );
            }
            saveThumbImage();
        }
    } else if( QFile::exists( CurrentTempPath ) ) {
        QFile f( CurrentTempPath );
        f.rename( CurrentImagePath );
    }
}

void ObservingList::slotSaveImage() {
    setCurrentImage( currentObject() );
    QFile f( CurrentTempPath);
    f.rename( CurrentImagePath );
    ui->SaveImage->setEnabled( false );
}  

void ObservingList::slotImageViewer() {
    ImageViewer *iv = 0;
    if( QFile::exists( CurrentImagePath ) )
        iv = new ImageViewer( CurrentImagePath, this );
    else if( QFile::exists( CurrentTempPath ) )
        iv = new ImageViewer( CurrentTempPath, this );

    if( iv )
        iv->show();
}

void ObservingList::slotDeleteAllImages() {
    if( KMessageBox::warningYesNo( 0, i18n( "This will delete all saved images. Are you sure you want to do this?" ), i18n( "Delete All Images" ) ) == KMessageBox::No )
        return;
    ui->ImagePreview->setCursor( Qt::ArrowCursor );
    ui->GoogleImage->setEnabled( false );
    ui->SaveImage->setEnabled( false );
    ui->DeleteImage->setEnabled( false );
    m_CurrentObject = 0;
    //Clear the selection in the Tables
    ui->TableView->clearSelection();
    ui->SessionView->clearSelection();
    ui->ImagePreview->clearPreview();
    QDirIterator iterator( KStandardDirs::locateLocal( "appdata", "" ) );
    while( iterator.hasNext() )
    {
        // TODO: Probably, there should be a different directory for cached images in the observing list.
        if( iterator.fileName().contains( "Image" ) && ( ! iterator.fileName().contains( "dat" ) ) && ( ! iterator.fileName().contains( "obslist" ) ) ) {
            QFile file( iterator.filePath() );
            file.remove();
        }
        iterator.next();
    }
}

void ObservingList::setSaveImagesButton() {
    ui->saveImages->setEnabled( false );
    if( sessionView ) {
        if( ! sessionList().isEmpty() )
            ui->saveImages->setEnabled( true );
    } else {
        if( ! obsList().isEmpty() )
            ui->saveImages->setEnabled( true );
    }
}

bool ObservingList::eventFilter( QObject *obj, QEvent *event ) {
    if( obj == ui->ImagePreview ) {
        if( event->type() == QEvent::MouseButtonRelease ) {
            if( currentObject() ) {
                if( ( ( QFile( CurrentImagePath ).size() < 13000 ) && (  QFile( CurrentTempPath ).size() < 13000 ) ) ) {
                    if( ! currentObject()->isSolarSystem() )
                        slotGetImage();
                    else
                        slotGoogleImage();
                }
                else
                    slotImageViewer();
            }
            return true;
        }
    }
    if( obj == ui->TableView->viewport() && ! noSelection ) {
        if( event->type() == QEvent::MouseButtonRelease ) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent* >(event);
            if( mouseEvent->button() == Qt::RightButton ) {
                QPoint pos( mouseEvent->globalX() , mouseEvent->globalY() );
                if( singleSelection )
                    pmenu->initPopupMenu( true, true, true, showScope, true, true );
                else
                    pmenu->initPopupMenu( true, false, false, false, true );
                pmenu->popup( pos );
                return true;
            }
        }
    }
    if( obj == ui->SessionView->viewport() && ! noSelection ) {
        if( event->type() == QEvent::MouseButtonRelease ) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent* >(event);
            if( mouseEvent->button() == Qt::RightButton ) {
                QPoint pos( mouseEvent->globalX() , mouseEvent->globalY() );
                if( singleSelection )
                    pmenu->initPopupMenu( false, true, true, showScope, true, true, true );
                else
                    pmenu->initPopupMenu( false, false, false, false, true, false, true );
                pmenu->popup( pos );
                return true;
            }
        }
    }
    return false;
}

void ObservingList::slotGoogleImage() {
    QPixmap *pm = new QPixmap;
    QPointer<ThumbnailPicker> tp = new ThumbnailPicker( currentObject(), *pm, this, 600, 600, i18n( "Image Chooser" ) );
    if ( tp->exec() == QDialog::Accepted ) {
        QFile f( CurrentImagePath );

        //If a real image was set, save it.
        if ( tp->imageFound() ) {
            tp->image()->save( f.fileName(), "PNG" ); 
            ui->ImagePreview->showPreview( KUrl( f.fileName() ) );
            saveThumbImage();
            slotNewSelection();
        }
    }
    delete tp;
}

void ObservingList::slotDeleteCurrentImage() {
    QFile::remove( CurrentImagePath );
    QFile::remove( CurrentTempPath );
    slotNewSelection();
}

void ObservingList::saveThumbImage() {
    if( ! QFile::exists( KStandardDirs::locateLocal( "appdata", ThumbImage ) ) ) {
        QImage img( CurrentImagePath );
        img = img.scaled( 200, 200, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
        img.save( KStandardDirs::locateLocal( "appdata", ThumbImage ) );
    }
}

void ObservingList::slotOALExport() {
    nativeSave = false;
    slotSaveSessionAs();
    nativeSave = true;
}

void ObservingList::slotAddVisibleObj() {
    KStarsDateTime lt = dt;
    lt.setTime( QTime(8,0,0) );
    QPointer<WUTDialog> w = new WUTDialog( ks, sessionView, geo, lt );
    w->init();
    QModelIndexList selectedItems;
    selectedItems = m_SortModel->mapSelectionToSource( ui->TableView->selectionModel()->selection() ).indexes();
    if ( selectedItems.size() )
        foreach ( const QModelIndex &i, selectedItems ) {
            foreach ( SkyObject *o, obsList() )
                if ( o->translatedName() == i.data().toString() && w->checkVisibility( o ) )
                    slotAddObject( o, true );
        }
    delete w;
}

SkyObject* ObservingList::findObjectByName( QString name ) {
    foreach( SkyObject* o, sessionList() )
        if( o->name() == name )
            return o;
    return NULL;
}

void ObservingList::selectObject( SkyObject *o ) {
    ui->tabWidget->setCurrentIndex( 1 );
    ui->SessionView->selectionModel()->clear();
    for ( int irow = m_Session->rowCount()-1; irow >= 0; --irow ) {
        QModelIndex mSortIndex = m_SortModelSession->index( irow, 0 );
        QModelIndex mIndex = m_SortModelSession->mapToSource( mSortIndex );
        int idxrow = mIndex.row();
        if(  m_Session->item(idxrow, 0)->text() == o->translatedName() )
            ui->SessionView->selectRow( idxrow );
        slotNewSelection();
    }
}

void ObservingList::setDefaultImage() {
    QFile file;
    if ( KSUtils::openDataFile( file, "noimage.png" ) ) {
       file.close();
       ui->ImagePreview->showPreview( KUrl( file.fileName() ) );
    } else
        ui->ImagePreview->hide();
}
#include "observinglist.moc"
