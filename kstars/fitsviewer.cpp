/***************************************************************************
                          FITSViewer.cpp  -  A FITSViewer for KStars
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com

 2006-03-03	Using CFITSIO, Porting to Qt4
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "fitsviewer.h"

#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kstandardaction.h>

#include <kdebug.h>
#include <ktoolbar.h>
#include <ktemporaryfile.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klineedit.h>
#include <kicon.h>
#include <KUndoStack>

#include <QFile>
#include <QCursor>
#include <QRadioButton>
#include <QClipboard>
#include <QImage>
#include <QRegExp>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QTreeWidget>
#include <QHeaderView>
#include <QApplication>



#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

#include "fitsimage.h"
#include "fitshistogram.h"
#include "ui_statform.h"
#include "ui_fitsheaderdialog.h"
#include "ksutils.h"
#include "Options.h"

FITSViewer::FITSViewer (const KUrl *url, QWidget *parent)
	: KXmlGuiWindow (parent)
{
    image      = NULL;
    currentURL = *url;
    histogram      = NULL;
    Dirty      = 0;

    history = new KUndoStack();
    history->setUndoLimit(10);
    history->clear();
    connect(history, SIGNAL(cleanChanged(bool)), this, SLOT(fitsRestore(bool)));

    history->createUndoAction(actionCollection());
    history->createRedoAction(actionCollection());

    /* Setup image widget */
    image = new FITSImage(this);
    setCentralWidget(image);

    statusBar()->insertItem(QString(), 0);
    statusBar()->setItemFixed(0, 100);
    statusBar()->insertItem(QString(), 1);
    statusBar()->setItemFixed(1, 100);
    statusBar()->insertItem(QString(), 2);
    statusBar()->setItemFixed(2, 100);
    statusBar()->insertItem(QString(), 3);
    statusBar()->setItemFixed(3, 50);
    statusBar()->insertPermanentItem(i18n("Welcome to KStars FITS Viewer"), 4, 1);
    statusBar()->setItemAlignment(4 , Qt::AlignLeft);

    /* FITS initializations */
    if (!initFITS())
    {
     close();
     return;
    }

    QFile tempFile;
    QAction *action;

    if (KSUtils::openDataFile( tempFile, "histogram.png" ) )
    {
    	action = actionCollection()->addAction("image_histogram");
        action->setIcon(KIcon(tempFile.fileName()));
        action->setText(i18n("Histogram"));
    	connect(action, SIGNAL(triggered(bool) ), SLOT (imageHistogram()));
    	action->setShortcuts(KShortcut( Qt::CTRL+Qt::Key_H ));
	tempFile.close();
    }
    else {
        action = actionCollection()->addAction("image_histogram");
        action->setIcon(KIcon("wizard"));
        action->setText(i18n("Histogram"));
        connect(action, SIGNAL(triggered(bool)), SLOT (imageHistogram()));
        action->setShortcuts(KShortcut( Qt::CTRL+Qt::Key_H ));
    }

    KStandardAction::open(this, SLOT(fileOpen()), actionCollection());
    KStandardAction::save(this, SLOT(fileSave()), actionCollection());
    KStandardAction::saveAs(this, SLOT(fileSaveAs()), actionCollection());
    KStandardAction::close(this, SLOT(slotClose()), actionCollection());
    KStandardAction::copy(this, SLOT(fitsCOPY()), actionCollection());
    KStandardAction::zoomIn(image, SLOT(fitsZoomIn()), actionCollection());
    KStandardAction::zoomOut(image, SLOT(fitsZoomOut()), actionCollection());
    action = actionCollection()->addAction("zoom_default" );
    action->setIcon(KIcon("zoom-best-fit"));
    action->setText(i18n( "&Default Zoom" ));
    connect(action, SIGNAL(triggered(bool) ), image, SLOT(fitsZoomDefault()));
    action->setShortcuts(KShortcut( Qt::CTRL+Qt::Key_D ));
    action = actionCollection()->addAction("image_stats");
    action->setIcon(KIcon("datashowchart"));
    action->setText(i18n( "Statistics"));
    connect(action, SIGNAL(triggered(bool)), SLOT(fitsStatistics()));
    action = actionCollection()->addAction("fits_editor");
    action->setIcon(KIcon("fileview-text"));
    action->setText(i18n( "FITS Header"));
    connect(action, SIGNAL(triggered(bool) ), SLOT(fitsHeader()));

   /* Create GUI */
   createGUI("fitsviewer.rc");

   /* initially resize in accord with KDE rules */
   resize(INITIAL_W, INITIAL_H);
}

FITSViewer::~FITSViewer()
{

}

bool  FITSViewer::initFITS()
{

    /* Display image in the central widget */
    if (image->loadFits(currentURL.path()) == -1) { close(); return false; }

    /* Clear history */
    history->clear();

    /* Set new file caption */
    setWindowTitle(currentURL.fileName());

   statusBar()->changeItem( QString("%1 x %2").arg( (int) image->stats.dim[0]).arg( (int) image->stats.dim[1]), 2);

    return true;

}

void FITSViewer::slotClose()
{

  if (Dirty)
  {

  QString caption = i18n( "Save Changes to FITS?" );
		QString message = i18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard() );
		if ( ans == KMessageBox::Yes )
			fileSave();
		else if ( ans == KMessageBox::No )
		{
			history->clear();
			fitsRestore();
		}
   }

   if (Dirty == 0)
    close();
}

