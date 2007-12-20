/***************************************************************************
               linelistindex.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-04
    copyright            : (C) 2007 James B. Bowlin
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

#ifndef LINELISTINDEX_H
#define LINELISTINDEX_H

#include <QList>
#include <QHash>

#include "skycomposite.h"
#include "skymesh.h"
#include "typedef.h"

class QPainter;
class LineList;

/* @class LineListIndex
 * Contains almost all the code needed for indexing and drawing and clipping
 * lines and polygons.
 *
 * @author James B. Bowlin @version 0.1
 */
class LineListIndex : public SkyComposite
{
public:
    /* @short Constructor
     * Simply set the internal skyMesh, parent, and name.
     * @param parent Pointer to the parent SkyComponent object
     * @param mesh Pointer to the universal SkyMesh instance
     * @param name name of the subclass used for debugging
     */
    LineListIndex( SkyComponent *parent, const QString& name="" );

    /* @short this is called from within the draw routines when the updateID
     * of the lineList is stale.  It is virtual because different subclasses
     * have different update routines.  NoPrecessIndex doesn't precess in
     * the updates and ConstellationLines must update its points as stars,
     * not points.  that doesn't precess the points.
     */
    virtual void JITupdate( KStarsData *data, LineList* lineList );

    /* @short Returns the SkyMesh object.
     */
    SkyMesh* skyMesh() { return  m_skyMesh;   }

    /* @short Returns the Hash of QLists of LineLists that
     * is used for doing the indexing line segments.
     */
    LineListHash* lineIndex() { return m_lineIndex; }

    /* @short Returns the Hash of QLists of LineLists that
     * is used for indexing filled polygons.
     */
    LineListHash* polyIndex() { return m_polyIndex; }

    /* @short as the name says, recreates the lineIndex using the LineLists
     * in the previous index.  Since we are indexing everything at J2000
     * this is only used by ConstellationLines which needs to reindex 
     * because of the proper motion of the stars.
     */
    void reindexLines();

    /* @short Typically called from within a subclasses init() method.
     * Adds the trixels covering the outline of lineList to the lineIndex.
     *
     * @param debug if greater than zero causes the number of trixels found
     * to be printed.
     */
    void appendLine( LineList* lineList, int debug=0 );

    /* @short Typically called from within a subclasses init() method.
     * Adds the trixels covering the full lineList to the polyIndex.
     *
     * @param debug if greater than zero causes the number of trixels found
     * to be printed.
     */
    void appendPoly( LineList* lineList, int debug=0 );

    /* @short a convenience method that adds a lineList to both the lineIndex
     * and the polyIndex.
     */
    void appendBoth( LineList* lineList, int debug=0 );

    /* @short Returns an IndexHash from the SkyMesh that contains the set of
     * trixels that cover lineList.  Overridden by SkipListIndex so it can
     * pass SkyMesh an IndexHash indicating which line segments should not
     * be indexed @param lineList contains the list of points to be covered.
     */
    virtual const IndexHash& getIndexHash(LineList* lineList );


    //----- Drawing Routines -----

    /* @short.  The top level draw routine.  Draws all the LineLists for any
     * subclass in one fell swoop which minimizes some of the loop overhead.
     * Overridden by MilkWay so it can decide whether to draw outlines or
     * filled.  Therefore MilkyWay does not need to override preDraw().  The
     * MilkyWay draw() routine calls all of the more specific draw()
     * routines below.
     */
    virtual void draw( QPainter &psky );

    /* @short Gives the subclasses access to the top of the draw() method.
     * Typically used for setting the QPen, etc. in the QPainter being
     * passed in.  Defaults to setting a thin white pen.
     */
    virtual void preDraw( QPainter &psky );

    /* @short Also overridden by SkipListIndex.  Controls skipping inside of
     * the draw() routines.  The default behavior is to simply return false
     * but this was moved into the .cpp file to prevent this header file
     * from generating repeated unused parameter warnings.
     */
    virtual bool skipAt( LineList* lineList, int i );


    virtual void updateLabelCandidates( const QPointF& o, LineList* lineList, int i );

    virtual void updateLabelCandidates( const QPoint& o, LineList* lineList, int i );

    /* @short a callback overridden by NoPrecessIndex so it can use the
     * drawing code with the non-reverse-precessed mesh buffer.
     */
    virtual MeshBufNum_t drawBuffer() { return DRAW_BUF; }

    /* @short Draws all the lines without making use of the index.  Used by
     * NoPrecessIndex for cases when the screen is zoomed out and creating
     * the 2nd mesh buffer would be more expensive than just drawing
     * everything.  There are no filled versions of the "All" routines
     * simply because they were not needed.
     */
    void drawAllLines( QPainter &psky );

    /* @short Draws all the lines in m_listList as simple lines in float
     * mode.
     */
    void drawLines( QPainter &psky );

    /* @short Draws all the lines in m_listList as filled polygons in float
     * mode.
     */
    void drawFilled( QPainter& psky );


    //----- Debugging and Info Routines -----

    /* @short
     */
    QString& name() { return m_name; }

    /* @short displays a message that we are loading m_name.  Also prints
     * out the message if skyMesh debug is greater than zero.
     */
    void intro();

    /* @short prints out some summary statistics if the skyMesh debug is
     * greater than 1.
     */
    void summary();

private:
    QString      m_name;
    int          m_lineIndexCnt;
    int          m_polyIndexCnt;

    //LineListList  m_listList;
    SkyMesh*      m_skyMesh;
    LineListHash* m_lineIndex;
    LineListHash* m_polyIndex;

    LineListList  m_listList;

};

#endif
