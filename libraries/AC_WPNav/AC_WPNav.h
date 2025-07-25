#pragma once

#include <AP_Common/AP_Common.h>
#include <AP_Param/AP_Param.h>
#include <AP_Math/AP_Math.h>
#include <AP_Math/SCurve.h>
#include <AP_Math/SplineCurve.h>
#include <AP_Common/Location.h>
#include <AC_AttitudeControl/AC_PosControl.h>      // Position control library
#include <AC_AttitudeControl/AC_AttitudeControl.h> // Attitude control library
#include <AP_Terrain/AP_Terrain.h>
#include <AC_Avoidance/AC_Avoid.h>                 // Stop at fence library

// maximum velocities and accelerations
#define WPNAV_ACCELERATION              250.0f      // maximum horizontal acceleration in cm/s/s that wp navigation will request

class AC_WPNav
{
public:

    /// Constructor
    AC_WPNav(const AP_AHRS_View& ahrs, AC_PosControl& pos_control, const AC_AttitudeControl& attitude_control);

    /// provide rangefinder based terrain offset
    /// terrain offset is the terrain's height above the EKF origin
    void set_rangefinder_terrain_offset_cm(bool use, bool healthy, float terrain_offset_cm) { _rangefinder_available = use; _rangefinder_healthy = healthy; _rangefinder_terrain_offset_cm = terrain_offset_cm;}

    // return true if range finder may be used for terrain following
    bool rangefinder_used() const { return _rangefinder_use; }
    bool rangefinder_used_and_healthy() const { return _rangefinder_use && _rangefinder_healthy; }

    // get expected source of terrain data if alt-above-terrain command is executed (used by Copter's ModeRTL)
    enum class TerrainSource {
        TERRAIN_UNAVAILABLE,
        TERRAIN_FROM_RANGEFINDER,
        TERRAIN_FROM_TERRAINDATABASE,
    };
    AC_WPNav::TerrainSource get_terrain_source() const;

    // get terrain's altitude (in cm above the ekf origin) at the current position (+ve means terrain below vehicle is above ekf origin's altitude)
    bool get_terrain_offset_cm(float& offset_cm);

    // return terrain following altitude margin.  vehicle will stop if distance from target altitude is larger than this margin
    float get_terrain_margin_m() const { return MAX(_terrain_margin_m, 0.1); }

    // convert location to vector from ekf origin.  terrain_alt is set to true if resulting vector's z-axis should be treated as alt-above-terrain
    //      returns false if conversion failed (likely because terrain data was not available)
    bool get_vector_NEU_cm(const Location &loc, Vector3f &pos_from_origin_NEU_cm, bool &terrain_alt);

    ///
    /// waypoint controller
    ///

    /// wp_and_spline_init_cm - initialise straight line and spline waypoint controllers
    ///     speed_cms is the desired max speed to travel between waypoints.  should be a positive value or omitted to use the default speed
    ///     updates target roll, pitch targets and I terms based on vehicle lean angles
    ///     should be called once before the waypoint controller is used but does not need to be called before subsequent updates to destination
    void wp_and_spline_init_cm(float speed_cms = 0.0f, Vector3f stopping_point = Vector3f{});

    /// set current target horizontal speed during wp navigation
    void set_speed_NE_cms(float speed_cms);

    /// set pause or resume during wp navigation
    void set_pause() { _paused = true; }
    void set_resume() { _paused = false; }

    /// get paused status
    bool paused() { return _paused; }

    /// set current target climb or descent rate during wp navigation
    void set_speed_up_cms(float speed_up_cms);
    void set_speed_down_cms(float speed_down_cms);

    /// get default target horizontal velocity during wp navigation
    float get_default_speed_NE_cms() const { return _wp_speed_cms; }

    /// get default target climb speed in cm/s during missions
    float get_default_speed_up_cms() const { return _wp_speed_up_cms; }

    /// get default target descent rate in cm/s during missions.  Note: always positive
    float get_default_speed_down_cms() const { return fabsf(_wp_speed_down_cms); }

    /// get_accel_U_cmss - returns vertical acceleration in cm/s/s during missions.  Note: always positive
    float get_accel_U_cmss() const { return _wp_accel_z_cmss; }

    /// get_wp_acceleration - returns acceleration in cm/s/s during missions
    float get_wp_acceleration_cmss() const { return (is_positive(_wp_accel_cmss)) ? _wp_accel_cmss : WPNAV_ACCELERATION; }

