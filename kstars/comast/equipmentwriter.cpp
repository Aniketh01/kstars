/***************************************************************************
                          equipmentwriter.cpp  -  description

                             -------------------
    begin                : Friday July 19, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "comast/equipmentwriter.h"
#include "ui_equipmentwriter.h"

#include <QFile>

#include <kstandarddirs.h>

#include "kstarsdata.h"
#include "comast/comast.h"
#include "comast/scope.h"
#include "comast/eyepiece.h"
#include "comast/lens.h"
#include "comast/filter.h"

EquipmentWriter::EquipmentWriter() {
    QWidget *widget = new QWidget;
    ui.setupUi( widget );
    setMainWidget( widget );
    setCaption( i18n( "Define Equipment" ) );
    setButtons( KDialog::Close );
    ks = KStars::Instance();
    nextScope = 0;
    nextEyepiece = 0;
    nextFilter = 0;
    nextLens = 0;
    loadEquipment();
    newScope = true;
    newEyepiece = true;
    newLens = true;
    newFilter = true;

    //make connections
    connect( this, SIGNAL( closeClicked() ), this, SLOT( slotClose() ) );
    connect( ui.NewScope, SIGNAL( clicked() ), this, SLOT( slotNewScope() ) );
    connect( ui.NewEyepiece, SIGNAL( clicked() ), this, SLOT( slotNewEyepiece() ) );
    connect( ui.NewLens, SIGNAL( clicked() ), this, SLOT( slotNewLens() ) );
    connect( ui.NewFilter, SIGNAL( clicked() ), this, SLOT( slotNewFilter() ) );
    connect( ui.AddScope, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( ui.AddEyepiece, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( ui.AddLens, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( ui.AddFilter, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( ui.ScopeList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetScope(QString) ) );
    connect( ui.EyepieceList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetEyepiece(QString) ) );
    connect( ui.LensList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetLens(QString) ) );
    connect( ui.FilterList, SIGNAL( currentTextChanged(const QString) ),
             this, SLOT( slotSetFilter(QString) ) );
    connect( ui.RemoveScope, SIGNAL( clicked() ), this, SLOT( slotRemoveScope() ) );
    connect( ui.RemoveEyepiece, SIGNAL( clicked() ), this, SLOT( slotRemoveEyepiece() ) );
    connect( ui.RemoveLens, SIGNAL( clicked() ), this, SLOT( slotRemoveLens() ) );
    connect( ui.RemoveFilter, SIGNAL( clicked() ), this, SLOT( slotRemoveFilter() ) );
}

void EquipmentWriter::slotAddScope() {
    while ( ks->data()->logObject()->findScopeById( i18nc( "prefix for ID number identifying a telescope (optional)", "scope" ) + '_' + QString::number( nextScope ) ) )
    nextScope++;
    Comast::Scope *s = new Comast::Scope( i18nc( "prefix for ID number identifying a telescope (optional)", "scope" ) + '_' + QString::number( nextScope++ ), ui.Model->text(), ui.Vendor->text(), ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value() ); 
    ks->data()->logObject()->scopeList()->append( s );
    saveEquipment(); //Save the new list.
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
}

void EquipmentWriter::slotRemoveScope() {
    Comast::Scope *s = ks->data()->logObject()->findScopeByName( ui.Id->text() );
    ks->data()->logObject()->scopeList()->removeAll( s );
    saveEquipment(); //Save the new list.
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.ScopeList->clear();
    foreach( Comast::Scope *s, *( ks->data()->logObject()->scopeList() ) )
        ui.ScopeList->addItem( s->name() );
}

void EquipmentWriter::slotSaveScope() {
    Comast::Scope *s = ks->data()->logObject()->findScopeByName( ui.Id->text() );
    if( s ) {
        s->setScope( ui.Id->text(), ui.Model->text(), ui.Vendor->text(), ui.Type->currentText(), ui.FocalLength->value(), ui.Aperture->value() );
    }
    saveEquipment(); //Save the new list.
}
 
void EquipmentWriter::slotSetScope( QString name) {
    Comast::Scope *s = ks->data()->logObject()->findScopeByName( name );
    if ( s ) {
        ui.Id->setText( s->id() ) ;
        ui.Model->setText( s->model() );
        ui.Vendor->setText( s->vendor() );
        ui.Type->setCurrentIndex( ui.Type->findText( s->type() ) );
        ui.FocalLength->setValue( s->focalLength() );
        ui.Aperture->setValue( s->aperture() );
        newScope = false;
    }
}
void EquipmentWriter::slotNewScope() {
    ui.Id->clear();
    ui.Model->clear();
    ui.Vendor->clear();
    ui.FocalLength->setValue(0);
    ui.ScopeList->selectionModel()->clear();
    newScope = true;
}

void EquipmentWriter::slotAddEyepiece() {
    while ( ks->data()->logObject()->findEyepieceById( i18nc("prefix for ID number identifying an eyepiece (optional)", "eyepiece") + '_' + QString::number( nextEyepiece ) ) )
    nextEyepiece++;
    Comast::Eyepiece *e = new Comast::Eyepiece( i18nc("prefix for ID number identifying an eyepiece (optional)", "eyepiece") + '_' + QString::number( nextEyepiece++ ), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->currentText(), ui.e_focalLength->value() );
    ks->data()->logObject()->eyepieceList()->append( e );
    saveEquipment(); //Save the new list.
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
}

void EquipmentWriter::slotRemoveEyepiece() {
    Comast::Eyepiece *e = ks->data()->logObject()->findEyepieceByName( ui.e_Id->text() );
    ks->data()->logObject()->eyepieceList()->removeAll( e );
    saveEquipment(); //Save the new list.
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
    ui.EyepieceList->clear();
    foreach( Comast::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
        ui.EyepieceList->addItem( e->name() );
}
void EquipmentWriter::slotSaveEyepiece() {
    Comast::Eyepiece *e = ks->data()->logObject()->findEyepieceByName( ui.e_Id->text() );
    if( e ){
        e->setEyepiece( ui.e_Id->text(), ui.e_Model->text(), ui.e_Vendor->text(), ui.Fov->value(), ui.FovUnit->currentText(), ui.e_focalLength->value() );
    } 
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetEyepiece( QString name ) {
    Comast::Eyepiece *e;
    e = ks->data()->logObject()->findEyepieceByName( name ); 
    if( e ) {
        ui.e_Id->setText( e->id() );
        ui.e_Model->setText( e->model() );
        ui.e_Vendor->setText( e->vendor() );
        ui.Fov->setValue( e->appFov() );
        ui.e_focalLength->setValue( e->focalLength() );
        newEyepiece = false;
    }
}

void EquipmentWriter::slotNewEyepiece() {
    ui.e_Id->clear();
    ui.e_Model->clear();
    ui.e_Vendor->clear();
    ui.Fov->setValue(0);
    ui.e_focalLength->setValue(0);
    ui.EyepieceList->selectionModel()->clear();
    newEyepiece = true;
}

void EquipmentWriter::slotAddLens() {
    while ( ks->data()->logObject()->findLensById( i18nc("prefix for ID number identifying a lens (optional)", "lens") + '_' + QString::number( nextLens ) ) )
    nextLens++;
    Comast::Lens *l = new Comast::Lens( i18nc("prefix for ID number identifying a lens (optional)", "lens") + '_' + QString::number( nextLens++ ), ui.l_Model->text(), ui.l_Vendor->text(), ui.l_Factor->value() );
    ks->data()->logObject()->lensList()->append( l );
    saveEquipment(); //Save the new list.
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
}

void EquipmentWriter::slotRemoveLens() {
    Comast::Lens *l = ks->data()->logObject()->findLensByName( ui.l_Id->text() );
    ks->data()->logObject()->lensList()->removeAll( l );
    saveEquipment(); //Save the new list.
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
    ui.LensList->clear();
    foreach( Comast::Lens *l, *( ks->data()->logObject()->lensList() ) )
        ui.LensList->addItem( l->name() );
}
void EquipmentWriter::slotSaveLens() {
    Comast::Lens *l = ks->data()->logObject()->findLensByName( ui.l_Id->text() );
    if( l ){
        l->setLens( ui.l_Id->text(), ui.l_Model->text(), ui.l_Vendor->text(), ui.l_Factor->value() );
    }
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetLens( QString name ) {
    Comast::Lens *l;
    l = ks->data()->logObject()->findLensByName( name );
    if( l ) {
        ui.l_Id->setText( l->id() );
        ui.l_Model->setText( l->model() );
        ui.l_Vendor->setText( l->vendor() );
        ui.l_Factor->setValue( l->factor() );
        newLens = false;
    }
}

void EquipmentWriter::slotNewLens() {
    ui.l_Id->clear();
    ui.l_Model->clear();
    ui.l_Vendor->clear();
    ui.l_Factor->setValue(0);
    ui.LensList->selectionModel()->clear();
    newLens = true;
}

void EquipmentWriter::slotAddFilter() {
    while ( ks->data()->logObject()->findFilterById( i18nc("prefix for ID number identifying a filter (optional)", "filter") + '_' + QString::number( nextFilter ) ) )
    nextFilter++;
    Comast::Filter *f = new Comast::Filter( i18nc("prefix for ID number identifying a filter (optional)", "filter") + '_' + QString::number( nextFilter++ ), ui.f_Model->text(), ui.f_Vendor->text(), ui.f_Type->text(), ui.f_Color->text() );
    ks->data()->logObject()->filterList()->append( f );
    saveEquipment(); //Save the new list.
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
}

void EquipmentWriter::slotRemoveFilter() {
    Comast::Filter *f = ks->data()->logObject()->findFilterByName( ui.f_Id->text() );
    ks->data()->logObject()->filterList()->removeAll( f );
    saveEquipment(); //Save the new list.
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
    ui.FilterList->clear();
    foreach( Comast::Filter *f, *( ks->data()->logObject()->filterList() ) )
        ui.FilterList->addItem( f->name() );
}

void EquipmentWriter::slotSaveFilter() {
    Comast::Filter *f = ks->data()->logObject()->findFilterByName( ui.f_Id->text() );
    if( f ){
        f->setFilter( ui.f_Id->text(), ui.f_Model->text(), ui.f_Vendor->text(), ui.f_Type->text(), ui.f_Color->text() );
    } 
    saveEquipment(); //Save the new list.
}

void EquipmentWriter::slotSetFilter( QString name ) {
    Comast::Filter *f;
    f = ks->data()->logObject()->findFilterByName( name ); 
    if( f ) {
        ui.f_Id->setText( f->id() );
        ui.f_Model->setText( f->model() );
        ui.f_Vendor->setText( f->vendor() );
        ui.f_Type->setText( f->type() );
        ui.f_Color->setText( f->color() );
        newFilter = false;
    }
}

void EquipmentWriter::slotNewFilter() {
    ui.f_Id->clear();
    ui.f_Model->clear();
    ui.f_Vendor->clear();
    ui.f_Type->clear();
    ui.f_Color->clear();
    ui.FilterList->selectionModel()->clear();
    newFilter = true;
}

void EquipmentWriter::saveEquipment() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "equipmentlist.xml" ) );   
    if ( ! f.open( QIODevice::WriteOnly ) ) {
        kDebug() << "Cannot write list to  file";
        return;
    }
    QTextStream ostream( &f );
    ks->data()->logObject()->writeBegin();
    ks->data()->logObject()->writeScopes();
    ks->data()->logObject()->writeEyepieces();
    ks->data()->logObject()->writeLenses();
    ks->data()->logObject()->writeFilters();
    ks->data()->logObject()->writeEnd();
    ostream << ks->data()->logObject()->writtenOutput();
    f.close();
}

void EquipmentWriter::loadEquipment() {
    QFile f;
    f.setFileName( KStandardDirs::locateLocal( "appdata", "equipmentlist.xml" ) );   
    if( ! f.open( QIODevice::ReadOnly ) )
        return;
    QTextStream istream( &f );
    ks->data()->logObject()->readBegin( istream.readAll() );
    f.close();
    ui.ScopeList->clear();
    ui.EyepieceList->clear();
    ui.LensList->clear();
    ui.FilterList->clear();
    foreach( Comast::Scope *s, *( ks->data()->logObject()->scopeList() ) )
        ui.ScopeList->addItem( s->name() );
    foreach( Comast::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
        ui.EyepieceList->addItem( e->name() );
    foreach( Comast::Lens *l, *( ks->data()->logObject()->lensList() ) )
        ui.LensList->addItem( l->name() );
    foreach( Comast::Filter *f, *( ks->data()->logObject()->filterList() ) )
        ui.FilterList->addItem( f->name() );
}

void EquipmentWriter::slotSave() {
    switch( ui.tabWidget->currentIndex() ) {
        case 0: {
            if( newScope )
                slotAddScope();
            else
                slotSaveScope();
            ui.ScopeList->clear();
            foreach( Comast::Scope *s, *( ks->data()->logObject()->scopeList() ) )
                ui.ScopeList->addItem( s->name() );
            break;
        }
        case 1: {
            if( newEyepiece )
                slotAddEyepiece();
            else
                slotSaveEyepiece();
            ui.EyepieceList->clear();
            foreach( Comast::Eyepiece *e, *( ks->data()->logObject()->eyepieceList() ) )
                ui.EyepieceList->addItem( e->name() );
            break;
        }
        case 2: {
            if( newLens )
                slotAddLens();
            else
                slotSaveLens();
            ui.LensList->clear();
            foreach( Comast::Lens *l, *( ks->data()->logObject()->lensList() ) )
                ui.LensList->addItem( l->name() );
            break;
        }
        case 3: {
            if( newFilter )
                slotAddFilter();
            else
                slotSaveFilter();
            ui.FilterList->clear();
            foreach( Comast::Filter *f, *( ks->data()->logObject()->filterList() ) )
                ui.FilterList->addItem( f->name() );
            break;
        }
    }
}
#include "equipmentwriter.moc"
