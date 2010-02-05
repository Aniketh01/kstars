/***************************************************************************
                          skyline.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon June 26 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstarss@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skyline.h"
#include "kstarsdata.h"
#include "ksnumbers.h"

SkyLine::SkyLine()
{}

SkyLine::SkyLine( const SkyPoint &start, const SkyPoint &end ) {
    SkyPoint *pStart = new SkyPoint( start );
    SkyPoint *pEnd   = new SkyPoint( end );
    m_pList.append( pStart );
    m_pList.append( pEnd );
}

SkyLine::SkyLine( SkyPoint *start, SkyPoint *end ) {
    SkyPoint *pStart = new SkyPoint( *start );
    SkyPoint *pEnd   = new SkyPoint( *end );
    m_pList.append( pStart );
    m_pList.append( pEnd );
}

SkyLine::SkyLine( QList<SkyPoint*> list ) {
    foreach ( SkyPoint *p, list ) {
        SkyPoint *p0 = new SkyPoint( *p );
        m_pList.append( p0 );
    }
}

SkyLine::~SkyLine() {
  clear();
}

void SkyLine::clear() {
  if ( m_pList.size() ) {
    qDeleteAll( m_pList );
    m_pList.clear();
  }
}

void SkyLine::append( const SkyPoint &p ) {
    SkyPoint *pAdd = new SkyPoint( p );
    m_pList.append( pAdd );
}

void SkyLine::append( SkyPoint *p ) {
    SkyPoint *pAdd = new SkyPoint( *p );
    m_pList.append( pAdd );
}

void SkyLine::setPoint( int i, SkyPoint *p ) {
    if ( i < 0 || i >= m_pList.size() ) {
        kDebug() << i18n("SkyLine index error: no such point: %1", i );
        return;
    }
    *( m_pList[i] ) = *p;
}

dms SkyLine::angularSize( int i ) const{
    if ( i < 0 || i+1 >= m_pList.size() ) {
        kDebug() << i18n("SkyLine index error: no such segment: %1", i );
        return dms();
    }

    SkyPoint *p1 = m_pList[i];
    SkyPoint *p2 = m_pList[i+1];
    double dalpha = p1->ra()->radians() - p2->ra()->radians();
    double ddelta = p1->dec()->radians() - p2->dec()->radians() ;

    double sa = sin(dalpha/2.);
    double sd = sin(ddelta/2.);

    double hava = sa*sa;
    double havd = sd*sd;

    double aux = havd + cos (p1->dec()->radians()) * cos(p2->dec()->radians()) * hava;

    dms angDist;
    angDist.setRadians( 2 * fabs(asin( sqrt(aux) )) );

    return angDist;
}

//DEPRECATED
// void SkyLine::setAngularSize( double size ) {
// 	double pa=positionAngle().radians();
//
// 	double x = (startPoint()->ra()->Degrees()  + size*cos(pa))/15.0;
// 	double y = startPoint()->dec()->Degrees() - size*sin(pa);
//
// 	setEndPoint( SkyPoint( x, y ) );
// }

dms SkyLine::positionAngle( int i ) const {
    if ( i < 0 || i+1 >= m_pList.size() ) {
        kDebug() << i18n("SkyLine index error: no such segment: %1", i );
        return dms();
    }

    SkyPoint *p1 = m_pList[i];
    SkyPoint *p2 = m_pList[i+1];
    double dx = p1->ra()->radians() - p2->ra()->radians();
    double dy = p2->dec()->radians() - p1->dec()->radians();

    return dms( atan2( dy, dx )/dms::DegToRad );
}

//DEPRECATE
// void SkyLine::rotate( const dms &angle ) {
// 	double dx = endPoint()->ra()->Degrees()  - startPoint()->ra()->Degrees();
// 	double dy = endPoint()->dec()->Degrees() - startPoint()->dec()->Degrees();
//
// 	double s, c;
// 	angle.SinCos( s, c );
//
// 	double dx0 = dx*c - dy*s;
// 	double dy0 = dx*s + dy*c;
//
// 	double x = (startPoint()->ra()->Degrees() + dx0)/15.0;
// 	double y = startPoint()->dec()->Degrees() + dy0;
//
// 	setEndPoint( SkyPoint( x, y ) );
// }

void SkyLine::update( KStarsData *d, KSNumbers *num ) {
    foreach ( SkyPoint *p, m_pList ) {
        if ( num )
            p->updateCoords( num );

        p->EquatorialToHorizontal( d->lst(), d->geo()->lat() );
    }
}
