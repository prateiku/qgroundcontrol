#include "QGCMapWidget.h"
#include "QGCMapToolbar.h"
#include "UASInterface.h"
#include "UASManager.h"
#include "MAV2DIcon.h"
#include "UASWaypointManager.h"

QGCMapWidget::QGCMapWidget(QWidget *parent) :
        mapcontrol::OPMapWidget(parent),
        currWPManager(NULL),
        firingWaypointChange(NULL)
{
    connect(UASManager::instance(), SIGNAL(UASCreated(UASInterface*)), this, SLOT(addUAS(UASInterface*)));
    connect(UASManager::instance(), SIGNAL(activeUASSet(UASInterface*)), this, SLOT(activeUASSet(UASInterface*)));
    foreach (UASInterface* uas, UASManager::instance()->getUASList())
    {
        addUAS(uas);
    }


    internals::PointLatLng pos_lat_lon = internals::PointLatLng(0, 0);

    //        // **************
    //        // default home position

    //        home_position.coord = pos_lat_lon;
    //        home_position.altitude = altitude;
    //        home_position.locked = false;

    //        // **************
    //        // default magic waypoint params

    //        magic_waypoint.map_wp_item = NULL;
    //        magic_waypoint.coord = home_position.coord;
    //        magic_waypoint.altitude = altitude;
    //        magic_waypoint.description = "Magic waypoint";
    //        magic_waypoint.locked = false;
    //        magic_waypoint.time_seconds = 0;
    //        magic_waypoint.hold_time_seconds = 0;

    const int safe_area_radius_list[] = {5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};   // meters

    const int uav_trail_time_list[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};                      // seconds

    const int uav_trail_distance_list[] = {1, 2, 5, 10, 20, 50, 100, 200, 500};             // meters

    SetMouseWheelZoomType(internals::MouseWheelZoomType::MousePositionWithoutCenter);	    // set how the mouse wheel zoom functions
    SetFollowMouse(true);				    // we want a contiuous mouse position reading

    SetShowHome(true);					    // display the HOME position on the map
    SetShowUAV(true);					    // display the UAV position on the map
    //SetShowDiagnostics(true); // Not needed in flight / production mode

    Home->SetSafeArea(safe_area_radius_list[0]);                         // set radius (meters)
    Home->SetShowSafeArea(true);                                         // show the safe area

    UAV->SetTrailTime(uav_trail_time_list[0]);                           // seconds
    UAV->SetTrailDistance(uav_trail_distance_list[1]);                   // meters

    // UAV->SetTrailType(UAVTrailType::ByTimeElapsed);
    //  UAV->SetTrailType(UAVTrailType::ByDistance);

    GPS->SetTrailTime(uav_trail_time_list[0]);                           // seconds
    GPS->SetTrailDistance(uav_trail_distance_list[1]);                   // meters

    // GPS->SetTrailType(UAVTrailType::ByTimeElapsed);

    SetCurrentPosition(pos_lat_lon);         // set the map position
    Home->SetCoord(pos_lat_lon);             // set the HOME position
    UAV->SetUAVPos(pos_lat_lon, 0.0);        // set the UAV position
    GPS->SetUAVPos(pos_lat_lon, 0.0);        // set the UAV position

    setFrameStyle(QFrame::NoFrame);      // no border frame
    setBackgroundBrush(QBrush(Qt::black)); // tile background

    // Set current home position
    updateHomePosition(UASManager::instance()->getHomeLatitude(), UASManager::instance()->getHomeLongitude(), UASManager::instance()->getHomeAltitude());

    // Set currently selected system
    activeUASSet(UASManager::instance()->getActiveUAS());

    // FIXME XXX this is a hack to trick OPs current 1-system design
    SetShowUAV(false);


    // Connect map updates to the adapter slots
    connect(this, SIGNAL(WPValuesChanged(WayPointItem*)), this, SLOT(handleMapWaypointEdit(WayPointItem*)));


    setFocus();
}

QGCMapWidget::~QGCMapWidget()
{
    SetShowHome(false);	// doing this appears to stop the map lib crashing on exit
    SetShowUAV(false);	//   "          "
}

/**
 *
 * @param uas the UAS/MAV to monitor/display with the HUD
 */
