/***************************************************************************
                          opscatalog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29 2004
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

#ifndef OPSCATALOG_H_
#define OPSCATALOG_H_

#include "ui_opscatalog.h"

/**@class OpsCatalog
	*The Catalog page for the Options window.  This page allows the user
	*to modify display of the major object catalogs in KStars:
	*@li Hipparcos/Tycho Star Catalog
	*@li Messier Catalog
	*@li NGC/IC Catalogs
	*@li Any Custom catalogs added by the user.
	*
	*@short Catalog page of the Options window.
	*@author Jason Harris
	*@version 1.0
	*/

class QWidget;
class KStars;
class QListWidgetItem;
class CustomCatalogComponent;

class OpsCatalog : public QFrame, public Ui::OpsCatalog
{
	Q_OBJECT

public:
	OpsCatalog( KStars *_ks );
	~OpsCatalog();

private slots:
	void updateDisplay();
	void selectCatalog();
	void slotAddCatalog();
	void slotLoadCatalog();
	void slotRemoveCatalog();
	void slotSetDrawStarMagnitude(double newValue);
	void slotSetDrawStarZoomOutMagnitude(double newValue);
	void slotStarWidgets(bool on);
	
private:
	void insertCatalog( const QString & filename );
	QString getCatalogName( const QString &filename );

	QListWidgetItem *showMessier, *showMessImages, *showNGC, *showIC;
	KStars *ksw;
};

#endif  //OPSCATALOG_H_

