
/*********************************************************************************************

    This is public domain software that was developed by or for the U.S. Naval Oceanographic
    Office and/or the U.S. Army Corps of Engineers.

    This is a work of the U.S. Government. In accordance with 17 USC 105, copyright protection
    is not available for any work of the U.S. Government.

    Neither the United States Government nor any employees of the United States Government,
    makes any warranty, express or implied, without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, or assumes any liability or
    responsibility for the accuracy, completeness, or usefulness of any information,
    apparatus, product, or process disclosed, or represents that its use would not infringe
    privately-owned rights. Reference herein to any specific commercial products, process,
    or service by trade name, trademark, manufacturer, or otherwise, does not necessarily
    constitute or imply its endorsement, recommendation, or favoring by the United States
    Government. The views and opinions of authors expressed herein do not necessarily state
    or reflect those of the United States Government, and shall not be used for advertising
    or product endorsement purposes.

*********************************************************************************************/

#include "CZMILwaveMonitor.hpp"
#include "CZMILwaveMonitorHelp.hpp"

double settings_version = 3.04;


CZMILwaveMonitor::CZMILwaveMonitor (int32_t *argc, char **argv, QWidget * parent):
  QMainWindow (parent, 0)
{        
  extern char        *optarg;

  filError = NULL;

  strcpy (progname, argv[0]);
  lock_track = NVFalse;
  zoom_rectangle = -1;


  QResource::registerResource ("/icons.rcc");


  //  Have to set the focus policy or keypress events don't work properly at first in Focus Follows Mouse mode

  setFocusPolicy (Qt::WheelFocus);
  setFocus (Qt::ActiveWindowFocusReason);


  //  Set the main icon

  setWindowIcon (QIcon (":/icons/wave_monitor.png"));

  zoomCursor = QCursor (QPixmap (":/icons/zoom_cursor.png"), 11, 11);

  kill_switch = ANCILLARY_FORCE_EXIT;

  int32_t option_index = 0;
  while (NVTrue) 
    {
      static struct option long_options[] = {{"actionkey00", required_argument, 0, 0},
                                             {"actionkey01", required_argument, 0, 0},
                                             {"actionkey02", required_argument, 0, 0},
                                             {"actionkey03", required_argument, 0, 0},
                                             {"actionkey04", required_argument, 0, 0},
                                             {"actionkey05", required_argument, 0, 0},
                                             {"actionkey06", required_argument, 0, 0},
                                             {"actionkey07", required_argument, 0, 0},
                                             {"actionkey08", required_argument, 0, 0},
                                             {"actionkey09", required_argument, 0, 0},
                                             {"actionkey10", required_argument, 0, 0},
                                             {"actionkey11", required_argument, 0, 0},
                                             {"shared_memory_key", required_argument, 0, 0},
                                             {"kill_switch", required_argument, 0, 0},
                                             {0, no_argument, 0, 0}};

      char c = (char) getopt_long (*argc, argv, "o", long_options, &option_index);
      if (c == -1) break;

      switch (c) 
        {
        case 0:
          switch (option_index)
            {
            case 12:
              sscanf (optarg, "%d", &key);
              break;

            case 13:
              sscanf (optarg, "%d", &kill_switch);
              break;

            default:
              char tmp;
              sscanf (optarg, "%1c", &tmp);
              ac[option_index] = (uint32_t) tmp;
              break;
            }
          break;
        }
    }


  //  This is the "tools" toolbar.  We have to do this here so that we can restore the toolbar location(s).

  primaryColorNear = Qt::green;
  secondaryColorNear = Qt::red;
  transPrimaryColorNear = primaryColorNear;
  transPrimaryColorNear.setAlpha (WAVE_ALPHA);
  transSecondaryColorNear = secondaryColorNear;
  transSecondaryColorNear.setAlpha (WAVE_ALPHA);

  drawReference = NVFalse;
  numReferences = 0;


  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //  Just playing with the Savitzky-Golay filter in the following block.  I'm going to leave it here in case anyone wants to
  //  play around with it.  The overhead is pretty much non-existent.
    /*
  for (int32_t i = 1 ; i <= SAVGOL_MMAX ; i++) signal[i] = ysave[i] = 0.0;
  nleft = 10;
  nright = 10;
  moment = 2;

  ndex[1] = 0;
  int32_t j = 3;
  for (int32_t i = 2 ; i <= nleft + 1 ; i++)
    {
      ndex[i] = i - j;
      j += 2;
    }

  j = 2;
  for (int32_t i = nleft + 2 ; i <= nleft + nright + 1 ; i++)
    {
      ndex[i] = i - j;
      j += 2;
    }


  //  Calculate Savitzky-Golay filter coefficients.

  savgol (coeff, nleft + nright + 1, nleft, nright, 0, moment);
    */
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


  envin ();


  // Set the application font

  QApplication::setFont (font);


  this->setWindowTitle (program_version);


  //  Get the shared memory area.  If it doesn't exist, quit.  It should have already been created 
  //  by pfmView or pfmEdit3D.

  QString skey;
  skey.sprintf ("%d_abe", key);

  abeShare = new QSharedMemory (skey);

  if (!abeShare->attach (QSharedMemory::ReadWrite))
    {
      fprintf (stderr, "%s %s %s %d - abeShare - %s\n", progname, __FILE__, __FUNCTION__, __LINE__, strerror (errno));
      exit (-1);
    }

  abe_share = (ABE_SHARE *) abeShare->data ();


  //  Open all the PFM structures that are being viewed in pfmEdit/pfmView

  num_pfms = abe_share->pfm_count;

  for (int32_t pfm = 0 ; pfm < num_pfms ; pfm++)
    {
      open_args[pfm] = abe_share->open_args[pfm];

      if ((pfm_handle[pfm] = open_existing_pfm_file (&open_args[pfm])) < 0)
        {
          QMessageBox::warning (this, tr ("Open PFM Structure"),
                                tr ("The file %1 is not a PFM handle or list file or there was an error reading the file.\nThe error message returned was:\n\n%2").arg 
                                (QDir::toNativeSeparators (QString (abe_share->open_args[pfm].list_path))).arg (pfm_error_str (pfm_error)));
        }
    }


  //  Set the window size and location from the defaults

  this->resize (width, height);
  this->move (window_x, window_y);


  //  Allocate the colors to be used for color by line.  These are full value and half value colors.
  //  We're going to range the colors from 200 to 0 then wrap around from 360 to 300 which gives us a 
  //  range of 260.  The colors transitions for this don't need to be smooth since the locations are
  //  somewhat random based on the line number.  These colors never change (unlike the color by depth or 
  //  attribute colors) but the alpha value may change.

  float hue_inc = 260.0 / (float) (NUMSHADES + 1);

  for (int32_t i = 0 ; i < 2 ; i++)
    {
      float start_hue = 200.0;

      for (int32_t j = 0 ; j < NUMSHADES ; j++)
        {
          int32_t hue = (int32_t) (start_hue - (float) j * hue_inc);
          if (hue < 0) hue = 360 + hue;

          int32_t k = NUMSHADES * i + j;


          if (i)
            {
              colorArray[k].setHsv (hue, 255, 127, 255);
            }
          else
            {
              colorArray[k].setHsv (hue, 255, 255, 255);
            }
        }
    }


  //  Set all of the default values.  

  /* 0: shallow1, 1: shallow2, 2: shallow3, 3: shallow4
     4: shallow5, 5: shallow6, 6: shallow7, 7: ir, 8: deep
     9: T0 */

  for (int32_t i = 0; i < NUM_WAVEFORMS; i++)
    {
      //  IR

      if (i == 7)
        {
          uTimeBound[i] = 640;
        }


      //  T0

      else if (i == 9)
        {
          uTimeBound[i] = 64;
        }


      //  Everything else

      else
        {
          uTimeBound[i] = UPPER_TIME_BOUND;
        }


      lTimeBound[i]= LOWER_TIME_BOUND;
      lCountBound[i] = LOWER_COUNT_BOUND;
      uCountBound[i] = UPPER_COUNT_BOUND;
    }        

  nonFileIO_force_redraw = NVFalse;

  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) mapBlocked[i] = NVFalse;

  cwf_read = 0;
  active_window_id = getpid ();


  //  Let pfmEdit know that we are up and running.


  /* Bit 1 constitutes CZMIL Waveform Viewer */    

  abe_share->mwShare.waveMonitorRunning |= 0x02;

  zoomFlag = NVFalse;
  zoomIsActive = NVFalse;
  abe_share->mwShare.multiMode = multiWaveMode;

  abe_share->mwShare.multiPresent[0] = -1;
  for (int32_t i = 1; i < MAX_STACK_POINTS; i++) abe_share->mwShare.multiPresent[i] = -1;

  abe_share->mwShare.multiSwitch = NVFalse;

  GetLabelSpacing ();


  //  Set the map values from the defaults

  mapdef.projection = NO_PROJECTION;
  mapdef.draw_width = width;
  mapdef.draw_height = height;
  mapdef.overlap_percent = 5;
  mapdef.grid_inc_x = 0.0;
  mapdef.grid_inc_y = 0.0;

  mapdef.coasts = NVFalse;
  mapdef.landmask = NVFalse;

  mapdef.border = 0;
  mapdef.coast_color = Qt::white;
  mapdef.grid_color = QColor (160, 160, 160, 127);

  mapdef.background_color = backgroundColor;

  mapdef.initial_bounds.min_x = 0;
  mapdef.initial_bounds.min_y = 0;

  mapdef.initial_bounds.max_x = 500;
  mapdef.initial_bounds.max_y = 500;


  QFrame *frame = new QFrame (this, 0);
  
  setCentralWidget (frame);

  for (int32_t i = 0; i < NUM_WAVEFORMS; i++)
    {
      map[i] = new nvMap (this, &mapdef);
      map[i]->setWhatsThis (mapText);
    }

  QHBoxLayout *hBox = new QHBoxLayout (frame);
  QVBoxLayout *vBox = new QVBoxLayout;
  QVBoxLayout *vBox2 = new QVBoxLayout;
  
  statusBar = new QStatusBar (frame);
  statusBar->setSizeGripEnabled (false);
  statusBar->show ();
  
  QHBoxLayout * hBox1 = new QHBoxLayout();
  
  hBox1->addWidget (map[0]);
  hBox1->addWidget (map[1]);
  hBox1->addWidget (map[2]);
  
  vBox2->addLayout(hBox1);
  vBox2->addWidget (statusBar);
  
  QHBoxLayout * hBox2 = new QHBoxLayout();
  
  hBox2->addWidget (map[3]);
  hBox2->addWidget (map[4]);
  hBox2->addWidget (map[5]);
  
  QHBoxLayout * hBox3 = new QHBoxLayout();
  hBox3->addWidget (map[6]);
  hBox3->addWidget (map[7]);

  QHBoxLayout * hBox4 = new QHBoxLayout();
  hBox4->addWidget (map[8]);
  hBox4->addWidget (map[9]);

  hBox3->addLayout (hBox4);


  vBox->addLayout (vBox2);
  vBox->addLayout (hBox2);
  vBox->addLayout (hBox3);
  
  hBox->addLayout (vBox);

  if (current_channel_mode)
    {
      map[4]->setVisible (true);
    }
  else
    {
      for (int32_t i = 0; i < NUM_WAVEFORMS; i++) map[i]->setVisible (savedSettings.visible[i]);
    }


  nameLabel = new QLabel ("                                        ", this);
  nameLabel->setAlignment (Qt::AlignLeft);
  nameLabel->setMinimumSize (nameLabel->sizeHint ());
  nameLabel->setWhatsThis (nameLabelText);
  nameLabel->setToolTip (tr ("File name"));

  recordLabel = new QLabel (" 0000000000 ", this);
  recordLabel->setAlignment (Qt::AlignLeft);
  recordLabel->setMinimumSize (recordLabel->sizeHint ());
  recordLabel->setWhatsThis (recordLabelText);
  recordLabel->setToolTip (tr ("Record number"));

  simDepth = new QLabel ("0000.00", this);
  simDepth->setAlignment (Qt::AlignCenter);
  simDepth->setMinimumSize (simDepth->sizeHint ());
  simDepth->setWhatsThis (simDepthText);
  simDepth->setToolTip (tr ("Tide/datum corrected depth"));
  simDepth->setMinimumSize (simDepth->sizeHint ());
  simDepth->setWhatsThis (simDepthText);
  simDepth->setToolTip (tr ("Simulated depth"));

  statusBar->addWidget (nameLabel, 1);
  statusBar->addWidget (recordLabel);
  statusBar->addWidget (simDepth);

  for (int32_t i = 0; i < NUM_WAVEFORMS; i++)
    {
      connect (map[i], SIGNAL (mousePressSignal (QMouseEvent *, double, double)), this, SLOT (slotMousePress (QMouseEvent *, double, double)));
      connect (map[i], SIGNAL (mouseMoveSignal (QMouseEvent *, double, double)), this, SLOT (slotMouseMove (QMouseEvent *, double, double)));
      connect (map[i], SIGNAL (resizeSignal (QResizeEvent *)), this, SLOT (slotResize (QResizeEvent *)));
      connect (map[i], SIGNAL (postRedrawSignal (NVMAP_DEF)), this, SLOT (slotPlotWaves (NVMAP_DEF)));
    }
  
  contextMenu = new QMenu (this);
  QAction *contextZoomIn = contextMenu->addAction (tr ("Zoom In"));
  QAction *contextResetZoom = contextMenu->addAction (tr ("Reset Zoom"));
  connect (contextZoomIn, SIGNAL (triggered ()), this, SLOT (slotZoomIn ()));
  
  connect (contextResetZoom, SIGNAL (triggered()), this, SLOT (slotContextResetZoom()));


  //  Setup the file menu.
  
  QAction *fileQuitAction = new QAction (tr ("&Quit"), this);
  
  
  fileQuitAction->setWhatsThis (tr ("Exit from application"));
  connect (fileQuitAction, SIGNAL (triggered ()), this, SLOT (slotQuit ()));
  
  QAction *filePrefsAction= new QAction (tr ("&Prefs"), this);
  filePrefsAction->setWhatsThis (tr ("Set Preferences"));
  connect (filePrefsAction, SIGNAL (triggered ()), this, SLOT (slotPrefs ()));
  
  
  QAction *fileOpen= new QAction (tr ("&Open"), this);
  fileOpen->setWhatsThis (tr ("Open Waveform File"));
  connect (fileOpen, SIGNAL (triggered ()), this, SLOT (slotOpen()));
  
  QAction *fileSave= new QAction (tr ("&Save"), this);
  fileSave->setWhatsThis (tr ("Save Waveform File"));
  connect (fileSave, SIGNAL (triggered ()), this, SLOT (slotSave()));

  
  QMenu *fileMenu = menuBar ()->addMenu (tr ("&File"));
  fileMenu->addAction (filePrefsAction);
  fileMenu->addSeparator ();
  fileMenu->addAction (fileOpen);
  fileMenu->addAction (fileSave);
  fileMenu->addSeparator ();
  fileMenu->addAction (fileQuitAction);


  // 
  // We construct a View menu having the following menu choices on it:
  // Toggle Line - allows us to look at the waveforms in point or line mode
  // Intensity Meter - provides a cross section of all sensors along a bin
  // Zoom In - allows for a waveform signature zoom
  // Reset Zoom - brings us back out to the original waveform
  // All connections for these menu items are set up here.
  //

  QString label = tr ("Toggle Current Channel Display (%1)").arg (QString (ac[11]));
  QAction *currentChanAct = new QAction (label, this);
  currentChanAct->setWhatsThis (currentChanText);
  connect (currentChanAct, SIGNAL (triggered()), this, SLOT (slotChannel ()));
  
  QAction * viewLineAct = new QAction (tr ("Toggle &Line"), this);
  viewLineAct->setWhatsThis (tr ("Toggle the waveform display between line and dot mode"));
  connect (viewLineAct, SIGNAL (triggered()), this, SLOT (slotMode ()));
  
  QAction * viewIntMeterAct = new QAction (tr ("Toggle &Intensity Meter"), this);
  viewIntMeterAct->setWhatsThis (tr ("Toggle the intensity meter"));
  connect (viewIntMeterAct, SIGNAL (triggered()), this, SLOT (slotIntMeter ()));
  
  QAction * viewInterestAct = new QAction (tr ("Toggle Interest &Point"), this);
  viewInterestAct->setWhatsThis (tr ("Toggle the display of the interest points on/off"));
  connect (viewInterestAct, SIGNAL (triggered()), this, SLOT (slotInterest ()));
  
  
  QActionGroup *chanGrp = new QActionGroup (this);
  connect (chanGrp, SIGNAL (triggered (QAction *)), this, SLOT (slotPaneToggle (QAction *)));
  chanGrp->setExclusive (false);


  // deep pane mapping

  label = tr ("Deep Channel Pane (%1)").arg (QString (ac[1]));
  deepChanAct = new QAction (label, this);
  deepChanAct->setWhatsThis (deepChanText);
  deepChanAct->setCheckable (true);
  deepChanAct->setChecked (savedSettings.visible[0]);
  chanGrp->addAction (deepChanAct);

  
  // shallow 1 pane mapping
  
  label = tr ("Shallow Channel 1 Pane (%1)").arg (QString (ac[2]));
  shal1ChanAct = new QAction (label, this);
  shal1ChanAct->setWhatsThis (shal1ChanText);
  shal1ChanAct->setCheckable (true);
  shal1ChanAct->setChecked (savedSettings.visible[1]);
  chanGrp->addAction (shal1ChanAct);


  // shallow2 pane mapping
  
  label = tr ("Shallow Channel 2 Pane (%1)").arg (QString (ac[3]));
  shal2ChanAct = new QAction (label, this);
  shal2ChanAct->setWhatsThis (shal2ChanText);
  shal2ChanAct->setCheckable (true);
  shal2ChanAct->setChecked (savedSettings.visible[2]);
  chanGrp->addAction (shal2ChanAct);
  
  
  // shallow3 pane mapping
  
  label = tr ("Shallow Channel 3 Pane (%1)").arg (QString (ac[4]));
  shal3ChanAct = new QAction (label, this);
  shal3ChanAct->setWhatsThis (shal3ChanText);
  shal3ChanAct->setCheckable (true);
  shal3ChanAct->setChecked (savedSettings.visible[3]);
  chanGrp->addAction (shal3ChanAct);
  
  
  // shallow4 pane mapping
  
  label = tr ("Shallow Channel 4 Pane (%1)").arg (QString (ac[5]));
  shal4ChanAct = new QAction (label, this);
  shal4ChanAct->setWhatsThis (shal4ChanText);
  shal4ChanAct->setCheckable (true);
  shal4ChanAct->setChecked (savedSettings.visible[4]);
  chanGrp->addAction (shal4ChanAct);
  
  
  // shallow5 pane mapping
  
  label = tr ("Shallow Channel 5 Pane (%1)").arg (QString (ac[6]));
  shal5ChanAct = new QAction (label, this);
  shal5ChanAct->setWhatsThis (shal5ChanText);
  shal5ChanAct->setCheckable (true);
  shal5ChanAct->setChecked (savedSettings.visible[5]);
  chanGrp->addAction (shal5ChanAct);
  
  
  // shallow6 pane mapping
  
  label = tr ("Shallow Channel 6 Pane (%1)").arg (QString (ac[7]));
  shal6ChanAct = new QAction (label, this);
  shal6ChanAct->setWhatsThis (shal6ChanText);
  shal6ChanAct->setCheckable (true);
  shal6ChanAct->setChecked (savedSettings.visible[6]);
  chanGrp->addAction (shal6ChanAct);
  
  
  // shallow7 pane mapping
  
  label = tr ("Shallow Channel 7 Pane (%1)").arg (QString (ac[8]));
  shal7ChanAct = new QAction (label, this);
  shal7ChanAct->setWhatsThis (shal7ChanText);
  shal7ChanAct->setCheckable (true);
  shal7ChanAct->setChecked (savedSettings.visible[7]);
  chanGrp->addAction (shal7ChanAct);
  
  
  // ir pane mapping
  
  label = tr ("IR Channel Pane (%1)").arg (QString (ac[9]));
  irChanAct = new QAction (label, this);
  irChanAct->setWhatsThis (irChanText);
  irChanAct->setCheckable (true);
  irChanAct->setChecked (savedSettings.visible[8]);
  chanGrp->addAction (irChanAct);
  

  // T0 pane mapping
  
  label = tr ("T0 Channel Pane (%1)").arg (QString (ac[10]));
  t0ChanAct = new QAction (label, this);
  t0ChanAct->setWhatsThis (t0ChanText);
  t0ChanAct->setCheckable (true);
  t0ChanAct->setChecked (savedSettings.visible[9]);
  chanGrp->addAction (t0ChanAct);
  
  
  QAction * viewZoomInAct = new QAction (tr ("&Zoom In"), this);
  viewZoomInAct->setWhatsThis (tr ("Zoom in on a waveform"));
  connect (viewZoomInAct, SIGNAL (triggered ()), this, SLOT (slotZoomIn ()));
  
  QAction * viewResetZoomAct = new QAction (tr ("&Reset Zoom"), this);
  viewResetZoomAct->setWhatsThis (tr ("Reset all zooms"));
  connect (viewResetZoomAct, SIGNAL (triggered ()), this, SLOT (slotResetZoom ()));
  
  
  QMenu * viewMenu = menuBar()->addMenu (tr ("&View"));
  viewMenu->addAction (currentChanAct);
  viewMenu->addAction (viewLineAct);
  viewMenu->addAction (viewIntMeterAct);
  viewMenu->addAction (viewInterestAct);
  viewMenu->addSeparator();

  
  // Add the pane display toggle menu items
  
  QMenu * togglePaneMenu = new QMenu (tr ("Pane Visibility"));

  
  togglePaneMenu->addAction (deepChanAct);
  togglePaneMenu->addAction (shal1ChanAct);
  togglePaneMenu->addAction (shal2ChanAct);
  togglePaneMenu->addAction (shal3ChanAct);
  togglePaneMenu->addAction (shal4ChanAct);
  togglePaneMenu->addAction (shal5ChanAct);
  togglePaneMenu->addAction (shal6ChanAct);
  togglePaneMenu->addAction (shal7ChanAct);
  togglePaneMenu->addAction (irChanAct);
  togglePaneMenu->addAction (t0ChanAct);
  
  viewMenu->addMenu (togglePaneMenu);
  
  viewMenu->addSeparator();
  
  viewMenu->addAction (viewZoomInAct);
  viewMenu->addAction (viewResetZoomAct);
  
  
  // 
  // Here, we set up the Multi-Wave menu which has the modes for 
  // looking at the waveforms.  These options include:
  // Nearest Neighbor - shows the MAX_STACK_POINTS - 1 nearest waveforms
  // Single Waveform - only shows what's under the cursor in pfmEdit
  //
  
  nearAct = new QAction (tr ("Nearest Neighbors"), this);
  nearAct->setWhatsThis (tr ("Shows the eight closest waveforms"));
  
  nearAct->setCheckable (true);
  
  connect (nearAct, SIGNAL (triggered()), this, SLOT (slotNear()));
  
  noneAct = new QAction (tr ("Single Waveform Mode"), this);
  noneAct->setWhatsThis (tr ("Only shows the targeted waveform"));
  noneAct->setCheckable (true);
  connect (noneAct, SIGNAL (triggered()), this, SLOT (slotNone()));
  
  QActionGroup * multiWaveGrp = new QActionGroup (this);
  
  multiWaveGrp->addAction (nearAct);
  multiWaveGrp->addAction (noneAct);
  
  if (multiWaveMode == 1)
    {
      noneAct->setChecked (true);
    }
  else
    {          
      nearAct->setChecked (true);
    }
  
  QMenu *multiMenu = menuBar ()->addMenu (tr ("&Multi-Wave"));
  multiMenu->addAction (nearAct);
  multiMenu->addAction (noneAct);

  
  // Create the Tools menu and color coding submenu.  On the submenu are the option to color
  // code by cursor or flightline.  The options on the submenu are part of an action group meaning
  // they are mutually exclusive.
  
  QMenu * toolsMenu = menuBar()->addMenu (tr ("&Tools"));
  
  QMenu * colorCodingMenu = new QMenu (tr ("Color Coding"));
  
  QAction * ccByCursor = new QAction (tr ("By Cursor"), this);
  ccByCursor->setCheckable (true);
  ccByCursor->setWhatsThis (tr ("Color codes waveforms based on pfmEdit's cursor colors"));
  connect (ccByCursor, SIGNAL (triggered()), this, SLOT (slotColorByCursor()));
  
  QAction * ccByFlightline = new QAction (tr ("By Flightline"), this);
  ccByFlightline->setCheckable (true);
  ccByFlightline->setWhatsThis (tr ("Color codes waveforms based on flighline color"));
  connect (ccByFlightline, SIGNAL (triggered()), this, SLOT (slotColorByFlightline()));
  
  QActionGroup * ccGroup = new QActionGroup (this);
  ccGroup->addAction (ccByCursor);
  ccGroup->addAction (ccByFlightline);
  
  ccMode ? ccByFlightline->setChecked (true) : ccByCursor->setChecked (true);
  
  colorCodingMenu->addAction (ccByCursor);
  colorCodingMenu->addAction (ccByFlightline);
  
  toolsMenu->addMenu (colorCodingMenu);

  
  // place the Reference Marks menu under the Tools menu option
  
  QMenu * referenceMenu = new QMenu (tr ("Reference Waveforms"));
  
  toggleReference = new QAction (tr ("Toggle Overlay Display"), this);
  toggleReference->setCheckable (true);
  toggleReference->setChecked (false);
  toggleReference->setWhatsThis (tr ("Shows/hides the reference waveforms of an opened .wfm file"));
  toggleReference->setEnabled (false);
  connect (toggleReference, SIGNAL (toggled (bool)), this, SLOT (slotToggleReference (bool)));
  
  clearReference = new QAction (tr ("Clear Reference Marks"), this);
  clearReference->setWhatsThis (tr ("Clears the reference waveforms from memory"));
  clearReference->setEnabled (false);
  connect (clearReference, SIGNAL (triggered ()), this, SLOT (slotClearReference ()));
  
  referenceMenu->addAction (toggleReference);
  referenceMenu->addAction (clearReference);
  
  toolsMenu->addMenu (referenceMenu);
  

  //  Setup the help menu.  I like leaving the About Qt in since this is
  //  a really nice package - and it's open source.
  
  QAction *aboutAct = new QAction (tr ("&About"), this);
  aboutAct->setShortcut (tr ("Ctrl+A", "About shortcut key"));
  aboutAct->setWhatsThis (tr ("Information about CZMILwaveMonitor"));
  connect (aboutAct, SIGNAL (triggered ()), this, SLOT (about ()));
  
  QAction *aboutQtAct = new QAction (tr ("About&Qt"), this);
  aboutQtAct->setWhatsThis (tr ("Information about Qt"));
  connect (aboutQtAct, SIGNAL (triggered ()), this, SLOT (aboutQt ()));
  
  QAction *whatThisAct = QWhatsThis::createAction (this);
  
  QMenu *helpMenu = menuBar ()->addMenu (tr ("&Help"));
  helpMenu->addAction (aboutAct);
  helpMenu->addSeparator ();
  helpMenu->addAction (aboutQtAct);
  helpMenu->addAction (whatThisAct);
  
  for (int32_t i = 0 ; i < NUM_WAVEFORMS ; i++)
    {
      map[i]->setCursor (Qt::ArrowCursor);
      map[i]->enableSignals ();


      //  Start with the length the same as the time bounds, then expand it a bit.

      chanBounds[i].length = uTimeBound[i] + 1;


      //  Push the vertical axis up to 1060 so we don't squash the numbers at the top of the pane.

      chanBounds[i].min_y = 0;
      chanBounds[i].max_y = 1060;
      
      chanBounds[i].max_x = chanBounds[i].length;
      chanBounds[i].min_x = 0;
      
      chanBounds[i].range_x = chanBounds[i].max_x - chanBounds[i].min_x;
      
      //  Add 5% to the X axis.
      
      // 
      //  Here, depending on which layout of the panes we are using (vertical/
      //  horizontal), we calculate the padding before we get to the axes.
      //
      
      chanBounds[i].max_x = chanBounds[i].max_x + NINT (((float) chanBounds[i].range_x * 0.05 + 1));
      chanBounds[i].range_x = chanBounds[i].max_x - chanBounds[i].min_x;
      
      chanBounds[i].range_y = chanBounds[i].max_y - chanBounds[i].min_y;
    }

  QTimer * track = new QTimer (this);
  connect (track, SIGNAL (timeout ()), this, SLOT (trackCursor ()));
  track->start (10);


  //  If we were in Current Channel mode when we quit we want to turn it off and turn it back on again after 
  //  killing all the panes except pane 4.

  if (current_channel_mode)
    {
      current_channel_mode = NVFalse;
      slotChannel ();
    }
}