void QGCMapWidget::addUAS(UASInterface* uas)
{
    qDebug() << "ADDING UAS";
    connect(uas, SIGNAL(globalPositionChanged(UASInterface*,double,double,double,quint64)), this, SLOT(updateGlobalPosition(UASInterface*,double,double,double,quint64)));
    //connect(uas, SIGNAL(attitudeChanged(UASInterface*,double,double,double,quint64)), this, SLOT(updateAttitude(UASInterface*,double,double,double,quint64)));
    connect(uas, SIGNAL(systemSpecsChanged(int)), this, SLOT(updateSystemSpecs(int)));
}

void QGCMapWidget::activeUASSet(UASInterface* uas)
{
    // Only execute if proper UAS is set
    if (!uas || !dynamic_cast<UASInterface*>(uas)) return;

    // Disconnect old MAV manager
    if (currWPManager) {
        // Disconnect the waypoint manager / data storage from the UI
        disconnect(currWPManager, SIGNAL(waypointListChanged(int)), this, SLOT(updateWaypointList(int)));
        disconnect(currWPManager, SIGNAL(waypointChanged(int, Waypoint*)), this, SLOT(updateWaypoint(int,Waypoint*)));
        disconnect(this, SIGNAL(waypointCreated(Waypoint*)), currWPManager, SLOT(addWaypoint(Waypoint*)));
        disconnect(this, SIGNAL(waypointChanged(Waypoint*)), currWPManager, SLOT(notifyOfChange(Waypoint*)));
    }

    if (uas) {
        currWPManager = uas->getWaypointManager();
//        QColor color = mav->getColor();
//        color.setAlphaF(0.9);
//        QPen* pen = new QPen(color);
//        pen->setWidth(3.0);
//        mavPens.insert(mav->getUASID(), pen);
//        // FIXME Remove after refactoring
//        waypointPath->setPen(pen);

        // Delete all waypoints and add waypoint from new system
        updateWaypointList(uas->getUASID());

        // Connect the waypoint manager / data storage to the UI
        connect(currWPManager, SIGNAL(waypointListChanged(int)), this, SLOT(updateWaypointList(int)));
        connect(currWPManager, SIGNAL(waypointChanged(int, Waypoint*)), this, SLOT(updateWaypoint(int,Waypoint*)));
        connect(this, SIGNAL(waypointCreated(Waypoint*)), currWPManager, SLOT(addWaypoint(Waypoint*)));
        connect(this, SIGNAL(waypointChanged(Waypoint*)), currWPManager, SLOT(notifyOfChange(Waypoint*)));
        updateSelectedSystem(uas->getUASID());
    }
}

/**
 * Updates the global position of one MAV and append the last movement to the trail
 *
 * @param uas The unmanned air system
 * @param lat Latitude in WGS84 ellipsoid
 * @param lon Longitutde in WGS84 ellipsoid
 * @param alt Altitude over mean sea level
 * @param usec Timestamp of the position message in milliseconds FIXME will move to microseconds
 */
void QGCMapWidget::updateGlobalPosition(UASInterface* uas, double lat, double lon, double alt, quint64 usec)
{
    Q_UNUSED(usec);

    // Get reference to graphic UAV item
    mapcontrol::UAVItem* uav = GetUAV(uas->getUASID());
    // Check if reference is valid, else create a new one
    if (uav == NULL)
    {
        MAV2DIcon* newUAV = new MAV2DIcon(map, this, uas);
        newUAV->setParentItem(map);
        UAVS.insert(uas->getUASID(), newUAV);
        uav = GetUAV(uas->getUASID());
    }

    // Set new lat/lon position of UAV icon
    internals::PointLatLng pos_lat_lon = internals::PointLatLng(lat, lon);
    uav->SetUAVPos(pos_lat_lon, alt);
    // Convert from radians to degrees and apply
    uav->SetUAVHeading((uas->getYaw()/M_PI)*180.0f);
}


void QGCMapWidget::updateSystemSpecs(int uas)
{
    foreach (mapcontrol::UAVItem* p, UAVS.values())
    {
        MAV2DIcon* icon = dynamic_cast<MAV2DIcon*>(p);
        if (icon && icon->getUASId() == uas)
        {
            // Set new airframe
            icon->setAirframe(UASManager::instance()->getUASForId(uas)->getAirframe());
            icon->drawIcon();
        }
    }
}

/**
 * Does not update the system type or configuration, only the selected status
 */
