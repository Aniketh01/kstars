/***************************************************************************
                         skylabeler.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-10
    copyright            : (C) 2007 by James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skylabeler.h"

#include <stdio.h>

#include <QPainter>

#include "Options.h"
#include "kstarsdata.h"   // MINZOOM
#include "skymap.h"

//---------------------------------------------------------------------------//
// A Little data container class
//---------------------------------------------------------------------------//

typedef struct LabelRun
{
    LabelRun(int s, int e) : start(s), end(e) {}
    int start;
    int end;

} LabelRun;


//----- Now for the main event ----------------------------------------------//

//----- Static Methods ------------------------------------------------------//


SkyLabeler* SkyLabeler::pinstance = 0;
SkyLabeler* SkyLabeler::Instance( )
{
    if ( ! pinstance ) pinstance = new SkyLabeler();
    return pinstance;
}


void SkyLabeler::SetZoomFont( QPainter& psky )
{
    QFont font( psky.font() );
    int deltaSize = 0;
    if ( Options::zoomFactor() < 2.0 * MINZOOM )
        deltaSize = 2;
    else if ( Options::zoomFactor() < 10.0 * MINZOOM )
        deltaSize = 1;

    if ( deltaSize ) {
        font.setPointSize( font.pointSize() - deltaSize );
        psky.setFont( font );
    }
}

double SkyLabeler::ZoomOffset()
{
    double scale = SkyMap::Instance()->scale();
    double offset = scale * dms::PI * Options::zoomFactor()/10800.0;
    return 4.0 + offset * 0.5;
}

//----- Constructor ---------------------------------------------------------//

SkyLabeler::SkyLabeler() : m_maxY(0), m_size(0), m_fontMetrics( QFont() ),
        labelList( NUM_LABEL_TYPES )
{
    m_errors = 0;
    m_minDeltaX = 30;    // when to merge two adjacent regions
    m_yDensity  = 1.0;   // controls vertical resolution

    m_marks = m_hits = m_misses = m_elements = 0;
}


SkyLabeler::~SkyLabeler()
{
    for (int y = 0; y < screenRows.size(); y++ ) {
        LabelRow* row = screenRows[y];
        for ( int i = 0; i < row->size(); i++) {
            delete row->at(i);
        }
        delete row;
    }
}

bool SkyLabeler::drawGuideLabel( QPainter& psky, QPointF& o, const QString& text,
                            double angle )
{
    // Create bounding rectangle by rotating the (height x width) rectangle
    qreal h = m_fontMetrics.height();
    qreal w = m_fontMetrics.width( text );
    qreal s = sin( angle * dms::PI / 180.0 );
    qreal c = cos( angle * dms::PI / 180.0 );

    qreal w2 = w / 2.0;

    qreal top, bot, left, right;

    // These numbers really do depend on the sign of the angle like this
    if ( angle >= 0.0 ) {
        top   = o.y()           - s * w2;
        bot   = o.y() + c *  h  + s * w2;
        left  = o.x() - c * w2  - s * h;
        right = o.x() + c * w2;
    }
    else {
        top   = o.y()           + s * w2;
        bot   = o.y() + c *  h  - s * w2;
        left  = o.x() - c * w2;
        right = o.x() + c * w2  - s * h;
    }

    // return false if label would overlap existing label
    if ( ! markRegion( left, right, top, bot) )
        return false;

    // for debugging the bounding rectangle:
    //psky.drawLine( QPointF( left,  top ), QPointF( right, top ) );
    //psky.drawLine( QPointF( right, top ), QPointF( right, bot ) );
    //psky.drawLine( QPointF( right, bot ), QPointF( left,  bot ) );
    //psky.drawLine( QPointF( left,  bot ), QPointF( left,  top ) );

    // otherwise draw the label and return true
    psky.save();
    psky.translate( o );

    psky.rotate( angle );                        //rotate the coordinate system
    psky.drawText( QPointF( -w2, h ), text );
    psky.restore();                              //reset coordinate system

    return true;
}

void SkyLabeler::setFont( QPainter& psky, const QFont& font )
{
    psky.setFont( font );
    m_fontMetrics = QFontMetrics( font );
}

void SkyLabeler::shrinkFont( QPainter& psky, int delta )
{
    QFont font( psky.font() );
    font.setPointSize( font.pointSize() - delta );
    setFont( psky, font );
}

void SkyLabeler::useStdFont(QPainter& psky)
{
    setFont( psky, m_stdFont );
}

void SkyLabeler::resetFont(QPainter& psky)
{
    setFont( psky, m_skyFont );
}

void SkyLabeler::getMargins( QPainter& psky, const QString& text, float *left,
                             float *right, float *top, float *bot )
{
    float height     = m_fontMetrics.height();
    float width      = m_fontMetrics.width( text );
    float sideMargin = m_fontMetrics.width("MM") + width / 2.0;

    // Create the margins within which it is okay to draw the label
    *right = psky.window().width() - sideMargin;
    *left  = sideMargin;
    *top   = height;
    *bot   = psky.window().height() - 2.0 * height;
}

void SkyLabeler::reset( SkyMap* skyMap, QPainter& psky )
{
    // ----- Set up Zoom Dependent Font -----

    m_stdFont = QFont( psky.font() );
    SkyLabeler::SetZoomFont( psky );
    m_skyFont = psky.font( );
    m_fontMetrics = QFontMetrics( m_skyFont );
    m_minDeltaX = (int) m_fontMetrics.width("MMMMM");

    // ----- Set up Zoom Dependent Offset -----
    m_offset = SkyLabeler::ZoomOffset();

    // ----- Prepare Virtual Screen -----
    m_yScale = (m_fontMetrics.height() + 1.0) / m_yDensity;

    int maxY = int( skyMap->height() / m_yScale );
    if ( maxY < 1 ) maxY = 1;                         // prevents a crash below?

    int m_maxX = skyMap->width();
    m_size = (maxY + 1) * m_maxX;

    // Resize if needed:
    if ( maxY > m_maxY ) {
        screenRows.resize( m_maxY );
        for ( int y = m_maxY; y <= maxY; y++) {
            screenRows.append( new LabelRow() );
        }
        //printf("resize: %d -> %d, size:%d\n", m_maxY, maxY, screenRows.size());
    }

    // Clear all pre-existing rows as needed

    int minMaxY = (maxY < m_maxY) ? maxY : m_maxY;

    for (int y = 0; y <= minMaxY; y++) {
        LabelRow* row = screenRows[y];
        for ( int i = 0; i < row->size(); i++) {
            delete row->at(i);
        }
        row->clear();
    }

    // never decrease m_maxY:
    if ( m_maxY < maxY ) m_maxY = maxY;

    // reset the counters
    m_marks = m_hits = m_misses = m_elements = 0;

    //----- Clear out labelList -----
    for (int i = 0; i < labelList.size(); i++) {
        labelList[ i ].clear();
    }
}


// We use Run Length Encoding to hold the information instead of an array of
// chars.  This is both faster and smaller but the code is more complicated.
//
// This code is easy to break and hard to fix.

bool SkyLabeler::markText( const QPointF& p, const QString& text )
{

    qreal maxX =  p.x() + m_fontMetrics.width( text );
    qreal minY = p.y() - m_fontMetrics.height();
    return markRegion( p.x(), maxX, p.y(), minY );
}

bool SkyLabeler::markText( QPainter &psky, const QPointF& p, const 
QString& text )
{
    qreal maxX =  p.x() + m_fontMetrics.width( text );
    qreal minY = p.y() - m_fontMetrics.height();
    bool mark =  markRegion( p.x(), maxX, p.y(), minY );
    if ( ! mark ) return mark;

    qreal x = p.x();
    qreal y = p.y();
    qreal x2 = maxX;
    qreal y2 = minY;
    psky.drawLine( QPointF(x,  y),  QPointF(x2, y));
    psky.drawLine( QPointF(x2, y),  QPointF(x2, y2));
    psky.drawLine( QPointF(x2, y2), QPointF(x,  y2));
    psky.drawLine( QPointF(x,  y2), QPointF(x,  y));
    return mark;
}

bool SkyLabeler::markRect( qreal x, qreal y, qreal width, qreal height, QPainter& psky )
{
    /***
    QColor color( "red" );
    psky.setPen( QPen( QBrush( color ), 1, Qt::SolidLine ) );	

    qreal x2 = x + width;
    qreal y2 = y + height;
    psky.drawLine( QPointF( x, y ),  QPointF( x2, y ));
    psky.drawLine( QPointF( x2, y ), QPointF( x2, y2 ));
    psky.drawLine( QPointF( x2, y2 ),QPointF( x, y2 ));
    psky.drawLine( QPointF( x, y2 ), QPointF( x,y ));
    ***/

    return markRegion( x, x + width, y + height, y );
}
bool SkyLabeler::markRegion( qreal left, qreal right, qreal top, qreal bot )
{
    if ( m_maxY < 1 ) {
        if ( ! m_errors++ )
            fprintf(stderr, "Someone forgot to reset the SkyLabeler!\n");
        return true;
    }

    // setup x coordinates of rectangular region
    int minX = int( left );
    int maxX = int( right );
    if (maxX < minX) {
        maxX = minX;
        minX = int( right );
    }

    // setup y coordinates
    int maxY = int( bot / m_yScale );
    int minY = int( top / m_yScale );

    if ( maxY < 0 ) maxY = 0;
    if ( maxY > m_maxY ) maxY = m_maxY;
    if ( minY < 0 ) minY = 0;
    if ( minY > m_maxY ) minY = m_maxY;

    if ( maxY < minY ) {
        int temp = maxY;
        maxY = minY;
        minY = temp;
    }

    // check to see if we overlap any existing label
    // We must check all rows before we start marking
    for (int y = minY; y <= maxY; y++ ) {
        LabelRow* row = screenRows[ y ];
        int i;
        for ( i = 0; i < row->size(); i++) {
            if ( row->at( i )->end < minX ) continue;  // skip past these
            if ( row->at( i )->start > maxX ) break;
            m_misses++;
            return false;
        }
    }

    m_hits++;
    m_marks += (maxX - minX + 1) * (maxY - minY + 1);

    // Okay, there was no overlap so let's insert the current rectangle into
    // screenRows.

    for ( int y = minY; y <= maxY; y++ ) {
        LabelRow* row = screenRows[ y ];

        // Simplest case: an empty row
        if ( row->size() < 1 ) {
            row->append( new LabelRun( minX, maxX ) );
            m_elements++;
            continue;
        }

        // Find out our place in the universe (or row).
        // H'mm.  Maybe we could cache these numbers above.
        int i;
        for ( i = 0; i < row->size(); i++ ) {
            if ( row->at(i)->end >= minX ) break;
        }

        // i now points to first label PAST ours

        // if we are first, append or merge at start of list
        if ( i == 0 ) {
            if ( row->at(0)->start - maxX < m_minDeltaX ) {
                row->at(0)->start = minX;
            }
            else {
                row->insert( 0, new LabelRun(minX, maxX) );
                m_elements++;
            }
            continue;
        }

        // if we are past the last label, merge or append at end
        else if ( i == row->size() ) {
            if ( minX - row->at(i-1)->end < m_minDeltaX ) {
                row->at(i-1)->end = maxX;
            }
            else {
                row->append( new LabelRun(minX, maxX) );
                m_elements++;
            }
            continue;
        }

        // if we got here, we must insert or merge the new label
        //  between [i-1] and [i]

        bool mergeHead = ( minX - row->at(i-1)->end < m_minDeltaX );
        bool mergeTail = ( row->at(i)->start - maxX < m_minDeltaX );

        // double merge => combine all 3 into one
        if ( mergeHead && mergeTail ) {
            row->at(i-1)->end = row->at(i)->end;
            delete row->at( i );
            row->removeAt( i );
            m_elements--;
        }

        // Merge label with [i-1]
        else if ( mergeHead ) {
            row->at(i-1)->end = maxX;
        }

        // Merge label with [i]
        else if ( mergeTail ) {
            row->at(i)->start = minX;
        }

        // insert between the two
        else {
            row->insert( i, new LabelRun( minX, maxX) );
            m_elements++;
        }
    }

    return true;
}


