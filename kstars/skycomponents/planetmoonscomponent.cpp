/***************************************************************************
                         planetmoonscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Mar 13 2009
    copyright            : (C) 2009 by Vipul Kumar Singh, Médéric Boquien
    email                : vipulkrsingh@gmail.com, mboquien@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "planetmoonscomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>

#include "skyobjects/jupitermoons.h"
#include "skyobjects/ksplanetbase.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skypoint.h" 
#include "skyobjects/trailobject.h" 
#include "dms.h"
#include "Options.h"
#include "solarsystemsinglecomponent.h"
#include "solarsystemcomposite.h"
#include "skylabeler.h"

PlanetMoonsComponent::PlanetMoonsComponent( SkyComposite *p,
                                            SolarSystemSingleComponent *planetComponent,
                                            KSPlanetBase::Planets planet ) :
    SkyComponent( p  )
{
    pmoons = 0;
    PlanetMoonsComponent::planet = planet;
    m_Planet = planetComponent;
}

PlanetMoonsComponent::~PlanetMoonsComponent()
{
    delete pmoons;
}

bool PlanetMoonsComponent::selected() {
    return m_Planet->selected();
}
    
void PlanetMoonsComponent::init()
{
    /*
    if (planet == KSPlanetBase::JUPITER)
        pmoons = new JupiterMoons();
    else
        pmoons = new SaturnMoons();
    */
    Q_ASSERT( planet == KSPlanetBase::JUPITER );
    delete pmoons;
    pmoons = new JupiterMoons();
    int nmoons = pmoons->nMoons();
    for ( int i=0; i<nmoons; ++i ) 
        objectNames(SkyObject::MOON).append( pmoons->name(i) );
}

void PlanetMoonsComponent::update( KSNumbers * )
{
    KStarsData *data = KStarsData::Instance();
    if ( selected() )
        pmoons->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
}

void PlanetMoonsComponent::updateMoons( KSNumbers *num )
{
    //FIXME: evil cast
    if ( selected() )
        pmoons->findPosition( num, (KSPlanet*)(m_Planet->planet()), (KSSun*)(parent()->findByName( "Sun" )) );
}

SkyObject* PlanetMoonsComponent::findByName( const QString &name ) {
    int nmoons = pmoons->nMoons();
    
    for ( int i=0; i<nmoons; ++i ) {
        TrailObject *moon = pmoons->moon(i);
        if ( QString::compare( moon->name(), name, Qt::CaseInsensitive ) == 0 || 
            QString::compare( moon->longname(), name, Qt::CaseInsensitive ) == 0 ||
            QString::compare( moon->name2(), name, Qt::CaseInsensitive ) == 0 )
            return moon;
    }

    return 0;
}

SkyObject* PlanetMoonsComponent::objectNearest( SkyPoint *p, double &maxrad ) { 
    SkyObject *oBest = 0;
    int nmoons = pmoons->nMoons();
    
    for ( int i=0; i<nmoons; ++i ) {
        SkyObject *moon = (SkyObject*)(pmoons->moon(i));
        double r = moon->angularDistanceTo( p ).Degrees();
        if ( r < maxrad ) {
            maxrad = r;
            oBest = moon;
        }
    }

    return oBest;

}

bool PlanetMoonsComponent::addTrail( SkyObject *o ) {
    int nmoons = pmoons->nMoons();
    
    for ( int i=0; i<nmoons; i++ ) {
        if ( o == pmoons->moon(i) ) {
            pmoons->moon(i)->addToTrail();
            return true;
        }
    }

    return false;
}

bool PlanetMoonsComponent::removeTrail( SkyObject *o ) {
    int nmoons = pmoons->nMoons();
    
    for ( int i=0; i<nmoons; i++ ) {
        if ( o == pmoons->moon(i) ) {
            pmoons->moon(i)->clearTrail();
            return true;
        }
    }

    return false;
}

void PlanetMoonsComponent::clearTrailsExcept( SkyObject *exOb ) {
    int nmoons = pmoons->nMoons();
    
    for ( int i=0; i<nmoons; i++ ) 
        if ( exOb != pmoons->moon(i) ) 
            pmoons->moon(i)->clearTrail();
}

