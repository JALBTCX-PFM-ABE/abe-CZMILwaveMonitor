
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

QString prefsText = 
  CZMILwaveMonitor::tr ("<img source=\":/icons/prefs.png\"> Click this button to change program preferences.  This includes "
			"position format and the colors.");
QString currentChanText = 
  CZMILwaveMonitor::tr ("Click this button to toggle between displaying just the waveform from the channel for the currently "
                        "active point in pfmEdit3D and displaying all of the channels for the currently active point in "
                        "pfmEdit3D.<br><br>"
                        "You may also use the hot key shown at the end of the label to toggle this setting.");

QString modeText = 
  CZMILwaveMonitor::tr ("<img source=\":/icons/mode_line.png\"> <img source=\":/icons/mode_dot.png\"> Click this button to "
			"toggle between line and dot drawing modes for the wave display.  When selected the waves are drawn "
			"as lines, when unselected the waves are drawn as unconnected dots.");

QString horVerText = CZMILwaveMonitor::tr ("<img source =\":/icons/verArrow.png\"><img source=\":/icons/horArrow.png\">Click "
					   "this button to toggle between displaying the waveform signatures either vertically "
					   "or horizontally.  When selected, the signatures run vertical, when unselected, the "
					   "signatures run horizontal.");

QString intMeterText = CZMILwaveMonitor::tr ("<img source=\":/icons/ColumnChart.png\"> Click this button to activate an "
					     "interactive count meter that will slice through a bin of all waveform signatures "
					     "(APD, IR, RAMAN, and PMT).  The intensity meter provides information about the "
					     "counts of all waveforms at a time bin of interest.");

QString quitText = 
  CZMILwaveMonitor::tr ("<img source=\":/icons/quit.png\"> Click this button to <b><em>exit</em></b> from the program.  "
			"You can also use the <b>Quit</b> entry in the <b>File</b> menu.");
QString mapText = 
  CZMILwaveMonitor::tr ("This is the CZMILwaveMonitor program, a companion to the pfmEdit3D program for viewing CZMIL "
			"LIDAR waveforms.  There are a number of action keys that may be pressed while in the "
			"pfmEdit3D window.  The default actions and key values are :<br><br>"
			"<ul>"
			"<li>n - Toggle between nearest neighbor and single waveform display mode</li>"
			"<li>0 - Toggle display of Deep waveform</li>"
			"<li>1 - Toggle display of Shallow Channel 1 waveform</li>"
			"<li>2 - Toggle display of Shallow Channel 2 waveform</li>"
			"<li>3 - Toggle display of Shallow Channel 3 waveform</li>"
			"<li>4 - Toggle display of Shallow Channel 4 waveform</li>"
			"<li>5 - Toggle display of Shallow Channel 5 waveform</li>"
			"<li>6 - Toggle display of Shallow Channel 6 waveform</li>"
			"<li>7 - Toggle display of Shallow Channel 7 waveform</li>"
			"<li>8 - Toggle display of IR waveform</li>"
			"<li>9 - Toggle display of T0 waveform</li>"
			"<li>c - Toggle <b>Current Channel</b> mode</li>"
			"<li>f - Toggle freeze of the waveform (freezes all external programs from pfmEdit)</li>"
			"<li>F - Toggle freeze of all of the waveforms (freezes all external programs from pfmEdit)</li>"
			"</ul>"
			"The actual key values may be changed in pfmEdit3D using the Preferences->Ancillary Programs "
			"dialog.<br><br>"
                        "In addition to the above keys there are two keys that will work from the CZMILwaveMonitor window. "
                        "These are the <b>+</b> and <b>-</b> keys. The <b>+</b> key will move forward in the CPF and CWF files by "
                        "one record.  The <b>-</b> key will move back in the CPF file by one record.<br><br>"
                        "Help is available on all fields in CZMILwaveMonitor using the What's This pointer.");

QString bGrpText = 
  CZMILwaveMonitor::tr ("Select the format in which you want all geographic positions to be displayed.");

QString closePrefsText = 
  CZMILwaveMonitor::tr ("Click this button to close the preferences dialog.");

QString restoreDefaultsText = 
  CZMILwaveMonitor::tr ("Click this button to restore colors, size, and position format to the default settings.");

QString nameLabelText = 
  CZMILwaveMonitor::tr ("This is the name of the input file from which this point was taken");


QString recordLabelText = 
  CZMILwaveMonitor::tr ("This is the record number of the record from which this point was taken.<br><br>"
                        "<b>IMPORTANT NOTE: You can use the + and - keys in this application to move forward and "
                        "backward through the file.  The record will reset to the nearest record in pfmEdit3D "
                        "when you move the cursor in pfmEdit3D.</b>");

QString correctDepthText = 
  CZMILwaveMonitor::tr ("This is the final depth after tide (DGPS) or datum (KGPS) corrections.  Note that these "
			"corrections may not have been made yet.  Check the tide/datum field.");

QString simDepthText =
  CZMILwaveMonitor::tr("This field is simply the simulated depth field.");

QString simDepth1Text =
  CZMILwaveMonitor::tr("This field is simply the simulated depth1 field.");

QString simDepth2Text =
  CZMILwaveMonitor::tr("This field is simply the simulated depth2 field.");

QString simKGPSElevText =
  CZMILwaveMonitor::tr("This field is simply the simulated KGPS elevation field.");

QString reportedDepthText = 
  CZMILwaveMonitor::tr ("This is the depth without the tide/datum correction.  This can come from either the "
			"kgps_elevation field or the result_depth field depending on whether the survey was done "
			"in KGPS or DGPS mode respectively.");

QString deepChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Deep</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString shal1ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Shal1</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString shal2ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Shal2</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString shal3ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Shal3</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString shal4ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Shal4</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString shal5ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Shal5</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString shal6ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Shal6</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString shal7ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>Shal7</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString irChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>IR</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

QString t0ChanText = 
  CZMILwaveMonitor::tr ("Check this entry to hide or show the <b>T0</b> pane. You may also use the hot key shown at the "
                        "end of the label to toggle this setting.");

