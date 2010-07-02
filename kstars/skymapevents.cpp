/***************************************************************************
                          skymapevents.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Feb 10 2001
    copyright            : (C) 2001 by Jason Harris
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

//This file contains Event handlers for the SkyMap class.

#include <QCursor>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPaintEvent>

#include <kstatusbar.h>
#include <kio/job.h>

#include "skymap.h"
#include "skyqpainter.h"
#include "skyglpainter.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "simclock.h"
#include "kspopupmenu.h"
#include "skyobjects/ksplanetbase.h"
#include "widgets/infoboxwidget.h"

#include "skycomponents/skymapcomposite.h"

// TODO: Remove if debug key binding is removed
#include "skycomponents/skylabeler.h"
#include "skycomponents/starcomponent.h"


void SkyMap::resizeEvent( QResizeEvent * )
{
    computeSkymap = true; // skymap must be new computed

    //FIXME: No equivalent for this line in Qt4 ??
    //	if ( testWState( Qt::WState_AutoMask ) ) updateMask();

    *sky  = sky->scaled( width(), height() );
    *sky2 = sky2->scaled( width(), height() );

    // Resize infoboxes container.
    // FIXME: this is not really pretty. Maybe there are some better way to this???
    m_iboxes->resize( size() );
}

void SkyMap::keyPressEvent( QKeyEvent *e ) {
    KStars* kstars = KStars::Instance();
    QString s;
    bool arrowKeyPressed( false );
    bool shiftPressed( false );
    float step = 1.0;
    if ( e->modifiers() & Qt::ShiftModifier ) { step = 10.0; shiftPressed = true; }

    //If the DBus resume key is not empty, then DBus processing is
    //paused while we wait for a keypress
    if ( ! data->resumeKey.isEmpty() && QKeySequence(e->key()) == data->resumeKey ) {
        //The resumeKey was pressed.  Signal that it was pressed by
        //resetting it to empty; this will break the loop in
        //KStars::waitForKey()
        data->resumeKey = QKeySequence();
        return;
    }

    switch ( e->key() ) {
    case Qt::Key_Left :
        if ( Options::useAltAz() ) {
            focus()->setAz( dms( focus()->az().Degrees() - step * MINZOOM/Options::zoomFactor() ).reduce() );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            focus()->setRA( focus()->ra().Hours() + 0.05*step * MINZOOM/Options::zoomFactor() );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Right :
        if ( Options::useAltAz() ) {
            focus()->setAz( dms( focus()->az().Degrees() + step * MINZOOM/Options::zoomFactor() ).reduce() );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            focus()->setRA( focus()->ra().Hours() - 0.05*step * MINZOOM/Options::zoomFactor() );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Up :
        if ( Options::useAltAz() ) {
            focus()->setAlt( focus()->alt().Degrees() + step * MINZOOM/Options::zoomFactor() );
            if ( focus()->alt().Degrees() > 90.0 ) focus()->setAlt( 90.0 );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            focus()->setDec( focus()->dec().Degrees() + step * MINZOOM/Options::zoomFactor() );
            if (focus()->dec().Degrees() > 90.0) focus()->setDec( 90.0 );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Down:
        if ( Options::useAltAz() ) {
            focus()->setAlt( focus()->alt().Degrees() - step * MINZOOM/Options::zoomFactor() );
            if ( focus()->alt().Degrees() < -90.0 ) focus()->setAlt( -90.0 );
            focus()->HorizontalToEquatorial(data->lst(), data->geo()->lat() );
        } else {
            focus()->setDec( focus()->dec().Degrees() - step * MINZOOM/Options::zoomFactor() );
            if (focus()->dec().Degrees() < -90.0) focus()->setDec( -90.0 );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        arrowKeyPressed = true;
        slewing = true;
        ++scrollCount;
        break;

    case Qt::Key_Plus:   //Zoom in
    case Qt::Key_Equal:
        zoomInOrMagStep( e->modifiers() );
        break;

    case Qt::Key_Minus:  //Zoom out
    case Qt::Key_Underscore:
        zoomOutOrMagStep( e->modifiers() );
        break;

    case Qt::Key_0: //center on Sun
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::SUN ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_1: //center on Mercury
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::MERCURY ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_2: //center on Venus
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::VENUS ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_3: //center on Moon
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::MOON ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_4: //center on Mars
        setClickedObject( data->skyComposite()->planet( KSPlanetBase:: MARS) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_5: //center on Jupiter
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::JUPITER ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_6: //center on Saturn
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::SATURN ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_7: //center on Uranus
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::URANUS ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_8: //center on Neptune
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::NEPTUNE ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_9: //center on Pluto
        setClickedObject( data->skyComposite()->planet( KSPlanetBase::PLUTO ) );
        setClickedPoint( clickedObject() );
        slotCenter();
        break;

    case Qt::Key_BracketLeft:   // Begin measuring angular distance
        if ( !isAngleMode() ) {
            slotBeginAngularDistance();
        }
        break;
    case Qt::Key_Escape:        // Cancel angular distance measurement
        if  (isAngleMode() ) {
            slotCancelAngularDistance();
        }
        break;
    case Qt::Key_Comma:  //advance one step backward in time
    case Qt::Key_Less:
        if ( data->clock()->isActive() ) data->clock()->stop();
        data->clock()->setScale( -1.0 * data->clock()->scale() ); //temporarily need negative time step
        data->clock()->manualTick( true );
        data->clock()->setScale( -1.0 * data->clock()->scale() ); //reset original sign of time step
        update();
        qApp->processEvents();
        break;

    case Qt::Key_Period: //advance one step forward in time
    case Qt::Key_Greater:
        if ( data->clock()->isActive() ) data->clock()->stop();
        data->clock()->manualTick( true );
        update();
        qApp->processEvents();
        break;

    case Qt::Key_C: //Center clicked object
        if ( clickedObject() ) slotCenter();
        break;

    case Qt::Key_D: //Details window for Clicked/Centered object
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) { 
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        if ( clickedObject() ) {
            slotDetail();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    case Qt::Key_P: //Show Popup menu for Clicked/Centered object
        if ( shiftPressed ) {
            if ( focusObject() ) 
                focusObject()->showPopupMenu( pmenu, QCursor::pos() );
        } else {
            if ( clickedObject() )
                clickedObject()->showPopupMenu( pmenu, QCursor::pos() );
        }
        break;

    case Qt::Key_O: //Add object to Observing List
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) {
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        if ( clickedObject() ) {
            kstars->observingList()->slotAddObject();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    case Qt::Key_L: //Toggle User label on Clicked/Centered object
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) {
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        if ( clickedObject() ) {
            if ( isObjectLabeled( clickedObject() ) )
                slotRemoveObjectLabel();
            else
                slotAddObjectLabel();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    case Qt::Key_T: //Toggle planet trail on Clicked/Centered object (if solsys)
    {
        SkyObject *orig = 0;
        if ( shiftPressed ) {
            orig = clickedObject();
            setClickedObject( focusObject() );
        }

        KSPlanetBase* planet = dynamic_cast<KSPlanetBase*>( clickedObject() );
        if( planet ) {
            if( planet->hasTrail() )
                slotRemovePlanetTrail();
            else
                slotAddPlanetTrail();
        }

        if ( orig ) {
            setClickedObject( orig );
        }
        break;
    }

    //DEBUG_REFRACT
    case Qt::Key_Q:
        {
            for ( double alt=-0.5; alt<30.5; alt+=1.0 ) {
                dms a( alt );
                dms b( refract( a, true ) ); //find apparent alt from true alt
                dms c( refract( b, false ) );

                kDebug() << a.toDMSString() << b.toDMSString() << c.toDMSString();
            }
            break;
        }
    case Qt::Key_R:
        {
            // Toggle relativistic corrections
            Options::setUseRelativistic( ! Options::useRelativistic() );
            kDebug() << "Relativistc corrections: " << Options::useRelativistic();
            forceUpdate();
            break;
        }
    case Qt::Key_A:
        Options::setUseAntialias( ! Options::useAntialias() );
        kDebug() << "Use Antialiasing: " << Options::useAntialias();
        forceUpdate();
        break;
    default:
        // We don't want to do anything in this case. Key is unknown
        return;
    }

    if ( arrowKeyPressed ) {
        stopTracking();
        if ( scrollCount > 10 ) {
            setDestination( focus() );
            scrollCount = 0;
        }
    }

    forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

//DEBUG_KIO_JOB
void SkyMap::slotJobResult( KJob *job ) {
    KIO::StoredTransferJob *stjob = (KIO::StoredTransferJob*)job;

    QPixmap pm;
    pm.loadFromData( stjob->data() );

    //DEBUG
    kDebug() << "Pixmap: " << pm.width() << "x" << pm.height();

}

void SkyMap::stopTracking() {
    KStars* kstars = KStars::Instance();
    emit positionChanged( focus() );
    if( kstars && Options::isTracking() )
        kstars->slotTrack();
}

void SkyMap::keyReleaseEvent( QKeyEvent *e ) {
    switch ( e->key() ) {
    case Qt::Key_Plus:   //Zoom in
    case Qt::Key_Equal:
    case Qt::Key_Minus:  //Zoom out
    case Qt::Key_Underscore:

    case Qt::Key_Left :  //no break; continue to Qt::Key_Down
    case Qt::Key_Right :  //no break; continue to Qt::Key_Down
    case Qt::Key_Up :  //no break; continue to Qt::Key_Down
    case Qt::Key_Down :
        slewing = false;
        scrollCount = 0;

        if ( Options::useAltAz() )
            setDestinationAltAz( focus()->alt().Degrees(), focus()->az().Degrees() );
        else
            setDestination( focus() );

        showFocusCoords();
        forceUpdate();  // Need a full update to draw faint objects that are not drawn while slewing.
        break;
    }
}

void SkyMap::mouseMoveEvent( QMouseEvent *e ) {
    if ( Options::useHoverLabel() ) {
        //First of all, if the transientObject() pointer is not NULL, then
        //we just moved off of a hovered object.  Begin fading the label.
        if ( transientObject() && ! TransientTimer.isActive() ) {
            fadeTransientLabel();
        }

        //Start a single-shot timer to monitor whether we are currently hovering.
        //The idea is that whenever a moveEvent occurs, the timer is reset.  It
        //will only timeout if there are no move events for HOVER_INTERVAL ms
        HoverTimer.start( HOVER_INTERVAL );
    }

    //Are we defining a ZoomRect?
    if ( ZoomRect.center().x() > 0 && ZoomRect.center().y() > 0 ) {
        //cancel operation if the user let go of CTRL
        if ( !( e->modifiers() & Qt::ControlModifier ) ) {
            ZoomRect = QRect(); //invalidate ZoomRect
            update();
        } else {
            //Resize the rectangle so that it passes through the cursor position
            QPoint pcenter = ZoomRect.center();
            int dx = abs(e->x() - pcenter.x());
            int dy = abs(e->y() - pcenter.y());
            if ( dx == 0 || float(dy)/float(dx) > float(height())/float(width()) ) {
                //Size rect by height
                ZoomRect.setHeight( 2*dy );
                ZoomRect.setWidth( 2*dy*width()/height() );
            } else {
                //Size rect by height
                ZoomRect.setWidth( 2*dx );
                ZoomRect.setHeight( 2*dx*height()/width() );
            }
            ZoomRect.moveCenter( pcenter ); //reset center

            update();
            return;
        }
    }

    if ( unusablePoint( e->pos() ) ) return;  // break if point is unusable

    //determine RA, Dec of mouse pointer
    setMousePoint( fromScreen( e->pos(), data->lst(), data->geo()->lat() ) );
    mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

    double dyPix = 0.5*height() - e->y();
    if ( midMouseButtonDown ) { //zoom according to y-offset
        float yoff = dyPix - y0;
        if (yoff > 10 ) {
            y0 = dyPix;
            slotZoomIn();
        }
        if (yoff < -10 ) {
            y0 = dyPix;
            slotZoomOut();
        }
    }

    if ( mouseButtonDown ) {
        // set the mouseMoveCursor and set slewing=true, if they are not set yet
        if( !mouseMoveCursor )
            setMouseMoveCursor();
        if( !slewing ) {
            slewing = true;
            stopTracking(); //toggle tracking off
        }

        //Update focus such that the sky coords at mouse cursor remain approximately constant
        if ( Options::useAltAz() ) {
            mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            clickedPoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            dms dAz = mousePoint()->az().Degrees() - clickedPoint()->az().Degrees();
            dms dAlt = mousePoint()->alt().Degrees() - clickedPoint()->alt().Degrees();
            focus()->setAz( focus()->az().Degrees() - dAz.Degrees() ); //move focus in opposite direction
            focus()->setAz( focus()->az().reduce() );
            focus()->setAlt(
                KSUtils::clamp( focus()->alt().Degrees() - dAlt.Degrees() , -90.0 , 90.0 ) );
            focus()->HorizontalToEquatorial( data->lst(), data->geo()->lat() );
        } else {
            dms dRA = mousePoint()->ra().Degrees() - clickedPoint()->ra().Degrees();
            dms dDec = mousePoint()->dec().Degrees() - clickedPoint()->dec().Degrees();
            focus()->setRA( focus()->ra().Hours() - dRA.Hours() ); //move focus in opposite direction
            focus()->setRA( focus()->ra().reduce() );
            focus()->setDec(
                KSUtils::clamp( focus()->dec().Degrees() - dDec.Degrees() , -90.0 , 90.0 ) );
            focus()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        }

        ++scrollCount;
        if ( scrollCount > 4 ) {
            showFocusCoords();
            scrollCount = 0;
        }

        //redetermine RA, Dec of mouse pointer, using new focus
        setMousePoint( fromScreen( e->pos(), data->lst(), data->geo()->lat() ) );
        mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        setClickedPoint( mousePoint() );

        forceUpdate();  // must be new computed

    } else { //mouse button not down
        emit mousePointChanged( mousePoint() );
    }
}

void SkyMap::wheelEvent( QWheelEvent *e ) {
    if ( e->delta() > 0 ) 
        zoomInOrMagStep ( e->modifiers() );
    else
        zoomOutOrMagStep( e->modifiers() );
}

void SkyMap::mouseReleaseEvent( QMouseEvent * ) {
    if ( ZoomRect.isValid() ) {
        //Zoom in on center of Zoom Circle, by a factor equal to the ratio
        //of the sky pixmap's width to the Zoom Circle's diameter
        float factor = float(width()) / float(ZoomRect.width());

        stopTracking();

        SkyPoint newcenter = fromScreen( ZoomRect.center(), data->lst(), data->geo()->lat() );

        setFocus( &newcenter );
        setDestination( &newcenter );
        setDefaultMouseCursor();

        setZoomFactor( Options::zoomFactor() * factor );

        ZoomRect = QRect(); //invalidate ZoomRect
    } else {
        setDefaultMouseCursor();
        ZoomRect = QRect(); //just in case user Ctrl+clicked + released w/o dragging...
    }

    if (mouseMoveCursor) setDefaultMouseCursor();	// set default cursor
    if (mouseButtonDown) { //false if double-clicked, because it's unset there.
        mouseButtonDown = false;
        if ( slewing ) {
            slewing = false;

            if ( Options::useAltAz() )
                setDestinationAltAz( focus()->alt().Degrees(), focus()->az().Degrees() );
            else
                setDestination( focus() );
        }
        forceUpdate();	// is needed because after moving the sky not all stars are shown
    }
    if ( midMouseButtonDown ) midMouseButtonDown = false;  // if middle button was pressed unset here

    scrollCount = 0;
}

void SkyMap::mousePressEvent( QMouseEvent *e ) {
    KStars* kstars = KStars::Instance();

    if ( ( e->modifiers() & Qt::ControlModifier ) && (e->button() == Qt::LeftButton) ) {
        ZoomRect.moveCenter( e->pos() );
        setZoomMouseCursor();
        update(); //refresh without redrawing skymap
        return;
    }

    // if button is down and cursor is not moved set the move cursor after 500 ms
    QTimer::singleShot(500, this, SLOT (setMouseMoveCursor()));

    // break if point is unusable
    if ( unusablePoint( e->pos() ) )
        return;

    if ( !midMouseButtonDown && e->button() == Qt::MidButton ) {
        y0 = 0.5*height() - e->y();  //record y pixel coordinate for middle-button zooming
        midMouseButtonDown = true;
    }

    if ( !mouseButtonDown ) {
        if ( e->button() == Qt::LeftButton ) {
            mouseButtonDown = true;
            scrollCount = 0;
        }

        //determine RA, Dec of mouse pointer
        setMousePoint( fromScreen( e->pos(), data->lst(), data->geo()->lat() ) );
        mousePoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
        setClickedPoint( mousePoint() );
        clickedPoint()->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        //Find object nearest to clickedPoint()
        double maxrad = 1000.0/Options::zoomFactor();
        SkyObject* obj = data->skyComposite()->objectNearest( clickedPoint(), maxrad );
        setClickedObject( obj );
        if( obj )
            setClickedPoint(  obj );

        switch( e->button() ) {
        case Qt::LeftButton:
            if( kstars ) {
                QString name;
                if( clickedObject() )
                    name = clickedObject()->translatedLongName();
                else
                    name = i18n( "Empty sky" );
                kstars->statusBar()->changeItem(name, 0 );
            }
            break;
        case Qt::RightButton:
            if( angularDistanceMode ) {
                // Compute angular distance.
                slotEndAngularDistance();
            } else {
                // Show popup menu
                if( clickedObject() ) {
                    clickedObject()->showPopupMenu( pmenu, QCursor::pos() );
                } else {
                    SkyObject o( SkyObject::TYPE_UNKNOWN, clickedPoint()->ra().Hours(), clickedPoint()->dec().Degrees() );
                    pmenu->createEmptyMenu( &o );
                    pmenu->popup( QCursor::pos() );
                }
            }
            break;
        default: ;
        }
    }
}

void SkyMap::mouseDoubleClickEvent( QMouseEvent *e ) {
    if ( e->button() == Qt::LeftButton && !unusablePoint( e->pos() ) ) {
        mouseButtonDown = false;
        if( e->x() != width()/2 || e->y() != height()/2 )
            slotCenter();
    }
}

#ifdef USEGL
void SkyMap::initializeGL()
{
}

void SkyMap::resizeGL(int width, int height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    //do nothing since we resize in SkyGLPainter::paintGL()
}

void SkyMap::paintGL()
{
    SkyGLPainter psky(this);
    //FIXME: we may want to move this into the components.
    psky.begin();

    //Draw all sky elements
    psky.drawSkyBackground();
    data->skyComposite()->draw( &psky );
    //Finish up
    psky.end();
}
#else
void SkyMap::paintEvent( QPaintEvent *event )
{
    //If computeSkymap is false, then we just refresh the window using the stored sky pixmap
    //and draw the "overlays" on top.  This lets us update the overlay information rapidly
    //without needing to recompute the entire skymap.
    //use update() to trigger this "short" paint event; to force a full "recompute"
    //of the skymap, use forceUpdate().
    

    if (!computeSkymap)
    {
        *sky2 = *sky;
        drawOverlays( sky2 );
        QPainter p;
        p.begin( this );
        p.drawLine(0,0,1,1); // Dummy operation to circumvent bug
        p.drawPixmap( 0, 0, *sky2 );
        p.end();
        return ; // exit because the pixmap is repainted and that's all what we want
    } 

    // FIXME: used to to notify infobox about possible change of object coordinates
    // Not elegant at all. Should find better option
    showFocusCoords();

    setMapGeometry();

    //FIXME: What to do about the visibility logic?
    // 	//checkSlewing combines the slewing flag (which is true when the display is actually in motion),
    // 	//the hideOnSlew option (which is true if slewing should hide objects),
    // 	//and clockSlewing (which is true if the timescale exceeds Options::slewTimeScale)
    // 	bool checkSlewing = ( ( slewing || ( clockSlewing && data->clock()->isActive() ) )
    // 				&& Options::hideOnSlew() );
    //
    // 	//shortcuts to inform whether to draw different objects
    // 	bool drawPlanets( Options::showSolarSystem() && !(checkSlewing && Options::hidePlanets() ) );
    // 	bool drawMW( Options::showMilkyWay() && !(checkSlewing && Options::hideMilkyWay() ) );
    // 	bool drawCNames( Options::showCNames() && !(checkSlewing && Options::hideCNames() ) );
    // 	bool drawCLines( Options::showCLines() &&!(checkSlewing && Options::hideCLines() ) );
    // 	bool drawCBounds( Options::showCBounds() &&!(checkSlewing && Options::hideCBounds() ) );
    // 	bool drawGrid( Options::showGrid() && !(checkSlewing && Options::hideGrid() ) );

    SkyQPainter psky(this, sky);
    //FIXME: we may want to move this into the components.
    psky.begin();
    
    //Draw all sky elements
    psky.drawSkyBackground();
    data->skyComposite()->draw( &psky );
    //Finish up
    psky.end();


    *sky2 = *sky;
    drawOverlays( sky2 );
    //TIMING
    //	t2.start();

    
    QPainter psky2;
    psky2.begin( this );
    psky2.drawLine(0,0,1,1); // Dummy op.
    psky2.drawPixmap( 0, 0, *sky2 );
    psky2.end();

    // DEBUG stuff known to "work for sure" -- copied from the Qt example
    /*
    kDebug() << "HERE!";

    QLinearGradient gradient(QPointF(50, -20), QPointF(80, 20));
    gradient.setColorAt(0.0, Qt::white);
    gradient.setColorAt(1.0, QColor(0xa6, 0xce, 0x39));

    QBrush background = QBrush(QColor(64, 32, 64));

    QBrush circleBrush = QBrush(gradient);
    QPen circlePen = QPen(Qt::black);
    circlePen.setWidth(1);
    QPen textPen = QPen(Qt::white);
    QFont textFont;
    textFont.setPixelSize(50);

    QPainter *painter = new QPainter();

    painter->begin(this);
    painter->setRenderHint(QPainter::Antialiasing);

    //    painter->fillRect(QRect(0,0,1,1), background);
    painter->drawPixmap(0, 0, *sky2);

    painter->translate(100, 100);
//! [1]

//! [2]
    painter->save();
    painter->setBrush(circleBrush);
    painter->setPen(circlePen);
    painter->rotate(100 * 0.030);

    qreal r = 100/1000.0;
    int n = 30;
    for (int i = 0; i < n; ++i) {
        painter->rotate(30);
        qreal radius = 0 + 120.0*((i+r)/n);
        qreal circleRadius = 1 + ((i+r)/n)*20;
        painter->drawEllipse(QRectF(radius, -circleRadius,
                                    circleRadius*2, circleRadius*2));
    }
    painter->restore();
//! [2]

//! [3]
    painter->setPen(textPen);
    painter->setFont(textFont);
    painter->drawText(QRect(-50, -50, 100, 100), Qt::AlignCenter, "Qt");



    painter->end();

    delete painter;
    kDebug() << "DONE!";
    */
    computeSkymap = false;	// use forceUpdate() to compute new skymap else old pixmap will be shown
}
#endif

