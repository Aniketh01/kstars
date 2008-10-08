/***************************************************************************
                          starcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/14/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "deepstarcomponent.h"

#include <QPixmap>
#include <QPainter>

#include <QRectF>
#include <QFontMetricsF>
#include <kglobal.h>

#include "Options.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "starobject.h"

#include "skymesh.h"

#include "binfilehelper.h"
#include "byteswap.h"
#include "starblockfactory.h"

#include "starcomponent.h"

#include <kde_file.h>

DeepStarComponent::DeepStarComponent( SkyComponent *parent, QString fileName, float trigMag, bool staticstars )
    : dataFileName( fileName ), ListComponent(parent), triggerMag( trigMag ), m_FaintMagnitude(-5.0), 
      m_reindexNum( J2000 ), staticStars( staticstars )
{
    fileOpened = false;
}

bool DeepStarComponent::loadStaticStars() {
    FILE *dataFile;
    StarObject plainStarTemplate;

    if( !staticStars )
        return true;
    if( !fileOpened )
        return false;

    dataFile = starReader.getFileHandle();
    rewind( dataFile );

    if( !starReader.readHeader() ) {
        kDebug() << "Error reading header of catalog file " << dataFileName << ": " << starReader.getErrorNumber() << ": " << starReader.getError() << endl;
        return false;
    }

    if( starReader.guessRecordSize() != 16 && starReader.guessRecordSize() != 32 ) {
        kDebug() << "Cannot understand catalog file " << dataFileName << endl;
        return false;
    }

    KDE_fseek(dataFile, starReader.getDataOffset(), SEEK_SET);

    qint16 faintmag;
    quint8 htm_level;
    quint16 t_MSpT;
    fread( &faintmag, 2, 1, dataFile );
    fread( &htm_level, 1, 1, dataFile );
    fread( &t_MSpT, 2, 1, dataFile ); // Unused

    // TODO: Read the multiplying factor from the dataFile
    m_FaintMagnitude = faintmag / 100.0;

    if( htm_level != m_skyMesh->level() )
        kDebug() << "WARNING: HTM Level in shallow star data file and HTM Level in m_skyMesh do not match. EXPECT TROUBLE" << endl;

    for(Trixel i = 0; i < m_skyMesh->size(); ++i) {

        Trixel trixel = i;
        StarBlock *SB = new StarBlock( starReader.getRecordCount( i ) );
        if( !SB )
            kDebug() << "ERROR: Could not allocate new StarBlock to hold shallow unnamed stars for trixel " << trixel << endl;
        m_starBlockList.at( trixel )->setStaticBlock( SB );
        
        for(unsigned long j = 0; j < (unsigned long) starReader.getRecordCount(i); ++j) {
            bool fread_success;
            if( starReader.guessRecordSize() == 32 )
                fread_success = fread( &stardata, sizeof( starData ), 1, dataFile );
            else if( starReader.guessRecordSize() == 16 )
                fread_success = fread( &deepstardata, sizeof( deepStarData ), 1, dataFile );

            if( !fread_success ) {
                kDebug() << "ERROR: Could not read starData structure for star #" << j << " under trixel #" << trixel << endl;
            }

            /* Swap Bytes when required */            
            if( starReader.getByteSwap() ) {
                if( starReader.guessRecordSize() == 32 )
                    byteSwap( &stardata );
                else
                    byteSwap( &deepstardata );
            }


            /*
             * CAUTION: We avoid trying to construct StarObjects using the constructors [The C++ way]
             *          in order to gain speed. Instead, one template StarObject is constructed and
             *          all other unnamed stars are created by doing a raw copy from this using memcpy()
             *          and then calling StarObject::init() to replace the default data with the correct
             *          data.
             *          This means that this section of the code plays around with pointers to a great
             *          extend and has a chance of breaking down / causing segfaults.
             */
                    
            /* Make a copy of the star template and set up the data in it */
            if( starReader.guessRecordSize() == 32 )
                plainStarTemplate.init( &stardata );
            else
                plainStarTemplate.init( &deepstardata );
            plainStarTemplate.EquatorialToHorizontal( data()->lst(), data()->geo()->lat() );
            if( !SB->addStar( &plainStarTemplate ) )
                kDebug() << "CODE ERROR: More unnamed static stars in trixel " << trixel << " than we allocated space for!" << endl;
            StarObject *star = SB->star( SB->getStarCount() - 1 );

            if( star->getHDIndex() != 0 )
                m_CatalogNumber.insert( star->getHDIndex(), star );

        }
    }

    return true;
}

DeepStarComponent::~DeepStarComponent() {
  if( fileOpened )
    starReader.closeFile();
  fileOpened = false;
}

bool DeepStarComponent::selected() {
    return Options::showStars() && fileOpened;
}