void SkyLabeler::addLabel( const QPointF& p, SkyObject *obj, SkyLabeler::label_t type )
{
    if ( obj->translatedName().isEmpty() ) return;
    labelList[ (int)type ].append( SkyLabel( p, obj ) );
}

void SkyLabeler::drawQueuedLabels( QPainter& psky )
{
    KStarsData* data = KStarsData::Instance();

    resetFont( psky );
    psky.setPen( QColor( data->colorScheme()->colorNamed( "PNameColor" ) ) );
    drawQueuedLabelsType( psky, PLANET_LABEL );

    if ( labelList[ JUPITER_MOON_LABEL ].size() > 0 ) {
        shrinkFont( psky, 2 );
        drawQueuedLabelsType( psky, JUPITER_MOON_LABEL );
        resetFont( psky );
    }

    drawQueuedLabelsType( psky, ASTEROID_LABEL );
    drawQueuedLabelsType( psky, COMET_LABEL );

}

void SkyLabeler::drawQueuedLabelsType( QPainter& psky, SkyLabeler::label_t type )
{
    LabelList list = labelList[ type ];
    for ( int i = 0; i < list.size(); i ++ ) {
        list.at(i).obj->drawNameLabel( psky, list.at(i).o );
    }
}

//----- Diagnostic and information routines -----