CZMILwaveMonitor::~CZMILwaveMonitor ()
{
}


// This slot is attached to the pane/channel action group and will dynamically hide/display panes

void CZMILwaveMonitor::slotPaneToggle (QAction *realID)
{
  int32_t deltaSize, windowOffset, totalCurHeight;
  int32_t row1Height, row2Height, row3Height;
  
  deltaSize = totalCurHeight = 0;

  row1Height = map[0]->size ().rheight ();

  for (int32_t i = 0 ; i < 3 ; i++)
    {
      if (map[i]->isVisible ())
        {
          row1Height = map[i]->size ().rheight ();
          break;
        }
    }

  row2Height = map[3]->size ().rheight ();

  for (int32_t i = 3 ; i < 6 ; i++)
    {
      if (map[i]->isVisible ())
        {
          row2Height = map[i]->size ().rheight ();
          break;
        }
    }

  row3Height = map[6]->size ().rheight ();

  for (int32_t i = 6 ; i < 9 ; i++)
    {
      if (map[i]->isVisible ())
        {
          row3Height = map[i]->size ().rheight ();
          break;
        }
    }

  if (map[0]->isVisible () || map[1]->isVisible () || map[2]->isVisible ()) totalCurHeight += row1Height;

  if (map[3]->isVisible () || map[4]->isVisible () || map[5]->isVisible ()) totalCurHeight += row2Height;

  if (map[6]->isVisible () || map[7]->isVisible () || map[8]->isVisible () || map[9]->isVisible ()) totalCurHeight += row3Height;

  windowOffset = this->size ().rheight () - totalCurHeight;

  if (realID == deepChanAct)
    {
      map[0]->setVisible (realID->isChecked ());
    }
  else if (realID == shal1ChanAct)
    {
      map[1]->setVisible (realID->isChecked ());
    }
  else if (realID == shal2ChanAct)
    {
      map[2]->setVisible (realID->isChecked ());
    }
  else if (realID == shal3ChanAct)
    {
      map[3]->setVisible (realID->isChecked ());
    }
  else if (realID == shal4ChanAct)
    {
      map[4]->setVisible (realID->isChecked ());
    }
  else if (realID == shal5ChanAct)
    {
      map[5]->setVisible (realID->isChecked ());
    }
  else if (realID == shal6ChanAct)
    {
      map[6]->setVisible (realID->isChecked ());
    }
  else if (realID == shal7ChanAct)
    {
      map[7]->setVisible (realID->isChecked ());
    }
  else if (realID == irChanAct)
    {
      map[8]->setVisible (realID->isChecked ());
    }
  else if (realID == t0ChanAct)
    {
      map[9]->setVisible (realID->isChecked ());
    }

  if (map[0]->isVisible () || map[1]->isVisible () || map[2]->isVisible ()) deltaSize += row1Height;

  if (map[3]->isVisible () || map[4]->isVisible () || map[5]->isVisible ()) deltaSize += row2Height;

  if (map[6]->isVisible () || map[7]->isVisible () || map[8]->isVisible () || map[9]->isVisible ()) deltaSize += row3Height;

  this->resize(this->size ().rwidth (), windowOffset + deltaSize);
}




//
// Slot:                slotToggleReference
//
// This slot will be called automagically when the user selects the "Toggle Overlay" option from
// the menu.  It will set the key flags or variables to enable the reference waveforms to be 
// shown or hidden within the sensor panes.
//

void CZMILwaveMonitor::slotToggleReference (bool toggleStatus)
{
  drawReference = toggleStatus;
  
  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) 
    {
      if (map[i]->isVisible()) map[i]->redrawMapArea (NVTrue);
    }
}

//
// Slot:                slotClearReference 
//
// This slot will set any needed flags to halt the drawing of the reference waveforms and will
// clean up any memory structures that housed the reference marks.
//

void CZMILwaveMonitor::slotClearReference ()
{        
  // uncomment this since I don't have this function over here.
  clearReferenceData ();
  
  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) 
    {
      if (map[i]->isVisible()) map[i]->redrawMapArea (NVTrue);
    }
  
  toggleReference->setChecked (false);
  toggleReference->setEnabled (false);
  clearReference->setEnabled (false);
}



// 
// slot:        slotNone
//                                        
// This slot will fire when the Single-Waveform mode is selected off of
// the menu.  We redraw the CZMILwaveMonitor module to reflect this change.
//

void CZMILwaveMonitor::slotNone ()
{
  multiWaveMode = 1;
  
  abe_share->mwShare.multiSwitch = NVTrue;
  abe_share->mwShare.multiMode = multiWaveMode;

  force_redraw = NVTrue;
}



// 
// slot:        slotNear
//                                        
// This slot will fire when the Nearest Neighbor mode is selected off of
// the menu.  We redraw the CZMILwaveMonitor module to reflect this change.
//

void CZMILwaveMonitor::slotNear ()
{
  multiWaveMode = 0;
  abe_share->mwShare.multiSwitch = NVTrue;
  abe_share->mwShare.multiMode = multiWaveMode;

  force_redraw = NVTrue;
}



// There are two slots defined here that are fired when the user selects one of the options off
// of the Color Coding submenu on the Tools menu.
//

void CZMILwaveMonitor::slotColorByCursor (void)
{
  ccMode = CC_BY_CURSOR;
  nonFileIO_force_redraw = NVTrue;
}


void CZMILwaveMonitor::slotColorByFlightline (void)
{
  ccMode = CC_BY_FLIGHTLINE;
  nonFileIO_force_redraw = NVTrue;
}


//
// event:        closeEvent
//
// This event will be called when the CZMILwaveMonitor module is closed using the stupid
// little X button.
//

void CZMILwaveMonitor::closeEvent (QCloseEvent * event __attribute__ ((unused)))
{
  slotQuit ();
}


//
// method:        GetLabelSpacing
//
// This function will create fonts that are to be used in the different
// sensor panes to denote the tick marks and legend labels.  We calculate
// the width and height of a Charter font of points 12 and 8.  We also calculate
// the amount of space in pixels to get to either the Time or Count axis.
// Lastly, we calculate the width of the text "Time (ns)" and "Counts".  All
// of these values will be used at a later time to build the individual
// panes.
//

void
CZMILwaveMonitor::GetLabelSpacing (void)
{
  QFont font ("Charter", 12);
  QFont font2 ("Charter", 8);
  
  QFontMetrics fm (font);
  QFontMetrics fm2 (font2);
  
  pt12Height = fm.height ();
  pt8Height = fm2.height ();
  
  verLblSpace = LBL_PIX_SPACING + pt12Height + LBL_PIX_SPACING + pt8Height + LBL_PIX_SPACING;
  
  pt12Width = fm.width ("X");
  gridWidth = fm2.width ("1024");
  
  horLblSpace = LBL_PIX_SPACING + pt12Width + LBL_PIX_SPACING + gridWidth + LBL_PIX_SPACING;
  
  timeWidth = fm.width (tr ("Time (ns)", "Make sure that this is the same translation as all other translations of this string"));
  countsWidth = fm.width (tr ("Counts", "Make sure that this is the same translation as all other translations of this string"));
}




void 
CZMILwaveMonitor::slotResize (QResizeEvent *e __attribute__ ((unused)))
{
  //
  // Here, we find the offsets pixelwise as to where the beginning and
  // end of our Time axis is.  These values get placed in sigBegin and
  // in sigEnd.
  //

  // here, we grap the sending map and store the new begin/end axis position in pixels for each
  // sensor map

  nvMap * sendingMap = (nvMap *) sender ();

  int32_t mapType = -1;


  // we always want to get the axisPixelBegin/End combo based on the full extent of the axis no
  // matter if we are zooming or not.  We are only concerned with the pixel locations and they
  // must be consistent.  Therefore, we don't play off of the current lTimeBound/uTimeBound arrays,
  // we play off of a nonzoomed axis.  

  int32_t lCount, lTime, uTime;


  // change lTime to 1 for proper scaling.  there is no time bin of 0.

  lTime = LOWER_TIME_BOUND;
  lCount = LOWER_COUNT_BOUND;

  mapType = GetSensorMapType (sendingMap);

  uTime = uTimeBound[mapType];


  int32_t dummy;
  scaleWave (lTime, lCount, &(axisPixelBegin[mapType]), &dummy, mapType, sendingMap->getMapdef());
  scaleWave (uTime, lCount, &(axisPixelEnd[mapType]), &dummy, mapType, sendingMap->getMapdef());


  // Because when we resize we can't be in another window (that's an assumption), we can call
  // the panes redrawMapArea  

  sendingMap->redrawMapArea (NVTrue);
}