bool openIndexFile( ) {
    // TODO: Work out the details
    /*
    if( hdidxReader.openFile( "Henry-Draper.idx" ) )
        kDebug() << "Could not open HD Index file. Search by HD numbers for deep stars will not work." << endl;
    */
    return 0;
}

//This function is empty for a reason; we override the normal 
//update function in favor of JiT updates for stars.
void DeepStarComponent::update( KStarsData *data, KSNumbers *num ) {   
    Q_UNUSED(data)   
    Q_UNUSED(num)   
}   

// TODO: Optimize draw, if it is worth it.
void DeepStarComponent::draw( QPainter& psky ) {
    if ( !fileOpened ) return;

    SkyMap *map = SkyMap::Instance();
    KStarsData* data = KStarsData::Instance();
    UpdateID updateID = data->updateID();

    float radius = map->fov();
    if ( radius > 90.0 ) radius = 90.0;

    if ( m_skyMesh != SkyMesh::Instance() && m_skyMesh->inDraw() ) {
        printf("Warning: aborting concurrent DeepStarComponent::draw()");
    }
    bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );

    //shortcuts to inform whether to draw different objects
    bool hideFaintStars( checkSlewing && Options::hideStars() );
    double hideStarsMag = Options::magLimitHideStar();

    //adjust maglimit for ZoomLevel
    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());
    // TODO: Enable hiding of faint stars

    // Old formula:
    //    float maglim = ( 2.000 + 2.444 * Options::memUsage() / 10.0 ) * ( lgz - lgmin ) + Options::magLimitDrawStarZoomOut();

    /*
     Explanation for the following formula:
     --------------------------------------
     Estimates from a sample of 125000 stars shows that, magnitude 
     limit vs. number of stars follows the formula:
       nStars = 10^(.45 * maglim + .95)
     (A better formula is available here: http://www.astro.uu.nl/~strous/AA/en/antwoorden/magnituden.html
      which we do not implement for simplicity)
     We want to keep the star density on screen a constant. This is directly proportional to the number of stars
     and directly proportional to the area on screen. The area is in turn inversely proportional to the square
     of the zoom factor ( zoomFactor / MINZOOM ). This means that (taking logarithms):
       0.45 * maglim + 0.95 - 2 * log( ZoomFactor ) - log( Star Density ) - log( Some proportionality constant )
     hence the formula. We've gathered together all the constants and set it to 3.5, so as to set the minimum
     possible value of maglim to 3.5
    */
     
    //    float maglim = 4.444 * ( lgz - lgmin ) + 2.222 * log10( Options::starDensity() ) + 3.5;

    // Reducing the slope w.r.t zoom factor to avoid the extremely fast increase in star density with zoom
    // that 4.444 gives us (although that is what the derivation gives us)

    float maglim = 3.7 * ( lgz - lgmin ) + 2.222 * log10( Options::starDensity() ) + 3.5;

    if( maglim < triggerMag )
        return;

    m_zoomMagLimit = maglim;

    double maxSize = 10.0;

    m_skyMesh->inDraw( true );

    SkyPoint* focus = map->focus();
    m_skyMesh->aperture( focus, radius + 1.0, DRAW_BUF ); // divide by 2 for testing

    MeshIterator region(m_skyMesh, DRAW_BUF);

    magLim = maglim;

    StarBlockFactory *m_StarBlockFactory = StarBlockFactory::Instance();
    //    m_StarBlockFactory->drawID = m_skyMesh->drawID();
    //    kDebug() << "Mesh size = " << m_skyMesh->size() << "; drawID = " << m_skyMesh->drawID();
    QTime t;
    int nTrixels = 0;

    t_dynamicLoad = 0;
    t_updateCache = 0;
    t_drawUnnamed = 0;

    visibleStarCount = 0;

    t.start();
    // Old formula:
    //    float sizeMagLim = ( 2.000 + 2.444 * Options::memUsage() / 10.0 ) * ( lgz - lgmin ) + 5.8;

    // Using the maglim to compute the sizes of stars reduces
    // discernability between brighter and fainter stars at high zoom
    // levels. To fix that, we use an "arbitrary" constant in place of
    // the variable star density.
    // Not using this formula now.
    //    float sizeMagLim = 4.444 * ( lgz - lgmin ) + 5.0;

    float sizeMagLim = maglim;

    //    if( sizeMagLim > m_FaintMagnitude * ( 1 - 1.5/16 ) )
    //        sizeMagLim = m_FaintMagnitude * ( 1 - 1.5/16 );
    float sizeFactor = 10.0 + (lgz - lgmin);

    // Mark used blocks in the LRU Cache. Not required for static stars
    if( !staticStars ) {
        while( region.hasNext() ) {
            Trixel currentRegion = region.next();
            for( int i = 0; i < m_starBlockList.at( currentRegion )->getBlockCount(); ++i ) {
                StarBlock *prevBlock = ( ( i >= 1 ) ? m_starBlockList.at( currentRegion )->block( i - 1 ) : NULL );
                StarBlock *block = m_starBlockList.at( currentRegion )->block( i );
                
                if( i == 0 )
                    if( !m_StarBlockFactory->markFirst( block ) )
                        kDebug() << "markFirst failed in trixel" << currentRegion;
                if( i > 0 )
                    if( !m_StarBlockFactory->markNext( prevBlock, block ) )
                        kDebug() << "markNext failed in trixel" << currentRegion << "while marking block" << i;
                if( i < m_starBlockList.at( currentRegion )->getBlockCount() 
                    && m_starBlockList.at( currentRegion )->block( i )->getFaintMag() < maglim )
                    break;
                    
            }
        }
        t_updateCache = t.restart();
        region.reset();
    }

    while ( region.hasNext() ) {
        ++nTrixels;
        Trixel currentRegion = region.next();

        // NOTE: We are guessing that the last 1.5/16 magnitudes in the catalog are just additions and the star catalog
        //       is actually supposed to reach out continuously enough only to mag m_FaintMagnitude * ( 1 - 1.5/16 )
        // TODO: Is there a better way? We may have to change the magnitude tolerance if the catalog changes
        // Static stars need not execute fillToMag

	if( !staticStars && !m_starBlockList.at( currentRegion )->fillToMag( maglim ) && maglim <= m_FaintMagnitude * ( 1 - 1.5/16 ) ) {
            kDebug() << "SBL::fillToMag( " << maglim << " ) failed for trixel " 
                     << currentRegion << " !"<< endl;
	}

        t_dynamicLoad += t.restart();

        //        kDebug() << "Drawing SBL for trixel " << currentRegion << ", SBL has " 
        //                 <<  m_starBlockList[ currentRegion ]->getBlockCount() << " blocks" << endl;
        for( int i = 0; i < m_starBlockList.at( currentRegion )->getBlockCount(); ++i ) {
            StarBlock *block = m_starBlockList.at( currentRegion )->block( i );
            //            kDebug() << "---> Drawing stars from block " << i << " of trixel " << 
            //                currentRegion << ". SB has " << block->getStarCount() << " stars" << endl;
            for( int j = 0; j < block->getStarCount(); j++ ) {

                StarObject *curStar = block->star( j );

                //                kDebug() << "We claim that he's from trixel " << currentRegion 
                //<< ", and indexStar says he's from " << m_skyMesh->indexStar( curStar );

                if ( curStar->updateID != updateID )
                    curStar->JITupdate( data );

                float mag = curStar->mag();

                if ( mag > maglim || ( hideFaintStars && mag > hideStarsMag ) )
                    break;

                if ( ! map->checkVisibility( curStar ) ) continue;

                QPointF o = map->toScreen( curStar );

                if ( ! map->onScreen( o ) ) continue;
                
                float size = ( sizeFactor*( sizeMagLim - mag ) / sizeMagLim ) + 1.;
                if ( size <= 1.0 ) size = 1.0;
                if( size > maxSize ) size = maxSize;

                curStar->draw( psky, o.x(), o.y(), size, (starColorMode()==0),
                               starColorIntensity(), true );
                visibleStarCount++;
            }
        }

        // DEBUG: Uncomment to identify problems with Star Block Factory / preservation of Magnitude Order in the LRU Cache
        //        verifySBLIntegrity();        
        t_drawUnnamed += t.restart();

    }
    m_skyMesh->inDraw( false );

}