void PlanetMoonsComponent::draw( QPainter& psky )
{
    if ( !(planet == KSPlanetBase::JUPITER && Options::showJupiter() ) ) return;

    SkyMap *map = SkyMap::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    psky.setPen( QPen( QColor( "white" ) ) );

    if ( Options::zoomFactor() <= 10.*MINZOOM ) return;

    //In order to get the z-order right for the moons and the planet,
    //we need to first draw the moons that are further away than the planet,
    //then re-draw the planet, then draw the moons nearer than the planet.
    QList<QPointF> frontMoons;
    int nmoons = pmoons->nMoons();
    
    for ( int i=0; i<nmoons; ++i ) {
        QPointF o = map->toScreen( pmoons->moon(i) );

        if ( ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) ) {
            if ( pmoons->z(i) < 0.0 ) { //Moon is nearer than the planet
                frontMoons.append( o );
            } else {
                //Draw Moons that are further than the planet
                psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
            }
        }
    }

    //Now redraw the planet
    m_Planet->draw( psky );

    //Now draw the remaining moons, as stored in frontMoons
    psky.setPen( QPen( QColor( "white" ) ) );
    foreach ( const QPointF &o, frontMoons ) {
        psky.drawEllipse( QRectF( o.x()-1., o.y()-1., 2., 2. ) );
    }

    //Draw Moon name labels if at high zoom
    if ( ! (Options::showPlanetNames() && Options::zoomFactor() > 50.*MINZOOM) ) return;
    for ( int i=0; i<nmoons; ++i ) {
        QPointF o = map->toScreen( pmoons->moon(i) );

        if ( ! map->onScreen( o ) ) continue;
        /*
        if (planet ==KSPlanetBase::SATURN)
	  SkyLabeler::AddLabel( o, pmoons->moon(i), SkyLabeler::SATURN_MOON_LABEL );
	else
        */
        SkyLabeler::AddLabel( o, pmoons->moon(i), SkyLabeler::JUPITER_MOON_LABEL );
    }
}

void PlanetMoonsComponent::drawTrails( QPainter& psky ) {
    SkyMap *map = SkyMap::Instance();
    KStarsData *data = KStarsData::Instance();

    float Width = map->scale() * map->width();
    float Height = map->scale() * map->height();

    QColor tcolor1 = QColor( data->colorScheme()->colorNamed( "PlanetTrailColor" ) );
    QColor tcolor2 = QColor( data->colorScheme()->colorNamed( "SkyColor" ) );

    int nmoons = pmoons->nMoons();
    
    for ( int i=0; i<nmoons; ++i ) {
        TrailObject *moon = pmoons->moon(i);

        if ( ! selected() || ! moon->hasTrail() )
            continue;

        SkyPoint p = moon->trail().first();
        QPointF o = map->toScreen( &p );
        QPointF oLast( o );

        bool doDrawLine(false);
        int j = 0;
        int n = moon->trail().size();

        if ( ( o.x() >= -1000. && o.x() <= Width+1000.
                && o.y() >= -1000. && o.y() <= Height+1000. ) ) {
            //		psky.moveTo(o.x(), o.y());
            doDrawLine = true;
        }

        psky.setPen( QPen( tcolor1, 1 ) );
        bool firstPoint( true );
        foreach ( p, moon->trail() ) {
            if ( firstPoint ) { //skip first point
                firstPoint = false;
                continue;
            }

            if ( Options::fadePlanetTrails() ) {
                //Define interpolated color
                QColor tcolor = QColor(
                                    (j*tcolor1.red()   + (n-j)*tcolor2.red())/n,
                                    (j*tcolor1.green() + (n-j)*tcolor2.green())/n,
                                    (j*tcolor1.blue()  + (n-j)*tcolor2.blue())/n );
                ++j;
                psky.setPen( QPen( tcolor, 1 ) );
            }

            o = map->toScreen( &p );
            if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {

                //Want to disable line-drawing if this point and the last are both outside bounds of display.
                //FIXME: map->rect() should return QRectF
                if ( ! map->rect().contains( o.toPoint() ) && ! map->rect().contains( oLast.toPoint() ) ) doDrawLine = false;
    
                if ( doDrawLine ) {
                    psky.drawLine( oLast, o );
                } else {
                    doDrawLine = true;
                }
            }
    
            oLast = o;
        }
    }
}