    /// get_corner_acceleration_cmss - returns maximum acceleration in cm/s/s used during cornering in missions
    float get_corner_acceleration_cmss() const { return (is_positive(_wp_accel_c_cmss)) ? _wp_accel_c_cmss : 2.0 * get_wp_acceleration_cmss(); }

    /// get_wp_destination_NEU_cm waypoint using position vector
    /// x,y are distance from ekf origin in cm
    /// z may be cm above ekf origin or terrain (see origin_and_destination_are_terrain_alt method)
    const Vector3f &get_wp_destination_NEU_cm() const { return _destination_neu_cm; }

    /// get origin using position vector (distance from ekf origin in cm)
    const Vector3f &get_wp_origin_NEU_cm() const { return _origin_neu_cm; }

    /// true if origin.z and destination.z are alt-above-terrain, false if alt-above-ekf-origin
    bool origin_and_destination_are_terrain_alt() const { return _terrain_alt; }

    /// set_wp_destination_NEU_cm waypoint using location class
    ///     provide the next_destination if known
    ///     returns false if conversion from location to vector from ekf origin cannot be calculated
    bool set_wp_destination_loc(const Location& destination);
    bool set_wp_destination_next_loc(const Location& destination);

    // get destination as a location.  Altitude frame will be absolute (AMSL) or above terrain
    // returns false if unable to return a destination (for example if origin has not yet been set)
    bool get_wp_destination_loc(Location& destination) const;

    // returns object avoidance adjusted destination which is always the same as get_wp_destination_NEU_cm
    // having this function unifies the AC_WPNav_OA and AC_WPNav interfaces making vehicle code simpler
    virtual bool get_oa_wp_destination(Location& destination) const { return get_wp_destination_loc(destination); }

    /// set_wp_destination_NEU_cm waypoint using position vector (distance from ekf origin in cm)
    ///     terrain_alt should be true if destination.z is a desired altitude above terrain
    virtual bool set_wp_destination_NEU_cm(const Vector3f& destination_neu_cm, bool terrain_alt = false);
    bool set_wp_destination_next_NEU_cm(const Vector3f& destination_neu_cm, bool terrain_alt = false);

    /// set waypoint destination using NED position vector from ekf origin in meters
    ///     provide next_destination_NED if known
    bool set_wp_destination_NED_cm(const Vector3f& destination_NED_cm);
    bool set_wp_destination_next_NED_cm(const Vector3f& destination_NED_cm);

    /// shifts the origin and destination horizontally to the current position
    ///     used to reset the track when taking off without horizontal position control
    ///     relies on set_wp_destination_NEU_cm or set_wp_origin_and_destination having been called first
    void shift_wp_origin_and_destination_to_current_pos_NE(); // todo: Not used

    /// shifts the origin and destination horizontally to the achievable stopping point
    ///     used to reset the track when horizontal navigation is enabled after having been disabled (see Copter's wp_navalt_min)
    ///     relies on set_wp_destination_NEU_cm or set_wp_origin_and_destination having been called first
    void shift_wp_origin_and_destination_to_stopping_point_NE(); // todo: Not used

    /// get_wp_stopping_point_cm - calculates stopping point based on current position, velocity, waypoint acceleration
    ///		results placed in stopping_position vector
    void get_wp_stopping_point_NE_cm(Vector2f& stopping_point_NE_cm) const;
    void get_wp_stopping_point_NEU_cm(Vector3f& stopping_point) const;

    /// get_wp_distance_to_destination - get horizontal distance to destination in cm
    virtual float get_wp_distance_to_destination_cm() const;

    /// get_bearing_to_destination - get bearing to next waypoint in centi-degrees
    virtual int32_t get_wp_bearing_to_destination_cd() const;

    /// reached_destination - true when we have come within RADIUS cm of the waypoint
    virtual bool reached_wp_destination() const { return _flags.reached_destination; }

    // reached_wp_destination_NE - true if within RADIUS_CM of waypoint in x/y
    bool reached_wp_destination_NE() const {
        return get_wp_distance_to_destination_cm() < _wp_radius_cm;
    }

    // get wp_radius parameter value in cm
    float get_wp_radius_cm() const { return _wp_radius_cm; }

    /// update_wpnav - run the wp controller - should be called at 100hz or higher
    virtual bool update_wpnav();