float SkyLabeler::fillRatio()
{
    if ( m_size == 0 ) return 0.0;
    return 100.0 * float(m_marks) / float(m_size);
}

float SkyLabeler::hitRatio()
{
    if (m_hits == 0 ) return 0.0;
    return 100.0 * float(m_hits) / ( float(m_hits + m_misses) );
}

void SkyLabeler::printInfo()
{
    printf("SkyLabeler:\n");
    printf("  fillRatio=%.1f%%\n", fillRatio() );
    printf("  hits=%d  misses=%d  ratio=%.1f%%\n", m_hits, m_misses, hitRatio());
    printf("  yScale=%.1f yDensity=%.1f maxY=%d\n", m_yScale, m_yDensity, m_maxY );

    printf("  screenRows=%d elements=%d virtualSize=%.1f Kbytes\n",
           screenRows.size(), m_elements, float(m_size) / 1024.0 );

    return;

    static const char *labelName[NUM_LABEL_TYPES];

    labelName[         STAR_LABEL ] = "Star";
    labelName[     ASTEROID_LABEL ] = "Asteroid";
    labelName[        COMET_LABEL ] = "Comet";
    labelName[       PLANET_LABEL ] = "Planet";
    labelName[ JUPITER_MOON_LABEL ] = "Jupiter Moon";
    labelName[     DEEP_SKY_LABEL ] = "Deep Sky Object";
    labelName[ CONSTEL_NAME_LABEL ] = "Constellation Name";

    for ( int i = 0; i < NUM_LABEL_TYPES; i++ ) {
        printf("  %20ss: %d\n", labelName[ i ], labelList[ i ].size() );
    }

    // Check for errors in the data structure
    for (int y = 0; y <= m_maxY; y++) {
        LabelRow* row = screenRows[y];
        int size = row->size();
        if ( size < 2 ) continue;

        bool error = false;
        for (int i = 1; i < size; i++) {
            if ( row->at(i-1)->end > row->at(i)->start ) error = true;
        }
        if ( ! error ) continue;

        printf("ERROR: %3d: ", y );
        for (int i=0; i < row->size(); i++) {
            printf("(%d, %d) ", row->at(i)->start, row->at(i)->end );
        }
        printf("\n");
    }
}

void SkyLabeler::incDensity()
{
    if ( m_yDensity < 1.0 )
        m_yDensity += 0.1;
    else
        m_yDensity++;

    if ( m_yDensity > 12.0 ) m_yDensity = 12.0;
}

void SkyLabeler::decDensity()
{
    if ( m_yDensity  <= 1.0)
        m_yDensity -= 0.1;
    else
        m_yDensity--;

    if ( m_yDensity < 0.1 ) m_yDensity = 0.1;
}