void
CZMILwaveMonitor::slotHelp ()
{
  QWhatsThis::enterWhatsThisMode ();
}



//
// method:        leftMouse
//
// This method is called when the left mouse button is clicked.  This
// function handles the actual zooming protocol.
//

void 
CZMILwaveMonitor::leftMouse (int32_t x, int32_t y, nvMap *l_map)
{
  int32_t             *px, *py;
  double              *mx, *my;
  NV_I32_COORD2       box[2];


  // if we are zooming
  
  if (zoomFlag)
    {
      if (zoomIsActive)
        {
          // we make sure we're in the proper pane
          
          if (l_map == zoomMap)
            {
              l_map->closeRubberbandRectangle (zoom_rectangle, x, y, &px, &py, &mx, &my);

              box[0].x = 999999;
              box[0].y = 999999;
              box[1].x = -999999;
              box[1].y = -999999;
              for (int32_t i = 0 ; i < 4 ; i++)
                {
                  if (px[i] < box[0].x) box[0].x = px[i];
                  if (py[i] < box[0].y) box[0].y = py[i];
                  if (px[i] > box[1].x) box[1].x = px[i];
                  if (py[i] > box[1].y) box[1].y = py[i];
                }


              NVMAP_DEF l_mapdef = l_map->getMapdef ();


              //  Make sure we're inside of the axes.

              if (box[0].x < horLblSpace) box[0].x = horLblSpace;
              if (box[1].y > (l_mapdef.draw_height - verLblSpace)) box[1].y = l_mapdef.draw_height - verLblSpace;


              //  Determine which map is passed in and get its length for the Time axis.
  
              int32_t mi = GetSensorMapType (l_map);

  
              int32_t timeLegendBegin, timeLegendEnd, countLegendBegin, countLegendEnd, zoomLength, timeBin, timeBin2, timeBin3, timeBin4 = 0;

              unScaleWave (box[0].x, box[0].y, &timeBin, &timeBin4, mi, l_mapdef);
              unScaleWave (box[1].x, box[1].y, &timeBin2, &timeBin3, mi, l_mapdef);
  

              zoomLength = chanBounds[mi].length;
  

              // get the corner points of the selection box
  
              scaleWave (LOWER_TIME_BOUND, LOWER_COUNT_BOUND, &timeLegendBegin, &countLegendBegin, mi, l_map->getMapdef ());
  
              scaleWave (zoomLength-1, UPPER_COUNT_BOUND, &timeLegendEnd, &countLegendEnd, mi, l_map->getMapdef());


              // based on ratio of selection box, calculate new extents from current extents stored
              // in lTimeBound, uTimeBound, lCountBound, and uCountBound
  
              int32_t timeTemp = lTimeBound[mi], countTemp = lCountBound[mi];
  
              int32_t lTime = (int32_t) (timeTemp + (uTimeBound[mi] -timeTemp + 1) * ((timeBin - 1) / (float) (zoomLength - 1))); 
              int32_t uTime = (int32_t) (timeTemp + (uTimeBound[mi] -timeTemp + 1) * ((timeBin2- 1) / (float) (zoomLength - 1)));
  
              int32_t lCount = (int32_t) (countTemp + (uCountBound[mi] - countTemp + 1) * (timeBin3 / (float) (UPPER_COUNT_BOUND)));
  
              int32_t uCount = (int32_t) (countTemp + (uCountBound[mi] - countTemp + 1) * (timeBin4 / (float) (UPPER_COUNT_BOUND)));
  
              if (((uCount - lCount) > 1) && ((uTime - lTime) > 1))
                {
                  lTimeBound[mi] = lTime;
                  uTimeBound[mi] = uTime;
                  lCountBound[mi] = lCount;
                  uCountBound[mi] = uCount;
                }


              l_map->discardRubberbandRectangle (&zoom_rectangle);

              zoomFlag = zoomIsActive = NVFalse;
 
              for (int32_t i = 0 ; i < NUM_WAVEFORMS ; i++) map[i]->setCursor (Qt::ArrowCursor);

              force_redraw = NVTrue;
            }
        }
      else
        {
          // store the map we are in

          zoomMap = l_map;
          zoomIsActive = NVTrue;
          zoomMap->anchorRubberbandRectangle (&zoom_rectangle, x, y, Qt::white, 2, Qt::SolidLine); 
        }
    }
}


void 
CZMILwaveMonitor::midMouse (double x __attribute__ ((unused)), double y __attribute__ ((unused)))
{
  // if we are zooming

  if (zoomFlag)
    {
      // if we have set our first pivot point, then we are finishing the zoom selection
      
      if (zoomIsActive)
        {
          zoomMap->discardRubberbandRectangle (&zoom_rectangle);
          zoomIsActive = NVFalse;
          zoomFlag = NVFalse;
          for (int32_t i = 0 ; i < NUM_WAVEFORMS ; i++) map[i]->setCursor (Qt::ArrowCursor);
        }
    }
}




// modified right click to store current zooming pane

void CZMILwaveMonitor::rightMouse (int32_t x, int32_t y, nvMap * l_map)
{
  
  if (!zoomIsActive)
    {
      contextMenu->move(x,y);
      contextMenu->show();
      zoomOutMap = l_map;
    }
}



void 
CZMILwaveMonitor::slotMousePress (QMouseEvent * e __attribute__ ((unused)), double x __attribute__ ((unused)), double y __attribute__ ((unused)))
{
  nvMap *l_map = (nvMap *) sender ();
  
  if (e->button () == Qt::LeftButton) leftMouse (e->pos().x(), e->pos().y(), l_map);
  if (e->button () == Qt::MidButton) midMouse (x, y);
  if (e->button () == Qt::RightButton) rightMouse (e->globalX(), e->globalY(), l_map);
}



//
// Method:                                GetSensorMapType
//
// This method will ingest a map in question and compare it against the sensor array and
// return the sensor type as an integer.
//

int32_t CZMILwaveMonitor::GetSensorMapType (nvMap *mapInQuestion)
{
  if (mapInQuestion == map[0]) return CZMIL_DEEP_CHANNEL;
  if (mapInQuestion == map[1]) return CZMIL_SHALLOW_CHANNEL_1;
  if (mapInQuestion == map[2]) return CZMIL_SHALLOW_CHANNEL_2;
  if (mapInQuestion == map[3]) return CZMIL_SHALLOW_CHANNEL_3;
  if (mapInQuestion == map[4]) return CZMIL_SHALLOW_CHANNEL_4;
  if (mapInQuestion == map[5]) return CZMIL_SHALLOW_CHANNEL_5;
  if (mapInQuestion == map[6]) return CZMIL_SHALLOW_CHANNEL_6;
  if (mapInQuestion == map[7]) return CZMIL_SHALLOW_CHANNEL_7;
  if (mapInQuestion == map[8]) return CZMIL_IR_CHANNEL;
  return 9;
}



//
// Method:                                GetMapVisibilityButtonState
//

uint8_t CZMILwaveMonitor::GetMapVisibilityButtonState (int32_t mapIndex)
{
  if (mapIndex == 0) return deepChanAct->isChecked ();
  if (mapIndex == 1) return shal1ChanAct->isChecked ();
  if (mapIndex == 2) return shal2ChanAct->isChecked ();
  if (mapIndex == 3) return shal3ChanAct->isChecked ();
  if (mapIndex == 4) return shal4ChanAct->isChecked ();
  if (mapIndex == 5) return shal5ChanAct->isChecked ();
  if (mapIndex == 6) return shal6ChanAct->isChecked ();
  if (mapIndex == 7) return shal7ChanAct->isChecked ();
  if (mapIndex == 8) return irChanAct->isChecked ();
  return t0ChanAct->isChecked ();
}



//
// slotMouseMove
//
// This function will be called when a mouse is moved across a map pane.
// We also may be showing the intensity meter so we capture which time 
// bin we are on based on the difference between the beginning of the axis
// and the pixel we are currently on.
//
void
CZMILwaveMonitor::slotMouseMove (QMouseEvent *e, double x, double y)
{
  //  Let other ABE programs know which window we're in.
  
  abe_share->active_window_id = active_window_id;
  
  
  if (zoomFlag && zoomIsActive)
    {
      zoomMap->dragRubberbandRectangle (zoom_rectangle, x, y);
    }
  else
    {
      if (showIntMeter)
        {
          // grab the sending map, store the mapType as an index and the length so we don't have
          // to duplicate code.  get the binHighlighted value.  This value represents a location
          // on the time axis which is sensor dependent.  For example on the T0 axis this value
          // could be within the range of 0-64.

          nvMap *sendingMap = (nvMap *) (sender ());

          int32_t mapType, length = 0;


          // We get the sensor type, store the length respectively and block all other maps with
          // the exception of the sendingMap.  Thus, we will process only sendingMap in trackCursor
          //

          mapType = GetSensorMapType (sendingMap);

          length = chanBounds[mapType].length;

          binHighlighted[mapType] = NINT ((e->x() - axisPixelBegin[mapType]) / (float)(axisPixelEnd[mapType] - axisPixelBegin[mapType]) * (length - 2)) + 1;


          // block all maps except the sending map, take the nonFileIO route in trackCursor

          for (int32_t i = 0; i < NUM_WAVEFORMS; i++) mapBlocked[i] = NVTrue;

          mapBlocked[mapType] = NVFalse;

          nonFileIO_force_redraw = NVTrue;
        }  
    }
}



//
// method:        drawMultiGrid
//
// This method will print the intensity information for all multiple
// waveforms in a grid-like pattern in the top right of the screen.
//

void CZMILwaveMonitor::drawMultiGrid(CZMIL_CWF_Data multiWave[], NVMAP_DEF l_mapdef, nvMap * l_map)
{ 
  
  int32_t colStart = l_mapdef.draw_width - 110;
  int32_t rowStart = 60;
  QColor tempColor;
  QString tempString;
  int32_t lTime, uTime;

  
  // reduced code down to for loop and defaulted mapIndex to 0
  
  int32_t mapIndex = 0;
  
  for (int32_t i = 1; i < NUM_WAVEFORMS; i++ ) 
    {    
      if ( l_map == map[i] )
        { 
          mapIndex = i;
          break;
        }
    }
  
  lTime = lTimeBound[mapIndex];
  uTime = uTimeBound[mapIndex];


  for (int32_t i = 0 ; i < 3 ; i++)
    {
      for (int32_t j = 0 ; j < 3 ; j++)
        {
          if (abe_share->mwShare.multiPresent[j + (i * 3)] != -1)
            {
              tempColor.setRgb (abe_share->mwShare.multiColors[j + (i * 3)].r,abe_share->mwShare.multiColors[j + (i * 3)].g, abe_share->mwShare.multiColors[j + (i * 3)].b);
              tempColor.setAlpha (abe_share->mwShare.multiColors[j + (i * 3)].a);
              
              if (scaledBinHighlighted[mapIndex] <=uTime && scaledBinHighlighted[mapIndex] >=lTime)
                {
                  uint16_t * wData;
                  uint8_t * ndx;
                  
                  getWaveformData (mapIndex, wData, ndx, &(multiWave[j + (i * 3)]));
                  
                  tempString = QString ("%1").arg (wData[scaledBinHighlighted[mapIndex]]);
                }
              else
                {
                  tempString = tr ("N/A", "Abbreviation for Not Applicable");
                }
              
              l_map->drawText(tempString, colStart + (j*35), rowStart + (i*20), tempColor, NVTrue);
            }
        }
    } 
}




// function revamped to account for reference lines
//
// method:        drawMultiWaves
//
// This method draws mulitple waveforms.  This code was "borrowed" from
// Jan's function that draws a single waveform.  We'll go in deeper within
// the function.
//

void CZMILwaveMonitor::drawMultiWaves (nvMap *l_map, NVMAP_DEF l_mapdef, int32_t mapIndex)
{
  // we allocate space for czmil information for the
  // maximum number of waveforms
  
  uint16_t                *dat[MAX_STACK_POINTS];
  uint8_t                 *ndx[MAX_STACK_POINTS];
  static int32_t          save_rec_multi[MAX_STACK_POINTS], save_chan_multi[MAX_STACK_POINTS], save_ret_multi[MAX_STACK_POINTS];
  static CZMIL_CWF_Data   save_cwf_multi[MAX_STACK_POINTS];
  static CZMIL_CPF_Data   save_cpf_multi[MAX_STACK_POINTS];
  int32_t                 pix_x[2], pix_y[2];
  QColor                  curWaveColorNear, curPrimaryColorNear, curSecondaryColorNear, tempColor;
  int32_t                 wave_type;
  int32_t                 lTime, uTime, lCount, uCount, length;
  int32_t                 scaledTimeValue, scaledCountValue;
  uint8_t                 currentValid = NVFalse;
  uint8_t                 previousValid = NVFalse;
  
  
  // Determine what type of waveform is passed in.  we store the current boundaries
  // of the axes in lTime, uTime, lCount, uCount.  We could be in a zoomed state. We
  // also must store the length of the time bin in length since it varies based on 
  // sensor.
  
  int32_t lengths[NUM_WAVEFORMS];
  
  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) lengths[i] = chanBounds[i].length;
  
  wave_type = GetSensorMapType (l_map);
  length = lengths[wave_type];
  lTime = lTimeBound[wave_type];
  uTime = uTimeBound[wave_type];
  lCount = lCountBound[wave_type];
  uCount = uCountBound[wave_type];
  
  // we loop through the multiple waveforms and for any that exist, we store the sensor
  // information into save_cwf_multi.  This information has been already processed in
  // processMultiWaves.  The czmil record, record number, and line information are also
  // stored.
  
  lock_track = NVTrue;
  for (int32_t i = 0 ; i < MAX_STACK_POINTS ; i++)
    {
      /*                
        changed next line check from multiPresent to multiLocation == -1 to indicate no data
      */
      if (abe_share->mwShare.multiLocation[i] != -1)
        {
          //  Because the trackCursor function may be changing the data while we're still plotting it we save it
          //  to this static structure;
          
          save_cwf_multi[i] = cwf_record_multi[i];


          //  Check for T0

          if (wave_type != 9) getWaveformData (wave_type, dat[i], ndx[i], &(save_cwf_multi[i]));
          
          save_cpf_multi[i] = cpf_record_multi[i];
          save_rec_multi[i] = recnum_multi[i];
          save_chan_multi[i] = chan_multi[i];
          save_ret_multi[i] = ret_multi[i];
        }
      else
        {
          save_rec_multi[i] = -1;
        }
    }
  lock_track = NVFalse;
  
  
  // if we have the intensity meter turned on, show the stats
  
  if (showIntMeter) drawMultiGrid (save_cwf_multi, l_mapdef, l_map);
  
  //  Loop through the multiple waveforms skipping slots that are empty.
  //  Loop backwards so that the point of interest is plotted on top.
  
  for (int32_t i = MAX_STACK_POINTS - 1 ; i >= 0 ; i--)
    {
      // if we have a valid record number
      
      if (save_rec_multi[i] != -1)
        {                                        
          // set tempColor equal to whatever respective cursor in pfmEdit is or set color equal to
          // flightline color
          
          if (ccMode == CC_BY_CURSOR) 
            {
              tempColor.setRgb (abe_share->mwShare.multiColors[i].r, abe_share->mwShare.multiColors[i].g, abe_share->mwShare.multiColors[i].b);
            }
          else
            {
              tempColor = colorArray[abe_share->mwShare.multiFlightlineIndex[i]];
            }
          
          tempColor.setAlpha (abe_share->mwShare.multiColors[i].a);
          
          curWaveColorNear = tempColor;
          
          curPrimaryColorNear = transPrimaryColorNear;
          curSecondaryColorNear = transSecondaryColorNear;
          
          if (cwf_read_multi[i] == CZMIL_SUCCESS)
            {
              //  T0

              if (wave_type == 9)
                {
                  // Here, we loop through the time axis and if the intensity values for the
                  // time axis fall within the lCount-uCount range, we find the point projection
                  // so we can later plot it.  We are also keeping track with the streak of valid
                  // points since in a zoomed state there could be breaks in the line segments
                  // as values fall in and out of the area.
                
                  for (int32_t j = 0 ; j < 64 ; j++)
                    {
                      if ((save_cwf_multi[i].T0[j] >= lCount) && (save_cwf_multi[i].T0[j] <= uCount) && (j >= lTime) && (j <= uTime)) 
                        {
                          currentValid = NVTrue;

                          scaledTimeValue = (int32_t)(((j - lTime) / ((float)(uTime - lTime)) * (length - 2)) + 1);

                          scaledCountValue = (int32_t)(( save_cwf_multi[i].T0[j] - lCount) / ((float)(uCount - lCount)) * UPPER_COUNT_BOUND);

                          scaleWave (scaledTimeValue, scaledCountValue, &pix_x[1], &pix_y[1], wave_type, l_mapdef);
                        }
                      else
                        {
                          currentValid = NVFalse;
                        }
                    
                    
                      // either make a line or draw a dot.  we will only draw a line if the
                      // previous point was valid or within the current area
                    
                      if (wave_line_mode)
                        {
                          if (currentValid && previousValid)
                            {
                              l_map->drawLine (pix_x[0], pix_y[0], pix_x[1], pix_y[1], curWaveColorNear, 2, NVFalse, Qt::SolidLine);
                            }
                        }
                      else
                        {
                          if (currentValid) l_map->fillRectangle (pix_x[0], pix_y[0], SPOT_SIZE, SPOT_SIZE, curWaveColorNear, NVFalse);
                        }
                      
                      pix_x[0] = pix_x[1];
                      pix_y[0] = pix_y[1];
                      previousValid = currentValid;
                    }
                }
              else
                {
                  int32_t ndxStart, ndxEnd;
              
                  ndxStart = (lTime - 1) / 64;
                  ndxEnd = (uTime - 1) / 64;
              
                  trimWaveformNdxs (ndx[i], ndxStart, ndxEnd);
              
                  currentValid = previousValid = NVFalse;
              
                  for (int32_t x = ndxStart; x <= ndxEnd; x++)
                    {
                      if (!ndx[x])
                        {
                          previousValid = NVFalse;
                          continue;
                        }
                

                      // Here, we loop through the time axis and if the intensity values for the
                      // time axis fall within the lCount-uCount range, we find the point projection
                      // so we can later plot it.  We are also keeping track with the streak of valid
                      // points since in a zoomed state there could be breaks in the line segments
                      // as values fall in and out of the area.
                
                      for (int32_t j = x * 64 ; j < (x * 64 + 64) ; j++)
                        {
                          if ((dat[i][j] >= lCount) && (dat[i][j] <= uCount) && (j >= lTime) && (j <= uTime)) 
                            {
                              currentValid = NVTrue;
                          
                              scaledTimeValue = (int32_t)(((j - lTime) / ((float)(uTime - lTime)) * (length - 2)) + 1);

                              scaledCountValue = (int32_t)(( dat[i][j] - lCount) / ((float)(uCount - lCount)) * UPPER_COUNT_BOUND);
                        
                              scaleWave (scaledTimeValue, scaledCountValue, &pix_x[1], &pix_y[1], wave_type, l_mapdef);
                            }
                          else
                            {
                              currentValid = NVFalse;
                            }
                    
                    
                          // either make a line or draw a dot.  we will only draw a line if the
                          // previous point was valid or within the current area
                    
                          if (wave_line_mode)
                            {
                              if (currentValid && previousValid)
                                {
                                  l_map->drawLine (pix_x[0], pix_y[0], pix_x[1], pix_y[1], curWaveColorNear, 2, NVFalse, Qt::SolidLine);
                                }
                            }
                          else
                            {
                              if (currentValid) l_map->fillRectangle (pix_x[0], pix_y[0], SPOT_SIZE, SPOT_SIZE, curWaveColorNear, NVFalse);
                            }
                      
                          pix_x[0] = pix_x[1];
                          pix_y[0] = pix_y[1];
                          previousValid = currentValid;
                        }
                    }
                }
            } 


          if (interest_point_mode)
            {
              if (wave_type != 9)
                {
                  if (save_cpf_multi[i].channel[save_chan_multi[i]][save_ret_multi[i]].interest_point != 0.0 && mapIndex == save_chan_multi[i])
                    {
                      int32_t prev_point = (int32_t) save_cpf_multi[i].channel[save_chan_multi[i]][save_ret_multi[i]].interest_point;
                      int32_t next_point = (int32_t) save_cpf_multi[i].channel[save_chan_multi[i]][save_ret_multi[i]].interest_point + 1;

                      int32_t cy = save_cwf_multi[i].channel[save_chan_multi[i]][prev_point] +
                        ((save_cwf_multi[i].channel[save_chan_multi[i]][next_point] - save_cwf_multi[i].channel[save_chan_multi[i]][prev_point]) / 2);

                      int32_t cx = save_cpf_multi[i].channel[save_chan_multi[i]][save_ret_multi[i]].interest_point;

                      scaleWave (cx, cy, &pix_x[0], &pix_y[0], save_chan_multi[i], l_mapdef);

                      drawX (pix_x[0], pix_y[0], 10, 2, Qt::green, l_map);
                    }
                }
            }
        }
    }        
  
  sim_depth = QString ("%L1").arg (save_cpf_multi[0].channel[save_chan_multi[0]][save_ret_multi[0]].elevation, 0, 'f', 2);
}