void FITSViewer::closeEvent(QCloseEvent *ev)
{

  if (Dirty)
  {

  QString caption = i18n( "Save Changes to FITS?" );
		QString message = i18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard() );
		if ( ans == KMessageBox::Yes )
			fileSave();
		else if ( ans == KMessageBox::No )
		{
			history->clear();
			fitsRestore();
		}
   }

   if (Dirty == 0)
    ev->accept();
   else
    ev->ignore();

}

void FITSViewer::fileOpen()
{

  if (Dirty)
  {

  QString caption = i18n( "Save Changes to FITS?" );
		QString message = i18n( "The current FITS file has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard() );
		if ( ans == KMessageBox::Yes )
			fileSave();
		else if ( ans == KMessageBox::No )
		{
			history->clear();
			fitsRestore();
		}
   }

   KUrl fileURL = KFileDialog::getOpenUrl( QDir::homePath(), "*.fits *.fit *.fts|Flexible Image Transport System");

  if (fileURL.isEmpty())
    return;


  currentURL = fileURL;

  // Close FITS if open and delete it
  if (histogram != NULL)
  {
  	histogram->close();
  	delete (histogram);
  	histogram = NULL;
  }

  initFITS();

}

void FITSViewer::fileSave()
{

  int err_status;
  char err_text[FLEN_STATUS];

  KUrl backupCurrent = currentURL;
  QString currentDir = Options::fitsSaveDirectory();

  // If no changes made, return.
  if (Dirty == 0 && !currentURL.isEmpty())
    return;

  if (currentURL.isEmpty())
  {
  	currentURL = KFileDialog::getSaveUrl( currentDir, "*.fits |Flexible Image Transport System");
	// if user presses cancel
	if (currentURL.isEmpty())
	{
	  currentURL = backupCurrent;
	  return;
	}
	if (currentURL.path().contains('.') == 0) currentURL.setPath(currentURL.path() + ".fits");

	if (QFile::exists(currentURL.path()))
        {

            int r=KMessageBox::warningContinueCancel(0,
            i18n( "A file named \"%1\" already exists. "
                  "Overwrite it?", currentURL.fileName() ),
            i18n( "Overwrite File?" ),
	    KGuiItem(i18n( "&Overwrite" )) );

		if(r==KMessageBox::Cancel) return;
         }
   }

  if ( currentURL.isValid() )
  {
	  if ( (err_status = image->saveFITS('!' + currentURL.path())) < 0)
	  {
		  fits_get_errstatus(err_status, err_text);
		  // Use KMessageBox or something here
		  KMessageBox::error(0, i18n("FITS file save error: %1",
                              QString::fromUtf8(err_text)), i18n("FITS Save"));
		  return;
	  }


	statusBar()->changeItem(i18n("File saved."), 3);

	Dirty = 0;
	history->clear();
	fitsRestore();
  }
  else
  {
		QString message = i18n( "Invalid URL: %1", currentURL.url() );
		KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
  }
}

void FITSViewer::fileSaveAs()
{

  currentURL = QString();
  fileSave();
}

void FITSViewer::fitsCOPY()
{
   QApplication::clipboard()->setImage(*image->displayImage);
}

void FITSViewer::imageHistogram()
{

  if (histogram == NULL)
  {
    histogram = new FITSHistogram(this);
    histogram->show();
  }
  else
    histogram->show();

}

void FITSViewer::fitsRestore(bool clean)
{
 if (clean)
 {
 	Dirty = 0;
 	setWindowTitle(currentURL.fileName());
 }
}

void FITSViewer::fitsChange()
{

 Dirty = 1;

 setWindowTitle(currentURL.fileName() + i18n(" [modified]"));
}

void FITSViewer::fitsStatistics()
{
  QDialog statDialog;
  Ui::statForm stat;
  stat.setupUi(&statDialog);

  stat.widthOUT->setText(QString::number(image->stats.dim[0]));
  stat.heightOUT->setText(QString::number(image->stats.dim[1]));
  stat.bitpixOUT->setText(QString::number(image->stats.bitpix));
  stat.maxOUT->setText(QString::number(image->stats.max));
  stat.minOUT->setText(QString::number(image->stats.min));
  stat.meanOUT->setText(QString::number(image->stats.average));
  stat.stddevOUT->setText(QString::number(image->stats.stddev));

  statDialog.exec();
}

void FITSViewer::fitsHeader()
{
   QString recordList;
   QString record;
   QStringList properties;
   QTableWidgetItem *tempItem;
   int nkeys;
   int err_status;
   char err_text[FLEN_STATUS];

   if ( (err_status = image->getFITSRecord(recordList, nkeys)) < 0)
   {
        fits_get_errstatus(err_status, err_text);
	KMessageBox::error(0, i18n("FITS record error: %1", QString::fromUtf8(err_text)), i18n("FITS Header"));
	return;
   }

   QDialog fitsHeaderDialog;
   Ui::fitsHeaderDialog header;
   header.setupUi(&fitsHeaderDialog);



   header.tableWidget->setRowCount(nkeys);
   for (int i=0; i < nkeys; i++)
   {

   	record = recordList.mid(i*80, 80);

   	// I love regexp!
   	properties = record.split(QRegExp("[=/]"));

	tempItem = new QTableWidgetItem(properties[0].simplified());
	header.tableWidget->setItem(i, 0, tempItem);

	if (properties.size() > 1)
	{
		tempItem = new QTableWidgetItem(properties[1].simplified());
		header.tableWidget->setItem(i, 1, tempItem);
	}
	if (properties.size() > 2)
	{
		tempItem = new QTableWidgetItem(properties[2].simplified());
		header.tableWidget->setItem(i, 2, tempItem);
	}

   }

   header.tableWidget->resizeColumnsToContents();

   fitsHeaderDialog.exec();

}

#include "fitsviewer.moc"
