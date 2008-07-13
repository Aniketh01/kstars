/***************************************************************************
                          opscatalog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29  2004
    copyright            : (C) 2004 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "opscatalog.h"

#include <QList>
#include <QListWidgetItem>
#include <QTextStream>

#include <kfiledialog.h>
#include <kactioncollection.h>
#include <kconfigdialog.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "addcatdialog.h"
#include "widgets/magnitudespinbox.h"
#include "skycomponents/customcatalogcomponent.h"

OpsCatalog::OpsCatalog( KStars *_ks )
        : QFrame( _ks ), ksw(_ks)
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

    //Populate CatalogList
    showIC = new QListWidgetItem( i18n( "Index Catalog (IC)" ), CatalogList );
    showIC->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showIC->setCheckState( Options::showIC() ?  Qt::Checked : Qt::Unchecked );

    showNGC = new QListWidgetItem( i18n( "New General Catalog (NGC)" ), CatalogList );
    showNGC->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showNGC->setCheckState( Options::showNGC() ?  Qt::Checked : Qt::Unchecked );

    showMessImages = new QListWidgetItem( i18n( "Messier Catalog (images)" ), CatalogList );
    showMessImages->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showMessImages->setCheckState( Options::showMessierImages()  ?  Qt::Checked : Qt::Unchecked );

    showMessier = new QListWidgetItem( i18n( "Messier Catalog (symbols)" ), CatalogList );
    showMessier->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showMessier->setCheckState( Options::showMessier() ?  Qt::Checked : Qt::Unchecked );

    m_ShowMessier = Options::showMessier();
    m_ShowMessImages = Options::showMessierImages();
    m_ShowNGC = Options::showNGC();
    m_ShowIC = Options::showIC();

    //    kcfg_MagLimitDrawStar->setValue( Options::magLimitDrawStar() );
    kcfg_MemUsage->setValue( Options::memUsage() );
    kcfg_MagLimitDrawStarZoomOut->setValue( Options::magLimitDrawStarZoomOut() );
    //    m_MagLimitDrawStar = kcfg_MagLimitDrawStar->value();
    m_MemUsage = kcfg_MemUsage->value();
    m_MagLimitDrawStarZoomOut = kcfg_MagLimitDrawStarZoomOut->value();

    //    kcfg_MagLimitDrawStar->setMinimum( Options::magLimitDrawStarZoomOut() );
    kcfg_MagLimitDrawStarZoomOut->setMaximum( 12.0 );

    kcfg_MagLimitDrawDeepSky->setMaximum( 16.0 );
    kcfg_MagLimitDrawDeepSkyZoomOut->setMaximum( 16.0 );

    //disable star-related widgets if not showing stars
    if ( ! kcfg_ShowStars->isChecked() ) slotStarWidgets(false);

    //Add custom catalogs, if necessary
    for ( int i = 0; i < ksw->data()->skyComposite()->customCatalogs().size(); ++i ) {
        CustomCatalogComponent *cc = ((CustomCatalogComponent*)ksw->data()->skyComposite()->customCatalogs()[i]);
        QListWidgetItem *newItem = new QListWidgetItem( cc->name(), CatalogList );
        newItem->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
        newItem->setCheckState( Options::showCatalog()[i] ?  Qt::Checked : Qt::Unchecked );
    }

    m_CustomCatalogFile = Options::catalogFile();
    m_ShowCustomCatalog = Options::showCatalog();

    connect( CatalogList, SIGNAL( itemClicked( QListWidgetItem* ) ), this, SLOT( updateCustomCatalogs() ) );
    connect( CatalogList, SIGNAL( itemSelectionChanged() ), this, SLOT( selectCatalog() ) );
    connect( AddCatalog, SIGNAL( clicked() ), this, SLOT( slotAddCatalog() ) );
    connect( LoadCatalog, SIGNAL( clicked() ), this, SLOT( slotLoadCatalog() ) );
    connect( RemoveCatalog, SIGNAL( clicked() ), this, SLOT( slotRemoveCatalog() ) );

    /*
    connect( kcfg_MagLimitDrawStar, SIGNAL( valueChanged(double) ),
             SLOT( slotSetDrawStarMagnitude(double) ) );
    connect( kcfg_MagLimitDrawStarZoomOut, SIGNAL( valueChanged(double) ),
             SLOT( slotSetDrawStarZoomOutMagnitude(double) ) );
    */
    connect( kcfg_ShowStars, SIGNAL( toggled(bool) ), SLOT( slotStarWidgets(bool) ) );
    connect( kcfg_ShowDeepSky, SIGNAL( toggled(bool) ), SLOT( slotDeepSkyWidgets(bool) ) );
    connect( m_ConfigDialog, SIGNAL( applyClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( okClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( cancelClicked() ), SLOT( slotCancel() ) );
}

//empty destructor
OpsCatalog::~OpsCatalog() {}

void OpsCatalog::updateCustomCatalogs() {
    m_ShowMessier = showMessier->checkState();
    m_ShowMessImages = showMessImages->checkState();
    m_ShowNGC = showNGC->checkState();
    m_ShowIC = showIC->checkState();

    for ( int i=0; i < ksw->data()->skyComposite()->customCatalogs().size(); ++i ) {
        QString name = ((CustomCatalogComponent*)ksw->data()->skyComposite()->customCatalogs()[i])->name();
        QList<QListWidgetItem*> l = CatalogList->findItems( name, Qt::MatchExactly );
        m_ShowCustomCatalog[i] = (l[0]->checkState()==Qt::Checked) ? 1 : 0;
    }

    m_ConfigDialog->enableButtonApply( true );
}

void OpsCatalog::selectCatalog() {
    //If selected item is a custom catalog, enable the remove button (otherwise, disable it)
    RemoveCatalog->setEnabled( false );
    foreach ( SkyComponent *sc, ksw->data()->skyComposite()->customCatalogs() ) {
        CustomCatalogComponent *cc = (CustomCatalogComponent*)sc;
        if ( CatalogList->currentItem()->text() == cc->name() ) {
            RemoveCatalog->setEnabled( true );
            break;
        }
    }
}

void OpsCatalog::slotAddCatalog() {
    AddCatDialog ac( ksw );
    if ( ac.exec()==QDialog::Accepted )
        insertCatalog( ac.filename() );
}

void OpsCatalog::slotLoadCatalog() {
    //Get the filename from the user
    QString filename = KFileDialog::getOpenFileName( QDir::homePath(), "*");
    if ( ! filename.isEmpty() ) {
        //test integrity of file before trying to add it
      CustomCatalogComponent newCat( ksw->data()->skyComposite(), filename, true, Options::showOther );
        newCat.init( ksw->data() );
        if ( newCat.objectList().size() )
            insertCatalog( filename );
    }
}

void OpsCatalog::insertCatalog( const QString &filename ) {
    //Get the new catalog's name, add entry to the listbox
    QString name = getCatalogName( filename );

    QListWidgetItem *newItem = new QListWidgetItem( name, CatalogList );
    newItem->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    newItem->setCheckState( Qt::Checked );

    m_CustomCatalogFile.append( filename );
    m_ShowCustomCatalog.append( true );

    m_ConfigDialog->enableButtonApply( true );
}

void OpsCatalog::slotRemoveCatalog() {
    //Remove CatalogName, CatalogFile, and ShowCatalog entries, and decrement CatalogCount
    for ( int i=0; i < ksw->data()->skyComposite()->customCatalogs().size(); ++i ) {
        CustomCatalogComponent *cc = ((CustomCatalogComponent*)ksw->data()->skyComposite()->customCatalogs()[i]);
        QString name = cc->name();

        if ( CatalogList->currentItem()->text() == name ) {
            m_CustomCatalogFile.removeAll( m_CustomCatalogFile[i] );
            m_ShowCustomCatalog.removeAll( m_ShowCustomCatalog[i] );
            break;
        }
    }

    //Remove entry in the QListView
    CatalogList->takeItem( CatalogList->row( CatalogList->currentItem() ) );

    m_ConfigDialog->enableButtonApply( true );
}

/*
void OpsCatalog::slotSetDrawStarMagnitude(double newValue) {
    m_MagLimitDrawStar = newValue; 
    kcfg_MagLimitDrawStarZoomOut->setMaximum( newValue );
    m_ConfigDialog->enableButtonApply( true );
}

void OpsCatalog::slotSetDrawStarZoomOutMagnitude(double newValue) {
    m_MagLimitDrawStarZoomOut = newValue;
    kcfg_MagLimitDrawStar->setMinimum( newValue );
    m_ConfigDialog->enableButtonApply( true );
}
*/

void OpsCatalog::slotApply() {
    Options::setMemUsage( kcfg_MemUsage->value() );
    Options::setMagLimitDrawStarZoomOut( kcfg_MagLimitDrawStarZoomOut->value() );

    //FIXME: need to add the ShowDeepSky meta-option to the config dialog!
    //For now, I'll set showDeepSky to true if any catalog options changed
    if ( m_ShowMessier != Options::showMessier() || m_ShowMessImages != Options::showMessierImages()
         || m_ShowNGC != Options::showNGC() || m_ShowIC != Options::showIC() ) {
        Options::setShowDeepSky( true );
    }

    Options::setShowMessier( m_ShowMessier );
    Options::setShowMessierImages( m_ShowMessImages );
    Options::setShowNGC( m_ShowNGC );
    Options::setShowIC( m_ShowIC );

    //Remove custom catalogs as needed
    for ( int i=0; i < Options::catalogFile().size(); ++i ) {
        QString filename = Options::catalogFile()[i];

        if ( ! m_CustomCatalogFile.contains( filename ) ) {
            //Remove this catalog
            QString name = getCatalogName( filename );
            ksw->data()->skyComposite()->removeCustomCatalog( name );
        }
    }

    //Add custom catalogs as needed
    for ( int i=0; i < m_CustomCatalogFile.size(); ++i ) {
        QString filename = m_CustomCatalogFile[i];

        if ( ! Options::catalogFile().contains( filename ) ) {
            //Add this catalog
            ksw->data()->skyComposite()->addCustomCatalog( filename, ksw->data(),  Options::showOther );
        }
    }

    Options::setCatalogFile( m_CustomCatalogFile );
    Options::setShowCatalog( m_ShowCustomCatalog );

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    ksw->data()->setFullTimeUpdate();
    ksw->updateTime();
    ksw->map()->forceUpdate();
}

void OpsCatalog::slotCancel() {
    //Revert all local option placeholders to the values in the global config object
    //    m_MagLimitDrawStar = Options::magLimitDrawStar();
    m_MemUsage = Options::memUsage();
    m_MagLimitDrawStarZoomOut = Options::magLimitDrawStarZoomOut();

    m_ShowMessier = Options::showMessier();
    m_ShowMessImages = Options::showMessierImages();
    m_ShowNGC = Options::showNGC();
    m_ShowIC = Options::showIC();

    m_CustomCatalogFile = Options::catalogFile();
    m_ShowCustomCatalog = Options::showCatalog();

}

void OpsCatalog::slotStarWidgets(bool on) {
    //    LabelMagStars->setEnabled(on);
    LabelMemUsage->setEnabled(on);
    LabelMagStarsZoomOut->setEnabled(on);
    LabelDensity->setEnabled(on);
    //    LabelMag1->setEnabled(on);
    LabelMag2->setEnabled(on);
    //    kcfg_MagLimitDrawStar->setEnabled(on);
    kcfg_MemUsage->setEnabled(on);
    kcfg_MagLimitDrawStarZoomOut->setEnabled(on);
    kcfg_StarLabelDensity->setEnabled(on);
    kcfg_ShowStarNames->setEnabled(on);
    kcfg_ShowStarMagnitudes->setEnabled(on);
}

void OpsCatalog::slotDeepSkyWidgets(bool on) {
    CatalogList->setEnabled( on );
    AddCatalog->setEnabled( on );
    LoadCatalog->setEnabled( on );
    LabelMagDeepSky->setEnabled( on );
    LabelMagDeepSkyZoomOut->setEnabled( on );
    kcfg_MagLimitDrawDeepSky->setEnabled( on );
    kcfg_MagLimitDrawDeepSkyZoomOut->setEnabled( on );
    LabelMag3->setEnabled( on );
    LabelMag4->setEnabled( on );
    if ( on ) {
        //Enable RemoveCatalog if the selected catalog is custom
        selectCatalog();
    } else {
        RemoveCatalog->setEnabled( on );
    }
}

QString OpsCatalog::getCatalogName( const QString &filename ) {
    QString name = QString();
    QFile f( filename );

    if ( f.open( QIODevice::ReadOnly ) ) {
        QTextStream stream( &f );
        while ( ! stream.atEnd() ) {
            QString line = stream.readLine();
            if ( line.indexOf( "# Name: " ) == 0 ) {
                name = line.mid( line.indexOf(":")+2 );
                break;
            }
        }
    }

    return name;  //no name was parsed
}

#include "opscatalog.moc"