    // returns true if update_wpnav has been run very recently
    bool is_active() const;

    // force stopping at next waypoint.  Used by Dijkstra's object avoidance when path from destination to next destination is not clear
    // only affects regular (e.g. non-spline) waypoints
    // returns true if this had any affect on the path
    bool force_stop_at_next_wp();

    ///
    /// spline methods
    ///

    /// set_spline_destination_NEU_cm waypoint using location class
    ///     returns false if conversion from location to vector from ekf origin cannot be calculated
    ///     next_destination should be the next segment's destination
    ///     next_is_spline should be true if next_destination is a spline segment
    bool set_spline_destination_loc(const Location& destination, const Location& next_destination, bool next_is_spline);

    /// set next destination (e.g. the one after the current destination) as a spline segment specified as a location
    ///     returns false if conversion from location to vector from ekf origin cannot be calculated
    ///     next_next_destination should be the next segment's destination
    ///     next_next_is_spline should be true if next_next_destination is a spline segment
    bool set_spline_destination_next_loc(const Location& next_destination, const Location& next_next_destination, bool next_next_is_spline);

    /// set_spline_destination_NEU_cm waypoint using position vector (distance from ekf origin in cm)
    ///     terrain_alt should be true if destination.z is a desired altitude above terrain (false if its desired altitudes above ekf origin)
    ///     next_destination is the next segment's destination
    ///     next_terrain_alt should be true if next_destination.z is a desired altitude above terrain (false if its desired altitudes above ekf origin)
    ///     next_destination.z must be in the same "frame" as destination.z (i.e. if destination is a alt-above-terrain, next_destination must be too)
    ///     next_is_spline should be true if next_destination is a spline segment
    bool set_spline_destination_NEU_cm(const Vector3f& destination_neu_cm, bool terrain_alt, const Vector3f& next_destination_neu_cm, bool next_terrain_alt, bool next_is_spline);

    /// set next destination (e.g. the one after the current destination) as an offset (in cm, NEU frame) from the EKF origin
    ///     next_terrain_alt should be true if next_destination.z is a desired altitude above terrain (false if its desired altitudes above ekf origin)
    ///     next_next_destination is the next segment's destination
    ///     next_next_terrain_alt should be true if next_next_destination.z is a desired altitude above terrain (false if it is desired altitude above ekf origin)
    ///     next_next_destination.z must be in the same "frame" as destination.z (i.e. if next_destination is a alt-above-terrain, next_next_destination must be too)
    ///     next_next_is_spline should be true if next_next_destination is a spline segment
    bool set_spline_destination_next_NEU_cm(const Vector3f& next_destination_neu_cm, bool next_terrain_alt, const Vector3f& next_next_destination_neu_cm, bool next_next_terrain_alt, bool next_next_is_spline);

    ///
    /// shared methods
    ///

    /// Returns the desired roll angle in radians from the position controller.
    float get_roll_rad() const { return _pos_control.get_roll_rad(); }

    /// Returns the desired pitch angle in radians from the position controller.
    float get_pitch_rad() const { return _pos_control.get_pitch_rad(); }

    /// Returns the desired yaw target in radians from the position controller.
    float get_yaw_rad() const { return _pos_control.get_yaw_rad(); }

    /// Returns the desired thrust direction vector for tilt control from the position controller.
    Vector3f get_thrust_vector() const { return _pos_control.get_thrust_vector(); }

    /// Returns the desired roll angle in centidegrees from the position controller.
    float get_roll() const { return _pos_control.get_roll_cd(); }

    /// Returns the desired pitch angle in centidegrees from the position controller.
    float get_pitch() const { return _pos_control.get_pitch_cd(); }

    /// Returns the desired yaw target in centidegrees from the position controller.
    float get_yaw() const { return _pos_control.get_yaw_cd(); }

    /// advance_wp_target_along_track - move target location along track from origin to destination
    bool advance_wp_target_along_track(float dt);

    /// recalculate path with update speed and/or acceleration limits
    void update_track_with_speed_accel_limits();

    /// return the crosstrack_error - horizontal error of the actual position vs the desired position
    float crosstrack_error() const { return _pos_control.crosstrack_error();}

    static const struct AP_Param::GroupInfo var_info[];

protected:

    // flags structure
    struct wpnav_flags {
        uint8_t reached_destination     : 1;    // true if we have reached the destination
        uint8_t fast_waypoint           : 1;    // true if we should ignore the waypoint radius and consider the waypoint complete once the intermediate target has reached the waypoint
        uint8_t wp_yaw_set              : 1;    // true if yaw target has been set
    } _flags;

    // helper function to calculate scurve jerk and jerk_time values
    // updates _scurve_jerk_max_msss and _scurve_snap_max_mssss
    void calc_scurve_jerk_and_snap();

    // references and pointers to external libraries
    const AP_AHRS_View&     _ahrs;
    AC_PosControl&          _pos_control;
    const AC_AttitudeControl& _attitude_control;

    // parameters
    AP_Float    _wp_speed_cms;      // default maximum horizontal speed in cm/s during missions
    AP_Float    _wp_speed_up_cms;   // default maximum climb rate in cm/s
    AP_Float    _wp_speed_down_cms; // default maximum descent rate in cm/s
    AP_Float    _wp_radius_cm;      // distance from a waypoint in cm that, when crossed, indicates the wp has been reached
    AP_Float    _wp_accel_cmss;     // horizontal acceleration in cm/s/s during missions
    AP_Float    _wp_accel_c_cmss;   // cornering acceleration in cm/s/s during missions
    AP_Float    _wp_accel_z_cmss;   // vertical acceleration in cm/s/s during missions
    AP_Float    _wp_jerk_msss;      // maximum jerk used to generate scurve trajectories in m/s/s/s
    AP_Float    _terrain_margin_m;  // terrain following altitude margin. vehicle will stop if distance from target altitude is larger than this margin

    // WPNAV_SPEED param change checker
    bool _check_wp_speed_change;    // if true WPNAV_SPEED param should be checked for changes in-flight
    float _last_wp_speed_cms;       // last recorded WPNAV_SPEED, used for changing speed in-flight
    float _last_wp_speed_up_cms;    // last recorded WPNAV_SPEED_UP, used for changing speed in-flight
    float _last_wp_speed_down_cms;  // last recorded WPNAV_SPEED_DN, used for changing speed in-flight

    // scurve
    SCurve _scurve_prev_leg;        // previous scurve trajectory used to blend with current scurve trajectory
    SCurve _scurve_this_leg;        // current scurve trajectory
    SCurve _scurve_next_leg;        // next scurve trajectory used to blend with current scurve trajectory
    float _scurve_jerk_max_msss;    // scurve jerk max in m/s/s/s
    float _scurve_snap_max_mssss;   // scurve snap in m/s/s/s/s

    // spline curves
    SplineCurve _spline_this_leg;   // spline curve for current segment
    SplineCurve _spline_next_leg;   // spline curve for next segment

    // the type of this leg
    bool _this_leg_is_spline;       // true if this leg is a spline
    bool _next_leg_is_spline;       // true if the next leg is a spline

    // waypoint controller internal variables
    uint32_t    _wp_last_update_ms;         // time of last update_wpnav call (in ms)
    float       _wp_desired_speed_ne_cms;   // desired wp speed in cm/sec
    Vector3f    _origin_neu_cm;             // starting point of trip to next waypoint in cm from ekf origin
    Vector3f    _destination_neu_cm;        // target destination in cm from ekf origin
    Vector3f    _next_destination_neu_cm;   // next target destination in cm from ekf origin
    float       _track_dt_scalar;           // time compression multiplier to slow the progress along the track
    float       _offset_vel_cms;            // horizontal velocity reference used to slow the aircraft for pause and to ensure the aircraft can maintain height above terrain
    float       _offset_accel_cmss;         // horizontal acceleration reference used to slow the aircraft for pause and to ensure the aircraft can maintain height above terrain
    bool        _paused;                    // flag for pausing waypoint controller

    // terrain following variables
    bool        _terrain_alt;                   // true if origin and destination.z are alt-above-terrain, false if alt-above-ekf-origin
    bool        _rangefinder_available;         // true if rangefinder is enabled (user switch can turn this true/false)
    AP_Int8     _rangefinder_use;               // parameter that specifies if the range finder should be used for terrain following commands
    bool        _rangefinder_healthy;           // true if rangefinder distance is healthy (i.e. between min and maximum)
    float       _rangefinder_terrain_offset_cm; // latest rangefinder based terrain offset (e.g. terrain's height above EKF origin)
};
