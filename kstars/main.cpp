/***************************************************************************
                          main.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb  5 01:11:45 PST 2001
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

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdebug.h>
#include <kapplication.h>

#include "kstars.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "skymap.h"
#include "simclock.h"
#include "ksnumbers.h"
#include "Options.h"
//Added by qt3to4:
#include <QPixmap>
#include <kglobal.h>

#define KSTARS_VERSION "1.2.90"

static const char description[] =
	I18N_NOOP("Desktop Planetarium");
static const char notice[] =
	I18N_NOOP("Some images in KStars are for non-commercial use only.  See README.images.");


int main(int argc, char *argv[])
{
	KAboutData aboutData( "kstars", 0, ki18n("KStars"),
		KSTARS_VERSION, ki18n(description), KAboutData::License_GPL,
		ki18n("(c) 2001-2003, The KStars Team"), ki18n(notice), "http://edu.kde.org/kstars");
	aboutData.addAuthor(ki18n("Jason Harris"),KLocalizedString(), "jharris@30doradus.org", "http://www.30doradus.org");
	aboutData.addAuthor(ki18n("Heiko Evermann"),KLocalizedString(), "heiko@evermann.de", "http://www.evermann.de");
	aboutData.addAuthor(ki18n("Thomas Kabelmann"), KLocalizedString(), "tk78@gmx.de");
	aboutData.addAuthor(ki18n("Pablo de Vicente"), KLocalizedString(), "pvicentea@wanadoo.es");
	aboutData.addAuthor(ki18n("Jasem Mutlaq"), KLocalizedString(), "mutlaqja@ikarustech.com");
	aboutData.addAuthor(ki18n("Carsten Niehaus"), KLocalizedString(), "cniehaus@gmx.de");
	aboutData.addAuthor(ki18n("Mark Hollomon"), KLocalizedString(), "mhh@mindspring.com");
	KCmdLineArgs::init( argc, argv, &aboutData );

	KCmdLineOptions options;
	options.add("!dump", ki18n( "Dump sky image to file" ));
	options.add("script ", ki18n( "Script to execute" ));
	options.add("width ", ki18n( "Width of sky image" ), "640");
	options.add("height ", ki18n( "Height of sky image" ), "480");
	options.add("filename ", ki18n( "Filename for sky image" ), "kstars.png");
	options.add("date ", ki18n( "Date and time" ));
	options.add("!paused", ki18n( "Start with clock paused" ));
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.
	KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

	KApplication a;

	if ( args->isSet( "dump" ) ) {
		kDebug() << i18n( "Dumping sky image" ) << endl;

		//parse filename and image format
		const char* format = "PNG";
		QString fname = args->getOption( "filename" );
		QString ext = fname.mid( fname.lastIndexOf(".")+1 );
		if ( ext.toLower() == "png" ) { format = "PNG"; }
		else if ( ext.toLower() == "jpg" || ext.toLower() == "jpeg" ) { format = "JPG"; }
		else if ( ext.toLower() == "gif" ) { format = "GIF"; }
		else if ( ext.toLower() == "pnm" ) { format = "PNM"; }
		else if ( ext.toLower() == "bmp" ) { format = "BMP"; }
		else { kWarning() << i18n( "Could not parse image format of %1; assuming PNG.", fname ) << endl; }

		//parse width and height
		bool ok(false);
		int w(0), h(0);
		w = args->getOption( "width" ).toInt( &ok );
		if ( ok ) h =  args->getOption( "height" ).toInt( &ok );
		if ( !ok ) {
			kWarning() << "Unable to parse arguments: " << endl;
			kWarning() << "Width: " << args->getOption( "width" )
				<< "  Height: " << args->getOption( "height" ) << endl;
			return 1;
		}

		KStarsData *dat = new KStarsData();
		QObject::connect( dat, SIGNAL( progressText(QString) ), dat, SLOT( slotConsoleMessage(QString) ) );
		dat->initialize();
		while (!dat->startupComplete) { qApp->processEvents(); }

		//Set Geographic Location
		dat->setLocationFromOptions(); 

		//Set color scheme
		dat->colorScheme()->loadFromConfig( KGlobal::config().data() );

		//set clock now that we have a location:
		//Check to see if user provided a date/time string.  If not, use current CPU time
		QString datestring = args->getOption( "date" );
		KStarsDateTime kdt;
		if ( ! datestring.isEmpty() ) {
			if ( datestring.contains( "-" ) ) { //assume ISODate format
				if ( datestring.contains( ":" ) ) { //also includes time
					kdt = KStarsDateTime::fromString( datestring, Qt::ISODate );
				} else { //string probably contains date only
					kdt.setDate( ExtDate::fromString( datestring, Qt::ISODate ) );
					kdt.setTime( QTime( 0, 0, 0 ) );
				}
			} else { //assume Text format for date string
				kdt = dat->geo()->LTtoUT( KStarsDateTime::fromString( datestring, Qt::TextDate ) );
			}
			
			if ( ! kdt.isValid() ) {
				kWarning() << i18n( "Could not parse Date/Time string: " ) << datestring << endl;
				kWarning() << i18n( "Valid date formats: " ) << endl;
				kWarning() << "  1950-02-25  ;  1950-02-25 05:30:00" << endl;
				kWarning() << "  Feb 25 1950 ;  Feb 25 1950 05:30:00" << endl;
				kWarning() << "  25 Feb 1950 ;  25 Feb 1950 05:30:00" << endl;
				kWarning() << i18n( "Using CPU date/time instead." ) << endl;
				
				kdt = dat->geo()->LTtoUT( KStarsDateTime::currentDateTime() );
			}
		} else { 
			kdt = dat->geo()->LTtoUT( KStarsDateTime::currentDateTime() );
		}
		dat->clock()->setUTC( kdt );
		
		KSNumbers num( dat->ut().djd() );
		//		dat->initGuides(&num);

		SkyMap *map = new SkyMap( dat );
		map->resize( w, h );
		QPixmap sky( w, h );

		map->setDestination( new SkyPoint( Options::focusRA(), Options::focusDec() ) );
		map->destination()->EquatorialToHorizontal( dat->lst(), dat->geo()->lat() );
		map->setFocus( map->destination() );
		map->focus()->EquatorialToHorizontal( dat->lst(), dat->geo()->lat() );

		//Execute the specified script
		QString scriptfile = args->getOption( "script" );
		if ( ! scriptfile.isEmpty() ) {
			if ( dat->executeScript( scriptfile, map ) ) {
				std::cout << i18n( "Script executed." ).toUtf8().data() << std::endl;
			} else {
				kWarning() << i18n( "Could not execute script." ) << endl;
			}
		}

		dat->setFullTimeUpdate();
		dat->updateTime(dat->geo(), map );

		qApp->processEvents();
		map->setMapGeometry();
		map->exportSkyImage( &sky );
		qApp->processEvents();

		if ( ! sky.save( fname, format ) ) kWarning() << i18n( "Unable to save image: %1 ", fname ) << endl;
		else kDebug() << i18n( "Saved to file: %1", fname ) << endl;

		delete map;
		delete dat;
		return 0;
	}

	//start up normally in GUI mode
	
	//Try to parse the given date string
	QString datestring = args->getOption( "date" );
	if ( ! datestring.isEmpty() && ! KStarsDateTime::fromString( datestring ).isValid() ) {
		kWarning() << i18n("Specified date (%1) is invalid.  Will use current CPU date instead.", datestring ) << endl;
		datestring = QString();
	}
	
	new KStars( true, ! args->isSet( "paused" ), datestring );
	args->clear();
	QObject::connect(kapp, SIGNAL(lastWindowClosed()), kapp, SLOT(quit()));
	return a.exec();

}