//
// method:        processMultiWaves
//
// This method will open the hof file and waveform files and load the
// data for each valid waveform.
//

void CZMILwaveMonitor::processMultiWaves (ABE_SHARE l_share)
{        
  int32_t cpf_hnd, cwf_hnd;
  
  l_abe_share = *abe_share;
  
  // loop through the waveforms, the valid ones anyways
  
  for (int32_t i = 0 ; i < MAX_STACK_POINTS ; i++)
    {
      //  Make sure this isn't an empty slot
      
      if (l_abe_share.mwShare.multiLocation[i] != -1)
        {
          cwf_read_multi[i] = 0;
          
          int16_t type;
          read_list_file (pfm_handle[l_abe_share.mwShare.multiPfm[i]], l_abe_share.mwShare.multiFile[i], cpf_path_multi[i], &type);


          /* incorporating PFM_CZMIL_DATA */
          
          if (type == PFM_CZMIL_DATA)                                        
            {
              strcpy (line_name_multi[i], read_line_file (pfm_handle[l_abe_share.mwShare.multiPfm[i]], l_abe_share.mwShare.multiLine[i]));
              
              strcpy (cwf_path_multi[i], cpf_path_multi[i]);
              sprintf (&cwf_path_multi[i][strlen (cwf_path_multi[i]) - 4], ".cwf");


              CZMIL_CPF_Header cpf_header;
              CZMIL_CWF_Header cwf_header;
              
              cpf_hnd = czmil_open_cpf_file (cpf_path_multi[i], &cpf_header, CZMIL_READONLY);
              
              if (cpf_hnd < 0)
                {
                  if (filError) filError->close ();
                  filError = new QMessageBox (QMessageBox::Warning, tr ("Open CZMIL file"), tr ("Error opening %1 : %2").arg 
                                              (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                              (Qt::WindowFlags) Qt::WA_DeleteOnClose);
                  filError->show ();
                  return;
                }                                
              
              
              cwf_hnd = czmil_open_cwf_file (cwf_path_multi[i], &cwf_header, CZMIL_READONLY);
              
              if (cwf_hnd < 0)
                {
                  if (filError) filError->close ();
                  filError = new QMessageBox (QMessageBox::Warning, tr ("Open CZMIL file"), tr ("Error opening %1 : %2").arg 
                                              (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                              (Qt::WindowFlags) Qt::WA_DeleteOnClose);
                  filError->show ();
                  return;
                }


              //  Save for slotPlotWaves.
                  
              recnum_multi[i] = l_abe_share.mwShare.multiRecord[i];
              czmil_read_cpf_record (cpf_hnd, recnum_multi[i], &(cpf_record_multi[i]));
                  
              cwf_read_multi[i] = czmil_read_cwf_record (cwf_hnd, l_abe_share.mwShare.multiRecord[i], &(cwf_record_multi[i]));

              chan_multi[i] = l_share.mwShare.multiSubrecord[i] / 100;
              ret_multi[i] = l_share.mwShare.multiSubrecord[i] % 100;

              czmil_close_cpf_file (cpf_hnd);
              czmil_close_cwf_file (cwf_hnd);
            }
        }
    }
}



//  Timer - timeout signal.  Very much like an X workproc.

void
CZMILwaveMonitor::trackCursor ()
{
  int32_t                 year, day, mday, month, hour, minute;
  float                   second;
  int32_t                 cpf_hnd, cwf_hnd;
  static uint8_t          first = NVTrue;


  //  We want to exit if we have locked the tracker to update our saved waveforms (in slotPlotWaves).

  if (lock_track) return;


  //  Since this is always a child process of something we want to exit if we see the CHILD_PROCESS_FORCE_EXIT key.
  //  We also want to exit on the ANCILLARY_FORCE_EXIT key (from pfmEdit) or if our own personal kill signal
  //  has been placeed in abe_share->key.

  if (abe_share->key == CHILD_PROCESS_FORCE_EXIT || abe_share->key == ANCILLARY_FORCE_EXIT || abe_share->key == kill_switch) slotQuit ();


  // This is a tricky little piece of work.  Instead of duplicating the code to cause an action here for keypress events that happen
  // in pfmEdit3D we just send the key event from pfmEdit3D to this app's keypress event handler.  Clever, n'est–ce pas?

  for (int32_t i = 0 ; i < 12 ; i++)
    {
      if (abe_share->key == ac[i])
        {
          abe_share->key = 0;
          abe_share->modcode = NO_ACTION_REQUIRED;

          QKeyEvent *event = new QKeyEvent (QEvent::KeyPress, ac[i], Qt::NoModifier, QString (ac[i]));
          QCoreApplication::postEvent (this, event);
          break;
        }
    }


  if (abe_share->modcode == WAVEMONITOR_FORCE_REDRAW) force_redraw = NVTrue;


  //  Check for CZMIL, no memory lock, record change, or force_redraw.

  if (((abe_share->mwShare.multiType[0] == PFM_CZMIL_DATA) && abe_share->key < INT32_MAX && l_share.nearest_point != abe_share->nearest_point) ||
      force_redraw || nonFileIO_force_redraw)
    {
      force_redraw = NVFalse;


      //  Save the record number for using +/- to move through the file.

      current_record = l_share.mwShare.multiRecord[0];


      //  If we need to read the record...

      if (!nonFileIO_force_redraw)
        {
          l_share = *abe_share;


          // 
          // if we are in nearest-neighbors mode and all slots are filled
          // we process the data for the multiple waveforms
          //

          if (multiWaveMode == 0 && abe_share->mwShare.multiNum == MAX_STACK_POINTS) processMultiWaves (l_share);


          // We do not need to open this file for the primary cursor if it is frozen

          if (abe_share->mwShare.multiPresent[0] != 0)
            {
              //  Open the CZMIL files and read the data.

              char string[512];

              strcpy (string, l_share.nearest_filename);
              db_name.sprintf ("%s", pfm_basename (l_share.nearest_filename));


              strcpy (string, l_share.nearest_filename);

              sprintf (&string[strlen (string) - 4], ".cwf");

              strcpy (cwf_path, string);

              CZMIL_CPF_Header cpf_header;
              CZMIL_CWF_Header cwf_header;

              cpf_hnd = czmil_open_cpf_file (l_share.nearest_filename, &cpf_header, CZMIL_READONLY);

              if (cpf_hnd < 0)
                {
                  if (filError) filError->close ();
                  filError = new QMessageBox (QMessageBox::Warning, tr ("Open CZMIL file"), tr ("Error opening %1 : %2").arg 
                                              (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                              (Qt::WindowFlags) Qt::WA_DeleteOnClose);
                  filError->show ();
                  return;
                }

              cwf_hnd = czmil_open_cwf_file (cwf_path, &cwf_header, CZMIL_READONLY);

              if (cwf_hnd < 0)
                {
                  if (filError) filError->close ();
                  filError = new QMessageBox (QMessageBox::Warning, tr ("Open CZMIL file"), tr ("Error opening %1 : %2").arg 
                                              (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                              (Qt::WindowFlags) Qt::WA_DeleteOnClose);
                  filError->show ();
                  return;
                }


              //  Save for slotPlotWaves.

              recnum = l_share.mwShare.multiRecord[0];

              strcpy (filename, l_share.nearest_filename);

              czmil_read_cpf_record (cpf_hnd, recnum, &cpf_record);

              /*
              for (int32_t q = 0 ; q < 9 ; q++)
                {
                  fprintf(stderr,"%s %s %d %d %d\n",NVFFL,q,cpf_record.returns[q]);

                  for (int32_t r = 0 ; r < cpf_record.returns[q] ; r++) fprintf(stderr,"%s %s %d %d %f\n",NVFFL,r,cpf_record.channel[q][r].interest_point);
                }
              */


              cwf_read = czmil_read_cwf_record (cwf_hnd, recnum, &cwf_record);


              chan = l_share.mwShare.multiSubrecord[0] / 100;
              ret = l_share.mwShare.multiSubrecord[0] % 100;


              //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

              //  Savitzky-Golay filter (just for fun).
              /*
              for (int32_t j = 0 ; j < 9 ; j++)
                {
                  int32_t ndata = cwf_record.number_of_packets[j] * 64;

                  for (int32_t k = 0 ; k < ndata ; k++) signal[k + 1] = ysave[k + 1] = (float) cwf_record.channel[j][k];

                  for (int32_t k = 1 ; k <= ndata - nright ; k++)
                    {
                      signal[k] = 0.0;
                      for (int32_t m = 1 ; m <= nleft + nright + 1 ; m++)
                        {
                          if (k + ndex[m] > 0)
                            {
                              signal[k] += coeff[m] * ysave[k + ndex[m]];
                            }
                        }
                    }

                  for (int32_t k = 0 ; k < ndata ; k++) cwf_record.channel[j][k] = NINT (signal[k + 1]);
                }
              */
              //////////////////////////////////////////////////////////////////////////////////////////////////////////////////


              czmil_close_cpf_file (cpf_hnd);
              czmil_close_cwf_file (cwf_hnd);

              czmil_cvtime (cpf_record.timestamp, &year, &day, &hour, &minute, &second);

              czmil_jday2mday(year, day, &month, &mday);
              month++;

              date_time = QString ("%1-%2-%3 (%4) %5:%6:%7").arg (year + 1900).arg (month, 2, 10, QChar ('0')).arg (mday, 2, 10, QChar ('0')).arg
                (day, 3, 10, QChar ('0')).arg (hour, 2, 10, QChar('0')).arg (minute, 2, 10, QChar('0')).arg (second, 5, 'f', 2, QChar ('0'));
              l_share.key = 0;
              abe_share->key = 0;

              abe_share->modcode = PFM_CZMIL_DATA;
            }          
        }

      nonFileIO_force_redraw = NVFalse;


      // We only want to draw the maps if they are visible

      for (int32_t i = 0; i < NUM_WAVEFORMS; i++) 
        {
          // only redraw the maps that are not blocked.  
          // but in special situations like intensity meter or zoom drawing only one will not be
          // blocked.

          if (map[i]->isVisible () && (!mapBlocked[i]))
            {
              map[i]->redrawMapArea (NVTrue);
            }
          else
            {
              mapBlocked[i] = NVFalse;
            }
        }
    }


  //  Display the startup info message the first time through.

  if (first)
    {
      QString startupMessageText = tr ("The following action keys are available in %1\n\n"
                                       "%2 = Toggle between nearest neighbor and single waveform display mode\n"
                                       "%3 = Toggle Deep on or off\n"
                                       "%4 = Toggle Shal1 on or off\n"
                                       "%5 = Toggle Shal2 on or off\n"
                                       "%6 = Toggle Shal3 on or off\n"
                                       "%7 = Toggle Shal4 on or off\n"
                                       "%8 = Toggle Shal5 on or off\n"
                                       "%9 = Toggle Shal6 on or off\n"
                                       "%10 = Toggle Shal7 on or off\n"
                                       "%11 = Toggle IR on or off\n"
                                       "%12 = Toggle T0 on or off\n"
                                       "%13 = Toggle <b>Current Channel</b> mode on or off\n"                 
                                       "You can change these key values in the %1\n"
                                       "Preferences->Ancillary Programs window\n\n\n"
                                       "You can turn off this startup message in the\n"
                                       "CZMILwaveMonitor Preferences window.").arg (parentName).arg (ac[0]).arg (ac[1]).arg (ac[2]).arg (ac[3]).arg (ac[4]).arg
        (ac[5]).arg (ac[6]).arg (ac[7]).arg (ac[8]).arg (ac[9]).arg (ac[10]).arg (ac[11]);

      if (startup_message) QMessageBox::information (this, tr ("CZMILwaveMonitor Startup Message"), startupMessageText);

      first = NVFalse;
    }
}



void
CZMILwaveMonitor::keyPressEvent (QKeyEvent *e)
{
  //  Looking for ancillary program hot keys and action keys.

  QString key = e->text ();


  //  Get the keystring.  We need this for Del, Ins, Home, or End if we want to use them for ancillary program hot keys.

  QKeySequence keySeq = e->key ();
  QString keyString = keySeq.toString ();
  if (keyString == "Del" || keyString == "Ins" || keyString == "Home" || keyString == "End") key = keyString;


  //  If key is NULL this is a modifier so we don't want to check it against the hot/action keys.

  if (key == "") return;


  //  Check for the + or - key for moving one record forward or backward in the file.

  if (key == "+" || key == "-")
    {
      //  Open the CZMIL files and read the data.

      char string[512];
      int32_t year, day, mday, month, hour, minute;
      float second;

      strcpy (string, l_share.nearest_filename);
      db_name.sprintf ("%s", pfm_basename (l_share.nearest_filename));


      strcpy (string, l_share.nearest_filename);

      sprintf (&string[strlen (string) - 4], ".cwf");

      strcpy (cwf_path, string);

      CZMIL_CPF_Header cpf_header;
      CZMIL_CWF_Header cwf_header;

      int32_t cpf_hnd = czmil_open_cpf_file (l_share.nearest_filename, &cpf_header, CZMIL_READONLY);

      if (cpf_hnd < 0)
        {
          if (filError) filError->close ();
          filError = new QMessageBox (QMessageBox::Warning, tr ("Open CZMIL file"), tr ("Error opening %1 : %2").arg 
                                      (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                      (Qt::WindowFlags) Qt::WA_DeleteOnClose);
          filError->show ();
          return;
        }

      int32_t cwf_hnd = czmil_open_cwf_file (cwf_path, &cwf_header, CZMIL_READONLY);

      if (cwf_hnd < 0)
        {
          if (filError) filError->close ();
          filError = new QMessageBox (QMessageBox::Warning, tr ("Open CZMIL file"), tr ("Error opening %1 : %2").arg 
                                      (QDir::toNativeSeparators (QString (l_share.nearest_filename))).arg (strerror (errno)), QMessageBox::NoButton, this, 
                                      (Qt::WindowFlags) Qt::WA_DeleteOnClose);
          filError->show ();
          return;
        }


      if (key == "-")
        {
          if (current_record > 0) current_record--;
        }
      else
        {
          if (current_record < cpf_header.number_of_records - 1) current_record++;
        }


      //  Set the record number in the status bar

      QString recText;
      recText.setNum (current_record);
      recordLabel->setText (recText);


      //  Save for slotPlotWaves.

      recnum = current_record;

      strcpy (filename, l_share.nearest_filename);

      czmil_read_cpf_record (cpf_hnd, recnum, &cpf_record);

      cwf_read = czmil_read_cwf_record (cwf_hnd, recnum, &cwf_record);

      chan = 0;
      ret = 0;


      czmil_close_cpf_file (cpf_hnd);
      czmil_close_cwf_file (cwf_hnd);

      czmil_cvtime (cpf_record.timestamp, &year, &day, &hour, &minute, &second);

      czmil_jday2mday(year, day, &month, &mday);
      month++;

      date_time = QString ("%1-%2-%3 (%4) %5:%6:%7").arg (year + 1900).arg (month, 2, 10, QChar ('0')).arg (mday, 2, 10, QChar ('0')).arg
        (day, 3, 10, QChar ('0')).arg (hour, 2, 10, QChar('0')).arg (minute, 2, 10, QChar('0')).arg (second, 5, 'f', 2, QChar ('0'));


      // We only want to draw the maps if they are visible

      for (int32_t i = 0; i < NUM_WAVEFORMS; i++) 
        {
          // only redraw the maps that are not blocked.  
          // but in special situations like intensity meter or zoom drawing only one will not be
          // blocked.

          if (map[i]->isVisible () && (!mapBlocked[i]))
            {
              map[i]->redrawMapArea (NVTrue);
            }
          else
            {
              mapBlocked[i] = NVFalse;
            }
        }
    }


  //  Concatenate the (single) modifier and the key value.  There's probably a better way to do this but I don't know
  //  what it is at the moment.

  QString modifier = "";
  if (e->modifiers () == Qt::AltModifier) modifier = "Alt+";
  if (e->modifiers () == Qt::ControlModifier) modifier = "Ctrl+";
  if (e->modifiers () == Qt::MetaModifier) modifier = "Meta+";


  if (!modifier.isEmpty ()) key = modifier + keyString;


  //  Check for action keys.  This is the 'n' key.

  if (key == QString (ac[0]))
    {
      if (multiWaveMode)
        {
          nearAct->setChecked (true);
          slotNear ();
        }
      else
        {
          noneAct->setChecked (true);
          slotNone ();
        }
    }


  //  This is the 'c' key.

  if (key == QString (ac[11])) slotChannel ();


  //  Number keys for toggling pane visibility.

  if (!current_channel_mode)
    {
      for (int32_t i = 1 ; i < 11 ; i++)
        {
          if (key == QString (ac[i]))
            {
              uint8_t checked = NVTrue;
              if (map[i - 1]->isVisible ()) checked = NVFalse;


              switch (i - 1)
                {
                case 0:
                  deepChanAct->setChecked (checked);
                  slotPaneToggle (deepChanAct);
                  break;

                case 1:
                  shal1ChanAct->setChecked (checked);
                  slotPaneToggle (shal1ChanAct);
                  break;

                case 2:
                  shal2ChanAct->setChecked (checked);
                  slotPaneToggle (shal2ChanAct);
                  break;

                case 3:
                  shal3ChanAct->setChecked (checked);
                  slotPaneToggle (shal3ChanAct);
                  break;

                case 4:
                  shal4ChanAct->setChecked (checked);
                  slotPaneToggle (shal4ChanAct);
                  break;

                case 5:
                  shal5ChanAct->setChecked (checked);
                  slotPaneToggle (shal5ChanAct);
                  break;

                case 6:
                  shal6ChanAct->setChecked (checked);
                  slotPaneToggle (shal6ChanAct);
                  break;

                case 7:
                  shal7ChanAct->setChecked (checked);
                  slotPaneToggle (shal7ChanAct);
                  break;

                case 8:
                  irChanAct->setChecked (checked);
                  slotPaneToggle (irChanAct);
                  break;

                case 9:
                  t0ChanAct->setChecked (checked);
                  slotPaneToggle (t0ChanAct);
                  break;
                }


              force_redraw = NVTrue;
          
              break;
            }
        }
    }

  e->accept ();
}



//  A bunch of slots.

void 
CZMILwaveMonitor::slotQuit ()
{
  envout ();
  
  //  Let pfmEdit know that we have quit.
  
  // lock the shared memory to set CZMILwaveMonitorRunning
  
  abeShare->lock ();
  
  //  Let the parent program know that we have died from something other than the kill switch from the parent.
  
  if (abe_share->key != kill_switch) abe_share->killed = kill_switch;
  
  /* Bit 1 constitutes CZMIL waveform viewer */  
  abe_share->mwShare.waveMonitorRunning &= 0xfd;
  
  abeShare->unlock ();
  
  //  Let go of the shared memory.
  
  abeShare->detach ();
  
  if ( numReferences > 0 ) clearReferenceData();
  
  exit (0);
}



void 
CZMILwaveMonitor::slotChannel ()
{
  //  Toggle the mode.

  current_channel_mode = !current_channel_mode;


  //  Switch to showing only the channel of the currently active point in pfmEdit3D.

  if (current_channel_mode)
    {
      //  Disable the Pane Visibility menu options.

      deepChanAct->setEnabled (false);
      shal1ChanAct->setEnabled (false);
      shal2ChanAct->setEnabled (false);
      shal3ChanAct->setEnabled (false);
      shal4ChanAct->setEnabled (false);
      shal5ChanAct->setEnabled (false);
      shal6ChanAct->setEnabled (false);
      shal7ChanAct->setEnabled (false);
      irChanAct->setEnabled (false);
      t0ChanAct->setEnabled (false);


      //  Turn off everything except pane 4 (shal4) where we'll be displaying the current channel data.

      for (int32_t i = 0 ; i < 10 ; i++)
        {
          if (i != 4)
            {
              map[i]->setVisible (false);
            }
          else
            {
              map[i]->setVisible (true);
            }
        }
    }


  //  Switch to showing all channels (or those that are enabled in the Pane Visibility menu) of the currently active point in pfmEdit3D.

  else
    {
      //  Enable the Pane Visibility menu options.

      deepChanAct->setEnabled (true);
      shal1ChanAct->setEnabled (true);
      shal2ChanAct->setEnabled (true);
      shal3ChanAct->setEnabled (true);
      shal4ChanAct->setEnabled (true);
      shal5ChanAct->setEnabled (true);
      shal6ChanAct->setEnabled (true);
      shal7ChanAct->setEnabled (true);
      irChanAct->setEnabled (true);
      t0ChanAct->setEnabled (true);


      //  Reset the maps based on the current state of the pane toggle buttons in the Pane Visibility menu.

      int32_t row1Height = map[0]->size ().rheight ();

      for (int32_t i = 0 ; i < 3 ; i++)
        {
          if (GetMapVisibilityButtonState (i))
            {
              map[i]->setVisible (true);
              row1Height = map[i]->size ().rheight ();
            }
        }


      int32_t row2Height = map[3]->size ().rheight ();

      for (int32_t i = 3 ; i < 6 ; i++)
        {
          if (GetMapVisibilityButtonState (i))
            {
              map[i]->setVisible (true);
              row2Height = map[i]->size ().rheight ();
            }
          else
            {
              if (i == 4) map[i]->setVisible (false);
            }
        }


      int32_t row3Height = map[6]->size ().rheight ();

      for (int32_t i = 6 ; i < 10 ; i++)
        {
          if (GetMapVisibilityButtonState (i))
            {
              map[i]->setVisible (true);
              row3Height = map[i]->size ().rheight ();
            }
        }


      int32_t totalCurHeight = 0;

      if (map[0]->isVisible () || map[1]->isVisible () || map[2]->isVisible ()) totalCurHeight += row1Height;

      if (map[3]->isVisible () || map[4]->isVisible () || map[5]->isVisible ()) totalCurHeight += row2Height;

      if (map[6]->isVisible () || map[7]->isVisible () || map[8]->isVisible () || map[9]->isVisible ()) totalCurHeight += row3Height;


      int32_t windowOffset = this->size ().rheight () - totalCurHeight;


      int32_t deltaSize = 0;

      if (map[0]->isVisible () || map[1]->isVisible () || map[2]->isVisible ()) deltaSize += row1Height;

      if (map[3]->isVisible () || map[4]->isVisible () || map[5]->isVisible ()) deltaSize += row2Height;

      if (map[6]->isVisible () || map[7]->isVisible () || map[8]->isVisible () || map[9]->isVisible ()) deltaSize += row3Height;

      this->resize(this->size ().rwidth (), windowOffset + deltaSize);
    }


  force_redraw = NVTrue;
}



void 
CZMILwaveMonitor::slotMode ()
{
  wave_line_mode = !wave_line_mode;
  

  force_redraw = NVTrue;
}



void 
CZMILwaveMonitor::slotInterest ()
{
  interest_point_mode = !interest_point_mode;
  

  force_redraw = NVTrue;
}



//
// slot:        slotIntMeter
//
// This slot is fired when the user toggles the intensity meter.  We change
// our flag and if we are turning the meter on we change our cursor to 
// the pointing hand.  Otherwise, we go back to the standard.  
//

void
CZMILwaveMonitor::slotIntMeter () 
{
  showIntMeter = !showIntMeter;
  
  if (showIntMeter)
    {
      map[0]->setCursor (Qt::PointingHandCursor);
    }
  else
    {
      map[0]->setCursor (Qt::ArrowCursor);
    }
  

  force_redraw = NVTrue;
}



//
// slot:        slotZoomIn
//                                        
// This slot is called either from a menu selection or a right-menu context
// click.  It simply changes our zooming flag to true.
//

void
CZMILwaveMonitor::slotZoomIn()
{
  zoomFlag = NVTrue;
  for (int32_t i = 0 ; i < NUM_WAVEFORMS ; i++) map[i]->setCursor (zoomCursor);
}



// fixed ltimebound in and utimebound in the following 3 functions
//
// slotresetzoom 
//        - delete rev 1.2 change
// slotContextResetZoom
//        - changed raman hard code from 200 to ir.length
//        - subtracted 1 from the lengths to properly set the utimebounds
//
// slot:        slotResetZoom
//
// This slot is called from the Reset Zoom menu selection or right-click
// option.  We turn zooming off by changing zoomFlag.  We reset our time
// and count bounds to the original values and redraw the CZMILwaveMonitor.
//

void
CZMILwaveMonitor::slotResetZoom()
{
  zoomFlag = zoomIsActive = NVFalse;

  
  for (int32_t i = 0 ; i < NUM_WAVEFORMS ; i++)
    {
      lTimeBound[i] = LOWER_TIME_BOUND;
      uTimeBound[i] = chanBounds[i].length - 1;
      lCountBound[i] = LOWER_COUNT_BOUND;
      uCountBound[i] = UPPER_COUNT_BOUND;
      map[i]->setCursor (Qt::ArrowCursor);
    }


  force_redraw = NVTrue;
}




// called from contextMenu's resetZoom action (this only resets the zoom for the window in which it is called)

void
CZMILwaveMonitor::slotContextResetZoom()
{
  zoomFlag = zoomIsActive = NVFalse;

  int32_t i = GetSensorMapType (zoomOutMap);
  
  lTimeBound[i] = LOWER_TIME_BOUND;
  uTimeBound[i] = chanBounds[i].length - 1;
  lCountBound[i] = LOWER_COUNT_BOUND;
  uCountBound[i] = UPPER_COUNT_BOUND;
  map[i]->setCursor (Qt::ArrowCursor);

  nonFileIO_force_redraw = NVTrue;
}



void 
CZMILwaveMonitor::setFields ()
{
  if (pos_format == "hdms") bGrp->button (0)->setChecked (true);
  if (pos_format == "hdm") bGrp->button (1)->setChecked (true);
  if (pos_format == "hd") bGrp->button (2)->setChecked (true);
  if (pos_format == "dms") bGrp->button (3)->setChecked (true);
  if (pos_format == "dm") bGrp->button (4)->setChecked (true);
  if (pos_format == "d") bGrp->button (5)->setChecked (true);
  
  
  if (startup_message)
    {
      sMessage->setCheckState (Qt::Checked);
    }
  else
    {
      sMessage->setCheckState (Qt::Unchecked);
    }
  
  
  int32_t hue, sat, val;
  
  surfaceColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bSurfacePalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bSurfacePalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bSurfacePalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bSurfacePalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bSurfacePalette.setColor (QPalette::Normal, QPalette::Button, surfaceColor);
  bSurfacePalette.setColor (QPalette::Inactive, QPalette::Button, surfaceColor);
  bSurfaceColor->setPalette (bSurfacePalette);
  
  
  primaryColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bPrimaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bPrimaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bPrimaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bPrimaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bPrimaryPalette.setColor (QPalette::Normal, QPalette::Button, primaryColor);
  bPrimaryPalette.setColor (QPalette::Inactive, QPalette::Button, primaryColor);
  bPrimaryColor->setPalette (bPrimaryPalette);
  
  
  secondaryColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bSecondaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bSecondaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bSecondaryPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bSecondaryPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bSecondaryPalette.setColor (QPalette::Normal, QPalette::Button, secondaryColor);
  bSecondaryPalette.setColor (QPalette::Inactive, QPalette::Button, secondaryColor);
  bSecondaryColor->setPalette (bSecondaryPalette);
  
  
  highlightColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bHighlightPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bHighlightPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bHighlightPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bHighlightPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bHighlightPalette.setColor (QPalette::Normal, QPalette::Button, highlightColor);
  bHighlightPalette.setColor (QPalette::Inactive, QPalette::Button, highlightColor);
  bHighlightColor->setPalette (bHighlightPalette);
  
  
  backgroundColor.getHsv (&hue, &sat, &val);
  if (val < 128)
    {
      bBackgroundPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bBackgroundPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bBackgroundPalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bBackgroundPalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bBackgroundPalette.setColor (QPalette::Normal, QPalette::Button, backgroundColor);
  bBackgroundPalette.setColor (QPalette::Inactive, QPalette::Button, backgroundColor);
  bBackgroundColor->setPalette (bBackgroundPalette);
  
  // we account for making the text of the button white or black based on the shading of 
  // the color of the button.  we then set the color of the pallete attached to the button
  // to that of what the user picked within the QColorDialog.  We update the palette.
  
  referenceColor.getHsv (&hue, &sat, &val);
  
  if (val < 128)
    {
      bReferencePalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::white);
      bReferencePalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::white);
    }
  else
    {
      bReferencePalette.setColor (QPalette::Normal, QPalette::ButtonText, Qt::black);
      bReferencePalette.setColor (QPalette::Inactive, QPalette::ButtonText, Qt::black);
    }
  bReferencePalette.setColor (QPalette::Normal, QPalette::Button, referenceColor);
  bReferencePalette.setColor (QPalette::Inactive, QPalette::Button, referenceColor);
  bReferenceColor->setPalette (bReferencePalette);

  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) 
    {
      if (map[i]->isVisible()) map[i]->redrawMapArea (NVTrue);
    }
}



// 
// method:        GetTickSpacing
//
// This method will get the intervals for our tick marks for our legends.
// For a range of < 10, we do single tick steps.  Anything over that is done
// by a tenth of the range.
//

void
CZMILwaveMonitor::GetTickSpacing (int32_t lTimeBound, int32_t uTimeBound, int32_t lCountBound, int32_t uCountBound,
                                  int32_t &timeSpacing, int32_t &countSpacing, int32_t mapIndex)
{
  int32_t timeRange, countRange;

  timeRange = uTimeBound - lTimeBound + 1;
  countRange = uCountBound - lCountBound;


  //  IR and T0 are shorter (in screen real estate)

  if (mapIndex == 7)
    {
      if (timeRange <= 10)
        {
          timeSpacing = 1;
        }
      else
        {
          timeSpacing = timeRange / 4;
        }
    }
  else if (mapIndex == 9)
    {
      if (timeRange <= 10)
        {
          timeSpacing = 1;
        }
      else
        {
          timeSpacing = timeRange / 4;
        }
    }
  else
    {
      if (timeRange <= 10)
        {
          timeSpacing = 1;
        }
      else
        {
          timeSpacing = timeRange / 10;
        }
    }


  if (countRange <= 10)
    {
      countSpacing = 1;
    }
  else
    {
      countSpacing = countRange / 10;
    }
}



//
// method:        InsertGridTicks
//
// This method will take the current bounds of the sensor type and proceed
// to display a tick mark with the proper numbers under them for either
// vertical or horizontal mode.
//

void
CZMILwaveMonitor::InsertGridTicks (int32_t start, int32_t end, char axis, int32_t spacing, int32_t type, int32_t axisIndex, nvMap * l_map, QColor color)
{
  int32_t pix_x, pix_y;
  QString value;
  int32_t length = 0, scaledValue, lCount = 0, uCount = 0, lTime = 0, uTime = 0;


  // get the time and count bounds based on type
  /* revised and fixed for new indexing */  

  length = chanBounds[type].length;

  lCount = lCountBound[type];
  uCount = uCountBound[type];
  lTime = lTimeBound[type];
  uTime = uTimeBound[type];


  // we move from the starting value to the end value
  
  for (int32_t i = start; i <= end; i++) 
    {
      // if we've hit a spot for the tick mark, we will check which axis and which display
      // mode we are in (vertical/horizontal).  Then, we will draw a tick mark and the number
      // to place.

      if ((i % spacing) == 0) 
        {
          if ((axis == 'x') || (axis == 'X')) 
            {
              scaledValue = (int32_t) (((i - lTime) / ((float) (uTime - lTime)) * (length - 2)) + 1);
              scaleWave (scaledValue, axisIndex, &pix_x, &pix_y, type, l_map->getMapdef());
              l_map->drawLine (pix_x, pix_y, pix_x, pix_y + LBL_PIX_SPACING - 2, color, 1, NVFalse, Qt::SolidLine);

              value = QString ("%1").arg (i, 3, 10, QChar (' '));
              l_map->drawText (value, NINT (pix_x - gridWidth / 2.0), pix_y + LBL_PIX_SPACING + pt8Height, 90.0, 8, color, NVTrue);
            }
          else 
            {
              scaledValue = (int32_t) ((i - lCount) / ((float) (uCount - lCount)) * UPPER_COUNT_BOUND);
              scaleWave (axisIndex, scaledValue, &pix_x, &pix_y, type, l_map->getMapdef());
              l_map->drawLine (pix_x, pix_y, pix_x - LBL_PIX_SPACING - 2, pix_y, color, 1, NVFalse, Qt::SolidLine);

              value = QString ("%1").arg (i, 3, 10, QChar (' '));
              l_map->drawText (value, pix_x - LBL_PIX_SPACING - gridWidth - 3, NINT (pix_y + pt8Height / 2.0), 90.0, 8, color, NVTrue);
            }
        }
    }
}



void
CZMILwaveMonitor::slotPrefs ()
{
  prefsD = new QDialog (this, (Qt::WindowFlags) Qt::WA_DeleteOnClose);
  
  prefsD->setWindowTitle (tr ("CZMIL Waveform Viewer Preferences"));
  
  prefsD->setModal (true);
  
  QVBoxLayout *vbox = new QVBoxLayout (prefsD);
  vbox->setMargin (5);
  vbox->setSpacing (5);
  
  QGroupBox *fbox = new QGroupBox (tr ("Position Format"), prefsD);
  fbox->setWhatsThis (bGrpText);
  
  QRadioButton *hdms = new QRadioButton (tr ("Hemisphere Degrees Minutes Seconds.decimal"));
  QRadioButton *hdm_ = new QRadioButton (tr ("Hemisphere Degrees Minutes.decimal"));
  QRadioButton *hd__ = new QRadioButton (tr ("Hemisphere Degrees.decimal"));
  QRadioButton *sdms = new QRadioButton (tr ("+/-Degrees Minutes Seconds.decimal"));
  QRadioButton *sdm_ = new QRadioButton (tr ("+/-Degrees Minutes.decimal"));
  QRadioButton *sd__ = new QRadioButton (tr ("+/-Degrees.decimal"));
  
  bGrp = new QButtonGroup (prefsD);
  bGrp->setExclusive (true);
  connect (bGrp, SIGNAL (buttonClicked (int)), this, SLOT (slotPosClicked (int)));
  
  bGrp->addButton (hdms, 0);
  bGrp->addButton (hdm_, 1);
  bGrp->addButton (hd__, 2);
  bGrp->addButton (sdms, 3);
  bGrp->addButton (sdm_, 4);
  bGrp->addButton (sd__, 5);
  
  QHBoxLayout *fboxSplit = new QHBoxLayout;
  QVBoxLayout *fboxLeft = new QVBoxLayout;
  QVBoxLayout *fboxRight = new QVBoxLayout;
  fboxSplit->addLayout (fboxLeft);
  fboxSplit->addLayout (fboxRight);
  fboxLeft->addWidget (hdms);
  fboxLeft->addWidget (hdm_);
  fboxLeft->addWidget (hd__);
  fboxRight->addWidget (sdms);
  fboxRight->addWidget (sdm_);
  fboxRight->addWidget (sd__);
  fbox->setLayout (fboxSplit);
  
  vbox->addWidget (fbox, 1);
  
  if (pos_format == "hdms") bGrp->button (0)->setChecked (true);
  if (pos_format == "hdm") bGrp->button (1)->setChecked (true);
  if (pos_format == "hd") bGrp->button (2)->setChecked (true);
  if (pos_format == "dms") bGrp->button (3)->setChecked (true);
  if (pos_format == "dm") bGrp->button (4)->setChecked (true);
  if (pos_format == "d") bGrp->button (5)->setChecked (true);
  
  
  QGroupBox *cbox = new QGroupBox (tr ("Colors"), this);
  QVBoxLayout *cboxLayout = new QVBoxLayout;
  cbox->setLayout (cboxLayout);
  QHBoxLayout *cboxTopLayout = new QHBoxLayout;
  
  // since we are adding another layout which is at the bottom, the former bottom becomes the middle
  // and we still maintain the bottom layout for the reference color QPushButton
  
  QHBoxLayout *cboxMiddleLayout = new QHBoxLayout;
  QHBoxLayout *cboxBottomLayout = new QHBoxLayout;
  
  cboxLayout->addLayout (cboxTopLayout);
  cboxLayout->addLayout (cboxMiddleLayout);
  cboxLayout->addLayout (cboxBottomLayout);
  
  bSurfaceColor = new QPushButton (tr ("Surface"), this);
  bSurfaceColor->setToolTip (tr ("Change surface return color"));
  bSurfaceColor->setWhatsThis (bSurfaceColor->toolTip ());
  bSurfacePalette = bSurfaceColor->palette ();
  connect (bSurfaceColor, SIGNAL (clicked ()), this, SLOT (slotSurfaceColor ()));
  cboxTopLayout->addWidget (bSurfaceColor);
  
  
  bPrimaryColor = new QPushButton (tr ("Primary"), this);
  bPrimaryColor->setToolTip (tr ("Change primary return marker color"));
  bPrimaryColor->setWhatsThis (bPrimaryColor->toolTip ());
  bPrimaryPalette = bPrimaryColor->palette ();
  connect (bPrimaryColor, SIGNAL (clicked ()), this, SLOT (slotPrimaryColor ()));
  cboxTopLayout->addWidget (bPrimaryColor);
  
  bSecondaryColor = new QPushButton (tr ("Secondary"), this);
  bSecondaryColor->setToolTip (tr ("Change secondary return marker color"));
  bSecondaryColor->setWhatsThis (bSecondaryColor->toolTip ());
  bSecondaryPalette = bSecondaryColor->palette ();
  connect (bSecondaryColor, SIGNAL (clicked ()), this, SLOT (slotSecondaryColor ()));
  cboxMiddleLayout->addWidget (bSecondaryColor);

  bHighlightColor = new QPushButton (tr ("Highlight"), this);
  bHighlightColor->setToolTip (tr ("Change axis highlight color"));
  bHighlightColor->setWhatsThis (bHighlightColor->toolTip ());
  bHighlightPalette = bHighlightColor->palette ();
  connect (bHighlightColor, SIGNAL (clicked ()), this, SLOT (slotHighlightColor ()));
  cboxMiddleLayout->addWidget (bHighlightColor);

  
  bBackgroundColor = new QPushButton (tr ("Background"), this);
  bBackgroundColor->setToolTip (tr ("Change display background color"));
  bBackgroundColor->setWhatsThis (bBackgroundColor->toolTip ());
  bBackgroundPalette = bBackgroundColor->palette ();
  connect (bBackgroundColor, SIGNAL (clicked ()), this, SLOT (slotBackgroundColor ()));
  
  cboxBottomLayout->addWidget (bBackgroundColor);
  
  // adding the ability to change the reference mark overlay color
  
  bReferenceColor = new QPushButton (tr ("Reference"), this);
  bReferenceColor->setToolTip (tr ("Change reference overlay color"));
  bReferenceColor->setWhatsThis (bReferenceColor->toolTip());
  bReferencePalette = bReferenceColor->palette ();
  connect (bReferenceColor, SIGNAL (clicked ()), this, SLOT (slotReferenceColor ()));
  cboxBottomLayout->addWidget (bReferenceColor);
  
  vbox->addWidget (cbox, 1);
  
  
  QGroupBox *mBox = new QGroupBox (tr ("Display startup message"), this);
  QHBoxLayout *mBoxLayout = new QHBoxLayout;
  mBox->setLayout (mBoxLayout);
  sMessage = new QCheckBox (mBox);
  sMessage->setToolTip (tr ("Toggle display of startup message when program starts"));
  mBoxLayout->addWidget (sMessage);
  
  
  vbox->addWidget (mBox, 1);
  
  
  QHBoxLayout *actions = new QHBoxLayout (0);
  vbox->addLayout (actions);
  
  QPushButton *bHelp = new QPushButton (prefsD);
  bHelp->setIcon (QIcon (":/icons/contextHelp.png"));
  bHelp->setToolTip (tr ("Enter What's This mode for help"));
  connect (bHelp, SIGNAL (clicked ()), this, SLOT (slotHelp ()));
  actions->addWidget (bHelp);
  
  actions->addStretch (10);
  
  bRestoreDefaults = new QPushButton (tr ("Restore Defaults"), this);
  bRestoreDefaults->setToolTip (tr ("Restore all preferences to the default state"));
  bRestoreDefaults->setWhatsThis (restoreDefaultsText);
  connect (bRestoreDefaults, SIGNAL (clicked ()), this, SLOT (slotRestoreDefaults ()));
  actions->addWidget (bRestoreDefaults);
  
  QPushButton *closeButton = new QPushButton (tr ("Close"), prefsD);
  closeButton->setToolTip (tr ("Close the preferences dialog"));
  connect (closeButton, SIGNAL (clicked ()), this, SLOT (slotClosePrefs ()));
  actions->addWidget (closeButton);
  
  setFields ();
  
  prefsD->show ();
}


//
// Slot:                slotReferenceColor
//
// This slot will automagically be called when the user presses the "Reference" color button
// to change the appearance of the reference waveform signatures.  A color dialog box will be
// displayed allowing the user to select the new color.
//

void CZMILwaveMonitor::slotReferenceColor ()
{
  QColor clr;
  
  clr = QColorDialog::getColor (referenceColor, this, tr ("CZMIL Waveform Viewer Reference Color"), QColorDialog::ShowAlphaChannel);
  
  if (clr.isValid ()) referenceColor = clr;
  
  setFields ();
}



  // now saves last directory of opening a file


void CZMILwaveMonitor::slotOpen ()
{
  QFileDialog fd;
  if (!QDir(savedSettings.openDirectory).exists()  ) savedSettings.openDirectory = ".";
  
  QString fileName = fd.getOpenFileName(this, tr ("Open Waveform File"), savedSettings.openDirectory, tr ("Waveform Files (*.wfm)"));
  if ( fileName.isNull() ) return;
  
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  
  savedSettings.openDirectory = QFileInfo(fileName).canonicalPath();
  
  QTextStream ts(&file);
  QString tok;
  
  do ts >> tok; while ( tok != "Time_Bins"  && !ts.atEnd() );
  
  if (!ts.atEnd())
    {
    
      int32_t numCols;
      for( numCols = 0; tok != "1"; ts >> tok ) numCols++;
    
      loadReferenceData(ts, numCols);
    }
  
  file.close();
  
  // save refFileLbl and refFileWidth on a successful open
  
  if (drawReference)
    {
    
      QFileInfo refFileInfo (fileName);
      refFileLbl = refFileInfo.fileName ();
    
      // get type and size of font we are typing with
    
      QFont font("Charter", 12);
      QFontMetrics fontMetric (font);
      refFileWidth = fontMetric.width (refFileLbl);
    
      for (int32_t i = 0 ; i < NUM_WAVEFORMS; i++) 
        {
          if (map[i]->isVisible()) map[i]->redrawMapArea(NVTrue);
        }
    }
}



void CZMILwaveMonitor::loadReferenceData(QTextStream &fileDat, int32_t numCols)
{
  allocateReferenceData ((numCols-1) / NUM_WAVEFORMS);
  
  QString tok;
  bool isInt;
  int32_t val, r;
  int32_t writes = 0;
  
  for (r = 0; r < UPPER_TIME_BOUND && !fileDat.atEnd(); r++)
    {
    
      if (r>0) fileDat >> tok; // skip the time_bin already read for first so no skipping the first;
    
      for (int32_t wave = 0; wave < numReferences; wave++) 
        {
          for (int32_t c = 0; c < NUM_WAVEFORMS && !fileDat.atEnd(); c++)
            {
        
              fileDat >> tok;
        
              val = tok.toInt (&isInt, 10);
              
              if (isInt)
                { 
                  writes++;
                  referenceData[wave][c][r+1] = (int16_t)val;
                } // start saving data at referenceData[wave][0][1] to work with old code
            }
        }
    }
  
  
  // set referenceData true if read through all UPPER_TIME_BOUND 
  
  drawReference = (r == UPPER_TIME_BOUND);
  

  // we do want to redraw all the maps so we can see the reference waveforms but we need to
  // do it after we have the reference label established which is later on in slotOpen.
  // Therefore, I deleted the redrawing here.
  
  if (!drawReference) clearReferenceData ();

  
  // assumption:  if we get here, all went well with the opening of the file.
  // we simply enable the reference mark entries.
  
  toggleReference->setEnabled (drawReference);
  toggleReference->setChecked (drawReference);
  clearReference->setEnabled  (drawReference);
}


void CZMILwaveMonitor::allocateReferenceData(int32_t numWaves)
{
  clearReferenceData();
  
  int32_t lengths[NUM_WAVEFORMS];
  
  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) lengths[i] = chanBounds[i].length;
  
  referenceData = (uint16_t ***) calloc( numWaves, sizeof(uint16_t **) );
  
  for (int32_t i = 0; i < numWaves; i ++ )
    {
    
      referenceData[i] = (uint16_t **) calloc ( 5, sizeof (uint16_t *));
    
      for (int32_t j = 0; j < NUM_WAVEFORMS; j++) referenceData[i][j] = (uint16_t *) calloc (lengths[j], sizeof (uint16_t));
    }
  
  numReferences = numWaves;
}



void CZMILwaveMonitor::clearReferenceData()
{
  if (numReferences > 0)
    {
      for (int32_t i = 0; i < numReferences; i ++ )
        {                
          for (int32_t j = 0; j < NUM_WAVEFORMS; j++) free(referenceData[i][j]);
          free(referenceData[i]);
        }
      free( referenceData);
      numReferences = 0;
      drawReference = NVFalse;
    }
}




// now saves last directory of saving a file

void CZMILwaveMonitor::slotSave ()
{
  QFileDialog fd;

  if (!QDir(savedSettings.saveDirectory).exists() ) savedSettings.saveDirectory = ".";
  
  QString saveName = fd.getSaveFileName(this, tr ("Save Waveform File"), savedSettings.saveDirectory, tr ("Waveform Files (*.wfm)"));


  if (!saveName.isNull() )
    {
      if (!saveName.endsWith (".wfm")) saveName.append (".wfm");

      ofstream outFile(qPrintable(saveName));
      if ( outFile.is_open() )
        {
          writeWaveformHeader( outFile, saveName );
          writeWaveformData(outFile);
          outFile.close();
          savedSettings.saveDirectory = QFileInfo(saveName).canonicalPath();
        }
      else
        {
          QMessageBox::warning (this, tr ("File not saved"), tr ("Unable to open %1").arg (saveName));
        } 
    }
}


void CZMILwaveMonitor::writeWaveformHeader(ofstream &outFile, QString &fileName)
{
  QString pfmFiles = tr ("PFM File(s)");
  
  
  for (int32_t pfm = 0 ; pfm < num_pfms ; pfm++) pfmFiles += tr ("\t") + tr (open_args[pfm].list_path) + "\n";
  
  outFile << "; " << qPrintable( QDateTime::currentDateTime().toString("MM/dd/yyyy h:mmap") ) << "\n"
          << "; " << qPrintable( fileName ) << "\n"
          << "; " << qPrintable( pfmFiles ) << "\n";
}


//
// method:                WriteWaveformData 
//
// This method will produce the waveform data portion of the .wfm file.  It will write the
// waveform data headings for all waveforms combining the record number and flightline.  It will
// then traverse through the timebins with the waveform information for all sensors.
//

void CZMILwaveMonitor::writeWaveformData (ofstream &outFile)
{
  // only supports CZMIL data sets        
  
  if (abe_share->mwShare.multiType[0] == PFM_CZMIL_DATA)
    {
      outFile << ";------------------------------------\n";
      outFile << "; Waveform data\n"; 
      outFile << ";------------------------------------\n\n";
    
      outFile << "Time_Bins\t";
    
      char flightLn[20];
    
      char sensorHeadings[NUM_WAVEFORMS - 1][7] = {"DEEP_", "SHAL1_", "SHAL2_", "SHAL3_", "SHAL4_", "SHAL5_", "SHAL6_", "SHAL7_", "IR_"};
    
      QString appendage;
    
      int32_t numWaveforms = multiWaveMode ? 1 : MAX_STACK_POINTS;
    
      for (int32_t i = 0; i < numWaveforms; i++)
        {
          strcpy (flightLn, read_line_file (pfm_handle[abe_share->mwShare.multiPfm[i]], abe_share->mwShare.multiLine[i]));
      
          // needed to get the gps time to be added to the header column labels
      
          QString gpsTime = RetrieveGPSTime (i);
      
          appendage.sprintf ("%s_%s_%d", qPrintable (gpsTime), flightLn, abe_share->mwShare.multiRecord[i]);
      
          appendage.remove (appendage.indexOf ("Line "), 5);
      
          for (int32_t j = 0; j < NUM_WAVEFORMS - 1; j++)
            {
              outFile << sensorHeadings[j] << qPrintable (appendage) << "\t";
            }
        }
    
      outFile << "\n";
      outFile.flush ();
    

      // no record 0.  
    
      for (int32_t i = 1; i <= UPPER_TIME_BOUND; i++)
        {
      
          outFile << i << "\t";
      
          if (multiWaveMode)
            {
              writeSingleTableEntry (outFile, i);
            }
          else
            {
              writeNNTableEntry (outFile, i);
            }
          
          outFile << "\n";
        }
    
      outFile.flush ();
    }
}



//
// Method:                RetrieveGPSTime
//
// This function will return just the GPS time that is embedded within the .hof file.  This
// will usually be a string composed of 4 digits that will be used as header labels within
// a .wfm file.
//

QString CZMILwaveMonitor::RetrieveGPSTime (int32_t index)
{
  int16_t type;
  char czmil_file[512];
  
  QString gpsTime;
  
  read_list_file (pfm_handle[abe_share->mwShare.multiPfm[index]], abe_share->mwShare.multiFile[index], czmil_file, &type);
  
  gpsTime = QString (pfm_basename (czmil_file));
  
  gpsTime = gpsTime.section ("_", 3, 3);
  
  return gpsTime;
}



//
// method:                WriteSingleTableEntry
//
// This method accounts for the single waveform mode.  It will write out the deep, shallow, and ir channels.
// information.


void CZMILwaveMonitor::writeSingleTableEntry (ofstream &outfile, int32_t timeIndex)
{
  for (int32_t i = 0; i < NUM_WAVEFORMS - 1; i++) outfile << (int32_t) cwf_record.channel[i][timeIndex] << "\t";
}




//
// method:                WriteNNTableEntry
//
// This method accounts for the nearest neighbors mode.  It will write out the PMT, APD, TBDD, IR, and RAMAN
// information.
//

void CZMILwaveMonitor::writeNNTableEntry (ofstream &outfile, int32_t timeIndex)
{
  for (int32_t i = 0; i < MAX_STACK_POINTS; i++)
    {
      for (int32_t j = 0; j < NUM_WAVEFORMS - 1; j++)
        {
          outfile << (int32_t)cwf_record_multi[i].channel[j][timeIndex] << "\t";
        }        
    }
}





void
CZMILwaveMonitor::slotPosClicked (int id)
{
  switch (id)
    {
    case 0:
    default:
      pos_format = "hdms";
      break;
      
    case 1:
      pos_format = "hdm";
      break;
      
    case 2:
      pos_format = "hd";
      break;
      
    case 3:
      pos_format = "dms";
      break;
      
    case 4:
      pos_format = "dm";
      break;
      
    case 5:
      pos_format = "d";
      break;
    }
}



void
CZMILwaveMonitor::slotClosePrefs ()
{
  if (sMessage->checkState () == Qt::Checked)
    {
      startup_message = NVTrue;
    }
  else
    {
      startup_message = NVFalse;
    }
  
  prefsD->close ();
}



void
CZMILwaveMonitor::slotSurfaceColor ()
{
  QColor clr;
  
  clr = QColorDialog::getColor (surfaceColor, this, tr ("CZMIL Waveform Viewer Surface Return Color"), QColorDialog::ShowAlphaChannel);
  
  if (clr.isValid ()) surfaceColor = clr;
  
  setFields ();
}



void
CZMILwaveMonitor::slotPrimaryColor ()
{
  QColor clr;
  
  clr = QColorDialog::getColor (primaryColor, this, tr ("CZMIL Waveform Viewer Primary Marker Color"), QColorDialog::ShowAlphaChannel);
  
  if (clr.isValid ()) primaryColor = clr;
  
  setFields ();
}



void
CZMILwaveMonitor::slotSecondaryColor ()
{
  QColor clr;
  
  clr = QColorDialog::getColor (secondaryColor, this, tr ("CZMIL Waveform Viewer Secondary Marker Color"), QColorDialog::ShowAlphaChannel);
  
  if (clr.isValid ()) secondaryColor = clr;
  
  setFields ();
}



void
CZMILwaveMonitor::slotHighlightColor ()
{
  QColor clr;
  
  clr = QColorDialog::getColor (highlightColor, this, tr ("CZMIL Waveform Viewer Highlight Axis Color"), QColorDialog::ShowAlphaChannel);
  
  if (clr.isValid ()) highlightColor = clr;
  
  setFields ();
}



void
CZMILwaveMonitor::slotBackgroundColor ()
{
  QColor clr;
  
  clr = QColorDialog::getColor (backgroundColor, this, tr ("CZMIL Waveform Viewer Background Color"));
  
  if (clr.isValid ()) backgroundColor = clr;
  

  for (int32_t i = 0 ; i < NUM_WAVEFORMS ; i++) 
    {
      map[i]->setBackgroundColor (backgroundColor);
    }

  setFields ();
}



//  Convert from waveform coordinates to pixels.

void 
CZMILwaveMonitor::scaleWave (int32_t x, int32_t y, int32_t *pix_x, int32_t *pix_y, int32_t type, NVMAP_DEF l_mapdef)
{
  BOUNDS *b = &(chanBounds[type]);

  *pix_x = NINT (((float) (x - b->min_x) / b->range_x) * (float) (l_mapdef.draw_width - horLblSpace)) + horLblSpace;

  *pix_y = NINT (((float) (b->max_y - (y - b->min_y)) / (float) b->range_y) * (float) (l_mapdef.draw_height - verLblSpace));
}



//  Convert from pixels to waveform coordinates.

void 
CZMILwaveMonitor::unScaleWave (int32_t pix_x, int32_t pix_y, int32_t *x, int32_t *y, int32_t type, NVMAP_DEF l_mapdef)
{
  BOUNDS *b = &(chanBounds[type]);

  *x = (((pix_x - horLblSpace) / (float) (l_mapdef.draw_width - horLblSpace)) * b->range_x) + b->min_x;

  *y = b->max_y - (((float) pix_y / (float) (l_mapdef.draw_height - verLblSpace)) * (float) b->range_y);
}


void 
CZMILwaveMonitor::slotPlotWaves (NVMAP_DEF l_mapdef)
{
  static int32_t          save_chan, save_ret;
  static CZMIL_CWF_Data   save_cwf;
  static CZMIL_CPF_Data   save_cpf;
  static QString          save_name;
  int32_t                 pix_x[2], pix_y[2];


  // 
  //                      we have some new variables added here.  We declare a half opaque green
  //                      for the intensity meter.  scaledTimeValue and scaledCountValue are the
  //                      resultant time and count spots in the selection area after the zoom
  //                      for the current sensor.  The usingMapX parameters reference what pane
  //                      we are currently processing.  previousValid and currentValid are needed
  //                      for line segments to attach.  For a zoomed in region, the lines could
  //                      go in and out of the area. timeSpacing and countSpacing will hold the 
  //                      interval values for our tick marks
  //

  static QColor intMeterColor (0, 255, 0, 255);  // half opaque green    
  uint8_t usingMap[NUM_WAVEFORMS];
  static uint8_t firstTime[NUM_WAVEFORMS] = {NVTrue, NVTrue, NVTrue, NVTrue, NVTrue, NVTrue, NVTrue, NVTrue, NVTrue, NVTrue};

  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) usingMap[i] = NVFalse;

  int32_t timeSpacing, countSpacing;
  nvMap *l_map = (nvMap *) (sender ());


  // key variables that allows us not to duplicate code later on

  int32_t mapIndex, mapLength;


  //  If we haven't updated the point and read a new one, as they say in Poltergeist, "GET OOOOUUUUT!".

  if (cwf_read != CZMIL_SUCCESS) return;


  //  Figure out which widget sent the signal          

  int32_t lengths[NUM_WAVEFORMS];
  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) lengths[i] = chanBounds[i].length;


  //  This actually gets the correct number to go with the widget.  The deep channel which is map widget
  //  0 returns 8.  All the others get mapped properly as well.

  mapIndex = GetSensorMapType (l_map);

  mapLength = lengths[mapIndex];

  usingMap[mapIndex] = NVTrue;


  // change darkgray to referenceColor

  if (drawReference)
    {
      for (int32_t i = 0 ; i < numReferences ; i++) drawSingleWave (l_map, l_mapdef, referenceData[i][mapIndex], mapIndex, mapLength, referenceColor);
    }


  // check to see if ref line should be drawn

  //  Because the trackCursor function may be changing the data while we're still plotting it we save it
  //  to this static structure.  lock_track stops trackCursor from updating while we're trying to get an
  //  atomic snapshot of the data for the latest point.

  lock_track = NVTrue;
  save_cwf = cwf_record;
  save_cpf = cpf_record;
  save_chan = chan;
  save_ret = ret;
  save_name = db_name;
  lock_track = NVFalse;


  //  If we're in current channel mode we only want to plot the current channel data to map[4].

  if (current_channel_mode) mapIndex = save_chan;


  //for (int32_t q = 0 ; q < 9 ; q++) fprintf(stderr,"%s %s %d %d %d %d\n",NVFFL,q,save_cpf.returns[q],lengths[q]);

  //
  // The first time through for any sensor we set the beginning and
  // ending position for the intensity meter  
  //

  // Because we have to get the beginning/ending pixel positions of the time axis the first time
  // through this function, firstTime was changed to an array for each of the sensor maps.  So, for
  // each sensor map, store the beginning/ending axis pixel postiions and set the firstTime array
  // to false.  This is a cludgy way to do it and you would think that slotResize would handle this
  // but slotResize is not called upon window startup like OpenGL.  It is called only when the user
  // resizes the window.
  // 

  if (firstTime[mapIndex] && usingMap[mapIndex])  
    {
      scaleWave (lTimeBound[mapIndex], lCountBound[mapIndex], &(axisPixelBegin[mapIndex]), &pix_y[0], mapIndex, l_mapdef);
      scaleWave (uTimeBound[mapIndex], lCountBound[mapIndex], &(axisPixelEnd[mapIndex]), &pix_y[0], mapIndex, l_mapdef);

      firstTime[mapIndex] = NVFalse;
    }


  // 
  // We check to see if we are drawing the intensity meter.  If so, we get
  // the bin that is highlighted and clamp it to 1-500 if we are past that
  // range.  We scale it if it is in a zoomed range.  We find the projected
  // spot for the pane depending on vertical or horizontal view.  Finally, we
  // draw the line.
  //

  if (showIntMeter)
    {
      // First off, clamp the bin highlighted to a 1-(mapLength -1) value depending on the sensor.
      // Next, scale the binHighlighted based on the current zooming extents of the sensor pane. 
      // Get the intensity meter pixel positions off of the binHighlighted and then draw the line
      // in a lime green color.

      if (binHighlighted[mapIndex] < 1)
        {
          binHighlighted[mapIndex] = 1;
        }
      else
        {
          if (binHighlighted[mapIndex] > (mapLength - 1)) binHighlighted[mapIndex] = mapLength - 1;
        }

      scaledBinHighlighted[mapIndex] = (int32_t) ((lTimeBound[mapIndex] + (((binHighlighted[mapIndex] - 1) / ((float)(mapLength - 2))) *
                                                                            ((float) (uTimeBound[mapIndex] - lTimeBound[mapIndex]))) + 0.5));

      scaleWave (binHighlighted[mapIndex], LOWER_COUNT_BOUND, &pix_x[0], &pix_y[0], mapIndex, l_mapdef);
      scaleWave (binHighlighted[mapIndex], UPPER_COUNT_BOUND, &pix_x[1], &pix_y[1], mapIndex, l_mapdef);

      l_map->drawLine (pix_x[0], pix_y[0], pix_x[1], pix_y[1], intMeterColor, 1, NVFalse, Qt::SolidLine);
    }


  // just a simple move. scaled bin highlighted needs to be computed before a possible drawMultiWaves is called
  // if we are in nearest neighbor mode, draw the multiple waveforms

  uint8_t usingAnyMap = NVFalse;

  for (int32_t i = 0; i < NUM_WAVEFORMS; i++)
    {
      if (usingMap[i])
        {
          usingAnyMap = NVTrue;
          break;
        }
    }  

  if (usingAnyMap && multiWaveMode == 0 && abe_share->mwShare.multiNum == MAX_STACK_POINTS)
    {
      drawMultiWaves (l_map, l_mapdef, mapIndex);
    }


  //
  // if we are not in nearest neighbor.  This
  // means that all cursors in pfmEdit will be represented in this code block.
  // This code block is focusing on displaying the current focused cursor. 
  //  

  if (multiWaveMode == 1)
    {
      // Here, we check to see which mode we are in and we load up the color by cursor or flightline

      if (ccMode == CC_BY_CURSOR) 
        {
          waveColor.setRgb (abe_share->mwShare.multiColors[0].r, abe_share->mwShare.multiColors[0].g, abe_share->mwShare.multiColors[0].b);
        }
      else 
        {
          waveColor = colorArray[abe_share->mwShare.multiFlightlineIndex[0]];
        }

      waveColor.setAlpha (abe_share->mwShare.multiColors[0].a);


      // draw wave form

      drawSingleWave (l_map, l_mapdef, &save_cwf, mapIndex, mapLength, waveColor );

      sim_depth.sprintf ("%.2f", save_cpf.channel[save_chan][save_ret].elevation);


      int32_t lTime = lTimeBound[mapIndex];
      int32_t uTime = uTimeBound[mapIndex];
      int32_t lCount = lCountBound[mapIndex];
      int32_t uCount = uCountBound[mapIndex];

      if (interest_point_mode)
        {
          //  T0

          if (mapIndex == 9)
            {
              if (save_cpf.t0_interest_point != 0.0)
                {
                  int32_t prev_point = (int32_t) save_cpf.t0_interest_point;
                  int32_t next_point = (int32_t) save_cpf.t0_interest_point + 1;

                  int32_t cy = save_cwf.T0[prev_point] + ((save_cwf.T0[next_point] - save_cwf.T0[prev_point]) / 2);

                  int32_t cx = save_cpf.t0_interest_point;

                  if (cx <= uTime && cx >= lTime && cy <= uCount && cy >= lCount)
                    {
                      int32_t scaledTimeValue = (int32_t) (((cx - lTime) / ((float) (uTime - lTime)) * (mapLength - 2)) + 1);

                      int32_t scaledCountValue = (int32_t) ((cy - lCount) / ((float) (uCount - lCount)) * UPPER_COUNT_BOUND);

                      scaleWave (scaledTimeValue, scaledCountValue, &pix_x[0], &pix_y[0], mapIndex, l_mapdef);

                      drawX (pix_x[0], pix_y[0], 10, 2, primaryColor, l_map);
                    }
                }
            }
          else
            {
              for (int32_t i = 0 ; i < save_cpf.returns[mapIndex] ; i++)
                {
                  if (save_cpf.channel[mapIndex][i].interest_point != 0.0)
                    {
                      int32_t prev_point = (int32_t) save_cpf.channel[mapIndex][i].interest_point;
                      int32_t next_point = (int32_t) save_cpf.channel[mapIndex][i].interest_point + 1;

                      int32_t cy = save_cwf.channel[mapIndex][prev_point] + ((save_cwf.channel[mapIndex][next_point] - save_cwf.channel[mapIndex][prev_point]) / 2);

                      int32_t cx = save_cpf.channel[mapIndex][i].interest_point;

                      if (cx <= uTime && cx >= lTime && cy <= uCount && cy >= lCount)
                        {
                          int32_t scaledTimeValue = (int32_t) (((cx - lTime) / ((float) (uTime - lTime)) * (mapLength - 2)) + 1);

                          int32_t scaledCountValue = (int32_t) ((cy - lCount) / ((float) (uCount - lCount)) * UPPER_COUNT_BOUND);

                          scaleWave (scaledTimeValue, scaledCountValue, &pix_x[0], &pix_y[0], mapIndex, l_mapdef);

                          if (mapIndex == save_chan && i == save_ret)
                            {
                              if ((!save_cpf.channel[mapIndex][i].ip_rank) && (save_cpf.optech_classification[mapIndex] != CZMIL_OPTECH_CLASS_HYBRID))
                                {
                                  drawX (pix_x[0], pix_y[0], 10, 2, surfaceColor, l_map);
                                }
                              else
                                {
                                  drawX (pix_x[0], pix_y[0], 10, 2, primaryColor, l_map);
                                }
                            }
                          else
                            {
                              if ((!save_cpf.channel[mapIndex][i].ip_rank) && (save_cpf.optech_classification[mapIndex] != CZMIL_OPTECH_CLASS_HYBRID))
                                {
                                  drawX (pix_x[0], pix_y[0], 10, 2, surfaceColor, l_map);
                                }
                              else
                                {
                                  drawX (pix_x[0], pix_y[0], 7, 2, secondaryColor, l_map);
                                }
                            }
                        }
                    }
                }
            }
        }
    }


  //  Set the status bar labels

  nameLabel->setText (save_name);
  QString recText;
  recText.setNum (current_record);
  recordLabel->setText (recText);
  simDepth->setText (sim_depth);

  int32_t w = l_mapdef.draw_width, h = l_mapdef.draw_height;


  //
  // 110 is hardcoded in to provide the same spacing.  i'm just placing
  // the labels at the top right to get out of the way of the signatures. 
  // if we have the intensity meter up, set the labels
  //

  if (showIntMeter && !(zoomIsActive == NVTrue && zoomFlag == NVTrue))
    {
      //  T0

      if (mapIndex == 9)
        {
        }
      else
        {
          // Since the intensity meters are independent and not linked, we had to point them to the 
          // proper spots in the scaledBinHighlighted array.  This block of code could be cleaned up 
          // by the way.             

          uint16_t * waveformData;

          waveformData = save_cwf.channel[mapIndex];

          if (usingMap[mapIndex])
            {
              if ((scaledBinHighlighted[mapIndex] > uTimeBound[mapIndex]) || (scaledBinHighlighted[mapIndex] < lTimeBound[mapIndex]))
                {
                  chanTxtLbl[mapIndex] = "N/A";
                }
              else
                {
                  chanTxtLbl[mapIndex].sprintf ("%3d", waveformData[scaledBinHighlighted[mapIndex]]);
                }
            }

          binTxtLbl.sprintf ("Bin #%3d", scaledBinHighlighted[mapIndex]);
        }
    }

  int32_t fromTop = 110;

  if (showIntMeter && multiWaveMode == 1) 
    {           
      if (usingMap[mapIndex]) l_map->drawText (chanTxtLbl[mapIndex], w - 35, fromTop - 70, Qt::white, NVTrue);
    }

  static QString lbls[NUM_WAVEFORMS] = {"Shal1", "Shal2", "Shal3", "Shal4", "Shal5", "Shal6", "Shal7", "IR", "Deep", "T0"};

  int32_t lblXOffset    = 65;
  int32_t binLblXOffset = 140;

  if (showIntMeter) l_map->drawText ( binTxtLbl, w - binLblXOffset, fromTop - 90, intMeterColor, NVTrue);

  l_map->drawText (lbls[mapIndex], w - lblXOffset, fromTop - 90, Qt::white, NVTrue);


  // draw the reference overlay file in the event that we are visualizing them

  if (drawReference) l_map->drawText (refFileLbl, NINT (w * 0.5 - refFileWidth / 2.0), fromTop - 90, referenceColor, NVTrue);


  if (mapIndex == save_chan)
    {
      l_map->drawText ("Time (ns)", NINT (w * 0.5 - timeWidth / 2.0), h - LBL_PIX_SPACING, highlightColor, NVTrue);
      l_map->drawText ("Counts", LBL_PIX_SPACING, NINT (h * 0.5 - countsWidth / 2.0), 180.0, 12, highlightColor, NVTrue);


      // draw axis lines  

      scaleWave (UPPER_TIME_BOUND, LOWER_COUNT_BOUND, &pix_x[0], &pix_y[0], CZMIL_DEEP_CHANNEL, l_mapdef);

      l_map->drawLine (horLblSpace, h - verLblSpace, pix_x[0], h - verLblSpace, highlightColor, 1, NVFalse, Qt::SolidLine);

      scaleWave (LOWER_TIME_BOUND, UPPER_COUNT_BOUND, &pix_x[0], &pix_y[0], CZMIL_DEEP_CHANNEL, l_mapdef);

      l_map->drawLine (horLblSpace, h - verLblSpace, horLblSpace, pix_y[0], highlightColor, 1, NVFalse, Qt::SolidLine);
    }
  else
    {
      l_map->drawText ("Time (ns)", NINT (w * 0.5 - timeWidth / 2.0), h - LBL_PIX_SPACING, Qt::white, NVTrue);
      l_map->drawText ("Counts", LBL_PIX_SPACING, NINT (h * 0.5 - countsWidth / 2.0), 180.0, 12, Qt::white, NVTrue);


      // draw axis lines  

      scaleWave (UPPER_TIME_BOUND, LOWER_COUNT_BOUND, &pix_x[0], &pix_y[0], CZMIL_DEEP_CHANNEL, l_mapdef);

      l_map->drawLine (horLblSpace, h - verLblSpace, pix_x[0], h - verLblSpace, Qt::white, 1, NVFalse, Qt::SolidLine);

      scaleWave (LOWER_TIME_BOUND, UPPER_COUNT_BOUND, &pix_x[0], &pix_y[0], CZMIL_DEEP_CHANNEL, l_mapdef);

      l_map->drawLine (horLblSpace, h - verLblSpace, horLblSpace, pix_y[0], Qt::white, 1, NVFalse, Qt::SolidLine);
    }


  //
  // draw grid lines with text:  the 'Y' axis grid marks on the counts 
  // axis run down.  column '0' of the time axis.  The 'X' axis grid 
  // marks on the time axis run along the counts axis at row 0.  Here
  // we insert the ticks with numbers.
  //

  GetTickSpacing (lTimeBound[mapIndex], uTimeBound[mapIndex], lCountBound[mapIndex], uCountBound[mapIndex], timeSpacing, countSpacing, mapIndex);

  if (mapIndex == save_chan)
    {
      InsertGridTicks (lTimeBound[mapIndex], uTimeBound[mapIndex], 'X', timeSpacing, mapIndex, 0, l_map, highlightColor);
      InsertGridTicks (lCountBound[mapIndex], uCountBound[mapIndex], 'Y', countSpacing, mapIndex, 0, l_map, highlightColor);
    }
  else
    {
      InsertGridTicks (lTimeBound[mapIndex], uTimeBound[mapIndex], 'X', timeSpacing, mapIndex, 0, l_map, Qt::white);
      InsertGridTicks (lCountBound[mapIndex], uCountBound[mapIndex], 'Y', countSpacing, mapIndex, 0, l_map, Qt::white);
    }  
  l_map->setCursor (Qt::ArrowCursor);
}