void DeepStarComponent::init( KStarsData *data ) {
  m_Data = data;
  openDataFile();
  if( staticStars )
      loadStaticStars();
  kDebug() << "Loaded catalog file " << dataFileName << "(hopefully)";
}

bool DeepStarComponent::openDataFile() {

    if( starReader.getFileHandle() )
        return true;

    starReader.openFile( dataFileName );
    fileOpened = false;
    if( !starReader.getFileHandle() )
        kDebug() << "WARNING: Failed to open deep star catalog " << dataFileName << ". Disabling it." << endl;
    else if( !starReader.readHeader() )
        kDebug() << "WARNING: Header read error for deep star catalog " << dataFileName << "!! Disabling it!" << endl;
    else {
        qint16 faintmag;
        quint8 htm_level;
        fread( &faintmag, 2, 1, starReader.getFileHandle() );
        if( starReader.guessRecordSize() == 16 )
            m_FaintMagnitude = faintmag / 1000.0;
        else
            m_FaintMagnitude = faintmag / 100.0;
        fread( &htm_level, 1, 1, starReader.getFileHandle() );
        kDebug() << "Processing " << dataFileName << ", HTMesh Level" << htm_level;
        m_skyMesh = SkyMesh::Instance( htm_level );
        if( !m_skyMesh ) {
            if( !( m_skyMesh = SkyMesh::Create( KStarsData::Instance(), htm_level ) ) ) {
                kDebug() << "Could not create HTMesh of level " << htm_level << " for catalog " << dataFileName << ". Skipping it.";
                return false;
            }
        }
        meshLevel = htm_level;
        fread( &MSpT, 2, 1, starReader.getFileHandle() );
        fileOpened = true;
        kDebug() << "  Sky Mesh Size: " << m_skyMesh->size();
        for (long int i = 0; i < m_skyMesh->size(); i++) {
            StarBlockList *sbl = new StarBlockList( i, this );
            if( !sbl ) {
                kDebug() << "NULL starBlockList. Expect trouble!";
            }
            m_starBlockList.append( sbl );
        }
        m_zoomMagLimit = 0.06;
    }

    return fileOpened;
}