double SkyMap::zoomFactor( const int modifier ) {
    double factor = ( modifier & Qt::ControlModifier) ? DZOOM : 2.0; 
    if ( modifier & Qt::ShiftModifier ) 
        factor = sqrt( factor );
    return factor;
}

void SkyMap::zoomInOrMagStep( const int modifier ) {
    if ( modifier & Qt::AltModifier )
        incMagLimit( modifier );
    else
        setZoomFactor( Options::zoomFactor() * zoomFactor( modifier ) );
}

    
void SkyMap::zoomOutOrMagStep( const int modifier ) {
    if ( modifier & Qt::AltModifier )
        decMagLimit( modifier );
    else
        setZoomFactor( Options::zoomFactor() / zoomFactor (modifier ) );
}

double SkyMap::magFactor( const int modifier ) {
    double factor = ( modifier & Qt::ControlModifier) ? 0.2 : 1.0; 
    if ( modifier & Qt::ShiftModifier ) 
        factor /= 2.0;
    return factor;
}

void SkyMap::incMagLimit( const int modifier ) {
    double limit = 2.222 * log10(static_cast<double>( Options::starDensity() )) + 0.35;
    limit += magFactor( modifier );
    if ( limit > 7.94 ) limit = 7.94;
    Options::setStarDensity( pow( 10, ( limit - 0.35 ) / 2.222) );
    //printf("maglim set to %3.1f\n", limit);
    forceUpdate();
}

void SkyMap::decMagLimit( const int modifier ) {
    double limit = 2.222 * log10(static_cast<double>( Options::starDensity() )) + 0.35;
    limit -= magFactor( modifier );
    if ( limit < 3.55 ) limit = 3.55;
    Options::setStarDensity( pow( 10, ( limit - 0.35 ) / 2.222) );
    //printf("maglim set to %3.1f\n", limit);
    forceUpdate();
}