void 
CZMILwaveMonitor::drawX (int32_t x, int32_t y, int32_t size, int32_t width, QColor color, nvMap * l_map)
{
  int32_t hs = size / 2;
  
  l_map->drawLine (x - hs, y + hs, x + hs, y - hs, color, width, NVTrue, Qt::SolidLine);
  l_map->drawLine (x + hs, y + hs, x - hs, y - hs, color, width, NVTrue, Qt::SolidLine);
}


void CZMILwaveMonitor::getWaveformData (int32_t mapIndex, uint16_t * &data, uint8_t * &ndx, CZMIL_CWF_Data * wData)
{
  data = wData->channel[mapIndex];
  ndx = wData->channel_ndx[mapIndex];
}


uint8_t CZMILwaveMonitor::trimWaveformNdxs (uint8_t * ndx, int32_t &start, int32_t &end)
{
  uint8_t valid = NVTrue;
  
  while (ndx[start] == 0)
    {
      start++;
      if (start == CZMIL_MAX_PACKETS) return NVFalse;
    }
  
  while (ndx[end] == 0)
    {
      end--;
      if (end < 0) return NVFalse;
    }
  
  return valid;
}




void CZMILwaveMonitor::drawSingleWave(nvMap *l_map, NVMAP_DEF &l_mapdef, CZMIL_CWF_Data * wData, int32_t mapIndex, int32_t mapLength, QColor waveColor )
{
  uint8_t previousValid = NVFalse, currentValid = NVFalse;
  
  int32_t pix_x[2], pix_y[2], scaledTimeValue, scaledCountValue;
  
  int32_t uTime = uTimeBound[mapIndex];
  
  int32_t lTime = lTimeBound[mapIndex];
  int32_t ndxStart = (lTime - 1) / 64;                 // time starts at 1
  int32_t ndxEnd = (uTime - 1) / 64;                        // time starts at 1
  
  uint16_t * dat = NULL;
  uint8_t * ndx = NULL;


  //  T0
  
  if (mapIndex == 9)
    {
      for (int32_t j = 0; j < 64; j++)
        {
          if ((wData->T0[j] >= lCountBound[mapIndex]) && (wData->T0[j] <= uCountBound[mapIndex]) && (j >= lTime) && (j <= uTime))
            {
              currentValid = NVTrue;
              
              scaledTimeValue = (int32_t) (((j - lTimeBound[mapIndex]) / ((float) (uTimeBound[mapIndex] - lTimeBound[mapIndex])) * (mapLength - 2)) + 1);
        
              scaledCountValue = (int32_t) ((wData->T0[j] - lCountBound[mapIndex]) / ((float) (uCountBound[mapIndex] - lCountBound[mapIndex])) * UPPER_COUNT_BOUND);
        
              scaleWave (scaledTimeValue, scaledCountValue, &pix_x[1], &pix_y[1], mapIndex, l_mapdef);
            }
          else
            {
              currentValid = NVFalse;
            }
      
      
            if (wave_line_mode)
              {
                if (currentValid && previousValid) l_map->drawLine (pix_x[0], pix_y[0], pix_x[1], pix_y[1], waveColor, 2, NVFalse, Qt::SolidLine);
              }
            else if (currentValid)
              {
                l_map->fillRectangle (pix_x[0], pix_y[0], SPOT_SIZE, SPOT_SIZE, waveColor, NVFalse);
              }

            pix_x[0] = pix_x[1];
            pix_y[0] = pix_y[1];
            previousValid = currentValid;
        }        
    }
  else
    {
      getWaveformData (mapIndex, dat, ndx, wData);
      trimWaveformNdxs (ndx, ndxStart, ndxEnd);

      for (int32_t i = ndxStart; i <= ndxEnd; i++)
        {
          if (!ndx[i])
            {
              previousValid = NVFalse;
              continue;
            }
    
          for (int32_t j = (i * 64); j < (i * 64 + 64); j++)
            {
              if ((dat[j] >= lCountBound[mapIndex]) && (dat[j] <= uCountBound[mapIndex]) && (j >= lTime) && (j <= uTime))
                {
                  currentValid = NVTrue;
              
                  scaledTimeValue = (int32_t) (((j - lTimeBound[mapIndex]) / ((float) (uTimeBound[mapIndex] - lTimeBound[mapIndex])) * (mapLength - 2)) + 1);
        
                  scaledCountValue = (int32_t) ((dat[j] - lCountBound[mapIndex]) / ((float) (uCountBound[mapIndex] - lCountBound[mapIndex])) * UPPER_COUNT_BOUND);
        
                  scaleWave (scaledTimeValue, scaledCountValue, &pix_x[1], &pix_y[1], mapIndex, l_mapdef);
                }
              else
                {
                  currentValid = NVFalse;
                }
      
      
              if (wave_line_mode)
                {
                  if (currentValid && previousValid) l_map->drawLine (pix_x[0], pix_y[0], pix_x[1], pix_y[1], waveColor, 2, NVFalse, Qt::SolidLine);
                }
              else if (currentValid)
                {
                  l_map->fillRectangle (pix_x[0], pix_y[0], SPOT_SIZE, SPOT_SIZE, waveColor, NVFalse);
                }
      
              pix_x[0] = pix_x[1];
              pix_y[0] = pix_y[1];
              previousValid = currentValid;
            }        
        }
    }
}