void QGCMapWidget::updateSelectedSystem(int uas)
{
    foreach (mapcontrol::UAVItem* p, UAVS.values())
    {
        MAV2DIcon* icon = dynamic_cast<MAV2DIcon*>(p);
        if (icon)
        {
            // Set as selected if ids match
            icon->setSelectedUAS((icon->getUASId() == uas));
        }
    }
}


// MAP NAVIGATION
void QGCMapWidget::showGoToDialog()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Please enter coordinates"),
                                         tr("Coordinates (Lat,Lon):"), QLineEdit::Normal,
                                         QString("%1,%2").arg(CurrentPosition().Lat()).arg( CurrentPosition().Lng()), &ok);
    if (ok && !text.isEmpty()) {
        QStringList split = text.split(",");
        if (split.length() == 2) {
            bool convert;
            double latitude = split.first().toDouble(&convert);
            ok &= convert;
            double longitude = split.last().toDouble(&convert);
            ok &= convert;

            if (ok) {
                internals::PointLatLng pos_lat_lon = internals::PointLatLng(latitude, longitude);
                SetCurrentPosition(pos_lat_lon);        // set the map position
            }
        }
    }
}


void QGCMapWidget::updateHomePosition(double latitude, double longitude, double altitude)
{
    Home->SetCoord(internals::PointLatLng(latitude, longitude));
    Home->SetAltitude(altitude);
    SetShowHome(true);                      // display the HOME position on the map
}


// WAYPOINT MAP INTERACTION FUNCTIONS

//void QGCMapWidget::createWaypointAtMousePos(QMouseEvent)
//{

//}

void QGCMapWidget::handleMapWaypointEdit(mapcontrol::WayPointItem* waypoint)
{
    qDebug() << "UPDATING WP FROM MAP";
    // Block circle updates
    Waypoint* wp = iconsToWaypoints.value(waypoint, NULL);
    // Protect from vicious double update cycle
    if (firingWaypointChange == wp || !wp) return;
    // Not in cycle, block now from entering it
    firingWaypointChange = wp;

    // Update WP values
    internals::PointLatLng pos = waypoint->Coord();
    wp->setLatitude(pos.Lat());
    wp->setLongitude(pos.Lng());
    wp->setAltitude(waypoint->Altitude());

    emit waypointChanged(wp);
    firingWaypointChange = NULL;
}

// WAYPOINT UPDATE FUNCTIONS

/**
 * This function is called if a a single waypoint is updated and
 * also if the whole list changes.
 */
void QGCMapWidget::updateWaypoint(int uas, Waypoint* wp)
{
    // Source of the event was in this widget, do nothing
    if (firingWaypointChange == wp) return;
        // Currently only accept waypoint updates from the UAS in focus
        // this has to be changed to accept read-only updates from other systems as well.
        if (UASManager::instance()->getUASForId(uas)->getWaypointManager() == currWPManager) {
            // Only accept waypoints in global coordinate frame
            if (wp->getFrame() == MAV_FRAME_GLOBAL && wp->isNavigationType()) {
                // We're good, this is a global waypoint

                // Get the index of this waypoint
                // note the call to getGlobalFrameAndNavTypeIndexOf()
                // as we're only handling global waypoints
                int wpindex = UASManager::instance()->getUASForId(uas)->getWaypointManager()->getGlobalFrameAndNavTypeIndexOf(wp);
                // If not found, return (this should never happen, but helps safety)
                if (wpindex == -1) return;
                // Mark this wp as currently edited
                firingWaypointChange = wp;

                // Check if wp exists yet in map
                if (!waypointsToIcons.contains(wp)) {
                    // Create icon for new WP
                    mapcontrol::WayPointItem* icon = WPCreate(internals::PointLatLng(wp->getLatitude(), wp->getLongitude()), wp->getAltitude(), wp->getDescription());
                    icon->SetHeading(wp->getYaw());
                    // Update maps to allow inverse data association
                    waypointsToIcons.insert(wp, icon);
                    iconsToWaypoints.insert(icon, wp);
                } else {
                    // Waypoint exists, block it's signals and update it
                    mapcontrol::WayPointItem* icon = waypointsToIcons.value(wp);
                    // Make sure we don't die on a null pointer
                    // this should never happen, just a precaution
                    if (!icon) return;
                    // Block outgoing signals to prevent an infinite signal loop
                    // should not happen, just a precaution
                    this->blockSignals(true);
                    // Update the WP
                    icon->SetCoord(internals::PointLatLng(wp->getLatitude(), wp->getLongitude()));
                    icon->SetAltitude(wp->getAltitude());
                    icon->SetHeading(wp->getYaw());
                    icon->SetNumber(wp->getId());
                    // Re-enable signals again
                    this->blockSignals(false);
                }
                firingWaypointChange = NULL;

            } else {
                // Check if the index of this waypoint is larger than the global
                // waypoint list. This implies that the coordinate frame of this
                // waypoint was changed and the list containing only global
                // waypoints was shortened. Thus update the whole list
                if (waypointsToIcons.size() > UASManager::instance()->getUASForId(uas)->getWaypointManager()->getGlobalFrameAndNavTypeCount()) {
                    updateWaypointList(uas);
                }
            }
        }
}

