#ifndef TRACK_H
#define TRACK_H

#include <memory>
#include <unordered_set>

#include <boost/uuid/uuid.hpp>

#include <Common/time.h>

#include "estimationfilter.hpp"

class DetectionReport;

class Track
{
public:
  typedef std::unordered_set<class Feature*> features_set_t;

  /**
   * @brief c-tor. Creates track based on given creation time.
   *
   *  Creation time is given, because when working in batch mode, current time cannot be used to maintain tracks,
   *  e.g. When replaying data, tracks initialized with time in future (the simulation time) would live too long (longer than when it's real-time tracking).
   *  Default value causes setting current local time, instead of given.
   *  So, whenever you want to create Track without outside time synchronizer and the moment of creation is a moment when track appeared in system,
   *  use default argument to c-tor.
   *  Initializes estimation filter with given longitude, latitude and metersOverSea.
   * @param Estimation filter to use, when predicting next state of Track
   * @param longitude of Track
   * @param latitude of Track
   * @param meters over sea position of Track
   * @param variance of longitude (measurement of certainty)
   * @param variance of latitude (measurement of certainty)
   * @param variance of meters over sea position of Track (measurement of certainty)
   * @param Time of track creation, needed to maintain track (remove after no-update time).
   */
  Track(std::unique_ptr<estimation::EstimationFilter<> > filter,
        double longitude, double latitude, double metersOverSea,
        double lonVar, double latVar, double mosVar,
        time_types::ptime_t = time_types::clock_t::now());

  /**
   * @brief Refreshes Track (sets it's last update time to given)
   * @param Time of refresh
   */
  void refresh(time_types::ptime_t refreshTime = time_types::clock_t::now());

  /**
   * @brief Sets estimation filter to be used when predicting next state of track.
   * @param Filter to be assigned to track. Ownership is transferred to track.
   */
  void setEstimationFilter(std::unique_ptr<estimation::EstimationFilter<> > filter);

  features_set_t getFeatures() const;
  const features_set_t& getFeaturesRef() const;

  double getLongitude() const;
  double getLatitude() const;
  double getMetersOverSea() const;

  double getLongitudeVelocity() const;
  double getLatitudeVelocity() const;
  double getMetersOverSeaVelocity() const;

  double getPredictedLongitude() const;
  double getPredictedLatitude() const;
  double getPredictedMetersOverSea() const;

  double getLongitudePredictionVariance() const;
  double getLatitudePredictionVariance() const;
  double getMetersOverSeaPredictionVariance() const;

  std::tuple<double,double,double> getPredictedState() const;

  time_types::ptime_t getRefreshTime() const;

  boost::uuids::uuid getUuid() const;

  /**
   * @brief Puts model state of given DR to Track's estimation filter
   *  It's invoking correct() method on EstimationFilter assigned to Track,
   *  with values corresponding to the measured.
   * @param DetectionReport representing measurement
   */
  void applyMeasurement(const DetectionReport&);

  /**
   * @brief Puts appropriate data into model state in EstimationFilter.
   * @param longitude
   * @param latitude
   * @param meters over sea
   * @param how much time passed from last measurement
   * @overload applyMeasurement(const DetectionReport&);
   */
  void applyMeasurement(double longitude, double latitude, double mos,
                        time_types::duration_t timePassed);

  bool isTrackValid(time_types::ptime_t currentTime,
                    time_types::duration_t TTL) const;

  std::unique_ptr<Track> clone() const;

private:
  Track(const Track&);

  static estimation::EstimationFilter<>::vector_t
    coordsToStateVector(double longitude,
                        double latitude,
                        double metersoversea,
                        double longitudeVelocity,
                        double latitudeVelocity,
                        double metersoverseaVelocity);

  std::pair<
              estimation::EstimationFilter<>::vector_t,
              estimation::EstimationFilter<>::vector_t
           > initializeFilter(double longitude,
                              double latitude,
                              double metersoversea,
                              double varLon,
                              double varLat,
                              double varMos);

  void storePredictions(std::pair<
                          estimation::EstimationFilter<>::vector_t,
                          estimation::EstimationFilter<>::vector_t
                        > prediction);

  double lon_;
  double lat_;
  double mos_;

  double lonVel_;
  double latVel_;
  double mosVel_;

  double predictedLon_;
  double predictedLat_;
  double predictedMos_; // not yet implemented

  double lonPredictionVar_;
  double latPredictionVar_;
  double mosPredictionVar_; // not yet implemented

  features_set_t features_;
  std::unique_ptr<estimation::EstimationFilter<> > estimationFilter_;
  time_types::ptime_t refreshTime_;

  const boost::uuids::uuid uuid_;
};

class HumanTrack : public Track
{
  // TODO implement this
};

class VehicleTrack : public Track
{
  // TODO implement this
};

#endif // TRACK_H