SkyObject *DeepStarComponent::findByHDIndex( int HDnum ) {
    // Currently, we only handle HD catalog indexes
    return m_CatalogNumber.value( HDnum, NULL ); // TODO: Maybe, make this more general.
}

// This uses the main star index for looking up nearby stars but then
// filters out objects with the generic name "star".  We could easily
// build an index for just the named stars which would make this go
// much faster still.  -jbb
//
SkyObject* DeepStarComponent::objectNearest( SkyPoint *p, double &maxrad )
{
    StarObject *oBest = 0;

    if( !fileOpened )
        return NULL;

    m_skyMesh->index( p, maxrad + 1.0, OBJ_NEAREST_BUF);

    MeshIterator region( m_skyMesh, OBJ_NEAREST_BUF );

    while ( region.hasNext() ) {
        Trixel currentRegion = region.next();
        for( int i = 0; i < m_starBlockList.at( currentRegion )->getBlockCount(); ++i ) {
            StarBlock *block = m_starBlockList.at( currentRegion )->block( i );
            for( int j = 0; j < block->getStarCount(); ++j ) {
                StarObject* star =  block->star( j );
                if( !star ) continue;
                if ( star->mag() > m_zoomMagLimit ) continue;
                
                double r = star->angularDistanceTo( p ).Degrees();
                if ( r < maxrad ) {
                    oBest = star;
                    maxrad = r;
                }
            }
        }
    }
    return (SkyObject*) oBest;
}

int DeepStarComponent::starColorMode( void ) const {
    return m_Data->colorScheme()->starColorMode();
}

int DeepStarComponent::starColorIntensity( void ) const {
    return m_Data->colorScheme()->starColorIntensity();
}

void DeepStarComponent::byteSwap( deepStarData *stardata ) {
    bswap_32( stardata->RA );
    bswap_32( stardata->Dec );
    bswap_16( stardata->dRA );
    bswap_16( stardata->dDec );
    bswap_16( stardata->B );
    bswap_16( stardata->V );
}

void DeepStarComponent::byteSwap( starData *stardata ) {
    bswap_32( stardata->RA );
    bswap_32( stardata->Dec );
    bswap_32( stardata->dRA );
    bswap_32( stardata->dDec );
    bswap_32( stardata->parallax );
    bswap_32( stardata->HD );
    bswap_16( stardata->mag );
    bswap_16( stardata->bv_index );
}

bool DeepStarComponent::verifySBLIntegrity() {
    float faintMag = -5.0;
    bool integrity = true;
    for(Trixel trixel = 0; trixel < m_skyMesh->size(); ++trixel) {
        for(int i = 0; i < m_starBlockList[ trixel ]->getBlockCount(); ++i) {
            StarBlock *block = m_starBlockList[ trixel ]->block( i );
            if( i == 0 )
                faintMag = block->getBrightMag();
            // NOTE: Assumes 2 decimal places in magnitude field. TODO: Change if it ever does change
            if( block->getBrightMag() != faintMag && ( block->getBrightMag() - faintMag ) > 0.5) {
                kDebug() << "Trixel " << trixel << ": ERROR: faintMag of prev block = " << faintMag 
                         << ", brightMag of block #" << i << " = " << block->getBrightMag();
                integrity = false;
            }
            if( i > 1 && ( !block->prev ) )
                kDebug() << "Trixel " << trixel << ": ERROR: Block" << i << "is unlinked in LRU Cache";
            if( block->prev && block->prev->parent == m_starBlockList[ trixel ] 
                && block->prev != m_starBlockList[ trixel ]->block( i - 1 ) ) {
                kDebug() << "Trixel " << trixel << ": ERROR: SBF LRU Cache linked list seems to be broken at before block " << i << endl;
                integrity = false;
            }
            faintMag = block->getFaintMag();
        }
    }
    return integrity;
}