/**
 * Update the whole list of waypoints. This is e.g. necessary if the list order changed.
 * The UAS manager will emit the appropriate signal whenever updating the list
 * is necessary.
 */
void QGCMapWidget::updateWaypointList(int uas)
{
    // Currently only accept waypoint updates from the UAS in focus
    // this has to be changed to accept read-only updates from other systems as well.
    if (UASManager::instance()->getUASForId(uas)->getWaypointManager() == currWPManager) {
    qDebug() << "UPDATING WP LIST";
    // Get current WP list
    // compare to local WP maps and
    // update / remove all WPs

//    int localCount = waypointsToIcons.count();

    // ORDER MATTERS HERE!
    // TWO LOOPS ARE NEEDED - INFINITY LOOP ELSE

    // Delete first all old waypoints
    // this is suboptimal (quadratic, but wps should stay in the sub-100 range anyway)
    QVector<Waypoint* > wps = currWPManager->getGlobalFrameAndNavTypeWaypointList();
    foreach (Waypoint* wp, waypointsToIcons.keys())
    {
        // Get icon to work on
        mapcontrol::WayPointItem* icon = waypointsToIcons.value(wp);
        if (!wps.contains(wp))
        {
            waypointsToIcons.remove(wp);
            iconsToWaypoints.remove(icon);
            delete icon;
            icon = NULL;
        }
    }

    // Update all existing waypoints
    foreach (Waypoint* wp, waypointsToIcons.keys())
    {
        // Update remaining waypoints
        updateWaypoint(uas, wp);
    }

    // Update all potentially new waypoints
    foreach (Waypoint* wp, wps)
    {
        // Update / add only if new
        if (!waypointsToIcons.contains(wp)) updateWaypoint(uas, wp);
    }

//    int globalCount = uasInstance->getWaypointManager()->getGlobalFrameAndNavTypeCount();

//        // Get already existing waypoints
//        UASInterface* uasInstance = UASManager::instance()->getUASForId(uas);
//        if (uasInstance) {
//            // Get update rect of old content, this is what will be redrawn
//            // in the last step
//            QRect updateRect = waypointPath->boundingBox().toRect();

//            // Get all waypoints, including non-global waypoints
//            QVector<Waypoint*> wpList = uasInstance->getWaypointManager()->getWaypointList();

//            // Clear if necessary
//            if (wpList.count() == 0) {
//                clearWaypoints(uas);
//                return;
//            }

//            // Trim internal list to number of global waypoints in the waypoint manager list
//            int overSize = waypointPath->points().count() - uasInstance->getWaypointManager()->getGlobalFrameAndNavTypeCount();
//            if (overSize > 0) {
//                // Remove n waypoints at the end of the list
//                // the remaining waypoints will be updated
//                // in the next step
//                for (int i = 0; i < overSize; ++i) {
//                    wps.removeLast();
//                    mc->layer("Waypoints")->removeGeometry(wpIcons.last());
//                    wpIcons.removeLast();
//                    waypointPath->points().removeLast();
//                }
//            }

//            // Load all existing waypoints into map view
//            foreach (Waypoint* wp, wpList) {
//                // Block map draw updates, since we update everything in the next step
//                // but update internal data structures.
//                // Please note that updateWaypoint() ignores non-global waypoints
//                updateWaypoint(mav->getUASID(), wp, false);
//            }

//            // Update view
//            if (isVisible()) mc->updateRequest(updateRect);
//        }
                        }
}