void CZMILwaveMonitor::drawSingleWave(nvMap *l_map, NVMAP_DEF &l_mapdef, uint16_t * dat, int32_t mapIndex, int32_t mapLength, QColor waveColor )
{
  uint8_t previousValid = NVFalse, currentValid = NVFalse;
  int32_t pix_x[2], pix_y[2], scaledTimeValue, scaledCountValue;
  int32_t uTime = uTimeBound[mapIndex];
  
  for (int32_t i = lTimeBound[mapIndex]; i <= uTime; i++)
    {
      if ((dat[i] >= lCountBound[mapIndex]) && (dat[i] <= uCountBound[mapIndex]))
        {
          currentValid = NVTrue;
          scaledTimeValue = (int32_t) (((i - lTimeBound[mapIndex]) / ((float) (uTimeBound[mapIndex] - lTimeBound[mapIndex])) * (mapLength - 2)) + 1);
      
          scaledCountValue = (int32_t) ((dat[i] - lCountBound[mapIndex]) / ((float) (uCountBound[mapIndex] -
                                                                                           lCountBound[mapIndex])) * UPPER_COUNT_BOUND);
      
          scaleWave (scaledTimeValue, scaledCountValue, &pix_x[1], &pix_y[1], mapIndex, l_mapdef);
        }
      else
        {
          currentValid = NVFalse;
        }
    
    
      if (wave_line_mode)
        {
          if (currentValid && previousValid) l_map->drawLine (pix_x[0], pix_y[0], pix_x[1], pix_y[1], waveColor, 2, NVFalse, Qt::SolidLine);
        }
      else
        {
          if (currentValid) l_map->fillRectangle (pix_x[0], pix_y[0], SPOT_SIZE, SPOT_SIZE, waveColor, NVFalse);
        }
    
      pix_x[0] = pix_x[1];
      pix_y[0] = pix_y[1];
      previousValid = currentValid;
    }        
}




