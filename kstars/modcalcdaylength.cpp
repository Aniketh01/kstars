/***************************************************************************
                          modcalcdaylength.cpp  -  description
                             -------------------
    begin                : wed jun 12 2002
    copyright            : (C) 2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modcalcdaylength.h"
#include "modcalcprec.h"
#include "dms.h"
#include "dmsbox.h"
#include "skyobject.h"
#include "ksutils.h"
#include "geolocation.h"
#include "kstars.h"
#include "timebox.h"


#include <kapplication.h> //...already included in modcalcdaylength.h
#include <qdatetimeedit.h>

modCalcDayLength::modCalcDayLength(QWidget *parentSplit, const char *name) : modCalcDayLengthDlg(parentSplit,name) {

	showCurrentDate();
 	initGeo();
	showLongLat();
	this->show();

}

modCalcDayLength::~modCalcDayLength(){
}

void modCalcDayLength::showCurrentDate (void)
{
	QDateTime dt = QDateTime::currentDateTime();
	datBox->setDate( dt.date() );
}

void modCalcDayLength::initGeo(void)
{
	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	geoPlace = new GeoLocation( ks->geo() );
}

void modCalcDayLength::showLongLat(void)
{

	KStars *ks = (KStars*) parent()->parent()->parent(); // QSplitter->AstroCalc->KStars
	longBox->show( ks->geo()->lng() );
	latBox->show( ks->geo()->lat() );
}

void modCalcDayLength::getGeoLocation (void)
{
	geoPlace->setLong( longBox->createDms() );
	geoPlace->setLat(  latBox->createDms() );
	geoPlace->setHeight( 0.0);

}

QDateTime modCalcDayLength::getQDateTime (void)
{
	QDateTime dt ( datBox->date() , QTime(8,0,0) );
	return dt;
}

long double modCalcDayLength::computeJdFromCalendar (void)
{
	long double julianDay;

	julianDay = KSUtils::UTtoJulian( getQDateTime() );

	return julianDay;
}

void modCalcDayLength::slotClearCoords(){

	azSetBox->clearFields();
	azRiseBox->clearFields();
	elTransitBox->clearFields();

	// reset to current date
	datBox->setDate(QDate::currentDate());

// reset times to 00:00:00
/*	QTime time(0,0,0);
	setTimeBox->setTime(time);
	riseTimeBox->setTime(time);
	transitTimeBox->setTime(time);*/
	setTimeBox->clearFields();
	riseTimeBox->clearFields();
	transitTimeBox->clearFields();

//	dayLBox->setTime(time);
	dayLBox->clearFields();
}

QTime modCalcDayLength::lengthOfDay(QTime setQTime, QTime riseQTime){

	QTime dL(0,0,0);

  int dds = riseQTime.secsTo(setQTime);
  QTime dLength = dL.addSecs( dds );

	return dLength;
}

void modCalcDayLength::slotComputePosTime()
{

	long double jd0 = computeJdFromCalendar();
	getGeoLocation();

	KSNumbers * num = new KSNumbers(jd0);
	KSSun *Sun = new KSSun((KStars*) parent()->parent()->parent());
	Sun->findPosition(num);

	QTime setQtime = Sun->setTime(jd0 , geoPlace);
	QTime riseQtime = Sun->riseTime(jd0 , geoPlace);
	QTime transitQtime = Sun->transitTime(jd0 , geoPlace);

	dms setAz = Sun->riseSetTimeAz(jd0, geoPlace, false);
	dms riseAz = Sun->riseSetTimeAz(jd0, geoPlace, true);
	dms transAlt = Sun->transitAltitude(jd0, geoPlace);

	azSetBox->show( setAz );
	elTransitBox->show( transAlt );
	azRiseBox->show( riseAz );

/*	setTimeBox->setTime( setQtime );
	riseTimeBox->setTime( riseQtime );
	transitTimeBox->setTime( transitQtime );*/
	setTimeBox->showTime( setQtime );
	riseTimeBox->showTime( riseQtime );
	transitTimeBox->showTime( transitQtime );

	QTime dayLQtime = lengthOfDay (setQtime,riseQtime);

//	dayLBox->setTime( dayLQtime );
	dayLBox->showTime( dayLQtime );

	delete num;

}
#include "modcalcdaylength.moc"