void
CZMILwaveMonitor::slotRestoreDefaults ()
{        
  static uint8_t first = NVTrue;
  
  pos_format = "hdms";
  width = WAVE_X_SIZE;
  height = WAVE_Y_SIZE;
  window_x = 0;
  window_y = 0;

  
  //  Get the proper version based on the ABE_CZMIL environment variable.

  strcpy (program_version, VERSION);

  if (getenv ("ABE_CZMIL") != NULL)
    {
      if (strstr (getenv ("ABE_CZMIL"), "CZMIL"))
        {
          QString qv = QString (VERSION);
          QString str = qv.section (' ', 6, 8);

          str.prepend ("CME Software - CZMIL Waveform Viewer ");

          strcpy (program_version, str.toLatin1 ());
        }
      else
        {
          strcpy (program_version, VERSION);
        }
    }


  /* set some more variables to defaults in the event that this module is being
     run for the first time. */
  
  current_channel_mode = NVFalse;  // turn off single, current channel display
  wave_line_mode = NVTrue;         // turn on lines
  interest_point_mode = NVTrue;    // turn on interest points
  showIntMeter = 0;                // turn off intensity meter
  
  // make all panes visible
  
  for (int32_t i = 0; i < NUM_WAVEFORMS; i++) savedSettings.visible[i] = NVTrue;
  
  savedSettings.saveDirectory = savedSettings.openDirectory = ".";
  
  ccMode = 0;
  
  surfaceColor = Qt::yellow;
  primaryColor = Qt::green;
  
  /* we have a weird situation where first time the app is installed, primaryCursor's
     alpha value is 0, so we will force it to be totatlly opaque. */
  
  primaryColor.setAlpha (255);
  
  secondaryColor = Qt::red;
  highlightColor = Qt::red;
  backgroundColor = Qt::black;

  // added default color of reference overlays
  
  referenceColor = Qt::darkGray;
  
  startup_message = NVFalse;
  
  multiWaveMode = 1;
  

  //  The first time will be called from envin and the prefs dialog won't exist yet.
  
  if (!first) setFields ();

  first = NVFalse;
  

  force_redraw = NVTrue;
}




void
CZMILwaveMonitor::about ()
{
  QMessageBox::about (this, program_version, "CZMIL Waveform Viewer<br><br>Author : Gary Morris (USM)<br><br>" + mapText);
}




void
CZMILwaveMonitor::aboutQt ()
{
  QMessageBox::aboutQt (this, program_version);
}





//  Get the users defaults.

void
CZMILwaveMonitor::envin ()
{
  //  We need to get the font from the global settings.

#ifdef NVWIN3X
  QString ini_file2 = QString (getenv ("USERPROFILE")) + "/ABE.config/" + "globalABE.ini";
#else
  QString ini_file2 = QString (getenv ("HOME")) + "/ABE.config/" + "globalABE.ini";
#endif

  font = QApplication::font ();

  QSettings settings2 (ini_file2, QSettings::IniFormat);
  settings2.beginGroup ("globalABE");


  QString defaultFont = font.toString ();
  QString fontString = settings2.value (QString ("ABE map GUI font"), defaultFont).toString ();
  font.fromString (fontString);


  settings2.endGroup ();


  double saved_version = 4.04;
  
  
  // Set Defaults so the if keys don't exist the parms are defined
  
  slotRestoreDefaults ();
  force_redraw = NVFalse;


  //  Get the INI file name

#ifdef NVWIN3X
  QString ini_file = QString (getenv ("USERPROFILE")) + "/ABE.config/CZMILWaveMonitor.ini";
#else
  QString ini_file = QString (getenv ("HOME")) + "/ABE.config/CZMILWaveMonitor.ini";
#endif

  QSettings settings (ini_file, QSettings::IniFormat);
  settings.beginGroup (QString ("CZMILwaveMonitor"));

  saved_version = settings.value (QString ("settings version"), saved_version).toDouble ();

  //  If the settings version has changed we need to leave the values at the new defaults since they may have changed.

  if (settings_version != saved_version) return;

  pos_format = settings.value (QString ("position form"), pos_format).toString ();

  width = settings.value (QString ("width"), width).toInt ();

  height = settings.value (QString ("height"), height).toInt ();

  window_x = settings.value (QString ("window x"), window_x).toInt ();

  window_y = settings.value (QString ("window y"), window_y).toInt ();


  startup_message = settings.value (QString ("Display Startup Message"), startup_message).toBool ();
  
  
  current_channel_mode = settings.value (QString ("Current channel mode flag"), current_channel_mode).toBool ();
  
  wave_line_mode = settings.value (QString ("Wave line mode flag"), wave_line_mode).toBool ();
  
  interest_point_mode = settings.value (QString ("Interest point mode flag"), interest_point_mode).toBool ();
  
  // 
  // set the inensity meter flag in the settings as well as the multiwave
  // mode (0: NN, 1:SWM)
  //
  
  showIntMeter = settings.value (QString ("Intensity Meter"), showIntMeter).toBool();
  multiWaveMode = settings.value (QString ("Multiwave Mode"), multiWaveMode).toInt();
  
  for (int32_t i = 0; i < NUM_WAVEFORMS; i++)
    {
      QString label = QString ("Channel %1 Visible").arg(i);
      savedSettings.visible[i] = settings.value (label, true).toBool();
    }
  
  savedSettings.saveDirectory = settings.value ( QString ("saveDirectory"), QDir(".").canonicalPath() ).toString();
  savedSettings.openDirectory = settings.value ( QString ("openDirectory"), QDir(".").canonicalPath() ).toString();
  
  int32_t red, green, blue, alpha;
  
  red = settings.value (QString ("Surface color/red"), surfaceColor.red ()).toInt ();
  green = settings.value (QString ("Surface color/green"), surfaceColor.green ()).toInt ();
  blue = settings.value (QString ("Surface color/blue"), surfaceColor.blue ()).toInt ();
  alpha = settings.value (QString ("Surface color/alpha"), surfaceColor.alpha ()).toInt ();
  surfaceColor.setRgb (red, green, blue, alpha);
  
  red = settings.value (QString ("Primary color/red"), primaryColor.red ()).toInt ();
  green = settings.value (QString ("Primary color/green"), primaryColor.green ()).toInt ();
  blue = settings.value (QString ("Primary color/blue"), primaryColor.blue ()).toInt ();
  alpha = settings.value (QString ("Primary color/alpha"), primaryColor.alpha ()).toInt ();
  primaryColor.setRgb (red, green, blue, alpha);
  
  red = settings.value (QString ("Secondary color/red"), secondaryColor.red ()).toInt ();
  green = settings.value (QString ("Secondary color/green"), secondaryColor.green ()).toInt ();
  blue = settings.value (QString ("Secondary color/blue"), secondaryColor.blue ()).toInt ();
  alpha = settings.value (QString ("Secondary color/alpha"), secondaryColor.alpha ()).toInt ();
  secondaryColor.setRgb (red, green, blue, alpha);
  
  red = settings.value (QString ("Highlight color/red"), highlightColor.red ()).toInt ();
  green = settings.value (QString ("Highlight color/green"), highlightColor.green ()).toInt ();
  blue = settings.value (QString ("Highlight color/blue"), highlightColor.blue ()).toInt ();
  alpha = settings.value (QString ("Highlight color/alpha"), highlightColor.alpha ()).toInt ();
  highlightColor.setRgb (red, green, blue, alpha);
  
  red = settings.value (QString ("Background color/red"), backgroundColor.red ()).toInt ();
  green = settings.value (QString ("Background color/green"), backgroundColor.green ()).toInt ();
  blue = settings.value (QString ("Background color/blue"), backgroundColor.blue ()).toInt ();
  alpha = settings.value (QString ("Background color/alpha"), backgroundColor.alpha ()).toInt ();
  backgroundColor.setRgb (red, green, blue, alpha);
  
  // adding reference color as another thing to remember
  
  red = settings.value (QString ("Reference color/red"), referenceColor.red ()).toInt ();
  green = settings.value (QString ("Reference color/green"), referenceColor.green ()).toInt ();
  blue = settings.value (QString ("Reference color/blue"), referenceColor.blue ()).toInt ();
  alpha = settings.value (QString ("Reference color/alpha"), referenceColor.alpha ()).toInt ();
  referenceColor.setRgb (red, green, blue, alpha);
  
  // get the settings for the color code mode
  
  ccMode = settings.value (QString ("Color code setting"), 0).toInt ();
  
  this->restoreState (settings.value (QString ("main window state")).toByteArray (), (int32_t) (settings_version * 100.0));
  
  settings.endGroup ();
}




//  Save the users defaults.

void
CZMILwaveMonitor::envout ()
{
  //  Use frame geometry to get the absolute x and y.
  
  QRect tmp = this->frameGeometry ();
  
  window_x = tmp.x ();
  window_y = tmp.y ();
  
  
  //  Use geometry to get the width and height.
  
  tmp = this->geometry ();
  width = tmp.width ();
  height = tmp.height ();
  
  
  //  Get the INI file name

#ifdef NVWIN3X
  QString ini_file = QString (getenv ("USERPROFILE")) + "/ABE.config/CZMILWaveMonitor.ini";
#else
  QString ini_file = QString (getenv ("HOME")) + "/ABE.config/CZMILWaveMonitor.ini";
#endif

  QSettings settings (ini_file, QSettings::IniFormat);
  settings.beginGroup (QString ("CZMILwaveMonitor"));
  
  
  settings.setValue (QString ("settings version"), settings_version);
  
  settings.setValue (QString ("position form"), pos_format);
  
  settings.setValue (QString ("width"), width);
  
  settings.setValue (QString ("height"), height);
  
  settings.setValue (QString ("window x"), window_x);
  
  settings.setValue (QString ("window y"), window_y);
  
  
  settings.setValue (QString ("Display Startup Message"), startup_message);
  
  
  settings.setValue (QString ("Current channel mode flag"), current_channel_mode);
  
  settings.setValue (QString ("Wave line mode flag"), wave_line_mode);
  
  settings.setValue (QString ("Interest point mode flag"), interest_point_mode);
  
  settings.setValue (QString ("Intensity Meter"), showIntMeter);
  settings.setValue (QString ("Multiwave Mode"), multiWaveMode);


  for (int32_t i = 0; i < NUM_WAVEFORMS; i++)
    {
      QString label = QString ("Channel %1 Visible").arg(i);
      settings.setValue (label, GetMapVisibilityButtonState (i));
    }  

  
  settings.setValue (QString ("saveDirectory"), savedSettings.saveDirectory );
  settings.setValue (QString ("openDirectory"), savedSettings.openDirectory );
  
  settings.setValue (QString ("Surface color/red"), surfaceColor.red ());
  settings.setValue (QString ("Surface color/green"), surfaceColor.green ());
  settings.setValue (QString ("Surface color/blue"), surfaceColor.blue ());
  settings.setValue (QString ("Surface color/alpha"), surfaceColor.alpha ());
  
  settings.setValue (QString ("Primary color/red"), primaryColor.red ());
  settings.setValue (QString ("Primary color/green"), primaryColor.green ());
  settings.setValue (QString ("Primary color/blue"), primaryColor.blue ());
  settings.setValue (QString ("Primary color/alpha"), primaryColor.alpha ());
  
  settings.setValue (QString ("Secondary color/red"), secondaryColor.red ());
  settings.setValue (QString ("Secondary color/green"), secondaryColor.green ());
  settings.setValue (QString ("Secondary color/blue"), secondaryColor.blue ());
  settings.setValue (QString ("Secondary color/alpha"), secondaryColor.alpha ());
  
  settings.setValue (QString ("Highlight color/red"), highlightColor.red ());
  settings.setValue (QString ("Highlight color/green"), highlightColor.green ());
  settings.setValue (QString ("Highlight color/blue"), highlightColor.blue ());
  settings.setValue (QString ("Highlight color/alpha"), highlightColor.alpha ());
  
  settings.setValue (QString ("Background color/red"), backgroundColor.red ());
  settings.setValue (QString ("Background color/green"), backgroundColor.green ());
  settings.setValue (QString ("Background color/blue"), backgroundColor.blue ());
  settings.setValue (QString ("Background color/alpha"), backgroundColor.alpha ());
  
  // saving reference color
  
  settings.setValue (QString ("Reference color/red"), referenceColor.red ());
  settings.setValue (QString ("Reference color/green"), referenceColor.green ());
  settings.setValue (QString ("Reference color/blue"), referenceColor.blue ());
  settings.setValue (QString ("Reference color/alpha"), referenceColor.alpha ());
  
  // save the color code mode setting
  
  settings.setValue (QString ("Color code setting"), ccMode);
  
  settings.setValue (QString ("main window state"), this->saveState (NINT (settings_version * 100.0)));
  
  settings.endGroup ();
}
